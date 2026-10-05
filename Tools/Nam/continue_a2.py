#!/usr/bin/env python3
"""Run bounded A2 refinement stages; stop for independent tests or review.

Linux coordinator for the offline production workspace. It never declares a
release, modifies acceptance limits, starts a second worker on the same run,
or restarts a plateau automatically. Progress and checkpoints survive stages.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from quality_profile import PROFILE


def atomic_json(path, value):
    temporary = path.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(value, indent=2)+'\n')
    temporary.replace(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['data', 'run', 'trainer']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--warm-start', type=Path)
    p.add_argument('--max-steps', type=int, default=60000)
    p.add_argument('--chunk', type=int, default=2000)
    p.add_argument('--threads', type=int, default=4)
    args = p.parse_args()
    if args.chunk <= 0 or args.max_steps <= 0:
        raise ValueError('Positive step budget required')
    args.run.mkdir(parents=True, exist_ok=True)
    lock = (args.run/'coordinator.lock').open('w')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError('Another coordinator already owns this run')
    progress_file = args.run/'progress.json'
    progress = json.loads(progress_file.read_text()) if progress_file.exists() else {'stages': []}
    if progress.get('status') in ['READY_FOR_INDEPENDENT_TESTS', 'REVIEW_REQUIRED']:
        raise RuntimeError('Review the prior result before starting another coordinator')
    checkpoint = args.run/'checkpoint.pt'
    step = 0
    if checkpoint.exists():
        import torch
        step = int(torch.load(checkpoint, map_location='cpu', weights_only=False)['step'])
    elif args.warm_start is None:
        raise ValueError('A new run needs --warm-start')
    progress.update({'status': 'RUNNING', 'pid': os.getpid(), 'started_at_unix': time.time(),
                     'data': str(args.data.resolve()), 'run': str(args.run.resolve()),
                     'quality_profile': PROFILE, 'max_steps': args.max_steps,
                     'threads': args.threads, 'chunk_steps': args.chunk})
    atomic_json(progress_file, progress)
    while step < args.max_steps:
        end = min(step+args.chunk, args.max_steps)
        cmd = [sys.executable, '-u', str(Path(__file__).with_name('train_a2.py')),
               '--data', str(args.data.resolve()), '--out', str(args.run.resolve()),
               '--trainer', str(args.trainer.resolve()), '--steps', str(end),
               '--batch', '4', '--frames', '8192', '--threads', str(args.threads),
               '--lr', '.001', '--lr-half-life', '10000', '--lr-origin', '0',
               '--tail-fraction', '.05', '--loss-normalization', 'channel',
               '--validation-selection', 'full']
        cmd += ['--resume'] if checkpoint.exists() else ['--warm-start', str(args.warm_start.resolve())]
        log = args.run/f'train-{step}-{end}.log'
        progress.update({'stage_start': step, 'stage_end': end, 'log': log.name, 'updated_at_unix': time.time()})
        atomic_json(progress_file, progress)
        with log.open('w') as stream:
            completed = subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT)
        if completed.returncode:
            progress.update({'status': 'REVIEW_REQUIRED', 'reason': 'trainer_failed', 'exit_code': completed.returncode})
            atomic_json(progress_file, progress)
            return
        validation = json.loads((args.run/'validation.json').read_text())
        rows = validation['channels']
        aggregate = sum(v['esr'] for v in rows.values())/len(rows)
        passed = all(v['tone3000_fidelity']['fidelity_pass'] for v in rows.values())
        stage = {'step': end, 'mean_validation_esr': aggregate,
                 'channel_esr': {n: v['esr'] for n, v in rows.items()},
                 'validation_fidelity_pass': passed, 'updated_at_unix': time.time()}
        progress['stages'].append(stage)
        progress.update({'last_completed_step': end, 'last_result': stage, 'updated_at_unix': time.time()})
        print(json.dumps(stage), flush=True)
        if passed:
            progress['status'] = 'READY_FOR_INDEPENDENT_TESTS'
        elif len(progress['stages']) >= 6 and aggregate >= .99*progress['stages'][-6]['mean_validation_esr']:
            progress.update({'status': 'REVIEW_REQUIRED', 'reason': 'less_than_one_percent_improvement_across_last_five_stages'})
        elif end >= args.max_steps:
            progress['status'] = 'STEP_BUDGET_REACHED'
        atomic_json(progress_file, progress)
        if progress['status'] != 'RUNNING':
            return
        step = end


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Run bounded A2 refinement stages; stop for independent tests or review.

Linux coordinator for the offline production workspace. It never declares a
release, modifies acceptance limits, starts a second worker on the same run,
or restarts a plateau automatically. Progress and checkpoints survive stages.
One verified stage is the default; --max-stages 0 explicitly opts into the
previous multi-stage loop. A paused run is safe to resume after preservation.
"""
import argparse
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import uuid

from quality_profile import PROFILE


NAMES = ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok')


def atomic_json(path, value):
    temporary = path.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(value, indent=2)+'\n')
    temporary.replace(path)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def file_version(path):
    """Distinguish a completed rewrite even when an unchanged best model wins."""
    if not path.exists():
        return None
    stat = path.stat()
    return (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)


def stage_files(run, end):
    return [run/'checkpoint.pt', run/'validation.json', run/f'validation-step-{end}.json',
            *[run/f'Nastrond-{name}.nam' for name in NAMES]]


def load_checkpoint(path):
    import torch
    return torch.load(path, map_location='cpu', weights_only=False)


def verify_stage(run, data, end, before, log):
    """Require complete, fresh, matching evidence before calling a stage done.

    A zero exit code alone can leave an old validation.json or only some newly
    exported channels. Do not count those files as a successful bounded stage.
    Legacy trainers have no final-report provenance fields, so also verify the
    fresh terminal export marker, saved selections and all exported NAM hashes.
    """
    for path in stage_files(run, end):
        version = file_version(path)
        if version is None or version == before.get(path.name):
            raise RuntimeError('Missing or stale stage output: '+path.name)
    checkpoint = load_checkpoint(run/'checkpoint.pt')
    config = json.loads((run/'training-config.json').read_text())
    manifest = json.loads((data/'manifest.json').read_text())
    if checkpoint['step'] != end:
        raise RuntimeError('Checkpoint step does not match requested stage end')
    if checkpoint['identity'] != config['identity']:
        raise RuntimeError('Checkpoint/config identity mismatch')
    if config['dataset_sha256'] != sha256(data/'manifest.json') or config['source'] != manifest['source']:
        raise RuntimeError('Stage dataset/source identity mismatch')
    validation = json.loads((run/'validation.json').read_text())
    if validation['quality_profile'] != PROFILE:
        raise RuntimeError('Stage quality profile mismatch')
    for key, expected in [('checkpoint_step', end), ('identity', checkpoint['identity'])]:
        if key in validation and validation[key] != expected:
            raise RuntimeError('Validation provenance mismatch: '+key)
    latest = json.loads((run/f'validation-step-{end}.json').read_text())
    rows = validation['channels']
    if set(rows) != set(NAMES) or set(latest['esr_by_channel']) != set(NAMES):
        raise RuntimeError('Stage has missing or unexpected channels')
    if latest['best_steps'] != checkpoint['best_steps'] or len(checkpoint['best_steps']) != len(NAMES):
        raise RuntimeError('Checkpoint/latest validation selection mismatch')
    for index, name in enumerate(NAMES):
        row = rows[name]
        if row['selected_step'] != checkpoint['best_steps'][index] or not 0 <= row['selected_step'] <= end:
            raise RuntimeError('Export/checkpoint selection mismatch: '+name)
        if not math.isfinite(row['esr']) or row['esr'] < 0 or not math.isfinite(latest['esr_by_channel'][name]):
            raise RuntimeError('Invalid validation ESR: '+name)
        if type(row['tone3000_fidelity']['fidelity_pass']) is not bool:
            raise RuntimeError('Invalid fidelity result: '+name)
        if row['sha256'] != sha256(run/f'Nastrond-{name}.nam'):
            raise RuntimeError('Export hash mismatch: '+name)
        delta = row['official_roundtrip_max_abs']
        if not math.isfinite(delta) or delta > 1e-6 or delta < 0:
            raise RuntimeError('NAM export roundtrip failed: '+name)
    exported = None
    for line in log.read_text().splitlines():
        try:
            item = json.loads(line)
        except (ValueError, TypeError):
            continue
        if isinstance(item, dict) and item.get('stage') == 'exported':
            exported = item
    if exported is None or exported.get('channels') != rows:
        raise RuntimeError('Missing or mismatched final export marker')
    return validation, checkpoint


def terminate_child(child):
    """Stop only this coordinator's child process group, then reap the child."""
    if child.poll() is not None:
        return child.returncode
    try:
        os.killpg(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        return child.wait(timeout=10)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        return child.wait()


def run_child(cmd, stream, lock, interrupted, on_start=None):
    # Retaining the same flock in the child prevents another coordinator from
    # starting a duplicate after an uncatchable parent SIGKILL/runtime teardown.
    child = subprocess.Popen(cmd, stdout=stream, stderr=subprocess.STDOUT,
                             pass_fds=(lock.fileno(),), start_new_session=True)
    try:
        if on_start is not None:
            on_start(child.pid)
        while True:
            if interrupted['signal'] is not None:
                return terminate_child(child)
            try:
                return child.wait(timeout=1)
            except subprocess.TimeoutExpired:
                continue
    except BaseException:
        terminate_child(child)
        raise


def trainer_command(args, end, checkpoint):
    cmd = [sys.executable, '-u', str(Path(__file__).with_name('train_a2.py')),
           '--data', str(args.data.resolve()), '--out', str(args.run.resolve()),
           '--trainer', str(args.trainer.resolve()), '--steps', str(end),
           '--batch', '4', '--frames', '8192', '--threads', str(args.threads),
           '--validation-selection', 'full']
    if args.recipe is not None:
        cmd += ['--recipe', args.recipe]
    if args.recipe != 'official-a2':
        cmd += ['--lr', '.001', '--lr-half-life', '10000', '--lr-origin', '0',
                '--loss-normalization', 'channel']
    if getattr(args, 'learning_rate', None) is not None:
        cmd += ['--lr', str(args.learning_rate)]
    tail_fraction = args.tail_fraction if args.tail_fraction is not None else (.05 if args.recipe != 'official-a2' else None)
    if tail_fraction is not None:
        cmd += ['--tail-fraction', str(tail_fraction)]
    cmd += ['--resume'] if checkpoint.exists() else ['--warm-start', str(args.warm_start.resolve())]
    return cmd


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['data', 'run', 'trainer']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--warm-start', type=Path)
    p.add_argument('--max-steps', type=int, default=60000)
    p.add_argument('--chunk', type=int, default=2000)
    p.add_argument('--threads', type=int, default=4)
    p.add_argument('--recipe', choices=['legacy', 'official-a2'],
                   help='Optional trainer recipe; omission preserves the legacy objective')
    p.add_argument('--tail-fraction', type=float,
                   help='Optional matched-data override; default legacy .05, official-a2 0')
    p.add_argument('--learning-rate', type=float,
                   help='Experimental LR override; changing an official recipe identity requires a fresh warm start')
    p.add_argument('--max-stages', type=int, default=1,
                   help='Verified stages in this invocation (default 1; 0 keeps running until a stop condition)')
    args = p.parse_args()
    if args.chunk <= 0 or args.max_steps <= 0 or args.threads <= 0 or args.max_stages < 0:
        raise ValueError('Positive step/thread budgets and nonnegative --max-stages required')
    if args.tail_fraction is not None and not 0 <= args.tail_fraction <= .25:
        raise ValueError('Tail fraction must be between 0 and .25')
    if args.learning_rate is not None and (not math.isfinite(args.learning_rate) or args.learning_rate <= 0):
        raise ValueError('Learning-rate override must be finite and positive')
    args.run.mkdir(parents=True, exist_ok=True)
    lock = (args.run/'coordinator.lock').open('a+')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError('Another coordinator or its trainer already owns this run')
    progress_file = args.run/'progress.json'
    progress = json.loads(progress_file.read_text()) if progress_file.exists() else {'stages': []}
    progress.setdefault('stages', [])
    if progress.get('status') in ['READY_FOR_INDEPENDENT_TESTS', 'REVIEW_REQUIRED']:
        raise RuntimeError('Review the prior result before starting another coordinator')
    checkpoint = args.run/'checkpoint.pt'
    step = 0
    if checkpoint.exists():
        step = int(load_checkpoint(checkpoint)['step'])
    elif args.warm_start is None:
        raise ValueError('A new run needs --warm-start')
    if progress.get('status') == 'RUNNING':
        progress.setdefault('interruptions', []).append({
            'reason': 'prior_coordinator_no_longer_holds_lock', 'cause': 'unknown',
            'detected_at_unix': time.time(), 'last_saved_step': step,
            'prior_stage_start': progress.get('stage_start'), 'prior_stage_end': progress.get('stage_end'),
            'prior_log': progress.get('log')})
    # A prior signal/failure is retained in history rather than attached to a
    # newly running stage. Status never uses PID presence as liveness evidence.
    if progress.get('status') == 'INTERRUPTED':
        progress.setdefault('interruptions', []).append({
            key: progress[key] for key in ['status', 'reason', 'signal', 'exit_code',
                                          'last_saved_step', 'updated_at_unix'] if key in progress})
    for key in ['reason', 'signal', 'exit_code', 'error', 'child_pid']:
        progress.pop(key, None)
    progress.update({'status': 'RUNNING', 'pid': os.getpid(), 'started_at_unix': time.time(),
                     'data': str(args.data.resolve()), 'run': str(args.run.resolve()),
                     'quality_profile': PROFILE, 'max_steps': args.max_steps,
                     'threads': args.threads, 'chunk_steps': args.chunk,
                     'max_stages_this_invocation': args.max_stages,
                     'recipe': args.recipe or 'legacy', 'tail_fraction_override': args.tail_fraction})
    atomic_json(progress_file, progress)
    interrupted = {'signal': None}
    previous_handlers = {}
    def on_signal(signum, _frame):
        if interrupted['signal'] is None:
            interrupted['signal'] = signum
    for signum in [signal.SIGINT, signal.SIGTERM, signal.SIGHUP]:
        previous_handlers[signum] = signal.signal(signum, on_signal)
    completed_stages = 0
    try:
        if step >= args.max_steps:
            progress.update({'status': 'STEP_BUDGET_REACHED', 'last_saved_step': step,
                             'updated_at_unix': time.time()})
            atomic_json(progress_file, progress)
            return 0
        while step < args.max_steps:
            end = min(step+args.chunk, args.max_steps)
            cmd = trainer_command(args, end, checkpoint)
            # Never erase the output of an interrupted attempt at this range.
            log = args.run/f'train-{step}-{end}-{time.time_ns()}-{uuid.uuid4().hex[:8]}.log'
            before = {path.name: file_version(path) for path in stage_files(args.run, end)}
            progress.update({'stage_start': step, 'stage_end': end, 'log': log.name,
                             'command': cmd, 'updated_at_unix': time.time()})
            atomic_json(progress_file, progress)
            def on_start(pid):
                progress.update({'child_pid': pid, 'updated_at_unix': time.time()})
                atomic_json(progress_file, progress)
            with log.open('x') as stream:
                returncode = run_child(cmd, stream, lock, interrupted, on_start)
            if interrupted['signal'] is not None:
                signum = interrupted['signal']
                progress.update({'status': 'INTERRUPTED', 'reason': 'coordinator_received_signal',
                                 'signal': signal.Signals(signum).name, 'exit_code': returncode,
                                 'last_saved_step': int(load_checkpoint(checkpoint)['step']) if checkpoint.exists() else 0,
                                 'updated_at_unix': time.time()})
                atomic_json(progress_file, progress)
                return 128+signum
            if returncode:
                progress.update({'status': 'REVIEW_REQUIRED', 'reason': 'trainer_failed',
                                 'exit_code': returncode, 'updated_at_unix': time.time()})
                if returncode < 0:
                    progress['signal'] = signal.Signals(-returncode).name
                atomic_json(progress_file, progress)
                return 1
            validation, saved = verify_stage(args.run, args.data, end, before, log)
            rows = validation['channels']
            aggregate = sum(v['esr'] for v in rows.values())/len(rows)
            passed = all(v['tone3000_fidelity']['fidelity_pass'] for v in rows.values())
            stage = {'step': end, 'mean_validation_esr': aggregate,
                     'channel_esr': {n: v['esr'] for n, v in rows.items()},
                     'validation_fidelity_pass': passed, 'updated_at_unix': time.time(),
                     'checkpoint_sha256': sha256(checkpoint), 'identity': saved['identity'],
                     'validation_sha256': sha256(args.run/'validation.json'),
                     'export_sha256': {n: v['sha256'] for n, v in rows.items()}, 'log': log.name}
            progress['stages'].append(stage)
            progress.update({'last_completed_step': end, 'last_saved_step': end,
                             'last_result': stage, 'updated_at_unix': time.time()})
            print(json.dumps(stage), flush=True)
            completed_stages += 1
            if passed:
                progress['status'] = 'READY_FOR_INDEPENDENT_TESTS'
            elif len(progress['stages']) >= 6 and aggregate >= .99*progress['stages'][-6]['mean_validation_esr']:
                progress.update({'status': 'REVIEW_REQUIRED', 'reason': 'less_than_one_percent_improvement_across_last_five_stages'})
            elif end >= args.max_steps:
                progress['status'] = 'STEP_BUDGET_REACHED'
            elif args.max_stages and completed_stages >= args.max_stages:
                progress['status'] = 'PAUSED_AFTER_STAGE'
            atomic_json(progress_file, progress)
            if progress['status'] != 'RUNNING':
                return 0
            step = end
    except Exception as exc:
        progress.update({'status': 'REVIEW_REQUIRED', 'reason': 'coordinator_or_stage_verification_failed',
                         'error': f'{type(exc).__name__}: {exc}', 'updated_at_unix': time.time()})
        atomic_json(progress_file, progress)
        raise
    finally:
        for signum, handler in previous_handlers.items():
            signal.signal(signum, handler)
        # close, not LOCK_UN: the child may be the last owner after an
        # unexpected parent failure, and must retain exclusion until it exits.
        lock.close()


if __name__ == '__main__':
    sys.exit(main())

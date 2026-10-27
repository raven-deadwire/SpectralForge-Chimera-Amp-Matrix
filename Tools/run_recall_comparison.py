#!/usr/bin/env python3
"""Sequential, real-time-paced recall experiment; never a listening approval."""
import argparse
import csv
import hashlib
import json
import platform
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=2)
    parser.add_argument('--warm-control-only', action='store_true',
                        help='isolate existing response-cache warming without recycled kernels')
    parser.add_argument('--final-default', action='store_true',
                        help='exercise the processor default without a policy override, with warm A/B')
    args = parser.parse_args()
    if args.final_default and args.warm_control_only:
        parser.error('select only one focused experiment')
    if args.repeats < 1:
        parser.error('--repeats must be positive')
    args.out.mkdir(parents=True, exist_ok=True)
    rows, runs = [], []
    variants = [('baseline', False), ('serial', False), ('priority', False), ('parallel', False),
                ('cache', False), ('cache', True), ('priority', True)]
    if args.warm_control_only:
        variants = [('priority', True)]
    if args.final_default:
        variants = [('default', True)]
    # Rotate order to reduce systematic warm-up/temperature bias. Do not run
    # comparisons concurrently: that would contaminate worker/RT timing.
    for repeat in range(args.repeats):
        order = variants[repeat % len(variants):] + variants[:repeat % len(variants)]
        for block in (64, 128, 256):
            for policy, warm in order:
                label = policy + ('-warm' if warm else '-cold')
                stem = f'{label}-{block}-r{repeat + 1}'
                output = args.out / (stem + '.csv')
                command = [str(args.probe.resolve()), '--rate', '48000', '--block', str(block),
                           '--recalls-only', str(output.resolve())]
                if policy != 'default':
                    command += ['--policy', policy]
                if warm:
                    command.append('--warm-ab')
                run = subprocess.run(command, capture_output=True, text=True, timeout=300)
                (args.out / (stem + '.log')).write_text(run.stdout + run.stderr)
                runs.append(dict(label=label, block=block, repeat=repeat + 1, exit_code=run.returncode))
                if output.exists():
                    with output.open(newline='') as stream:
                        for row in csv.DictReader(stream):
                            row.update(policy=policy, warm_ab=warm, repeat=repeat + 1)
                            row['response_60ms'] = 0 <= float(row['response_ms']) <= 60
                            row['acoustic_60ms'] = 0 <= float(row['recovery_ms']) <= 60
                            row['response_ready_wall_60ms'] = 0 <= float(row['response_ready_wall_ms']) <= 60
                            row['acoustic_ready_wall_60ms'] = 0 <= float(row['recovery_ready_wall_ms']) <= 60
                            rows.append(row)
                print(stem, 'exit', run.returncode, flush=True)
    tracked = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', 'Source', 'Tests', 'CMakeLists.txt'], text=True).splitlines()
    fingerprint = hashlib.sha256()
    for name in sorted(set(tracked)):
        fingerprint.update(name.encode() + b'\0' + Path(name).read_bytes())
    summary = dict(platform=platform.platform(), processor=platform.processor(),
                   base_sha=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                   tracked_source_sha256=fingerprint.hexdigest(), rate=48000, runs=runs, rows=rows,
                   final_default_without_override=args.final_default,
                   listening_approved=False, physical_audio_device_tested=False,
                   obs_tested=False, daw_tested=False,
                   notes=['Process CPU is total process CPU / wall time during the post-switch horizon.',
                          'Overruns count callback durations beyond the sample-buffer budget, not device xruns.',
                          'Ready-wall metrics measure buffer return after the recall request, not a physical speaker.',
                          'baseline uses sequential scheduling, 50 ms CAB crossfades and the strengthened readiness guard.',
                          'serial uses #194 instant swaps with sequential scheduling; priority/parallel/cache use 2 ms polling.',
                          'warm_ab warms only A/B scenarios; preset rows remain cold.',
                          'priority-warm controls for the pre-existing immutable response cache; it does not recycle kernels.'])
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    return int(any(run['exit_code'] for run in runs))


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Focused observer/cadence ABBA + BAAB collection. Diagnostic, never acceptance.

Build both binaries before invoking this file. It deliberately performs no
offline parsing between trials; analyze/verify after all measurements finish.
"""
import argparse
import hashlib
import json
import platform
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--control', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--canonical-reset', choices=('yes', 'no'), default='no')
    parser.add_argument('--experiments', choices=('observer', 'cadence', 'both'), default='both')
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    binaries = {'stages': args.probe.resolve(), 'off': args.control.resolve()}
    hashes = {k: hashlib.sha256(p.read_bytes()).hexdigest() for k, p in binaries.items()}
    experiments = (['observer'] if args.experiments == 'observer' else
                   ['cadence-off', 'cadence-stages'] if args.experiments == 'cadence' else
                   ['observer', 'cadence-off', 'cadence-stages'])
    plan = []
    for experiment in experiments:
        for round_, order in enumerate(('ABBA', 'BAAB')):
            for slot, arm in enumerate(order):
                traced = arm == 'B' if experiment == 'observer' else experiment == 'cadence-stages'
                cadence = 'legacy' if experiment == 'observer' or arm == 'A' else 'continuous'
                plan.append(dict(experiment=experiment, round=round_, order=order, slot=slot,
                                 arm=arm, level='stages' if traced else 'off', cadence=cadence))
    (root / 'plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    report = dict(diagnostic_only=True, physical_device_dropout='not_tested',
                  platform=platform.uname()._asdict(), canonical_reset=args.canonical_reset,
                  complete=False, trials=[])
    for trial in plan:
        label = f"{trial['experiment']}-{trial['round']}-{trial['slot']}-{trial['arm']}"
        command = [str(binaries[trial['level']]), '--mode', 'cab', '--engine', 'expanded-v2',
                   '--channels', '2', '--trace-level', trial['level'], '--cadence', trial['cadence'],
                   '--canonical-reset', args.canonical_reset, '--output-dir', str(root / label)]
        with (root / (label + '.log')).open('w') as log:
            try:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=900)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
                log.write('\nCOLLECTION TIMEOUT: incomplete evidence; original deadline unchanged\n')
        report['trials'].append(dict(**trial, label=label, command=command, returncode=code,
                                     binary_sha256=hashes[trial['level']]))
        (root / 'report.partial.json').write_text(json.dumps(report, indent=2) + '\n')
        print(label, code, flush=True)
    report['complete'] = all(t['returncode'] == 0 for t in report['trials'])
    # Complete collection is not independent verification or a performance pass.
    (root / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

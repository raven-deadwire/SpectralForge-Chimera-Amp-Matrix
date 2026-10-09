#!/usr/bin/env python3
"""Preserve raw CI receipts after platform checks or candidate assembly."""
import argparse
from pathlib import Path
import subprocess
import release_evidence as evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--run-attempt', required=True)
    parser.add_argument('--kind', choices=evidence.KINDS, required=True)
    parser.add_argument('--output', type=Path, required=True)
    for name in ('ctest', 'package', 'installer', 'msix', 'scan', 'assemble', 'source', 'upload'):
        parser.add_argument('--' + name, default='', choices=sorted(evidence.OUTCOMES))
    args = parser.parse_args()
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=evidence.ROOT, text=True).strip()
    evidence.gate.require(actual == args.commit, 'Producer source must be checked-out HEAD')
    revision = evidence.identity(args.commit, args.run_id, args.run_attempt)
    outcomes = {name: getattr(args, name) for name in ('ctest', 'package', 'installer', 'msix', 'scan', 'assemble', 'source', 'upload')}
    evidence.collect_bundle(evidence.ROOT, args.output, args.kind, revision, outcomes)
    print(f'Preserved {args.kind} evidence for {args.commit}, run {args.run_id}/{args.run_attempt}')


if __name__ == '__main__':
    main()


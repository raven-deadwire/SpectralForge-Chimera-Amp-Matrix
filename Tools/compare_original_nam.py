#!/usr/bin/env python3
"""Measure local NAM captures against the actual OriginalAmpProcessor at 4x.

Compatibility entry point for `nastrond_nam_tool.py compare`; the arguments and
the v1 report keys are unchanged. Reports observations, not hardware
equivalence or musical acceptance. NAM weights and rendered audio stay outside
the repository. Requires numpy/scipy.
"""
import argparse
from pathlib import Path

from nastrond_nam import compare


def main():
    parser = argparse.ArgumentParser()
    for name in ['models', 'manifest', 'nam-render', 'original-render', 'out']:
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--states', type=Path, help='Additional named renderer states, retaining the frozen v1 suite')
    args = parser.parse_args()
    compare.run(args.models, args.manifest, args.nam_render, args.original_render, args.out,
                args.workers, args.states)


if __name__ == '__main__':
    main()

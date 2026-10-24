#!/usr/bin/env python3
"""Náströnd NAM reference toolchain. Observations only, never hardware matching.

  fetch     acquire the manifest's NAM files into a private directory
  compare   render probe through OriginalAmp states and NAM captures
  gain-csv  regenerate the gain summary CSV from a comparison report
  audit     macro-direction audit over baseline/incoming/candidate reports
  groups    print the macro target groups derived from the manifest

NAM weights and rendered audio stay outside the repository. Requires numpy/scipy
for compare/gain-csv/audit. See docs/NASTROND_NAM_TOOLCHAIN.md.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

MANIFEST = Path(__file__).resolve().parents[1] / 'docs/reference/nastrond-nam-manifest.json'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fetch(args):
    from nastrond_nam import fetch
    fetch.run(args.manifest, args.out)


def compare(args):
    from nastrond_nam import compare
    compare.run(args.models, args.manifest, args.nam_render, args.original_render, args.out,
                args.workers, args.states)


def gain_csv(args):
    from nastrond_nam import summary
    text = summary.gain_csv(json.loads(args.comparison.read_text()))
    if args.out:
        args.out.write_text(text)
    else:
        sys.stdout.write(text)


def audit(args):
    from nastrond_nam import references, summary
    paths = {name: getattr(args, name) for name in ['baseline', 'incoming', 'candidate']}
    groups = references.target_groups(references.load(args.manifest))
    result = summary.audit({name: json.loads(path.read_text()) for name, path in paths.items()}, groups,
                           metric=args.metric, scope=args.scope, min_improvement_db=args.min_improvement_db,
                           max_default_drift_db=args.max_default_drift_db, manifest_path=args.manifest)
    result['comparison_sha256'] = {name: digest(path) for name, path in paths.items()}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(result['status']['synthetic_endpoint_direction'], 'synthetic macro direction only')
    for macro, group in result['groups'].items():
        names = summary.state_names(args.scope, macro)
        for segment in ['pluck', 'chord']:
            v = group['measurements']['candidate'][segment]['mean_scores_db']
            print(f'{macro}/{segment}: {v[names[1]]:.3f} -> {v[names[2]]:.3f} dB')
    if result['failures']:
        raise SystemExit('; '.join(result['failures']))


def groups(args):
    from nastrond_nam import references
    for macro, (family, ids) in references.target_groups(references.load(args.manifest)).items():
        print(macro, family, *ids)


def parser():
    root = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = root.add_subparsers(dest='command', required=True)

    def command(name, function, *required):
        sub = commands.add_parser(name)
        sub.set_defaults(function=function)
        sub.add_argument('--manifest', type=Path, default=MANIFEST)
        for option in required:
            sub.add_argument('--' + option, type=Path, required=True)
        return sub

    command('fetch', fetch, 'out')
    sub = command('compare', compare, 'models', 'nam-render', 'original-render', 'out')
    sub.add_argument('--workers', type=int, default=2)
    sub.add_argument('--states', type=Path, help='Additional named renderer states, retaining the frozen v1 suite')
    sub = command('gain-csv', gain_csv, 'comparison')
    sub.add_argument('--out', type=Path)
    sub = command('audit', audit, 'baseline', 'incoming', 'candidate', 'out')
    sub.add_argument('--metric', default='log_spectrum_rms_db',
                     choices=['log_spectrum_rms_db', 'band_spectrum_rms_db'],
                     help='v1 linear-bin metric (default, comparable with frozen evidence) or octave-weighted bands')
    sub.add_argument('--scope', help='State prefix such as channel_surtr; default uses the frozen v1 states')
    sub.add_argument('--min-improvement-db', type=float, default=.1)
    sub.add_argument('--max-default-drift-db', type=float, default=.001)
    command('groups', groups)
    return root


def main(argv=None):
    args = parser().parse_args(argv)
    args.function(args)


if __name__ == '__main__':
    main()

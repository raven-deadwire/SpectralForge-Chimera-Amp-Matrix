#!/usr/bin/env python3
"""Verify an attribution artifact manifest and recompute its route associations.

The CI's independent raw verifier results remain distinct from this local
manifest/recalculation audit. A complete artifact can contain failed gates.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
from analyze_trace import analyze
from audit_original import equal, summarize


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.input
    manifest = json.loads((root / 'SHA256SUMS.json').read_text())
    for name, expected in manifest.items():
        assert digest((root / name).read_bytes()) == expected, name
    report = json.loads((root / 'report.json').read_text())
    result = dict(source_sha=report['source_sha'], order=report['order'],
                  manifest_verified=len(manifest), ci_collection_complete=report['complete'],
                  all_required_commands_passed=report.get('all_required_commands_passed'),
                  independent_raw_verification='CI verifier; manifest verified locally',
                  associations_recomputed=0, gates=[], focused=[], controls=[],
                  physical_device_dropout='not_tested', product_fix_demonstrated=False)
    for gate in report['gates']:
        folder = root / 'gates' / gate['name']
        routes = []
        for path in sorted((folder / 'cab-timing-diagnostics').rglob('*.csv')):
            with path.open() as f:
                routes.append(dict(file=str(path.relative_to(folder)), **summarize(list(csv.DictReader(f)))))
        result['gates'].append(dict(name=gate['name'], tests=gate['evidence']['tests'],
                                    routes=len(routes),
                                    p99_failed_routes=[r for r in routes if r['wall_p99_us'] > r['budget_us']],
                                    focused=[r for r in routes if r['file'].endswith('extended/expanded-v2/96000-32-2.csv')]))
    for trial in report['trials']:
        for mode in trial['modes']:
            assert mode['integrity']['complete'], (trial['label'], mode['mode'])
            folder = root / 'trials' / trial['label']
            saved = json.loads((folder / (mode['mode'] + '-associations.json')).read_text())
            for path in sorted((folder / mode['mode']).glob('*.metadata.json')):
                prefix = Path(str(path)[:-len('.metadata.json')])
                associations = analyze(prefix)
                equal(saved[path.name], associations)
                result['associations_recomputed'] += 1
                meta = json.loads(path.read_text())
                record = dict(label=trial['label'], trace_level=trial['trace_level'],
                              cadence=trial['cadence'], resource_population=trial['resource_population'],
                              route=path.name, summaries=associations['summaries'], waits=associations['waits'])
                if mode['mode'] != 'cab':
                    result['controls'].append(record)
                elif meta['engine'] == 'expanded-v2' and meta['channels'] == 2:
                    with Path(str(prefix) + '.callbacks.csv').open() as f:
                        rows = list(csv.DictReader(f))
                    record['forced_output_checksum_sha256'] = digest(','.join(r['checksum'] for r in rows[1200:]).encode())
                    with Path(str(prefix) + '.slots.csv').open() as f:
                        slots = [r for r in csv.DictReader(f) if int(r['callback_index']) >= 1200]
                    record['forced_slot_state_available'] = bool(slots)
                    record['forced_slot_state_sha256'] = digest(json.dumps(slots, sort_keys=True).encode()) if slots else None
                    result['focused'].append(record)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('gates','focused','controls')}))


if __name__ == '__main__':
    main()

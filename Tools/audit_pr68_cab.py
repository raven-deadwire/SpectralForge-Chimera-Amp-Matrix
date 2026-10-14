"""Recompute the failed source's original evidence; never change acceptance."""
import argparse
import csv
import hashlib
import json
from pathlib import Path

def audit(root):
    checked = 0
    for entry in json.loads((root / 'SHA256SUMS.json').read_text()):
        path = root / entry['path']
        if not path.resolve().is_relative_to(root.resolve()):
            raise ValueError('Manifest path escapes artifact')
        data = path.read_bytes()
        if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError('Manifest mismatch: ' + entry['path'])
        checked += 1
    routes, misses = [], []
    for path in sorted((root / 'candidate/cab-timing-diagnostics').rglob('*.csv')):
        rows = list(csv.DictReader(path.open()))
        assert len(rows) == 1600
        for index, row in enumerate(rows):
            assert int(row['callback_index']) == index
            assert float(row['budget_us']) == 1e6 * int(row['block_size']) / int(row['sample_rate'])
            assert int(row['deadline_miss']) == (float(row['wall_us']) > float(row['budget_us']))
            if int(row['deadline_miss']):
                misses.append(dict(row, path=str(path.relative_to(root))))
        wall = sorted(float(row['wall_us']) for row in rows)
        routes.append(dict(path=str(path.relative_to(root)), callbacks=len(rows), p99_us=wall[1584],
                           budget_us=float(rows[0]['budget_us']), passed=wall[1584] < float(rows[0]['budget_us'])))
    assert len(routes) == 66
    target = next(r for r in misses if r['engine'] == 'array-v3-6x10' and r['sample_rate'] == '96000'
                  and r['block_size'] == '256' and r['channels'] == '2' and r['callback_index'] == '539')
    return dict(manifest_files_checked=checked, callbacks=sum(r['callbacks'] for r in routes),
                routes=routes, failed_routes=[r for r in routes if not r['passed']], miss_records=misses,
                misses=len(misses), cpu_over=sum(float(r['thread_cpu_us']) > float(r['budget_us']) for r in misses),
                forced_misses=sum(r['phase'] == 'forced_six_slot_publication' for r in misses),
                highlighted_callback=target, cpu_wall_percent=100*float(target['thread_cpu_us'])/float(target['wall_us']),
                wall_minus_cpu_us=float(target['wall_us'])-float(target['thread_cpu_us']),
                subtraction_is_exact_descheduling=False, product_fix_demonstrated=False)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('artifact', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = audit(args.artifact)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('routes','miss_records','highlighted_callback')}))

"""Descriptive investigation, never a physical-host acceptance gate."""
import csv
import hashlib
import json
import statistics
import sys
from pathlib import Path

root = Path(sys.argv[1])
raw = root / 'cost-investigation.csv'
rows = list(csv.DictReader(raw.open()))
result = {'daw_acceptance': 'NOT_ASSESSED', 'sha256': hashlib.sha256(raw.read_bytes()).hexdigest(), 'cells': []}
for rate in sorted({r['rate'] for r in rows}, key=float):
    cell = {'rate': rate, 'implementations': {}, 'rounds': []}
    for impl in sorted({r['implementation'] for r in rows}):
        selected = [r for r in rows if r['rate'] == rate and r['implementation'] == impl]
        cell['implementations'][impl] = {metric: statistics.median(float(r[metric]) / int(r['repeats']) for r in selected) for metric in ('update_ms', 'feed_analysis_cpu_ms', 'feed_analysis_wall_ms')}
    for round_id in sorted({r['round'] for r in rows}, key=int):
        selected = {r['implementation']: r for r in rows if r['rate'] == rate and r['round'] == round_id}
        cell['rounds'].append({'round': round_id, 'update_reductions_percent': {left + '->' + right: 100 * (1 - float(selected[right]['update_ms']) / float(selected[left]['update_ms'])) for left, right in [('baseline', 'candidate'), ('baseline', 'baseline_AA'), ('candidate', 'candidate_AA')]}})
    result['cells'].append(cell)
    a, b = cell['implementations']['baseline'], cell['implementations']['candidate']
    print(rate, 'update reduction %', 100 * (1 - b['update_ms'] / a['update_ms']), 'CPU reduction %', 100 * (1 - b['feed_analysis_cpu_ms'] / a['feed_analysis_cpu_ms']))
stages = list(csv.DictReader((root / 'fft-stage-controls.csv').open()))
result['isolated_stages'] = {impl: {metric: statistics.median(float(r[metric]) for r in stages if r['implementation'] == impl) for metric in ('prepare_ms', 'real_fft_ms', 'power_and_tail_ms', 'total_thread_cpu_ms', 'checksum')} for impl in ('baseline', 'candidate')}
result['stage_limit'] = 'Isolated control; CPU includes timers/checksum; cannot directly assign full-update residuals.'
result['all_accounting_valid'] = all(int(r['drops']) == 0 and int(r['windows']) == 128 * int(r['repeats']) and int(r['samples']) == 65536 * int(r['repeats']) for r in rows)
(root / 'analysis.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result['isolated_stages'], indent=2))
assert result['all_accounting_valid']

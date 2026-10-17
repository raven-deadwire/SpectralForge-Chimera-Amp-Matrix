"""Exploratory callback-cadence strata; never used as an acceptance gate.

The original harness sleeps AFTER callbacks with index % 8 == 0, during
worker_and_automation only. This analysis does not remove those callbacks.
"""
import argparse
import csv
import json
import statistics
from collections import defaultdict
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--artifacts', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
result = []
for kind in ('split', 'clearhead', 'fusedlead', 'tile32'):
    root = a.artifacts / kind / 'evidence'
    report = json.loads((root / 'report.json').read_text())
    groups = defaultdict(lambda: defaultdict(list))
    for t in report['trials']:
        if t['experiment'] != 'AB':
            continue
        label = f"r{t['round']}-{t['mode']}-{t['experiment']}-{t['slot']}-{t['logical']}"
        for f in (root / 'trials' / label).rglob('96000-32-*.csv'):
            if f.name.endswith('.spectral.csv'):
                continue
            with f.open() as stream:
                rows = list(csv.DictReader(stream))
            spectral = {}
            if t['mode'] == 'profile':
                with Path(str(f) + '.spectral.csv').open() as stream:
                    spectral = {int(r['callback_index']): int(r['wall_ns']) / 1000 for r in csv.DictReader(stream)}
            for late in (False, True):
                for after_sleep in (False, True):
                    pop = [r for r in rows if (80 if late else 0) <= int(r['callback_index']) < 1200
                           and (int(r['callback_index']) % 8 == 1) == after_sleep]
                    key = (kind, t['mode'], t['arm'], rows[0]['engine'], int(rows[0]['channels']), late, after_sleep)
                    g = groups[key]
                    g['callbacks'].append(len(pop))
                    g['misses'].append(sum(int(r['deadline_miss']) for r in pop))
                    g['cpu_mean_us'].append(statistics.mean(float(r['thread_cpu_us']) for r in pop))
                    g['wall_mean_us'].append(statistics.mean(float(r['wall_us']) for r in pop))
                    if spectral:
                        g['mac_wall_mean_us'].append(statistics.mean(spectral[int(r['callback_index'])] for r in pop))
    for key, values in groups.items():
        row = dict(zip(('candidate', 'mode', 'arm', 'engine', 'channels', 'exclude_first_80', 'after_sleep'), key))
        row.update({k: sum(v) if k in ('callbacks', 'misses') else statistics.median(v) for k, v in values.items()})
        result.append(row)
    print(kind, 'cadence strata complete', flush=True)
a.output.write_text(json.dumps(result, indent=2), encoding='utf-8')

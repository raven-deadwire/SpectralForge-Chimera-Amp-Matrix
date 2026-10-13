#!/usr/bin/env python3
"""Descriptive matched-route contrasts from independently verified CSV outputs.

This does not estimate a causal QoS effect or assume independent callbacks.
It requires the completed ABBA artifact to pass independent validation first.
"""
import argparse
import csv
import json
from pathlib import Path
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    validation = json.loads((args.input / 'validation.json').read_text())
    if not validation['valid'] or len(validation['archives']) != 1:
        raise ValueError('Require one independently verified archive')
    archive = validation['archives'][0]
    if archive['status'] != 'verified_complete' or archive['order'] != 'ABBA':
        raise ValueError('Require completed ABBA evidence; do not relabel a different order')
    summaries = []
    for filename, phase in [('route-summary.csv', 'all'),
                            ('phase-summary.csv', 'worker_and_automation'),
                            ('phase-summary.csv', 'forced_six_slot_publication')]:
        with (args.input / filename).open(newline='') as stream:
            rows = list(csv.DictReader(stream))
        routes = {}
        for row in rows:
            if row['workload_kind'] != 'diagnostic-cab' or (phase != 'all' and row['phase'] != phase):
                continue
            key = tuple(row[k] for k in ['suite', 'engine', 'sample_rate', 'block_size', 'channels'])
            if row['trial'] in routes.setdefault(key, {}):
                raise ValueError('Duplicated trial/route')
            routes[key][row['trial']] = row
        if len(routes) != 66 or any(set(r) != {'01-A', '02-B', '03-B', '04-A'} for r in routes.values()):
            raise ValueError('Contrasts require all four observations for each of 66 routes')
        for quantile, field in [('p50', 'observed_p50_us'),
                                ('p99', 'p99_us' if phase == 'all' else 'observed_p99_us')]:
            balanced, repeated_a = [], []
            for trials in routes.values():
                a1, a4, b2, b3 = [float(trials[t][field]) for t in ['01-A', '04-A', '02-B', '03-B']]
                if min(a1, a4, b2, b3) <= 0:
                    raise ValueError('Nonpositive quantile cannot form this descriptive ratio')
                balanced.append((b2 + b3) / (a1 + a4))
                repeated_a.append(a4 / a1)
            summaries.append(dict(archive=archive['archive'], run_id=archive['run_id'], phase=phase,
                quantile=quantile, matched_routes=66,
                median_route_ratio_Bmean_to_Amean=statistics.median(balanced),
                routes_Bmean_below_Amean=sum(r < 1 for r in balanced),
                median_route_ratio_lateA_to_earlyA=statistics.median(repeated_a),
                routes_lateA_below_earlyA=sum(r < 1 for r in repeated_a)))
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(archive_sha256=archive['archive_sha256'], descriptive_only=True,
        causal_qos_effect_estimated=False,
        formulas={'balanced_per_route': '(metric_02B + metric_03B) / (metric_01A + metric_04A)',
                  'same_condition_per_route': 'metric_04A / metric_01A',
                  'aggregation': 'Median of 66 within-route ratios; no pooled route quantile.'},
        limitations=['Only two trials per condition; callbacks may be dependent.',
                     'Time/context and preparation/worker-state variation remain possible.',
                     'A lower miss count is not a general latency improvement.'],
        results=summaries)
    (args.output / 'matched-route-contrasts.json').write_text(json.dumps(report, indent=2) + '\n')
    with (args.output / 'matched-route-contrasts.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summaries[0]))
        writer.writeheader(); writer.writerows(summaries)


if __name__ == '__main__':
    main()

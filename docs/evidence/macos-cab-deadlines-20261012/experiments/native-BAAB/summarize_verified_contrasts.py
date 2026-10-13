#!/usr/bin/env python3
"""Descriptive matched-route contrasts from independently verified BAAB CSVs.

Explicit BAAB labels: 01-B, 02-A, 03-A, 04-B. This does not estimate a causal
QoS effect or assume independent callbacks. ABBA indices are not substituted.
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
    if archive['status'] != 'verified_complete' or archive['order'] != 'BAAB':
        raise ValueError('Require completed BAAB evidence; do not relabel a different order')
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
        if len(routes) != 66 or any(set(r) != {'01-B', '02-A', '03-A', '04-B'} for r in routes.values()):
            raise ValueError('Contrasts require all four BAAB observations for each of 66 routes')
        for quantile, field in [('p50', 'observed_p50_us'),
                                ('p99', 'p99_us' if phase == 'all' else 'observed_p99_us')]:
            balanced, repeated_b, repeated_a = [], [], []
            for trials in routes.values():
                b1, a2, a3, b4 = [float(trials[t][field]) for t in ['01-B', '02-A', '03-A', '04-B']]
                if min(b1, a2, a3, b4) <= 0:
                    raise ValueError('Nonpositive quantile cannot form this descriptive ratio')
                balanced.append((b1 + b4) / (a2 + a3))
                repeated_b.append(b4 / b1)
                repeated_a.append(a3 / a2)
            summaries.append(dict(archive=archive['archive'], run_id=archive['run_id'], phase=phase,
                quantile=quantile, matched_routes=66,
                median_route_ratio_Bmean_to_Amean=statistics.median(balanced),
                routes_Bmean_below_Amean=sum(r < 1 for r in balanced),
                median_route_ratio_lateB_to_earlyB=statistics.median(repeated_b),
                routes_lateB_below_earlyB=sum(r < 1 for r in repeated_b),
                median_route_ratio_secondA_to_firstA=statistics.median(repeated_a),
                routes_secondA_below_firstA=sum(r < 1 for r in repeated_a)))
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(archive_sha256=archive['archive_sha256'], descriptive_only=True,
        causal_qos_effect_estimated=False, order='BAAB',
        formulas={'balanced_per_route': '(metric_01B + metric_04B) / (metric_02A + metric_03A)',
                  'same_outer_condition_per_route': 'metric_04B / metric_01B',
                  'same_inner_condition_per_route': 'metric_03A / metric_02A',
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

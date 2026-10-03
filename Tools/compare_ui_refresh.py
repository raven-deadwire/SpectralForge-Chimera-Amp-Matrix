#!/usr/bin/env python3
"""Compare matching real-peer UI probes; never grant the Windows DAW gate."""
import argparse
import json
from pathlib import Path
from statistics import median


def compare(baseline, candidate):
    for key in ('os', 'cpu', 'juce', 'sample_rate', 'block_size', 'fixture', 'scope'):
        if baseline[key] != candidate[key]:
            raise ValueError(f'Mismatched benchmark condition: {key}')
    grouped = []
    for report in (baseline, candidate):
        groups = {}
        for row in report['rows']:
            key = (row['scene'], row['scale'], row['instances'])
            if not row['parameters_unchanged']:
                raise ValueError(f'UI mutated audio parameters: {key}')
            groups.setdefault(key, []).append(row)
        grouped.append(groups)
    before, after = grouped
    if before.keys() != after.keys():
        raise ValueError('Scene coverage differs')
    scenes = []
    for key, a in before.items():
        b = after[key]
        if len(a) != len(b) or len(a) < 3:
            raise ValueError(f'Need at least three matching repeats: {key}')
        # Baseline visible scenes must actually have painted. A headless timer
        # result can otherwise masquerade as a successful repaint optimisation.
        if not key[0].startswith(('closed', 'hidden')) and not all(r['editor_paints'] > 0 for r in a):
            raise ValueError(f'Baseline lacks real native paints: {key}')
        base = median(r['ui_one_core_percent'] for r in a)
        new = median(r['ui_one_core_percent'] for r in b)
        row = dict(scene=key[0], scale=key[1], instances=key[2],
                   baseline_ui_one_core_percent=base, candidate_ui_one_core_percent=new,
                   reduction_percent=100*(base-new)/base if base else None,
                   baseline_median_full_paints=median(r['full_editor_paints'] for r in a),
                   candidate_median_full_paints=median(r['full_editor_paints'] for r in b))
        if 'callback_p99_us' in a[0]:
            for label, rows in (('baseline', a), ('candidate', b)):
                for metric in ('callback_mean_us', 'callback_p95_us', 'callback_p99_us'):
                    row[f'{label}_median_{metric}'] = median(r[metric] for r in rows)
                row[f'{label}_deadline_misses_total'] = sum(r['callback_deadline_misses'] for r in rows)
        scenes.append(row)
    hashes = {r['audio_output_fnv64'] for report in (baseline,candidate)
              for r in report['rows'] if 'audio_output_fnv64' in r}
    counts = {r['callback_count'] for report in (baseline,candidate)
              for r in report['rows'] if 'callback_count' in r}
    return dict(baseline_revision=baseline['source_revision'], candidate_revision=candidate['source_revision'],
                parameters_preserved='PASS', synthetic_audio_identity='PASS' if len(hashes)==1 and counts=={1500} else 'BLOCKED',
                audio_hashes=sorted(hashes), scenes=scenes,
                ui_75_percent_stretch='BLOCKED',
                windows_daw_and_30_minute_soak='BLOCKED',
                scope='Matched Linux/X11 or Windows probe only. UI CPU is message-thread time including processor/APVTS timers; no driver/display-server CPU. Synthetic audio equality is not a real DAW deadline guarantee.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = compare(json.loads(args.baseline.read_text()), json.loads(args.candidate.read_text()))
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    if result['synthetic_audio_identity'] != 'PASS':
        raise SystemExit('Synthetic audio changed: see output report')

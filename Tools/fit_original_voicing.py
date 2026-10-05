#!/usr/bin/env python3
"""Fit a bounded broad contour to a pre-contour OriginalAmp render.

Uses plucks only for fitting and chords only for reporting. Requires the raw
outputs of compare_original_nam.py; it does not download or redistribute NAMs.
This informs one original voice, not five cloned amps or listening acceptance.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from scipy import optimize, signal
from compare_original_nam import digest, read
from validate_nam import SR, biquad, contour, metrics

# Unboosted CH2 / high or unspecified gain. Related training variants share
# one family weight; they are not independent hardware observations.
GROUPS = {
    'vh4': [569473, 569474, 569477],
    'twinjet': [652436, 652437],
    'granophyre': [403977, 421075, 421074],
    'matamp': [614727, 561845, 543854],
}
OFFSET = 21 * SR


def raw(path):
    x = np.fromfile(path, dtype=np.float32).astype(np.float64)
    if len(x) != 37 * SR or not np.isfinite(x).all():
        raise ValueError(f'Expected finite 37-second mono float32 render: {path}')
    return x[OFFSET:]


def main():
    parser = argparse.ArgumentParser()
    for name in ['manifest', 'comparison', 'pre-contour', 'baseline', 'renders', 'out']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    comparison = json.loads(args.comparison.read_text())
    if digest(args.manifest) != comparison['manifest_sha256']:
        raise ValueError('Comparison/manifest mismatch')
    if digest(args.pre_contour) != comparison['original']['default']['audio_sha256']:
        raise ValueError('Pre-contour render does not match comparison')
    candidate, baseline = raw(args.pre_contour), raw(args.baseline)
    f, pb = signal.welch(candidate[:8*SR], SR, nperseg=8192)
    # Equal log-frequency bands avoid giving treble most of the fit weight.
    edges = np.geomspace(65, 8000, 43)
    masks = [(f >= a) & (f < b) for a, b in zip(edges[:-1], edges[1:])]
    masks = [m for m in masks if m.any()]
    frequencies = np.array([np.exp(np.mean(np.log(f[m]))) for m in masks])
    power = np.array([pb[m].mean() for m in masks])
    rows = []
    for family, ids in GROUPS.items():
        for model_id in ids:
            item = next(r for r in manifest['references'] if r['model_id'] == model_id)
            if item['family'] != family or not item['head_timbre_comparison_eligible']:
                raise ValueError('Unexpected reference identity or scope')
            path = args.renders / f'nam-{model_id}-digital_equal.wav'
            measured = next(r for r in comparison['references'] if r['model_id'] == model_id)
            if digest(path) != measured['variants']['digital_equal']['audio_sha256']:
                raise ValueError(f'Reference render hash mismatch: {path}')
            y = read(path)[OFFSET:]
            _, pa = signal.welch(y[:8*SR], SR, nperseg=8192)
            target = 10*np.log10(np.array([pa[m].mean() for m in masks])/power)
            rows.append((family, model_id, y, target, 1/np.sqrt(len(ids))))

    def residual(v):
        response = np.ones(len(frequencies), complex)
        for kind, hz, db, q in [('low', 100, v[0], .707), ('peak', 500, v[1], .65),
                                ('high', 2000, v[2], .707)]:
            response *= signal.freqz(*biquad(kind, hz, db, q),
                                     worN=2*np.pi*frequencies/SR)[1]
        shape = 20*np.log10(abs(response))
        # Each recording has an arbitrary output level. Family and frequency
        # weights are equal; a small regularizer discourages needless boosts.
        return np.r_[np.concatenate([(shape+v[3+i]-r[3])*r[4]/np.sqrt(len(frequencies))
                                     for i, r in enumerate(rows)]), .04*v[:3]]

    fit = optimize.least_squares(residual, np.zeros(3+len(rows)),
                                bounds=([-6]*3+[-60]*len(rows), [6]*3+[60]*len(rows)))
    gains = np.round(fit.x[:3], 2)
    shaped = contour(candidate[:8*SR], gains)
    trim = float(20*np.log10(np.linalg.norm(baseline[:8*SR])/np.linalg.norm(shaped)))
    evidence = []
    for family, model_id, reference, _, _ in rows:
        observations = {}
        for i, kind in enumerate(['pluck', 'chord']):
            a, b = reference[i*8*SR:(i+1)*8*SR], candidate[i*8*SR:(i+1)*8*SR]
            observations[kind] = {'pre_contour': metrics(a, b),
                                  'proposed': metrics(a, contour(b, gains))}
        evidence.append(dict(family=family, model_id=model_id, measurements=observations))
    report = dict(
        protocol='Equal family weight; 42 logarithmic bands 65-8000 Hz; pluck fit / held-out chord; '
                 'identical digital input; independent reference output offsets; +/-6 dB bound; '
                 'Granophyre provenance unverified; Meshuggah amp+cab excluded',
        comparison_sha256=digest(args.comparison), manifest_sha256=digest(args.manifest),
        pre_contour_audio_sha256=digest(args.pre_contour), baseline_audio_sha256=digest(args.baseline),
        pre_contour_renderer_sha256=comparison['original_renderer_sha256'],
        nam_renderer_sha256=comparison['nam_renderer_sha256'],
        gains_db=gains.tolist(), output_trim_db=trim, output_multiplier=float(10**(trim/20)),
        caveat='Offline contour is a proposal; validate the implemented oversampled DSP separately. '
               'Synthetic inputs do not certify instrument feel or hardware equivalence.',
        references=evidence)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print('Contour dB:', gains.tolist(), 'output multiplier:', report['output_multiplier'])


if __name__ == '__main__':
    main()

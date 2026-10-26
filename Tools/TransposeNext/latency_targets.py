#!/usr/bin/env python3
"""Evaluate a Transpose candidate against the split latency targets.

Targets and limits live in Validation/transpose-latency-targets.json; see
docs/TRANSPOSE_LATENCY_TARGETS.md. All audio is synthetic. A gate reports
PASS/FAIL; T3 (upshift low band) is a report with a no-regression ceiling,
not a 10 ms claim. Real-time and external acceptance are not measured here.
"""
import argparse, json, platform, subprocess
from pathlib import Path
import numpy as np
from scipy import signal
from evaluate import attacks
from legacy_metrics import tone_metrics, sha
from weak_attack_study import FREQUENCIES, generate, event_detection
from weak_attack_holdout import holdout_input, lowband_extra
from attack_snr import event_snrs

ROOT = Path(__file__).resolve().parents[2]
SPEC = ROOT/'Validation'/'transpose-latency-targets.json'

def long_notes(f0, rate, spacing=.25, decay=.08, n=10, seed=1):
    """Ten 6-harmonic notes, 80 ms decay, 250 ms apart, 4 kHz+ onset click.

    Short fixtures (18-40 ms decays) understate the low-band delay; this
    is the long-note case from the 2026-10-23 upshift study.
    """
    rng = np.random.default_rng(seed); t = np.arange(int((.3+spacing*n+.3)*rate))/rate
    z = np.zeros_like(t); onsets = []
    hp = signal.butter(4, 4000, btype='high', fs=rate, output='sos')
    for k in range(n):
        s = .3+k*spacing; o = int(round(s*rate)); onsets.append(o); u = t[o:]-s; ph = rng.uniform(0, 2*np.pi, 6)
        z[o:] += sum(.15/h*np.sin(2*np.pi*f0*h*u+ph[h-1]) for h in range(1, 7))*(1-np.exp(-u/.001))*np.exp(-u/decay)
        m = int(.004*rate); click = rng.uniform(-1, 1, m)*np.exp(-np.arange(m)/(.0007*rate))*.05
        z[o:o+m] += signal.sosfilt(hp, click)
    return np.column_stack((z, -.65*z)).astype('<f4'), onsets

def inputs(rate, holdout_seeds=(8139, 20261024), long_seed=1):
    """(key, x, meta, roles) for every scored input at one sample rate."""
    out = []
    for inst, f in FREQUENCIES.items():
        for name, roles in [('riff75', {'attack', 'short'}), ('riff50', {'short'}), ('alternating_velocity', {'attack', 'short'}),
                            ('quiet_markers_over_sustain', {'attack', 'markers'}), ('quieter_markers', {'attack', 'markers'}),
                            ('harmonics', {'steady'}), ('tremolo', {'guard'}), ('vibrato', {'guard'}),
                            ('bright_sustain', {'guard', 'steady'}), ('noise_floor', {'guard', 'steady'})]:
            x, m = generate(name, rate, inst); out.append((f'{inst}/{name}', x, m, roles))
        for seed in holdout_seeds:
            for mode, roles in [('random_plucks', {'attack', 'short'}), ('random_soft_after_loud', {'attack', 'short'}),
                                ('random_markers', {'attack', 'markers'})]:
                x, on = holdout_input(f, mode, seed, rate)
                out.append((f'{inst}/holdout{seed}-{mode}', x, {'rate': rate, 'onsets': on, 'name': mode}, roles))
        x, on = long_notes(f, rate, seed=long_seed)
        out.append((f'{inst}/long_notes' if long_seed == 1 else f'{inst}/long_notes_seed{long_seed}', x, {'rate': rate, 'onsets': on, 'name': 'long_notes', 'fundamental_hz': f}, {'attack', 'long'}))
    return out

def main():
    p = argparse.ArgumentParser(); p.add_argument('--renderer', type=Path, required=True); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--engine', default='corr40-split-rt'); p.add_argument('--rates', default='44100,48000,96000')
    p.add_argument('--evaluate-only', action='store_true', help='re-score an existing results.json against the current spec')
    # A fresh holdout: seeds never used to choose any candidate parameter.
    p.add_argument('--holdout-seeds', default='8139,20261024'); p.add_argument('--long-seed', type=int, default=1)
    p.add_argument('--event-snr-min', type=float, help='T1: count only attack events this many dB above the 1-8 kHz floor (off by default; needs per-event rows)')
    a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=True); a.renderer = a.renderer.resolve()
    spec = json.loads(SPEC.read_text()); shifts = spec['conditions']['shifts']; block = spec['conditions']['block']
    if a.evaluate_only:
        old = json.loads((a.out/'results.json').read_text()); rows = old['rows']; report = {**old['report'], **evaluate(spec, rows, a.event_snr_min), 'spec_sha256': sha(SPEC), 'event_snr_min_db': a.event_snr_min}
        (a.out/'results.json').write_text(json.dumps({'report': report, 'rows': rows}, indent=1)+'\n')
        for t in report['targets']: print(t['id'], t['verdict'], json.dumps(t['observed'], ensure_ascii=False))
        return
    rows = []
    for rate in [int(r) for r in a.rates.split(',')]:
        for key, x, meta, roles in inputs(rate, tuple(int(v) for v in a.holdout_seeds.split(',')), a.long_seed):
            ip = a.out/'input.f32'; x.tofile(ip)
            before = attacks(x, meta) if 'attack' in roles else None
            for shift in shifts:
                op = a.out/'output.f32'
                r = subprocess.run([str(a.renderer), str(ip), str(op), str(rate), str(block), str(shift), a.engine, str(a.out/'callbacks.csv')],
                                   check=True, capture_output=True, text=True)
                native = json.loads(r.stdout); y = np.fromfile(op, dtype='<f4').reshape(-1, 2)
                row = {'input': key, 'rate': rate, 'shift': shift, 'roles': sorted(roles), 'input_sha256': sha(ip), 'output_sha256': sha(op),
                       'onset_resets': native['onset_resets'], 'low_onset_resets': native.get('low_onset_resets')}
                row['onset_association'] = event_detection(native['onset_samples'], {**meta, 'rate': rate})
                if before is not None:
                    extra = np.array(attacks(y, meta)['attack_lags_ms'])-np.array(before['attack_lags_ms'])
                    # Same validity rules as the study/holdout: the input's own
                    # peak must sit at its event; riff50's search window overlaps.
                    row['attack_metric_valid'] = bool(before['attack_lag_max_ms'] <= 10) and meta['name'] != 'riff50'
                    row['attack_additional_max_ms'] = float(max(extra))
                    row['attack_added_ms'] = [float(v) for v in extra]; row['attack_event_snr_db'] = event_snrs(x, meta['onsets'], rate)
                if roles & {'short', 'long'}: row['lowband_additional_ms'] = lowband_extra(x, y, meta['onsets'], rate)
                if 'steady' in roles:
                    f = meta['fundamental_hz']*2**(shift/12); row['tone'] = tone_metrics(y, rate, f, .12)
                rows.append(row)
            print(rate, key, flush=True)
    report = evaluate(spec, rows, a.event_snr_min)
    report.update(event_snr_min_db=a.event_snr_min, engine=a.engine, renderer_sha256=sha(a.renderer), platform=platform.platform(), spec_sha256=sha(SPEC),
                  synthetic_only=True, real_DI='NOT_AVAILABLE', actual_DAW='NOT_RUN', fixed_PDC='NOT_IMPLEMENTED')
    (a.out/'results.json').write_text(json.dumps({'report': report, 'rows': rows}, indent=1)+'\n')
    for t in report['targets']: print(t['id'], t['verdict'], json.dumps(t['observed'], ensure_ascii=False))

def evaluate(spec, rows, event_snr_min=None):
    T = {t['id']: t for t in spec['targets']}; out = []
    if event_snr_min is None:
        att = [r['attack_additional_max_ms'] for r in rows if r.get('attack_metric_valid')]
    else:
        # Opt-in amendment: only events at least event_snr_min dB above the 1-8 kHz floor.
        att = [v for r in rows if r.get('attack_metric_valid') for v, s in zip(r['attack_added_ms'], r['attack_event_snr_db']) if s >= event_snr_min]
    out.append({'id': 'T1_attack', 'limit_ms': T['T1_attack']['limit_ms'], 'observed': {'max_ms': round(max(att), 3), 'cases': len(att)},
                'verdict': 'PASS' if max(att) <= T['T1_attack']['limit_ms'] else 'FAIL'})
    def low(sel, shifts):
        v = [x for r in rows if sel(r) and r['shift'] in shifts for x in r['lowband_additional_ms']]
        return round(max(v), 3) if v else None
    d = T['T2_low_onset_down']; obs = {'short_max_ms': low(lambda r: 'short' in r['roles'], d['shifts']), 'long_max_ms': low(lambda r: 'long' in r['roles'], d['shifts'])}
    out.append({'id': d['id'], 'limit_ms': d['limit_ms'], 'observed': obs, 'verdict': 'PASS' if max(obs.values()) <= d['limit_ms'] else 'FAIL'})
    u = T['T3_low_onset_up']; obs = {}; ok = True
    for name, ceiling in u['regression_ceiling_ms'].items():
        if name == 'short':
            val = low(lambda r: 'short' in r['roles'], u['shifts'])
        else:
            val = low(lambda r, n=name: 'long' in r['roles'] and r['input'].startswith(n+'/'), u['shifts'])
        obs[name] = val; ok &= val is not None and val <= ceiling
    out.append({'id': u['id'], 'regression_ceiling_ms': u['regression_ceiling_ms'], 'observed': obs,
                'verdict': 'REPORT_WITHIN_CEILING' if ok else 'REGRESSION'})
    q = T['T4_quality']; steady = [r['tone'] for r in rows if 'tone' in r]
    unmatched = sum(len(r['onset_association']['unmatched_resets']) for r in rows)
    missed = sum(r['onset_association']['missed_events'] for r in rows)
    marker_low = sum(max(0, (r['low_onset_resets'] or 0)-1) for r in rows if 'markers' in r['roles'] or 'guard' in r['roles'] or 'steady' in r['roles'])
    obs = {'steady_warble_max_cents': round(max(t['warble_p95_p5_cents'] for t in steady), 3),
           'steady_abs_error_max_cents': round(max(abs(t['dominant_error_cents']) for t in steady), 3),
           'unmatched_resets': unmatched, 'missed_events': missed, 'low_resets_on_markers_or_sustain_after_startup': marker_low}
    ok = (obs['steady_warble_max_cents'] <= q['steady_warble_max_cents'] and obs['steady_abs_error_max_cents'] <= q['steady_abs_error_max_cents']
          and unmatched <= q['unmatched_resets_max'] and missed <= q['missed_events_max'] and marker_low <= q['low_resets_on_markers_max'])
    out.append({'id': q['id'], 'limits': {k: v for k, v in q.items() if k.endswith('_max') or k.endswith('_cents')}, 'observed': obs, 'verdict': 'PASS' if ok else 'FAIL'})
    for tid in ('T5_realtime', 'T6_external'):
        out.append({'id': tid, 'verdict': T[tid]['verdict_here'], 'observed': {'see': T[tid].get('evidence', T[tid].get('note'))}})
    return {'targets': out}

if __name__ == '__main__':
    main()

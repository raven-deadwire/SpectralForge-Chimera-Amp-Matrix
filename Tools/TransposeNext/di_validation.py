#!/usr/bin/env python3
"""T6 real-DI validation for Transpose research candidates.

Prepares and scores real (not synthetic) DI recordings against the split
latency targets (docs/TRANSPOSE_LATENCY_TARGETS.md). Protocol and the real-DI
adaptations of each metric are fixed in docs/TRANSPOSE_T6_DI_PROTOCOL.md
before any recording is seen. Steps:

  plan      check a take manifest; list what is missing (no audio needed)
  label     offline reference onsets for human review (never fed to the engine)
  freeze    pre-register manifest, audio, reviewed labels, renderer and spec
  run       render and score; refuses anything that differs from the freeze
  selftest  pipeline check on synthetic WAVs with known onsets (not T6 evidence)

Listening, DAW acceptance, fixed PDC and product integration remain separate
parts of T6 and are not measured here.
"""
import argparse, csv, json, platform, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.ndimage import maximum_filter1d
from scipy.io import wavfile
from evaluate import attacks
from legacy_metrics import tone_metrics, sha
from weak_attack_holdout import lowband_extra, holdout_input
from latency_targets import long_notes
from weak_attack_study import FREQUENCIES, generate

ROOT = Path(__file__).resolve().parents[2]
SPEC = ROOT/'Validation'/'transpose-latency-targets.json'
RATES = (44100, 48000, 96000)
# Segment kind -> latency_targets roles. Same metrics, real audio.
ROLES = {'riff': {'attack', 'short'}, 'dynamics': {'attack', 'short'}, 'long': {'attack', 'long'},
         'ghost': {'attack', 'markers'}, 'steady': {'steady'}, 'guard': {'guard'}}
MIN_NOTES = {'riff': 24, 'dynamics': 16, 'long': 10, 'ghost': 8, 'steady': 1, 'guard': 1}
SILENCE_DB = -40.  # segment edges, relative to the segment peak
EDGE_S = .2

def read_wav(path, channel=0):
    """Mono float64 in [-1, 1], rate, and input checks. No resampling, no gain."""
    rate, d = wavfile.read(path)
    if d.ndim == 2: d = d[:, channel]
    if d.dtype == np.int16: z, full = d/32768., np.abs(d).max() >= 32767
    elif d.dtype == np.int32: z, full = d/2.**31, np.abs(d.astype(np.int64)).max() >= 2**31-256  # 24-bit is left-justified
    elif d.dtype == np.float32: z, full = d.astype(float), np.abs(d).max() >= .999
    else: raise ValueError(f'{path}: unsupported sample format {d.dtype}')
    z = np.asarray(z, float); issues = []
    if rate not in RATES: issues.append(f'sample rate {rate} not in {RATES}')
    if full: issues.append('clipped (full-scale samples)')
    if not np.all(np.isfinite(z)): issues.append('nonfinite samples')
    peak = float(np.abs(z).max()) if len(z) else 0.
    if peak < .01: issues.append(f'too quiet (peak {20*np.log10(max(peak, 1e-12)):.1f} dBFS < -40)')
    # DC from the quietest 10% of 50 ms blocks; decaying low notes are not DC.
    n = max(1, round(.05*rate)); blocks = z[:len(z)//n*n].reshape(-1, n) if len(z) >= n else z[None, :]
    quiet = blocks[np.argsort((blocks**2).mean(1))[:max(1, len(blocks)//10)]]
    if abs(float(quiet.mean())) > 1e-3: issues.append('DC offset > 1e-3')
    if len(z) > 600*rate: issues.append('longer than 10 minutes')
    return z, rate, issues

def segment_issues(z, rate, seg):
    a, b = round(seg['start_s']*rate), round(seg['end_s']*rate); e = round(EDGE_S*rate); out = []
    if a < 0 or b > len(z) or b-a < 4*e: return [f"{seg['id']}: bounds outside file or shorter than {4*EDGE_S}s"]
    peak = np.abs(z[a:b]).max(); rms = lambda u: np.sqrt(np.mean(u*u))
    for name, u in (('start', z[a:a+e]), ('end', z[b-e:b])):
        if rms(u) > peak*10**(SILENCE_DB/20): out.append(f"{seg['id']}: {name} {EDGE_S}s not silent (muted strings required)")
    if seg['kind'] == 'steady' and seg['end_s']-seg['start_s'] < 4.5: out.append(f"{seg['id']}: steady segment shorter than 4.5 s")
    return out

def reference_onsets(z, rate, rise_db=9., gap_ms=12., floor_db=-60., look_ms=40.):
    """Offline reference onsets for review. Not the engine's detector.

    Detection: zero-phase 1-8 kHz energy (pick/finger contact). The mean of the
    next 5 ms must exceed by rise_db the largest 5 ms mean of the previous
    look_ms (one period of a 25 Hz string), so periodic bright sustain and
    vibrato do not retrigger. A full-band feature is not used: below ~60 Hz a
    short window follows the waveform phase. Position: the first sample, from
    10 ms before, where the causal 0.25 ms envelope of a steeper (8th-order)
    1-8 kHz band exceeds both 2x its maximum over the previous look_ms and
    -20 dB of the attack peak (causal, so no filter pre-echo).
    """
    hop = max(1, round(.0005*rate)); a5, L = round(.005*rate), round(look_ms/1000*rate)
    e = signal.sosfiltfilt(signal.butter(3, [1000, 8000], fs=rate, btype='bandpass', output='sos'), z)**2
    c = np.concatenate(([0.], np.cumsum(e))); m5 = np.zeros(len(e)); m5[:len(e)-a5] = (c[a5:len(e)]-c[:len(e)-a5])/a5
    mx = maximum_filter1d(m5, size=L, origin=(L-1)//2)  # max of m5 over [t-L+1, t]
    idx = np.arange(L+a5, len(e)-a5, hop); ahead, behind = m5[idx], mx[idx-a5]; floor = e.max()*10**(floor_db/10)
    r = 10*np.log10((ahead+floor*1e-3)/(behind+floor*1e-3)); ok = (r >= rise_db) & (ahead >= floor)
    peaks, _ = signal.find_peaks(np.where(ok, r, 0.), distance=max(1, round(gap_ms*rate/1000/hop)))
    sc = signal.butter(4, [1000, 8000], fs=rate, btype='bandpass', output='sos')
    w = max(1, round(.00025*rate)); ec = np.convolve(signal.sosfilt(sc, z)**2, np.ones(w)/w)[:len(z)]
    out = []
    for p in peaks:
        s0 = int(idx[p]); lo = max(0, s0-round(.010*rate))
        bg = ec[max(0, s0-L):max(1, lo)].max(); top = ec[s0:s0+a5].max()
        hit = np.nonzero(ec[lo:s0+a5] > max(2*bg, .01*top)+1e-30)[0]
        out.append((lo+int(hit[0]) if len(hit) else s0, float(r[p])))
    return out

def load_manifest(path):
    m = json.loads(Path(path).read_text()); base = Path(path).resolve().parent
    for t in m['takes']:
        for k in ('wav', 'labels'):
            if t.get(k): t[k+'_path'] = (base/t[k]).resolve()
    return m

def read_labels(path):
    with open(path, newline='') as f: return [int(r['sample']) for r in csv.DictReader(f) if r.get('keep', 'yes').strip().lower() in ('yes', 'y', '1', 'true')]

def check(m, need_labels=True):
    """All blocking problems for a manifest; empty list means ready."""
    p = []
    if not m.get('takes'): return ['no takes']
    for t in m['takes']:
        tid = t.get('id', '?')
        if not t.get('wav'): p.append(f'{tid}: no recording (wav is null)'); continue
        if not t['wav_path'].exists(): p.append(f'{tid}: {t["wav_path"]} missing'); continue
        z, rate, issues = read_wav(t['wav_path'], t.get('channel', 0)); p += [f'{tid}: {i}' for i in issues]
        if t.get('rate') and t['rate'] != rate: p.append(f'{tid}: manifest rate {t["rate"]} != file {rate}')
        for s in t['segments']:
            if s['kind'] not in ROLES: p.append(f"{tid}/{s['id']}: unknown kind {s['kind']}")
            else: p += [f'{tid}/{i}' for i in segment_issues(z, rate, s)]
            if s['kind'] == 'steady' and not s.get('f0_hint_hz'): p.append(f"{tid}/{s['id']}: steady needs f0_hint_hz")
        if not need_labels: continue
        if not t.get('labels') or not t['labels_path'].exists(): p.append(f'{tid}: no reviewed labels'); continue
        if not t.get('labels_reviewed'): p.append(f'{tid}: labels_reviewed is false')
        lab = read_labels(t['labels_path'])
        for s in t['segments']:
            n = sum(round(s['start_s']*rate) <= v < round(s['end_s']*rate) for v in lab)
            if s['kind'] in MIN_NOTES and n < MIN_NOTES[s['kind']]: p.append(f"{tid}/{s['id']}: {n} labels < {MIN_NOTES[s['kind']]}")
            if s['kind'] in ('steady', 'guard') and n != 1: p.append(f"{tid}/{s['id']}: {s['kind']} needs exactly one label, has {n}")
            if s['kind'] == 'steady' and n == 1:
                v = next(v for v in lab if round(s['start_s']*rate) <= v < round(s['end_s']*rate))
                if v+round(3.25*rate) > round(s['end_s']*rate): p.append(f"{tid}/{s['id']}: steady note needs 3.25 s after its label inside the segment")
    return p

def cmd_plan(a):
    m = load_manifest(a.manifest); p = check(m)
    print(json.dumps({'status': 'READY_TO_FREEZE' if not p else 'MISSING_INPUT', 'problems': p}, ensure_ascii=False, indent=1))
    return 0

def cmd_label(a):
    m = load_manifest(a.manifest); t = next(t for t in m['takes'] if t['id'] == a.take)
    z, rate, issues = read_wav(t['wav_path'], t.get('channel', 0))
    if issues: sys.exit(f'{a.take}: ' + '; '.join(issues))
    rows = []
    for s in t['segments']:
        lo, hi = round(s['start_s']*rate), round(s['end_s']*rate)
        for v, r in reference_onsets(z[lo:hi], rate):
            rows.append({'sample': lo+v, 'time_s': f'{(lo+v)/rate:.5f}', 'segment': s['id'], 'rise_db': f'{r:.1f}', 'keep': 'yes', 'note': ''})
    with open(a.out, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]) if rows else ['sample', 'time_s', 'segment', 'rise_db', 'keep', 'note']); w.writeheader(); w.writerows(rows)
    print(f'{len(rows)} candidate onsets -> {a.out}. Review every row by ear/eye; set keep=no for non-attacks, add missed attacks, then set labels_reviewed=true.')
    return 0

def git_commit():
    try: return subprocess.run(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
    except Exception: return None

def freeze_record(m, manifest, renderer, engine):
    return {'manifest_sha256': sha(Path(manifest)), 'renderer_sha256': sha(Path(renderer)), 'spec_sha256': sha(SPEC), 'engine': engine,
            'protocol_sha256': sha(ROOT/'docs'/'TRANSPOSE_T6_DI_PROTOCOL.md'), 'script_sha256': sha(Path(__file__)),
            'takes': {t['id']: {'wav_sha256': sha(t['wav_path']), 'labels_sha256': sha(t['labels_path'])} for t in m['takes']}}

def cmd_freeze(a):
    m = load_manifest(a.manifest); p = check(m)
    if p: sys.exit('not ready:\n  ' + '\n  '.join(p))
    rec = {'kind': 'T6_real_DI_preregistration', 'git_commit': git_commit(), **freeze_record(m, a.manifest, a.renderer.resolve(), a.engine),
           'rules': ['one run per engine', 'no change to labels, segments, metrics, criteria or parameters after seeing results', 'all failures reported']}
    Path(a.out).write_text(json.dumps(rec, indent=1)+'\n'); print(f'pre-registered -> {a.out}. Commit and push it before `run`.')
    return 0

def render(renderer, x, rate, block, shift, engine, work):
    ip, op = work/'in.f32', work/'out.f32'; x.astype('<f4').tofile(ip)
    r = subprocess.run([str(renderer), str(ip), str(op), str(rate), str(block), str(shift), engine, str(work/'cb.csv')], check=True, capture_output=True, text=True)
    native = json.loads(r.stdout)
    if native.get('onset_log_truncated'): raise RuntimeError('engine onset log truncated; shorten the segment')
    return np.fromfile(op, dtype='<f4').reshape(-1, 2), native, sha(ip), sha(op)

def associate(detected, labels, rate, before_ms):
    """Each label matches the first unused detection in [label-before_ms, label+10ms]."""
    used, missed, lags = set(), 0, []
    for on in labels:
        c = [i for i, d in enumerate(detected) if i not in used and -before_ms*rate/1000 <= d-on <= .01*rate]
        if c: used.add(c[0]); lags.append((detected[c[0]]-on)*1000/rate)
        else: missed += 1
    # Startup (<10 ms of the rendered segment) is excluded, as for synthetic inputs.
    return {'missed_events': missed, 'unmatched_resets': [int(d) for i, d in enumerate(detected) if i not in used and d >= .01*rate], 'detector_lags_ms': lags}

def steady_relative(x, y, rate, label, f0_hint, shift):
    """Output pitch error and warble relative to the input note (real strings drift)."""
    w0 = label+round(.5*rate); win = lambda u: np.column_stack((u, u))[w0-round(.75*rate):w0+round(2.75*rate)]
    src = tone_metrics(win(x), rate, f0_hint, 1.); f_in = src['dominant_hz']
    out = tone_metrics(win(y), rate, f_in*2**(shift/12), 1.)
    return {'input_hz': f_in, 'input_warble_cents': src['warble_p95_p5_cents'], 'output_warble_cents': out['warble_p95_p5_cents'],
            'warble_added_cents': out['warble_p95_p5_cents']-src['warble_p95_p5_cents'], 'error_cents': out['dominant_error_cents']}

def score_take(t, renderer, engine, shifts, block, before_ms, work):
    z, rate, _ = read_wav(t['wav_path'], t.get('channel', 0)); lab = read_labels(t['labels_path']); rows = []
    for s in t['segments']:
        lo, hi = round(s['start_s']*rate), round(s['end_s']*rate); roles = ROLES[s['kind']]
        on = [v-lo for v in lab if lo <= v < hi]; x = np.column_stack((z[lo:hi], z[lo:hi]))
        meta = {'rate': rate, 'onsets': on, 'name': s['kind']}
        before = attacks(x, meta) if 'attack' in roles else None
        for shift in shifts:
            y, native, ish, osh = render(renderer, x, rate, block, shift, engine, work)
            row = {'take': t['id'], 'instrument': t.get('instrument'), 'segment': s['id'], 'kind': s['kind'], 'rate': rate, 'shift': shift,
                   'roles': sorted(roles), 'labels': len(on), 'input_sha256': ish, 'output_sha256': osh,
                   'onset_resets': native['onset_resets'], 'low_onset_resets': native.get('low_onset_resets'),
                   'onset_association': associate(native['onset_samples'], on, rate, before_ms)}
            if before is not None:
                extra = np.array(attacks(y, meta)['attack_lags_ms'])-np.array(before['attack_lags_ms'])
                valid = np.array(before['attack_lags_ms']) <= 10
                row['attack_valid_events'] = int(valid.sum()); row['attack_additional_ms'] = [float(v) for v in extra[valid]]
            if roles & {'short', 'long'}: row['lowband_additional_ms'] = lowband_extra(x, y, on, rate)
            if 'steady' in roles: row['steady'] = steady_relative(x[:, 0], y[:, 0], rate, on[0], s['f0_hint_hz'], shift)
            rows.append(row)
        print(t['id'], s['id'], flush=True)
    return rows

def evaluate_di(spec, rows):
    """Real-DI verdicts. Same limits as the synthetic spec; adaptations are in the protocol doc."""
    T = {t['id']: t for t in spec['targets']}; out = []
    att = [v for r in rows for v in r.get('attack_additional_ms', [])]
    n_valid = sum(r.get('attack_valid_events', 0) for r in rows if r['shift'] == rows[0]['shift'])
    lim = T['T1_attack']['limit_ms']
    out.append({'id': 'T1_attack', 'limit_ms': lim, 'observed': {'max_ms': round(max(att), 3) if att else None, 'valid_events_per_shift': n_valid},
                'verdict': 'INSUFFICIENT' if n_valid < 20 else ('PASS' if max(att) <= lim else 'FAIL')})
    def low(sel, shifts):
        v = [x for r in rows if sel(r) and r['shift'] in shifts for x in r.get('lowband_additional_ms', [])]
        return round(max(v), 3) if v else None
    d = T['T2_low_onset_down']; obs = {'short_max_ms': low(lambda r: 'short' in r['roles'], d['shifts']), 'long_max_ms': low(lambda r: 'long' in r['roles'], d['shifts'])}
    vals = [v for v in obs.values() if v is not None]
    out.append({'id': d['id'], 'limit_ms': d['limit_ms'], 'observed': obs, 'verdict': 'INSUFFICIENT' if len(vals) < 2 else ('PASS' if max(vals) <= d['limit_ms'] else 'FAIL')})
    u = T['T3_low_onset_up']; obs = {}
    for inst in sorted({r['instrument'] for r in rows}):
        obs[inst] = {'short_max_ms': low(lambda r: r['instrument'] == inst and 'short' in r['roles'], u['shifts']),
                     'long_max_ms': low(lambda r: r['instrument'] == inst and 'long' in r['roles'], u['shifts'])}
    out.append({'id': u['id'], 'observed': obs, 'synthetic_regression_ceiling_ms': u['regression_ceiling_ms'], 'verdict': 'REPORT'})
    q = T['T4_quality']; st = [r['steady'] for r in rows if 'steady' in r]
    unmatched = [(r['take'], r['segment'], r['shift'], round(d/r['rate'], 4)) for r in rows for d in r['onset_association']['unmatched_resets']]
    missed = sum(r['onset_association']['missed_events'] for r in rows)
    low_extra = sum(max(0, (r['low_onset_resets'] or 0)-1) for r in rows if r['roles'] and set(r['roles']) & {'markers', 'guard', 'steady'})
    obs = {'steady_warble_added_max_cents': round(max(s['warble_added_cents'] for s in st), 3) if st else None,
           'steady_abs_error_max_cents': round(max(abs(s['error_cents']) for s in st), 3) if st else None,
           'unmatched_resets': len(unmatched), 'unmatched_where': unmatched[:50], 'missed_events': missed,
           'low_resets_on_markers_or_sustain_after_first': low_extra}
    ok = (st and obs['steady_warble_added_max_cents'] <= q['steady_warble_max_cents'] and obs['steady_abs_error_max_cents'] <= q['steady_abs_error_max_cents']
          and len(unmatched) <= q['unmatched_resets_max'] and missed <= q['missed_events_max'] and low_extra <= q['low_resets_on_markers_max'])
    out.append({'id': q['id'], 'observed': obs, 'verdict': 'INSUFFICIENT' if not st else ('PASS' if ok else 'FAIL')})
    out.append({'id': 'T5_realtime', 'verdict': 'NOT_MEASURED_BY_THIS_SCRIPT'})
    out.append({'id': 'T6_external', 'verdict': 'PARTIAL', 'observed': {'real_DI_metrics': 'this report', 'listening': 'NOT_RUN', 'DAW': 'NOT_RUN', 'fixed_PDC': 'NOT_IMPLEMENTED', 'product_integration': 'NOT_RUN'}})
    return {'targets': out}

def cmd_run(a):
    m = load_manifest(a.manifest); p = check(m)
    if p: sys.exit('not ready:\n  ' + '\n  '.join(p))
    renderer = a.renderer.resolve()
    pre = json.loads(Path(a.prereg).read_text()); now = freeze_record(m, a.manifest, renderer, a.engine)
    diff = [k for k in now if now[k] != pre.get(k)]
    if diff: sys.exit(f'differs from pre-registration: {diff}')
    spec = json.loads(SPEC.read_text()); a.out.mkdir(parents=True, exist_ok=True); rows = []
    with tempfile.TemporaryDirectory() as w:
        for t in m['takes']: rows += score_take(t, renderer, a.engine, spec['conditions']['shifts'], spec['conditions']['block'], m.get('label_tolerance_before_ms', 2.), Path(w))
    report = evaluate_di(spec, rows)
    report.update(engine=a.engine, renderer_sha256=sha(renderer), spec_sha256=sha(SPEC), platform=platform.platform(), real_DI=True,
                  preregistered=sha(Path(a.prereg)), listening='NOT_RUN', actual_DAW='NOT_RUN', fixed_PDC='NOT_IMPLEMENTED')
    (a.out/'results.json').write_text(json.dumps({'report': report, 'rows': rows}, indent=1)+'\n')
    for t in report['targets']: print(t['id'], t['verdict'], json.dumps(t.get('observed', {}), ensure_ascii=False))
    return 0

def cmd_selftest(a):
    """Synthetic WAVs with known onsets: labeler accuracy and scorer parity. Not T6 evidence."""
    out = a.out; out.mkdir(parents=True, exist_ok=True); rate = 48000; pad = np.zeros(round(.6*rate))
    report = {'kind': 'pipeline_selftest_synthetic_not_T6_evidence', 'labeler': []}
    takes = []
    for inst, f in FREQUENCIES.items():
        parts, segs, truth, t0 = [], [], [], 0
        def add(sid, kind, x, on, **kw):
            nonlocal t0
            x = x[:, 0].astype(float)*np.minimum(1, np.arange(len(x))/(.001*rate))*np.minimum(1, np.arange(len(x))[::-1]/(.05*rate))
            z = np.concatenate((pad, x, pad)); parts.append(z)
            segs.append({'id': sid, 'kind': kind, 'start_s': t0/rate, 'end_s': (t0+len(z))/rate, **kw})
            truth.extend(t0+len(pad)+o for o in on); t0 += len(z)
        x, m = generate('riff75', rate, inst); add('riff', 'riff', x, m['onsets'])
        x, on = holdout_input(f, 'random_soft_after_loud', 8139, rate); add('dynamics', 'dynamics', x, on)
        x, on = long_notes(f, rate); add('long', 'long', x, on)
        x, m = generate('quiet_markers_over_sustain', rate, inst); add('ghost', 'ghost', x, [0]+m['onsets'])
        x, m = generate('vibrato', rate, inst); add('guard', 'guard', x, [0])
        u = np.arange(round(5*rate))/rate; st = sum(.12*a*np.sin(2*np.pi*f*h*u+.19*h) for h, a in enumerate([1, .8, .5, .3, .2, .16, .12, .1], 1))
        add('steady', 'steady', st[:, None], [0], f0_hint_hz=f)
        wav = out/f'{inst}.wav'; wavfile.write(wav, rate, (np.concatenate(parts)*.9).astype(np.float32))
        # Steady fixtures start at full level (no attack); its single label is the segment start.
        z, _, _ = read_wav(wav); found = [s for s, _ in reference_onsets(z, rate)]; att = truth
        err = [min(found, key=lambda s: abs(s-v))-v for v in att]
        extra = [round(s/rate, 4) for s in found if min(abs(s-v) for v in att) > .005*rate]
        report['labeler'].append({'take': inst, 'truth': len(att), 'found': len(found), 'max_abs_error_ms': round(max(abs(e) for e in err)*1000/rate, 3),
                                  'within_1ms': sum(abs(e) <= rate/1000 for e in err), 'extra_candidates_s': extra})
        lp = out/f'{inst}-labels.csv'
        with open(lp, 'w', newline='') as fh:
            w = csv.writer(fh); w.writerow(['sample', 'keep'])
            for v in truth: w.writerow([v, 'yes'])
        takes.append({'id': inst, 'instrument': inst, 'wav': wav.name, 'labels': lp.name, 'labels_reviewed': True, 'segments': segs})
    man = out/'selftest-manifest.json'; man.write_text(json.dumps({'version': 1, 'status': 'SELFTEST_SYNTHETIC', 'takes': takes}, indent=1))
    # Fixtures are shorter than a real take; only the minimum-note counts may fail.
    m = load_manifest(man); report['manifest_problems'] = p = [x for x in check(m) if 'labels <' not in x]
    spec = json.loads(SPEC.read_text())
    with tempfile.TemporaryDirectory() as w:
        rows = []
        for t in m['takes']: rows += score_take(t, a.renderer.resolve(), a.engine, [-2, 2], 128, 2., Path(w))
    report['targets'] = evaluate_di(spec, rows)['targets']; v = {t['id']: t['verdict'] for t in report['targets']}
    report['ok'] = bool(not p and all(l['within_1ms'] == l['truth'] for l in report['labeler']) and all(v[k] == 'PASS' for k in ('T1_attack', 'T2_low_onset_down', 'T4_quality')))
    (out/'selftest.json').write_text(json.dumps(report, indent=1)+'\n'); print(json.dumps(report, indent=1)); return 0 if report['ok'] else 1

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter); sub = p.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('plan'); s.add_argument('--manifest', required=True); s.set_defaults(f=cmd_plan)
    s = sub.add_parser('label'); s.add_argument('--manifest', required=True); s.add_argument('--take', required=True); s.add_argument('--out', required=True); s.set_defaults(f=cmd_label)
    for name, f in (('freeze', cmd_freeze), ('run', cmd_run)):
        s = sub.add_parser(name); s.add_argument('--manifest', required=True); s.add_argument('--renderer', type=Path, required=True)
        s.add_argument('--engine', default='corr40-split-v2')
        if name == 'freeze': s.add_argument('--out', required=True)
        else: s.add_argument('--prereg', required=True); s.add_argument('--out', type=Path, required=True)
        s.set_defaults(f=f)
    s = sub.add_parser('selftest'); s.add_argument('--renderer', type=Path, required=True); s.add_argument('--out', type=Path, required=True)
    s.add_argument('--engine', default='corr40-split-v2'); s.set_defaults(f=cmd_selftest)
    a = p.parse_args(); sys.exit(a.f(a))

if __name__ == '__main__':
    main()

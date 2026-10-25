#!/usr/bin/env python3
"""T6 blind listening: product Transpose vs the corr40-split-v3 candidate on real DI.

Uses the same take manifest as di_validation.py (labels are not needed).
Design, listener rules and the decision rule are fixed in
docs/TRANSPOSE_T6_LISTENING_DAW.md before any listener result exists.

  prepare   render, align, loudness-match, write blind ABX + paired-rating sets
  score     pool every listener's sheets against the hidden key; verdict
  selftest  synthetic WAVs + simulated listeners (pipeline check, not evidence)

Offline clips judge sound quality only. Latency feel needs live playing in a
DAW (the DAW checklist); a hidden constant offset is removed here on purpose.
"""
import argparse, csv, json, math, random, sys, tempfile
from pathlib import Path
import numpy as np
from scipy.io import wavfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from transpose_measure import lufs_integrated  # noqa: E402
from blind_listening import _binom_tail  # noqa: E402
from di_validation import load_manifest, read_wav, render, check  # noqa: E402
from legacy_metrics import sha  # noqa: E402

# T6 candidate: v3 (block-invariant). v2 stays the synthetic-target record.
CONDITIONS = {'product': 'baseline', 'candidate': 'corr40-split-v3'}
SHIFTS = (-2, 2)
PAIRED_KINDS = ('riff', 'dynamics', 'long')
ABX_KIND, ABX_TRIALS, PAIRED_REPEATS = 'riff', 12, 2
CLIP_S, TARGET_LUFS, PEAK_DBFS = 10., -23., -1.
ATTRS = ('attack', 'low_end', 'artifacts')  # 0-5; artifacts: 0 none, 5 severe
GATE = {'min_listeners': 3, 'max_attr_deficit': .5, 'min_preference_share': .5}

def clip(t, seg, z, rate, renderer, work):
    """Both conditions at each shift, aligned and loudness-matched (mono float32)."""
    a = round(seg['start_s']*rate); b = min(round(seg['end_s']*rate), a+round(CLIP_S*rate))
    x = np.column_stack((z[a:b], z[a:b])); out = {}
    for shift in SHIFTS:
        ys = {}
        for cond, engine in CONDITIONS.items():
            y, native, _, _ = render(renderer, x, rate, 128, shift, engine, work)
            # Remove the product's advertised FFT latency (constant); the
            # candidate's variable 2-15 ms is left as is. Offline clips judge
            # sound, not timing.
            d = max(0, int(native.get('nominal_samples', 0)))
            ys[cond] = np.concatenate((y[d:, 0], np.zeros(d))).astype(float)
        gains = {c: 10**((TARGET_LUFS-lufs_integrated(y, rate))/20) for c, y in ys.items()}
        peak = max(np.abs(y).max()*gains[c] for c, y in ys.items())
        trim = min(1., 10**(PEAK_DBFS/20)/peak) if peak > 0 else 1.
        out[shift] = {c: (y*gains[c]*trim).astype(np.float32) for c, y in ys.items()}
        out[shift]['gain_db'] = {c: round(20*math.log10(gains[c]*trim), 2) for c in ys}
    return out

def cmd_prepare(a):
    m = load_manifest(a.manifest); p = [x for x in check(m, need_labels=False)]
    if p: sys.exit('not ready:\n  ' + '\n  '.join(p))
    out = Path(a.out); (out/'clips').mkdir(parents=True, exist_ok=True); (out/'listener').mkdir(exist_ok=True)
    rng = random.Random(a.seed); key = {'seed': a.seed, 'design': {'shifts': SHIFTS, 'paired_kinds': PAIRED_KINDS, 'abx_kind': ABX_KIND,
          'abx_trials': ABX_TRIALS, 'paired_repeats': PAIRED_REPEATS, 'clip_s': CLIP_S, 'target_lufs': TARGET_LUFS}, 'abx': [], 'paired': [], 'gains_db': {}}
    clips = []
    with tempfile.TemporaryDirectory() as w:
        for t in m['takes']:
            z, rate, _ = read_wav(t['wav_path'], t.get('channel', 0))
            for seg in t['segments']:
                if seg['kind'] not in PAIRED_KINDS: continue
                for shift, ys in clip(t, seg, z, rate, a.renderer.resolve(), Path(w)).items():
                    names = {}
                    for cond in CONDITIONS:
                        names[cond] = f"{t['id']}-{seg['id']}-{shift:+d}-{cond}.wav"
                        wavfile.write(out/'clips'/names[cond], rate, ys[cond])
                    key['gains_db'][f"{t['id']}/{seg['id']}/{shift:+d}"] = ys['gain_db']
                    clips.append({'take': t['id'], 'segment': seg['id'], 'kind': seg['kind'], 'shift': shift, 'rate': rate, 'files': names})
    # Listener files carry neutral names only; the key maps them back.
    def neutral(src, tag):
        name = f'{tag}.wav'; (out/'listener'/name).write_bytes((out/'clips'/src).read_bytes()); return name
    abx = [['trial', 'play_A', 'play_B', 'play_X', 'answer_A_or_B', 'confidence_1_5', 'notes']]
    n = 0
    for c in [c for c in clips if c['kind'] == ABX_KIND]:
        for _ in range(ABX_TRIALS):
            n += 1; swap = rng.random() < .5; xa = rng.choice('AB')
            A, B = ('candidate', 'product') if swap else ('product', 'candidate')
            files = {k: neutral(c['files'][v], f'abx{n:03d}-{k}') for k, v in (('A', A), ('B', B), ('X', A if xa == 'A' else B))}
            key['abx'].append({'trial': n, 'take': c['take'], 'segment': c['segment'], 'shift': c['shift'], 'A': A, 'B': B, 'x_truth': xa})
            abx.append([n, files['A'], files['B'], files['X'], '', '', ''])
    paired = [['trial', 'play_P', 'play_Q', 'preferred_P_Q_or_none'] + [f'{x}_{s}' for x in ATTRS for s in 'PQ'] + ['notes']]
    n = 0
    order = [c for c in clips for _ in range(PAIRED_REPEATS)]; rng.shuffle(order)
    for c in order:
        n += 1; P, Q = ('candidate', 'product') if rng.random() < .5 else ('product', 'candidate')
        key['paired'].append({'trial': n, 'take': c['take'], 'segment': c['segment'], 'shift': c['shift'], 'P': P, 'Q': Q})
        paired.append([n, neutral(c['files'][P], f'pair{n:03d}-P'), neutral(c['files'][Q], f'pair{n:03d}-Q')] + ['']*(len(paired[0])-3))
    # ABX trials are shuffled across takes/shifts too, so a listener cannot learn a block.
    head, body = abx[0], abx[1:]; perm = list(range(len(body))); rng.shuffle(perm)
    key['abx_order'] = [body[i][0] for i in perm]
    for name, rows in (('abx_sheet.csv', [head]+[body[i] for i in perm]), ('paired_sheet.csv', paired)):
        with open(out/'listener'/name, 'w', newline='') as f: csv.writer(f).writerows(rows)
    key['manifest_sha256'] = sha(Path(a.manifest)); key['renderer_sha256'] = sha(a.renderer.resolve())
    (out/'blind_key.json').write_text(json.dumps(key, indent=1)+'\n')
    print(f"{len(key['abx'])} ABX + {len(key['paired'])} paired trials -> {out/'listener'}; keep {out/'blind_key.json'} from listeners")
    return 0

def score(key, sheets):
    """sheets: {listener: (abx_rows, paired_rows)}. Pooled verdict per the protocol."""
    res = {'listeners': {}, 'pooled': {}}
    ratings = {c: {x: [] for x in ATTRS} for c in CONDITIONS}; pref = {'product': 0, 'candidate': 0, 'none': 0}; notes = []
    for who, (abx_rows, paired_rows) in sheets.items():
        A = {int(r['trial']): r for r in abx_rows if r.get('trial')}; k = n = 0
        for t in key['abx']:
            ans = A.get(t['trial'], {}).get('answer_A_or_B', '').strip().upper()
            if ans in ('A', 'B'): n += 1; k += ans == t['x_truth']
        res['listeners'][who] = {'abx_correct': k, 'abx_answered': n, 'abx_p_one_sided': _binom_tail(k, n) if n else None}
        P = {int(r['trial']): r for r in paired_rows if r.get('trial')}
        for t in key['paired']:
            r = P.get(t['trial'])
            if not r: continue
            choice = r.get('preferred_P_Q_or_none', '').strip().upper()
            pref[t[choice] if choice in ('P', 'Q') else 'none'] += 1 if choice in ('P', 'Q', 'NONE') else 0
            for x in ATTRS:
                for s in 'PQ':
                    v = (r.get(f'{x}_{s}') or '').strip()
                    if v: ratings[t[s]][x].append(float(v))
            if (r.get('notes') or '').strip(): notes.append({'listener': who, 'take': t['take'], 'segment': t['segment'], 'shift': t['shift'], 'note': r['notes'].strip()})
    mean = {c: {x: (round(float(np.mean(v)), 3) if v else None) for x, v in d.items()} for c, d in ratings.items()}
    # Higher attack/low_end is better; higher artifacts is worse.
    deficit = {x: (None if mean['candidate'][x] is None or mean['product'][x] is None else
                   round((mean['product'][x]-mean['candidate'][x]) if x != 'artifacts' else (mean['candidate'][x]-mean['product'][x]), 3)) for x in ATTRS}
    decided = pref['product']+pref['candidate']; share = pref['candidate']/decided if decided else None
    res['pooled'] = {'means': mean, 'candidate_deficit': deficit, 'preference': pref, 'candidate_share_of_decided': share, 'notes': notes}
    if len(sheets) < GATE['min_listeners'] or share is None or any(v is None for v in deficit.values()): verdict = 'INSUFFICIENT'
    else: verdict = 'PASS' if share >= GATE['min_preference_share'] and all(v <= GATE['max_attr_deficit'] for v in deficit.values()) else 'FAIL'
    res['verdict'] = verdict; res['gate'] = GATE
    return res

def read_csv(p):
    with open(p, newline='') as f: return list(csv.DictReader(f))

def cmd_score(a):
    key = json.loads(Path(a.key).read_text()); sheets = {}
    for d in a.listener:
        d = Path(d); sheets[d.name] = (read_csv(d/'abx_sheet.csv'), read_csv(d/'paired_sheet.csv'))
    r = score(key, sheets); Path(a.out).write_text(json.dumps(r, indent=1, ensure_ascii=False)+'\n')
    print(json.dumps({k: r[k] for k in ('verdict', 'listeners')} | {'pooled': {k: v for k, v in r['pooled'].items() if k != 'notes'}}, indent=1, ensure_ascii=False))
    return 0

def cmd_selftest(a):
    """Synthetic takes from di_validation selftest; three simulated listeners."""
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    import subprocess
    st = out/'di'
    if not (st/'selftest-manifest.json').exists():
        subprocess.run([sys.executable, str(Path(__file__).with_name('di_validation.py')), 'selftest', '--renderer', str(a.renderer), '--out', str(st)], check=True, capture_output=True)
    cmd_prepare(argparse.Namespace(manifest=str(st/'selftest-manifest.json'), renderer=a.renderer, out=str(out/'run'), seed=7))
    key = json.loads((out/'run'/'blind_key.json').read_text()); rng = random.Random(1); sheets = {}
    for who, skill in (('sim-perfect', 1.), ('sim-chance', .5), ('sim-prefers-candidate', .8)):
        abx = read_csv(out/'run'/'listener'/'abx_sheet.csv'); paired = read_csv(out/'run'/'listener'/'paired_sheet.csv')
        truth = {t['trial']: t for t in key['abx']}
        for r in abx:
            t = truth[int(r['trial'])]; r['answer_A_or_B'] = t['x_truth'] if rng.random() < skill else ('B' if t['x_truth'] == 'A' else 'A')
        ptruth = {t['trial']: t for t in key['paired']}
        for r in paired:
            t = ptruth[int(r['trial'])]; good = 'P' if t['P'] == 'candidate' else 'Q'
            r['preferred_P_Q_or_none'] = good if rng.random() < skill else ('Q' if good == 'P' else 'P')
            for x in ATTRS:
                for s in 'PQ': r[f'{x}_{s}'] = str(3 if t[s] == 'candidate' else 2) if x != 'artifacts' else str(1 if t[s] == 'candidate' else 2)
        sheets[who] = (abx, paired)
    r = score(key, sheets)
    ok = (r['verdict'] == 'PASS' and r['listeners']['sim-perfect']['abx_correct'] == len(key['abx'])
          and len(list((out/'run'/'listener').glob('*.wav'))) == 3*len(key['abx'])+2*len(key['paired']))
    summary = {'kind': 'listening_selftest_synthetic_not_T6_evidence', 'abx_trials': len(key['abx']), 'paired_trials': len(key['paired']),
               'gains_db_range': [min(min(v.values()) for v in key['gains_db'].values()), max(max(v.values()) for v in key['gains_db'].values())],
               'score': {k: r[k] for k in ('verdict', 'listeners')} | {'pooled': {k: v for k, v in r['pooled'].items() if k != 'notes'}}, 'ok': ok}
    (out/'listening_selftest.json').write_text(json.dumps(summary, indent=1)+'\n'); print(json.dumps(summary, indent=1))
    return 0 if ok else 1

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter); s = p.add_subparsers(dest='cmd', required=True)
    q = s.add_parser('prepare'); q.add_argument('--manifest', required=True); q.add_argument('--renderer', type=Path, required=True)
    q.add_argument('--out', required=True); q.add_argument('--seed', type=int, required=True); q.set_defaults(f=cmd_prepare)
    q = s.add_parser('score'); q.add_argument('--key', required=True); q.add_argument('--listener', action='append', required=True, help='folder with that listener\'s abx_sheet.csv and paired_sheet.csv')
    q.add_argument('--out', required=True); q.set_defaults(f=cmd_score)
    q = s.add_parser('selftest'); q.add_argument('--renderer', type=Path, required=True); q.add_argument('--out', required=True); q.set_defaults(f=cmd_selftest)
    a = p.parse_args(); sys.exit(a.f(a))

if __name__ == '__main__':
    main()

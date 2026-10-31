#!/usr/bin/env python3
"""Independent PR #234 validation; synthetic evidence, not product acceptance.

Build the unmodified main probe and the candidate with the same JUCE/compiler.
freeze and commit/push registration before running any phase. Each phase uses
an exclusive start marker and each worker owns its temporary directory.
CPU must run alone, after the other phases have finished.
"""
import argparse
import concurrent.futures
import datetime
import gzip
import json
import platform
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from latency_targets import inputs, evaluate
from legacy_metrics import sha
from v3sinc_validation import tone, interharm, equivalence
from residual_validation import net_db
from weak_attack_study import FREQUENCIES
from weak_attack_holdout import holdout_input

ROOT = Path(__file__).resolve().parents[2]
SPEC = ROOT / 'Validation/transpose-latency-targets.json'
RATES = [44100, 48000, 96000]
SHIFTS = [-2, -1, 1, 2]
ENGINES = ['corr40-split-v3', 'corr40-split-v3-edge',
           'corr40-split-v3-sinc', 'corr40-split-v3-sinc-edge']
HOLDOUT = [2026103101, 2026103107]
LONG = [2026103113]
SWEEP_SEED = 2026103129


def write(path, obj):
    path.write_text(json.dumps(obj, indent=2, allow_nan=False) + '\n')


def packed(path, obj):
    with gzip.open(path, 'wt') as f:
        json.dump(obj, f, allow_nan=False)


def sources():
    paths = sorted((ROOT / 'Tools/TransposeNext').glob('*'))
    return {str(p.relative_to(ROOT)): sha(p) for p in paths + [SPEC]
            if p.suffix in ('.py', '.cpp', '.h', '.json') and p.is_file()}


def travel(rate, shift):
    fade = int(np.ceil(rate * .002))
    sfade = min(4 * fade, max(fade, int(np.ceil(rate * .004))))
    guard = abs(1 - 2 ** (shift / 12)) * max(fade, sfade) + 2
    return (rate * .040 - guard - (rate * .002 + guard)) / 4


def render(exe, tmp, x, rate, shift, engine, block=128, cpu=False):
    ip, op, cb = tmp / 'in.f32', tmp / 'out.f32', tmp / 'cb.csv'
    x.astype('<f4').tofile(ip)
    cmd = [str(exe), str(ip), str(op), str(rate), str(block), str(shift), engine, str(cb)]
    if cpu:
        cmd.append('cpu')
    r = subprocess.run(cmd, check=True, capture_output=True, text=True)
    y = np.fromfile(op, dtype='<f4').reshape(-1, 2)
    assert y.shape == x.shape and np.isfinite(y).all()
    return y, json.loads(r.stdout)


def sweep_job(arg):
    binary, rate, shift, k, seed = arg
    rows = []
    rng = np.random.default_rng(np.random.SeedSequence([seed, rate, shift + 12, k]))
    fc = k * rate / travel(rate, shift)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        for index, offset in enumerate(np.linspace(-1, 1, 51)):
            f = float(fc + k * offset)
            phase = rng.uniform(0, 2 * np.pi, int(8000 // f))
            z = tone(f, rate, .8, phase, secs=2)
            x = np.column_stack((z, -.65 * z)).astype('<f4')
            lo, hi = round(.5 * rate), round(1.8 * rate)
            fi = f * 2 ** (shift / 12)
            ih, ir = interharm(tone(fi, rate, .8, phase, secs=2)[lo:hi], fi, rate)
            ideal = float(10 * np.log10(ir / ih))
            row = dict(rate=rate, shift=shift, multiple=k, index=index, f0=f,
                       ideal_db=ideal, input_sha256=__import__('hashlib').sha256(x.tobytes()).hexdigest())
            for engine in ENGINES:
                y, _ = render(binary, tmp, x, rate, shift, engine)
                h, r = interharm(y[lo:hi, 0].astype(float), fi, rate)
                raw = float(10 * np.log10(r / h))
                row[engine] = dict(raw_db=raw, net_db=float(net_db(raw, ideal)),
                                   harmonic_gain_db=float(10 * np.log10(h / ih)))
            rows.append(row)
    return rows


def sweep(a):
    jobs = [(a.bin / 'TransposeProbe', r, s, k, SWEEP_SEED)
            for r in RATES for s in SHIFTS for k in (1, 2, 3)]
    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
        for group in pool.map(sweep_job, jobs):
            rows += group
            print('sweep', len(rows), flush=True)
    packed(a.out / 'sweep-rows.json.gz', rows)
    summary = []
    for rate in RATES:
        for k in (1, 2, 3):
            group = [r for r in rows if r['rate'] == rate and r['multiple'] == k]
            result = dict(rate=rate, multiple=k, cases=len(group))
            for engine in ENGINES:
                v = [r[engine]['net_db'] for r in group]
                result[engine] = dict(worst_db=max(v), median_db=float(np.median(v)),
                                      over_minus50=sum(x > -50 for x in v),
                                      raw_worst_db=max(r[engine]['raw_db'] for r in group))
            summary.append(result)
    # Per-case regressions are preserved, including changes at the metric floor.
    regressions = {e: [dict(rate=r['rate'], shift=r['shift'], multiple=r['multiple'], f0=r['f0'],
                          before=r[e]['net_db'], after=r[e + '-edge']['net_db']) for r in rows
                       if r[e + '-edge']['net_db'] > r[e]['net_db'] + 1]
                   for e in (ENGINES[0], ENGINES[2])}
    write(a.out / 'sweep-summary.json', dict(groups=summary, regressions_over_1db=regressions,
          sinc_all_below_minus50=all(r[ENGINES[3]]['net_db'] <= -50 for r in rows)))


def targets(a):
    def run(engine):
        out = a.out / engine
        out.mkdir()
        cmd = [sys.executable, str(ROOT / 'Tools/TransposeNext/latency_targets.py'),
               '--renderer', str(a.bin / 'TransposeProbe'), '--engine', engine, '--out', str(out),
               '--holdout-seeds', ','.join(map(str, HOLDOUT)), '--long-seeds', ','.join(map(str, LONG))]
        with (out / 'run.log').open('w') as f:
            subprocess.run(cmd, check=True, stdout=f, stderr=subprocess.STDOUT)
        return engine, json.loads((out / 'results.json').read_text())
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
        results = dict(pool.map(run, ENGINES))
    spec = json.loads(SPEC.read_text())
    reports = {}
    for engine, data in results.items():
        rows = data['rows']
        fresh = [r for r in rows if '/holdout' in r['input'] or '/long_notes_seed' in r['input']]
        reports[engine] = dict(renders=len(rows), all=data['report'], fresh_renders=len(fresh),
            fresh_plus_steady=evaluate(spec, fresh + [r for r in rows if 'steady' in r['roles']]))
        packed(a.out / (engine + '.json.gz'), data)
    comparisons = {}
    for base in (ENGINES[0], ENGINES[2]):
        ref, cand = results[base]['rows'], results[base + '-edge']['rows']
        eq = equivalence(ref, cand)
        eq['by_instrument'] = {}
        for inst in FREQUENCIES:
            rr = [r for r in ref if r['input'].startswith(inst + '/')]
            cc = [r for r in cand if r['input'].startswith(inst + '/')]
            changes = []
            for r, c in zip(rr, cc):
                assert (r['input'], r['rate'], r['shift']) == (c['input'], c['rate'], c['shift'])
                changes.extend(v-u for u,v in zip(r.get('lowband_additional_ms', []), c.get('lowband_additional_ms', [])))
            eq['by_instrument'][inst] = dict(max_abs_ms=max(map(abs, changes)),
                max_increase_ms=max(changes), max_decrease_ms=min(changes))
        comparisons[base] = eq
    write(a.out / 'target-summary.json', dict(reports=reports, comparisons=comparisons))


def controls(rate):
    for inst, f in FREQUENCIES.items():
        for mode in ('random_plucks', 'random_markers'):
            x, _ = holdout_input(f, mode, HOLDOUT[0], rate)
            yield inst + '/' + mode, x
    for k in (1, 2, 3):
        f = k * rate / travel(rate, 1) + .12 * k
        rng = np.random.default_rng(SWEEP_SEED + k)
        z = tone(f, rate, .8, rng.uniform(0, 2*np.pi, int(8000//f)), secs=2)
        yield 'edge' + str(k), np.column_stack((z, -.65*z)).astype('<f4')


def blocks(a):
    def run(arg):
        rate, name, x, shift, engine = arg
        with tempfile.TemporaryDirectory() as d:
            ip = Path(d) / 'in.f32'; x.tofile(ip)
            p = subprocess.run([str(a.bin / 'BlockInvariance'), str(ip), str(rate), str(shift),
                                engine.removeprefix('corr40-split-')], capture_output=True, text=True, check=True)
        return dict(rate=rate, input=name, shift=shift, engine=engine, differences=json.loads(p.stdout))
    jobs = [(r,n,x,s,e) for r in RATES for n,x in controls(r) for s in SHIFTS for e in ENGINES]
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
        rows = list(pool.map(run, jobs))
    values = [v for r in rows for v in r['differences'].values()]
    write(a.out / 'blocks.json', dict(cases=len(rows), comparisons=len(values),
          nonzero=sum(v != 0 for v in values), max_diff=max(values), rows=rows))


def parity(a):
    rows = []
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        for rate in RATES:
            for name, x in controls(rate):
                for shift in SHIFTS:
                    for engine in (ENGINES[0], ENGINES[2]):
                        y0, n0 = render(a.baseline, tmp, x, rate, shift, engine)
                        y1, n1 = render(a.bin / 'TransposeProbe', tmp, x, rate, shift, engine)
                        same = y0.tobytes() == y1.tobytes()
                        decisions = all(n0[k] == n1[k] for k in ('onset_samples','low_onset_resets','splices'))
                        rows.append(dict(rate=rate,input=name,shift=shift,engine=engine,bit_equal=same,decisions_equal=decisions))
    write(a.out / 'parity.json', dict(cases=len(rows), failed=sum(not(r['bit_equal'] and r['decisions_equal']) for r in rows),rows=rows))


def cpu(a):
    rows = []
    raw_dir = a.out / 'cpu-callbacks'; raw_dir.mkdir()
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        for rate in RATES:
            for name, x in controls(rate):
                if name not in ('bass_B0/random_plucks', 'bass_dropA_A0/random_plucks', 'edge1', 'edge2', 'edge3'):
                    continue
                x = np.tile(x, (3, 1))
                for block in (64, 128, 256):
                    for shift in (-2, 1):
                        for repeat in range(5):
                            order = ENGINES if repeat % 2 == 0 else ENGINES[::-1]
                            for engine in order:
                                _, native = render(a.bin / 'TransposeProbe', tmp, x, rate, shift, engine, block, True)
                                cb = np.genfromtxt(tmp / 'cb.csv', delimiter=',', names=True)
                                key = f'{rate}-{name.replace("/","_")}-{block}-{shift}-{repeat}-{engine}'
                                with gzip.open(raw_dir / (key + '.csv.gz'), 'wb') as f:
                                    f.write((tmp / 'cb.csv').read_bytes())
                                rows.append(dict(rate=rate,input=name,block=block,shift=shift,repeat=repeat,engine=engine,
                                    input_sha256=sha(tmp/'in.f32'),output_sha256=sha(tmp/'out.f32'),**native,
                                    cpu_percent=float(cb['thread_cpu_us'].sum()/(len(x)/rate*1e6)*100),
                                    cpu_p99_us=float(np.percentile(cb['thread_cpu_us'],99)),cpu_max_us=float(cb['thread_cpu_us'].max()),
                                    cpu_overruns=int(np.sum(cb['thread_cpu_us']>cb['samples']/rate*1e6))))
                        print('cpu',rate,name,block,shift,flush=True)
    write(a.out / 'cpu.json', dict(status='SHARED_LINUX_DIAGNOSTIC_NOT_DAW_ACCEPTANCE',rows=rows))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('phase', choices=['freeze','sweep','targets','blocks','parity','cpu'])
    p.add_argument('--bin', type=Path, required=True)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--workers', type=int, default=4)
    a = p.parse_args(); a.bin=a.bin.resolve(); a.baseline=a.baseline.resolve(); a.out=a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=True)
    current = dict(sources_sha256=sources(), binaries_sha256={n:sha(a.bin/n) for n in ('TransposeProbe','BlockInvariance')},
                   baseline_sha256=sha(a.baseline),spec_sha256=sha(SPEC))
    reg = a.out / 'preregistration.json'
    if a.phase == 'freeze':
        data = dict(**current,base_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            source_pr_sha='3f28c3e09ec437a898868e5db0784acb30e33cdc',
            created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),client_date_kst='2026-10-31',
            platform=platform.platform(),holdout_seeds=HOLDOUT,long_seeds=LONG,sweep_seed=SWEEP_SEED,
            rates=RATES,shifts=SHIFTS,engines=ENGINES,synthetic_only=True,
            rules=dict(sweep='Each of fc/2fc/3fc +/-1/2/3 Hz, 51 points, new random phases. Sinc-edge worst net <= -50 dB at every rate/multiple. Report raw residual and all >1 dB regressions; no retuning.',
                targets='Unchanged spec v2: T1/T2/T4 PASS, T3 within existing ceilings. Preserve failures on either arm. New holdout seeds and long seed, plus existing fixed controls.',
                lowband='Max absolute delay difference <= 0.05 ms; report signed differences separately for B0/A0/A1.',
                onset='No increased missed events/unmatched resets or changed reset counts; report association timing differences, do not hide them.',
                blocks='All four modes: fixed 1,37,64,100,256,512,1024,2048,4096 and five seeded random patterns vs 128: bit-identical.',
                parity='Candidate option OFF vs unmodified main binary: bit-identical output and decisions on 216 cases.',
                cpu='Five alternating-order passes, all rates, +/- selected shifts (-2/+1), blocks64/128/256, B0/A0 plucks and all three edge regions. Thread CPU mean/p99/max, wall max and every overrun. Report only; shared Linux cannot certify a DAW.'),
            exclusions=['No product Source edits','No fixed PDC implementation','No real DI','No listening','No Windows/macOS DAW'],
            run_policy='Commit/push before rendering. One phase run; retain failures. CPU alone after other work.')
        with reg.open('x') as f: json.dump(data,f,indent=2)
        print(sha(reg)); return
    data=json.loads(reg.read_text())
    assert all(data[k]==v for k,v in current.items()), 'Source/binary/spec changed since registration'
    with (a.out/(a.phase+'-started.json')).open('x') as f:
        json.dump(dict(registration_sha256=sha(reg),started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat()),f)
    globals()[a.phase](a)


if __name__ == '__main__':
    main()

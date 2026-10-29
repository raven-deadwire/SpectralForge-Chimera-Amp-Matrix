#!/usr/bin/env python3
"""Preregistered fresh-seed re-validation of the v3-sinc high-band residual.

Replaces the failed P5 rule of v3sinc_validation.py (median excess over the
ideal tone <= +2 dB). That rule failed because the ideal tone's own
inter-harmonic floor drops from about -48 to -97 dB as f0 rises, so excess
grew with f0 for the same output. This rule scores the net residual: output
inter-harmonic energy minus the ideal tone's (the measurement floor), re the
output harmonic energy. freeze writes hashes, seed and rules before any fresh
tone is rendered; commit/push it before run.
"""
import argparse, datetime, json, subprocess, tempfile
from pathlib import Path
import numpy as np
from legacy_metrics import sha
from v3sinc_validation import tone, interharm, render, REF, CAND

ROOT=Path(__file__).resolve().parents[2]
SEED=86603
TONES=16
RATES=[44100,48000,96000]
SHIFTS=[-2,-1,1,2]
FLOOR_DB=-120.0

def cases(seed):
    rng=np.random.default_rng(seed);out=[]
    for i in range(TONES):
        f=float(np.exp(rng.uniform(np.log(27.5),np.log(330.))));tilt=float(rng.uniform(.5,1.5))
        out.append({'case':i,'f0_hz':f,'tilt':tilt,'phase':rng.uniform(0,2*np.pi,int(8000//f)).tolist()})
    return out

def net_db(residual_re_harm_db,ideal_re_harm_db):
    """Output inter-harmonic energy above the ideal tone's own floor, re output harmonics."""
    p=10**(residual_re_harm_db/10)-10**(ideal_re_harm_db/10)
    return 10*np.log10(p) if p>10**(FLOOR_DB/10) else FLOOR_DB

def measure(binary,seed,rates=RATES):
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp)
        for rate in rates:
            for c in cases(seed):
                z=tone(c['f0_hz'],rate,c['tilt'],c['phase']);np.column_stack((z,-.65*z)).astype('<f4').tofile(tmp/'i.f32')
                lo,hi=round(.5*rate),len(z)-round(.3*rate)
                for sh in SHIFTS:
                    fi=c['f0_hz']*2**(sh/12);hi_,ri=interharm(tone(fi,rate,c['tilt'],c['phase'])[lo:hi],fi,rate)
                    ideal=10*np.log10(ri/hi_)
                    row={'rate':rate,'case':c['case'],'f0_hz':round(c['f0_hz'],3),'tilt':round(c['tilt'],3),'shift':sh,'ideal_residual_db_re_harm':ideal}
                    for engine,tag in [(REF,'v3'),(CAND,'v3sinc')]:
                        y=render(binary,tmp/'i.f32',tmp/'o.f32',rate,sh,engine,tmp/'cb.csv')[lo:hi,0].astype(float)
                        hy,ry=interharm(y,fi,rate);res=10*np.log10(ry/hy)
                        row[tag]={'residual_db_re_harm':res,'net_db_re_harm':net_db(res,ideal),'excess_db':res-ideal,
                                  'harm_out_vs_ideal_db':10*np.log10(hy/hi_)}
                    rows.append(row)
                print(rate,c['case'],round(c['f0_hz'],1),[round(float(r['v3sinc']['net_db_re_harm']),1) for r in rows[-len(SHIFTS):]],flush=True)
    return rows

def judge(rows):
    out={'by_rate':{},'verdicts':{}}
    for rate in sorted({r['rate'] for r in rows}):
        s=[r for r in rows if r['rate']==rate]
        n3=np.array([r['v3']['net_db_re_harm'] for r in s]);ns=np.array([r['v3sinc']['net_db_re_harm'] for r in s])
        both=[{'case':r['case'],'f0_hz':r['f0_hz'],'shift':r['shift'],'v3':round(float(r['v3']['residual_db_re_harm']),1),'v3sinc':round(float(r['v3sinc']['residual_db_re_harm']),1)}
              for r in s if r['v3']['residual_db_re_harm']>-40 and r['v3sinc']['residual_db_re_harm']>-40]
        q={'cases':len(s),'v3sinc_net_median_db':round(float(np.median(ns)),2),'v3sinc_net_max_db':round(float(ns.max()),2),
           'v3sinc_share_net_le_-50':round(float(np.mean(ns<=-50)),4),'v3_net_median_db':round(float(np.median(n3)),2),
           'median_improvement_db':round(float(np.median(n3-ns)),2),'worst_v3sinc_minus_v3_db':round(float((ns-n3).max()),2),
           'v3sinc_at_floor':int(np.sum(ns<=FLOOR_DB)),'v3_at_floor':int(np.sum(n3<=FLOOR_DB)),
           'excess_median_db':{'v3':round(float(np.median([r['v3']['excess_db'] for r in s])),2),'v3sinc':round(float(np.median([r['v3sinc']['excess_db'] for r in s])),2)},
           'harm_loss_median_db':{'v3':round(float(np.median([r['v3']['harm_out_vs_ideal_db'] for r in s])),2),'v3sinc':round(float(np.median([r['v3sinc']['harm_out_vs_ideal_db'] for r in s])),2)},
           'both_engines_above_-40db':both}
        q['R1']=q['v3sinc_net_median_db']<=-60
        q['R2']=q['v3sinc_share_net_le_-50']>=.90
        q['R3']=q['median_improvement_db']>=10 if rate in (44100,48000) else None
        q['R4']=q['worst_v3sinc_minus_v3_db']<=1
        out['by_rate'][str(rate)]=q
    for k in ('R1','R2','R3','R4'):
        v=[q[k] for q in out['by_rate'].values() if q[k] is not None]
        out['verdicts'][k]='PASS' if all(v) else 'FAIL'
    out['verdicts']['overall']='PASS' if all(v=='PASS' for v in out['verdicts'].values()) else 'FAIL'
    return out

def main():
    p=argparse.ArgumentParser();p.add_argument('phase',choices=['freeze','run'])
    p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();a.bin=a.bin.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    reg=a.out/'preregistration.json';tools=ROOT/'Tools/TransposeNext'
    sources={str(f.relative_to(ROOT)):sha(f) for f in [tools/'residual_validation.py',tools/'v3sinc_validation.py',tools/'legacy_metrics.py',
                                                      tools/'CorrelationPitch.h',tools/'TransientSplitPitch.h',tools/'TransposeProbe.cpp']}
    binary={'TransposeProbe':sha(a.bin/'TransposeProbe')}
    if a.phase=='freeze':
        data={'purpose':'fresh-seed re-validation of the corr40-split-v3-sinc high-band residual with a floor-corrected rule; registered before any tone of this seed is rendered',
              'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'client_date_kst':'2026-10-29',
              'base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'sources_sha256':sources,'binaries_sha256':binary,
              'binary_check':'v3 and v3-sinc output bit-identical to the 2026-10-29 validation build (8 renders)',
              'replaces':'P5 of docs/evidence/transpose-v3sinc-fresh-seed-20261029/preregistration.json (verdict FAIL, kept)',
              'fresh_seed':SEED,'previously_used_residual_seeds':[70711],'tones_per_rate':TONES,'rates':RATES,'shifts':SHIFTS,
              'inputs':'3 s harmonic tones to 8 kHz, f0 log-uniform 27.5-330 Hz, amplitude 1/h^tilt with tilt uniform 0.5-1.5, random phases; ideal reference = same harmonic set at f0*2^(shift/12)',
              'metric':'1-8 kHz Welch (32768) energy farther than f0/4 from the harmonics; net residual = output inter-harmonic energy minus the ideal tone inter-harmonic energy, re output harmonic energy, in dB (floor -120 dB)',
              'development_data':'rules were set after seeing the 2026-10-28 study tones (27.5-55 Hz) and the 2026-10-29 seed 70711 tones; on both, v3-sinc net median is -67 to -82 dB, 98-100 % of cases <= -50 dB, improvement 17-21 dB at 44.1/48 kHz, worst +0.2 dB vs v3',
              'pass_rule':{'R1':'per rate: v3-sinc median net residual <= -60 dB',
                           'R2':'per rate: at least 90 % of v3-sinc cases have net residual <= -50 dB',
                           'R3':'44.1 and 48 kHz: median (v3 net - v3-sinc net) >= 10 dB',
                           'R4':'per rate: no case with v3-sinc net residual > v3 net residual + 1 dB',
                           'overall':'R1-R4 all PASS'},
              'report_only':['excess over the ideal tone (old metric)','harmonic loss vs ideal','cases where both engines leave a residual above -40 dB re harmonics (engine-level, not interpolation)','cases clamped at the -120 dB floor'],
              'rules':['Commit/push this registration before rendering seed 86603','Single run; keep the result whatever it is; no change of seed, thresholds or metric after results',
                       'Synthetic tones only; no listening or real DI claim','Product Source/ and engine code unchanged']}
        with reg.open('x') as f:json.dump(data,f,indent=2);f.write('\n')
        print(sha(reg));return
    data=json.loads(reg.read_text())
    if sources!=data['sources_sha256'] or binary!=data['binaries_sha256']:
        raise SystemExit('Source/binary changed since preregistration; refusing to run')
    with (a.out/'run-started.json').open('x') as f:
        json.dump({'registration_sha256':sha(reg),'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()},f)
    rows=measure(a.bin/'TransposeProbe',SEED);res=judge(rows)
    (a.out/'residual-revalidation.json').write_text(json.dumps({'summary':res,'rows':rows},indent=1)+'\n')
    print(json.dumps(res['verdicts']),flush=True)

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Preregistered fresh-seed validation of corr40-split-v3-sinc-edge against corr40-split-v3-sinc.

v3-sinc-edge = v3-sinc + splice-search edge refine on the transient path
(CorrelationPitch::setSpliceEdgeRefine; docs/TRANSPOSE_SPLICE_EDGE_107HZ_20261030.md).
freeze writes hashes, seeds and pass rules before any fresh input is rendered;
commit/push it before running the other phases. Each phase creates an exclusive
started marker, so a failed or partial run is kept instead of silently replaced.
"""
import argparse, csv, datetime, json, platform, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from latency_targets import inputs, evaluate
from legacy_metrics import sha
from v3sinc_validation import tone, interharm, render
from residual_validation import cases, net_db

ROOT=Path(__file__).resolve().parents[2]
SPEC=ROOT/'Validation/transpose-latency-targets.json'
HOLDOUT=[64807,67082,69282]
LONG=[4796,5099,5385]
SWEEP_SEED=91652
RESIDUAL_SEED=94868
RATES=[44100,48000,96000]
SHIFTS=[-2,-1,1,2]
REF,CAND='corr40-split-v3-sinc','corr40-split-v3-sinc-edge'

def tracked_sources():
    paths=list((ROOT/'Tools/TransposeNext').glob('*.h'))+list((ROOT/'Tools/TransposeNext').glob('*.cpp'))+list((ROOT/'Tools/TransposeNext').glob('*.py'))
    paths+=[SPEC]
    return {str(p.relative_to(ROOT)):sha(p) for p in sorted(paths) if p.is_file()}

def travel(rate,shift):
    """Transient-path minimum splice travel (samples): a quarter of the search range."""
    delta=1-2**(shift/12);mx=rate*.040;mn=rate*.002;fade=int(np.ceil(rate*.002));sfade=min(4*fade,max(fade,int(np.ceil(rate*.004))))
    guard=abs(delta)*max(fade,sfade)+2;return (mx-guard-(mn+guard))*.25

def tone_case(a,engine_rows,rate,sh,f0,tilt,ph,secs,lo_s,hi_s,tmp):
    z=tone(f0,rate,tilt,ph,secs=secs);np.column_stack((z,-.65*z)).astype('<f4').tofile(tmp/'i.f32')
    lo,hi=round(lo_s*rate),len(z)-round(hi_s*rate);fi=f0*2**(sh/12)
    h0,r0=interharm(tone(fi,rate,tilt,ph,secs=secs)[lo:hi],fi,rate);ideal=10*np.log10(r0/h0)
    for engine,tag in engine_rows:
        y=render(a.bin/'TransposeProbe',tmp/'i.f32',tmp/'o.f32',rate,sh,engine,tmp/'cb.csv')[lo:hi,0].astype(float)
        h,r=interharm(y,fi,rate);yield tag,{'residual_db_re_harm':10*np.log10(r/h),'net_db_re_harm':net_db(10*np.log10(r/h),ideal)}

def phase_sweep(a,data,k):
    rng=np.random.default_rng(SWEEP_SEED+k);rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp)
        for rate in RATES:
            for sh in SHIFTS:
                fc=k*rate/travel(rate,sh)
                for f0 in np.round(np.arange(fc-1.0*k,fc+1.0*k+1e-9,0.04*k),4):
                    f0=float(f0);tilt=float(rng.uniform(.5,1.5));ph=rng.uniform(0,2*np.pi,int(8000//f0)).tolist()
                    row={'k':k,'rate':rate,'shift':sh,'fc':fc,'f0':f0,'tilt':tilt}
                    for tag,v in tone_case(a,[(REF,'v3sinc'),(CAND,'edge')],rate,sh,f0,tilt,ph,2,.5,.2,tmp):row[tag]=v
                    rows.append(row)
                s=[r for r in rows if r['rate']==rate and r['shift']==sh]
                print(k,rate,sh,'worst ref %.1f cand %.1f'%(max(r['v3sinc']['net_db_re_harm'] for r in s),max(r['edge']['net_db_re_harm'] for r in s)),flush=True)
    (a.out/f'sweep{k}.json').write_text(json.dumps(rows,indent=1)+'\n')

def phase_residual(a,data):
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp)
        for rate in RATES:
            for c in cases(RESIDUAL_SEED):
                for sh in SHIFTS:
                    row={'rate':rate,'case':c['case'],'f0':round(c['f0_hz'],3),'tilt':round(c['tilt'],3),'shift':sh}
                    for tag,v in tone_case(a,[(REF,'v3sinc'),(CAND,'edge')],rate,sh,c['f0_hz'],c['tilt'],c['phase'],3,.5,.3,tmp):row[tag]=v
                    rows.append(row)
                print(rate,c['case'],round(c['f0_hz'],1),[round(float(r['edge']['net_db_re_harm']),1) for r in rows[-len(SHIFTS):]],flush=True)
    (a.out/'residual.json').write_text(json.dumps(rows,indent=1)+'\n')

COUNT_FIELDS=('onset_resets','low_onset_resets')
ASSOC_FIELDS=('expected_events','matched_within_10ms','unmatched_resets','missed_events')

def phase_targets(a,data):
    for engine in [REF,CAND]:
        out=a.out/engine;out.mkdir()
        cmd=[sys.executable,str(ROOT/'Tools/TransposeNext/latency_targets.py'),'--renderer',str(a.bin/'TransposeProbe'),'--engine',engine,'--out',str(out),
             '--holdout-seeds',','.join(map(str,HOLDOUT)),'--long-seeds',','.join(map(str,LONG))]
        with (out/'run.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,cwd=ROOT)
    spec=json.loads(SPEC.read_text());rep={}
    for engine in [REF,CAND]:
        r=json.loads((a.out/engine/'results.json').read_text());fresh=[x for x in r['rows'] if '/holdout' in x['input'] or '/long_notes_seed' in x['input']]
        rep[engine]={'renders':len(r['rows']),'all':r['report'],'fresh_renders':len(fresh),'fresh_only':evaluate(spec,fresh+[x for x in r['rows'] if 'steady' in x['roles']])}
        print(engine,json.dumps(r['report']['targets'][:5]),flush=True)
    ref={(x['input'],x['rate'],x['shift']):x for x in json.loads((a.out/REF/'results.json').read_text())['rows']}
    cand={(x['input'],x['rate'],x['shift']):x for x in json.loads((a.out/CAND/'results.json').read_text())['rows']}
    assert ref.keys()==cand.keys()
    counts=[];lags=[];low=[];att=[]
    snr_min=next(t for t in spec['targets'] if t['id']=='T1_attack')['event_snr_min_db']
    for k in sorted(ref):
        r,c=ref[k],cand[k]
        ra,ca=r.get('onset_association') or {},c.get('onset_association') or {}
        if any(r.get(f)!=c.get(f) for f in COUNT_FIELDS) or any(ra.get(f)!=ca.get(f) for f in ASSOC_FIELDS):counts.append(list(map(str,k)))
        elif ra.get('detector_lags_ms')!=ca.get('detector_lags_ms'):lags.append(list(map(str,k)))
        for u,v in zip(r.get('lowband_additional_ms') or [],c.get('lowband_additional_ms') or []):
            if u is not None and v is not None:low.append(abs(v-u))
        if r.get('attack_metric_valid') and c.get('attack_metric_valid'):
            for u,v,s in zip(r.get('attack_added_ms',[]),c.get('attack_added_ms',[]),r.get('attack_event_snr_db',[])):
                if s is not None and s>=snr_min:att.append(abs(v-u))
    def target(engine,i):return rep[engine]['all']['targets'][i]['observed']
    summary_diff={}
    for i,keys in ((0,['max_ms']),(2,['short_max_ms','long_max_ms']),(3,['short','bass_B0','bass_dropA_A0','guitar_dropA_A1'])):
        for key in keys:summary_diff[rep[REF]['all']['targets'][i]['id']+'.'+key]=round(abs(target(CAND,i)[key]-target(REF,i)[key]),4)
    rep['equivalence']={'renders_compared':len(ref),'renders_with_count_difference':counts,'renders_with_detector_lag_difference_only':len(lags),
                        'lowband_abs_diff_max_ms':round(max(low),4) if low else None,'lowband_values':len(low),
                        'attack_abs_diff_counted':{'n':len(att),'max_ms':round(max(att),4) if att else None,'p99_ms':round(float(np.quantile(att,.99)),4) if att else None},
                        'target_summary_abs_diff_ms':summary_diff}
    (a.out/'target-summary.json').write_text(json.dumps(rep,indent=2)+'\n');print(json.dumps(rep['equivalence']),flush=True)

def phase_blocks(a,data):
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        ip=Path(tmp)/'in.f32'
        for rate in RATES:
            for key,x,_,_ in inputs(rate,HOLDOUT,long_seeds=LONG):
                if '/holdout' not in key and '/long_notes_seed' not in key:continue
                x.tofile(ip)
                for shift in SHIFTS:
                    r=subprocess.run([str(a.bin/'BlockInvariance'),str(ip),str(rate),str(shift),'v3-sinc-edge'],check=True,capture_output=True,text=True)
                    rows.append({'input':key,'rate':rate,'shift':shift,'max_diff':json.loads(r.stdout)})
                print(rate,key,flush=True)
    values=[v for r in rows for v in r['max_diff'].values()]
    summary={'cases':len(rows),'patterns':len(values),'nonzero':sum(v!=0 for v in values),'max_diff':max(values),'verdict':'PASS' if all(v==0 for v in values) else 'FAIL'}
    (a.out/'block-invariance.json').write_text(json.dumps({'summary':summary,'rows':rows},indent=1)+'\n');print(summary)

def phase_cpu(a,data):
    # Report only (shared VM): thread CPU per callback, 48 kHz, alternating engine order.
    rows=[];rate=48000
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp);sources=[]
        for key,x,_,_ in inputs(rate,(),long_seeds=LONG[:1]):
            if '/long_notes_seed' in key and not key.startswith('guitar'):sources.append((key,x))
        f0=rate/travel(rate,1);z=tone(f0,rate,1.,list(np.random.default_rng(SWEEP_SEED).uniform(0,2*np.pi,int(8000//f0))),secs=6)
        sources.append((f'tone_fc_{f0:.3f}Hz',np.column_stack((z,-.65*z)).astype('<f4')))
        for key,x in sources:
            x.tofile(tmp/'in.f32')
            for block in (64,128):
                for shift in (-2,1,2):
                    for repeat in range(3):
                        for engine in ([REF,CAND] if repeat%2==0 else [CAND,REF]):
                            subprocess.run([str(a.bin/'TransposeProbe'),str(tmp/'in.f32'),str(tmp/'out.f32'),str(rate),str(block),str(shift),engine,str(tmp/'cb.csv'),'cpu'],capture_output=True,check=True)
                            us=np.array([float(c['thread_cpu_us']) for c in csv.DictReader((tmp/'cb.csv').open())])
                            rows.append({'input':key,'block':block,'shift':shift,'repeat':repeat,'engine':engine,'mean_us':float(us.mean()),'p99_us':float(np.quantile(us,.99)),'max_us':float(us.max())})
            print(key,flush=True)
    (a.out/'cpu.json').write_text(json.dumps({'platform':platform.platform(),'shared_vm':True,'rows':rows},indent=1)+'\n')

PHASES={'sweep1':lambda a,d:phase_sweep(a,d,1),'sweep2':lambda a,d:phase_sweep(a,d,2),'sweep3':lambda a,d:phase_sweep(a,d,3),
        'residual':phase_residual,'targets':phase_targets,'blocks':phase_blocks,'cpu':phase_cpu}

def main():
    p=argparse.ArgumentParser();p.add_argument('phase',choices=['freeze',*PHASES])
    p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();a.bin=a.bin.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    reg=a.out/'preregistration.json'
    bins={n:sha(a.bin/n) for n in ['TransposeProbe','BlockInvariance']}
    if a.phase=='freeze':
        data={'purpose':'fresh-seed validation of corr40-split-v3-sinc-edge (v3-sinc + transient-path splice-search edge refine) against corr40-split-v3-sinc; registered before any fresh input is rendered',
              'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'client_date_kst':'2026-10-31',
              'base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'sources_sha256':tracked_sources(),'binaries_sha256':bins,'spec_sha256':sha(SPEC),
              'binary_check':'with the option off, v2/v3/v4/v3-sinc output bit-identical to a build of main c4d5431 (72 renders); v3-sinc-edge reproduces the 2026-10-30 experiment patch exactly (612-point fc sweep identical)',
              'fresh_holdout_seeds':HOLDOUT,'fresh_long_seeds':LONG,'sweep_rng_seed':SWEEP_SEED,'residual_rng_seed':RESIDUAL_SEED,
              'previously_used_seeds':[1,8139,20261024,31415,27182,16180,2718,14142,17320,22360,1414,26457,31623,36056,2236,41421,44721,48990,3317,
                                       196613,262147,327673,104729,130363,155921,52915,57735,61803,3606,4123,4472,70711,86603,7,11],
              'rates':RATES,'shifts':SHIFTS,'block':128,'candidate':CAND,'reference':REF,
              'sweep':'for k = 1, 2, 3 and every rate x shift: f0 from k*fc-k to k*fc+k Hz in 51 steps, fc = rate / transient minimum travel; 2 s tones to 8 kHz, tilt U(0.5,1.5), random phases (RNG sweep_rng_seed + k); metric = net inter-harmonic residual (residual_validation.py)',
              'residual':'16 tones per rate from residual_validation.cases(residual_rng_seed), shifts +-1/+-2, 3 s, net residual',
              'development_data':'thresholds set after the 2026-10-30 analysis (docs/evidence/transpose-splice-edge-107hz-20261030): fc/2fc/3fc sweeps (candidate worst -53.1 dB, median change 0.00 dB), seed 70711/86603 tone sets (candidate medians -69.6 to -79.6 dB, 100 % <= -50 dB, median change -0.62 to 0.00 dB), known-seed targets (T1-T4 identical, low-band diff <= 0.05 ms)',
              'pass_rule':{
                  'E1_sweep_worst':'for every rate and k: candidate worst net residual <= -50 dB',
                  'E2_sweep_median':'for every rate and k: median (candidate - reference) net residual <= +0.5 dB',
                  'E3_residual_set':'per rate: candidate median net <= -60 dB, >= 90 % of cases <= -50 dB, and candidate median - reference median <= +0.5 dB',
                  'E4_targets':'candidate spec version 2 (all inputs and fresh-only): T1_attack, T2, T4 PASS, T3 REPORT_WITHIN_CEILING; every render: onset_resets, low_onset_resets, expected/matched/unmatched/missed identical to reference; low-band delay |diff| max <= 0.1 ms; |diff| of T1 max, T2 short/long and the four T3 values <= 0.1 ms each',
                  'E5_blocks':'BlockInvariance v3-sinc-edge on every fresh input, rate and shift: 13 block patterns bit-identical'},
              'report_only':['reference sweep cases above -50 dB (the defect being fixed)','renders whose only change is a detector lag','counted attack delay differences',
                             'CPU per callback at 48 kHz, 64/128 blocks, shifts -2/+1/+2 (shared VM; T5 needs dedicated hardware)'],
              'rules':['Commit/push this registration before rendering fresh seeds','Single run per phase; keep failures; no retuning of thresholds, seeds or metrics after results',
                       'Synthetic inputs only; no real DI, listening or DAW evidence is claimed','Product Source/ unchanged; the option stays off by default']}
        with reg.open('x') as f:json.dump(data,f,indent=2);f.write('\n')
        print(sha(reg));return
    data=json.loads(reg.read_text())
    if tracked_sources()!=data['sources_sha256'] or bins!=data['binaries_sha256']:
        raise SystemExit('Source/binary changed since preregistration; refusing to run')
    with (a.out/(a.phase+'-started.json')).open('x') as f:
        json.dump({'registration_sha256':sha(reg),'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()},f)
    PHASES[a.phase](a,data)

if __name__=='__main__':main()

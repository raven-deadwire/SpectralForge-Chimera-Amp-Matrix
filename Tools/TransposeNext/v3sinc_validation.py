#!/usr/bin/env python3
"""Preregistered fresh-seed validation of corr40-split-v3-sinc against corr40-split-v3.

freeze writes hashes, seeds and pass rules before any fresh input is rendered;
commit/push it before running the other phases. Each phase creates an exclusive
started marker, so a failed or partial run is kept instead of silently replaced.
"""
import argparse, datetime, json, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from scipy import signal
from latency_targets import inputs, evaluate
from legacy_metrics import sha
from attack_snr import hf_envelope, event_snr_db
from weak_attack_study import FREQUENCIES
from weak_attack_holdout import holdout_input

ROOT=Path(__file__).resolve().parents[2]
SPEC=ROOT/'Validation/transpose-latency-targets.json'
HOLDOUT=[52915,57735,61803]
LONG=[3606,4123,4472]
RESIDUAL_SEED=70711
RATES=[44100,48000,96000]
SHIFTS=[-2,-1,1,2]
REF,CAND='corr40-split-v3','corr40-split-v3-sinc'

def tracked_sources():
    paths=list((ROOT/'Tools/TransposeNext').glob('*.h'))+list((ROOT/'Tools/TransposeNext').glob('*.cpp'))+list((ROOT/'Tools/TransposeNext').glob('*.py'))
    paths+=[SPEC]
    return {str(p.relative_to(ROOT)):sha(p) for p in sorted(paths) if p.is_file()}

def render(exe,ip,op,rate,shift,engine,cb):
    subprocess.run([str(exe),str(ip),str(op),str(rate),'128',str(shift),engine,str(cb)],check=True,capture_output=True)
    return np.fromfile(op,dtype='<f4').reshape(-1,2)

def equivalence(ref_rows,cand_rows):
    """Detections, low-band delay and counted attack delay of v3-sinc against v3 on identical renders."""
    ref={(x['input'],x['rate'],x['shift']):x for x in ref_rows};cand={(x['input'],x['rate'],x['shift']):x for x in cand_rows}
    assert ref.keys()==cand.keys()
    snr_min=next(t for t in json.loads(SPEC.read_text())['targets'] if t['id']=='T1_attack')['event_snr_min_db']
    det_diff=[];low=[];att=[];att_fresh=[]
    for k in sorted(ref):
        r,c=ref[k],cand[k]
        if any(r.get(f)!=c.get(f) for f in ('onset_resets','low_onset_resets','onset_association')):det_diff.append(list(map(str,k)))
        for u,v in zip(r.get('lowband_additional_ms') or [],c.get('lowband_additional_ms') or []):
            if u is not None and v is not None:low.append(abs(v-u))
        if r.get('attack_metric_valid') and c.get('attack_metric_valid'):
            for u,v,s in zip(r.get('attack_added_ms',[]),c.get('attack_added_ms',[]),r.get('attack_event_snr_db',[])):
                if s is not None and s>=snr_min:
                    att.append(abs(v-u))
                    if '/holdout' in k[0] or '/long_notes_seed' in k[0]:att_fresh.append(abs(v-u))
    def stats(v):return {'n':len(v),'max_ms':round(max(v),4) if v else None,'p99_ms':round(float(np.quantile(v,.99)),4) if v else None,
                         'over_1_sample_at_44k1':int(sum(x>1000/44100 for x in v))}
    return {'renders_compared':len(ref),'renders_with_detection_difference':det_diff,'lowband_abs_diff':stats(low),
            'attack_abs_diff_counted_events':stats(att),'attack_abs_diff_counted_fresh_events':stats(att_fresh)}

def phase_targets(a,data):
    reports={}
    for engine in [REF,CAND]:
        out=a.out/engine;out.mkdir()
        cmd=[sys.executable,str(ROOT/'Tools/TransposeNext/latency_targets.py'),'--renderer',str(a.bin/'TransposeProbe'),'--engine',engine,'--out',str(out),
             '--holdout-seeds',','.join(map(str,HOLDOUT)),'--long-seeds',','.join(map(str,LONG))]
        with (out/'run.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,cwd=ROOT)
        r=json.loads((out/'results.json').read_text());fresh=[x for x in r['rows'] if '/holdout' in x['input'] or '/long_notes_seed' in x['input']]
        reports[engine]={'renders':len(r['rows']),'all':r['report'],'fresh_renders':len(fresh),
                         'fresh_only':evaluate(json.loads(SPEC.read_text()),fresh+[x for x in r['rows'] if 'steady' in x['roles']]),
                         'fresh_only_note':'Fresh inputs plus the unchanged steady controls for the T4 tone metrics'}
        print(engine,json.dumps(r['report']['targets']),flush=True)
    eq=equivalence(json.loads((a.out/REF/'results.json').read_text())['rows'],json.loads((a.out/CAND/'results.json').read_text())['rows'])
    reports['equivalence']=eq
    (a.out/'target-summary.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(eq),flush=True)

def phase_pdc(a,data):
    out=a.out/'pdc';tool=str(ROOT/'Tools/TransposeNext/pdc_study.py')
    with (a.out/'pdc-run.log').open('w') as log:
        subprocess.run([sys.executable,tool,'run','--renderer',str(a.bin/'TransposeProbe'),'--out',str(out),'--engine',CAND,
                        '--holdout-seeds',','.join(map(str,HOLDOUT)),'--long-seed',str(LONG[0])],stdout=log,stderr=subprocess.STDOUT,check=True,cwd=ROOT)
        subprocess.run([sys.executable,tool,'evaluate','--out',str(out),'--fixed-l','2.5','--event-snr-min','10'],stdout=log,stderr=subprocess.STDOUT,check=True,cwd=ROOT)
    print((a.out/'pdc-run.log').read_text()[-1200:],flush=True)

def tone(f,rate,tilt,phase,secs=3):
    # One sine per phase entry: the ideally shifted tone keeps the input's harmonic set.
    t=np.arange(secs*rate)/rate
    return sum(.12/h**tilt*np.sin(2*np.pi*f*h*t+ph) for h,ph in enumerate(phase,1))

def interharm(y,f,rate,lo=1000,hi=8000):
    # Same metric as docs/evidence/transpose-upshift-splice-noise-20261028/residual.py.
    F,P=signal.welch(y,rate,nperseg=2**15,noverlap=2**14)
    band=(F>=lo)&(F<=hi);near=np.abs(F-np.round(F/f)*f)<=f/4
    return P[band&near].sum(),P[band&~near].sum()

def residual_cases():
    rng=np.random.default_rng(RESIDUAL_SEED);cases=[]
    for i in range(12):
        f=float(np.exp(rng.uniform(np.log(27.5),np.log(330.))));tilt=float(rng.uniform(.5,1.5))
        cases.append({'case':i,'f0_hz':f,'tilt':tilt,'phase':rng.uniform(0,2*np.pi,int(8000//f)).tolist()})
    return cases

def phase_residual(a,data):
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp)
        for rate in RATES:
            for c in residual_cases():
                z=tone(c['f0_hz'],rate,c['tilt'],c['phase']);np.column_stack((z,-.65*z)).astype('<f4').tofile(tmp/'i.f32')
                lo,hi=round(.5*rate),len(z)-round(.3*rate)
                for sh in SHIFTS:
                    fi=c['f0_hz']*2**(sh/12);ideal=tone(fi,rate,c['tilt'],c['phase'])[lo:hi];hi_,ri=interharm(ideal,fi,rate)
                    row={'rate':rate,'case':c['case'],'f0_hz':round(c['f0_hz'],3),'tilt':round(c['tilt'],3),'shift':sh}
                    for engine,tag in [(REF,'v3'),(CAND,'v3sinc')]:
                        y=render(a.bin/'TransposeProbe',tmp/'i.f32',tmp/'o.f32',rate,sh,engine,tmp/'cb.csv')[lo:hi,0].astype(float)
                        hy,ry=interharm(y,fi,rate)
                        row[tag]={'harm_out_vs_ideal_db':10*np.log10(hy/hi_),'excess_db':10*np.log10(ry/hy)-10*np.log10(ri/hi_)}
                    rows.append(row)
                print(rate,c['case'],round(c['f0_hz'],1),[round(float(r['v3']['excess_db']),1) for r in rows[-len(SHIFTS):]],[round(float(r['v3sinc']['excess_db']),1) for r in rows[-len(SHIFTS):]],flush=True)
    summary={}
    for rate in RATES:
        s=[r for r in rows if r['rate']==rate];v3=np.array([r['v3']['excess_db'] for r in s]);vs=np.array([r['v3sinc']['excess_db'] for r in s])
        summary[str(rate)]={'cases':len(s),'v3_median_db':round(float(np.median(v3)),2),'v3_max_db':round(float(v3.max()),2),
                            'v3sinc_median_db':round(float(np.median(vs)),2),'v3sinc_max_db':round(float(vs.max()),2),
                            'median_improvement_db':round(float(np.median(v3-vs)),2),'worst_v3sinc_minus_v3_db':round(float((vs-v3).max()),2),
                            'harm_loss_median_db':{'v3':round(float(np.median([r['v3']['harm_out_vs_ideal_db'] for r in s])),2),
                                                   'v3sinc':round(float(np.median([r['v3sinc']['harm_out_vs_ideal_db'] for r in s])),2)}}
    (a.out/'residual.json').write_text(json.dumps({'summary':summary,'rows':rows},indent=1)+'\n');print(json.dumps(summary),flush=True)

def phase_echo(a,data):
    # Report only: marker tail re-read on the fresh random-marker inputs (marker_echo.py metric).
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        tmp=Path(tmp)
        for rate in RATES:
            for inst,f in FREQUENCIES.items():
                for s in HOLDOUT:
                    x,on=holdout_input(f,'random_markers',s,rate);x.astype('<f4').tofile(tmp/'i.f32');ei=hf_envelope(x,rate)
                    for sh in SHIFTS:
                        outs={tag:hf_envelope(render(a.bin/'TransposeProbe',tmp/'i.f32',tmp/'o.f32',rate,sh,engine,tmp/'cb.csv'),rate) for engine,tag in [(REF,'v3'),(CAND,'v3sinc')]}
                        for k,o in enumerate(on):
                            end=min(o+round(.060*rate),(on[k+1]-round(.005*rate)) if k+1<len(on) else len(ei))
                            if end<=o+round(.020*rate):continue
                            def ratio(e):return 20*np.log10(e[o+round(.015*rate):end].max()/max(e[o:o+round(.015*rate)].max(),1e-12))
                            rows.append({'rate':rate,'inst':inst,'seed':s,'shift':sh,'event':k,'snr_db':event_snr_db(ei,o,rate),'echo_in_db':ratio(ei),
                                         **{f'echo_{t}_db':ratio(e) for t,e in outs.items()}})
                print(rate,inst,flush=True)
    summary={}
    for d,name in [(-1,'down'),(1,'up')]:
        s=[r for r in rows if np.sign(r['shift'])==d and r['snr_db']>=10]
        summary[name]={'events':len(s),**{t:{'median_out_minus_in_db':round(float(np.median([r[f'echo_{t}_db']-r['echo_in_db'] for r in s])),2) if s else None,
                                            'max_out_db':round(float(max(r[f'echo_{t}_db'] for r in s)),2) if s else None,
                                            'over_6db':int(sum(r[f'echo_{t}_db']-r['echo_in_db']>6 for r in s))} for t in ('v3','v3sinc')}}
    (a.out/'marker-echo.json').write_text(json.dumps({'summary':summary,'rows':rows})+'\n');print(json.dumps(summary),flush=True)

def phase_blocks(a,data):
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        ip=Path(tmp)/'in.f32'
        for rate in RATES:
            for key,x,_,_ in inputs(rate,HOLDOUT,long_seeds=LONG):
                if '/holdout' not in key and '/long_notes_seed' not in key:continue
                x.tofile(ip)
                for shift in SHIFTS:
                    r=subprocess.run([str(a.bin/'BlockInvariance'),str(ip),str(rate),str(shift),'v3-sinc'],check=True,capture_output=True,text=True)
                    rows.append({'input':key,'rate':rate,'shift':shift,'max_diff':json.loads(r.stdout)})
                print(rate,key,flush=True)
    values=[v for r in rows for v in r['max_diff'].values()]
    summary={'cases':len(rows),'patterns':len(values),'nonzero':sum(v!=0 for v in values),'max_diff':max(values),'verdict':'PASS' if all(v==0 for v in values) else 'FAIL'}
    (a.out/'block-invariance.json').write_text(json.dumps({'summary':summary,'rows':rows},indent=1)+'\n');print(summary)

PHASES={'targets':phase_targets,'pdc':phase_pdc,'residual':phase_residual,'echo':phase_echo,'blocks':phase_blocks}

def main():
    p=argparse.ArgumentParser();p.add_argument('phase',choices=['freeze',*PHASES])
    p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();a.bin=a.bin.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    reg=a.out/'preregistration.json'
    bins={n:sha(a.bin/n) for n in ['TransposeProbe','BlockInvariance']}
    if a.phase=='freeze':
        data={'purpose':'fresh-seed validation of corr40-split-v3-sinc (v3 + 16-tap Kaiser sinc output reads) against corr40-split-v3; registered before any fresh input is rendered',
              'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'client_date_kst':'2026-10-29',
              'base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'sources_sha256':tracked_sources(),'binaries_sha256':bins,'spec_sha256':sha(SPEC),
              'binary_check':'v2/v3 output bit-identical to the pre-registered renderer (SHA256 5625cb1f...), v3-sinc to the 2026-10-28 study build, v4 to the main build (4 renders each)',
              'fresh_holdout_seeds':HOLDOUT,'fresh_long_seeds':LONG,'pdc_long_seed':LONG[0],'residual_rng_seed':RESIDUAL_SEED,
              'previously_used_seeds':[1,8139,20261024,31415,27182,16180,2718,14142,17320,22360,1414,26457,31623,36056,2236,41421,44721,48990,3317,
                                       196613,262147,327673,104729,130363,155921],
              'rates':RATES,'shifts':SHIFTS,'block':128,'candidate':CAND,'reference':REF,
              'pass_rule':{
                  'P1_targets':'v3-sinc, spec version 2, all inputs and fresh-only: T1_attack PASS, T2 PASS, T3 REPORT_WITHIN_CEILING (ceilings unchanged), T4 PASS',
                  'P2_decisions':'every render: onset_resets, low_onset_resets and onset_association identical to v3',
                  'P3_lowband':'max |low-band delay difference| to v3 <= 0.05 ms (2026-10-28 known seeds: 0.004 ms)',
                  'P4_pdc':'pdc_study, v3-sinc, fixed L = 2.5 ms, event SNR >= 10 dB: max |attack residual| <= 4.5 ms at each rate (same threshold as the v3 runs)',
                  'P5_residual':'12 fresh bright sustains per rate (f0 log-uniform 27.5-330 Hz, harmonic tilt 0.5-1.5, random phases; RNG 70711), shifts +-1/+-2, 1-8 kHz inter-harmonic excess over the ideally shifted tone: '
                                'at 44.1 and 48 kHz v3-sinc median <= +2 dB and median improvement over v3 >= 5 dB; at 96 kHz v3-sinc median <= +1 dB; at every rate no case with v3-sinc > v3 + 1 dB',
                  'P6_blocks':'BlockInvariance v3-sinc on every fresh input, rate and shift: 13 block patterns bit-identical'},
              'report_only':['attack delay differences to v3 (counted events)','T1 faint events','marker tail re-read on fresh random markers (marker_echo.py metric), v3 vs v3-sinc',
                             'harmonic loss vs ideal','max excess per rate','CPU: not re-measured (2026-10-28: +13-14 % mean, shared VM)'],
              'rules':['Commit/push this registration before rendering fresh seeds','Single run per phase; keep failures; no retuning of thresholds, seeds or metrics after results',
                       'Synthetic inputs only; no real DI, listening or DAW evidence is claimed','Product Source/ and engine code unchanged']}
        with reg.open('x') as f:json.dump(data,f,indent=2);f.write('\n')
        print(sha(reg));return
    data=json.loads(reg.read_text())
    if tracked_sources()!=data['sources_sha256'] or bins!=data['binaries_sha256']:
        raise SystemExit('Source/binary changed since preregistration; refusing to run')
    with (a.out/(a.phase+'-started.json')).open('x') as f:
        json.dump({'registration_sha256':sha(reg),'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()},f)
    PHASES[a.phase](a,data)

if __name__=='__main__':main()

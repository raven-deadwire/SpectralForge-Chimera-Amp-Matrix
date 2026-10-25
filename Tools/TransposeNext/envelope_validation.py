#!/usr/bin/env python3
"""Frozen follow-up validation. Original scorers, thresholds, 13 block patterns.
CPU phase must run after all builds and functional jobs finish, on its own.
"""
import argparse,datetime,gzip,hashlib,json,platform,subprocess,sys
from pathlib import Path
import numpy as np
from latency_targets import inputs,evaluate
from envelope_study import envelope,dips
from weak_attack_study import FREQUENCIES
ROOT=Path(__file__).resolve().parents[2]
HOLD=[524287,786433,999983];LONG=[524309,786457,1000003]
ENGINES=['corr40-split-v3','corr40-split-v4','env-floor125']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def sources():
    pp=list((ROOT/'Tools/TransposeNext').glob('*.py'))+list((ROOT/'Tools/TransposeNext').glob('*.cpp'))+list((ROOT/'Tools/TransposeNext').glob('*.h'))+[ROOT/'Validation/transpose-latency-targets.json']
    return {str(p.relative_to(ROOT)):sha(p) for p in sorted(pp)}
def save(p,data):
    p.write_bytes(gzip.compress(json.dumps(data,allow_nan=False).encode(),mtime=0))
def main():
    p=argparse.ArgumentParser();p.add_argument('phase',choices=['freeze','targets','blocks','cpu']);p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    a.bin=a.bin.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);reg=a.out/'registration.json'
    bins={n:sha(a.bin/n)for n in ['EnvelopeProbe','EnvelopeTrace','EnvelopeBlocks','EnvelopeTests']}
    if a.phase=='freeze':
        data={'date_kst':'2026-10-25','timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'sources':sources(),'binaries':bins,
              'spec_sha256':sha(ROOT/'Validation/transpose-latency-targets.json'),'parent':'be06fab38fef98aa7c51d2ab7a698917de167dc1',
              'candidate':'env-floor125','selection':'exploratory known-data max dip reduced; retain original 2ms fades, detector, hold, guard, sample hold; no gate threshold changes',
              'fresh_within_this_task_holdout':HOLD,'fresh_within_this_task_long':LONG,'engines':ENGINES,
              'rules':['Commit remote registration before targets or blocks; do not retune after new results','All inputs synthetic; old published seeds are development data','All failure rows retained; no product or T6 default changes'],
              'cpu_plan':'Sequential 3 rates x blocks 64/128/256 x B0/A0 long104729 x shifts +/-2 x 3 repetitions x 3 engines; extra 96k/64 +/-2 B0/A0 12 balanced repetitions; preserve callback raw data, wall/thread clocks, splice counts, affinity and per-run hashes. Shared VM only.'}
        with reg.open('x')as f:json.dump(data,f,indent=2)
        return
    data=json.loads(reg.read_text());assert sources()==data['sources'] and bins==data['binaries'],'Source/binary changed'
    with (a.out/(a.phase+'-started.json')).open('x')as f:json.dump({'registration_sha':sha(reg)},f)
    if a.phase=='targets':
        summary={}
        for engine in ENGINES:
            d=a.out/engine
            cmd=[sys.executable,str(ROOT/'Tools/TransposeNext/latency_targets.py'),'--renderer',str(a.bin/'EnvelopeProbe'),'--engine',engine,'--out',str(d),'--holdout-seeds',','.join(map(str,HOLD)),'--long-seeds',','.join(map(str,LONG))]
            d.mkdir();subprocess.run(cmd,stdout=(d/'run.log').open('w'),stderr=subprocess.STDOUT,check=True)
            r=json.loads((d/'results.json').read_text());save(a.out/(engine+'-targets.json.gz'),r)
            fresh=[q for q in r['rows']if '/holdout' in q['input']or'/long_notes_seed' in q['input']]
            summary[engine]={'renders':len(r['rows']),'all':r['report'],'fresh_renders':len(fresh),'fresh_plus_steady_controls':evaluate(json.loads((ROOT/'Validation/transpose-latency-targets.json').read_text()),fresh+[q for q in r['rows']if'steady'in q['roles']])}
            print(engine,json.dumps(summary[engine]['all']['targets']),flush=True)
        (a.out/'target-summary.json').write_text(json.dumps(summary,indent=2))
        # Exploratory dip comparison across ALL new plucks/long notes; no selection of notes.
        rows=[];w=a.out/'dip-work';w.mkdir()
        for rate in [44100,48000,96000]:
            for key,x,m,roles in inputs(rate,HOLD,long_seeds=LONG):
                if not ('/holdout' in key or '/long_notes_seed' in key)or not roles&{'short','long'}:continue
                f=FREQUENCIES[key.split('/')[0]];x.tofile(w/'in.f32');ind=dips(envelope(x[:,0],f,rate),m['onsets'],rate)
                for shift in [1,2]:
                    for engine in ENGINES:
                        subprocess.run([str(a.bin/'EnvelopeProbe'),str(w/'in.f32'),str(w/'out.f32'),str(rate),'128',str(shift),engine,str(w/'cb.csv')],stdout=subprocess.DEVNULL,check=True)
                        y=np.fromfile(w/'out.f32',dtype='<f4').reshape(-1,2);od=dips(envelope(y[:,0],f*2**(shift/12),rate),m['onsets'],rate)
                        rows.append({'input':key,'rate':rate,'shift':shift,'engine':engine,'extra_dip_db':[o['dip_db']-i['dip_db']for o,i in zip(od,ind)],'output_dips':od,'input_dips':ind})
        save(a.out/'new-envelope.json.gz',rows)
    elif a.phase=='blocks':
        rows=[];ip=a.out/'block-in.f32'
        for rate in [44100,48000,96000]:
            for key,x,_,_ in inputs(rate,HOLD,long_seeds=LONG):
                if not ('/holdout' in key or '/long_notes_seed' in key):continue
                x.tofile(ip)
                for shift in [-2,-1,1,2]:
                    q=subprocess.run([str(a.bin/'EnvelopeBlocks'),str(ip),str(rate),str(shift),'env-floor125'],capture_output=True,text=True,check=True)
                    rows.append({'input':key,'rate':rate,'shift':shift,'patterns':json.loads(q.stdout)})
            print('blocks',rate,flush=True)
        vv=[v for r in rows for v in r['patterns'].values()];save(a.out/'blocks.json.gz',{'rows':rows,'cases':len(rows),'comparisons':len(vv),'nonzero':sum(v!=0 for v in vv),'max_diff':max(vv)})
    else:
        import os
        # Pin this harness and child renders to one allowed CPU. No realtime priority claim.
        affinity=sorted(os.sched_getaffinity(0));os.sched_setaffinity(0,{affinity[0]})
        rows=[];w=a.out/'cpu-work';w.mkdir();raw=a.out/'callbacks';raw.mkdir()
        for rate in [44100,48000,96000]:
            for key,x,_,_ in inputs(rate,(),long_seeds=[104729]):
                if '/long_notes_seed'not in key or key.startswith('guitar'):continue
                x.tofile(w/'in.f32')
                for block in [64,128,256]:
                    for shift in [-2,2]:
                        reps=15 if rate==96000 and block==64 else 3
                        for rep in range(reps):
                            # Six orders balance position and pair order, includes v3/v4 A/A downshift controls.
                            orders=[ENGINES,ENGINES[::-1],[ENGINES[1],ENGINES[2],ENGINES[0]],[ENGINES[0],ENGINES[2],ENGINES[1]],[ENGINES[2],ENGINES[0],ENGINES[1]],[ENGINES[1],ENGINES[0],ENGINES[2]]]
                            for engine in orders[rep%6]:
                                cmd=[str(a.bin/'EnvelopeProbe'),str(w/'in.f32'),str(w/'out.f32'),str(rate),str(block),str(shift),engine,str(w/'cb.csv'),'cpu']
                                r=subprocess.run(cmd,capture_output=True,text=True,check=True);cb=np.genfromtxt(w/'cb.csv',delimiter=',',names=True);budget=cb['samples']/rate*1e6
                                fn=f'{rate}-{block}-{key.split("/")[0]}-{shift}-{rep}-{engine}.csv.gz';(raw/fn).write_bytes(gzip.compress((w/'cb.csv').read_bytes(),mtime=0))
                                row={'input':key,'rate':rate,'block':block,'shift':shift,'repeat':rep,'engine':engine,'order':orders[rep%6], 'output_sha256':sha(w/'out.f32'),'native':json.loads(r.stdout),'raw':fn}
                                for clock,col in [('wall','microseconds'),('thread','thread_cpu_us')]:
                                    us=cb[col];bad=np.flatnonzero(us>budget)
                                    row[clock]={'mean_us':float(us.mean()),'p99_us':float(np.quantile(us,.99)),'max_us':float(us.max()),'callbacks':len(us),'misses':len(bad),'miss_records':[{'callback':int(i),'us':float(us[i]),'deadline_us':float(budget[i]),'body_splices':int(cb['body_splices'][i]),'high_splices':int(cb['high_splices'][i])}for i in bad]}
                                row['splice_counts']={'body':int(cb['body_splices'].sum()),'high':int(cb['high_splices'].sum())};rows.append(row)
                    print('cpu',rate,key,block,flush=True)
        save(a.out/'cpu.json.gz',{'platform':platform.platform(),'affinity_before':affinity,'pinned_cpu':affinity[0],'shared_vm':True,'rows':rows})
if __name__=='__main__':main()

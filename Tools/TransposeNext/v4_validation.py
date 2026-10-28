#!/usr/bin/env python3
"""Preregistered v4 research validation. No thresholds or scoring changes.

freeze writes hashes/seeds before any fresh input is rendered; commit/push it
before targets/blocks. Each phase creates an exclusive started marker, retaining
failed/partial runs instead of silently replacing them. CPU runs separately.
"""
import argparse, csv, datetime, hashlib, json, platform, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from latency_targets import inputs, evaluate
from legacy_metrics import sha

ROOT=Path(__file__).resolve().parents[2]
SPEC=ROOT/'Validation/transpose-latency-targets.json'
HOLDOUT=[196613,262147,327673]
LONG=[104729,130363,155921]

def tracked_sources():
    paths=list((ROOT/'Tools/TransposeNext').glob('*.h'))+list((ROOT/'Tools/TransposeNext').glob('*.cpp'))+list((ROOT/'Tools/TransposeNext').glob('*.py'))
    paths+=list((ROOT/'Tools/TransposeNext/plugin').glob('*'))+[SPEC]
    return {str(p.relative_to(ROOT)):sha(p) for p in sorted(paths) if p.is_file()}

def main():
    p=argparse.ArgumentParser();p.add_argument('phase',choices=['freeze','targets','blocks','cpu'])
    p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();a.bin=a.bin.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    reg=a.out/'preregistration.json'
    bins={n:sha(a.bin/n) for n in ['TransposeProbe','BlockInvariance','OnsetTrace']}
    if a.phase=='freeze':
        data={'purpose':'corr40-split-v4 low upshift read-age floor, untouched T1-T4 limits',
              'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'client_date_kst':'2026-10-25',
              'base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'sources_sha256':tracked_sources(),'binaries_sha256':bins,'spec_sha256':sha(SPEC),
              'fresh_holdout_seeds':HOLDOUT,'fresh_long_seeds':LONG,
              'previously_seen_seeds':[1,8139,20261024,31415,27182,16180,2718,14142,17320,22360,1414],
              'rates':[44100,48000,96000],'shifts':[-2,-1,1,2],'block':128,
              'candidate':'corr40-split-v4','reference':'corr40-split-v3',
              'pass_rule':'T1/T2/T4 PASS; all original T3 ceilings unchanged; fresh-only and all-input verdicts; 13 block patterns bit-identical',
              'cpu_plan':'Separate from builds/functional tests: v3/v4, 3 rates, 64/128/256 blocks, +/-2 st, 3 repetitions, B0 and A0 long notes first fresh seed; wall/thread CPU tails and all misses retained',
              'rules':['Commit/push registration before rendering fresh seeds','Single run per phase; retain failure; no retuning after results','No real DI, listening or actual DAW evidence is claimed','No product integration or release']}
        with reg.open('x') as f:json.dump(data,f,indent=2);f.write('\n')
        print(sha(reg));return
    data=json.loads(reg.read_text())
    if tracked_sources()!=data['sources_sha256'] or bins!=data['binaries_sha256']:
        raise SystemExit('Source/binary changed since preregistration; refusing to run')
    with (a.out/(a.phase+'-started.json')).open('x') as f:
        json.dump({'registration_sha256':sha(reg),'execution_host_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()},f)
    if a.phase=='targets':
        reports={}
        for engine in [data['reference'],data['candidate']]:
            out=a.out/engine;out.mkdir()
            cmd=[sys.executable,str(ROOT/'Tools/TransposeNext/latency_targets.py'),'--renderer',str(a.bin/'TransposeProbe'),'--engine',engine,'--out',str(out),'--holdout-seeds',','.join(map(str,HOLDOUT)),'--long-seeds',','.join(map(str,LONG))]
            with (out/'run.log').open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,cwd=ROOT)
            r=json.loads((out/'results.json').read_text());fresh=[x for x in r['rows'] if '/holdout' in x['input'] or '/long_notes_seed' in x['input']]
            reports[engine]={'renders':len(r['rows']),'all':r['report'],'fresh_renders':len(fresh),'fresh_only':evaluate(json.loads(SPEC.read_text()),fresh+[x for x in r['rows'] if 'steady' in x['roles']])}
            # Steady fixtures are explicitly reused as T4 controls; fresh data
            # itself has no stationary tone. Counts for fresh renders exclude them.
            reports[engine]['fresh_only_note']='Fresh inputs plus unchanged steady controls for T4 tone metrics'
            print(engine,json.dumps(r['report']['targets']),flush=True)
        (a.out/'target-summary.json').write_text(json.dumps(reports,indent=2)+'\n')
    elif a.phase=='blocks':
        rows=[]
        with tempfile.TemporaryDirectory() as tmp:
            ip=Path(tmp)/'in.f32'
            for rate in data['rates']:
                for key,x,_,_ in inputs(rate,HOLDOUT,long_seeds=LONG):
                    if '/holdout' not in key and '/long_notes_seed' not in key:continue
                    x.tofile(ip)
                    for shift in data['shifts']:
                        r=subprocess.run([str(a.bin/'BlockInvariance'),str(ip),str(rate),str(shift),'v4'],check=True,capture_output=True,text=True)
                        rows.append({'input':key,'rate':rate,'shift':shift,'max_diff':json.loads(r.stdout)})
                    print(rate,key,flush=True)
        values=[v for r in rows for v in r['max_diff'].values()]
        summary={'cases':len(rows),'patterns':len(values),'nonzero':sum(v!=0 for v in values),'max_diff':max(values),'verdict':'PASS' if all(v==0 for v in values) else 'FAIL'}
        (a.out/'block-invariance.json').write_text(json.dumps({'summary':summary,'rows':rows},indent=1)+'\n');print(summary)
    else:
        rows=[]
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp)
            for rate in data['rates']:
                for key,x,_,_ in inputs(rate,(),long_seeds=LONG[:1]):
                    if '/long_notes_seed' not in key or key.startswith('guitar'):continue
                    x.tofile(tmp/'in.f32')
                    for block in [64,128,256]:
                        for shift in [-2,2]:
                            for repeat in range(3):
                                for engine in ([data['reference'],data['candidate']] if repeat%2==0 else [data['candidate'],data['reference']]):
                                    cmd=[str(a.bin/'TransposeProbe'),str(tmp/'in.f32'),str(tmp/'out.f32'),str(rate),str(block),str(shift),engine,str(tmp/'callbacks.csv'),'cpu']
                                    r=subprocess.run(cmd,capture_output=True,text=True,check=True)
                                    cb=list(csv.DictReader((tmp/'callbacks.csv').open()))
                                    row={'input':key,'rate':rate,'block':block,'shift':shift,'repeat':repeat,'engine':engine,'native':json.loads(r.stdout)}
                                    for clock,col in [('wall','microseconds'),('thread','thread_cpu_us')]:
                                        us=np.array([float(c[col]) for c in cb]);deadlines=np.array([int(c['samples'])/rate*1e6 for c in cb])
                                        row[clock]={'mean_us':float(us.mean()),'p99_us':float(np.quantile(us,.99)),'max_us':float(us.max()),'misses':int(np.sum(us>deadlines)),'callbacks':len(cb)}
                                    rows.append(row)
                        print(rate,key,block,flush=True)
        (a.out/'cpu.json').write_text(json.dumps({'platform':platform.platform(),'shared_vm':True,'dedicated_hardware_acceptance':False,'rows':rows},indent=1)+'\n')

if __name__=='__main__':main()

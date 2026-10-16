#!/usr/bin/env python3
"""New shared-FFT experiment. Sequential A/B and identical A/A; no releases."""
import argparse, csv, hashlib, itertools, json, math, os, pathlib, platform, random, statistics, subprocess, time

HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
ROUTES=list(itertools.product(range(3),(32,64),(1,2)))

def digest(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def command(cmd,log,env=None):
    log.parent.mkdir(parents=True,exist_ok=True)
    with log.open('w') as out:
        r=subprocess.run([str(x) for x in cmd],stdout=out,stderr=subprocess.STDOUT,env=env)
    if r.returncode: raise RuntimeError(f'Failed {r.returncode}: {log}')

def summary(path):
    with path.open() as f: rows=list(csv.DictReader(f))
    assert all(int(r['new_calls'])==int(r['delete_calls'])==0 for r in rows)
    values=rows[200:]
    assert len(values)>=400
    q=lambda key:sorted(float(r[key]) for r in values)[int(.99*len(values))]
    return dict(cpu_mean_ns=statistics.mean(float(r['cpu_ns']) for r in values),cpu_p99_ns=q('cpu_ns'),wall_p99_ns=q('wall_ns'),
        hits=sum(int(r['hits']) for r in values),misses=sum(int(r['misses']) for r in values),overflow=sum(int(r['overflow']) for r in values),
        forward_calls=sum(int(r['fft_forward_calls']) for r in values),mac_calls=sum(int(r['mac_calls']) for r in values),
        deadline_misses=sum(float(r['wall_ns'])>int(r['samples'])/96000*1e9 for r in values))

def analyze(trials):
    results=[]
    rng=random.Random(20261016)
    draws=[[rng.randrange(6) for _ in range(6)] for _ in range(10000)]
    for route in ROUTES:
        key='-'.join(map(str,route))
        control=[t for t in trials if t['mode']=='control']
        out={'route':key}
        for metric in ('cpu_mean_ns','cpu_p99_ns','wall_p99_ns'):
            effects={}
            for group in ('AB','AA'):
                vals=[]
                for round_ in range(6):
                    t=[t for t in control if t['round']==round_ and t['group']==group]
                    a=[math.log(ti['routes'][key][metric]) for ti in t if ti['logical']=='A']
                    b=[math.log(ti['routes'][key][metric]) for ti in t if ti['logical']=='B']
                    assert len(a)==len(b)==2
                    vals.append(statistics.mean(a)-statistics.mean(b))
                effects[group]=vals
            ab=effects['AB'];aa=effects['AA']
            boot=sorted(1-math.exp(-statistics.mean(ab[i] for i in draw)) for draw in draws)
            lo,hi=boot[249],boot[9749]
            aa_bound=max(abs(1-math.exp(-v)) for v in aa)
            order_ok=all(statistics.mean(ab[i] for i in range(parity,6,2))>0 for parity in (0,1))
            out[metric]={'improvement':1-math.exp(-statistics.mean(ab)), 'ci95':[lo,hi], 'aa_max_abs':aa_bound,
                'improved':lo>max(.02,aa_bound) and order_ok,'regressed':hi < -max(.02,aa_bound),'quartet_log_effects':effects}
        profiles=[t['routes'][key] for t in trials if t['mode']=='profile' and t['group']=='AB']
        forward={arm:[t['routes'][key]['forward_calls'] for t in trials if t['mode']=='profile' and t['group']=='AB' and t['arm']==arm] for arm in ('A','B')}
        mac={arm:[t['routes'][key]['mac_calls'] for t in trials if t['mode']=='profile' and t['group']=='AB' and t['arm']==arm] for arm in ('A','B')}
        out['forward_calls']=forward;out['mac_schedule_equal']=mac['A']==mac['B']
        assert out['mac_schedule_equal']
        assert set(forward['A'])=={8400} and set(forward['B'])=={4200}
        assert all(t['overflow']==0 for t in profiles)
        results.append(out)
    return {'rows':results,'cpu_routes_improved':sum(r['cpu_mean_ns']['improved'] for r in results),
        'all_routes_improved':all(r['cpu_mean_ns']['improved'] for r in results),
        'established_regressions':sum(r[m]['regressed'] for r in results for m in ('cpu_mean_ns','cpu_p99_ns','wall_p99_ns')),
        'release_approved':False}

def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True)
    p.add_argument('--correctness-only',action='store_true');a=p.parse_args()
    build=a.build.resolve();out=a.output.resolve()
    if out.exists():raise RuntimeError('Fresh evidence directory required')
    out.mkdir(parents=True)
    env=os.environ.copy();env['CHIMERA_FFT_IR_DIR']=str(out/'irs')
    report={'platform':platform.platform(),'machine':platform.machine(),'source':'c2263e4c90bffb9a56cc41285ebf8207e641b3aa',
        'juce':'d6181bde38d858c283c3b7bf699ce6340c050b5d','release_approved':False,'complete':False,
        'apple_silicon_performance_demonstrated':False,'scope':'CAB only: three independent input lanes, two mics each; synchronous publications',
        'binaries':{name:digest(build/name) for name in ('A_control','B_control','A_profile','B_profile','A_original_contracts','B_original_contracts')},
        'correctness':[],'trials':[]}
    try:
        # All builds must already be complete. No build or parallel activity during timings.
        for arm in ('A','B'): command([build/f'{arm}_original_contracts'],out/f'{arm}-original.log',env)
        for family,block,channels in list(itertools.product(range(3),(17,32,64,128,256,512),(1,2))):
            for scenario in (0,1):
                label=f'{family}-{block}-{channels}-s{scenario}';paths={}
                for arm in ('A','B'):
                    prefix=out/'correctness'/f'{label}-{arm}'
                    command([build/f'{arm}_control',family,block,channels,scenario,str(prefix)+'.f32',str(prefix)+'.csv',2600,0],pathlib.Path(str(prefix)+'.log'),env)
                    paths[arm]=pathlib.Path(str(prefix)+'.f32');summary(pathlib.Path(str(prefix)+'.csv'))
                assert paths['A'].read_bytes()==paths['B'].read_bytes(),f'Output bits differ: {label}'
                report['correctness'].append({'route':label,'floats':paths['A'].stat().st_size//4,'sha256':digest(paths['A'])})
        # Fallback IR, unequal channels and simultaneous independent processors.
        for scenario,parallel in ((2,0),(3,0),(1,1)):
            for arm in ('A','B'):
                prefix=out/'edge'/f's{scenario}-p{parallel}-{arm}'
                command([build/f'{arm}_control',2,32,2,scenario,str(prefix)+'.f32',str(prefix)+'.csv',2600,parallel],pathlib.Path(str(prefix)+'.log'),env)
            for suffix in ('0','1') if parallel else ('',):
                left=out/'edge'/f's{scenario}-p{parallel}-A.f32{suffix}';right=out/'edge'/f's{scenario}-p{parallel}-B.f32{suffix}'
                assert left.read_bytes()==right.read_bytes()
                for arm in ('A','B'):summary(out/'edge'/f's{scenario}-p{parallel}-{arm}.csv{suffix}')
                report['correctness'].append({'route':f'edge-s{scenario}-p{parallel}-{suffix}','floats':left.stat().st_size//4,'sha256':digest(left)})
        report['irs']={x.name:digest(x) for x in sorted((out/'irs').glob('*'))}
        if not a.correctness_only:
            for round_ in range(6):
                order='ABBA' if round_%2==0 else 'BAAB'
                for mode in (('control','profile') if round_%2==0 else ('profile','control')):
                    for group in (('AA','AB') if round_%2==0 else ('AB','AA')):
                        for slot,logical in enumerate(order):
                            arm=logical if group=='AB' else 'A'
                            t={'round':round_,'mode':mode,'group':group,'slot':slot,'logical':logical,'arm':arm,'routes':{}}
                            for family,block,channels in ROUTES:
                                key=f'{family}-{block}-{channels}';prefix=out/'trials'/f'r{round_}-{mode}-{group}-{slot}-{logical}'/key
                                command([build/f'{arm}_{mode}',family,block,channels,0,'-',str(prefix)+'.csv',1600,0],pathlib.Path(str(prefix)+'.log'),env)
                                t['routes'][key]=summary(pathlib.Path(str(prefix)+'.csv'))
                            report['trials'].append(t)
                            print(f'trial {len(report["trials"])}/96',flush=True)
            report['analysis']=analyze(report['trials'])
            report['apple_silicon_performance_demonstrated']=platform.system()=='Darwin' and platform.machine()=='arm64' and report['analysis']['all_routes_improved'] and report['analysis']['established_regressions']==0
        assert report['irs']=={x.name:digest(x) for x in sorted((out/'irs').glob('*'))}
        report['complete']=True
    finally:
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        manifest={str(p.relative_to(out)):{'bytes':p.stat().st_size,'sha256':digest(p)} for p in out.rglob('*') if p.is_file()}
        (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('correctness','trials','analysis','binaries','irs')},indent=2))

if __name__=='__main__':main()

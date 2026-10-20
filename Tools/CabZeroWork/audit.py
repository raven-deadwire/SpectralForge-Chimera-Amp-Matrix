"""Independent evidence completeness/identity and summary arithmetic audit."""
import argparse,csv,gzip,hashlib,json,math,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);a=p.parse_args();root=a.input
trials=json.loads((root/'trials.json').read_text());summary=json.loads((root/'summaries.json').read_text())
lookup={(s['trial'],s['route'],s['phase']):s for s in summary}
assert len(trials)==96 and len({t['name'] for t in trials})==96
expected={f'{e}-{b}-{c}' for e in range(3) for b in [32,64] for c in [1,2]}
canonical={p.name[:-3]:hashlib.sha256(gzip.decompress(p.read_bytes())).hexdigest() for p in (root/'canonical').glob('*.gz')}
metadata=json.loads((root/'build.json').read_text());states={};callbacks=0;verified=0;ir=None
for t in trials:
    assert t['returncode']==0 and t['identity_equal']
    assert t['outputs']==canonical
    assert t['binary_sha256']==metadata['variants'][t['variant']]['binaries']['probe']
    folder=root/'raw'/t['name'];files={p.name.split('.')[0]:p for p in folder.glob('*.csv.gz') if p.name!='ir.csv.gz'}
    assert set(files)==expected
    identity=gzip.decompress((folder/'ir.csv.gz').read_bytes())
    if ir is None:ir=identity
    assert ir==identity
    for route,file in files.items():
        with gzip.open(file,'rt') as f:rows=list(csv.DictReader(f))
        assert len(rows)==2000
        state=[];budget=int(route.split('-')[1])*1e6/96000
        for n,r in enumerate(rows):
            assert int(r['callback'])==n and abs(float(r['budget_us'])-budget)<1e-6
            wall,cpu=map(float,[r['wall_us'],r['cpu_us']])
            assert math.isfinite(wall) and math.isfinite(cpu) and min(wall,cpu)>=0
            assert int(r['wall_miss'])==int(wall>budget) and int(r['cpu_over'])==int(cpu>budget)
            assert int(r['wall_only'])==int(wall>budget and cpu<=budget)
            assert int(r['allocations'])==int(r['deletions'])==0
            state.append(tuple(r[k] for k in ['state_before','state_after','output_hash','fading_before','fading_after']))
        if route not in states:states[route]=state
        assert state==states[route];callbacks+=len(rows)
        for phase in ['cold','steady_before','publication','steady_after','fade_active']:
            selected=[r for r in rows if int(r['fading_before'])+int(r['fading_after'])>0] if phase=='fade_active' else [r for r in rows if r['phase']==phase]
            s=lookup[(t['name'],route,phase)];cpu=sorted(float(r['cpu_us']) for r in selected);wall=sorted(float(r['wall_us']) for r in selected)
            assert s['count']==len(selected)
            for field,value in [('cpu_p50',statistics.median(cpu)),('cpu_mean',statistics.mean(cpu)),('cpu_p99',cpu[int(len(cpu)*.99)]),('wall_p50',statistics.median(wall)),('wall_p99',wall[int(len(wall)*.99)])]:
                assert abs(s[field]-value)<1e-8
            assert s['wall_miss']==sum(v>budget for v in wall) and s['cpu_over']==sum(v>budget for v in cpu)
            verified+=1
result={'audit_pass':True,'trials':len(trials),'callbacks':callbacks,'summaries_recomputed':verified,
    'canonical_output_files':len(canonical),'all_output_sha256_equal':True,'all_states_equal':True,
    'all_clocks_finite':True,'allocation_deletion_observations_zero':True}
(root/'audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

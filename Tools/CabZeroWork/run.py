"""Sequential, state-matched A/B and identical-binary A/A collection.

Predeclared screening (not a product acceptance gate): six ABBA/BAAB quartets,
six interleaved A/A quartets per candidate; median paired CPU improvement >5%
and greater than the largest absolute A/A median shift, all six directions
positive, and no increase in route/phase wall misses or CPU-over-budget counts.
Both wall and CPU limits remain 1e6*block/96000, without deleting any outliers.
"""
import argparse,csv,gzip,hashlib,json,os,shutil,statistics,subprocess,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def run(binary,args,path):
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('w') as log:
        result=subprocess.run([str(binary),*map(str,args)],cwd=path.parent,stdout=log,stderr=subprocess.STDOUT)
    return result.returncode
def collect(build,out):
    out.mkdir(parents=True,exist_ok=True)
    shutil.copy2(build/'build.json',out/'build.json')
    provenance={'source_sha':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
        'inputs':{str(p.relative_to(ROOT)):digest(p) for folder in ['Source','Tests','Tools/CabZeroWork']
            for p in sorted((ROOT/folder).rglob('*')) if p.is_file() and '__pycache__' not in str(p)},
        'cpu':subprocess.check_output(['lscpu'],text=True)}
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    # All regressions finish before timing. Preserve failure logs and continue
    # other independent tests; no retries or changing their assertions.
    gates=[]
    for variant in ['baseline','empty_partition','zero_history']:
        for name,args in [('parity',[]),('realtime',[]),('integration',[]),
            ('integration',['--expanded']),('integration',['--layouts']),
            ('integration',['--expanded','--extended-buffers'])]:
            tag=variant+'-'+name+''.join(args).replace('--','-')
            envpath=out/'gates'/tag
            # Existing native test exports relative to this isolated cwd.
            rc=run(build/variant/name,args,envpath/'output.log')
            gates.append({'variant':variant,'test':name,'args':args,'returncode':rc})
            print('GATE',tag,rc,flush=True)
    (out/'gates.json').write_text(json.dumps(gates,indent=2)+'\n')
    canonical={};trials=[]
    for candidate in ['empty_partition','zero_history']:
        for q in range(6):
            order='ABBA' if q%2==0 else 'BAAB'
            for kind in ['AB','AA']:
                for position,arm in enumerate(order):
                    variant=candidate if kind=='AB' and arm=='B' else 'baseline'
                    name=f'{candidate}-{q}-{kind}-{position}-{arm}'
                    path=out/'raw'/name
                    started=time.time();rc=run(build/variant/'probe',[path],out/'logs'/(name+'.log'))
                    trial={'name':name,'candidate':candidate,'quartet':q,'kind':kind,'arm':arm,
                        'variant':variant,'position':position,'returncode':rc,'started_epoch':started,
                        'binary_sha256':digest(build/variant/'probe'),'outputs':{},'identity_equal':True}
                    if rc: raise RuntimeError(f'probe failed: {name}; evidence retained')
                    for file in sorted(path.glob('*.f32')):
                        d=digest(file);trial['outputs'][file.name]=d
                        if file.name not in canonical:
                            canonical[file.name]=d
                            dest=out/'canonical'/file.name;dest.parent.mkdir(exist_ok=True)
                            with gzip.open(str(dest)+'.gz','wb') as f:f.write(file.read_bytes())
                        if d!=canonical[file.name]:
                            trial['identity_equal']=False
                            # Keep differing output for exact residual analysis.
                        else:file.unlink()
                    for file in sorted(path.glob('*.csv')):
                        data=file.read_bytes()
                        with gzip.open(str(file)+'.gz','wb') as f:f.write(data)
                        file.unlink()
                    trials.append(trial)
                    (out/'trials.json').write_text(json.dumps(trials,indent=2)+'\n')
                    print('TRIAL',name,'rc',rc,'output_exact',trial['identity_equal'],flush=True)
    analyze(out)

def analyze(out, candidates=("empty_partition", "zero_history")):
    trials=json.loads((out/'trials.json').read_text());populations={};identities={};ir_identity=None
    def percentile(xs,p):return sorted(xs)[min(len(xs)-1,int(len(xs)*p))]
    summaries=[]
    for t in trials:
        path=out/'raw'/t['name'];pop={}
        with gzip.open(path/'ir.csv.gz','rt') as f: ir=f.read()
        if ir_identity is None:ir_identity=ir
        if ir!=ir_identity:raise RuntimeError('IR identity changed')
        for file in sorted(path.glob('*.csv.gz')):
            if file.name=='ir.csv.gz':continue
            with gzip.open(file,'rt') as f: rows=list(csv.DictReader(f))
            route=file.name.split('.')[0];block=int(route.split('-')[1]);budget=1e6*block/96000
            if len(rows)!=2000 or [int(r['callback']) for r in rows]!=list(range(2000)):raise RuntimeError('incomplete callback population')
            state=tuple((r['state_before'],r['state_after'],r['output_hash'],r['fading_before'],r['fading_after']) for r in rows)
            if route not in identities:identities[route]=state
            if state!=identities[route]:t['identity_equal']=False
            for r in rows:
                if abs(float(r['budget_us'])-budget)>1e-6 or int(r['allocations']) or int(r['deletions']):raise RuntimeError('budget/allocation contract')
                if int(r['wall_miss'])!=(float(r['wall_us'])>budget) or int(r['cpu_over'])!=(float(r['cpu_us'])>budget):raise RuntimeError('bad miss flags')
            for phase in ['cold','steady_before','publication','steady_after','fade_active']:
                subset=[r for r in rows if (int(r['fading_before']) or int(r['fading_after']))] if phase=='fade_active' else [r for r in rows if r['phase']==phase]
                cpu=[float(r['cpu_us']) for r in subset];wall=[float(r['wall_us']) for r in subset]
                s={'trial':t['name'],'route':route,'phase':phase,'count':len(subset),
                    'cpu_p50':statistics.median(cpu),'cpu_mean':statistics.mean(cpu),'cpu_p99':percentile(cpu,.99),
                    'wall_p50':statistics.median(wall),'wall_p99':percentile(wall,.99),
                    'wall_miss':sum(x>budget for x in wall),'cpu_over':sum(x>budget for x in cpu),
                    'cpu_accompanied':sum(float(r['wall_us'])>budget and float(r['cpu_us'])>budget for r in subset),
                    'wall_only':sum(float(r['wall_us'])>budget and float(r['cpu_us'])<=budget for r in subset)}
                pop[(route,phase)]=s;summaries.append(s)
        populations[t['name']]=pop
    decisions=[]
    for candidate in candidates:
        matched=all(t['identity_equal'] for t in trials if t['candidate']==candidate)
        gates=json.loads((out/'gates.json').read_text())
        native_gates_pass=all(g['returncode']==0 for g in gates if g['variant'] in ['baseline',candidate])
        for route,phase in next(iter(populations.values())):
            effects=[];noise=[];counts={a:{m:0 for m in ['wall_miss','cpu_over']} for a in 'AB'}
            for q in range(6):
                for kind,dest in [('AB',effects),('AA',noise)]:
                    group=[t for t in trials if t['candidate']==candidate and t['quartet']==q and t['kind']==kind]
                    if len(group)!=4:raise RuntimeError('quartet incomplete')
                    values={arm:[populations[t['name']][(route,phase)] for t in group if t['arm']==arm] for arm in 'AB'}
                    a=statistics.mean(v['cpu_p50'] for v in values['A']);b=statistics.mean(v['cpu_p50'] for v in values['B'])
                    dest.append(100*(1-b/a))
                    if kind=='AB':
                        for arm in 'AB':
                            for m in counts[arm]:counts[arm][m]+=sum(v[m] for v in values[arm])
            envelope=max(abs(n) for n in noise);effect=statistics.median(effects)
            improvement=matched and effect>max(5,envelope) and all(e>0 for e in effects)
            deadline_ok=all(counts['B'][m]<=counts['A'][m] for m in counts['A'])
            decisions.append({'candidate':candidate,'route':route,'phase':phase,'median_cpu_improvement_pct':effect,
                'paired_effects_pct':effects,'aa_noise_pct':noise,'aa_envelope_pct':envelope,
                'cpu_improvement_demonstrated':improvement,'identity_equal':matched,
                'deadline_counts':counts,'deadline_nonincrease':deadline_ok,
                'native_gates_pass':native_gates_pass,
                'local_screen_pass':improvement and deadline_ok and native_gates_pass})
    (out/'summaries.json').write_text(json.dumps(summaries,indent=2)+'\n')
    result={'schema':1,'trials':len(trials),'callbacks':len(trials)*12*2000,
        'all_output_and_state_equal':all(t['identity_equal'] for t in trials),'decisions':decisions,
        'macos_verified':False,'windows_verified':False,'full_processor_verified':False,
        'physical_daw_verified':False,'integration_eligible':False,'release_approved':False,'aax_approved':False}
    (out/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='decisions'}),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--analyze-only',action='store_true')
    a=p.parse_args()
    if a.analyze_only:analyze(a.output)
    else:collect(a.build.resolve(),a.output.resolve())

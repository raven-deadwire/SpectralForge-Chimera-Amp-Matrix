"""Independent Apple FFT experiment reusing PR109's unchanged CAB replay/screen.

No timing reruns, excluded outliers, threshold relaxation or product writes.
"""
import argparse, csv, gzip, hashlib, importlib.util, json, math, platform, shutil, statistics, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('zero_work',ROOT/'Tools/CabZeroWork/run.py')
zero=importlib.util.module_from_spec(spec);spec.loader.exec_module(zero)
ROUTES={f'{e}-{b}-{c}' for e in range(3) for b in [32,64] for c in [1,2]}
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def execute(binary,args,log):
    log.parent.mkdir(parents=True,exist_ok=True)
    with log.open('w') as f:r=subprocess.run([str(binary),*map(str,args)],cwd=log.parent,stdout=f,stderr=subprocess.STDOUT)
    return r.returncode

def preserve_outputs(folder,canonical,out):
    hashes={};equal=True
    for p in sorted(folder.glob('*.f32')):
        d=sha(p);hashes[p.name]=d
        if p.name not in canonical:
            canonical[p.name]=d;dest=out/'canonical';dest.mkdir(exist_ok=True)
            with gzip.open(dest/(p.name+'.gz'),'wb') as f:f.write(p.read_bytes())
        if canonical[p.name]!=d:equal=False
        else:p.unlink()
    for p in folder.glob('*.csv'):
        with gzip.open(str(p)+'.gz','wb') as f:f.write(p.read_bytes())
        p.unlink()
    return hashes,equal

def audit_cab(out,trials,expected_trials):
    if len(trials)!=expected_trials or len({t['name'] for t in trials})!=expected_trials:raise ValueError('missing/duplicate trial')
    states={};ir=None;callbacks=0
    for t in trials:
        path=out/'raw'/t['name']
        files={p.name.split('.')[0]:p for p in path.glob('*.csv.gz') if p.name!='ir.csv.gz'}
        if set(files)!=ROUTES or set(t['outputs'])!={r+'.f32' for r in ROUTES}:raise ValueError('missing/extra CAB route/output')
        identity=gzip.decompress((path/'ir.csv.gz').read_bytes())
        if ir is None:ir=identity
        if identity!=ir:raise ValueError('IR identity changed')
        for route,file in files.items():
            with gzip.open(file,'rt') as f:rows=list(csv.DictReader(f))
            if len(rows)!=2000:raise ValueError('missing callbacks')
            signature=[];budget=1e6*int(route.split('-')[1])/96000
            for n,r in enumerate(rows):
                cpu,wall=float(r['cpu_us']),float(r['wall_us'])
                if not all(math.isfinite(x) and x>=0 for x in [cpu,wall]):raise ValueError('invalid clock')
                if int(r['callback'])!=n or abs(float(r['budget_us'])-budget)>1e-6:raise ValueError('callback/budget')
                if int(r['wall_miss'])!=(wall>budget) or int(r['cpu_over'])!=(cpu>budget) or int(r['wall_only'])!=(wall>budget and cpu<=budget):raise ValueError('miss flags')
                if int(r['allocations']) or int(r['deletions']):raise ValueError('RT new/delete')
                signature.append(tuple(r[k] for k in ['state_before','state_after','output_hash','fading_before','fading_after']))
            if route not in states:states[route]=signature
            if signature!=states[route]:t['identity_equal']=False
            callbacks+=len(rows)
    write(out/'audit.json',{'complete':True,'trials':len(trials),'callbacks':callbacks,
          'state_and_output_equal':all(t['identity_equal'] for t in trials),
          'finite_clocks':True,'callback_new_delete_observations':0})

def collect(build,out,correctness_only):
    if out.exists():raise ValueError('fresh output directory required; do not overwrite a population')
    out.mkdir(parents=True)
    bins={v:{k:build/'bin'/f'apple-{v}-{k}' for k in ['fft-check','fft-probe','parity','realtime','integration','ir-state','probe']} for v in ['baseline','candidate']}
    manifest={'source_sha':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
      'platform':platform.platform(),'machine':platform.machine(),'native_macos':platform.system()=='Darwin',
      'variants':{v:{'binaries':{k:sha(p) for k,p in paths.items()}} for v,paths in bins.items()},
      'inputs':{str(p.relative_to(ROOT)):sha(p) for folder in ['Source','Tests','Tools/CabZeroWork','Tools/CabAppleFFT']
                for p in sorted((ROOT/folder).rglob('*')) if p.is_file() and '__pycache__' not in str(p)}}
    write(out/'build.json',manifest)
    shutil.copy2(build/'overlay/static-verification.json',out/'static-verification.json')
    for name in ['manual-build.json','CMakeCache.txt','compile_commands.json']:
        if (build/name).exists():shutil.copy2(build/name,out/name)
    for cmd,name in [(['sysctl','-a'],'sysctl.txt'),(['sw_vers'],'sw-vers.txt'),(['clang++','--version'],'compiler.txt')] if platform.system()=='Darwin' else [(['lscpu'],'lscpu.txt'),(['g++','--version'],'compiler.txt')]:
        execute(Path(shutil.which(cmd[0])),cmd[1:],out/name)
    gates=[];apple=[]
    for v in bins:
        checks=[('fft-check',[out/f'{v}-fft-bits.csv']),('parity',[]),('realtime',[]),('ir-state',[]),
                ('integration',[]),('integration',['--expanded']),('integration',['--layouts']),('integration',['--expanded','--extended-buffers'])]
        for k,args in checks:
            tag=v+'-'+k+''.join(str(x) for x in args if str(x).startswith('--'))
            rc=execute(bins[v][k],args,out/'gates'/tag/'output.log')
            gates.append({'variant':v,'test':k,'args':list(map(str,args)),'returncode':rc})
            write(out/'gates.json',gates);print('GATE',tag,rc,flush=True)
        with (out/f'{v}-fft-bits.csv').open() as f:rows=list(csv.DictReader(f))
        if len(rows)!=180:raise ValueError('incomplete FFT equivalence suite')
        apple.extend(int(r['apple_backend'])==1 for r in rows)
    native_apple=manifest['native_macos'] and all(apple)
    write(out/'backend.json',{'native_macos':manifest['native_macos'],'all_cases_apple_backend':all(apple),
                             'performance_claim_allowed':native_apple and not correctness_only})
    if not correctness_only and not native_apple:raise ValueError('full performance run requires verified native AppleFFT')
    trials=[];canonical={}
    schedule=[(0,'control',i,a) for i,a in enumerate('AB')] if correctness_only else [
        (q,kind,pos,arm) for q in range(6) for kind in ['AB','AA']
        for pos,arm in enumerate('ABBA' if q%2==0 else 'BAAB')]
    for q,kind,pos,arm in schedule:
        v='candidate' if kind!='AA' and arm=='B' else 'baseline'
        name=f'candidate-{q}-{kind}-{pos}-{arm}';path=out/'raw'/name
        rc=execute(bins[v]['probe'],[path],out/'logs'/(name+'.log'))
        t={'name':name,'candidate':'candidate','quartet':q,'kind':kind,'arm':arm,'position':pos,
           'variant':v,'returncode':rc,'binary_sha256':sha(bins[v]['probe'])}
        if rc:
            write(out/'failed-trial.json',t);raise RuntimeError('CAB probe failed; evidence retained')
        t['outputs'],t['identity_equal']=preserve_outputs(path,canonical,out)
        trials.append(t);write(out/'trials.json',trials);print('TRIAL',name,t['identity_equal'],flush=True)
    audit_cab(out,trials,2 if correctness_only else 48)
    write(out/'trials.json',trials)
    if correctness_only:
        write(out/'decision.json',{'decision':'not_adopted_native_measurement_required','native_apple_executed':native_apple,
              'negative_control_only':not native_apple,'correctness_gates_pass':all(g['returncode']==0 for g in gates),
              'cab_output_state_equal':all(t['identity_equal'] for t in trials),'performance_improvement_demonstrated':False,
              'integration_eligible':False,'release_approved':False,'aax_approved':False})
        return
    zero.analyze(out,candidates=('candidate',))
    # FFT timing is a separate population, with its own identical-binary A/A.
    fftout=out/'fft';fftout.mkdir();ffttrials=[];canonical={};summaries={}
    for q,kind,pos,arm in schedule:
        v='candidate' if kind=='AB' and arm=='B' else 'baseline'
        name=f'fft-{q}-{kind}-{pos}-{arm}';path=fftout/'raw'/name
        rc=execute(bins[v]['fft-probe'],[path],fftout/'logs'/(name+'.log'))
        if rc:raise RuntimeError('FFT probe failed; evidence retained')
        with (path/'fft.csv').open() as f:rows=list(csv.DictReader(f))
        if len(rows)!=800:raise ValueError('incomplete FFT timing')
        per={}
        for order in [7,8,9,10]:
            group=[r for r in rows if int(r['order'])==order]
            if [int(r['batch']) for r in group]!=list(range(200)):raise ValueError('FFT batch population')
            if any(int(r['apple_backend'])!=1 or int(r['transforms'])!=128 for r in group):raise ValueError('FFT backend/count')
            if any(not math.isfinite(float(r[k])) or float(r[k])<0 for r in group for k in ['cpu_us','wall_us']):raise ValueError('FFT clock')
            per[order]={k:statistics.median(float(r[k])/128 for r in group) for k in ['cpu_us','wall_us']}
        summaries[name]=per
        hashes,equal=preserve_outputs(path,canonical,fftout)
        ffttrials.append({'name':name,'quartet':q,'kind':kind,'arm':arm,'position':pos,'variant':v,
                          'binary_sha256':sha(bins[v]['fft-probe']),'outputs':hashes,'identity_equal':equal})
        write(fftout/'trials.json',ffttrials);print('FFT',name,equal,flush=True)
    fft_decisions=[]
    for order in [7,8,9,10]:
        effects=[];noise=[]
        for q in range(6):
            for kind,dest in [('AB',effects),('AA',noise)]:
                group=[t for t in ffttrials if t['quartet']==q and t['kind']==kind]
                mean={arm:statistics.mean(summaries[t['name']][order]['cpu_us'] for t in group if t['arm']==arm) for arm in 'AB'}
                dest.append(100*(1-mean['B']/mean['A']))
        median=statistics.median(effects);envelope=max(map(abs,noise))
        fft_decisions.append({'order':order,'paired_effects_pct':effects,'aa_noise_pct':noise,
          'median_cpu_improvement_pct':median,'aa_envelope_pct':envelope,
          'screen_pass':median>max(5,envelope) and min(effects)>0 and all(t['identity_equal'] for t in ffttrials)})
    write(fftout/'summaries.json',summaries);write(fftout/'analysis.json',fft_decisions)
    analysis=json.loads((out/'analysis.json').read_text())
    numerical_ok=all(g['returncode']==0 for g in gates if g['test']=='fft-check') and all(t['identity_equal'] for t in trials+ffttrials)
    all_gates=len(gates)==16 and all(g['returncode']==0 for g in gates)
    stereo=[d for d in analysis['decisions'] if d['route'].endswith('-2')]
    # Require broad benefit, not a cherry-picked route. Mono is an unchanged control.
    eligible=numerical_ok and all_gates and all(d['screen_pass'] for d in fft_decisions) and all(d['local_screen_pass'] for d in stereo) and all(d['deadline_nonincrease'] for d in analysis['decisions'])
    decision='eligible_for_separate_product_review' if eligible else 'reject_current_candidate' if not numerical_ok else 'not_adopted_benefit_not_proven'
    write(out/'decision.json',{'decision':decision,'native_apple_executed':True,'numerical_equivalence':numerical_ok,
         'all_correctness_gates_pass':all_gates,'fft_screens_pass':sum(d['screen_pass'] for d in fft_decisions),
         'cab_stereo_screens_pass':sum(d['local_screen_pass'] for d in stereo),'cab_stereo_screens_total':len(stereo),
         'integration_eligible':False,'release_approved':False,'aax_approved':False,
         'full_processor_verified':False,'physical_daw_verified':False})

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--correctness-only',action='store_true')
    a=p.parse_args();collect(a.build.resolve(),a.output.resolve(),a.correctness_only)

#!/usr/bin/env python3
"""Pinned NEON-only comparison. No release or product approval is granted here."""
from __future__ import annotations
import argparse, csv, hashlib, importlib.util, json, math, os, platform, random
import shutil, statistics, subprocess, sys, traceback
from pathlib import Path

PARENT='c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
CANDIDATE='29865abb4da3ab78592c970a263ee62af63fc673'
REJECTED='6b66180d22f4f354aad63691d981e5295fccbd55'
JUCE='d6181bde38d858c283c3b7bf699ce6340c050b5d'
ROUNDS=6
ENGINES={'original-v1':[], 'expanded-v2':['--expanded'], 'array-v3-6x10':['--layouts']}
TESTS=['ChimeraOriginalCabIntegrationTests','ChimeraOriginalCabRealtimeTests',
       'ChimeraCabExpansionIntegrationTests','ChimeraCabExtendedTimingTests','ChimeraCabLayoutIntegrationTests']

def write(path, data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def capture(cmd,cwd=None): return subprocess.check_output(cmd,cwd=cwd,text=True).strip()
def percentile(values,p): return sorted(values)[min(len(values)-1,int(len(values)*p))]

def comparison(ab,aa):
    """Quartets, not autocorrelated callbacks, are the independent units."""
    if len(ab)!=ROUNDS or len(aa)!=ROUNDS or not all(math.isfinite(v) for v in ab+aa):
        raise ValueError('Incomplete/nonfinite quartet population')
    rng=random.Random(73015)
    boot=[statistics.mean(rng.choices(ab,k=len(ab))) for _ in range(10000)]
    noise=max(abs(v) for v in aa)
    low,high=percentile(boot,.025),percentile(boot,.975)
    orders=[statistics.mean(ab[::2]),statistics.mean(ab[1::2])]
    return dict(improvement_pct=statistics.mean(ab),ci95_pct=[low,high],
        aa_max_abs_pct=noise,aa_effects_pct=aa,ab_effects_pct=ab,order_effects_pct=orders,
        demonstrated=low>max(2.,noise) and min(orders)>0,
        regression=high < -max(2.,noise))

def generate_probe(source):
    # Diagnostic-only copy; every product source and original acceptance test stays untouched.
    text=source.read_text()
    replacements={
      'const std::vector<int> blocks=extendedBuffers ? std::vector<int>{32,64,128,256,512} : std::vector<int>{64,256};':
      'const std::vector<int> blocks{32,64};',
      'for(double sr:{44100.,48000.,96000.})':'for(double sr:{96000.})',
      '        reportProfile(engineName,sr,block,channels,"worker_and_automation",blockProfiles,0,1200);':
      '''        // Per-callback spectral MAC counters, emitted after the timed population.
        auto spectralPath=timingPath; spectralPath += ".spectral.csv";
        std::ofstream spectral(spectralPath); require(bool(spectral),"spectral CSV open failed");
        spectral << "callback_index,wall_ns,calls\\n";
        for(size_t i=0;i<blockProfiles.size();++i) {
            const auto stage=unsigned(cabProfile::Stage::SpectralMAC);
            spectral << i << ',' << blockProfiles[i].nanoseconds[stage] << ',' << blockProfiles[i].calls[stage] << '\\n';
        }
        spectral.close();require(bool(spectral),"spectral CSV flush failed");
        reportProfile(engineName,sr,block,channels,"worker_and_automation",blockProfiles,0,1200);'''}
    for before,after in replacements.items():
        if text.count(before)!=1: raise RuntimeError('Diagnostic copy anchor changed')
        text=text.replace(before,after)
    return text

def read_trial(root, mode, engine):
    suite='profile' if mode=='profile' else 'integration'
    result=[]
    for block in (32,64):
      for channels in (1,2):
        path=root/'cab-timing-diagnostics'/suite/engine/f'96000-{block}-{channels}.csv'
        rows=list(csv.DictReader(path.open()))
        if len(rows)!=1600: raise ValueError('Missing callback rows: '+str(path))
        previous=-1.
        for i,r in enumerate(rows):
            expected={'callback_index':str(i),'sample_rate':'96000','block_size':str(block),
              'channels':str(channels),'engine':engine,'suite':suite,'schema_version':'1',
              'phase':'worker_and_automation' if i<1200 else 'forced_six_slot_publication',
              'phase_index':str(i if i<1200 else i-1200),'automation_requested':str(int(i<80))}
            if any(r[k]!=v for k,v in expected.items()): raise ValueError('Changed route/phase identity')
            w,c=float(r['wall_us']),float(r['thread_cpu_us'])
            start,end=float(r['thread_cpu_start_us']),float(r['thread_cpu_end_us'])
            if r['thread_cpu_valid']!='1' or not all(math.isfinite(v) and v>=0 for v in (w,c,start,end)):
                raise ValueError('Invalid native clocks')
            if start<previous or end<start or not math.isclose(c,end-start,abs_tol=1e-8,rel_tol=1e-12):
                raise ValueError('Unpaired/nonmonotonic CPU clock')
            previous=end
            if not math.isclose(float(r['budget_us']),1e6*block/96000,rel_tol=1e-12): raise ValueError('Changed deadline')
            if int(r['deadline_miss'])!=int(w>1e6*block/96000): raise ValueError('Suppressed deadline miss')
        spectral=None
        if mode=='profile':
            spectral=list(csv.DictReader(Path(str(path)+'.spectral.csv').open()))
            if len(spectral)!=1600 or any(int(r['callback_index'])!=i or int(r['wall_ns'])<0 or int(r['calls'])<0 for i,r in enumerate(spectral)):
                raise ValueError('Incomplete spectral MAC population')
        for phase,lo,hi in [('all',0,1600),('worker_and_automation',0,1200),('forced_six_slot_publication',1200,1600)]:
            pop=rows[lo:hi]; walls=[float(r['wall_us']) for r in pop]; cpus=[float(r['thread_cpu_us']) for r in pop]
            record=dict(engine=engine,block=block,channels=channels,phase=phase,callbacks=len(pop),
              wall_p99_us=percentile(walls,.99),wall_max_us=max(walls),cpu_mean_us=statistics.mean(cpus),
              cpu_p50_us=percentile(cpus,.5),cpu_p99_us=percentile(cpus,.99),
              deadline_misses=sum(int(r['deadline_miss']) for r in pop),
              cpu_over_budget=sum(c>1e6*block/96000 for c in cpus))
            if spectral:
                mac=[int(r['wall_ns'])*.001 for r in spectral[lo:hi]]
                calls=[int(r['calls']) for r in spectral[lo:hi]]
                record.update(mac_mean_us=statistics.mean(mac),mac_p99_us=percentile(mac,.99),
                    mac_calls=sum(calls),mac_call_schedule_sha256=hashlib.sha256(json.dumps(calls).encode()).hexdigest())
            result.append(record)
    return result

def analyze(trials):
    groups={}
    for trial in trials:
      for row in trial['rows']:
        key=(trial['mode'],row['engine'],row['block'],row['channels'],row['phase'])
        groups.setdefault(key,[]).append((trial,row))
    if len(groups)!=72: raise ValueError('Incomplete mode/engine/buffer/channel/phase grid')
    reports=[]
    for key,entries in sorted(groups.items()):
        if len(entries)!=ROUNDS*8: raise ValueError('Missing or duplicated trial')
        metrics=['wall_p99_us','cpu_mean_us','cpu_p50_us','cpu_p99_us']
        if key[0]=='profile': metrics+=['mac_mean_us','mac_p99_us']
        record=dict(zip(('mode','engine','block','channels','phase'),key)); record['metrics']={}
        for metric in metrics:
            effects={}
            for experiment in ('AB','AA'):
                values=[]
                for round_ in range(ROUNDS):
                    rows=[(t,r) for t,r in entries if t['experiment']==experiment and t['round']==round_]
                    if sorted(t['slot'] for t,r in rows)!=[0,1,2,3]: raise ValueError('Invalid quartet')
                    a=[r[metric] for t,r in rows if t['logical']=='A']
                    b=[r[metric] for t,r in rows if t['logical']=='B']
                    if len(a)!=2 or len(b)!=2 or min(a+b)<=0: raise ValueError('Invalid ratio population')
                    values.append(100*(1-math.exp(statistics.mean(map(math.log,b))-statistics.mean(map(math.log,a)))))
                effects[experiment]=values
            result=comparison(effects['AB'],effects['AA'])
            result['parent_median']=statistics.median(r[metric] for t,r in entries if t['experiment']=='AB' and t['logical']=='A')
            result['candidate_median']=statistics.median(r[metric] for t,r in entries if t['experiment']=='AB' and t['logical']=='B')
            record['metrics'][metric]=result
        record['misses']={arm:{m:sum(r[m] for t,r in entries if t['experiment']=='AB' and t['logical']==arm) for m in ('callbacks','deadline_misses','cpu_over_budget')} for arm in ('A','B')}
        record['mac_exposure_identical']=(len({r['mac_call_schedule_sha256'] for t,r in entries})==1) if key[0]=='profile' else None
        reports.append(record)
    primary=[r for r in reports if r['phase']=='forced_six_slot_publication']
    demonstrated=all(r['metrics']['cpu_mean_us' if r['mode']=='control' else 'mac_mean_us']['demonstrated'] for r in primary)
    demonstrated=demonstrated and all(r['mac_exposure_identical'] for r in primary if r['mode']=='profile')
    regressions=[{k:r[k] for k in ('mode','engine','block','channels','phase')} for r in reports if r['mode']=='control' and (r['metrics']['wall_p99_us']['regression'] or r['metrics']['cpu_mean_us']['regression'])]
    return dict(complete=True,performance_demonstrated=demonstrated and not regressions,regressions=regressions,rows=reports)

def main():
    parser=argparse.ArgumentParser();
    for name in ('parent','candidate','build','output'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); parent,candidate,build,out=[getattr(args,n).resolve() for n in ('parent','candidate','build','output')]
    if platform.system()!='Darwin' or platform.machine()!='arm64': raise RuntimeError('Requires native Apple Silicon macOS')
    if build.exists() or out.exists(): raise RuntimeError('Fresh directories required; preserve prior attempts')
    out.mkdir(parents=True);build.mkdir(parents=True)
    controller=Path(__file__).resolve().parent
    spec=importlib.util.spec_from_file_location('gate_driver',parent/'Tools/run_cab_realtime_ci.py')
    driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver)
    report=dict(parent=PARENT,candidate=CANDIDATE,head_immediate_parent=REJECTED,
        rounds=ROUNDS,complete=False,performance_demonstrated=False,integration_eligible=False,
        release_approved=False,full_product_validation='PENDING',daw_acceptance='PENDING',
        commands=[],gates={},trials=[],errors=[])
    def save(): write(out/'report.partial.json',report)
    def run(cmd,cwd,log,required=True):
        receipt=driver.run_logged(cmd,cwd,out/log,timeout=1800,measured=True)
        report['commands'].append(receipt);save()
        if required and receipt['returncode']: raise RuntimeError(str(log)+' failed')
        return receipt
    try:
        report['sources']={arm:driver.source_snapshot(path,sha) for arm,path,sha in [('A',parent,PARENT),('B',candidate,CANDIDATE)]}
        if capture(['git','rev-parse',CANDIDATE+'^'],candidate)!=REJECTED or capture(['git','rev-parse',REJECTED+'^'],candidate)!=PARENT:
            raise RuntimeError('NEON ancestry changed')
        changed=capture(['git','diff','--name-only',PARENT,CANDIDATE],candidate).splitlines()
        if changed!=['Source/ModeledCabConvolution.h']: raise RuntimeError('Non-NEON source changes contaminate comparison')
        (out/'product-only.diff').write_text(capture(['git','diff',PARENT,CANDIDATE],candidate)+'\n')
        report['environment_before']=driver.environment_snapshot(out/'environment-before')
        test=parent/'Tests/OriginalCabIntegrationTests.cpp'
        if test.read_bytes()!=(candidate/'Tests/OriginalCabIntegrationTests.cpp').read_bytes(): raise RuntimeError('Test harness changed')
        probe=build/'probe.cpp';probe.write_text(generate_probe(test));shutil.copy2(probe,out/'generated-probe.cpp')
        report['probe_sha256']=digest(probe)
        # Configure/build every arm before any measured population; no concurrent compiler or benchmark.
        for arm,source in [('A',parent),('B',candidate)]:
            dest=build/arm
            cmd=['cmake','-S',controller,'-B',dest,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                '-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0',
                f'-DCHIMERA_PRODUCT_SOURCE={source}',f'-DCHIMERA_PROBE_SOURCE={probe}']
            if arm=='B': cmd.append(f'-DFETCHCONTENT_SOURCE_DIR_JUCE={build/"A/_deps/juce-src"}')
            run(cmd,source,Path('build-logs')/(arm+'-configure.log'))
            run(['cmake','--build',dest,'--config','Release','--target','ChimeraOriginalCabIntegrationTests',
                'ChimeraOriginalCabRealtimeTests','ChimeraCabABControl','ChimeraCabABProfile','--parallel','2'],source,Path('build-logs')/(arm+'-build.log'))
            metadata=out/'build-metadata'/arm;metadata.mkdir(parents=True)
            for path in [dest/'CMakeCache.txt',dest/'compile_commands.json',*dest.glob('CMakeFiles/*/CMakeCXXCompiler.cmake')]:
                if path.exists(): shutil.copy2(path,metadata/path.name)
        if capture(['git','rev-parse','HEAD'],build/'A/_deps/juce-src')!=JUCE: raise RuntimeError('JUCE dependency changed')
        report['juce_sha']=JUCE
        binaries={}
        for arm in ('A','B'):
            binaries[arm]={mode:driver.executable(build/arm,'ChimeraCabAB'+suffix) for mode,suffix in [('control','Control'),('profile','Profile')]}
        report['binaries']={arm:{m:dict(path=str(p),sha256=digest(p)) for m,p in modes.items()} for arm,modes in binaries.items()}
        # Original unmodified five CTests, 66 deadlines, 144 realtime routes and frozen bits on BOTH arms.
        for arm,source in [('A',parent),('B',candidate)]:
            dest=out/'gates'/arm;dest.mkdir(parents=True)
            regex='^('+'|'.join(TESTS)+')$';product=build/arm/'product'
            inventory=json.loads(capture(['ctest','--test-dir',str(product),'-C','Release','-R',regex,'--show-only=json-v1']))
            if sorted(t['name'] for t in inventory['tests'])!=sorted(TESTS): raise RuntimeError('Original test inventory changed')
            write(dest/'inventory.json',inventory)
            receipt=run(['ctest','--test-dir',product,'-C','Release','-R',regex,'--parallel','1','--verbose','--no-tests=error',
                '--timeout','600','--test-output-size-passed','10485760','--test-output-size-failed','10485760',
                '--output-junit',dest/'ctest.xml'],source,Path('gates')/arm/'console.log',required=False)
            driver.preserve_timing_samples(product,dest,('integration','extended'))
            shutil.copy2(product/'Testing/Temporary/LastTest.log',dest/'LastTest.log')
            specs=dict(driver.DEFAULT_TESTS);specs[driver.EXTENDED_TEST]=('expanded-v2',(32,64,128,256,512))
            evidence=driver.test_evidence(dest/'ctest.xml',specs,True)
            evidence['raw_verification']=driver.validate_timing_samples(dest,evidence['timing'])
            if len(evidence['timing'])!=66: raise RuntimeError('Missing original deadline routes')
            log=(dest/'console.log').read_text()
            evidence['frozen_bits_and_latency']=('exact_bits=1 immutable_spectra=1' in log and 'latency=0' in log and receipt['returncode']==0)
            evidence['returncode']=receipt['returncode'];report['gates'][arm]=evidence;save()
        # 6 AB and 6 AA quartets per mode. AA uses the exact same parent executable/hash in every slot.
        # Alternating outer order prevents always assigning AA/observer work to one time region.
        for round_ in range(ROUNDS):
          order='ABBA' if round_%2==0 else 'BAAB'
          for mode in (('control','profile') if round_%2==0 else ('profile','control')):
           for experiment in (('AA','AB') if round_%2==0 else ('AB','AA')):
            for slot,logical in enumerate(order):
                arm=logical if experiment=='AB' else 'A'
                trial=dict(round=round_,order=order,mode=mode,experiment=experiment,slot=slot,logical=logical,arm=arm,
                    binary_sha256=digest(binaries[arm][mode]),rows=[])
                label=f'r{round_}-{mode}-{experiment}-{slot}-{logical}'
                for engine,flags in ENGINES.items():
                    directory=out/'trials'/label/engine;directory.mkdir(parents=True)
                    receipt=run([binaries[arm][mode],*flags],directory,Path('trials')/label/(engine+'.log'),required=False)
                    # A deadline failure stays recorded and does not stop later scheduled arms.
                    log=(out/'trials'/label/(engine+'.log')).read_text()
                    if receipt['returncode'] and 'CAB-only p99 exceeds one block period (runner CPU gate)' not in log:
                        report['errors'].append(label+'/'+engine+': semantic/process failure')
                    trial['rows']+=read_trial(directory,mode,engine)
                report['trials'].append(trial);save()
        analysis=analyze(report['trials']);write(out/'analysis.json',analysis)
        report.update(complete=not report['errors'],performance_demonstrated=analysis['performance_demonstrated'])
        candidate_gate=report['gates']['B']
        report['integration_eligible']=report['complete'] and report['performance_demonstrated'] and candidate_gate['returncode']==0 and candidate_gate['frozen_bits_and_latency']
        report['environment_after']=driver.environment_snapshot(out/'environment-after')
        for source,sha in [(parent,PARENT),(candidate,CANDIDATE)]: driver.source_snapshot(source,sha)
        lines=['# Independent PR73 NEON comparison','',f'Parent `{PARENT}`; candidate `{CANDIDATE}`.',
          f'Complete: {report["complete"]}; performance demonstrated: {report["performance_demonstrated"]}; integration eligible for review only: {report["integration_eligible"]}.',
          'Full product validation, actual DAW acceptance and release approval remain PENDING.','',
          'Primary fixed forced-publication phase; medians across 12 observations per arm. Positive percentages mean improvement.',
          '| mode | engine | block | ch | metric | A µs | B µs | improvement % | 95% CI % | AA envelope % | proven |',
          '|---|---|---:|---:|---|---:|---:|---:|---|---:|---|']
        for r in analysis['rows']:
            if r['phase']!='forced_six_slot_publication': continue
            for metric in (['cpu_mean_us','wall_p99_us'] if r['mode']=='control' else ['mac_mean_us']):
                m=r['metrics'][metric]
                lines.append(f'| {r["mode"]} | {r["engine"]} | {r["block"]} | {r["channels"]} | {metric} | {m["parent_median"]:.3f} | {m["candidate_median"]:.3f} | {m["improvement_pct"]:.2f} | {m["ci95_pct"]} | {m["aa_max_abs_pct"]:.2f} | {m["demonstrated"]} |')
        (out/'README.md').write_text('\n'.join(lines)+'\n')
    except Exception:
        report['errors'].append(traceback.format_exc());print(report['errors'][-1]);report['complete']=False
    finally:
        write(out/'report.json',report)
        write(out/'manifest.json',{str(p.relative_to(out)):dict(bytes=p.stat().st_size,sha256=digest(p)) for p in out.rglob('*') if p.is_file() and p.name!='manifest.json'})
        print(json.dumps({k:report[k] for k in ('complete','performance_demonstrated','integration_eligible','release_approved','errors')}))
    return 0 if report['complete'] else 1
if __name__=='__main__': sys.exit(main())

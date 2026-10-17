#!/usr/bin/env python3
"""Pinned PR74 crossover protocol for one independent bounded prefetch candidate.
Only source identity, provenance and fail-closed gate checks are specialized.
No product, test, timing threshold, estimator or schedule is changed.
"""
from __future__ import annotations
import argparse, csv, hashlib, importlib.util, json, math, os, platform, random
import shutil, statistics, subprocess, sys, traceback
from pathlib import Path
from prepare import PARENT, HEADER, KINDS, generate

ROOT=Path(__file__).resolve().parents[2]
SHARED=ROOT/'Tools/CabNeonAB'

for filename,expected in {
    'run.py':'45ce016e4ed9620f4f3ac2a1ea25f6b8d0fafd57f4917a92786b74e6e27580bd',
    'CMakeLists.txt':'a35d5ed78555f3ce94717fe326885699baa417ea64cdcbcd911a58bc3e0c550c',
}.items():
    if hashlib.sha256((SHARED/filename).read_bytes()).hexdigest()!=expected:
        raise RuntimeError('Pinned PR74 experiment changed: '+filename)
spec=importlib.util.spec_from_file_location('pr74_protocol',SHARED/'run.py')
ab=importlib.util.module_from_spec(spec);spec.loader.exec_module(ab)
JUCE,ROUNDS,ENGINES,TESTS=ab.JUCE,ab.ROUNDS,ab.ENGINES,ab.TESTS
write,digest,capture=ab.write,ab.digest,ab.capture
generate_probe,read_trial,analyze=ab.generate_probe,ab.read_trial,ab.analyze

def contracts_present(log):
    return all(s in log for s in (
        'PASS CAB realtime equivalence routes=144 ',
        'exact_float_samples=2645088 fade_mix_samples=388800 exact_bits=1 immutable_spectra=1',
        'active=6 fading=3 fade_ms=50 latency=0'))

def gates_pass(gate):
    return (gate['returncode']==0 and gate['frozen_bits_and_latency']
        and len(gate['tests'])==5 and all(t['passed'] for t in gate['tests'])
        and len(gate['timing'])==66 and all(t['p99_within_deadline'] for t in gate['timing']))

def verify_compile_commands(out,report):
    normalized={}
    for arm in ('A','B'):
        source=report['sources'][arm]['path']
        build=str(Path(report['binaries'][arm]['control']['path']).parents[2])
        juce=str(Path(report['binaries']['A']['control']['path']).parents[2]/'_deps/juce-src')
        selected={}
        for c in json.loads((out/'build-metadata'/arm/'compile_commands.json').read_text()):
            if c['file'].endswith('/probe.cpp'):
                command=c['command'].replace(juce,'{JUCE}').replace(source,'{SOURCE}').replace(build,'{BUILD}')
                mode='profile' if 'CHIMERA_CAB_PROFILE=1' in command else 'control'
                if mode in selected: raise RuntimeError('Duplicate probe compile command')
                selected[mode]=command
        if set(selected)!={'control','profile'}: raise RuntimeError('Missing compiler commands')
        normalized[arm]=selected
    if normalized['A']!=normalized['B']: raise RuntimeError('Scalar A/B compiler flags differ')
    write(out/'normalized-compiler-commands.json',normalized)
    return True

def main():
    parser=argparse.ArgumentParser();
    for name in ('parent','candidate','build','output'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--kind',choices=KINDS,required=True)
    args=parser.parse_args(); parent,candidate,build,out=[getattr(args,n).resolve() for n in ('parent','candidate','build','output')]
    if platform.system()!='Darwin' or platform.machine()!='arm64': raise RuntimeError('Requires native Apple Silicon macOS')
    if build.exists() or out.exists(): raise RuntimeError('Fresh directories required; preserve prior attempts')
    out.mkdir(parents=True);build.mkdir(parents=True)
    CANDIDATE=capture(['git','rev-parse','HEAD'],candidate)
    baseline=capture(['git','show',PARENT+':'+HEADER],parent)+'\n'
    HEADER_SHA256=hashlib.sha256(generate(baseline,args.kind).encode()).hexdigest()
    controller=SHARED
    spec=importlib.util.spec_from_file_location('gate_driver',parent/'Tools/run_cab_realtime_ci.py')
    driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver)
    report=dict(parent=PARENT,candidate=CANDIDATE,head_immediate_parent=PARENT,
        kind=args.kind,header_sha256=HEADER_SHA256,
        controller_sha=capture(['git','rev-parse','HEAD'],ROOT),
        measurement_scope='Complete CAB callback (3 rigs / 6 mics); not PluginProcessor or DAW/device callback',
        rounds=ROUNDS,complete=False,performance_demonstrated=False,integration_eligible=False,
        release_approved=False,full_product_validation='PENDING',daw_acceptance='PENDING',
        commands=[],gates={},trials=[],errors=[])
    def save(): write(out/'report.partial.json',report)
    def run(cmd,cwd,log,required=True):
        print('RUN', str(log), flush=True)
        receipt=driver.run_logged(cmd,cwd,out/log,timeout=1800,measured=True)
        report['commands'].append(receipt);save()
        if required and receipt['returncode']: raise RuntimeError(str(log)+' failed')
        return receipt
    try:
        report['sources']={arm:driver.source_snapshot(path,sha) for arm,path,sha in [('A',parent,PARENT),('B',candidate,CANDIDATE)]}
        if capture(['git','rev-parse',CANDIDATE+'^'],candidate)!=PARENT:
            raise RuntimeError('Candidate ancestry changed')
        if capture(['git','diff','--name-only',PARENT,CANDIDATE],candidate).splitlines()!=[HEADER]:
            raise RuntimeError('Unexpected candidate changes')
        if digest(candidate/HEADER)!=HEADER_SHA256: raise RuntimeError('Generated header mismatch')
        (out/'scalar-header.h').write_text(baseline)
        shutil.copy2(candidate/HEADER,out/'candidate-header.h')
        report['compiler']=capture(['xcrun','clang++','--version'])
        report['xcode']=capture(['xcodebuild','-version'])
        if 'Apple clang version 17.0.0' not in report['compiler'] or not report['xcode'].startswith('Xcode 16.4'):
            raise RuntimeError('Pinned AppleClang 17 / Xcode 16.4 unavailable')
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
        report['compiler_commands_match']=verify_compile_commands(out,report)
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
            evidence['frozen_bits_and_latency']=contracts_present(log) and receipt['returncode']==0
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
        report['cab_contracts_pass']=all(gates_pass(report['gates'][arm]) for arm in ('A','B'))
        report['deadline_nonincrease']=all(
            r['misses']['B']['deadline_misses']<=r['misses']['A']['deadline_misses']
            and r['misses']['B']['cpu_over_budget']<=r['misses']['A']['cpu_over_budget']
            for r in analysis['rows'] if r['mode']=='control' and r['phase']=='forced_six_slot_publication')
        report['review_eligible']=report['complete'] and report['performance_demonstrated'] and report['cab_contracts_pass'] and report['deadline_nonincrease']
        report['integration_eligible']=False  # independent replication/review still required
        report['environment_after']=driver.environment_snapshot(out/'environment-after')
        for source,sha in [(parent,PARENT),(candidate,CANDIDATE)]: driver.source_snapshot(source,sha)
        lines=['# Independent scalar memory comparison','',f'Parent `{PARENT}`; candidate `{CANDIDATE}`.',
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

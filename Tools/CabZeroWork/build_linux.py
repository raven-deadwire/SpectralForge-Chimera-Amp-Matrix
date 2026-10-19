"""Portable GCC CAB build, independent of unavailable GUI/CMake dependencies.

This is not the official CMake/CTest or macOS build. All variants use identical
JUCE objects and flags. Measurements must start only after this script exits.
"""
import argparse, concurrent.futures, hashlib, json, os, platform, shutil, subprocess
from pathlib import Path
from generate import generate, BASE

ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--juce',type=Path,required=True)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--sanitizer',action='store_true')
    a=p.parse_args(); b=a.build.resolve(); j=a.juce.resolve(); b.mkdir(parents=True,exist_ok=True)
    if subprocess.check_output(['git','-C',str(j),'rev-parse','HEAD'],text=True).strip()!='d6181bde38d858c283c3b7bf699ce6340c050b5d':
        raise RuntimeError('JUCE must be exact 8.0.8 commit')
    source=(ROOT/'Source/ModeledCabConvolution.h').read_text()
    if hashlib.sha256(source.encode()).hexdigest()!='b35dff80ddc1de23061666ab7e7bca6e854cbb9d8da05aad09cb888dfc089f33':
        raise RuntimeError('unexpected PR105 baseline')
    flags=['-std=c++20','-O3','-DNDEBUG','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1',
           '-DJUCE_STANDALONE_APPLICATION=1','-DJUCE_USE_CURL=0','-DJUCE_WEB_BROWSER=0',
           '-DJUCE_USE_FLAC=0','-DJUCE_USE_OGGVORBIS=0','-pthread','-I'+str(j/'modules')]
    if a.sanitizer: flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    commands=[]
    def run(cmd,name):
        commands.append(cmd)
        with (b/(name+'.log')).open('w') as out:
            result=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT)
        if result.returncode: raise RuntimeError(f'{name} failed; see {b/name}.log')
    modules=['juce_core','juce_events','juce_data_structures','juce_audio_basics','juce_audio_formats','juce_dsp']
    objects=[str(b/(m+'.o')) for m in modules]
    tasks=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        for m,obj in zip(modules,objects):
            if not Path(obj).exists(): tasks.append(pool.submit(run,['g++',*flags,'-c',str(j/'modules'/m/(m+'.cpp')),'-o',obj],m))
        # JUCE 8 keeps compilation timestamp in a separate translation unit.
        stamp=j/'modules/juce_core/juce_core_CompilationTime.cpp'
        if stamp.exists():
            obj=str(b/'juce_stamp.o');objects.append(obj)
            if not Path(obj).exists(): tasks.append(pool.submit(run,['g++',*flags,'-c',str(stamp),'-o',obj],'juce_stamp'))
        for task in tasks: task.result()
    data_h='#pragma once\nnamespace ChimeraIRData {\n'
    data_cpp='#include "ChimeraIRData.h"\nnamespace ChimeraIRData {\n'
    for name in ['guitar_v30_sm57','guitar_jensen_sm57']:
        raw=(ROOT/'Assets/IRs'/(name+'.wav')).read_bytes(); symbol=name+'_wav'
        data_h+=f'extern const char* {symbol}; constexpr int {symbol}Size={len(raw)};\n'
        data_cpp+='static const unsigned char '+symbol+'_bytes[]={'+','.join(map(str,raw))+'};\n'
        data_cpp+=f'const char* {symbol}=reinterpret_cast<const char*>({symbol}_bytes);\n'
    (b/'ChimeraIRData.h').write_text(data_h+'}\n');(b/'ChimeraIRData.cpp').write_text(data_cpp+'}\n')
    run(['g++',*flags,'-c',str(b/'ChimeraIRData.cpp'),'-o',str(b/'irs.o')],'irs');objects.append(str(b/'irs.o'))
    variants={}
    for candidate in ['baseline','empty_partition','zero_history']:
        v=b/candidate;v.mkdir(exist_ok=True)
        shutil.copytree(ROOT/'Source',v/'Source',dirs_exist_ok=True)
        header=generate(source,candidate);(v/'Source/ModeledCabConvolution.h').write_text(header)
        (v/'Source/BaselineConvolution.h').write_text(source.replace('ModeledCabConvolution','BaselineConvolution'))
        inc=['-I'+str(v/'Source'),'-I'+str(ROOT/'Tests'),'-I'+str(b)]
        lib=str(v/'IRLibrary.o')
        run(['g++',*flags,*inc,'-c',str(v/'Source/IRLibrary.cpp'),'-o',lib],candidate+'-IRLibrary')
        targets={
            'parity':([ROOT/'Tools/CabZeroWork/Parity.cpp'],[]),
            'realtime':([ROOT/'Tests/CabRealtimeRegressionTests.cpp'],[]),
            'integration':([ROOT/'Tests/OriginalCabIntegrationTests.cpp'],[lib]),
            'probe':([ROOT/'Tools/CabZeroWork/Probe.cpp'],[]),
        }
        for name,(sources,extra) in targets.items():
            run(['g++',*flags,*inc,*map(str,sources),*objects,*extra,'-ldl','-lrt','-o',str(v/name)],candidate+'-'+name)
        variants[candidate]={'header_sha256':hashlib.sha256(header.encode()).hexdigest(),
            'binaries':{n:hashlib.sha256((v/n).read_bytes()).hexdigest() for n in targets}}
    metadata={'baseline':BASE,'juce':'d6181bde38d858c283c3b7bf699ce6340c050b5d',
        'compiler':subprocess.check_output(['g++','--version'],text=True),'platform':platform.platform(),
        'flags':flags,'variants':variants,'commands':commands,'sanitizer':a.sanitizer,
        'build_kind':'portable manual CAB-only; not official CMake/CTest'}
    (b/'build.json').write_text(json.dumps(metadata,indent=2)+'\n')
if __name__=='__main__': main()

#!/usr/bin/env python3
"""Compile actual production core, research core and untouched regressions."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--juce',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[2];a.juce=a.juce.resolve();a.out=a.out.resolve()
a.out.mkdir(parents=True,exist_ok=True)
pin='d6181bde38d858c283c3b7bf699ce6340c050b5d'
actual=subprocess.check_output(['git','-C',str(a.juce),'rev-parse','HEAD'],text=True).strip()
if actual!=pin:raise SystemExit('JUCE 8.0.8 pin mismatch')
if subprocess.check_output(['git','-C',str(a.juce),'diff','HEAD','--','modules']):raise SystemExit('JUCE modules modified')
flags=['-std=c++20','-O2','-DNDEBUG','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1','-DJUCE_USE_CURL=0','-DJUCE_WEB_BROWSER=0','-DJUCE_USE_FLAC=0','-DJUCE_USE_OGGVORBIS=0','-pthread','-I'+str(a.juce/'modules'),'-I'+str(root/'Source')]
modules=['juce_core','juce_audio_basics','juce_audio_formats','juce_dsp']
sources=[a.juce/'modules'/m/(m+'.cpp') for m in modules]+[a.juce/'modules/juce_core/juce_core_CompilationTime.cpp']
commands=[]
def compile_module(src):
    obj=a.out/(src.stem+'.o')
    cmd=['g++',*flags,'-c',str(src),'-o',str(obj)];commands.append(cmd)
    subprocess.run(cmd,check=True);return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:objects=list(pool.map(compile_module,sources))
for name,src in [('Probe',Path(__file__).with_name('Probe.cpp')),('PredictiveTests',Path(__file__).with_name('Tests.cpp')),*[(n,root/'Tests'/(n+'.cpp')) for n in ['PitchRegressionTests','StabilityTests','GateTests']]]:
    cmd=['g++',*flags,str(src),*map(str,objects),'-ldl','-lrt','-lz','-o',str(a.out/name)];commands.append(cmd);subprocess.run(cmd,check=True)
source_files=[*Path(__file__).parent.glob('*.*'),root/'Source/PolyPitch.h',root/'Source/GlobalDSP.h',*[root/'Tests'/(n+'.cpp') for n in ['PitchRegressionTests','StabilityTests','GateTests']]]
manifest={'base_sha':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'juce_sha':actual,'compiler':subprocess.check_output(['g++','--version'],text=True),'commands':commands,'sources':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in source_files}}
(a.out/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Fresh native build complete: probe, research tests and 3 production regressions')

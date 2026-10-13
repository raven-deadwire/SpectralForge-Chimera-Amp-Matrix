#!/usr/bin/env python3
"""Build the offline probes without GUI/system package installation."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from prepare_state_probe import prepare

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def build(a):
    here=Path(__file__).resolve().parent
    for path,expected in [(a.core,'1f42f88535884450104b8711d7595019afa0495b'),
                          (a.juce,'d6181bde38d858c283c3b7bf699ce6340c050b5d')]:
        actual=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
        if actual!=expected:raise ValueError('Dependency pin mismatch')
    a.out.mkdir(parents=True,exist_ok=False)
    overlay=a.out/'teacher-overlay'
    meta=prepare(a.repo,overlay)
    parts=['activations','conv1d','container','convnet','dsp','get_dsp','lstm','ring_buffer','util',
           'wavenet/a2_fast','wavenet/model','wavenet/slimmable']
    core_sources=[a.core/'NAM'/f'{p}.cpp' for p in parts]
    core_cmd=['g++','-std=c++17','-O3','-DNAM_ENABLE_A2_FAST=1','-DEIGEN_MPL2_ONLY=1','-DEIGEN_NO_DEBUG=1',
              '-I'+str(a.core),'-I'+str(a.core/'Dependencies/eigen'),'-I'+str(a.core/'Dependencies/nlohmann'),
              str(here/'CoreMemoryProbe.cpp'),*map(str,core_sources),'-o',str(a.out/'core-memory-probe')]
    modules=['juce_dsp','juce_audio_formats','juce_audio_basics','juce_events','juce_core']
    native_sources=[a.juce/'modules'/m/(m+'.cpp') for m in modules]
    native_sources.append(a.juce/'modules/juce_core/juce_core_CompilationTime.cpp')
    native_cmd=['g++','-std=c++20','-O2','-DNDEBUG','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1',
                '-DJUCE_STANDALONE_APPLICATION=1','-DJUCE_USE_CURL=0','-DJUCE_WEB_BROWSER=0',
                *['-DJUCE_MODULE_AVAILABLE_'+m+'=1' for m in modules],
                '-I'+str(a.juce/'modules'),'-I'+str(overlay/'Source'),
                str(overlay/'Tools/Nam/RenderNativeAmp.cpp'),*map(str,native_sources),
                '-pthread','-ldl','-lrt','-o',str(a.out/'teacher-state-probe')]
    meta['commands']=[core_cmd,native_cmd]
    meta['compiler']=subprocess.check_output(['g++','--version'],text=True).splitlines()[0]
    for label,command in [('core',core_cmd),('teacher',native_cmd)]:
        with (a.out/(label+'-build.log')).open('w') as log:
            subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT)
    meta['binaries']={name:digest(a.out/name) for name in ['core-memory-probe','teacher-state-probe']}
    meta['source_sha256']={str(p):digest(p) for p in [*core_sources,*native_sources,here/'CoreMemoryProbe.cpp',here/'prepare_state_probe.py']}
    (a.out/'build.json').write_text(json.dumps(meta,indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ['repo','core','juce','out']:p.add_argument('--'+n,type=Path,required=True)
    build(p.parse_args())

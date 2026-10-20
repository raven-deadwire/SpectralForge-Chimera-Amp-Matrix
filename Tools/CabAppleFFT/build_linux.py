"""Negative-control build only: Linux cannot exercise Apple vDSP."""
import argparse, concurrent.futures, json, subprocess
from pathlib import Path
from generate import ROOT, generate
p=argparse.ArgumentParser();p.add_argument('--juce',type=Path,required=True);p.add_argument('--build',type=Path,required=True)
a=p.parse_args();b=a.build.resolve();generate(a.juce,b/'overlay');j=b/'overlay/JUCE'
flags=['-std=c++20','-O3','-DNDEBUG','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1','-DJUCE_STANDALONE_APPLICATION=1',
       '-DJUCE_USE_CURL=0','-DJUCE_WEB_BROWSER=0','-DJUCE_USE_FLAC=0','-DJUCE_USE_OGGVORBIS=0','-pthread','-I'+str(j/'modules')]
commands=[]
def run(cmd,name):
    commands.append(cmd)
    with (b/(name+'.log')).open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError(name+' failed')
modules=['juce_core','juce_events','juce_data_structures','juce_audio_basics','juce_audio_formats','juce_dsp','juce_cryptography']
objects=[str(b/(m+'.o')) for m in modules]+[str(b/'stamp.o'),str(b/'irs.o')]
# Fresh build only: do not silently reuse stale objects or flags.
if any(Path(o).exists() for o in objects):raise RuntimeError('use a fresh build directory')
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    jobs=[pool.submit(run,['g++',*flags,'-c',str(j/'modules'/m/(m+'.cpp')),'-o',str(b/(m+'.o'))],m) for m in modules]
    for job in jobs:job.result()
run(['g++',*flags,'-c',str(j/'modules/juce_core/juce_core_CompilationTime.cpp'),'-o',str(b/'stamp.o')],'stamp')
h='#pragma once\nnamespace ChimeraIRData {\n';c='#include "ChimeraIRData.h"\nnamespace ChimeraIRData {\n'
for name in ['guitar_v30_sm57','guitar_jensen_sm57']:
    raw=(ROOT/'Assets/IRs'/(name+'.wav')).read_bytes();symbol=name+'_wav'
    h+=f'extern const char* {symbol}; constexpr int {symbol}Size={len(raw)};\n'
    c+='static const unsigned char '+symbol+'_bytes[]={'+','.join(map(str,raw))+'};\n'
    c+=f'const char* {symbol}=reinterpret_cast<const char*>({symbol}_bytes);\n'
(b/'ChimeraIRData.h').write_text(h+'}\n');(b/'ChimeraIRData.cpp').write_text(c+'}\n')
run(['g++',*flags,'-c',str(b/'ChimeraIRData.cpp'),'-o',str(b/'irs.o')],'irs')
(b/'bin').mkdir(exist_ok=True)
for v in ['baseline','candidate']:
    source=b/'overlay'/v/'Source';inc=['-I'+str(source),'-I'+str(ROOT/'Tests'),'-I'+str(b)]
    lib=str(b/(v+'-IRLibrary.o'));run(['g++',*flags,*inc,'-c',str(source/'IRLibrary.cpp'),'-o',lib],v+'-IRLibrary')
    targets={'fft-check':'Tools/CabAppleFFT/FFTCheck.cpp','fft-probe':'Tools/CabAppleFFT/FFTProbe.cpp',
             'parity':'Tools/CabZeroWork/Parity.cpp','realtime':'Tests/CabRealtimeRegressionTests.cpp',
             'integration':'Tests/OriginalCabIntegrationTests.cpp','ir-state':'Tests/IRLibraryTests.cpp','probe':'Tools/CabZeroWork/Probe.cpp'}
    for name,path in targets.items():
        extra=[lib] if name in ['integration','ir-state'] else []
        run(['g++',*flags,*inc,'-DCAB_APPLE_CANDIDATE='+str(int(v=='candidate')),str(ROOT/path),*objects,*extra,'-ldl','-lrt',
             '-o',str(b/'bin'/('apple-'+v+'-'+name))],v+'-'+name)
(b/'manual-build.json').write_text(json.dumps({'commands':commands,'flags':flags,'macos_execution':False,'cmake_ctest':False},indent=2)+'\n')

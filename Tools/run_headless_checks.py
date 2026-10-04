#!/usr/bin/env python3
"""Build the actual JUCE DSP checks without desktop development packages.

This is a Linux fallback, not a Windows/plugin/DAW certification. JUCE must
already be available at --juce (the same pinned 8.0.8 used by CMake).
"""
import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import subprocess
import sys
import time

p = argparse.ArgumentParser()
p.add_argument('--juce', required=True, type=pathlib.Path)
p.add_argument('--output', required=True, type=pathlib.Path)
p.add_argument('--source', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
a = p.parse_args()
src, out, juce = a.source.resolve(), a.output.resolve(), a.juce.resolve()
out.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(src / 'Tools'))
import chimera_version
build_identity = chimera_version.identity(src)
version_header = (src / 'cmake/ChimeraBuildVersion.h.in').read_text()
version_header = version_header.replace('@PROJECT_VERSION@', build_identity['product_version'])
version_header = version_header.replace('@CHIMERA_BUILD_REVISION@', build_identity['build_id'])
(out / 'ChimeraBuildVersion.h').write_text(version_header)
flags = ['g++', '-std=c++20', '-O2', '-pthread', '-DNDEBUG', '-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1',
         '-DJUCE_STANDALONE_APPLICATION=1', '-DJUCE_USE_CURL=0', '-DJUCE_WEB_BROWSER=0',
         '-I'+str(juce/'modules'), '-I'+str(src/'Source'), '-I'+str(out)]
records = []
def git_output(*args):
    return subprocess.check_output(['git', *args], cwd=src, text=True).strip()
source_files = [path for folder in ['Source','Tests','Tools'] for path in sorted((src/folder).rglob('*')) if path.is_file() and '__pycache__' not in path.parts]
source_files += [src / 'VERSION', src / 'cmake/ChimeraBuildVersion.h.in']
provenance = {'commit': git_output('rev-parse','HEAD'), 'tree':git_output('rev-parse','HEAD^{tree}'),
              'working_tree_clean_at_start':not git_output('status','--porcelain'),
              'source_sha256':{str(path.relative_to(src)):hashlib.sha256(path.read_bytes()).hexdigest() for path in source_files},
              'juce_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=juce,text=True).strip()}

def run(label, argv):
    started = time.time()
    result = subprocess.run(argv, cwd=src, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log = out/(label+'.log')
    log.write_bytes(result.stdout)
    record = dict(label=label, argv=argv, started=started, duration_seconds=time.time()-started,
                  exit_code=result.returncode, log=str(log), sha256=hashlib.sha256(result.stdout).hexdigest())
    records.append(record)
    print(label, 'exit', result.returncode, flush=True)
    if result.returncode:
        print(result.stdout.decode(errors='replace')[-6000:], flush=True)
        raise RuntimeError(label+' failed')

header = ['#pragma once', 'namespace ChimeraIRData {']
data = ['#include "ChimeraIRData.h"', 'namespace ChimeraIRData {']
for filename in ['guitar_v30_sm57.wav', 'guitar_jensen_sm57.wav']:
    name = filename.replace('.', '_')
    raw = (src/'Assets'/'IRs'/filename).read_bytes()
    header += [f'extern const char* {name};', f'extern const int {name}Size;']
    data += [f'static const unsigned char {name}_data[] = {{'+','.join(map(str,raw))+'};',
             f'const char* {name}=reinterpret_cast<const char*>({name}_data);',f'const int {name}Size={len(raw)};']
(out/'ChimeraIRData.h').write_text('\n'.join(header+['}']))
(out/'ChimeraIRData.cpp').write_text('\n'.join(data+['}']))
modules = ['juce_core', 'juce_events', 'juce_audio_basics', 'juce_audio_formats', 'juce_data_structures', 'juce_dsp', 'juce_cryptography']
objects = [out/(name+'.o') for name in modules]
def compile_module(name):
    source = juce/'modules'/name/(name+'.cpp')
    obj = out/(name+'.o')
    key = hashlib.sha256(source.read_bytes()+json.dumps(flags).encode()).hexdigest()
    marker = out/(name+'.key')
    if obj.exists() and marker.exists() and marker.read_text()==key:
        return
    run('compile-'+name, flags+['-c', str(source), '-o', str(obj)])
    marker.write_text(key)

try:
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(compile_module, modules))
    # JUCE's core module keeps its timestamp in a second translation unit.
    stamp = juce/'modules/juce_core/juce_core_CompilationTime.cpp'
    run('compile-time', flags+['-c',str(stamp),'-o',str(out/'compile-time.o')])
    objects.append(out/'compile-time.o')
    targets = [('ChimeraTests', ['Tests/ChimeraTests.cpp','Source/IRLibrary.cpp',str(out/'ChimeraIRData.cpp')]),
               ('ChimeraReleaseTests',['Tests/ReleaseSupportTests.cpp','Source/ReleaseSupport.cpp'])]
    if (src/'Tests/StabilityTests.cpp').exists():
        targets.append(('ChimeraStabilityTests',['Tests/StabilityTests.cpp']))
    if (src/'Tests/PedalBoardTests.cpp').exists():
        targets.append(('ChimeraPedalBoardTests',['Tests/PedalBoardTests.cpp']))
    if (src/'Tests/IRCollectionTests.cpp').exists():
        targets.append(('ChimeraIRCollectionTests',['Tests/IRCollectionTests.cpp']))
    if (src/'Tests/NewAmpTests.cpp').exists():
        targets.append(('ChimeraNewAmpTests',['Tests/NewAmpTests.cpp']))
    if (src/'Tests/PedalOctaverTests.cpp').exists():
        targets.append(('ChimeraPedalOctaverTests',['Tests/PedalOctaverTests.cpp']))
    for native in ['AmpNative','PostNative']:
        if (src/('Tests/'+native+'Tests.cpp')).exists():
            targets.append(('Chimera'+native+'Tests',['Tests/'+native+'Tests.cpp']))
    for name, sources in targets:
        run('link-'+name,flags+sources+list(map(str,objects))+['-ldl','-lrt','-lz','-o',str(out/name)])
        run('test-'+name,[str(out/name)])
finally:
    (out/'execution.json').write_text(json.dumps({'scope':'linux-headless-dsp-only', 'provenance':provenance, 'records':records},indent=2)+'\n')

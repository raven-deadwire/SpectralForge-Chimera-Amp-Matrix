#!/usr/bin/env python3
"""Small native research build; no GUI SDK or CMake required. JUCE 8.0.8 only."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--juce', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
a.juce = a.juce.resolve()
a.out = a.out.resolve()
a.out.mkdir(parents=True, exist_ok=True)
sha = subprocess.check_output(['git', '-C', str(a.juce), 'rev-parse', 'HEAD'], text=True).strip()
assert sha == 'd6181bde38d858c283c3b7bf699ce6340c050b5d', 'Requires pinned JUCE 8.0.8'
flags = ['-std=c++20', '-O2', '-DNDEBUG', '-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1',
         '-DJUCE_USE_CURL=0', '-DJUCE_WEB_BROWSER=0', '-DJUCE_USE_FLAC=0',
         '-DJUCE_USE_OGGVORBIS=0', '-pthread', '-I'+str(a.juce/'modules'),
         '-I'+str(root/'Source')]
if a.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
modules = ['juce_core', 'juce_audio_basics', 'juce_audio_formats', 'juce_dsp']
sources = [a.juce/'modules'/m/(m+'.cpp') for m in modules]
sources += [a.juce/'modules/juce_core/juce_core_CompilationTime.cpp']
commands = []
def compile_one(src):
    obj = a.out/(src.stem+'.o')
    key = hashlib.sha256(src.read_bytes()+json.dumps(flags).encode()).hexdigest()
    stamp = obj.with_suffix('.key')
    cmd = ['g++', *flags, '-c', str(src), '-o', str(obj)]
    commands.append(cmd)
    if not obj.exists() or not stamp.exists() or stamp.read_text() != key:
        subprocess.run(cmd, check=True)
        stamp.write_text(key)
    return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    objects = list(pool.map(compile_one, sources))
for name, src in [('TransposeProbe', Path(__file__).with_name('TransposeProbe.cpp')),
                  ('CorrelationTests', Path(__file__).with_name('CorrelationTests.cpp')),
                  *[(n, root/'Tests'/(n+'.cpp')) for n in ['PitchRegressionTests', 'StabilityTests', 'GateTests']]]:
    cmd = ['g++', *flags, str(src), *map(str, objects), '-ldl', '-lrt', '-lz', '-o', str(a.out/name)]
    commands.append(cmd)
    subprocess.run(cmd, check=True)
manifest = {'juce_sha': sha, 'compiler': subprocess.check_output(['g++', '--version'], text=True),
            'commands': commands}
(a.out/'build.json').write_text(json.dumps(manifest, indent=2))
print('Built research probe and four native regression executables')

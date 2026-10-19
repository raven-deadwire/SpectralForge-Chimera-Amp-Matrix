"""IR restore and Source-instrumented sanitizers; run before timed experiments.

JUCE dependency objects stay Release/uninstrumented. Sanitizer coverage is the
candidate header/harness, not an assertion of whole-JUCE instrumentation.
"""
import argparse,json,subprocess,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--juce',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--existing-no-leaks',action='store_true')
a=p.parse_args();b=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
meta=json.loads((b/'build.json').read_text());flags=meta['flags'];commands=[];results=[]
def execute(cmd,name):
    commands.append(cmd)
    with (out/(name+'.log')).open('w') as f:r=subprocess.run(cmd,cwd=out,stdout=f,stderr=subprocess.STDOUT)
    results.append({'name':name,'returncode':r.returncode});print(name,r.returncode,flush=True)
    (out/('checks-no-leaks.json' if a.existing_no_leaks else 'checks.json')).write_text(json.dumps({'commands':commands,'results':results,'juce_sanitized':False,'leak_detection':not a.existing_no_leaks},indent=2)+'\n')
    return r.returncode
if a.existing_no_leaks:
    # Explicit separate run for hosts without LSan /proc visibility; original
    # fatal logs remain intact. This does not verify leaks.
    os.environ['ASAN_OPTIONS']='detect_leaks=0'
    for candidate in ['empty_partition','zero_history']:
        for target in ['parity','realtime']:
            execute([str(b/candidate/(target+'-sanitize'))],candidate+'-'+target+'-sanitize-no-leaks')
    raise SystemExit(int(any(r['returncode'] for r in results)))
crypto=b/'juce_cryptography.o'
if execute(['g++',*flags,'-c',str(a.juce.resolve()/'modules/juce_cryptography/juce_cryptography.cpp'),'-o',str(crypto)],'compile-crypto'):raise SystemExit(1)
objects=[str(p) for p in sorted(b.glob('*.o'))]
for candidate in ['baseline','empty_partition','zero_history']:
    v=b/candidate;inc=['-I'+str(v/'Source'),'-I'+str(ROOT/'Tests'),'-I'+str(b)]
    command=['g++',*flags,*inc,str(ROOT/'Tests/IRLibraryTests.cpp'),str(v/'IRLibrary.o'),*objects,'-ldl','-lrt','-o',str(v/'ir-state')]
    if execute(command,candidate+'-compile-ir')==0:execute([str(v/'ir-state')],candidate+'-ir-state')
    if candidate=='baseline':continue
    for target,source in [('parity','Tools/CabZeroWork/Parity.cpp'),('realtime','Tests/CabRealtimeRegressionTests.cpp')]:
        binary=v/(target+'-sanitize')
        cmd=['g++',*flags,'-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*inc,str(ROOT/source),*objects,'-ldl','-lrt','-o',str(binary)]
        if execute(cmd,candidate+'-compile-'+target+'-sanitize')==0:execute([str(binary)],candidate+'-'+target+'-sanitize')
if any(r['returncode'] for r in results):raise SystemExit(1)

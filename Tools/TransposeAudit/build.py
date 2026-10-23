#!/usr/bin/env python3
"""Build pinned original DSPs together; namespace renaming only for snapshots."""
import argparse, hashlib, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--juce',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];src=Path(__file__).resolve().parent
a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);gen=a.out/'generated';gen.mkdir(exist_ok=True)
refs={'public':'3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58','pr125':'1ef032c441b06cac5c44076e8c231323f5e63749','pr128':'9d28b06c3cb6498724d6618d08bebac1771f63eb'}
manifest={'refs':refs,'snapshots':{}}
def get(ref,path):
 b=subprocess.check_output(['git','show',refs[ref]+':'+path],cwd=root);manifest['snapshots'][ref+':'+path]=hashlib.sha256(b).hexdigest();return b.decode()
for ref,ns in [('public','sfpublic'),('pr125','sf125')]:
 (gen/(ref+'.h')).write_text(get(ref,'Source/PolyPitch.h').replace('namespace spectralforge {','namespace '+ns+' {'))
global125=get('pr125','Source/GlobalDSP.h');wrapper=global125[global125.index('class Transposer {'):global125.index('// YIN on a decimated')]
(gen/'wrapper125.h').write_text('#pragma once\n#include "pr125.h"\nnamespace sf125 {\n'+wrapper+'\n}\n')
probe=get('pr128','Tools/TransposeNext/TransposeProbe.cpp')
probe=probe.replace('#include "PolyPitch.h"','#include "PolyPitch.h"\n#include "public.h"\n#include "pr125.h"\n#include "wrapper125.h"')
probe=probe.replace('if(name!="baseline" &&', 'if(name!="tight125" && name!="classic125" && name!="wrapper125" && name!="baseline128" && name!="baseline" &&',1)
probe=probe.replace('spectralforge::PolyPitch baseline;', 'sfpublic::PolyPitch baseline; sf125::PolyPitch candidate; spectralforge::PolyPitch baseline128; sf125::Transposer wrapper125;')
probe=probe.replace('else if(name=="relative"){relative.prepare', '''else if(name=="tight125" || name=="classic125"){candidate.prepare(spec,0,name=="tight125"?int(rate*.010)/8*8:0);candidate.setSemitones(shift);nominal=candidate.latency();}
        else if(name=="baseline128"){baseline128.prepare(spec);baseline128.setSemitones(shift);nominal=baseline128.latency();}
        else if(name=="wrapper125"){wrapper125.prepare(spec);wrapper125.setMode(2);nominal=wrapper125.latency();}
        else if(name=="relative"){relative.prepare''')
probe=probe.replace('else if(name=="relative")relative.process', '''else if(name=="tight125" || name=="classic125")candidate.process(in,out);
            else if(name=="baseline128")baseline128.process(in,out);
            else if(name=="wrapper125"){out.makeCopyOf(in,true);wrapper125.process(out,true,shift);}
            else if(name=="relative")relative.process''')
(gen/'AuditProbe.cpp').write_text(probe)
base=json.loads((a.out/'build.json').read_text());flags=base['commands'][-1];start=flags.index('-std=c++20');end=next(i for i,v in enumerate(flags) if v.endswith('GateTests.cpp'))
common=flags[start:end]+['-I'+str(gen),'-I'+str(root/'Tools/TransposeNext'),'-I'+str(root/'Tests')]
objects=list(a.out.glob('*.o'));commands=[]
def compile(source,name,extra=[]):
 cmd=['g++',*extra,*common,str(source),*map(str,objects),'-ldl','-lrt','-lz','-o',str(a.out/name)];subprocess.run(cmd,check=True);commands.append(cmd)
compile(gen/'AuditProbe.cpp','AuditProbe')
ftz=probe.replace('size_t callback=0,misses=0;', 'juce::ScopedNoDenormals noDenormals; size_t callback=0,misses=0;')
(gen/'AuditProbeFTZ.cpp').write_text(ftz);compile(gen/'AuditProbeFTZ.cpp','AuditProbeFTZ')
# Exact #125 regression source/header, original namespace; existing frozen fixture.
testdir=gen/'pr125-test';testdir.mkdir(exist_ok=True)
(testdir/'PolyPitch.h').write_text(get('pr125','Source/PolyPitch.h'))
(testdir/'PitchRegressionTests.cpp').write_text(get('pr125','Tests/PitchRegressionTests.cpp'))
compile(testdir/'PitchRegressionTests.cpp','PitchRegressionTests125',['-I'+str(testdir)])
# Existing CorrelationTests covers old split, not the current LPC/retrigger options.
# Extend just its test adapter to select the frozen #128 options; no DSP edit.
reg=get('pr128','Tools/TransposeNext/CorrelationTests.cpp').replace('split=limit==-42;','split=limit==-42 || limit==-43;').replace('if(split)b.prepare(spec);','if(split)b.prepare(spec,limit==-43,limit==-43,limit==-43,limit==-43);').replace('10.,40.,-40.,-41.,-42.','-43.')
(gen/'CorrelationTestsRT.cpp').write_text(reg);compile(gen/'CorrelationTestsRT.cpp','CorrelationTestsRT')
manifest.update(juce_sha=base['juce_sha'],compiler=base['compiler'],commands=commands,namespace_change_only=True)
(a.out/'audit-build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Built AuditProbe, exact PR125 regression, extended RT regression',flush=True)

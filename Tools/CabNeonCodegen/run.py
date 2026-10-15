#!/usr/bin/env python3
"""Native instruction-level isolation; no integration/release approval."""
import argparse, hashlib, json, platform, re, shlex, statistics, subprocess
from pathlib import Path
PARENT='c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
NEON='29865abb4da3ab78592c970a263ee62af63fc673'
NAMES=['scalar','neon4','neon8','neon16','scalar_ic1','scalar_ic2']

def body(text):
 start=text.index('        int i=0;',text.index('    void accumulate('))
 end=text.index('\n    void sumPartitions(',start)
 return text[start:end].rsplit('\n    }',1)[0]

def candidates(a,b):
 if b.count('for(;i+4<=bins;i+=4)')!=2:raise ValueError('Unexpected NEON source')
 marker='        for(;i+4<=bins;i+=4) {'
 pos=b.index(marker,b.index('#elif defined(__APPLE__)'))
 def unroll(n):return b[:pos]+f'        #pragma clang loop unroll_count({n})\n'+b[pos:]
 def interleave(n):return a.replace('        for(;i<bins;++i) {',f'        #pragma clang loop vectorize_width(4) interleave_count({n})\n        for(;i<bins;++i) {{')
 return [a,b,unroll(2),unroll(4),interleave(1),interleave(2)]

def run(cmd,out):
 p=subprocess.run(list(map(str,cmd)),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 out.write_text(p.stdout);print(p.stdout,flush=True)
 if p.returncode:raise RuntimeError(f'{out}: exit {p.returncode}')
 return p.stdout

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 if platform.system()!='Darwin' or platform.machine()!='arm64':raise RuntimeError('Native Apple Silicon required')
 root=Path(__file__).resolve().parents[2]
 env={k:subprocess.check_output(v,text=True).strip() for k,v in {'compiler':['xcrun','clang++','--version'],'hardware':['sysctl','-n','machdep.cpu.brand_string'],'os':['sw_vers'],'xcode':['xcodebuild','-version']}.items()}
 (out/'environment.json').write_text(json.dumps(env,indent=2))
 src=[]
 for name,sha in [('parent',PARENT),('neon',NEON)]:
  t=subprocess.check_output(['git','show',sha+':Source/ModeledCabConvolution.h'],cwd=root,text=True)
  (out/(name+'.h')).write_text(t);src.append(t)
 bodies=candidates(*map(body,src))
 kernels='#include <arm_neon.h>\n'
 for name,b in zip(NAMES,bodies):
  kernels+='extern "C" __attribute__((noinline)) void '+name+'(const float* __restrict input,const float* __restrict impulse,float* __restrict output,int bins) noexcept {\n'+b+'\n}\n'
 (out/'kernels.cpp').write_text(kernels)
 # Exactly the O3/gnu++20/arm64/min-12 flags captured in PR74, without fast-math or CPU overrides.
 flags=['-O3','-DNDEBUG','-std=gnu++20','-arch','arm64','-mmacosx-version-min=12.0']
 cxx=['xcrun','clang++',*flags]
 commands=[[*cxx,'-S','-gline-tables-only','-Rpass=loop-vectorize','-Rpass-missed=loop-vectorize','-Rpass=loop-unroll',out/'kernels.cpp','-o',out/'kernels.s'],[*cxx,'-c',out/'kernels.cpp','-o',out/'kernels.o'],[*cxx,root/'Tools/CabNeonCodegen/probe.cpp',out/'kernels.o','-o',out/'probe']]
 (out/'commands.json').write_text(json.dumps([[str(s) for s in c] for c in commands],indent=2))
 for i,cmd in enumerate(commands):run(cmd,out/f'compile-{i}.log')
 run([out/'probe'],out/'bits.log')
 # Source patches, isolated in diagnostics; production source in this branch is untouched.
 for name,b in zip(NAMES,bodies):
  (out/(name+'-ModeledCabConvolution.h')).write_text(src[1].replace(body(src[1]),b))
 rows=[]
 # Working sets cover small hot loops and realistic partition streams. A/A shares binary/function.
 for n,parts in [(65,64),(128,64),(129,32),(256,32),(513,8),(1024,8)]:
  for candidate in range(1,6):
   for round_ in range(6):
    order='ABBA' if round_%2==0 else 'BAAB'
    for exp in (('AA','AB') if round_%2==0 else ('AB','AA')):
     for slot,arm in enumerate(order):
      f=candidate if exp=='AB' and arm=='B' else 0
      line=subprocess.check_output([str(out/'probe'),str(f),str(n),str(parts)],text=True).strip()
      fields=line.split(',')
      row=dict(bins=n,partitions=parts,candidate=NAMES[candidate],round=round_,experiment=exp,slot=slot,arm=arm,actual=NAMES[f],ns=float(fields[3]),hash=fields[4])
      rows.append(row)
  print(f'kernel grid bins={n} parts={parts} complete',flush=True)
 (out/'raw.json').write_text(json.dumps(rows,indent=2))
 import importlib.util
 spec=importlib.util.spec_from_file_location('ab',root/'Tools/CabNeonAB/run.py');ab=importlib.util.module_from_spec(spec);spec.loader.exec_module(ab)
 summary=[]
 for n,parts in [(65,64),(128,64),(129,32),(256,32),(513,8),(1024,8)]:
  for name in NAMES[1:]:
   effects={};pop=[r for r in rows if r['bins']==n and r['candidate']==name]
   if len({r['hash'] for r in pop})!=1:raise RuntimeError('benchmark bit mismatch')
   for exp in ('AB','AA'):
    effects[exp]=[]
    for rnd in range(6):
     p=[r for r in pop if r['round']==rnd and r['experiment']==exp]
     import math
     means={arm:statistics.mean(math.log(r['ns']) for r in p if r['arm']==arm) for arm in 'AB'}
     effects[exp].append(100*(1-math.exp(means['B']-means['A'])))
   summary.append(dict(bins=n,partitions=parts,candidate=name,**ab.comparison(effects['AB'],effects['AA'])))
 report=dict(complete=True,scope='isolated extracted kernel; not full callback acceptance',integration_eligible=False,release_approved=False,rows=summary)
 (out/'summary.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
 (out/'manifest.json').write_text(json.dumps({str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file()},indent=2))
if __name__=='__main__':main()

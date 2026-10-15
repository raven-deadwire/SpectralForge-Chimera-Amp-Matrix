#!/usr/bin/env python3
"""Inspect real inlined product code and run original gates for a bounded-loop candidate.
No acceptance thresholds/test code/latency geometry are modified.
"""
import argparse, difflib, gzip, hashlib, importlib.util, json, platform, shlex, shutil, subprocess
from pathlib import Path
from run import PARENT,NEON

def load(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--kernels',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
 if platform.system()!='Darwin' or platform.machine()!='arm64':raise RuntimeError('Native Apple Silicon required')
 root=Path(__file__).resolve().parents[2];out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 source=out/'product-source';build=out/'build';kernels=args.kernels.resolve()
 subprocess.run(['git','worktree','add','--detach',str(source),PARENT],cwd=root,check=True)
 driver=load('driver',source/'Tools/run_cab_realtime_ci.py');ab=load('ab',root/'Tools/CabNeonAB/run.py')
 report=dict(complete=False,release_approved=False,integration_eligible=False,performance_demonstrated=False,parent=PARENT,neon=NEON,candidate='bound8',commands=[],sources={})
 def write(): (out/'report.json').write_text(json.dumps(report,indent=2))
 def run(cmd,log,cwd=source,required=True):
  print('RUN',log,flush=True)
  rec=driver.run_logged(cmd,cwd,out/log,timeout=1800,measured=False);report['commands'].append(rec);write()
  if required and rec['returncode']:raise RuntimeError(str(log)+' failed')
  return rec
 header=source/'Source/ModeledCabConvolution.h'
 exact_a=header.read_bytes();exact_b=subprocess.check_output(['git','show',NEON+':Source/ModeledCabConvolution.h'],cwd=root)
 if exact_a!=(kernels/'parent.h').read_bytes() or exact_b!=(kernels/'neon.h').read_bytes():raise RuntimeError('source mismatch')
 probe=out/'probe.cpp';probe.write_text(ab.generate_probe(source/'Tests/OriginalCabIntegrationTests.cpp'))
 try:
  run(['cmake','-S',root/'Tools/CabNeonAB','-B',build,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0',f'-DCHIMERA_PRODUCT_SOURCE={source}',f'-DCHIMERA_PROBE_SOURCE={probe}'],'configure.log')
  if subprocess.check_output(['git','rev-parse','HEAD'],cwd=build/'_deps/juce-src',text=True).strip()!=ab.JUCE:raise RuntimeError('JUCE changed')
  run(['cmake','--build',build,'--target','ChimeraIRs','--parallel','2'],'binary-data.log')
  commands=json.loads((build/'compile_commands.json').read_text());shutil.copy2(build/'compile_commands.json',out/'compile_commands.json')
  entry=next(e for e in commands if e['file'].endswith('/Tests/OriginalCabIntegrationTests.cpp'))
  # Actual integration TU, with the captured production compiler command. Only emission/remarks differ.
  original=shlex.split(entry['command']);flags=[];i=0
  while i<len(original):
   if original[i]=='-o':i+=2;continue
   if original[i]=='-c':i+=1;continue
   flags.append(original[i]);i+=1
  for name,contents in [('A',exact_a),('B',exact_b),('C4',(kernels/'bound4-ModeledCabConvolution.h').read_bytes()),('C8',(kernels/'bound8-ModeledCabConvolution.h').read_bytes())]:
   header.write_bytes(contents)
   assembly=out/(name+'.s')
   report['sources'][name]=dict(header_sha256=hashlib.sha256(contents).hexdigest(),base=PARENT)
   (out/(name+'.patch')).write_text(''.join(difflib.unified_diff(exact_b.decode().splitlines(True),contents.decode().splitlines(True),fromfile='a/Source/ModeledCabConvolution.h',tofile='b/Source/ModeledCabConvolution.h')))
   run([*flags,'-S','-o',assembly,'-Rpass=inline','-Rpass-missed=inline','-Rpass=loop-vectorize','-Rpass=loop-unroll'],name+'-codegen.log',Path(entry['directory']))
   with gzip.open(out/(name+'.s.gz'),'wb') as f:f.write(assembly.read_bytes())
   assembly.unlink()
  # Header now contains C8 only. Other product files and all original tests remain exact parent bytes.
  changed=subprocess.check_output(['git','diff','--name-only'],cwd=source,text=True).splitlines()
  if changed!=['Source/ModeledCabConvolution.h']:raise RuntimeError('unexpected product changes')
  run(['cmake','--build',build,'--target','ChimeraOriginalCabIntegrationTests','ChimeraOriginalCabRealtimeTests','ChimeraCabABControl','ChimeraCabABProfile','--parallel','2'],'C8-build.log')
  dest=out/'gates';dest.mkdir();product=build/'product';regex='^('+'|'.join(ab.TESTS)+')$'
  inventory=json.loads(subprocess.check_output(['ctest','--test-dir',str(product),'-C','Release','-R',regex,'--show-only=json-v1'],text=True))
  if sorted(t['name'] for t in inventory['tests'])!=sorted(ab.TESTS):raise RuntimeError('test inventory changed')
  receipt=run(['ctest','--test-dir',product,'-C','Release','-R',regex,'--parallel','1','--verbose','--no-tests=error','--timeout','600','--test-output-size-passed','10485760','--test-output-size-failed','10485760','--output-junit',dest/'ctest.xml'],'gates/console.log',required=False)
  driver.preserve_timing_samples(product,dest,('integration','extended'));shutil.copy2(product/'Testing/Temporary/LastTest.log',dest/'LastTest.log')
  specs=dict(driver.DEFAULT_TESTS);specs[driver.EXTENDED_TEST]=('expanded-v2',(32,64,128,256,512))
  evidence=driver.test_evidence(dest/'ctest.xml',specs,True);evidence['raw_verification']=driver.validate_timing_samples(dest,evidence['timing'])
  if len(evidence['timing'])!=66:raise RuntimeError('Missing deadline routes')
  log=(dest/'console.log').read_text()
  evidence['frozen_bits_and_latency']=('exact_bits=1 immutable_spectra=1' in log and 'latency=0' in log and receipt['returncode']==0)
  evidence['returncode']=receipt['returncode'];report['gates']=evidence
  # Focused callback screen only: every raw 1600-row population is retained; never an optimization acceptance claim.
  report['screen']=[]
  for mode,suffix in [('control','Control'),('profile','Profile')]:
   binary=driver.executable(build,'ChimeraCabAB'+suffix)
   for engine,engineflags in ab.ENGINES.items():
    folder=out/'screen'/mode/engine;folder.mkdir(parents=True)
    receipt=run([binary,*engineflags],f'screen/{mode}/{engine}.log',folder,required=False)
    report['screen'].append(dict(mode=mode,engine=engine,returncode=receipt['returncode'],rows=ab.read_trial(folder,mode,engine)))
  report['complete']=True;print(json.dumps(dict(complete=True,candidate='bound8',cab_ctest_returncode=evidence['returncode'],deadline_rows=len(evidence['timing']),frozen_bits_and_latency=evidence['frozen_bits_and_latency'],integration_eligible=False,release_approved=False)),flush=True)
 finally:
  write()
  # Keep compact artifacts, not the entire dependency/build tree. Raw callback CSVs already copied.
  manifest={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file() and not any(x in p.relative_to(out).parts for x in ('build','product-source'))}
  (out/'manifest.json').write_text(json.dumps(manifest,indent=2))
if __name__=='__main__':main()

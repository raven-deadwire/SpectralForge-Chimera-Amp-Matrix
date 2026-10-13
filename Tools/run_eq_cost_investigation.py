"""Run a manual analysis-only executable and retain raw environment evidence."""
import hashlib,json,os,platform,subprocess,sys
from pathlib import Path
build=Path(sys.argv[1]);out=build/'eq-cost-investigation';out.mkdir(parents=True,exist_ok=True)
name='ChimeraEQAnalyzerCostInvestigation'+('.exe' if os.name=='nt' else '')
binaries=[p for p in build.rglob(name) if p.is_file()]
if len(binaries)!=1:raise RuntimeError(f'Expected one executable, found {binaries}')
binary=binaries[0]
inputs=['Source/EQSpectrumAnalyzer.h','Source/GraphicalEQ.h','Tests/EQAnalyzerCostInvestigation.cpp','Tests/EQAnalyzerPerformanceTests.cpp','Tests/reference/EQSpectrumAnalyzerBeb72f.h','CMakeLists.txt']
manifest={'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'platform':platform.platform(),'machine':platform.machine(),'cpu_count':os.cpu_count(),'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'inputs':{p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs},'daw_acceptance':'NOT_ASSESSED','actual_core_frequency_trace':'NOT_OBSERVED'}
if platform.system()=='Darwin':
 manifest['sysctl_snapshot']=subprocess.run(['sysctl','hw.model','hw.machine','hw.ncpu','hw.physicalcpu','hw.logicalcpu','hw.cpufrequency','machdep.cpu.brand_string'],capture_output=True,text=True).__dict__['stdout']
manifest['command']=[str(binary.resolve()),str(out.resolve()),'12','16']
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
with (out/'run.log').open('w') as log:
 result=subprocess.run(manifest['command'],stdout=log,stderr=subprocess.STDOUT)
manifest['exit_code']=result.returncode
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if result.returncode:raise SystemExit(result.returncode)
subprocess.run([sys.executable,'Tests/analyze_eq_cost_investigation.py',str(out)],check=True)

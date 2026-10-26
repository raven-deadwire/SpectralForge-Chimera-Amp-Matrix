#!/usr/bin/env python3
"""Assemble measured results without rerendering or modifying acceptance policy."""
import gzip
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile

root=Path(__file__).resolve().parents[2];work=root/'build/transpose-next'
dest=root/'docs/evidence/transpose-correlation-20261018';dest.mkdir(parents=True,exist_ok=True)
detail=json.loads((work/'detail-qualified-source/results.json').read_text())
matrix=json.loads((work/'matrix/results.json').read_text())
stress=json.loads((work/'stress/stress.json').read_text())
timing=json.loads((work/'timing/timing.json').read_text())
assert [len(d['results']) for d in [detail,matrix,stress,timing]]==[50,108,6,45]
assert len({d['renderer_sha256']for d in [detail,matrix,stress,timing]})==1
for folder,file,target in [('detail-qualified-source','results.json','detail.json'),('matrix','results.json','matrix.json'),
                           ('stress','stress.json','stress.json'),('timing','timing.json','timing.json')]:
    shutil.copy2(work/folder/file,dest/target)
    callbacks=work/folder/'callbacks' if folder in ['detail-qualified-source','matrix'] else work/folder
    out=dest/'callbacks'/folder;out.mkdir(parents=True,exist_ok=True)
    for p in callbacks.glob('*.csv'):
        (out/(p.name+'.gz')).write_bytes(gzip.compress(p.read_bytes(),mtime=0))
for name in ['build.json','sanitizer-command.json','comparison.png']:
    shutil.copy2(work/name,dest/name)
for name in ['correlation-tests-final.log','PitchRegressionTests.log','StabilityTests.log','GateTests.log',
             'sanitizer.log','sanitizer-address-undefined.log','native-build-timing.log']:
    shutil.copy2(root.parent/name,dest/name)
rej=dest/'rejected-first-onset';rej.mkdir(exist_ok=True)
for p in (work/'onset-rejected').iterdir():shutil.copy2(p,rej/p.name)
audio=dest/'audio';audio.mkdir(exist_ok=True)
for name in ['harmonics','riff75']:
    for e in ['baseline','corr40-onset']:
        fname=f'{name}-48000-128--2-{e}.wav'
        shutil.copy2(work/'detail-qualified-source/audio'/fname,audio/fname)
for e in ['baseline','corr40-onset']:
    name='quiet_markers_over_sustain-'+e
    y=np.fromfile(work/'stress'/(name+'.f32'),dtype='<f4').reshape(-1,2)
    wavfile.write(audio/(name+'.wav'),48000,y)

groups={}
for r in matrix['results']:groups.setdefault((r['engine'],r['rate'],r['shift']),set()).add(r['output_sha256'])
assert len(groups)==36 and all(len(g)==1 for g in groups.values())
summary={'decision':'REJECT_PRODUCT_INTEGRATION_10MS_TARGET_NOT_MET','release_approved':False,
 'measured_native_source_commit':'f2fa6acb63e272cfbbdce4ce06c50d24eefc59a0',
 'released_product_commit':'3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58',
 'base_main_commit':'ec6fd639e2958d34605bfb04bb81ca02c1cfb38f',
 'native_renderer_sha256':detail['renderer_sha256'],
 'functional_regression_groups':162,'native_executables_passed':4,
 'matrix_render_count':108,'block_partition_groups_equal':36,
 'external_neural_dsp_measurement':'NOT_RECOVERED','actual_DI':'NOT_AVAILABLE','DAW_acceptance':'NOT_RUN',
 'sanitizer':{'address_undefined':'PASS_candidate_code_instrumented','JUCE_libraries':'UNINSTRUMENTED',
              'leak_sanitizer':'BLOCKED_ptrace_runtime_failure; preserved_original_failure'},
 'timing_context':'Shared Intel Xeon Platinum 8573C Linux container. Sequential five-pass forward/reverse engine order; other task tests/builds completed before timing. Not a pinned real-time audio host.',
 'cpu':[]}
for block in [64,128,256]:
    for engine in ['baseline','corr40','corr40-onset']:
        rr=[r for r in timing['results']if r['block']==block and r['engine']==engine]
        assert len(rr)==5 and all(r['thread_cpu_measured']for r in rr)
        summary['cpu'].append({'block':block,'engine':engine,
            'mean_thread_cpu_us_median':float(np.median([r['thread_cpu_mean_us']for r in rr])),
            'settled_p95_thread_cpu_us_median':float(np.median([r['settled_thread_cpu_p95_us']for r in rr])),
            'settled_p99_thread_cpu_us_median':float(np.median([r['settled_thread_cpu_p99_us']for r in rr])),
            'thread_cpu_audio_percent_median':float(np.median([r['thread_cpu_audio_percent']for r in rr])),
            'wall_deadline_misses_total':sum(r['deadline_misses']for r in rr),
            'wall_max_us':max(r['max_us']for r in rr)})
summary['research_file_sha256']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
    for p in (root/'Tools/TransposeNext').glob('*')if p.is_file()}
summary['production_diff_from_release']=subprocess.check_output(['git','-C',str(root),'diff','--name-only',
    summary['released_product_commit'],'HEAD','--','Source','Tests','Assets','CMakeLists.txt','VERSION','RELEASE_CHANNEL'],text=True).splitlines()
assert not summary['production_diff_from_release']
(dest/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
manifest={str(p.relative_to(dest)):hashlib.sha256(p.read_bytes()).hexdigest()
          for p in dest.rglob('*')if p.is_file()and p.name!='SHA256.json'}
(dest/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'files':len(manifest),'bytes':sum(p.stat().st_size for p in dest.rglob('*')if p.is_file()),
                  'decision':summary['decision']}))

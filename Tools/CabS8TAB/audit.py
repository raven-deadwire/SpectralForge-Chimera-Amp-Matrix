import csv,hashlib,io,json,math,statistics,sys,zipfile
from pathlib import Path
archive=Path(sys.argv[1]);dest=Path(sys.argv[2]);dest.mkdir(parents=True,exist_ok=True)
z=zipfile.ZipFile(archive); names=set(z.namelist())
manifest=json.loads(z.read('manifest.json'))
for name,record in manifest.items():
 data=z.read(name)
 assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],name
r=json.loads(z.read('report.json'));a=json.loads(z.read('analysis.json'))
assert r['complete'] and len(r['trials'])==96 and len(a['rows'])==72
assert r['parent']=='c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
assert r['candidate']=='20c2be7ab2bf4f72683dbf163055b8a7467901e3'
assert r['release_approved'] is False
assert r['patch_ref']=='2df0b1e7c1a1e7022ed77a577bf0effb99a672fa'
assert hashlib.sha256(z.read('S8T.patch')).hexdigest()==r['patch_sha256']=='7aaef8ab7367c44083009121c6b2ac66d5b6378bc25b731b2b3720f5afbfcde0'
assert r['compiler_commands_match']
assert hashlib.sha256(z.read('generated-probe.cpp')).hexdigest()==r['probe_sha256']
callbacks=0; checked=0
for t in r['trials']:
 assert t['arm']==(t['logical'] if t['experiment']=='AB' else 'A')
 assert t['binary_sha256']==r['binaries'][t['arm']][t['mode']]['sha256']
 label=f'r{t["round"]}-{t["mode"]}-{t["experiment"]}-{t["slot"]}-{t["logical"]}'
 for engine in ('original-v1','expanded-v2','array-v3-6x10'):
  for block in (32,64):
   for ch in (1,2):
    suite='profile' if t['mode']=='profile' else 'integration'
    name=f'trials/{label}/{engine}/cab-timing-diagnostics/{suite}/{engine}/96000-{block}-{ch}.csv'
    data=list(csv.DictReader(io.StringIO(z.read(name).decode())))
    assert len(data)==1600
    callbacks+=len(data)
    for i,row in enumerate(data):
     assert int(row['callback_index'])==i and row['thread_cpu_valid']=='1'
     assert int(row['deadline_miss'])==int(float(row['wall_us'])>block/96000*1e6)
    spec=None
    if t['mode']=='profile': spec=list(csv.DictReader(io.StringIO(z.read(name+'.spectral.csv').decode())))
    for phase,lo,hi in [('all',0,1600),('worker_and_automation',0,1200),('forced_six_slot_publication',1200,1600)]:
     summary=next(s for s in t['rows'] if (s['engine'],s['block'],s['channels'],s['phase'])==(engine,block,ch,phase))
     wall=[float(s['wall_us']) for s in data[lo:hi]];cpu=[float(s['thread_cpu_us']) for s in data[lo:hi]]
     values=dict(wall_p99_us=sorted(wall)[int(.99*len(wall))],cpu_mean_us=sum(cpu)/len(cpu),cpu_p99_us=sorted(cpu)[int(.99*len(cpu))],deadline_misses=sum(w>block/96000*1e6 for w in wall),cpu_over_budget=sum(c>block/96000*1e6 for c in cpu))
     if spec:
      mac=[int(s['wall_ns'])*.001 for s in spec[lo:hi]]
      values.update(mac_mean_us=sum(mac)/len(mac),mac_p99_us=sorted(mac)[int(.99*len(mac))])
     for k,v in values.items(): assert math.isclose(v,summary[k],rel_tol=1e-10,abs_tol=1e-8),(name,phase,k,v,summary[k])
     checked+=1
assert callbacks==1843200
# Compare actual probe compiler command lines after normalizing checkout/build locations.
commands={arm:json.loads(z.read(f'build-metadata/{arm}/compile_commands.json')) for arm in ('A','B')}
normalized={}
for arm in ('A','B'):
 source=r['sources'][arm]['path']
 build=str(Path(r['binaries'][arm]['control']['path']).parents[2])
 juce=str(Path(r['binaries']['A']['control']['path']).parents[2]/'_deps/juce-src')
 selected={}
 for c in commands[arm]:
  if c['file'].endswith('/probe.cpp'):
   s=c['command'].replace(juce,'{JUCE}').replace(source,'{SOURCE}').replace(build,'{BUILD}')
   mode='profile' if 'CHIMERA_CAB_PROFILE=1' in s else 'control'
   selected[mode]=s
 assert set(selected)=={'control','profile'}
 normalized[arm]=selected
assert normalized['A']==normalized['B'],normalized
result=dict(manifest_files_verified=len(manifest),raw_callbacks_recomputed=callbacks,route_phase_summaries_recomputed=checked,
 compiler_commands_match=True,aa_identical_binary=True,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
 collection_complete=r['complete'],performance_demonstrated=r['performance_demonstrated'],integration_eligible=r['integration_eligible'])
(dest/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
for name in ('report.json','analysis.json','README.md','environment-before/environment.json','manifest.json','product-only.diff'):
 p=dest/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(z.read(name))
print(json.dumps(result,indent=2))

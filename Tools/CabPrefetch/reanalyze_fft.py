import argparse,zipfile,hashlib,json,csv,io,statistics,importlib.util
from pathlib import Path
from collections import defaultdict
ROOT=Path(__file__).resolve().parent
# Permit Git's Windows checkout newline conversion, not protocol edits.
assert hashlib.sha256((ROOT/'fft_protocol.py').read_bytes().replace(b'\r\n',b'\n')).hexdigest()=='443aacdda01d39e4ab7cbbe5e6dfa93f89f30f32fbeaf2bae1eafdbb99d29fa8'
spec=importlib.util.spec_from_file_location('original',ROOT/'fft_protocol.py')
original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
def eq(a,b):
 if isinstance(a,dict):
  assert a.keys()==b.keys()
  for k in a:eq(a[k],b[k])
 elif isinstance(a,list):
  assert len(a)==len(b)
  for x,y in zip(a,b):eq(x,y)
 elif isinstance(a,float):assert abs(a-b)<=1e-8+abs(a)*1e-12,(a,b)
 else:assert a==b,(a,b)
parser=argparse.ArgumentParser()
parser.add_argument('--archive',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
path=args.archive
assert hashlib.file_digest(path.open('rb'),'sha256').hexdigest()=='63fc5fefb526a76bf7f1651825ccb9c2ffd08de01da06ea4bffd09b3ff12d67c'
result={'archive_sha256':'63fc5fefb526a76bf7f1651825ccb9c2ffd08de01da06ea4bffd09b3ff12d67c','rows':[],'misses':[]}
with zipfile.ZipFile(path) as z:
 manifest=json.loads(z.read('population/manifest.json'))
 for p,m in manifest.items():
  with z.open('population/'+p) as f:h=hashlib.file_digest(f,'sha256').hexdigest()
  assert z.getinfo('population/'+p).file_size==m['bytes'] and h==m['sha256'],p
 print('manifest verified',len(manifest),flush=True)
 result['verified_manifest']=len(manifest)
 report=json.loads(z.read('population/report.json'))
 expected=[]
 for round_ in range(6):
  order='ABBA' if round_%2==0 else 'BAAB'
  for mode in (('control','profile') if round_%2==0 else ('profile','control')):
   for group in (('AA','AB') if round_%2==0 else ('AB','AA')):
    for slot,logical in enumerate(order):
     expected.append(dict(round=round_,mode=mode,group=group,slot=slot,logical=logical,arm=logical if group=='AB' else 'A'))
 assert len(report['trials'])==len(expected)==96
 for t,e in zip(report['trials'],expected):
  assert all(t[k]==v for k,v in e.items())
  assert set(t['routes'])=={'-'.join(map(str,r)) for r in original.ROUTES}
 pairs=0
 for name in manifest:
  if '-A.f32' in name:
   other=name.replace('-A.f32','-B.f32')
   assert manifest[name]['bytes']==manifest[other]['bytes'] and manifest[name]['sha256']==manifest[other]['sha256']
   pairs+=1
 assert pairs==76
 result['verified_output_bit_pairs']=pairs
 values=defaultdict(lambda:defaultdict(list))
 for t in report['trials']:
  label=f"r{t['round']}-{t['mode']}-{t['group']}-{t['slot']}-{t['logical']}"
  for route,record in t['routes'].items():
   name=f'population/trials/{label}/{route}.csv'
   raw=list(csv.DictReader(io.StringIO(z.read(name).decode())))
   assert len(raw)==1600
   assert all(int(r['callback'])==i and int(r['samples'])==int(route.split('-')[1]) for i,r in enumerate(raw))
   assert all(int(r['new_calls'])==int(r['delete_calls'])==0 for r in raw)
   pop=raw[200:]
   q=lambda k:sorted(float(r[k]) for r in pop)[int(.99*len(pop))]
   computed=dict(cpu_mean_ns=statistics.mean(float(r['cpu_ns']) for r in pop),cpu_p99_ns=q('cpu_ns'),wall_p99_ns=q('wall_ns'),
    hits=sum(int(r['hits']) for r in pop),misses=sum(int(r['misses']) for r in pop),overflow=sum(int(r['overflow']) for r in pop),
    forward_calls=sum(int(r['fft_forward_calls']) for r in pop),mac_calls=sum(int(r['mac_calls']) for r in pop),
    deadline_misses=sum(float(r['wall_ns'])>int(r['samples'])/96000*1e9 for r in pop))
   eq(computed,record)
   if t['group']=='AB':
    key=(t['mode'],t['arm'],route)
    computed['cpu_over_budget']=sum(float(r['cpu_ns'])>int(r['samples'])/96000*1e9 for r in pop)
    if t['mode']=='profile':
     computed['forward_mean_ns']=statistics.mean(int(r['fft_forward_ns']) for r in pop)
     computed['mac_mean_ns']=statistics.mean(int(r['mac_ns']) for r in pop)
    for k,v in computed.items():values[key][k].append(v)
    if t['mode']=='control':
     for r in pop:
      if float(r['wall_ns'])>int(r['samples'])/96000*1e9:
       result['misses'].append(dict(route=route,arm=t['arm'],callback=int(r['callback']),wall_ns=float(r['wall_ns']),cpu_ns=float(r['cpu_ns']),file=name))
 analysis=original.analyze(report['trials']);eq(analysis,report['analysis'])
 for key,metrics in values.items():
  row=dict(zip(('mode','arm','route'),key),callbacks=16800)
  for k,v in metrics.items():row[k]=sum(v) if k in ('hits','misses','overflow','forward_calls','mac_calls','deadline_misses','cpu_over_budget') else statistics.median(v)
  result['rows'].append(row)
 result['analysis']=analysis
 result['verified_raw_summaries']=len(report['trials'])*12
 result['complete']=report['complete']
 result['apple_silicon_performance_demonstrated']=report['apple_silicon_performance_demonstrated']
 result['release_approved']=report['release_approved']
 result['correctness_routes']=len(report['correctness'])
 result['source']=report['source'];result['juce']=report['juce']
 result['hardware']=z.read('hardware.txt').decode()
args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
print('verified summaries',result['verified_raw_summaries'],'CPU routes improved',analysis['cpu_routes_improved'],'regressions',analysis['established_regressions'],flush=True)
for r in result['rows']:
 if '-32-' in r['route']:print(r)

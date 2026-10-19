#!/usr/bin/env python3
"""Offline paired reanalysis. Does not execute any rejected DSP candidate."""
import argparse,csv,hashlib,importlib.util,json,math,statistics
from collections import Counter,defaultdict
from pathlib import Path

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,obj):
 p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(obj,indent=2,allow_nan=False)+'\n')
def manifest(root,name):
 data=json.loads((root/name).read_text());entries=data if isinstance(data,list) else [dict(path=k,**v) for k,v in data.items()]
 for item in entries:
  p=root/item['path'];assert p.stat().st_size==item['bytes'] and digest(p)==item['sha256'],p
 return len(entries)
def equal(a,b):
 if isinstance(a,dict):
  assert a.keys()==b.keys()
  for k in a:equal(a[k],b[k])
 elif isinstance(a,list):
  assert len(a)==len(b)
  for x,y in zip(a,b):equal(x,y)
 elif isinstance(a,float):assert math.isfinite(a) and abs(a-b)<=1e-8+abs(a)*1e-12,(a,b)
 else:assert a==b,(a,b)
def summarize(rows):
 budget=float(rows[0]['budget_us']);miss=[r for r in rows if float(r['wall_us'])>budget]
 after=lambda r:r['phase']=='worker_and_automation' and int(r['callback_index'])%8==1
 def stat(rs):
  if not rs:return {'n':0}
  return dict(n=len(rs),misses=sum(float(r['wall_us'])>budget for r in rs),cpu_over_budget=sum(float(r['thread_cpu_us'])>budget for r in rs))
 return dict(n=len(rows),budget_us=budget,wall_p99_us=sorted(float(r['wall_us']) for r in rows)[int(.99*len(rows))],
  cpu_p99_us=sorted(float(r['thread_cpu_us']) for r in rows)[int(.99*len(rows))],misses=len(miss),
  misses_cpu_over_budget=sum(float(r['thread_cpu_us'])>budget for r in miss),
  misses_wall_only=sum(float(r['thread_cpu_us'])<=budget for r in miss),
  after_sleep=stat([r for r in rows if after(r)]),other=stat([r for r in rows if not after(r)]),
  phases={p:stat([r for r in rows if r['phase']==p]) for p in sorted({r['phase'] for r in rows})})
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--pr101',type=Path,required=True);ap.add_argument('--pr96',type=Path,required=True)
 ap.add_argument('--pr96-analyzer',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
 out={'diagnostic_only':True,'physical_device_dropout':'not_tested','product_fix_demonstrated':False,
  'pr101_manifest':manifest(args.pr101,'SHA256SUMS.json'),'pr96_manifest':manifest(args.pr96,'manifest.json')}
 out['pr101_routes']=[]
 for p in sorted((args.pr101/'candidate/cab-timing-diagnostics').rglob('*.csv')):
  rows=list(csv.DictReader(p.open()));out['pr101_routes'].append(dict(file=str(p.relative_to(args.pr101)),**summarize(rows)))
 p=args.pr101/'candidate/cab-timing-diagnostics/extended/expanded-v2/96000-32-2.csv';rows=list(csv.DictReader(p.open()));out['failure']=summarize(rows)
 out['failure']['miss_rows']=[r for r in rows if int(r['deadline_miss'])]
 spec=importlib.util.spec_from_file_location('pinned_pr96_analyzer',args.pr96_analyzer);ab=importlib.util.module_from_spec(spec);spec.loader.exec_module(ab)
 report=json.loads((args.pr96/'report.json').read_text());summaries=0;groups=defaultdict(list)
 expected=[]
 for rnd in range(6):
  order='ABBA' if rnd%2==0 else 'BAAB'
  for mode in (('control','profile') if rnd%2==0 else ('profile','control')):
   for experiment in (('AA','AB') if rnd%2==0 else ('AB','AA')):
    for slot,logical in enumerate(order):expected.append(dict(round=rnd,mode=mode,experiment=experiment,slot=slot,logical=logical,arm=logical if experiment=='AB' else 'A'))
 assert len(report['trials'])==len(expected)==96
 for trial,want in zip(report['trials'],expected):
  assert all(trial[k]==v for k,v in want.items())
  label=f"r{trial['round']}-{trial['mode']}-{trial['experiment']}-{trial['slot']}-{trial['logical']}";raw=[]
  for engine in ab.ENGINES:
   folder=args.pr96/'trials'/label/engine;raw+=ab.read_trial(folder,trial['mode'],engine)
   if trial['mode']=='control' and trial['experiment']=='AB':
    for p in folder.rglob('*.csv'):
     if p.name.endswith('.spectral.csv'):continue
     rows=list(csv.DictReader(p.open()))
     if rows and rows[0]['block_size']=='32':groups[trial['arm']].extend(rows)
  equal(raw,trial['rows']);summaries+=len(raw)
 analysis=ab.analyze(report['trials']);equal(analysis,json.loads((args.pr96/'analysis.json').read_text()))
 out['pr96']={'raw_summaries':summaries,'statistics':len(analysis['rows']),'performance_demonstrated':analysis['performance_demonstrated'],
  'decision_flags':{k:report[k] for k in ('cab_contracts_pass','deadline_nonincrease','review_eligible','integration_eligible','release_approved')},
  '32_sample_control_AB':{k:summarize(v) for k,v in groups.items()},'analyzer_sha256':digest(args.pr96_analyzer)}
 write(args.output,out);print(json.dumps({'failure':{k:v for k,v in out['failure'].items() if k!='miss_rows'},'pr96':out['pr96']},indent=2))
if __name__=='__main__':main()

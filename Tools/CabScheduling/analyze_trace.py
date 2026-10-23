#!/usr/bin/env python3
"""Join diagnostic callbacks, source stages, worker waits and pacing. No causal verdict."""
import argparse,csv,json,math,statistics
from collections import defaultdict
from pathlib import Path

def rows(p):
 with p.open() as f:return list(csv.DictReader(f))
def correlation(xs,ys):
 if len(xs)<2:return None
 mx,my=statistics.mean(xs),statistics.mean(ys)
 xy=sum((x-mx)*(y-my) for x,y in zip(xs,ys));xx=sum((x-mx)**2 for x in xs);yy=sum((y-my)**2 for y in ys)
 return xy/math.sqrt(xx*yy) if xx and yy else None

def analyze(prefix):
 meta=json.loads(Path(str(prefix)+'.metadata.json').read_text());assert meta['complete']
 callbacks=rows(Path(str(prefix)+'.callbacks.csv'));pacing=rows(Path(str(prefix)+'.pacing.csv'))
 assert len(callbacks)==len(pacing)==1600
 work=rows(Path(str(prefix)+'.worker.csv'));stages=defaultdict(lambda:defaultdict(float));accept=defaultdict(int)
 for e in rows(Path(str(prefix)+'.stages.csv')):
  i=int(e['callback_index']);assert 0<=i<1600
  # Raw inclusive scope sums. Recursive cab_process scopes overlap even with
  # the same name; do not use them as a callback total. The selected FFT/MAC/
  # swap/fade metrics below have disjoint same-named scopes on the audio thread.
  for clock in ('wall','cpu'):stages[i][e['stage']+'_'+clock+'_us']+=int(e[clock+'_ns'])/1000
  if e['stage']=='kernel_accept':accept[i]+=1
 waits=[]
 for e in work:
  if e['stage']!='worker_wait':continue
  requested=int(e['wait_requested_ns']);notified=int(e['wait_notified']);assert requested==20000000 and notified in (0,1)
  waits.append({'start_ns':int(e['wall_start_ns']),'end_ns':int(e['wall_end_ns']),'requested_ns':requested,'notified':bool(notified),
   'timeout_overshoot_us':None if notified else (int(e['wall_ns'])-requested)/1000})
 joined=[]
 for i,(r,p) in enumerate(zip(callbacks,pacing)):
  assert int(r['callback_index'])==int(p['callback_index'])==i
  cadence=meta['cadence'];assert p['cadence']==cadence and cadence in ('legacy','continuous')
  slept=cadence=='legacy' and i<1200 and i%8==0
  assert int(p['sleep_requested_ns'])==(1000000 if slept else 0)
  if slept:assert int(p['sleep_clock_valid'])==1 and int(p['sleep_end_ns'])>=int(p['sleep_start_ns'])>=int(r['wall_end_ns'])
  else:assert int(p['sleep_clock_valid'])==0 and int(p['sleep_start_ns'])==int(p['sleep_end_ns'])==0
  if i+1<len(callbacks):assert int(p['next_callback_start_ns'])==int(callbacks[i+1]['wall_start_ns'])
  start,end=int(r['wall_start_ns']),int(r['wall_end_ns']);budget=float(r['deadline_ns'])
  d={'callback_index':i,'phase':r['phase'],'wall_us':int(r['wall_ns'])/1000,'cpu_us':int(r['cpu_ns'])/1000,
   'miss':int(r['deadline_miss']),'cpu_over_budget':int(r['cpu_ns'])>budget,
   'wall_only_miss':int(r['deadline_miss']) and int(r['cpu_ns'])<=budget,
   'post_sleep':i>0 and int(pacing[i-1]['sleep_requested_ns'])>0,'kernel_accept_count':accept[i],
   'fading':stages[i]['fading_convolution_wall_us']>0,
   'worker_build_overlap':any(e['stage']=='worker_build' and int(e['wall_start_ns'])<end and int(e['wall_end_ns'])>start for e in work),
   'worker_publish_overlap':any(e['stage']=='worker_publish' and int(e['wall_start_ns'])<end and int(e['wall_end_ns'])>start for e in work),
   'post_worker_timeout':any(not w['notified'] and 0<=start-w['end_ns']<=budget for w in waits)}
  assert bool(d['miss'])==(int(r['wall_ns'])>budget)
  d.update(stages[i]);joined.append(d)
 categories=['post_sleep','kernel_accept_count','fading','worker_build_overlap','worker_publish_overlap','post_worker_timeout']
 summaries=[]
 metrics=['fft_forward','fft_inverse','spectral_mac','kernel_swap','fading_convolution','fade_mix']
 for phase in dict.fromkeys(r['phase'] for r in joined):
  pop=[r for r in joined if r['phase']==phase]
  summary={'phase':phase,'n':len(pop),'misses':sum(r['miss'] for r in pop),'cpu_over_budget':sum(r['cpu_over_budget'] for r in pop),
   'wall_only_misses':sum(r['wall_only_miss'] for r in pop),'wall_p99_us':sorted(r['wall_us'] for r in pop)[int(.99*len(pop))],
   'cpu_p99_us':sorted(r['cpu_us'] for r in pop)[int(.99*len(pop))],'categories':{},'associations':{}}
  for category in categories:
   if meta['trace_level']=='off' and category!='post_sleep':
    summary['categories'][category]={'available':False};continue
   summary['categories'][category]={str(on):{'n':sum(bool(r[category])==on for r in pop),'misses':sum(r['miss'] for r in pop if bool(r[category])==on)} for on in (False,True)}
  for stage in metrics:
   for clock in ('wall','cpu'):
    key=f'{stage}_{clock}_us'
    if meta['trace_level']=='off':
     summary['associations'][key]={'available':False};continue
    xs=[r.get(key,0) for r in pop]
    summary['associations'][key]={'mean_us':statistics.mean(xs),'pearson_with_wall':correlation(xs,[r['wall_us'] for r in pop]),
     'point_biserial_with_miss':correlation(xs,[r['miss'] for r in pop])}
  summaries.append(summary)
 return {'diagnostic_only':True,'cadence':meta['cadence'],'trace_level':meta['trace_level'],'stage_times_inclusive':True,
  'correlations_are_descriptive_not_causal':True,'worker_wake_metric':'timeout wait-return overshoot, not kernel scheduler runnable delay; signalled waits have no inferred deadline',
  'physical_device_dropout':'not_tested','waits':waits,'summaries':summaries,'miss_rows':[r for r in joined if r['miss']]}

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 result={p.name:analyze(Path(str(p)[:-len('.metadata.json')])) for p in a.input.glob('*.metadata.json')};assert result
 a.output.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
if __name__=='__main__':main()

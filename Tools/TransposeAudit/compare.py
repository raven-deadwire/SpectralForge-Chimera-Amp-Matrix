#!/usr/bin/env python3
"""Frozen PR125/128 comparison. Synthetic only; no release/PDC approval."""
import argparse,csv,gzip,hashlib,json,platform,subprocess,sys,os
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/TransposeNext'))
from weak_attack_study import generate,FREQUENCIES,event_detection
from weak_attack_holdout import lowband_extra
from evaluate import attacks
from legacy_metrics import tone_metrics,impulse_metrics,sha
ENGINES=['baseline','tight125','corr40-split-rt']
SHIFTS=[-2,-1,1,2]
# Frozen before seeing results; never used to tune any candidate.
SEED=2026102307
p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--timing',action='store_true');p.add_argument('--pin-cpu',action='store_true');a=p.parse_args();
if a.pin_cpu:os.sched_setaffinity(0,{min(os.sched_getaffinity(0))})
a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True)
work=a.out/'work';work.mkdir(exist_ok=True);cb=a.out/'callbacks';cb.mkdir(exist_ok=True);audio=a.out/'audio';audio.mkdir(exist_ok=True)
rows=[];inputs={};controls={};rate=48000

def holdout(instrument):
 f=FREQUENCIES[instrument];rng=np.random.default_rng(SEED);t=np.arange(3*rate)/rate;starts=.3+np.cumsum(rng.uniform(.08,.135,16));onsets=[round(s*rate) for s in starts];z=np.zeros_like(t)
 for index,on in enumerate(onsets):
  n=round(.065*rate);u=np.arange(n)/rate;gain=rng.uniform(.7,1) if index%2==0 else rng.uniform(.06,.2);decay=rng.uniform(.018,.04);phases=rng.uniform(-.2,.2,8)
  body=sum(.14/h*np.sin(2*np.pi*f*h*u+phases[h-1]) for h in range(1,9));env=(1-np.exp(-u/.001))*np.exp(-u/decay);env[-240:]*=np.linspace(1,0,240)
  z[on:on+n]+=gain*(body*env+.05*np.exp(-u/.002)*rng.uniform(-1,1,n))
 return np.column_stack((z,-.65*z)).astype('<f4'),{'name':'fresh_holdout','rate':rate,'onsets':onsets,'fundamental_hz':f,'seed':SEED,'synthetic':True}

def obtain(name,instrument):
 key=name+'-'+instrument
 if key not in inputs:
  if name=='fresh_holdout':x,m=holdout(instrument)
  elif name=='timing_mix':
   x,m=generate('alternating_velocity',rate,instrument);z,_=generate('quiet_markers_over_sustain',rate,instrument);x=np.tile(np.concatenate([x,z]),(3,1));m.pop('onsets',None)
  else:x,m=generate(name,rate,instrument)
  ip=work/(key+'.f32');x.tofile(ip);inputs[key]={**m,'sha256':sha(ip),'samples':len(x)}
 return key,work/(key+'.f32'),inputs[key]

def run(name,instrument,engine,shift,block=128,rep=0,score=True):
 key,ip,m=obtain(name,instrument);uid=f'{key}-{shift}-{engine}-b{block}-r{rep}';op=work/'output.f32';cp=work/'callbacks.csv'
 cmd=[str(a.renderer),str(ip),str(op),str(rate),str(block),str(shift),engine,str(cp)]+(['cpu'] if a.timing else [])
 r=subprocess.run(cmd,capture_output=True,text=True,check=True);native=json.loads(r.stdout)
 row={'input':key,'name':name,'instrument':instrument,'engine':engine,'shift':shift,'rate':rate,'block':block,'repetition':rep,**native,'output_sha256':sha(op)}
 dest=cb/(uid+'.csv.gz');dest.write_bytes(gzip.compress(cp.read_bytes(),mtime=0));row['callback_sha256']=sha(dest)
 y=np.fromfile(op,dtype='<f4').reshape(-1,2);x=np.fromfile(ip,dtype='<f4').reshape(-1,2)
 row['stereo_null_max']=float(np.max(np.abs(y[:,1]+.65*y[:,0])))
 if a.timing:
  data=np.genfromtxt(cp,delimiter=',',names=True);cpu=data['thread_cpu_us'];wall=data['microseconds'];budget=data['samples']/rate*1e6
  row['timing']={k:float(v) for k,v in {'cpu_mean_us':np.mean(cpu),'cpu_p99_us':np.quantile(cpu,.99),'cpu_p999_us':np.quantile(cpu,.999),'cpu_peak_us':max(cpu),'wall_p99_us':np.quantile(wall,.99),'wall_peak_us':max(wall),'cpu_budget_percent':100*sum(cpu)/sum(budget)}.items()}
  row['timing'].update(cpu_misses=int(sum(cpu>budget)),wall_misses=int(sum(wall>budget)),wall_only_misses=int(sum((wall>budget)&(cpu<=budget))),callbacks=len(cpu))
 elif score:
  if name in ('harmonics','weakfundamental','tremolo'):
   f=m['fundamental_hz']*2**(shift/12);amp=.012 if name=='weakfundamental' else .12
   row['tone']=tone_metrics(y,rate,f,amp);row['steady_screen_applicable']=name!='tremolo'
   ck=f'{f}-{amp}'
   if ck not in controls:
    t=np.arange(3*rate)/rate;z=amp*np.sin(2*np.pi*f*t);ideal=np.column_stack((z,-.65*z));v=tone_metrics(ideal,rate,f,amp)
    controls[ck]={'tone':v,'valid':abs(v['dominant_error_cents'])<1 and abs(v['target_gain_db'])<.1 and v['warble_p95_p5_cents']<2}
   row['ideal_tone_control_valid']=controls[ck]['valid']
  elif name=='impulses':
   row['impulse']=impulse_metrics(y,m);sos=signal.butter(4,120,fs=rate,output='sos');im=impulse_metrics(signal.sosfilt(sos,x.astype(float),axis=0),m);om=impulse_metrics(signal.sosfilt(sos,y.astype(float),axis=0),m)
   row['low_impulse']=om;row['low_impulse_input']=im
   row['low_impulse_extra_centroid_ms']=[v.get('centroid_ms',float('nan'))-u.get('centroid_ms',float('nan')) for u,v in zip(im['events'],om['events'])]
  else:
   before=attacks(x,m);after=attacks(y,m);extra=np.array(after['attack_lags_ms'])-np.array(before['attack_lags_ms'])
   row.update(input_attack=before,attack=after,attack_extra_ms=extra.tolist(),attack_metric_valid=bool(before['attack_lag_max_ms']<=10),lowband_extra_centroid_ms=lowband_extra(x,y,m['onsets'],rate),onset_association=event_detection(native['onset_samples'],m))
   # Test the reused scoring pipeline with known 10ms delay; expose window bias.
   if key not in controls:
    delayed=np.zeros_like(x);delayed[480:]=x[:-480];dm=attacks(delayed,m)
    controls[key]={'known_delay_ms':10,'attack_extra_ms':(np.array(dm['attack_lags_ms'])-np.array(before['attack_lags_ms'])).tolist(),'lowband_extra_centroid_ms':lowband_extra(x,delayed,m['onsets'],rate)}
  if (instrument,shift) in [('bass_B0',-2),('guitar_dropA_A1',2)] and name=='alternating_velocity':
   wavfile.write(audio/(uid+'.wav'),rate,y);wavfile.write(audio/(key+'-input.wav'),rate,x)
 rows.append(row)
 output={'status':'SYNTHETIC_ONLY_RELEASE_NOT_APPROVED','platform':platform.platform(),'cpu_affinity':sorted(os.sched_getaffinity(0)),'numpy':np.__version__,'renderer_sha256':sha(a.renderer),'frozen_holdout_seed':SEED,'inputs':inputs,'controls':controls,'results':rows,'real_DI':'NOT_AVAILABLE','Neural_DSP_original':'NOT_AVAILABLE','DAW_PDC':'NOT_RUN','hardware_roundtrip':'NOT_RUN'}
 (a.out/'results.json').write_text(json.dumps(output,indent=2,allow_nan=False)+'\n')
 print(len(rows),uid,flush=True)

if not a.timing:
 for inst in FREQUENCIES:
  for name in ['harmonics','weakfundamental','riff75','alternating_velocity','fresh_holdout']:
   for shift in SHIFTS:
    for engine in ENGINES:run(name,inst,engine,shift)
 for shift in SHIFTS:
  for engine in ENGINES:run('impulses','bass_B0',engine,shift)
 # Modulated signals are diagnostics; do not apply stationary warble limits.
 for inst in FREQUENCIES:
  for engine in ENGINES:run('tremolo',inst,engine,-2)
 # Block-boundary checks and public/default/wrapper equivalence checks.
 for shift in [-2,2]:
  for engine in ENGINES:
   for block in [64,256]:run('alternating_velocity','bass_B0',engine,shift,block,score=False)
 for name in ['harmonics','alternating_velocity']:
  for engine in ['classic125','baseline128','wrapper125']:run(name,'bass_B0',engine,-2,score=False)
else:
 # All builds/functional/quality runs must finish before invoking timing mode.
 for block,instruments,shifts in [(128,list(FREQUENCIES),SHIFTS),(32,['guitar_dropA_A1'],[2])]:
  for inst in instruments:
   for shift in shifts:
    for rep in range(3):
     order=ENGINES[rep:]+ENGINES[:rep]
     for engine in order:run('timing_mix',inst,engine,shift,block,rep,score=False)
print('DONE',len(rows),flush=True)

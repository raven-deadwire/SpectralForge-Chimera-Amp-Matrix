#!/usr/bin/env python3
"""Synthetic screening; audio labels never enter the C++ engine.

Fixed, provisional screens from prior Transpose research: abs pitch <=10 cents,
warble <=20 cents, coherent/local fundamental within 1 dB, impulse centroid <=10
ms, impulse 5-95% width <=2 ms. These are NOT modified release-approval gates.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import numpy as np
from scipy import signal
from scipy.io import wavfile
from legacy_metrics import fixture, tone_metrics, impulse_metrics, db

RATE=48000
F0=30.8677063285
ENGINES=['baseline','predict1024','predict2048','predict4096']
SHIFTS=[-2,-1,1,2]

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def source(name,shift=0):
    ratio=2**(shift/12)
    if name in ['impulses','riff75','riff125']:
        return fixture(name,RATE)
    t=np.arange(RATE*3)/RATE
    f=F0;amps=[.2]
    if name=='harmonics':amps=(.12*np.array([1,.8,.5,.3,.2,.16,.12,.1])).tolist()
    elif name=='weak':amps=(.12*np.array([.1,1,.6,.4,.3,.2,.15,.1])).tolist()
    elif name=='sineA0':f=27.5
    elif name=='sineE1':f=41.2034446
    elif name not in ['sineB0','dyad']:raise ValueError(name)
    x=sum(a*np.sin(2*np.pi*f*ratio*(i+1)*t+.19*(i+1)) for i,a in enumerate(amps))
    targets=[f*ratio*(i+1) for i in range(len(amps))]
    if name=='dyad':
        f2=F0*2**(2/12);x+=.14*np.sin(2*np.pi*f2*ratio*t+.71);targets.append(f2*ratio)
    return np.column_stack((x,-.65*x)).astype('<f4'),{'name':name,'rate':RATE,'synthetic':True,'fundamental_hz':f,'amplitude':amps[0],'targets':targets}

def out_of_target(y,targets):
    z=y[RATE:RATE*3,0].astype(float);w=signal.windows.hann(len(z),sym=False)
    power=np.abs(np.fft.rfft(z*w))**2;hz=np.fft.rfftfreq(len(z),1/RATE)
    valid=(hz>=15)&(hz<=16000);inside=np.zeros(len(hz),bool)
    for f in targets:inside|=abs(hz-f)<2.0
    return float(np.sum(power[valid&~inside])/max(np.sum(power[valid]),1.e-30))

def attacks(y,x,meta):
    # Offline scoring only. Same zero-phase filter and 2 ms RMS window applied
    # to input and output; input peak bias is retained. No per-event alignment.
    sos=signal.butter(3,[1000,8000],btype='bandpass',fs=RATE,output='sos')
    def env(a):
        h=signal.sosfiltfilt(sos,a[:,0].astype(float))
        return np.sqrt(np.maximum(0,np.convolve(h*h,np.ones(96)/96,'same')))
    ex,ey=env(x),env(y);events=[]
    for onset in meta['onsets']:
        end=onset+int(.070*RATE)
        px=onset+int(np.argmax(ex[onset:end]));py=onset+int(np.argmax(ey[onset:end]))
        inp=float(ex[px]);out=float(ey[py]);gain=db(out/max(inp,1.e-20))
        events.append({'peak_offset_ms':(py-px)/RATE*1000,'input_peak_ms':(px-onset)/RATE*1000,'peak_gain_db':gain,'timing_usable':gain>-20})
    usable=[e for e in events if e['timing_usable']]
    return {'events':events,'usable_count':len(usable),'peak_offset_median_ms':float(np.median([e['peak_offset_ms'] for e in usable])) if usable else None,'peak_gain_median_db':float(np.median([e['peak_gain_db'] for e in events]))}

def evaluate(renderer,out):
    out.mkdir(parents=True,exist_ok=True);raw=out/'raw';raw.mkdir(exist_ok=True)
    rows=[];controls=[];names=['sineB0','sineA0','sineE1','harmonics','weak','dyad','impulses','riff75','riff125']
    for name in names:
        x,meta=source(name);inp=raw/(name+'.f32');x.tofile(inp)
        for shift in SHIFTS:
            ideal,imeta=source(name,shift)
            tone=name not in ['impulses','riff75','riff125','dyad']
            if tone:
                oracle=tone_metrics(ideal,RATE,meta['fundamental_hz']*2**(shift/12),meta['amplitude'])
                ok=abs(oracle['dominant_error_cents'])<.1 and abs(oracle['target_gain_db'])<.1 and oracle['warble_p95_p5_cents']<1
                controls.append({'fixture':name,'shift':shift,'pass':ok,'metrics':oracle})
                # A failed ideal control invalidates this metric family. Keep
                # measuring/reporting diagnostics; never call it candidate PASS.
            for engine in ENGINES:
                key=f'{name}_{shift}_{engine}';audio=raw/(key+'.f32');cb=raw/(key+'.csv')
                proc=subprocess.run([str(renderer),str(inp),str(audio),str(RATE),'128',str(shift),engine,str(cb)],capture_output=True,text=True,check=True)
                y=np.fromfile(audio,dtype='<f4').reshape(-1,2)
                if y.shape!=x.shape or not np.isfinite(y).all():raise RuntimeError('invalid render')
                row={'fixture':name,'shift':shift,'engine':engine,'synthetic':True,'input_sha256':sha(inp),'output_sha256':sha(audio),'native':json.loads(proc.stdout),'peak_abs':float(np.max(np.abs(y)))}
                times=np.genfromtxt(cb,delimiter=',',names=True)
                row['diagnostic_timing']={'p99_wall_us':float(np.percentile(times['wall_us'],99)),'max_wall_us':float(max(times['wall_us'])),'deadline_misses':int(np.sum(times['wall_us']>times['samples']/RATE*1e6)),'callbacks':int(len(times)),'controlled_realtime_acceptance':False}
                if tone:
                    row['tone']=tone_metrics(y,RATE,meta['fundamental_hz']*2**(shift/12),meta['amplitude'])
                    row['tone_metric_control_valid']=ok
                    z=row['tone'];row['tone_screen_pass']=ok and abs(z['dominant_error_cents'])<=10 and z['warble_p95_p5_cents']<=20 and abs(z['target_gain_db'])<=1 and abs(z['local_fundamental_band_gain_db'])<=1
                    row['out_of_target_power_fraction']=out_of_target(y,imeta['targets'])
                elif name=='dyad':
                    # Close tones are inside the legacy demodulation filter's
                    # passband: DO NOT interpret its warble/frequency estimator.
                    z=y[RATE:3*RATE,0].astype(float);t=np.arange(len(z))/RATE;w=signal.windows.hann(len(z),sym=False)
                    row['dyad_target_gain_db']=[db((2*abs(np.sum(z*w*np.exp(-2j*np.pi*f*t)))/w.sum())/a) for f,a in zip(imeta['targets'],[.2,.14])]
                    row['out_of_target_power_fraction']=out_of_target(y,imeta['targets'])
                elif name=='impulses':
                    row['impulse']=impulse_metrics(y,meta);q=row['impulse']
                    row['latency_screen_pass']=q['missing_count']==0 and q['centroid_ms_range'][1]<=10 and q['smear_ms_range'][1]<=2
                    row['impulse_output_input_energy_db']=db(np.linalg.norm(y)/np.linalg.norm(x))
                else:row['attack']=attacks(y,x,meta)
                rows.append(row)
                (out/'checkpoint.json').write_text(json.dumps({'controls':controls,'rows':rows},indent=2,allow_nan=False)+'\n')
                if name=='riff75' and shift==-2:
                    wavfile.write(out/(engine+'_riff75_RAW.wav'),RATE,y)
                    scale=min(float(np.sqrt(np.mean(x*x))/max(np.sqrt(np.mean(y*y)),1.e-20)),.95/max(float(np.max(np.abs(y))),1.e-20))
                    wavfile.write(out/(engine+'_riff75_RMSmatched.wav'),RATE,(y*scale).astype(np.float32))
                    row['audition_gain']=scale
            print(name,shift,flush=True)
    statuses={}
    for engine in ENGINES:
        es=[r for r in rows if r['engine']==engine]
        statuses[engine]={'tone_pass':sum(r.get('tone_screen_pass',False) for r in es),'tone_total':sum('tone' in r for r in es),'latency_pass':sum(r.get('latency_screen_pass',False) for r in es),'latency_total':4,'release_eligible':False}
    report={'schema_version':1,'sample_rate':RATE,'block_size':128,'source_provenance':'actual Source/PolyPitch.h from product source 3bec6c70, byte-identical on base ec6fd639; independent predictive research','platform':platform.platform(),'renderer_sha256':sha(renderer),'reference_status':{'neural_original_measurement':'NOT_AVAILABLE','azhi_dahaka_isolated_DI':'NOT_AVAILABLE','actual_DAW':'NOT_RUN','host_reported_and_roundtrip_latency':'NOT_MEASURED','production_integration':False,'release_eligible':False},'screens':{'pitch_abs_cents':10,'warble_width_cents':20,'fundamental_abs_gain_db':1,'impulse_centroid_max_ms':10,'impulse_width_max_ms':2},'controls':controls,'summary':statuses,'rows':rows}
    (out/'results.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps(statuses,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    evaluate(a.renderer.resolve(),a.out.resolve())

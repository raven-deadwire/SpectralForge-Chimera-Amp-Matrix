#!/usr/bin/env python3
"""Native DSP comparison; synthetic fixtures only. Fixed, disclosed research gates.

legacy_metrics.py is the unchanged PR45 measurement script recovered from the
2026-10-13 comparison package. It supplies tone/impulse measurements, never DSP.
No oracle frequencies or note names are passed to the C++ engines.
"""
import argparse
import hashlib
import json
import platform
import subprocess
from pathlib import Path
import numpy as np
from scipy import signal
from scipy.io import wavfile
from legacy_metrics import fixture, tone_metrics, impulse_metrics, db, sha

ENGINES = ['baseline', 'relative', 'corr10', 'corr40', 'corr40-onset']
B0 = 30.8677063285

def make(name, rate):
    if name in ['sineB0','harmonics','riff75','riff125','impulses']:
        x,m=fixture(name,rate)
        m['amplitudes']=[.12] if name=='harmonics' else [.2]
        m['frequencies']=[B0]
        return x,m
    t=np.arange(3*rate)/rate
    frequencies=[B0,36.7080959897] if name=='dyad' else [B0]
    amps=[.12,.03] if name=='dyad' else [.012]
    x=sum(a*np.sin(2*np.pi*f*t+.31*k) for k,(a,f) in enumerate(zip(amps,frequencies)))
    if name=='weakfundamental':
        x+=sum(.1/h*np.sin(2*np.pi*B0*h*t+.17*h) for h in range(2,9))
    elif name!='dyad':raise ValueError(name)
    return np.column_stack((x,-.65*x)).astype('<f4'),{'name':name,'rate':rate,'synthetic':True,'frequencies':frequencies,'amplitudes':amps}

def attacks(y,meta):
    rate=meta['rate'];x=y[:,0].astype(float)
    # High-band envelope marks pick/noise attack. This is distinct from low-band
    # pitch acquisition and does not establish a fixed host PDC delay.
    hi=signal.sosfiltfilt(signal.butter(3,[1000,8000],fs=rate,btype='bandpass',output='sos'),x)
    width=round(.002*rate)
    env=np.sqrt(signal.convolve(hi*hi,np.ones(width)/width,mode='same'))
    onsets=meta['onsets'];positions=[];lags=[];peaks=[]
    for onset in onsets:
        a=onset;b=min(len(env),onset+round(.070*rate))
        pos=a+int(np.argmax(env[a:b]));positions.append(pos);peaks.append(float(env[pos]));lags.append((pos-onset)*1000/rate)
    # Event-specific valley before the next detected attack, not a fixed-delay
    # window that unfairly scores a variable-delay candidate.
    ratios=[]
    for i in range(len(positions)-1):
        a=positions[i];b=positions[i+1]
        if b>a:
            valley=float(np.mean(env[a+round((b-a)*.65):a+round((b-a)*.9)]))
            ratios.append(valley/max(peaks[i],1.e-15))
    return {'attack_lags_ms':lags,'attack_lag_median_ms':float(np.median(lags)),
            'attack_lag_max_ms':float(max(lags)), 'attack_jitter_range_ms':float(max(lags)-min(lags)),
            'hf_valley_to_peak_db':db(np.mean(ratios)),
            'all_events_have_energy':bool(min(peaks)>1.e-6)}

def main():
    p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--matrix',action='store_true')
    a=p.parse_args();a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True)
    audio=a.out/'audio';audio.mkdir(exist_ok=True);timing=a.out/'callbacks';timing.mkdir(exist_ok=True)
    rows=[];inputs={};oracle=[];metric_cache={}
    engines=['baseline','corr40','corr40-onset'] if a.matrix else ENGINES
    configurations=[]
    if a.matrix:
        for rate in [44100,48000,96000]:
            for block in [64,128,256]:
                for shift in [-2,-1,1,2]:
                    for engine in engines:
                        configurations.append(('harmonics',rate,block,shift,engine))
    else:
        for name in ['sineB0','harmonics','weakfundamental','dyad','impulses','riff75','riff125']:
            for engine in ENGINES:configurations.append((name,48000,128,-2,engine))
        for shift in [-1,1,2]:
            for engine in ENGINES:configurations.append(('harmonics',48000,128,shift,engine))
    for name,rate,block,shift,engine in configurations:
        key=f'{name}-{rate}';ip=audio/(key+'.f32')
        if key not in inputs:
            x,meta=make(name,rate);x.tofile(ip);inputs[key]={**meta,'sha256':sha(ip)}
        meta=inputs[key];uid=f'{key}-{block}-{shift}-{engine}'
        op=audio/(uid+'.f32');cp=timing/(uid+'.csv')
        r=subprocess.run([str(a.renderer),str(ip),str(op),str(rate),str(block),str(shift),engine,str(cp)],check=True,capture_output=True,text=True)
        y=np.fromfile(op,dtype='<f4').reshape(-1,2)
        row={'input':key,'engine':engine,'rate':rate,'block':block,'shift':shift,
             **json.loads(r.stdout),'output_sha256':sha(op),'callback_sha256':sha(cp)}
        row['stereo_null_max']=float(np.max(np.abs(y[:,1]+.65*y[:,0])))
        if name in ['sineB0','harmonics','weakfundamental','dyad']:
            metric_key=(row['output_sha256'],rate,tuple(meta['frequencies']),tuple(meta['amplitudes']),shift)
            if metric_key not in metric_cache:
                metric_cache[metric_key]=[tone_metrics(y,rate,f*2**(shift/12),amp) for f,amp in zip(meta['frequencies'],meta['amplitudes'])]
            row['voices']=metric_cache[metric_key]
            # Independent ideal shifted fixture validates the measurement floor.
            if key+str(shift) not in [o['id'] for o in oracle]:
                t=np.arange(3*rate)/rate
                z=sum(amp*np.sin(2*np.pi*f*2**(shift/12)*t+.31*k) for k,(f,amp) in enumerate(zip(meta['frequencies'],meta['amplitudes'])))
                ideal=np.column_stack((z,-.65*z))
                ov=[tone_metrics(ideal,rate,f*2**(shift/12),amp) for f,amp in zip(meta['frequencies'],meta['amplitudes'])]
                oracle.append({'id':key+str(shift),'voices':ov,
                               'valid':all(abs(v['dominant_error_cents'])<1 and abs(v['target_gain_db'])<.1 and v['warble_p95_p5_cents']<2 for v in ov)})
            row['tone_metric_valid']=next(o['valid'] for o in oracle if o['id']==key+str(shift))
        elif name=='impulses':row['impulse']=impulse_metrics(y,meta)
        else:
            row['attack']=attacks(y,meta)
            input_attack=attacks(np.fromfile(ip,dtype='<f4').reshape(-1,2),meta)
            row['input_attack']=input_attack
            extra=np.asarray(row['attack']['attack_lags_ms'])-np.asarray(input_attack['attack_lags_ms'])
            row['attack']['additional_peak_lag_ms_median']=float(np.median(extra))
            row['attack']['additional_peak_lag_ms_max']=float(max(extra))
        if not a.matrix:wavfile.write(audio/(uid+'.wav'),rate,y)
        rows.append(row)
        report={'status':'RESEARCH_ONLY_NOT_RELEASE_ACCEPTANCE','baseline_commit':'3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58',
                'platform':platform.platform(),'renderer_sha256':sha(a.renderer),'inputs':inputs,'oracle':oracle,'results':rows}
        (a.out/'results.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
        print(f'{len(rows)}/{len(configurations)} {uid}',flush=True)
    # Conservative engineering screen. These are research criteria, not a change
    # to repository release policy. Read-head age is NOT the measured latency.
    verdict={}
    for engine in engines:
        rr=[r for r in rows if r['engine']==engine]
        reasons=[]
        for r in rr:
            if 'voices' in r:
                if not r['tone_metric_valid']:
                    reasons.append('multi_voice_metric_failed_ideal_control_BLOCKED')
                    continue
                for v in r['voices']:
                    if abs(v['dominant_error_cents'])>10:reasons.append('pitch_error_gt_10_cents')
                    if v['warble_p95_p5_cents']>20:reasons.append('warble_gt_20_cents')
                    if not -1<=v['target_gain_db']<=1:reasons.append('coherent_fundamental_outside_1dB')
                    if not -1<=v['local_fundamental_band_gain_db']<=1:reasons.append('local_fundamental_outside_1dB')
            if r['nominal_samples']>r['rate']*.010:reasons.append('nominal_latency_gt_10ms')
            if r['nominal_samples']<0:reasons.append('variable_delay_no_fixed_PDC_contract')
            if 'impulse' in r:
                im=r['impulse']
                if im['missing_count']:reasons.append('missing_impulse')
                if any(e.get('energy95_ms',0)>10 for e in im['events']):reasons.append('measured_energy95_gt_10ms')
                if any(e.get('smear_ms',0)>2 for e in im['events']):reasons.append('impulse_spread_gt_2ms')
            if 'attack' in r and r['attack']['additional_peak_lag_ms_max']>10:reasons.append('measured_attack_gt_10ms')
        verdict[engine]={'target_pass':False,'synthetic_screen_pass':not reasons,'reasons':sorted(set(reasons)),
                         'real_DI':'NOT_AVAILABLE','DAW_PDC_automation_recall':'NOT_RUN',
                         'controlled_realtime_CPU':'NOT_RUN'}
    (a.out/'verdict.json').write_text(json.dumps(verdict,indent=2)+'\n')

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Reproducible weak-attack study. All audio is synthetic, not DI/reference audio.

Reuses PR100's input construction and unchanged attack/tone/impulse scorers.
No event times, frequencies or labels enter the C++ renderer. The 10 ms gate
is unchanged. Delay at a high-band event is not full-band latency or host PDC.
"""
import argparse
import gzip
import json
import platform
import subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from evaluate import attacks, B0, make
from legacy_metrics import fixture, tone_metrics, impulse_metrics, sha

FREQUENCIES = {'bass_B0': B0, 'bass_dropA_A0': 27.5, 'guitar_dropA_A1': 55.0}
ENGINES = ['baseline', 'corr40-onset', 'corr40-novelty', 'corr40-split']

def generate(name, rate, instrument):
    f = FREQUENCIES[instrument]
    base = 'riff75' if name in ('alternating_velocity', 'riff75', 'riff50') else 'harmonics'
    if name == 'impulses': base = 'impulses'
    if name == 'weakfundamental':
        x, meta = make(name, rate)
    else:
        x, meta = fixture(base, rate)
    t = np.arange(len(x))/rate
    if base == 'harmonics' and (f != B0 or name in ('bright_sustain', 'tremolo', 'vibrato', 'noise_floor', 'weakfundamental')):
        phase = 2*np.pi*f*t
        if name == 'vibrato': phase += (f*.012/5)*np.sin(2*np.pi*5*t)
        if name in ('bright_sustain', 'tremolo', 'vibrato'):
            z = sum(.12/h*np.sin(h*phase+.19*h) for h in range(1,65))
        elif name == 'weakfundamental':
            z = .012*np.sin(phase)+sum(.1/h*np.sin(h*phase+.17*h) for h in range(2,9))
        else:
            z = sum(.12*a*np.sin(h*phase+.19*h) for h,a in enumerate([1,.8,.5,.3,.2,.16,.12,.1],1))
        if name == 'tremolo': z *= .55+.45*np.sin(2*np.pi*7*t)
        x = np.column_stack((z,-.65*z)).astype('<f4')
    if base == 'riff75' and (f != B0 or name == 'riff50'):
        spacing = .05 if name == 'riff50' else .075
        z = np.zeros_like(t);rng = np.random.default_rng(8)
        for start in [.4+k*spacing for k in range(16)]:
            local = t-start;mask = (local>=0)&(local<spacing*.82);u = local[mask]
            body = sum(.16*a*np.sin(2*np.pi*f*h*u+.13*h) for h,a in enumerate([1,.6,.3,.15,.1],1))
            z[mask] += (1-np.exp(-u/.001))*np.exp(-u/.025)*body
            z[mask] += .07*np.exp(-u/.002)*rng.uniform(-1,1,len(u))
        x = np.column_stack((z,-.65*z)).astype('<f4')
        meta['onsets'] = [round((.4+k*spacing)*rate) for k in range(16)]
    if name == 'alternating_velocity':
        for i,on in enumerate(meta['onsets']):
            if i%2: x[on:on+round(.075*rate)] *= .15
    if name in ('quiet_markers_over_sustain', 'quieter_markers'):
        meta['onsets'] = [round((.4+.075*i)*rate) for i in range(16)]
        rng = np.random.default_rng(318 if name == 'quiet_markers_over_sustain' else 821)
        for on in meta['onsets']:
            u = np.arange(round(.012*rate))/rate
            marker = ((.004 if name == 'quiet_markers_over_sustain' else .001)*np.exp(-u/.002)*rng.uniform(-1,1,len(u))).astype('<f4')
            x[on:on+len(u),0] += marker;x[on:on+len(u),1] -= .65*marker
    if name == 'noise_floor':
        rng = np.random.default_rng(993);noise = rng.normal(0,1.e-5,len(x)).astype('<f4')
        x[:,0] += noise;x[:,1] -= .65*noise
    meta.update(name=name, instrument=instrument, fundamental_hz=f, rate=rate, synthetic=True)
    return x,meta

def event_detection(samples, meta):
    """Report detected resets, not a universal onset detector truth score.

    Startup (<10ms) is excluded on continuous controls. Associate attacks only
    within [0,10ms]; no engine input receives these offline truth locations.
    """
    rate=meta['rate'];truth=meta.get('onsets',[]);matched=set();false=[];lags=[]
    for sample in samples:
        if not truth and sample<round(.01*rate):continue
        if truth and sample<round(.01*rate) and 'markers' in meta['name']:continue
        candidates=[(i,on) for i,on in enumerate(truth) if i not in matched and 0<=sample-on<=rate*.01]
        if candidates:
            i,on=candidates[0];matched.add(i);lags.append((sample-on)*1000/rate)
        else:false.append(sample)
    return {'expected_events':len(truth),'matched_within_10ms':len(matched),
            'unmatched_resets':false,'missed_events':len(truth)-len(matched),
            'detector_lags_ms':lags}

def main():
    p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--suite',choices=['detail','matrix','guards','timing'],required=True)
    a=p.parse_args();a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True)
    work=a.out/'work';work.mkdir(exist_ok=True);cb=a.out/'callbacks';cb.mkdir(exist_ok=True)
    configs=[]
    if a.suite=='detail':
        for instrument in FREQUENCIES:
            for name in ['harmonics','weakfundamental','riff75','alternating_velocity','quiet_markers_over_sustain','impulses']:
                for e in ENGINES:configs.append((name,instrument,48000,128,-2,e,0))
    elif a.suite=='matrix':
        for instrument in FREQUENCIES:
            for rate in [44100,48000,96000]:
                for block in [64,128,256]:
                    for shift in [-2,-1,1,2]:
                        for name in ['harmonics','alternating_velocity','quiet_markers_over_sustain']:
                            for e in ['baseline','corr40-onset','corr40-split']:configs.append((name,instrument,rate,block,shift,e,0))
    elif a.suite=='guards':
        for instrument in FREQUENCIES:
            for name in ['bright_sustain','tremolo','vibrato','noise_floor','quieter_markers','riff50']:
                for e in ['corr40-onset','corr40-novelty','corr40-split']:configs.append((name,instrument,48000,128,-2,e,0))
    else:
        for block in [64,128,256]:
            for rep in range(5):
                order=['baseline','corr40-onset','corr40-split'];order=order[rep%3:]+order[:rep%3]
                for e in order:configs.append(('timing_mix','bass_B0',48000,block,-2,e,rep))
    rows=[];inputs={};metric_cache={};oracle={}
    for num,(name,instrument,rate,block,shift,engine,rep) in enumerate(configs):
        key=f'{name}-{instrument}-{rate}';uid=f'{key}-{block}-{shift}-{engine}-{rep}'
        ip=work/(key+'.f32')
        if key not in inputs:
            if name=='timing_mix':
                x,m=generate('alternating_velocity',rate,instrument);z,_=generate('quiet_markers_over_sustain',rate,instrument)
                x=np.tile(np.concatenate([x,z]),(5,1));m.update(name=name);m.pop('onsets',None)
            else:x,m=generate(name,rate,instrument)
            x.tofile(ip);inputs[key]={**m,'sha256':sha(ip),'samples':len(x)}
            if 'onsets' in m and name!='impulses':inputs[key]['input_attack']=attacks(x,m)
        meta=inputs[key];op=work/'output.f32';cp=work/'callbacks.csv'
        command=[str(a.renderer),str(ip),str(op),str(rate),str(block),str(shift),engine,str(cp)]
        if a.suite=='timing':command+=['cpu']
        r=subprocess.run(command,check=True,capture_output=True,text=True)
        row={'input':key,'engine':engine,'rate':rate,'block':block,'shift':shift,'repetition':rep,**json.loads(r.stdout),'output_sha256':sha(op)}
        compressed=cb/(uid+'.csv.gz');compressed.write_bytes(gzip.compress(cp.read_bytes(),mtime=0));row['callback_sha256']=sha(compressed)
        y=np.fromfile(op,dtype='<f4').reshape(-1,2)
        row['stereo_null_max']=float(np.max(np.abs(y[:,1]+.65*y[:,0])))
        if name!='timing_mix':
            row['onset_association']=event_detection(row['onset_samples'],meta)
            if 'input_attack' in meta:
                am=attacks(y,meta);extra=np.array(am['attack_lags_ms'])-np.array(meta['input_attack']['attack_lags_ms'])
                row['attack_metric_valid']=name!='riff50'  # original 70ms search overlaps 50ms events
                row['attack']={**am,'additional_lags_ms':extra.tolist(),'additional_max_ms':float(max(extra)),'additional_min_ms':float(min(extra))}
            if name=='impulses':row['impulse']=impulse_metrics(y,meta)
            elif name not in ['riff75','riff50','alternating_velocity']:
                f=meta['fundamental_hz']*2**(shift/12);amp=.012 if name=='weakfundamental' else .12
                mk=(row['output_sha256'],rate,f,amp)
                if mk not in metric_cache:metric_cache[mk]=tone_metrics(y,rate,f,amp)
                row['tone']=metric_cache[mk]
                ok=f'{rate}-{f}-{amp}'
                if ok not in oracle:
                    t=np.arange(3*rate)/rate;z=amp*np.sin(2*np.pi*f*t);ideal=np.column_stack((z,-.65*z))
                    om=tone_metrics(ideal,rate,f,amp)
                    oracle[ok]={'tone':om,'valid':abs(om['dominant_error_cents'])<1 and abs(om['target_gain_db'])<.1 and om['warble_p95_p5_cents']<2}
                row['ideal_tone_control_valid']=oracle[ok]['valid']
                row['steady_tone_screen_applicable']=name not in ('tremolo','vibrato')
        else:
            data=np.genfromtxt(cp,delimiter=',',names=True)
            cpu=data['thread_cpu_us'];wall=data['microseconds'];budget=data['samples']/rate*1e6
            row['timing_detail']={'thread_cpu_p99_us':float(np.quantile(cpu,.99)), 'thread_cpu_peak_us':float(max(cpu)),
                'thread_cpu_deadline_misses':int(np.sum(cpu>budget)), 'wall_deadline_misses':int(np.sum(wall>budget)),
                'wall_only_deadline_misses':int(np.sum((wall>budget)&(cpu<=budget)))}
        if a.suite=='detail' and name=='quiet_markers_over_sustain' and instrument=='bass_B0':
            audio=a.out/'audio';audio.mkdir(exist_ok=True)
            wavfile.write(audio/(uid+'.wav'),rate,y)
            if engine=='baseline':wavfile.write(audio/(key+'-input.wav'),rate,np.fromfile(ip,dtype='<f4').reshape(-1,2))
        rows.append(row)
        report={'status':'SYNTHETIC_RESEARCH_ONLY_TARGET_NOT_ACCEPTED','base_commit':'6ec78c475311827ccfd1949c9ae9ce364f79c0b4',
            'renderer_sha256':sha(a.renderer),'platform':platform.platform(),'suite':a.suite,
            'real_DI':'NOT_AVAILABLE','Neural_DSP_output':'NOT_AVAILABLE','fixed_PDC':'NOT_IMPLEMENTED','actual_DAW':'NOT_RUN',
            'inputs':inputs,'ideal_controls':oracle,'results':rows}
        (a.out/'results.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
        if num%12==0 or num+1==len(configs):print(f'{a.suite} {num+1}/{len(configs)} {uid}',flush=True)
    groups={}
    for row in rows:
        k=f"{row['input']}/{row['shift']}/{row['engine']}"
        groups.setdefault(k,[]).append(row)
    parity=[{'group':k,'blocks':[r['block'] for r in rs],'identical':len({r['output_sha256'] for r in rs})==1}
            for k,rs in groups.items() if len({r['block'] for r in rs})>1]
    (a.out/'partition_parity.json').write_text(json.dumps(parity,indent=2)+'\n')

if __name__=='__main__':main()

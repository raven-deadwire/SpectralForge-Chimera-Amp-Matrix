#!/usr/bin/env python3
"""Exploratory diagnostics, not a new gate. All inputs are synthetic.

Preset ablations: read floor .75/1/1.25/1.5 ms, or body splice fades 4/8 ms.
Original T1-T4 scorers and limits are imported unchanged.
"""
import argparse,gzip,json,subprocess,sys,hashlib
from pathlib import Path
import numpy as np
from scipy import signal
from weak_attack_study import generate,FREQUENCIES
from latency_targets import long_notes
from weak_attack_holdout import lowband_extra

ENGINES=['corr40-split-v3','corr40-split-v4','env-floor075','env-floor100','env-floor125','env-floor150','env-fade4','env-fade8']
def envelope(y,f,rate):
    sos=signal.butter(2,[f*.7,f*1.3],btype='band',fs=rate,output='sos')
    return np.abs(signal.hilbert(signal.sosfiltfilt(sos,np.asarray(y,dtype=float))))
def dips(env,on,rate):
    rows=[]
    for o,nx in zip(on,on[1:]+[len(env)]):
        start=o+int(.005*rate);end=min(nx,o+int(.060*rate));seg=env[start:end]
        future=np.maximum.accumulate(seg[::-1])[::-1];i=int(np.argmax(future/np.maximum(seg,1e-12)))
        j=i+int(np.argmax(seg[i:]))
        rows.append({'dip_db':float(20*np.log10(future[i]/max(seg[i],1e-12))),
                     'trough_ms':(start+i-o)*1000/rate,'future_peak_ms':(start+j-o)*1000/rate,
                     'trough':float(seg[i]),'future_peak':float(seg[j])})
    return rows
def render(binary,x,rate,shift,engine,work):
    x.tofile(work/'in.f32')
    r=subprocess.run([str(binary),str(work/'in.f32'),str(work/'out.f32'),str(rate),'128',str(shift),engine,str(work/'cb.csv')],check=True,capture_output=True,text=True)
    return np.fromfile(work/'out.f32',dtype='<f4').reshape(-1,2),json.loads(r.stdout)
def main():
    p=argparse.ArgumentParser();p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--engines',default=','.join(ENGINES));p.add_argument('--trace',action='store_true');a=p.parse_args()
    a.bin=a.bin.resolve();a.out.mkdir(parents=True,exist_ok=True);w=a.out/'work';w.mkdir(exist_ok=True)
    rows=[]
    for rate in [44100,48000,96000]:
        for inst,f in FREQUENCIES.items():
            for kind in ['alternating_velocity','riff75','long1414']:
                if kind=='long1414':x,on=long_notes(f,rate,seed=1414)
                else:x,m=generate(kind,rate,inst);on=m['onsets']
                ind=dips(envelope(x[:,0],f,rate),on,rate)
                for shift in [1,2]:
                    for engine in a.engines.split(','):
                        y,native=render(a.bin/'EnvelopeProbe',x,rate,shift,engine,w)
                        od=dips(envelope(y[:,0],f*2**(shift/12),rate),on,rate)
                        rows.append({'rate':rate,'input':inst+'/'+kind,'shift':shift,'engine':engine,
                                     'lowband_additional_ms':lowband_extra(x,y,on,rate),
                                     'dips':od,'input_dips':ind,'extra_dip_db':[q['dip_db']-r['dip_db'] for q,r in zip(od,ind)],'native':native,
                                     'output_sha256':hashlib.sha256(y.tobytes()).hexdigest()})
                        if a.trace and rate==48000 and shift==2 and ((inst=='guitar_dropA_A1' and kind=='alternating_velocity')or(inst=='bass_B0' and kind=='long1414')):
                            stem=inst+'-'+kind+'-'+engine
                            np.savez_compressed(a.out/(stem+'.npz'),x=x,y=y,onsets=on,rate=rate)
                            with (a.out/(stem+'.csv')).open('w') as out:
                                subprocess.run([str(a.bin/'EnvelopeTrace'),str(w/'in.f32'),str(rate),str(shift),engine],stdout=out,check=True)
                            raw=(a.out/(stem+'.csv')).read_bytes();(a.out/(stem+'.csv.gz')).write_bytes(gzip.compress(raw,mtime=0));(a.out/(stem+'.csv')).unlink()
                print(rate,inst,kind,flush=True)
    with gzip.open(a.out/'exploration.json.gz','wt') as out:json.dump({'synthetic_only':True,'rows':rows},out)
    for engine in a.engines.split(','):
        rs=[r for r in rows if r['engine']==engine];d=[v for r in rs if r['rate']==48000 for v in r['extra_dip_db']]
        print(engine,{'dip_p90':float(np.quantile(d,.9)),'dip_max':max(d),'B0_max':max(v for r in rs if r['input']=='bass_B0/long1414' for v in r['lowband_additional_ms'])},flush=True)
if __name__=='__main__':main()

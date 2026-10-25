import sys,json,subprocess,tempfile,numpy as np
sys.path.insert(0,'Tools/TransposeNext')  # run from the repository root
from pathlib import Path
from scipy import signal
from evaluate import attacks
from weak_attack_holdout import holdout_input
from weak_attack_study import FREQUENCIES
P=sys.argv[1]
SEEDS=[8139,20261024,31415,27182,16180,14142,17320,22360,26457,31623,36056]
def env(y,rate):
    hi=signal.sosfiltfilt(signal.butter(3,[1000,8000],fs=rate,btype='bandpass',output='sos'),y[:,0].astype(float))
    w=round(.002*rate);return np.sqrt(signal.convolve(hi*hi,np.ones(w)/w,mode='same'))
ev=[];floor=[]
with tempfile.TemporaryDirectory() as w:
    ip=Path(w)/'i.f32';op=Path(w)/'o.f32'
    for rate in (44100,48000,96000):
        for inst,f in FREQUENCIES.items():
            # Marker-free sustain (same 12 harmonics): does the engine raise the 1-8 kHz floor?
            t=np.arange(3*rate)/rate;z=sum(.12/h*np.sin(2*np.pi*f*h*t+.19*h) for h in range(1,13));xs=np.column_stack((z,-.65*z)).astype('<f4')
            xs.tofile(ip);es=env(xs,rate)[round(.5*rate):-round(.3*rate)]
            for sh in (-2,-1,1,2):
                subprocess.run([P,str(ip),str(op),str(rate),'128',str(sh),'corr40-split-v3',str(Path(w)/'c.csv')],check=True,capture_output=True)
                eo=env(np.fromfile(op,'<f4').reshape(-1,2),rate)[round(.5*rate):-round(.3*rate)]
                floor.append({'rate':rate,'inst':inst,'shift':sh,'input_floor_max':float(es.max()),'output_floor_max':float(eo.max()),'ratio_db':round(20*np.log10(eo.max()/es.max()),1)})
            for seed in SEEDS:
                x,on=holdout_input(f,'random_markers',seed,rate);meta={'rate':rate,'onsets':on,'name':'random_markers'};x.tofile(ip)
                ei=env(x,rate);a_in=attacks(x,meta)['attack_lags_ms']
                snr=[20*np.log10(ei[o:o+round(.01*rate)].max()/max(ei[max(0,o-round(.02*rate)):o].max(),1e-12)) for o in on]
                for sh in (-2,-1,1,2):
                    subprocess.run([P,str(ip),str(op),str(rate),'128',str(sh),'corr40-split-v3',str(Path(w)/'c.csv')],check=True,capture_output=True)
                    a_out=attacks(np.fromfile(op,'<f4').reshape(-1,2),meta)['attack_lags_ms']
                    for k in range(len(on)):ev.append({'rate':rate,'inst':inst,'seed':seed,'shift':sh,'snr_db':snr[k],'in_lag':a_in[k],'added':a_out[k]-a_in[k],'input_valid':max(a_in)<=10})
            print(rate,inst,flush=True)
json.dump({'events':ev,'floor':floor},open(sys.argv[2],'w'))
bins=[(-99,3),(3,6),(6,10),(10,20),(20,99)]
print('SNR bin | events | in_lag>10ms | |added-2.5|>3.5ms | max |added-2.5|')
for lo,hi in bins:
    s=[e for e in ev if lo<=e['snr_db']<hi]
    if s:print(f'{lo:>4}..{hi:<3} | {len(s):5d} | {sum(e["in_lag"]>10 for e in s):4d} | {sum(abs(e["added"]-2.5)>3.5 for e in s):4d} | {max(abs(e["added"]-2.5) for e in s):6.2f}')
print('floor ratio (output/input max 1-8k env on sustain) by shift:')
for sh in (-2,-1,1,2):
    r=[x['ratio_db'] for x in floor if x['shift']==sh];print(sh,min(r),max(r))

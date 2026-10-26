#!/usr/bin/env python3
"""Export scientific comparison figures from native renders, with no alignment tricks."""
import argparse
from pathlib import Path
import numpy as np
from scipy import signal
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

p=argparse.ArgumentParser();p.add_argument('--detail',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();rate=48000
def read(name):return np.fromfile(a.detail/'audio'/name,dtype='<f4').reshape(-1,2)[:,0].astype(float)
fig,ax=plt.subplots(2,1,figsize=(11,7),constrained_layout=True)
for engine,color in [('baseline','#dc684a'),('corr40','#7489a5'),('corr40-onset','#109786')]:
    x=read('harmonics-48000-128--2-'+engine+'.f32')[rate:rate*3]
    w=signal.windows.hann(len(x),sym=False)
    sp=np.abs(np.fft.rfft(x*w,n=rate*16))*2/w.sum()
    hz=np.fft.rfftfreq(rate*16,1/rate)
    mask=(hz>=20)&(hz<=75)
    ax[0].plot(hz[mask],20*np.log10(np.maximum(sp[mask],1.e-8)),label=engine,color=color)
    x=read('riff75-48000-128--2-'+engine+'.f32')
    hi=signal.sosfiltfilt(signal.butter(3,[1000,8000],btype='bandpass',fs=rate,output='sos'),x)
    env=np.sqrt(signal.convolve(hi*hi,np.ones(96)/96,mode='same'))
    t=np.arange(len(env))/rate
    mask=(t>=.39)&(t<=.72)
    # Individual scaling is explicitly limited to the timing panel. Spectrum
    # above is absolute level, so missing fundamental cannot be normalized away.
    ax[1].plot(t[mask]*1000,env[mask]/max(env[mask].max(),1.e-15),label=engine,color=color)
ax[0].axvline(27.5,color='black',linestyle=':',linewidth=1,label='A0 target (27.5 Hz)')
ax[0].set(xlabel='Frequency (Hz)',ylabel='Amplitude (dBFS)',ylim=(-100,-10),title='B0 harmonic sustain, -2 semitones: absolute low-frequency spectrum')
ax[0].legend(loc='upper right')
for i in range(5):ax[1].axvline(400+i*75,color='black',alpha=.3,linestyle=':')
ax[1].set(xlabel='Time from input start (ms)',ylabel='Normalized high-band envelope',title='75 ms plucks: original input timeline; no delay compensation')
ax[1].legend(loc='upper right')
for x in ax:x.grid(alpha=.2)
fig.suptitle('Synthetic diagnostics only — variable-delay candidates do not meet the full 10 ms target',fontsize=12)
a.out.parent.mkdir(parents=True,exist_ok=True);fig.savefig(a.out,dpi=160)

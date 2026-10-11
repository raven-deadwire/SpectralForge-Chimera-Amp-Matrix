#!/usr/bin/env python3
"""Plot diagnostic raw renders without changing their measured gain.

Usage: plot.py AUDIO_WORK_DIR OUTPUT_PNG
"""
import sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import signal
from scipy.io import wavfile

work, target = map(Path,sys.argv[1:])
colors={"baseline":"#dc613c","relative":"#248591","signalsmith-matched":"#7255af"}
labels={"baseline":"Current core","relative":"Relative-phase prototype","signalsmith-matched":"Signalsmith, matched window"}
def read(name,algorithm):
    rate,y=wavfile.read(work/f"{name}-{algorithm}-off--36-60-15.wav")
    return rate,y[:,0].astype(float)
plt.rcParams.update({"font.size":10,"axes.spines.top":False,"axes.spines.right":False})
fig,axes=plt.subplots(3,1,figsize=(10,10),layout="constrained")
fig.suptitle("Chimera Transpose research | 48 kHz, 128 samples, -2 semitones\nSynthetic inputs; no instrument-DI acceptance",fontsize=14,weight="bold")
for alg,col in colors.items():
    rate,x=read("harmonics",alg)
    z=x[int(.75*rate):int(2.75*rate)];w=signal.windows.hann(len(z),sym=False)
    hz=np.fft.rfftfreq(len(z),1/rate);amplitude=2*np.abs(np.fft.rfft(z*w))/w.sum()
    axes[0].plot(hz,20*np.log10(np.maximum(amplitude/.12,1e-7)),label=labels[alg],color=col,lw=1.5)
    rate,x=read("impulses",alg)
    onset=int(.4*rate);delay=2048
    lo=onset+delay-int(.055*rate);hi=onset+delay+int(.055*rate)
    # Cumulative energy is readable without hiding low-level smear behind a sharp peak.
    energy=x[lo:hi]**2;energy=np.cumsum(energy)/np.sum(energy)
    axes[1].plot((np.arange(lo,hi)-onset-delay)/rate*1000,energy,color=col,lw=1.7)
    rate,x=read("riff75",alg)
    env=np.sqrt(np.convolve(x*x,np.ones(240)/240,"same"))
    times=(np.arange(len(x))-2048)/rate
    axes[2].plot(times,env,color=col,lw=1.3)
for h in (1,2,3):axes[0].axvline(27.5*h,color="#9b9b9b",ls=":",lw=1)
axes[0].set(xlim=(18,100),ylim=(-65,5),ylabel="Amplitude / input fundamental (dB)",title="Harmonic bass: target-frequency lines at 27.5 / 55 / 82.5 Hz")
axes[0].legend(loc="lower right",fontsize=9)
axes[1].set(xlim=(-45,45),ylim=(0,1),ylabel="Cumulative impulse energy",xlabel="Time relative to nominal 42.67 ms delay (ms)",title="Impulse: sharper attacks do not establish pitch accuracy")
axes[1].axhline(.05,color="#b0b0b0",ls=":");axes[1].axhline(.95,color="#b0b0b0",ls=":")
axes[2].set(xlim=(.38,.78),ylabel="5 ms RMS envelope",xlabel="Input time, nominal delay removed (s)",title="75 ms repeated synthetic attacks | original output levels")
for a in axes:a.grid(alpha=.18)
target.parent.mkdir(parents=True,exist_ok=True)
fig.savefig(target,dpi=160)

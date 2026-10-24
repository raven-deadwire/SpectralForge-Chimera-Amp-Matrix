"""Shared deterministic synthetic excitation and provenance helpers."""
import hashlib, json
from pathlib import Path
import numpy as np
from scipy.signal import butter, sosfilt
RATE=48000
SOURCE='d93560445c78552768a0ed592e5ec62c3c648fb6'

def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write_json(p, data): Path(p).write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n')

def signal(seed, seconds):
    rng=np.random.default_rng(seed); n=round(seconds*RATE); x=np.zeros(n)
    # Plucked harmonic strings, noise bursts, multitone and long-decay probes.
    pos=0
    while pos<n:
        length=min(n-pos, int(RATE*rng.uniform(.18,1.1)))
        t=np.arange(length)/RATE; f=rng.choice([27.5,30.8677,41.2034,55.,65.406,82.407,110.,146.832,220.,329.628])
        y=np.zeros(length)
        for k in range(1,min(70,int(9000/f))):
            y+=np.sin(2*np.pi*f*k*t+rng.uniform(-.4,.4))*np.exp(-t*(rng.uniform(1,5)+k*.12))/k**rng.uniform(1,1.7)
        y+=rng.normal(0,.12,length)*np.exp(-t*80)
        y*=np.minimum(t/.0015,1)
        y/=max(1e-8,np.max(np.abs(y)))
        y*=10**(rng.choice([-30.,-24.,-18.,-12.,-6.,-3.])/20)
        x[pos:pos+length]=y
        pos+=length+int(RATE*rng.uniform(.015,.10))
    # Broader excitation in a dedicated quarter, with input-level variation.
    a=n//3;b=n//2
    noise=sosfilt(butter(2,[30,14000],btype='bandpass',fs=RATE,output='sos'),rng.normal(size=b-a))
    noise/=max(1e-8,np.max(np.abs(noise)))
    x[a:b]=noise*np.linspace(.03,.55,b-a)
    # True silence, and a tail long enough to inspect state decay.
    x[:RATE//4]=0;x[-RATE//2:]=0
    return x.astype('<f4')


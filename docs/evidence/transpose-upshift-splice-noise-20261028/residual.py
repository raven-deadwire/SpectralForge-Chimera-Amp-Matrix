# Bright sustain: harmonics up to 8 kHz plus optional noise. Compare 1-8 kHz energy that is NOT at
# the ideal shifted harmonics (inter-harmonic residual) between output and the ideal shifted tone.
import sys,json,subprocess,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[3]/'Tools'/'TransposeNext'))
from scipy import signal
from weak_attack_study import FREQUENCIES
TR=sys.argv[1]; W=sys.argv[2]; rates=list(map(int,sys.argv[3].split(','))); TILTS=[float(v) for v in sys.argv[4].split(',')]; OUT=sys.argv[5]; ENG=sys.argv[6] if len(sys.argv)>6 else 'v3'
def tone(f,rate,secs=3,top=8000,tilt=1.0):
    t=np.arange(secs*rate)/rate; H=int(top//f)
    return sum(.12/h**tilt*np.sin(2*np.pi*f*h*t+.19*h*h) for h in range(1,H+1))
def interharm(y,f,rate,lo=1000,hi=8000):
    # long Hann-window PSD; energy within +-f/4 of k*f is harmonic, rest residual
    n=len(y); F,P=signal.welch(y,rate,nperseg=2**15,noverlap=2**14)
    band=(F>=lo)&(F<=hi); k=np.round(F/f); near=np.abs(F-k*f)<=f/4
    return P[band&near].sum(),P[band&~near].sum()
rows=[]
for rate in rates:
  for inst,f in FREQUENCIES.items():
    for tilt in TILTS:
      z=tone(f,rate,tilt=tilt); x=np.column_stack((z,-.65*z)).astype('<f4'); x.tofile(W+'/i.f32')
      a,b=round(.5*rate),len(z)-round(.3*rate)
      for sh in (-2,-1,1,2):
        r=subprocess.run([TR,W+'/i.f32',W+'/o.f32',str(rate),str(sh),ENG],check=True,capture_output=True,text=True)
        y=np.fromfile(W+'/o.f32','<f4').reshape(-1,2)[a:b,0].astype(float)
        fi=f*2**(sh/12); ideal=tone(fi,rate,tilt=tilt)[a:b]
        hy,ry=interharm(y,fi,rate); hi_,ri=interharm(ideal,fi,rate)
        rows.append({'rate':rate,'inst':inst,'tilt':tilt,'shift':sh,'harm_out_vs_ideal_db':10*np.log10(hy/hi_),
                     'residual_db_re_harm':10*np.log10(ry/hy),'ideal_residual_db_re_harm':10*np.log10(ri/hi_),
                     'splices':len(json.loads(r.stdout)['transient_splices'])})
        q=rows[-1];print(rate,inst,tilt,sh,'harm %.1f'%q['harm_out_vs_ideal_db'],'resid %.1f (ideal %.1f)'%(q['residual_db_re_harm'],q['ideal_residual_db_re_harm']),q['splices'],flush=True)
json.dump(rows,open(OUT,'w'),indent=1)

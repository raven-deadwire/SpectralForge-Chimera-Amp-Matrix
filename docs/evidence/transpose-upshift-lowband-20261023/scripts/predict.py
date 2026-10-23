"""Low-band extrapolation accuracy vs horizon (past samples only)."""
import sys,json;sys.path.insert(0,'/home/user/SpectralForge-Chimera-Amp-Matrix/Tools/TransposeNext')
import numpy as np
from scipy import signal
from scipy.linalg import solve_toeplitz
from weak_attack_study import generate
rate=48000;D=12;fd=rate//D
sos=signal.butter(2,350,fs=rate,output='sos');sos=np.vstack([sos,sos])
def fit(seg,order):
  w=seg*np.hanning(len(seg));r=np.correlate(w,w,'full')[len(w)-1:len(w)+order];r[0]*=1.0001
  return solve_toeplitz(r[:-1],r[1:])
def extrap(hist,a,n):
  buf=list(hist[-len(a):]);out=[]
  for _ in range(n):
    v=float(np.dot(a,buf[::-1][:len(a)]));out.append(v);buf.append(v)
  return np.array(out)
def fit_cov(seg,order):
  # Covariance (least-squares) method: no window, unbiased pole radii.
  X=np.lib.stride_tricks.sliding_window_view(seg[:-1],order)[:, ::-1];return np.linalg.lstsq(X,seg[order:],rcond=None)[0]
cases=[]
for inst in ['bass_B0','bass_dropA_A0','guitar_dropA_A1']:
  for n in ['harmonics','riff75','alternating_velocity']:
    x,m=generate(n,rate,inst);cases.append((inst,n,x,m.get('onsets',[])))
if __name__=='__main__':
  for method,fitter,orders,wins in [('autocorrelation',fit,[10,16,24],[.032,.048]),('covariance',fit_cov,[12,24,32],[.024,.048])]:
    for kind in ['harmonics','riff75','alternating_velocity']:
      for order in orders:
        for win in wins:
          rng=np.random.default_rng(5);acc={h:[] for h in [1,2,4,6,8]}
          for inst,n,x,on in cases:
            if n!=kind:continue
            y=signal.sosfilt(sos,x.astype(float),axis=0).mean(1)[::D];W=int(win*fd)
            times=[o//D+int(rng.uniform(.012,.050)*fd) for o in on] if on else list(rng.integers(int(.5*fd),int(2.5*fd),16))
            for t in times:
              if t<W:continue
              a=fitter(y[t-W:t],order);pred=extrap(y[:t],a,int(.010*fd))
              for h in acc:
                k=int(h*fd/1000);true=y[t:t+k];e=pred[:k]-true;acc[h].append(10*np.log10(np.sum(true**2)/max(np.sum(e**2),1e-30)))
          print(method,kind,'order',order,'window_ms',int(win*1000),'SNR dB median/p10 by horizon ms',{h:(round(float(np.median(v)),1),round(float(np.percentile(v,10)),1)) for h,v in acc.items()},flush=True)

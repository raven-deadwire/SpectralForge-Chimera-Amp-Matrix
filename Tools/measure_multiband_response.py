#!/usr/bin/env python3
"""Reproducible offline audit of production MultibandDynamicsCore.h.
Requires a C++20 compiler, numpy and matplotlib; never runs FabFilter.
"""
import argparse, hashlib, json, math, platform, subprocess, tempfile
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parents[1]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--cxx',default='g++');args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='chimera-mb-audit-') as tmp:
        exe=Path(tmp)/'probe'
        subprocess.run([args.cxx,'-O2','-std=c++20','-I'+str(ROOT/'Source'),str(ROOT/'Tools/multiband_response_probe.cpp'),'-o',str(exe)],check=True)
        def run(*a):return np.frombuffer(subprocess.check_output([str(exe),*map(str,a)]),dtype=np.float32).astype(float)
        rows=[]; saved=None
        rates=[44100,48000,96000,192000]
        splits=[[120,700,3500],[40,56,78.4],[1000,1400,1960],[7000,10000,16000]]
        for sr in rates:
            for requested_cross in splits:
                cross = list(map(float, np.asarray(requested_cross, dtype=np.float32)))
                h=run('impulse',sr,*cross,1,0,0,0,0)
                H=np.fft.rfft(h);f=np.fft.rfftfreq(len(h),1/sr);mask=(f>=20)&(f<=min(20000,.45*sr))
                phase=np.unwrap(np.angle(H));gd=-np.gradient(phase,2*np.pi*f)*1000
                # Independent bilinear-transform LR4 response, not the SVF code.
                z=np.exp(2j*np.pi*f/sr);q=(1-1/z)/(1+1/z+1e-300)
                LP=[];HP=[];AP=[]
                for fc in cross:
                    u=q/np.tan(np.pi*fc/sr);den=1+np.sqrt(2)*u+u*u
                    l=1/den**2;r=u**4/den**2;LP.append(l);HP.append(r);AP.append(l+r)
                bands=np.array([AP[2]*AP[1]*LP[0],AP[2]*LP[1]*HP[0],LP[2]*HP[1]*HP[0],HP[2]*HP[1]*HP[0]])
                analytic=bands.sum(axis=0)
                worst_mix=0.;worst_weighted=0.
                for mix in [0,.25,.5,.75,1]:
                    for gains in [[-6]*4,[-6,3,-9,6]]:
                        mixed=np.fft.rfft(run('impulse',sr,*cross,mix,*gains))
                        weights=1+mix*(10**(np.array(gains)/20)-1)
                        expected=(bands*weights[:,None]).sum(axis=0)
                        error=float(np.max(np.abs(20*np.log10(np.abs(mixed[mask]/expected[mask])))))
                        if len(set(gains))==1:worst_mix=max(worst_mix,error)
                        else:worst_weighted=max(worst_weighted,error)
                row=dict(rate=sr,crossovers=cross,magnitude_error_db=float(np.max(np.abs(20*np.log10(np.abs(H[mask]))))),energy_error=float(abs(h@h-1)),complex_error=float(np.max(abs(H[mask]-analytic[mask]))),uniform_mix_error_db=worst_mix,weighted_mix_error_db=worst_weighted,group_delay_max_ms=float(max(gd[mask])),first_nonzero_sample=int(np.flatnonzero(h)[0]),raw_impulse_residual_l2=float(np.linalg.norm(h-np.eye(1,len(h))[0])))
                rows.append(row)
                if sr==48000 and requested_cross==splits[0]:saved=(h,f,H,phase,gd)
        # Exact-frequency DFT / local central phase derivative (not nearest FFT bin).
        h,f,H,phase,gd=saved;n=np.arange(len(h));points=[]
        def at(hz):return h@np.exp(-2j*np.pi*hz*n/48000)
        for hz in [31,40,80,120,300,700,1000,3500,10000]:
            a=at(hz);delay=-np.angle(at(hz+.01)/at(hz-.01))/(2*np.pi*.02)*1000
            points.append(dict(hz=hz,magnitude_db=float(20*np.log10(abs(a))),phase_deg=float(np.angle(a)*180/np.pi),group_delay_ms=float(delay),external_half_dry_db=float(20*np.log10(abs((1+a)*.5)))))
        transitions=[];curves={}
        for sr in rates:
            for hz in [40,80,120,700,3500,10000]:
                for mode in ['on','off','band_off']:
                    out=run(mode,sr,hz).reshape(2,-1);env=np.hypot(*out)/.25
                    ramp=math.ceil(sr*.01);window=env[:ramp]
                    row=dict(rate=sr,hz=hz,mode=mode,transition_samples=ramp,min_envelope_db=float(20*np.log10(max(min(window),1e-30))),max_envelope_db=float(20*np.log10(max(window))),time_below_minus3_ms=float(np.count_nonzero(window<10**(-3/20))/sr*1000),post_transition_max_abs_db=float(max(abs(20*np.log10(np.maximum(env[ramp:],1e-30))))))
                    transitions.append(row)
                    if sr==48000 and hz in [120,700,3500] and mode=='off':curves[hz]=env
        timing=[];timing_curves={}
        for detector in [0,1]:
            g=run('timing',48000,detector);steady=float(np.mean(g[62400:72000]));attack=g[24000:48000];release=g[72000:]
            def reach(a,condition):
                idx=np.flatnonzero(condition(a));return float((idx[0]+1)/48) if len(idx) else None
            timing.append(dict(detector=['peak','rms'][detector],steady_gr_db=steady,attack63_ms=reach(attack,lambda a:a>=steady*(1-np.exp(-1))),attack90_ms=reach(attack,lambda a:a>=steady*.9),release63_ms=reach(release,lambda a:a<=steady*np.exp(-1)),release90_ms=reach(release,lambda a:a<=steady*.1)))
            timing_curves[detector]=g
        result=dict(source_sha=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),core_sha256=hashlib.sha256((ROOT/'Source/MultibandDynamicsCore.h').read_bytes()).hexdigest(),platform=platform.platform(),compiler=subprocess.check_output([args.cxx,'--version'],text=True).splitlines()[0],numpy_version=np.__version__,byte_order=__import__('sys').byteorder,core_matches_committed_source=subprocess.check_output(['git','show','HEAD:Source/MultibandDynamicsCore.h'],cwd=ROOT)==(ROOT/'Source/MultibandDynamicsCore.h').read_bytes(),probe_sha256=hashlib.sha256((ROOT/'Tools/multiband_response_probe.cpp').read_bytes()).hexdigest(),runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),pro_mb_binary_tested=False,impulse_cases=rows,default_48k_points=points,transitions=transitions,timing=timing)
        (args.output/'measurements.json').write_text(json.dumps(result,indent=2)+'\n')
        import matplotlib;matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig,axes=plt.subplots(2,2,figsize=(12,8),layout='constrained')
        mask=(f>=20)&(f<=20000)
        axes[0,0].semilogx(f[mask],20*np.log10(abs(H[mask])),label='Active neutral sum')
        axes[0,0].semilogx(f[mask],20*np.log10(np.maximum(abs(.5+.5*H[mask]),1e-10)),label='External 50% raw dry (not internal mix)')
        axes[0,0].set(ylim=(-60,3),ylabel='Gain (dB)',xlabel='Hz',title='Flat magnitude does not mean raw-dry alignment');axes[0,0].legend(fontsize=8)
        axes[0,1].semilogx(f[mask],gd[mask]);axes[0,1].set(xlabel='Hz',ylabel='Group delay (ms)',title='IIR group delay; no added delay-line samples')
        for hz,env in curves.items():axes[1,0].plot(np.arange(len(env))/48,20*np.log10(np.maximum(env,1e-10)),label=f'{hz} Hz')
        axes[1,0].set(xlim=(0,15),ylim=(-45,2),xlabel='Time since OFF (ms)',ylabel='Quadrature envelope (dB)',title='Actual 10 ms global bypass transition');axes[1,0].legend()
        for detector,g in timing_curves.items():axes[1,1].plot(np.arange(len(g))/48-500,g,label=['Peak','RMS'][detector])
        axes[1,1].set(xlim=(-20,1500),xlabel='Time after level step (ms)',ylabel='Reduction (dB)',title='6 kHz burst; Attack 10 ms / Release 120 ms');axes[1,1].legend()
        for ax in axes.flat:ax.grid(alpha=.25)
        fig.savefig(args.output/'response.png',dpi=140);plt.close(fig)
        print(json.dumps(dict(worst_mag_db=max(x['magnitude_error_db'] for x in rows),worst_parallel_db=max(x['uniform_mix_error_db'] for x in rows),worst_weighted_db=max(x['weighted_mix_error_db'] for x in rows),default_48k_points=points,timing=timing,off_48k=[x for x in transitions if x['rate']==48000 and x['mode']=='off']),indent=2))
if __name__=='__main__':main()

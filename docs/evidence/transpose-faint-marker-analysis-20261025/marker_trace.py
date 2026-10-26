import sys,json,subprocess,tempfile,numpy as np
sys.path.insert(0,'Tools/TransposeNext')  # run from the repository root
from pathlib import Path
from scipy import signal
from evaluate import attacks
from weak_attack_holdout import holdout_input
T=sys.argv[1];P=sys.argv[2]
def env(y,rate):
    hi=signal.sosfiltfilt(signal.butter(3,[1000,8000],fs=rate,btype='bandpass',output='sos'),y[:,0].astype(float))
    w=round(.002*rate);return np.sqrt(signal.convolve(hi*hi,np.ones(w)/w,mode='same'))
def run(rate,shift,seed=26457,f=55.):
    x,on=holdout_input(f,'random_markers',seed,rate);meta={'rate':rate,'onsets':on,'name':'random_markers'}
    with tempfile.TemporaryDirectory() as w:
        ip=Path(w)/'i.f32';op=Path(w)/'o.f32';op2=Path(w)/'p.f32';x.tofile(ip)
        tr=json.loads(subprocess.run([T,str(ip),str(op),str(rate),str(shift),'v3'],check=True,capture_output=True,text=True).stdout)
        y=np.fromfile(op,'<f4').reshape(-1,2)
        subprocess.run([P,str(ip),str(op2),str(rate),'128',str(shift),'corr40-split-v3',str(Path(w)/'c.csv')],check=True,capture_output=True)
        same=bool(np.array_equal(y,np.fromfile(op2,'<f4').reshape(-1,2)))
    ei,eo=env(x,rate),env(y,rate);a_in=attacks(x,meta)['attack_lags_ms'];a_out=attacks(y,meta)['attack_lags_ms']
    ts=np.array(tr['transient_splices']);rows=[]
    for k,o in enumerate(on):
        pre=ei[max(0,o-round(.02*rate)):o].max();pk=ei[o:o+round(.01*rate)].max()
        po=o+round(a_out[k]*rate/1000)
        near=ts[np.argmin(np.abs(ts-po))] if len(ts) else None
        rows.append({'event':k,'marker_snr_db':round(20*np.log10(pk/max(pre,1e-12)),1),'in_lag_ms':round(a_in[k],2),'out_lag_ms':round(a_out[k],2),
                     'added_ms':round(a_out[k]-a_in[k],2),'out_peak':float(eo[po]),'in_peak':float(pk),
                     'nearest_transient_splice_ms_from_out_peak':None if near is None else round((po-near)*1000/rate,2)})
    return {'rate':rate,'shift':shift,'probe_identical':same,'transient_splices':len(ts),'body_splices':len(tr['body_splices']),'onset_resets':len(tr['onset_resets']),'events':rows}
out=[run(r,s) for r in (44100,48000) for s in (-2,1,2)]
json.dump(out,open(sys.argv[3],'w'),indent=1)
for o in out:
    print(o['rate'],o['shift'],'identical',o['probe_identical'],'tsplices',o['transient_splices'],'resets',o['onset_resets'])
    for e in o['events']:
        flag='  <<' if abs(e['added_ms']-2.5)>3.5 else ''
        print('  ev%2d snr %5.1f dB in %5.2f out %6.2f added %6.2f splice@peak %s%s'%(e['event'],e['marker_snr_db'],e['in_lag_ms'],e['out_lag_ms'],e['added_ms'],e['nearest_transient_splice_ms_from_out_peak'],flag))

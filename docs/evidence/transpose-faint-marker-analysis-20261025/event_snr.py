import sys,json,numpy as np,collections
sys.path.insert(0,'Tools/TransposeNext')  # run from the repository root
from scipy import signal
from latency_targets import inputs
from evaluate import attacks
SEEDS=(8139,20261024,31415,27182,16180,14142,17320,22360,26457,31623,36056)
def env(y,rate):
    hi=signal.sosfiltfilt(signal.butter(3,[1000,8000],fs=rate,btype='bandpass',output='sos'),y[:,0].astype(float))
    w=round(.002*rate);return np.sqrt(signal.convolve(hi*hi,np.ones(w)/w,mode='same'))
def event_snr(e,o,rate):
    # Peak within 10 ms after the event over the largest envelope 20-3 ms before it
    # (3 ms gap: the centred 2 ms window and zero-phase filter spread the attack back).
    return 20*np.log10(e[o:o+round(.01*rate)].max()/max(e[max(0,o-round(.02*rate)):max(1,o-round(.003*rate))].max(),1e-12))
if __name__=='__main__':
    stat=collections.defaultdict(list);table={}
    for rate in (44100,48000,96000):
        for key,x,meta,roles in inputs(rate,SEEDS,1):
            if 'attack' not in roles: continue
            e=env(x,rate);lags=attacks(x,meta)['attack_lags_ms']
            name=key.split('/')[1];name=name.split('-')[-1] if 'holdout' in name else name
            for k,(o,l) in enumerate(zip(meta['onsets'],lags)):
                s=event_snr(e,o,rate);stat[name].append((s,l));table[f'{rate}/{key}/{k}']=s
    print(f"{'input':28s} {'events':>6} {'SNR min':>8} {'SNR p5':>7} {'SNR med':>8} {'<10dB':>6} {'lag>10ms':>8} {'lag>10 & SNR>=10':>17}")
    for k,v in sorted(stat.items()):
        s=np.array([a for a,_ in v]);l=np.array([b for _,b in v])
        print(f"{k:28s} {len(v):6d} {s.min():8.1f} {np.percentile(s,5):7.1f} {np.median(s):8.1f} {int((s<10).sum()):6d} {int((l>10).sum()):8d} {int(((l>10)&(s>=10)).sum()):17d}")
    json.dump(table,open(sys.argv[1],'w'))

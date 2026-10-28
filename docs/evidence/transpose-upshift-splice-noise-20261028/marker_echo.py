# Re-read (echo) of marker transients after a backward splice. For each marker event, take the
# 1-8 kHz 2 ms RMS envelope; main = max within 0-15 ms after the event; echo = max within 15-60 ms
# (or until the next event - 5 ms). Echo ratio = echo/main in dB, output minus input (input has none).
import sys,json,subprocess,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[3]/'Tools'/'TransposeNext'))
from attack_snr import hf_envelope, event_snr_db
from weak_attack_study import FREQUENCIES, generate
from weak_attack_holdout import holdout_input
TR,W,OUT=sys.argv[1],sys.argv[2],sys.argv[3]; ENG=sys.argv[4]; rates=list(map(int,sys.argv[5].split(',')))
SEEDS=[8139,20261024]
rows=[]
for rate in rates:
  for inst,f in FREQUENCIES.items():
    cases=[('quiet_markers_over_sustain',)+generate('quiet_markers_over_sustain',rate,inst),('quieter_markers',)+generate('quieter_markers',rate,inst)]
    for s in SEEDS:
        x,on=holdout_input(f,'random_markers',s,rate);cases.append((f'random_markers{s}',x,{'onsets':on}))
    for name,x,meta in cases:
        on=list(meta['onsets']);x.astype('<f4').tofile(W+'/i.f32');ei=hf_envelope(x,rate)
        for sh in (-2,-1,1,2):
            subprocess.run([TR,W+'/i.f32',W+'/o.f32',str(rate),str(sh),ENG],check=True,capture_output=True)
            eo=hf_envelope(np.fromfile(W+'/o.f32','<f4').reshape(-1,2),rate)
            for k,o in enumerate(on):
                end=min(o+round(.060*rate),(on[k+1]-round(.005*rate)) if k+1<len(on) else len(eo))
                if end<=o+round(.020*rate):continue
                def ratio(e):
                    m=e[o:o+round(.015*rate)].max();q=e[o+round(.015*rate):end].max();return 20*np.log10(q/max(m,1e-12))
                rows.append({'rate':rate,'inst':inst,'input':name,'shift':sh,'event':k,'snr_db':event_snr_db(ei,o,rate),
                             'echo_in_db':ratio(ei),'echo_out_db':ratio(eo)})
    print(rate,inst,flush=True)
json.dump(rows,open(OUT,'w'))

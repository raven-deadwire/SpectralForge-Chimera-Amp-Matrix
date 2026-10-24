import sys;sys.path.insert(0,"Tools/TransposeNext")  # run from the repository root
import subprocess,json,tempfile,collections
from pathlib import Path
from weak_attack_study import FREQUENCIES,generate
from weak_attack_holdout import holdout_input
from latency_targets import long_notes
B=sys.argv[1];out=Path(sys.argv[2]);rows=[]
with tempfile.TemporaryDirectory() as w:
    ip=Path(w)/'in.f32'
    for rate in (44100,48000,96000):
        for inst,f in FREQUENCIES.items():
            ins=[(n,generate(n,rate,inst)[0]) for n in ('riff75','harmonics','bright_sustain','tremolo','vibrato','quiet_markers_over_sustain')]
            ins+=[('plucks',holdout_input(f,'random_plucks',8139,rate)[0]),('long_notes',long_notes(f,rate)[0])]
            for name,x in ins:
                x.tofile(ip)
                for sh in (-2,-1,1,2):
                    for blk in (64,128,256):
                        for e in ('v2','v3'):
                            r=subprocess.run([B,str(ip),str(rate),str(blk),str(sh),e],check=True,capture_output=True,text=True)
                            rows.append({'rate':rate,'input':f'{inst}/{name}','shift':sh,'block':blk,'engine':e,**json.loads(r.stdout)})
            print(rate,inst,flush=True)
summ={}
for e in ('v2','v3'):
    for blk in (64,128,256):
        sel=[r for r in rows if r['engine']==e and r['block']==blk]
        summ[f'{e}/block{blk}']={'callbacks':sum(r['callbacks'] for r in sel),'both_searched':sum(r['both_searched'] for r in sel),
            'body_splice_callbacks':sum(r['body_only']+r['both_searched'] for r in sel),'transient_splice_callbacks':sum(r['transient_only']+r['both_searched'] for r in sel),
            'cases_with_both':sum(r['both_searched']>0 for r in sel),'cases':len(sel)}
out.write_text(json.dumps({'summary':summ,'rows':rows},indent=1)+'\n');print(json.dumps(summ,indent=1))

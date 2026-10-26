import sys;sys.path.insert(0,"Tools/TransposeNext")  # run from the repository root
import subprocess,json,tempfile
from pathlib import Path
from weak_attack_study import FREQUENCIES,generate
from weak_attack_holdout import holdout_input
from latency_targets import long_notes
B=sys.argv[1];out=Path(sys.argv[2]);rows=[]
with tempfile.TemporaryDirectory() as w:
    ip=Path(w)/'in.f32'
    for rate in (44100,48000,96000):
        for inst,f in FREQUENCIES.items():
            ins=[('riff75',generate('riff75',rate,inst)[0]),('markers',generate('quieter_markers',rate,inst)[0]),
                 ('plucks',holdout_input(f,'random_plucks',8139,rate)[0]),('soft_after_loud',holdout_input(f,'random_soft_after_loud',20261024,rate)[0]),
                 ('long_notes',long_notes(f,rate)[0])]
            for name,x in ins:
                x.tofile(ip)
                for sh in (-2,-1,1,2):
                    for e in ('v2','v3'):
                        r=subprocess.run([B,str(ip),str(rate),str(sh),e],check=True,capture_output=True,text=True)
                        rows.append({'rate':rate,'input':f'{inst}/{name}','shift':sh,'engine':e,'max_diff':json.loads(r.stdout)})
            print(rate,inst,flush=True)
summ={}
for e in ('v2','v3'):
    sel=[r for r in rows if r['engine']==e]
    summ[e]={'cases':len(sel),'nonidentical_cases':sum(any(v>0 for v in r['max_diff'].values()) for r in sel),
             'patterns_nonzero':sum(v>0 for r in sel for v in r['max_diff'].values()),'patterns':sum(len(r['max_diff']) for r in sel),
             'max':max(v for r in sel for v in r['max_diff'].values())}
out.write_text(json.dumps({'summary':summ,'rows':rows},indent=1)+'\n');print(json.dumps(summ,indent=1))

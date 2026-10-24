import sys;sys.path.insert(0,"Tools/TransposeNext")  # run from the repository root
import subprocess,json,tempfile
from pathlib import Path
from weak_attack_study import FREQUENCIES
from weak_attack_holdout import holdout_input
from latency_targets import long_notes
B=sys.argv[1];out=Path(sys.argv[2]);rows=[]
with tempfile.TemporaryDirectory() as w:
    ip=Path(w)/'in.f32'
    for rate in (44100,48000,96000):
        for inst,f in FREQUENCIES.items():
            ins=[(f'holdout{s}-{m}',holdout_input(f,m,s,rate)[0]) for s in (14142,17320,22360) for m in ('random_plucks','random_soft_after_loud','random_markers')]
            ins.append(('long_notes_seed1414',long_notes(f,rate,seed=1414)[0]))
            for name,x in ins:
                x.tofile(ip)
                for sh in (-2,-1,1,2):
                    r=subprocess.run([B,str(ip),str(rate),str(sh),'v3'],check=True,capture_output=True,text=True)
                    rows.append({'rate':rate,'input':f'{inst}/{name}','shift':sh,'engine':'v3','max_diff':json.loads(r.stdout)})
            print(rate,inst,flush=True)
summ={'cases':len(rows),'nonidentical_cases':sum(any(v>0 for v in r['max_diff'].values()) for r in rows),
      'patterns':sum(len(r['max_diff']) for r in rows),'patterns_nonzero':sum(v>0 for r in rows for v in r['max_diff'].values()),
      'max':max(v for r in rows for v in r['max_diff'].values())}
summ['verdict']='PASS' if summ['patterns_nonzero']==0 else 'FAIL'
out.write_text(json.dumps({'summary':summ,'rows':rows},indent=1)+'\n');print(json.dumps(summ,indent=1))

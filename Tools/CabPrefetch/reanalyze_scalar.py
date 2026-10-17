import argparse, csv, json, hashlib, statistics, sys, importlib.util
from pathlib import Path
from collections import defaultdict
parser=argparse.ArgumentParser()
parser.add_argument('--artifacts',type=Path,required=True,help='Folders split, clearhead, fusedlead, tile32, each containing evidence/')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--kinds',nargs='+',choices=('split','clearhead','fusedlead','tile32','prefetch2'),default=('split','clearhead','fusedlead','tile32'))
args=parser.parse_args()
ROOT=args.artifacts.resolve()
REPO=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('ab',REPO/'Tools/CabNeonAB/run.py')
ab=importlib.util.module_from_spec(spec);spec.loader.exec_module(ab)
def eq(a,b):
    if isinstance(a,dict):
        assert a.keys()==b.keys()
        for k in a:eq(a[k],b[k])
    elif isinstance(a,list):
        assert len(a)==len(b)
        for x,y in zip(a,b):eq(x,y)
    elif isinstance(a,float):assert abs(a-b)<=1e-8+abs(a)*1e-12,(a,b)
    else:assert a==b,(a,b)
result={}
for kind in args.kinds:
    root=ROOT/kind/'evidence';report=json.loads((root/'report.json').read_text())
    manifest=json.loads((root/'manifest.json').read_text())
    for p,m in manifest.items():
        data=(root/p).read_bytes();assert len(data)==m['bytes'] and hashlib.sha256(data).hexdigest()==m['sha256'],p
    expected=[]
    for round_ in range(6):
        order='ABBA' if round_%2==0 else 'BAAB'
        for mode in (('control','profile') if round_%2==0 else ('profile','control')):
            for experiment in (('AA','AB') if round_%2==0 else ('AB','AA')):
                for slot,logical in enumerate(order):
                    expected.append(dict(round=round_,mode=mode,experiment=experiment,slot=slot,logical=logical,arm=logical if experiment=='AB' else 'A'))
    assert len(report['trials'])==len(expected)==96
    for t,e in zip(report['trials'],expected):assert all(t[k]==v for k,v in e.items())
    verified_rows=0
    stages=defaultdict(list);misses=[];summary=defaultdict(lambda:defaultdict(list))
    for t in report['trials']:
        label=f"r{t['round']}-{t['mode']}-{t['experiment']}-{t['slot']}-{t['logical']}"
        rows=[]
        for engine in ab.ENGINES:
            folder=root/'trials'/label/engine
            rows+=ab.read_trial(folder,t['mode'],engine)
            if t['experiment']=='AB':
                for line in (root/'trials'/label/(engine+'.log')).read_text().splitlines():
                    if line.startswith('PROFILE '):
                        d=dict(s.split('=',1) for s in line.split()[1:])
                        if d['block']=='32':stages[(t['arm'],engine,d['channels'],d['phase'],d['stage'])].append(float(d['mean_us']))
                if t['mode']=='control':
                    for p in folder.rglob('96000-32-*.csv'):
                        for r in csv.DictReader(p.open()):
                            if int(r['deadline_miss']):
                                misses.append(dict(arm=t['arm'],engine=engine,channels=int(r['channels']),phase=r['phase'],callback=int(r['callback_index']),wall=float(r['wall_us']),cpu=float(r['thread_cpu_us']),file=str(p.relative_to(root))))
        eq(rows,t['rows'])
        assert len(rows)==36
        verified_rows+=len(rows)
        if t['experiment']=='AB':
            for r in rows:
                if r['block']==32 and r['phase']!='all':
                    key=(t['mode'],t['arm'],r['engine'],r['channels'],r['phase'])
                    for k,v in r.items():
                        if isinstance(v,(float,int)) and k not in ('block','channels'):summary[key][k].append(v)
    analysis=ab.analyze(report['trials']);eq(analysis,json.loads((root/'analysis.json').read_text()))
    out={'verified_manifest':len(manifest),'verified_raw_summaries':verified_rows,'recomputed_decision':analysis['performance_demonstrated'],
         'report_flags':{k:report[k] for k in ('cab_contracts_pass','deadline_nonincrease','review_eligible','integration_eligible','release_approved')},
         'rows':[], 'stages':[], 'misses':misses, 'primary_statistics':[r for r in analysis['rows'] if r['block']==32 and r['phase']=='forced_six_slot_publication']}
    for key,d in summary.items():
        row=dict(zip(('mode','arm','engine','channels','phase'),key))
        row.update({k:(sum(v) if k in ('callbacks','deadline_misses','cpu_over_budget','mac_calls') else statistics.median(v)) for k,v in d.items()})
        out['rows'].append(row)
    for key,v in stages.items():out['stages'].append(dict(zip(('arm','engine','channels','phase','stage'),key),median_us=statistics.median(v)))
    result[kind]=out
    args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(kind,'verified',len(manifest),'misses',len(misses),'CPU-associated',sum(m['cpu']>1000/3 for m in misses),flush=True)

"""Keep paired timing summaries, every overrun, and raw callback hashes.

The reproduction harness writes every callback CSV. Those large intermediate
CSVs are reproducible; this compact evidence keeps all run summaries, all
deadline overruns (none dropped), maxima and SHA256 identities.
"""
import gzip
import hashlib
import io
import json
import sys
from pathlib import Path
import numpy as np

p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parent
data=json.loads((p/'cpu.json').read_text());rows=data['rows']
groups=[]
for rate in (44100,48000,96000):
    for name in sorted({r['input'] for r in rows}):
        for block in (64,128,256):
            for shift in (-2,1):
                for base in ('corr40-split-v3','corr40-split-v3-sinc'):
                    arms=[]
                    for engine in (base,base+'-edge'):
                        arm=sorted([r for r in rows if (r['rate'],r['input'],r['block'],r['shift'],r['engine'])==
                                    (rate,name,block,shift,engine)],key=lambda r:r['repeat'])
                        assert len(arm)==5
                        arms.append(arm)
                    a,b=arms
                    ratios=[100*(y['cpu_percent']/x['cpu_percent']-1) for x,y in zip(a,b)]
                    groups.append(dict(rate=rate,input=name,block=block,shift=shift,base=base,
                        cpu_relative_change_percent_median=float(np.median(ratios)),
                        cpu_relative_change_percent_range=[min(ratios),max(ratios)],
                        cpu_audio_percent_median_off=float(np.median([r['cpu_percent'] for r in a])),
                        cpu_audio_percent_median_on=float(np.median([r['cpu_percent'] for r in b])),
                        cpu_p99_us_median_off=float(np.median([r['cpu_p99_us'] for r in a])),
                        cpu_p99_us_median_on=float(np.median([r['cpu_p99_us'] for r in b])),
                        cpu_max_us_off=max(r['cpu_max_us'] for r in a),cpu_max_us_on=max(r['cpu_max_us'] for r in b),
                        wall_max_us_off=max(r['max_us'] for r in a),wall_max_us_on=max(r['max_us'] for r in b),
                        cpu_overruns_off=sum(r['cpu_overruns'] for r in a),cpu_overruns_on=sum(r['cpu_overruns'] for r in b),
                        wall_overruns_off=sum(r['deadline_misses'] for r in a),wall_overruns_on=sum(r['deadline_misses'] for r in b)))
summary={}
for base in ('corr40-split-v3','corr40-split-v3-sinc'):
    g=[x for x in groups if x['base']==base]
    v=[x['cpu_relative_change_percent_median'] for x in g]
    summary[base]=dict(conditions=len(g),relative_change_median=float(np.median(v)),
        relative_change_min=min(v),relative_change_max=max(v),
        **{key:sum(x[key] for x in g) for key in ('cpu_overruns_off','cpu_overruns_on','wall_overruns_off','wall_overruns_on')},
        **{key:max(x[key] for x in g) for key in ('cpu_max_us_off','cpu_max_us_on','wall_max_us_off','wall_max_us_on')})
manifest={};events=[]
for path in sorted((p/'cpu-callbacks').glob('*.csv.gz')):
    raw=path.read_bytes();manifest[path.name]=hashlib.sha256(raw).hexdigest()
    array=np.genfromtxt(io.BytesIO(gzip.decompress(raw)),delimiter=',',names=True)
    rate=int(path.name.split('-')[0]);deadline=array['samples']/rate*1e6
    for i in np.flatnonzero((array['thread_cpu_us']>deadline)|(array['microseconds']>deadline)):
        events.append(dict(file=path.name,callback=int(i),samples=int(array['samples'][i]),
            deadline_us=float(deadline[i]),thread_cpu_us=float(array['thread_cpu_us'][i]),wall_us=float(array['microseconds'][i])))
for name,obj in [('cpu-summary.json',dict(summary=summary,groups=groups)),('cpu-overruns.json',dict(events=events)),
                 ('cpu-callback-sha256.json',manifest)]:
    (p/name).write_text(json.dumps(obj,indent=2)+'\n')
with gzip.open(p/'cpu-results.json.gz','wt') as f:json.dump(data,f)
print(json.dumps(summary,indent=2))

"""Post-hoc read-only diagnostic for every target row with changed reset timing."""
import gzip
import io
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools/TransposeNext'))
from edge_validation import inputs,HOLDOUT,LONG,RATES,write
out=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else Path(__file__).resolve().parent
binary=Path(sys.argv[1]).resolve()
summary=json.loads((out/'target-summary.json').read_text())
selected=set(tuple(x) for v in summary['comparisons'].values() for x in v['renders_with_detection_difference'])
rows=[]
with tempfile.TemporaryDirectory() as d:
    ip=Path(d)/'in.f32'
    for rate in RATES:
        for key,x,_,_ in inputs(rate,HOLDOUT,long_seeds=LONG):
            for shift in (-2,-1,1,2):
                if (key,str(rate),str(shift)) not in selected:continue
                x.tofile(ip)
                row=dict(input=key,rate=rate,shift=shift)
                for engine in ('v3','v3-edge','v3-sinc','v3-sinc-edge'):
                    r=subprocess.run([str(binary),str(ip),str(rate),str(shift),engine],check=True,capture_output=True,text=True)
                    csv_rows=list(csv.DictReader(io.StringIO(r.stdout)))
                    row[engine]={event:[int(x['sample']) for x in csv_rows if x['event']==event]
                                 for event in ('detected','high_reset','low_confirm','low_reset')}
                    name=f'onset-{rate}-{key.replace("/","_")}-{shift}-{engine}.csv.gz'
                    with gzip.open(out/name,'wt') as f:f.write(r.stdout)
                rows.append(row)
write(out/'onset-diagnostic.json',dict(scope='Post-hoc, 13 cases with changed target association timing; not all inputs',rows=rows,
      detected_differences=sum(r[e]['detected']!=r[e+'-edge']['detected'] for r in rows for e in ('v3','v3-sinc')),
      low_confirm_differences=sum(r[e]['low_confirm']!=r[e+'-edge']['low_confirm'] for r in rows for e in ('v3','v3-sinc'))))

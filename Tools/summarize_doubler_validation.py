#!/usr/bin/env python3
"""Summarize raw Doubler evidence without accepting partial runs."""
import csv
import json
import statistics
import sys
from pathlib import Path

root=Path(sys.argv[1])
rows=list(csv.DictReader((root/'measurements.csv').open()))
assert len(rows)==9180, f'incomplete measurement: {len(rows)} / 9180'
assert len({tuple(r[k] for k in ('version','rate','spread_ms','fixture','hz','channel')) for r in rows})==9180
fixed=[r for r in rows if r['version']=='fixed']
old={tuple(r[k] for k in ('rate','spread_ms','fixture','hz','channel')):r for r in rows if r['version']=='192'}
metrics=('rms_gain_db','peak_dbfs','lr_correlation','side_mid_db','attack_3ms_db','samples_above_0dbfs')
assert all(all(r[k]==old[tuple(r[x] for x in ('rate','spread_ms','fixture','hz','channel'))][k] for k in metrics) for r in fixed), '192/fixed measurement changed'
def span(rs,key):
    return [min(float(r[key]) for r in rs),max(float(r[key]) for r in rs)]
summary={'rows':len(rows),'render_cases':len(rows)//3,'fixed_matches_192_metrics':True,
         'default_48k_mono':[r for r in rows if r['rate']=='48000' and r['spread_ms']=='6' and r['channel']=='2'],
         'fundamentals':{hz:span([r for r in fixed if r['fixture']=='tone' and r['hz']==hz],'rms_gain_db') for hz in ('27.5','55')},
         'fixtures':{name:{key:span([r for r in fixed if r['fixture']==name],key) for key in ('rms_gain_db','peak_dbfs','lr_correlation','attack_3ms_db')} for name in ('palm','bass','lead','noise')},
         'maximum_sample_peak':max(fixed,key=lambda r:float(r['peak_dbfs'])),
         'overs_cases':sum(int(r['samples_above_0dbfs'])>0 for r in fixed)}
if (root/'cpu.csv').exists():
    cpu=list(csv.DictReader((root/'cpu.csv').open()))
    assert len(cpu)==486,f'incomplete CPU benchmark: {len(cpu)} / 486'
    summary['cpu']=[]
    for sr in ('48000','96000'):
        for block in ('64','128','512'):
            for mode in ('0','1','2'):
                group=[r for r in cpu if (r['rate'],r['block'],r['mode'])==(sr,block,mode)]
                values={v:[float(r['thread_ns_per_stereo_frame']) for r in group if r['version']==v] for v in ('190','192','fixed')}
                med={v:statistics.median(a) for v,a in values.items()}
                summary['cpu'].append({'rate':int(sr),'block':int(block),'mode':int(mode),'median_ns':med,
                    'pr192_vs_190_percent':100*(med['192']/med['190']-1),
                    'fixed_vs_192_percent':100*(med['fixed']/med['192']-1),
                    'fixed_ns_range':[min(values['fixed']),max(values['fixed'])],
                    'max_wall_us':max(float(r['max_callback_wall_us']) for r in group),
                    'overruns':sum(int(r['overruns']) for r in group)})
if (root/'cpu-control.csv').exists():
    control=list(csv.DictReader((root/'cpu-control.csv').open()))
    assert len(control)==45, 'incomplete A/A control'
    by={v:{int(r['repeat']):float(r['thread_ns_per_stereo_frame']) for r in control if r['version']==v} for v in ('192_A','192_B','fixed')}
    aa=[100*(by['192_B'][i]/by['192_A'][i]-1) for i in range(15)]
    summary['cpu_control']={'aa_percent_range':[min(aa),max(aa)],'aa_percent_median':statistics.median(aa),
        'median_ns':{v:statistics.median(a.values()) for v,a in by.items()},
        'overruns':sum(int(r['overruns']) for r in control),'performance_conclusion':'inconclusive: A/A variability dominates'}
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='default_48k_mono'},indent=2))

#!/usr/bin/env python3
"""Summarize synthetic Phase 12 probe CSVs; no listening-quality verdict."""
import csv, json, sys, statistics
from collections import Counter, defaultdict

TEXT = {'head','channel_name','fixture','cab','style'}
KEY = ('model','channel','rate','f0','fixture','cab','style','driven','shelf_db','shelf_hz')
def read(path):
    with open(path) as f:
        rows = list(csv.DictReader(f))
    assert all(None not in row and None not in row.values() for row in rows), 'CSV columns do not align'
    for row in rows:
        for k in row:
            if k not in TEXT: row[k] = float(row[k])
    assert len({tuple(r[k] for k in KEY) for r in rows})==len(rows), 'Duplicate probe cases'
    return rows

def brief(row):
    return {**{k:row[k] for k in KEY[:7]}, 'head':row['head'],
            'p':[row['p_before'],row['p_after']], 'r':[row['r_before'],row['r_after']],
            'reported_low_error':[row['r_error_before'],row['r_error_after']],
            'wide25_100':[row['before_experimental_25_100_excess_db'],row['after_experimental_25_100_excess_db']],
            'f0_delta':row['after_f0_db']-row['before_f0_db'],
            'sub_delta':row['after_sub25_40_db']-row['before_sub25_40_db']}

def summary(rows):
    out={'cases':len(rows),'by_rate':dict(Counter(r['rate'] for r in rows)),
         'by_family':dict(Counter(r['fixture'] for r in rows)),
         'combined_error_increase':[brief(r) for r in rows if r['total_error_delta']>1e-4]}
    out['low_band_disagreements']=[]
    for row in rows:
        before=row['before_experimental_25_100_excess_db'];after=row['after_experimental_25_100_excess_db']
        movement=row['r_after']-row['r_before']
        # Observational comparison only: the wide target below40 is extrapolated.
        # Flag when a move opposite the complete-band discrepancy increases its
        # distance by >0.2dB, or exits the original1.5dB no-move tolerance.
        if abs(movement)>1e-6 and abs(after)>abs(before)+.2 and abs(after)>1.5:
            if (movement>0 and before>=0) or (movement<0 and before<=0):
                out['low_band_disagreements'].append(brief(row))
    out['niflheimr_gap_dry']=[brief(r) for r in rows if r['model']==25 and r['fixture']=='harmonic_gap' and r['cab']=='dry_head']
    by_head=defaultdict(list)
    for r in rows:by_head[r['head']].append(r)
    out['by_head']={head:{'cases':len(rs),
        'p_moves':[min(r['p_after']-r['p_before'] for r in rs),max(r['p_after']-r['p_before'] for r in rs)],
        'r_moves':[min(r['r_after']-r['r_before'] for r in rs),max(r['r_after']-r['r_before'] for r in rs)],
        'f0_delta':[min(r['after_f0_db']-r['before_f0_db'] for r in rs),max(r['after_f0_db']-r['before_f0_db'] for r in rs)],
        'sub_delta':[min(r['after_sub25_40_db']-r['before_sub25_40_db'] for r in rs),max(r['after_sub25_40_db']-r['before_sub25_40_db'] for r in rs)],
        'largest_grid_sub_interaction':max(abs(r['grid_sub_interaction_db']) for r in rs),
        'largest_grid_f0_interaction':max(abs(r['grid_f0_interaction_db']) for r in rs)} for head,rs in by_head.items()}
    timings=[r['inner_analysis_ms'] for r in rows]
    out['under_load_inner_analysis_ms']={'min':min(timings),'median':statistics.median(timings),'max':max(timings)}
    out['max_total_render_calls']=1+max(r['inner_analysis_render_calls'] for r in rows)
    return out

if len(sys.argv) not in (2,3):
    raise SystemExit('Usage: summarize_auto_set_power_bass.py BASELINE.csv [CANDIDATE.csv]')
base=read(sys.argv[1])
result={'baseline':summary(base)}
if len(sys.argv)>2:
    new=read(sys.argv[2]);result['candidate']=summary(new)
    index={tuple(r[k] for k in KEY):r for r in new}
    pairs=[(r,index[tuple(r[k] for k in KEY)]) for r in base if tuple(r[k] for k in KEY) in index]
    result['comparison']={'paired_cases':len(pairs),
                         'changed_proposals':sum(abs(a['p_after']-b['p_after'])>1e-6 or abs(a['r_after']-b['r_after'])>1e-6 for a,b in pairs),
                         'new_wide_error_worse':sum(abs(b['after_experimental_25_100_excess_db'])>abs(a['after_experimental_25_100_excess_db'])+.2 for a,b in pairs),
                         'new_wide_error_better':sum(abs(b['after_experimental_25_100_excess_db'])<abs(a['after_experimental_25_100_excess_db'])-.2 for a,b in pairs)}
print(json.dumps(result,indent=2,ensure_ascii=False))

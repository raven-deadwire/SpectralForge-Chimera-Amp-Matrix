# Applies the preregistered pass rules E1-E5 of edge_validation.py to its outputs.
# usage: python edge_judge.py <run dir> <out.json>
import sys,json,gzip,numpy as np
from pathlib import Path
run=Path(sys.argv[1]);out={}
def sweep(k):
    # The evidence copies of the sweeps are gzipped; read either form.
    p=run/f'sweep{k}.json'
    return json.load(open(p) if p.exists() else gzip.open(str(p)+'.gz','rt'))
sw=[r for k in (1,2,3) for r in sweep(k)]
e1={};e2={};refbad={}
for rate in (44100,48000,96000):
    for k in (1,2,3):
        s=[r for r in sw if r['rate']==rate and r['k']==k]
        c=np.array([r['edge']['net_db_re_harm'] for r in s]);b=np.array([r['v3sinc']['net_db_re_harm'] for r in s])
        key=f'{rate}/k{k}'
        e1[key]={'cand_worst_db':round(float(c.max()),2),'ref_worst_db':round(float(b.max()),2),'pass':bool(c.max()<=-50)}
        e2[key]={'median_change_db':round(float(np.median(c-b)),3),'max_change_db':round(float((c-b).max()),2),'pass':bool(np.median(c-b)<=.5)}
        refbad[key]={'ref_above_-50':int((b>-50).sum()),'cand_above_-50':int((c>-50).sum()),'points':len(s)}
res=json.load(open(run/'residual.json'));e3={}
for rate in (44100,48000,96000):
    s=[r for r in res if r['rate']==rate];c=np.array([r['edge']['net_db_re_harm'] for r in s]);b=np.array([r['v3sinc']['net_db_re_harm'] for r in s])
    e3[str(rate)]={'cases':len(s),'cand_median_db':round(float(np.median(c)),2),'cand_share_le_-50':round(float(np.mean(c<=-50)),4),'ref_median_db':round(float(np.median(b)),2),
                   'median_minus_ref_db':round(float(np.median(c)-np.median(b)),3),'cand_max_db':round(float(c.max()),2),'ref_max_db':round(float(b.max()),2),'changed':int(np.sum(np.abs(c-b)>.01)),
                   'pass':bool(np.median(c)<=-60 and np.mean(c<=-50)>=.9 and np.median(c)-np.median(b)<=.5)}
ts=json.load(open(run/'target-summary.json'));cand=ts['corr40-split-v3-sinc-edge'];eq=ts['equivalence']
def verdicts(rep):return {t['id']:t['verdict'] for t in rep['targets'][:5]}
va,vf=verdicts(cand['all']),verdicts(cand['fresh_only'])
ok=lambda v:v['T1_attack']=='PASS' and v['T2_low_onset_down']=='PASS' and v['T4_quality']=='PASS' and v['T3_low_onset_up']=='REPORT_WITHIN_CEILING'
e4={'all_inputs':va,'fresh_only':vf,'count_differences':len(eq['renders_with_count_difference']),'lowband_abs_diff_max_ms':eq['lowband_abs_diff_max_ms'],
    'target_summary_abs_diff_ms':eq['target_summary_abs_diff_ms'],
    'pass':bool(ok(va) and ok(vf) and not eq['renders_with_count_difference'] and eq['lowband_abs_diff_max_ms']<=.1 and max(eq['target_summary_abs_diff_ms'].values())<=.1)}
bl=json.load(open(run/'block-invariance.json'))['summary'];e5={**bl,'pass':bl['verdict']=='PASS'}
out={'E1_sweep_worst':{'pass':all(v['pass'] for v in e1.values()),'by':e1},'E2_sweep_median':{'pass':all(v['pass'] for v in e2.values()),'by':e2},
     'E3_residual_set':{'pass':all(v['pass'] for v in e3.values()),'by':e3},'E4_targets':e4,'E5_blocks':e5,'sweep_counts_report_only':refbad}
out['overall']='PASS' if all(out[k]['pass'] for k in ('E1_sweep_worst','E2_sweep_median','E3_residual_set','E4_targets','E5_blocks')) else 'FAIL'
json.dump(out,open(sys.argv[2],'w'),indent=1)
print(out['overall'],{k:out[k]['pass'] for k in ('E1_sweep_worst','E2_sweep_median','E3_residual_set','E4_targets','E5_blocks')})

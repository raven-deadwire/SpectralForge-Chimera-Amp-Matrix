#!/usr/bin/env python3
"""Summarise raw results without changing release criteria or hiding failures."""
import csv,json,platform,shutil,subprocess
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[2];base=ROOT/'build/transpose-audit';out=ROOT/'docs/evidence/transpose-pr125-pr128-audit-20261023';out.mkdir(parents=True,exist_ok=True)
q=json.loads((base/'quality/results.json').read_text());tim=json.loads((base/'timing-ftz/results.json').read_text());part=json.loads((base/'partition/results.json').read_text());rows=q['results'];engines=['baseline','tight125','corr40-split-rt'];insts=['bass_B0','bass_dropA_A0','guitar_dropA_A1'];labels={'baseline':'공개판 Classic','tight125':'#125 Tight','corr40-split-rt':'#128 RT'};notes=dict(zip(insts,['B0','A0','A1']))
summary=[]
for inst in insts:
 for eng in engines:
  tone=[x['tone'] for x in rows if x['instrument']==inst and x['engine']==eng and x['name']=='harmonics'];att=[x for x in rows if x['instrument']==inst and x['engine']==eng and x['name'] in ('riff75','alternating_velocity') and 'attack_extra_ms' in x];hold=[x for x in rows if x['instrument']==inst and x['engine']==eng and x['name']=='fresh_holdout']
  summary.append({'note':notes[inst],'engine':eng,'harmonic_pitch_abs_max_cents':max(abs(t['dominant_error_cents']) for t in tone),'harmonic_warble_max_cents':max(t['warble_p95_p5_cents'] for t in tone),'attack_extra_max_ms':max(v for x in att for v in x['attack_extra_ms']),'lowband_extra_max_ms':max(v for x in att for v in x['lowband_extra_centroid_ms']),'fresh_attack_max_ms':max(v for x in hold for v in x['attack_extra_ms']),'fresh_lowband_max_ms':max(v for x in hold for v in x['lowband_extra_centroid_ms'])})
cpu=[]
for eng in engines:
 rr=[x for x in tim['results'] if x['engine']==eng and x['block']==128]
 groups={}
 for x in rr:groups.setdefault((x['instrument'],x['shift']),[]).append(x)
 med=lambda k:[float(np.median([x['timing'][k] for x in g])) for g in groups.values()]
 cpu.append({'engine':eng,'mean_us_range':[min(med('cpu_mean_us')),max(med('cpu_mean_us'))],'budget_pct_range':[min(med('cpu_budget_percent')),max(med('cpu_budget_percent'))],'p99_us_range':[min(med('cpu_p99_us')),max(med('cpu_p99_us'))],'peak_cpu_us':max(x['timing']['cpu_peak_us'] for x in rr),'peak_wall_us':max(x['timing']['wall_peak_us'] for x in rr),'cpu_misses':sum(x['timing']['cpu_misses'] for x in rr),'wall_misses':sum(x['timing']['wall_misses'] for x in rr),'callbacks':sum(x['timing']['callbacks'] for x in rr)})
sm={'quality_renders':len(rows),'partition_renders':len(part),'timing_renders':len(tim['results']),'summary':summary,'cpu128':cpu,'release_approved':False,'target_achieved':False,'new_failure':'RT_downshift_block_partition_dependence','functional_tests':{'CorrelationTests':'PASS','PitchRegressionTests':'PASS','PitchRegressionTests125':'PASS','StabilityTests':'PASS','GateTests':'PASS','CorrelationTestsRT':'FAIL_partition-dependent_output'}}
(out/'summary.json').write_text(json.dumps(sm,indent=2)+'\n')
with (out/'comparison.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=summary[0],lineterminator="\n");w.writeheader();w.writerows(summary)
for src,name in [(base/'quality/results.json','quality.json'),(base/'timing-ftz/results.json','timing.json'),(base/'timing/results.json','timing-original-probe.json'),(base/'partition/results.json','partition.json'),(base/'build.json','build.json'),(base/'audit-build.json','audit-build.json')]:shutil.copy2(src,out/name)
for p in base.glob('*Tests*.log'):shutil.copy2(p,out/p.name)
(out/'environment.txt').write_text(subprocess.check_output(['lscpu'],text=True)+'\n'+platform.platform()+'\n')
# Full callback traces retained in the evidence archive, excluded from git text diff.
import tarfile
with tarfile.open(out/'callbacks.tar.gz','w:gz') as tar:
 for mode in ('quality','timing','timing-ftz'):
  tar.add(base/mode/'callbacks',arcname=mode)
# Human-auditable table for every pitch shift, not only maxima.
flat=[]
for x in rows:
 if x['engine'] not in engines:continue
 r={k:x[k] for k in ['name','instrument','engine','shift','block','nominal_samples','output_sha256']}
 if 'tone' in x:r.update(x['tone'])
 if 'attack_extra_ms' in x:r.update(attack_max_ms=max(x['attack_extra_ms']),attack_median_ms=float(np.median(x['attack_extra_ms'])),lowband_max_ms=max(x['lowband_extra_centroid_ms']),lowband_median_ms=float(np.median(x['lowband_extra_centroid_ms'])))
 flat.append(r)
fields=list(dict.fromkeys(k for r in flat for k in r))
with (out/'per-condition.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=fields,lineterminator="\n");w.writeheader();w.writerows(flat)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
fig,axes=plt.subplots(1,2,figsize=(12,4.5));colours=['#718096','#d8891c','#087f8c'];names=['Released Classic','PR125 Tight','PR128 RT']
for ax,key,title in zip(axes,['attack_extra_max_ms','lowband_extra_max_ms'],['High-band attack peak','Low-band energy centroid (proxy)']):
 for i,eng in enumerate(engines):
  vals=[next(r[key] for r in summary if r['note']==n and r['engine']==eng) for n in ['B0','A0','A1']]
  ax.bar(np.arange(3)+(i-1)*.24,vals,.22,label=names[i],color=colours[i])
 ax.axhline(10,color='#b72028',ls='--',lw=1.2,label='10 ms target');ax.set_xticks(range(3),['B0 30.87 Hz','A0 27.5 Hz','A1 55 Hz']);ax.set_ylabel('Additional delay (ms)');ax.set_title(title);ax.set_ylim(0,55);ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
axes[0].legend(fontsize=8);fig.suptitle('48 kHz / 128 samples: worst across +/-1, +/-2 semitones\nSynthetic repeated + alternating picks; no DAW or device latency',fontsize=12);fig.tight_layout();fig.savefig(out/'delay-comparison.png',dpi=160);plt.close(fig)
print(json.dumps(sm,indent=2))

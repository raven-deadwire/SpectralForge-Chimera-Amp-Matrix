"""Audit complete ABBA/BAAB data and describe per-fixture medians, not realtime acceptance."""
import csv
import json
import math
from pathlib import Path
import statistics
import sys

def analyze(path: Path) -> dict:
    with path.open(newline='', encoding='utf-8') as handle:
        rows=list(csv.DictReader(handle))
    if len(rows)!=1632:
        raise ValueError('Expected all 1536 measured and 96 warmup rows')
    warm=[r for r in rows if r['phase']=='warmup']
    measured=[r for r in rows if r['phase']=='measured']
    if len(warm)!=96 or len(measured)!=1536:
        raise ValueError('Incomplete population')
    grid=[(r,h,k) for r in (22050.,24000.,24500.)
          for h in (16.352,27.5,30.8677,41.2034,82.4069,110.,440.,1318.51) for k in (0,1)]
    keys=set()
    for row in rows:
        phase=row['phase']
        round_,fixture,position=(int(row[k]) for k in ('round','fixture','position'))
        key=(phase,round_,fixture,position)
        if key in keys or not 0<=fixture<48:
            raise ValueError('Duplicate/out-of-grid measurement')
        keys.add(key)
        identity=(float(row['rate']),float(row['hz']),int(row['kind']))
        if identity!=grid[fixture]:
            raise ValueError('Changed fixture identity')
        if phase=='measured':
            if not(0<=round_<8 and 0<=position<4):
                raise ValueError('Out-of-grid measured row')
            expected=('ABBA' if round_%2==0 else 'BAAB')[position]
            repetitions=4
        else:
            if phase!='warmup' or round_!=-1 or position not in (0,1):
                raise ValueError('Out-of-grid warmup row')
            expected='AB'[position]
            repetitions=1
        if row['variant']!=expected or int(row['repetitions'])!=repetitions:
            raise ValueError('Changed measurement sequence/workload')
        for metric in ('wall_us','process_cpu_us'):
            if not math.isfinite(float(row[metric])) or float(row[metric])<=0:
                raise ValueError('Invalid clock measurement')
    cells=[]
    for fixture in range(48):
        rs=[r for r in measured if int(r['fixture'])==fixture]
        d={'fixture':fixture,'rate':float(rs[0]['rate']),'hz':float(rs[0]['hz']),'kind':int(rs[0]['kind'])}
        for metric in ('wall_us','process_cpu_us'):
            a=[float(r[metric])/4 for r in rs if r['variant']=='A']
            b=[float(r[metric])/4 for r in rs if r['variant']=='B']
            if len(a)!=16 or len(b)!=16:
                raise ValueError('Incomplete arm')
            d[metric]={'baseline_median':statistics.median(a),'candidate_median':statistics.median(b),
                       'change_percent':100*(statistics.median(b)/statistics.median(a)-1),
                       'baseline_max':max(a),'candidate_max':max(b)}
        cells.append(d)
    result={'file':str(path),'rows':len(rows),'warmup_rows':len(warm),'measured_rows':len(measured),
            'performance_acceptance':'NOT_ASSESSED','cells':cells}
    for metric in ('wall_us','process_cpu_us'):
        changes=[c[metric]['change_percent'] for c in cells]
        result[metric]={'median_cell_change_percent':statistics.median(changes),
                        'improved_cells':sum(x<0 for x in changes),'range_change_percent':[min(changes),max(changes)]}
    return result

if __name__=='__main__':
    if len(sys.argv)!=2:
        raise SystemExit('Usage: analyze_tuner_refinement_benchmark.py benchmark.csv')
    try:
        print(json.dumps(analyze(Path(sys.argv[1])),indent=2))
    except (OSError,ValueError,KeyError,ZeroDivisionError) as exc:
        raise SystemExit(str(exc)) from exc

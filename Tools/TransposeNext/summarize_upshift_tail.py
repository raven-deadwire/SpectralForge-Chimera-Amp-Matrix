#!/usr/bin/env python3
"""Summarize stored synthetic runs, keeping the legacy and extended windows separate."""
import gzip,json,sys
from pathlib import Path
import numpy as np
folder=Path(sys.argv[1]);out={}
for p in sorted(folder.glob('*-v*.json.gz')):
    d=json.loads(gzip.decompress(p.read_bytes()))
    if 'report' not in d or 'rows' not in d:continue
    r={'targets':d['report']['targets'],'per_rate':d['report']['per_rate'],
       'baseline_bit_equal':sum(q['bit_equal'] for q in d['baseline_parity']),
       'render_count':len(d['rows']),'tails':{}}
    for direction,positive in [('up',True),('down',False)]:
        for rate in [44100,48000,96000,'all']:
            for window in [60,120]:
                events=[t for row in d['rows'] if (row['shift']>0)==positive and (rate=='all' or row['rate']==rate)
                        for t in row.get('tails',[]) if t['snr_db']>=10 and t['end_ms']==window]
                a=np.array([q['echo_out_db'] for q in events]);b=np.array([q['echo_in_db'] for q in events]);ex=a-b
                r['tails'][f'{direction}/{rate}/{window}']={'events':len(events),'input_median_db':float(np.median(b)),
                    'output_median_db':float(np.median(a)),'output_max_db':float(max(a)),
                    'excess_median_db':float(np.median(ex)),'excess_max_db':float(max(ex)),
                    'excess_over_6db':int(sum(ex>6))}
    out[p.name.removesuffix('.json.gz')]=r
(folder/'summary.json').write_text(json.dumps(out,indent=1)+'\n')
for name,r in out.items():
    print(name,[(t['id'],t['verdict'],t['observed']) for t in r['targets'][:5]])
    print('tails60',r['tails']['up/all/60'],'tails120',r['tails']['up/all/120'])

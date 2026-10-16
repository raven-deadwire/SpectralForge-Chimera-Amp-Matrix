#!/usr/bin/env python3
"""Recompute saved populations without running any benchmark."""
import csv, hashlib, json, math, pathlib, sys
from run import analyze, summary, ROUTES

root=pathlib.Path(sys.argv[1]).resolve()
manifest=json.loads((root/'manifest.json').read_text())
files={str(p.relative_to(root)) for p in root.rglob('*') if p.is_file()}
assert files==set(manifest)|{'manifest.json'}
for name,record in manifest.items():
    p=pathlib.Path(name);assert not p.is_absolute() and '..' not in p.parts
    data=(root/p).read_bytes()
    assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],name
report=json.loads((root/'report.json').read_text())
assert report['complete'] and report['release_approved'] is False
assert report['source']=='c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
assert report['juce']=='d6181bde38d858c283c3b7bf699ce6340c050b5d'
for arm in ('A','B'):
    text=(root/f'{arm}-original.log').read_text()
    for token in ('realtime equivalence routes=144','exact_float_samples=2645088','fade_mix_samples=388800','exact_bits=1 immutable_spectra=1','latency=0'):
        assert token in text
correctness=report['correctness'];assert len(correctness)==76
total=0
for entry in correctness:
    route=entry['route']
    if route.startswith('edge-'):
        label=route.removeprefix('edge-');scenario,parallel,suffix=label.split('-')
        paths=[root/'edge'/f'{scenario}-{parallel}-{arm}.f32{suffix}' for arm in ('A','B')]
    else:paths=[root/'correctness'/f'{route}-{arm}.f32' for arm in ('A','B')]
    a,b=(p.read_bytes() for p in paths)
    if route.startswith('edge-'):
        block,channels,scenario=32,2,int(scenario[1:])
    else:
        _,block,channels,scenario=route.split('-');block=int(block);channels=int(channels);scenario=int(scenario[1:])
    sizes=(1,17,63,block,3)
    expected=3*channels*sum(min(block,sizes[i%5]) if scenario else block for i in range(2600))
    assert a==b and len(a)//4==entry['floats']==expected and hashlib.sha256(a).hexdigest()==entry['sha256'],route
    total+=entry['floats']
assert report['irs']=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'irs').glob('*')}
order=[]
for r in range(6):
    for mode in (('control','profile') if r%2==0 else ('profile','control')):
        for group in (('AA','AB') if r%2==0 else ('AB','AA')):
            for slot,logical in enumerate('ABBA' if r%2==0 else 'BAAB'):
                order.append((r,mode,group,slot,logical,logical if group=='AB' else 'A'))
assert len(report['trials'])==96
raw=0
for trial,wanted in zip(report['trials'],order):
    assert tuple(trial[k] for k in ('round','mode','group','slot','logical','arm'))==wanted
    r,mode,group,slot,logical,arm=wanted
    for family,block,channels in ROUTES:
        key=f'{family}-{block}-{channels}'
        path=root/'trials'/f'r{r}-{mode}-{group}-{slot}-{logical}'/f'{key}.csv'
        assert summary(path)==trial['routes'][key]
        rows=list(csv.DictReader(path.open()));assert len(rows)==1600
        assert [int(x['callback']) for x in rows]==list(range(1600))
        assert all(int(x['samples'])==block for x in rows);raw+=len(rows)
assert raw==1843200
assert analyze(report['trials'])==report['analysis']
print(json.dumps({'manifest_files':len(manifest),'correctness_routes':len(correctness),'exact_floats':total,'raw_callbacks':raw,
    'statistics_recomputed':True,'analysis':{k:v for k,v in report['analysis'].items() if k!='rows'},'release_approved':False},indent=2))

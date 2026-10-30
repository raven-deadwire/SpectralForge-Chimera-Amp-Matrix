#!/usr/bin/env python3
"""Exercise native amp/cab capture with existing synthetic controls, never real DI evidence."""
import argparse
import json
from pathlib import Path
import subprocess

from di_regression import sha


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();exe=a.renderer.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    root=Path(__file__).resolve().parents[2]
    subprocess.run([str(exe),'--dynamic-eq-fixtures',str(out/'fixtures')],check=True)
    report={'kind':'SYNTHETIC_ONLY','real_di_completed':False,'listening_completed':False,
            'renderer_sha256':sha(exe),'cases':[]}
    for rate in (44100,48000,96000):
        for profile,kind,instrument,style in [('guitar-highgain-amp',1,'guitar','metal'),('guitar-highgain-v30',1,'guitar','metal'),('bass-clean-amp',0,'bass','clean')]:
            source=out/'fixtures'/f'{rate}-{kind}.wav';dest=out/f'{rate}-{profile}'
            profile_path=root/'Validation/auto-set-phase6'/f'{profile}.json'
            subprocess.run([str(exe),'--dynamic-eq-capture',str(source),str(profile_path),str(dest)],check=True)
            subprocess.run([str(exe),'--dynamic-eq-wav',str(dest/'pre-tone.wav'),instrument,style,'1',str(dest/'analysis')],check=True)
            result=json.loads((dest/'analysis/analysis.json').read_text())
            report['cases'].append({'rate':rate,'profile':profile,'fixture_sha256':sha(source),
                'profile_sha256':sha(profile_path),'capture_sha256':sha(dest/'pre-tone.wav'),
                'nodes':result['nodes'],'maximum_reduction_db':result['maximum_reduction_db']})
    report['status']='PASS' if all(not c['nodes'] and c['maximum_reduction_db']==0 for c in report['cases']) else 'FAIL'
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['status']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""Build research-only probes against the original pinned JUCE object files."""
import argparse,json,subprocess,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--original-build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];a.original_build=a.original_build.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
original=json.loads((a.original_build/'build.json').read_text());assert original['juce_sha']=='d6181bde38d858c283c3b7bf699ce6340c050b5d'
cmd=original['commands'][0];flags=cmd[1:cmd.index('-c')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
commands=[]
for name in ['EnvelopeProbe','EnvelopeTrace','EnvelopeBlocks','EnvelopeTests']:
    cmd=['g++',*flags,str(root/'Tools/TransposeNext'/(name+'.cpp')),*map(str,sorted(a.original_build.glob('*.o'))),'-ldl','-lrt','-lz','-o',str(a.out/name)]
    subprocess.run(cmd,check=True);commands.append(cmd)
(a.out/'build.json').write_text(json.dumps({'original':original,'commands':commands,'sanitizers_on_research_translation_units_only':a.sanitize},indent=2))

"""Compile supplemental traces/contracts using the research build's own flags.

Usage: python compile_diagnostics.py /absolute/path/to/candidate-build
The header-only candidate and this contract harness receive ASan/UBSan;
the reused JUCE objects retain the research build's ordinary compile flags.
"""
import json
import subprocess
import sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
binary=Path(sys.argv[1]).resolve()
manifest=json.loads((binary/'build.json').read_text())
for name,source in [('EdgeTrace','TransposeProbe'),('EdgeOnsetTrace','OnsetTrace'),('EdgeContractTests','CorrelationTests')]:
    cmd=next(list(c) for c in manifest['commands'] if c[-1].endswith('/'+source))
    i=next(i for i,x in enumerate(cmd) if x.endswith('/Tools/TransposeNext/'+source+'.cpp'))
    cmd[i]=str(HERE/(name+'.cpp'))
    cmd.insert(1,'-I'+str(ROOT/'Tools/TransposeNext'))
    if name=='EdgeContractTests':
        cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    cmd[-1]=str(binary/name)
    subprocess.run(cmd,check=True)

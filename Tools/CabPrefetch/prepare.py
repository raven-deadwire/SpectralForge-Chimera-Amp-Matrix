#!/usr/bin/env python3
"""Generate isolated scalar treatments from the exact validated scalar source."""
import argparse, hashlib, os, subprocess
from pathlib import Path
PARENT='c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
HEADER='Source/ModeledCabConvolution.h'
BASE_HASH='1b0fcc12ed4b20364d94bf044d075cbdc6fe681cfe10121a9b5904e67093788f'
KINDS=('prefetch2',)
def capture(cmd,cwd): return subprocess.check_output(cmd,cwd=cwd,text=True).strip()
def generate(text,kind):
    if kind not in KINDS: raise ValueError(kind)
    if hashlib.sha256(text.encode()).hexdigest()!=BASE_HASH: raise RuntimeError('Scalar header identity changed')
    anchor="""                accumulate(bankInput+size_t(index)*size_t(2*bins),"""
    hint="""                // Diagnostic hypothesis: hide upcoming partition read latency.
                // Two partitions ahead, only within the current private bank and IR.
                // No dereference, float arithmetic or partition ordering changes.
                if(partition+2<segments) {
                    int ahead=index+2;
                    if(ahead>=segments)ahead-=segments;
                    const auto* nextInput=bankInput+size_t(ahead)*size_t(2*bins);
                    const auto* nextImpulse=prepared->impulseSpectra.data()+size_t(partition+2)*size_t(2*bins);
                    __builtin_prefetch(nextInput,0,1);
                    __builtin_prefetch(nextInput+bins,0,1);
                    __builtin_prefetch(nextImpulse,0,1);
                    __builtin_prefetch(nextImpulse+bins,0,1);
                }
"""
    if text.count(anchor)!=1: raise RuntimeError('Partition anchor changed')
    return text.replace(anchor,hint+anchor)

def prepare(source,kind):
    source=Path(source).resolve()
    if capture(['git','rev-parse','HEAD'],source)!=PARENT or capture(['git','status','--porcelain','--untracked-files=all'],source):
        raise RuntimeError('Requires clean isolated scalar checkout at pinned parent')
    # Detached HEAD prevents accidentally moving a product branch.
    if subprocess.run(['git','symbolic-ref','-q','HEAD'],cwd=source,stdout=subprocess.DEVNULL).returncode==0:
        raise RuntimeError('Detached experimental checkout required')
    path=source/HEADER;path.write_text(generate(path.read_text(),kind))
    if capture(['git','diff','--name-only'],source).splitlines()!=[HEADER]: raise RuntimeError('Unexpected changes')
    subprocess.run(['git','add','--',HEADER],cwd=source,check=True)
    tree=capture(['git','write-tree'],source);env=os.environ.copy()
    for role in ('AUTHOR','COMMITTER'):
        env['GIT_'+role+'_NAME']='Chimera Prefetch Experiment'
        env['GIT_'+role+'_EMAIL']='cab-prefetch@invalid.local'
        env['GIT_'+role+'_DATE']='2026-10-18T00:00:00Z'
    sha=subprocess.check_output(['git','-c','commit.gpgsign=false','commit-tree',tree,'-p',PARENT],
        input='Experiment only: scalar memory '+kind+'; integration HOLD\n',text=True,cwd=source,env=env).strip()
    subprocess.run(['git','reset','--soft',sha],cwd=source,check=True)
    return sha
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--candidate',type=Path,required=True);p.add_argument('--kind',choices=KINDS,required=True)
    a=p.parse_args();print(prepare(a.candidate,a.kind))

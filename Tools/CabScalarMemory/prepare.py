#!/usr/bin/env python3
"""Generate isolated scalar treatments from the exact validated scalar source."""
import argparse, hashlib, os, subprocess
from pathlib import Path
PARENT='c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
HEADER='Source/ModeledCabConvolution.h'
BASE_HASH='1b0fcc12ed4b20364d94bf044d075cbdc6fe681cfe10121a9b5904e67093788f'
KINDS=('split','tile32','clearhead','fusedlead')
def capture(cmd,cwd): return subprocess.check_output(cmd,cwd=cwd,text=True).strip()
def generate(text,kind):
    if kind not in KINDS: raise ValueError(kind)
    if hashlib.sha256(text.encode()).hexdigest()!=BASE_HASH: raise RuntimeError('Scalar header identity changed')
    if kind=='fusedlead':
        old='        juce::FloatVectorOperations::copy(outputSpectrum.data(),pastSum.data(),2*bins);\n        accumulate(currentInput,prepared->impulseSpectra.data(),outputSpectrum.data());'
        new='        // Read the cached delayed sum directly; retain the leading arithmetic order.\n        const int size=bins;\n        const float* __restrict input=currentInput;\n        const float* __restrict impulse=prepared->impulseSpectra.data();\n        const float* __restrict past=pastSum.data();\n        float* __restrict output=outputSpectrum.data();\n        for(int i=0;i<size;++i) {\n            const float re=(past[i]+input[i]*impulse[i])-input[size+i]*impulse[size+i];\n            const float im=(past[size+i]+input[i]*impulse[size+i])+input[size+i]*impulse[i];\n            output[i]=re;output[size+i]=im;\n        }'
        if text.count(old)!=1: raise RuntimeError('Leading accumulation anchor changed')
        return text.replace(old,new)
    if kind=='clearhead':
        # Only [0,blockSize) is written between resets. The tail stays +0.
        a='std::fill(monoInput.begin(),monoInput.end(),0.f);inputPosition=0;'
        b='std::fill(inputTime.begin(),inputTime.end(),Complex{});\n                inputPosition=0;'
        if text.count(a)!=1 or text.count(b)!=1: raise RuntimeError('Zero-fill anchors changed')
        return text.replace(a,'std::fill_n(monoInput.begin(),blockSize,0.f);inputPosition=0;').replace(
            b,'std::fill_n(inputTime.begin(),blockSize,Complex{});\n                inputPosition=0;')
    start=text.index('            int index=bank.offset;')
    end=text.index('\n        }\n        juce::FloatVectorOperations::copy(outputSpectrum',start)
    if kind=='split':
        replacement='''            // Same ascending partition order, two contiguous ring spans.
            const size_t stride=size_t(2*bins);
            const int firstCount=segments-bank.offset-1;
            const float* input=bankInput+size_t(bank.offset+1)*stride;
            const float* impulse=prepared->impulseSpectra.data()+stride;
            for(int n=0;n<firstCount;++n) {
                accumulate(input,impulse,pastSum.data());
                input+=stride;impulse+=stride;
            }
            input=bankInput;
            for(int n=0;n<bank.offset;++n) {
                accumulate(input,impulse,pastSum.data());
                input+=stride;impulse+=stride;
            }'''
    else:
        replacement='''            // Frequency tiling changes reuse distance, never per-bin partition order.
            const int size=bins;
            const size_t stride=size_t(2*size);
            for(int begin=0;begin<size;begin+=32) {
                const int end=std::min(begin+32,size);
                int index=bank.offset;
                for(int partition=1;partition<segments;++partition) {
                    if(++index>=segments)index=0;
                    const float* __restrict input=bankInput+size_t(index)*stride;
                    const float* __restrict impulse=prepared->impulseSpectra.data()+size_t(partition)*stride;
                    float* __restrict output=pastSum.data();
                    for(int i=begin;i<end;++i) {
                        const float re=(output[i]+input[i]*impulse[i])-input[size+i]*impulse[size+i];
                        const float im=(output[size+i]+input[i]*impulse[size+i])+input[size+i]*impulse[i];
                        output[i]=re;output[size+i]=im;
                    }
                }
            }'''
    return text[:start]+replacement+text[end:]
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
        env['GIT_'+role+'_NAME']='Chimera Scalar Memory Experiment'
        env['GIT_'+role+'_EMAIL']='scalar-memory@invalid.local'
        env['GIT_'+role+'_DATE']='2026-10-16T00:00:00Z'
    sha=subprocess.check_output(['git','-c','commit.gpgsign=false','commit-tree',tree,'-p',PARENT],
        input='Experiment only: scalar memory '+kind+'; integration HOLD\n',text=True,cwd=source,env=env).strip()
    subprocess.run(['git','reset','--soft',sha],cwd=source,check=True)
    return sha
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--candidate',type=Path,required=True);p.add_argument('--kind',choices=KINDS,required=True)
    a=p.parse_args();print(prepare(a.candidate,a.kind))

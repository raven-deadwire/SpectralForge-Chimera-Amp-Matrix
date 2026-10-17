#!/usr/bin/env python3
"""Compile exact extracted methods: wrap/partial/odd-bin arithmetic parity only.
This is not an FFT, whole-CAB, realtime or Apple performance test.
"""
import argparse, json, subprocess, platform
from pathlib import Path
from prepare import generate
ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser();p.add_argument('--compiler',default='c++');p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=False)
    original=subprocess.check_output(['git','show','c2263e4c90bffb9a56cc41285ebf8207e641b3aa:Source/ModeledCabConvolution.h'],cwd=ROOT,text=True)
    def extract(text,name):
        methods=text[text.index('    struct InputBank'):text.index('    void processMono')]
        return '''struct '''+name+''' {
        int bins,segments,inputSegmentStep=3,currentSegment=0;
        struct Prepared {std::vector<float> impulseSpectra;};
        Prepared backing; Prepared* prepared=&backing;
        std::vector<float> inputSpectra,pastSum,outputSpectrum;
        '''+name+'''(int b,int s):bins(b),segments(s),backing{std::vector<float>(s*2*b)},
          inputSpectra(3*s*2*b),pastSum(2*b),outputSpectrum(2*b) {}
        '''+methods+'\n};\n'
    source='''#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <random>
#include <cstdint>
#include <cmath>
namespace juce {struct FloatVectorOperations {static void copy(float* d,const float* s,int n) {std::copy_n(s,n,d);}};}
'''
    source+=extract(original,'A')+extract(generate(original,'prefetch2'),'R')
    source+='''int main() {
      std::mt19937 rng(160026); size_t comparisons=0;
      for(int bins:{1,2,3,7,16,17,32,33,64,65,128,129,256,257,512,513,1024})
      for(int segments:{1,2,3,4,7,16,31,64})
      for(int step:{1,3}) {
        A a(bins,segments);R r(bins,segments);
        a.inputSegmentStep=r.inputSegmentStep=step;
        auto fill=[&](auto& v) {for(float& x:v) {
            const uint32_t v=rng(); x=float(int(v%2001)-1000)*0.0001f;
            if(v%19==0) x=std::copysign(0.f,x);
            if(v%23==0) x=std::copysign(1.e-39f,x);
        }};
        fill(a.inputSpectra);fill(a.backing.impulseSpectra);
        r.inputSpectra=a.inputSpectra;
        r.backing.impulseSpectra=a.backing.impulseSpectra;
        const auto immutable=a.backing.impulseSpectra; const auto inputs=a.inputSpectra;
        for(int cursor=0;cursor<step*segments;++cursor) {
          a.currentSegment=r.currentSegment=cursor;
          const auto bank=a.currentInputBank();
          const float* input=a.inputSpectra.data()+size_t(bank.start+bank.offset)*2*bins;
          for(bool first:{true,false,false}) {
            if(!first) {
              for(int i=0;i<2*bins;++i) {
                const size_t pos=size_t(bank.start+bank.offset)*2*bins+i;
                a.inputSpectra[pos]=r.inputSpectra[pos]=float(int(rng()%1001)-500)*.001f;
              }
            }
            a.sumPartitions(input,bank,first);
            auto rb=r.currentInputBank();
            r.sumPartitions(r.inputSpectra.data()+size_t(rb.start+rb.offset)*2*bins,rb,first);
            for(const auto* other:{&r.outputSpectrum}) {
              if(std::memcmp(a.outputSpectrum.data(),other->data(),size_t(2*bins)*sizeof(float))) return 1;
              comparisons+=2*bins;
            }
          }
        }
        if(immutable!=a.backing.impulseSpectra||immutable!=r.backing.impulseSpectra) return 2;
      }
      std::cout<<"PASS exact extracted scalar methods float comparisons="<<comparisons<<"\\n";
    }
'''
    (a.output/'kernels.cpp').write_text(source)
    flags=['-std=c++20','-O3','-DNDEBUG']
    if a.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    command=[a.compiler,*flags,str(a.output/'kernels.cpp'),'-o',str(a.output/'kernels')]
    (a.output/'command.json').write_text(json.dumps(command))
    subprocess.run(command,check=True)
    if platform.system()=='Darwin' and platform.machine()=='arm64':
        assembly=a.output/'kernels.s'
        subprocess.run([a.compiler,*flags,'-S',str(a.output/'kernels.cpp'),'-o',str(assembly)],check=True)
        if 'prfm' not in assembly.read_text():
            raise RuntimeError('Prefetch hint absent from native extracted-method assembly')
    result=subprocess.run([str(a.output/'kernels')],text=True,capture_output=True)
    (a.output/'stderr.txt').write_text(result.stderr)
    (a.output/'result.txt').write_text(result.stdout)
    result.check_returncode()
    (a.output/'result.txt').write_text(result.stdout);print(result.stdout,end='')
if __name__=='__main__':main()

#include "PostNativeDSP.h"
#include "reference/PostNativeDSPBeforeCache.h"
#include <chrono>
#include <iostream>
#include <iomanip>
using namespace spectralforge;
template<class DSP> double run(int rate,int block,int model,bool bypass){
 DSP dsp;dsp.prepare({double(rate),static_cast<juce::uint32>(block),2});auto state=defaultPostNativeState();
 for(auto& s:state.sections){s.selected=model;s.banks[model].bypass=bypass;}
 juce::AudioBuffer<float> b(2,block),input(2,block);for(int c=0;c<2;++c)for(int n=0;n<block;++n)input.setSample(c,n,.2f*std::sin(float(n)*.04f));
 auto process=[&]{for(int c=0;c<2;++c)b.copyFrom(c,0,input,c,0,block);for(int s=0;s<3;++s)dsp.processSection(b,s,state.sections[s]);};
 for(int i=0;i<100;++i)process();
 auto start=std::chrono::steady_clock::now();for(int i=0;i<1000;++i)process();auto end=std::chrono::steady_clock::now();
 if(!std::isfinite(b.getSample(0,0)))std::abort();
 return std::chrono::duration<double,std::micro>(end-start).count()/1000.;
}
int main(){std::cout<<"rate,block,model,bypass,repeat,order,before_us,after_us\n"<<std::setprecision(12);
 for(auto c:{std::pair{48000,256},std::pair{96000,64}})for(int m=0;m<3;++m)for(bool bypass:{true,false})for(int rep=0;rep<4;++rep){double a,b;if(rep%2==0){a=run<PostNativeReferenceDSP>(c.first,c.second,m,bypass);b=run<PostNativeDSP>(c.first,c.second,m,bypass);}else{b=run<PostNativeDSP>(c.first,c.second,m,bypass);a=run<PostNativeReferenceDSP>(c.first,c.second,m,bypass);}std::cout<<c.first<<','<<c.second<<','<<m<<','<<bypass<<','<<rep<<','<<(rep%2)<<','<<a<<','<<b<<'\n'<<std::flush;}
}

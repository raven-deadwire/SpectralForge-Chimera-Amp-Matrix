#include "TransientSplitPitch.h"
#include "CorrelationPitch.h"
#include <fstream>
#include <random>
#include <iostream>
std::vector<float> x;
template<class E,class P> std::vector<float> run(P prep,int shift,bool random,int maxb){
  E e;prep(e,maxb);e.setSemitones(shift);std::mt19937 r(5);std::uniform_int_distribution<int> d(1,maxb);
  size_t N=x.size()/2;std::vector<float> y(x.size());juce::AudioBuffer<float> in(2,maxb),out(2,maxb);
  for(size_t off=0;off<N;){int n=int(std::min<size_t>(random?d(r):128,N-off));in.setSize(2,n,false,false,true);out.setSize(2,n,false,false,true);
    for(int i=0;i<n;++i)for(int c=0;c<2;++c)in.setSample(c,i,x[2*(off+i)+c]);e.process(in,out);
    for(int i=0;i<n;++i)for(int c=0;c<2;++c)y[2*(off+i)+c]=out.getSample(c,i);off+=n;}
  return y;}
double md(const std::vector<float>&a,const std::vector<float>&b){double m=0;size_t first=a.size();for(size_t i=0;i<a.size();++i){double d=std::abs(a[i]-b[i]);if(d>0&&first==a.size())first=i;m=std::max(m,d);}std::cout<<" first="<<(first==a.size()?-1.0:first/2/48000.)<<" ";return m;}
int main(int argc,char**argv){std::ifstream f(argv[1],std::ios::binary|std::ios::ate);auto b=f.tellg();x.resize(b/4);f.seekg(0);f.read((char*)x.data(),b);
  int shift=std::stoi(argv[2]);
  struct C{const char* n;bool a,b,c,d,t;};
  for(C c:{C{"split",false,false,false,false,false},C{"low",true,false,false,false,false},C{"up",true,true,false,false,false},C{"lpc",true,true,true,false,false},C{"rt",true,true,true,true,false},C{"v2",true,true,true,true,true}}){
    auto prep=[&](transposeNext::TransientSplitPitch& e,int m){e.prepare({48000.,juce::uint32(m),2},c.a,c.b,c.c,c.d,c.t);};
    auto ref=run<transposeNext::TransientSplitPitch>(prep,shift,false,128);
    auto var=run<transposeNext::TransientSplitPitch>(prep,shift,true,512);
    std::cout<<c.n;double v=md(ref,var);std::cout<<v<<"\n";}
  auto prepc=[&](transposeNext::CorrelationPitch& e,int m){e.prepare({48000.,juce::uint32(m),2},40,true,true);};
  auto ref=run<transposeNext::CorrelationPitch>(prepc,shift,false,128);auto var=run<transposeNext::CorrelationPitch>(prepc,shift,true,512);
  std::cout<<"corr40-novelty";double v=md(ref,var);std::cout<<v<<"\n";}

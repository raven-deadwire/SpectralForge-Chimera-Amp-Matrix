#include <juce_dsp/juce_dsp.h>
#include "PerformanceUtilities.h"
#include "DoublerBaselines/Doubler190.h"
#include "DoublerBaselines/Doubler192.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__)
#include <time.h>
#endif

namespace {
using Current=spectralforge::PerformanceUtilities;
using Previous=Doubler192::PerformanceUtilities;
using Original=Doubler190::PerformanceUtilities;
constexpr double pi=juce::MathConstants<double>::pi;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
double db(double x){return 20*std::log10(std::max(1e-15,x));}
bool bits(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
struct Output {std::vector<float> l,r;};
// A full 0.13 Hz modulation cycle after a 100 ms warmup, not one LFO phase.
template<class DSP> Output render(const std::vector<float>& input,double sr,float spread,bool on=true,int block=128){
    DSP dsp;dsp.prepare({sr,juce::uint32(block),2});juce::AudioBuffer<float> buffer(2,block);
    Output o;o.l.resize(input.size());o.r.resize(input.size());
    for(size_t start=0;start<input.size();start+=size_t(block)){
        const int n=int(std::min(size_t(block),input.size()-start));buffer.setSize(2,n,false,false,true);
        for(int c=0;c<2;++c)buffer.copyFrom(c,0,input.data()+start,n);
        dsp.process(buffer,120,on,spread,false,false);
        for(int c=0;c<2;++c)std::copy_n(buffer.getReadPointer(c),n,(c==0?o.l:o.r).data()+start);
    }
    return o;
}
std::vector<float> fixture(const std::string& kind,double sr,double hz,double seconds=8.0){
    std::vector<float> x(size_t(sr*seconds));std::mt19937 rng(192);std::uniform_real_distribution<float> noise(-1,1);
    for(size_t n=0;n<x.size();++n){
        const double t=double(n)/sr;double v=0;
        if(kind=="tone")v=std::sin(2*pi*hz*t);
        else if(kind=="noise")v=noise(rng);
        else {
            const double local=std::fmod(t,.160);
            const bool mute=kind=="palm";
            const double env=mute ? (1-std::exp(-local/.0005))*std::exp(-local/.028) : (1-std::exp(-t/.001))*std::exp(-t/12);
            for(int h=1;h<=12 && hz*h<sr*.45;++h)v+=std::sin(2*pi*hz*h*(mute?local:t))/(h*h*.6+.4);
            v*=env;
        }
        x[n]=float(v);
    }
    float peak=0;for(float v:x)peak=std::max(peak,std::abs(v));
    for(float& v:x)v*=float(std::pow(10.,-.1))/peak; // -2 dBFS sample peak stress input
    return x;
}
struct Stats {double in{},out{},peak{},cross{},aa{},bb{},ss{},mm{},attackIn{},attackOut{};size_t count{},over{};};
void measure(const std::string& version,const std::string& kind,double hz,double sr,int spread,const std::vector<float>& x,const Output& o){
    Stats s;std::array<double,3> energy{},peak{},attack{};std::array<size_t,3> over{};
    double in=0;const size_t skip=size_t(sr*.1);
    for(size_t i=skip;i<x.size();++i){
        const double l=o.l[i],r=o.r[i],m=(l+r)/2,side=(l-r)/2;
        require(std::isfinite(l)&&std::isfinite(r),"non-finite output");in+=double(x[i])*x[i];
        s.cross+=l*r;s.aa+=l*l;s.bb+=r*r;s.ss+=side*side;s.mm+=m*m;
        const std::array<double,3> v{l,r,m};
        for(size_t c=0;c<3;++c){energy[c]+=v[c]*v[c];peak[c]=std::max(peak[c],std::abs(v[c]));over[c]+=std::abs(v[c])>1;}
    }
    double attackDry=0;int attacks=0;
    if(kind=="palm")for(int k=2;double(k)*.160+.003<double(x.size())/sr;++k){
        const size_t start=size_t(std::llround(k*.160*sr));const size_t span=size_t(sr*.003);
        std::array<double,3> a{};double dry=0;
        for(size_t i=start;i<start+span;++i){dry=std::max(dry,double(std::abs(x[i])));a[0]=std::max(a[0],double(std::abs(o.l[i])));a[1]=std::max(a[1],double(std::abs(o.r[i])));a[2]=std::max(a[2],std::abs((double(o.l[i])+o.r[i])/2));}
        attackDry+=db(dry);for(size_t c=0;c<3;++c)attack[c]+=db(a[c]);++attacks;
    }
    for(size_t c=0;c<3;++c)std::cout<<version<<','<<sr<<','<<spread<<','<<kind<<','<<hz<<','<<c<<','<<db(std::sqrt(energy[c]/in))<<','<<db(peak[c])<<','<<over[c]<<','<<s.cross/std::sqrt(s.aa*s.bb)<<','<<db(std::sqrt(s.ss/s.mm))<<','<<(attacks?(attack[c]-attackDry)/attacks:0)<<'\n';
}
void measurements(){
    std::cout<<"version,rate,spread_ms,fixture,hz,channel,rms_gain_db,peak_dbfs,samples_above_0dbfs,lr_correlation,side_mid_db,attack_3ms_db\n";
    for(double sr:{44100.,48000.,96000.}) {
        std::vector<std::pair<std::string,double>> specs;
        for(double hz:{27.5,40.,55.,80.,110.,160.,200.,250.,400.,1000.,3000.,6000.})specs.push_back({"tone",hz});
        for(auto spec:std::vector<std::pair<std::string,double>>{{"palm",27.5},{"palm",55.},{"bass",27.5},{"lead",880.},{"noise",0.}})specs.push_back(spec);
        for(const auto& [kind,hz]:specs){const auto x=fixture(kind,sr,hz);
            for(int spread=1;spread<=20;++spread){
                measure("190",kind,hz,sr,spread,x,render<Original>(x,sr,float(spread)));
                measure("192",kind,hz,sr,spread,x,render<Previous>(x,sr,float(spread)));
                measure("fixed",kind,hz,sr,spread,x,render<Current>(x,sr,float(spread)));
            }
        }
    }
}
void contracts(){
    std::mt19937 rng(27);std::uniform_real_distribution<float> noise(-.9f,.9f);
    for(double sr:{44100.,48000.,96000.,192000.})for(int channels:{1,2})for(int block:{1,64,128,257,1024}){
        Current current;Previous previous;current.prepare({sr,juce::uint32(block),juce::uint32(channels)});previous.prepare({sr,juce::uint32(block),juce::uint32(channels)});
        juce::AudioBuffer<float> a(channels,block),b(channels,block),dry(channels,block);
        for(int phase=0;phase<5;++phase){
            // Enough samples to complete on/off ramps, including single-sample blocks.
            const int blocks=int(sr*.065/block)+2;
            for(int k=0;k<blocks;++k){
                for(int c=0;c<channels;++c)for(int n=0;n<block;++n){float x=noise(rng);if(n%11==0)x=-0.f;a.setSample(c,n,x);b.setSample(c,n,x);dry.setSample(c,n,x);}
                const bool on=phase==1||phase==2;const bool click=phase==4;
                const float spread=phase==2?20.f:1.f;
                current.process(a,137,on,spread,click,k==0);previous.process(b,137,on,spread,click,k==0);
                for(int c=0;c<channels;++c)for(int n=0;n<block;++n){
                    require(std::isfinite(a.getSample(c,n)),"nonfinite automation output");
                    // Only signed zero is intentionally corrected at full bypass.
                    require(bits(a.getSample(c,n),b.getSample(c,n)) || (a.getSample(c,n)==0 && b.getSample(c,n)==0),"optimized audio differs from PR192");
                    if((phase==0 || (phase==3 && k*block>sr*.021)) && !click)
                        require(bits(a.getSample(c,n),dry.getSample(c,n)),"off not bit identical including signed zero");
                }
            }
        }
        current.reset();previous.reset();a.clear();b.clear();current.process(a,120,false,6,false,false);previous.process(b,120,false,6,false,false);
        for(int c=0;c<channels;++c)for(int n=0;n<block;++n)require(a.getSample(c,n)==0,"reset leaked history");
    }
    for(double sr:{44100.,48000.,96000.})for(int spread=1;spread<=20;++spread){
        // An impulse after settling proves immediate feedthrough; check both channels.
        Current dsp;dsp.prepare({sr,256,2});juce::AudioBuffer<float> b(2,256);b.clear();
        for(int k=0;k<40;++k)dsp.process(b,120,true,float(spread),false,false);
        b.clear();b.setSample(0,0,.5f);b.setSample(1,0,.5f);dsp.process(b,120,true,float(spread),false,false);
        require(b.getSample(0,0)>.4f&&b.getSample(1,0)>.4f,"dry feedthrough delayed");
        for(double hz:{27.5,55.}){
            const auto x=fixture("tone",sr,hz,1.0);const auto y=render<Current>(x,sr,float(spread));
            double xx=0,ll=0,rr=0,mm=0;
            for(size_t i=size_t(sr*.1);i<x.size();++i){xx+=double(x[i])*x[i];ll+=double(y.l[i])*y.l[i];rr+=double(y.r[i])*y.r[i];const double m=(double(y.l[i])+y.r[i])/2;mm+=m*m;}
            for(double e:{ll,rr,mm})require(std::abs(db(std::sqrt(e/xx)))<.5,"Drop A fundamental changed by more than 0.5 dB");
        }
    }
    const auto x=fixture("noise",48000,0,.6);const auto reference=render<Current>(x,48000,6,true,1);
    for(int block:{64,128,257,1024}){auto o=render<Current>(x,48000,6,true,block);require(o.l==reference.l&&o.r==reference.r,"block partition changed audio");}
    std::cout<<"PASS parity PR192: mono/stereo, four rates, five blocks, on/off/spread ramps, metronome; signed-zero bypass; reset; zero dry delay; A0/A1 1..20ms; block invariance\n";
}
double threadSeconds(){
#if defined(__linux__)
    timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return double(t.tv_sec)+double(t.tv_nsec)*1e-9;
#else
    return double(std::clock())/CLOCKS_PER_SEC;
#endif
}
volatile float sink=0;
template<class DSP> std::pair<double,double> bench(double sr,int block,int mode){
    DSP dsp;dsp.prepare({sr,juce::uint32(block),2});juce::AudioBuffer<float> b(2,block),source(2,block);
    for(int c=0;c<2;++c)for(int n=0;n<block;++n)source.setSample(c,n,float(.3*std::sin(n*.31+c*.2)));
    for(int k=0;k<100;++k){b.makeCopyOf(source,true);dsp.process(b,120,mode!=0,6,false,false);}
    const int loops=std::max(1,int(sr*1.0/block));double maxWall=0;std::vector<double> walls;walls.reserve(loops);
    const double started=threadSeconds();
    for(int k=0;k<loops;++k){
        b.copyFrom(0,0,source,0,0,block);b.copyFrom(1,0,source,1,0,block);
        const auto w=std::chrono::steady_clock::now();dsp.process(b,120,mode==2?(k%4<2):mode==1,6,false,false);
        const double duration=std::chrono::duration<double>(std::chrono::steady_clock::now()-w).count();maxWall=std::max(maxWall,duration);walls.push_back(duration);
        sink=b.getSample(0,block-1);
    }
    const double cpu=threadSeconds()-started;size_t overruns=0;for(double w:walls)overruns+=w>block/sr;
    // Absolute wall maximum includes scheduler interruptions, unlike thread CPU.
    std::cout<<cpu/(loops*block)*1e9<<','<<maxWall*1e6<<','<<overruns<<'\n';return {cpu,maxWall};
}
void benchmarkControl(){
    std::cout<<"version,rate,block,mode,repeat,thread_ns_per_stereo_frame,max_callback_wall_us,overruns\n";
    for(int rep=0;rep<15;++rep){
        std::cout<<"192_A,48000,128,1,"<<rep<<',';bench<Previous>(48000,128,1);
        std::cout<<"192_B,48000,128,1,"<<rep<<',';bench<Previous>(48000,128,1);
        std::cout<<"fixed,48000,128,1,"<<rep<<',';bench<Current>(48000,128,1);
    }
}
void benchmark(){
    std::cout<<"version,rate,block,mode,repeat,thread_ns_per_stereo_frame,max_callback_wall_us,overruns\n";
    for(double sr:{48000.,96000.})for(int block:{64,128,512})for(int mode:{0,1,2})for(int rep=0;rep<9;++rep){
        const auto run=[&](int which){std::cout<<(which==0?"190":which==1?"192":"fixed")<<','<<sr<<','<<block<<','<<mode<<','<<rep<<',';if(which==0)bench<Original>(sr,block,mode);else if(which==1)bench<Previous>(sr,block,mode);else bench<Current>(sr,block,mode);};
        if(rep%2){run(2);run(1);run(0);}else {run(0);run(1);run(2);}
    }
}
}
int main(int argc,char** argv){try{juce::ScopedNoDenormals noDenormals;if(argc>1&&std::string(argv[1])=="--measure")measurements();else if(argc>1&&std::string(argv[1])=="--benchmark")benchmark();else if(argc>1&&std::string(argv[1])=="--benchmark-control")benchmarkControl();else contracts();}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}return 0;}

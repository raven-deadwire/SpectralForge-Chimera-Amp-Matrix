#include "PostNativeDSP.h"
#include "FXChain.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace spectralforge;
namespace {
void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
float signal(int n,double rate){const double t=n/rate;const float env=std::fmod(t,.17)<.065?.65f:.07f;return env*float(.55*std::sin(2*juce::MathConstants<double>::pi*79*t)+.25*std::sin(2*juce::MathConstants<double>::pi*997*t)+.2*std::sin(2*juce::MathConstants<double>::pi*6357*t));}
struct Result{std::vector<float> samples;float meter{},gr{},leak{};};
Result render(int s,int m,PostNativeBank bank,int blockSize=128,double rate=48000,int length=24000){
    PostNativeDSP dsp;dsp.prepare({rate,(juce::uint32)blockSize,2});auto state=defaultPostNativeState();state.sections[s].selected=m;state.sections[s].banks[m]=bank;
    Result result;result.samples.resize((size_t)length);juce::AudioBuffer<float> b(2,blockSize);
    for(int offset=0;offset<length;offset+=blockSize){const int count=std::min(blockSize,length-offset);b.setSize(2,count,false,false,true);for(int n=0;n<count;++n){b.setSample(0,n,signal(offset+n,rate));b.setSample(1,n,0);}dsp.processSection(b,s,state.sections[s]);for(int n=0;n<count;++n){result.samples[(size_t)offset+n]=b.getSample(0,n);result.leak=std::max(result.leak,std::abs(b.getSample(1,n)));check(std::isfinite(b.getSample(0,n))&&std::abs(b.getSample(0,n))<=65,"nonfinite/unbounded");}}
    result.meter=dsp.meter(s);result.gr=dsp.reduction(s);return result;
}
double difference(const Result& a,const Result& b){double sum=0;for(size_t i=4000;i<a.samples.size();++i){const double d=a.samples[i]-b.samples[i];sum+=d*d;}return std::sqrt(sum/(a.samples.size()-4000));}
PostNativeBank usefulBank(int s,int m){auto b=defaultPostNativeState().sections[s].banks[m];b.bypass=false;if(s==0&&m==0){b.values[0]=-30;b.values[8]=1;}if(s==0&&m==1)b.values[0]=12;if(s==0&&m==2)b.values[1]=70;if(s==1&&m==1){b.values[1]=4;b.values[3]=1;}if(s==1&&m==2){b.values[0]=0;b.values[1]=2;}if(s==2&&m==0){b.values[0]=7;b.values[3]=9;b.values[6]=-8;b.values[9]=7;}if(s==2&&m==1){b.values[0]=7;b.values[1]=-8;b.values[3]=7;}if(s==2&&m==2){b.values[1]=6;b.values[2]=5;b.values[4]=7;b.values[6]=6;}return b;}
}
int main(){try{
    int controls=0,meters=0;double minimumControl=100;
    for(int s=0;s<3;++s)for(int m=0;m<3;++m){const auto& model=postNativeModel(s,m);auto base=usefulBank(s,m);const auto reference=render(s,m,base);check(reference.leak==0,"stereo leakage");
        const auto partition=render(s,m,base,37);check(difference(reference,partition)<1.e-6,"host block dependence");
        for(int c=0;c<model.controlCount;++c){const auto& p=model.controls[c];if(!p.connected)continue;auto lo=base,hi=base;
            if(s==1&&m==2&&c==9)lo.values[0]=hi.values[0]=2; // Independent INST gain only acts on INST.
            lo.values[c]=p.minimum;hi.values[c]=p.maximum;
            const auto a=render(s,m,lo),b=render(s,m,hi);const double diff=difference(a,b);
            const bool meterOnly=std::string(p.id)=="meter"||(s==1&&m==2&&c==11);
            if(meterOnly){check(std::abs(a.meter-b.meter)>1.e-4||diff>1.e-5,"meter mode not connected");++meters;}
            else {std::cout<<"control "<<s<<"/"<<m<<" "<<p.id<<" residual="<<diff<<"\n";check(diff>1.e-6,"control not connected: "+std::string(model.name)+" / "+p.id);minimumControl=std::min(minimumControl,diff);++controls;}
        }
        auto bypass=base;bypass.bypass=true;const auto dry=render(s,m,bypass);for(size_t n=0;n<dry.samples.size();++n)check(dry.samples[n]==signal((int)n,48000),"bypass not exact");
        if(s==0)check(reference.gr>.05f,"compressor GR missing");
    }
    int cases=0;float maximum=0;
    for(double rate:{44100.,48000.,96000.}){
        PostNativeDSP dsp;dsp.prepare({rate,128,2});auto state=defaultPostNativeState();juce::AudioBuffer<float> b(2,128);
        for(int cycle=0;cycle<96;++cycle){for(int s=0;s<3;++s){auto& section=state.sections[s];section.selected=(cycle/4)%3;auto& bank=section.banks[section.selected];bank.bypass=(cycle%17)==0;bank.trimDb=(cycle%3-1)*24.f;bank.levelDb=(cycle%5==0)?24.f:0.f;for(int c=0;c<postNativeModel(s,section.selected).controlCount;++c){const auto& spec=postNativeModel(s,section.selected).controls[c];bank.values[c]=(cycle%2)?spec.minimum:spec.maximum;}}sanitisePostNativeState(state);
            for(int n=0;n<128;++n){b.setSample(0,n,signal(cycle*128+n,rate)*4);b.setSample(1,n,-signal(cycle*128+n,rate)*2);}
            for(int s=0;s<3;++s)dsp.processSection(b,s,state.sections[s]);for(int c=0;c<2;++c)for(int n=0;n<128;++n){const float x=b.getSample(c,n);check(std::isfinite(x)&&std::abs(x)<=65,"stress overflow");maximum=std::max(maximum,std::abs(x));}++cases;
        }
    }
    // Reset eliminates prior model memory, and silence cannot generate signal.
    for(int s=0;s<3;++s)for(int m=0;m<3;++m){PostNativeDSP dsp;dsp.prepare({48000,128,2});auto state=defaultPostNativeState();state.sections[s].selected=m;state.sections[s].banks[m]=usefulBank(s,m);juce::AudioBuffer<float>b(2,128);for(int k=0;k<20;++k){for(int c=0;c<2;++c)for(int n=0;n<128;++n)b.setSample(c,n,signal(k*128+n,48000));dsp.processSection(b,s,state.sections[s]);}dsp.reset();b.clear();dsp.processSection(b,s,state.sections[s]);check(b.getMagnitude(0,128)==0,"reset silence leakage");}
    // POST retains the original fixed preamp latency even when its new bank is bypassed.
    {PostFXChain chain;chain.prepare({48000,128,2});FXState state;state.postNative=defaultPostNativeState();juce::AudioBuffer<float>b(2,128);b.clear();b.setSample(0,0,1);chain.process(b,state);int peak=0;for(int n=1;n<128;++n)if(std::abs(b.getSample(0,n))>std::abs(b.getSample(0,peak)))peak=n;check(peak==chain.latency(),"POST native preamp latency changed");}
    std::cout<<"POST native PASS: models=9 audio_controls="<<controls<<" meter_controls="<<meters<<" min_control_residual="<<minimumControl<<" rate_switch_stress_blocks="<<cases<<" peak="<<maximum<<" stereo_leak=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"POST native FAIL: "<<e.what()<<"\n";return 1;}}

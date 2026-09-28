#include "Amplifier.h"
#include "AmpNativeCatalog.h"
#include "AmpNativeTransitionTests.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace { std::atomic<bool> allocationWatch{false}; std::atomic<size_t> watchedAllocations{0}; }
void* operator new(std::size_t size) { if(allocationWatch.load(std::memory_order_relaxed))++watchedAllocations;if(auto* p=std::malloc(std::max(size,std::size_t{1})))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace spectralforge;
constexpr double pi=3.14159265358979323846;
void require(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
void set(AmpNativeState& s,std::string_view key,float value){const auto i=ampNativeControlIndex(s.model,key);if(i>=0)s.values[(size_t)i]=value;}
float sample(int n,double rate){const double t=n/rate;return(float)(.12*(1+.28*std::sin(2*pi*3.7*t))*(std::sin(2*pi*73.4*t)+.42*std::sin(2*pi*221*t)+.28*std::sin(2*pi*997*t)+.12*std::sin(2*pi*4511*t)));}
std::vector<float> render(AmpNativeState state,double rate=48000,int count=16800,float inputScale=1.f) {
    AmpNativeDSP dsp;dsp.prepare(rate);dsp.set(state);dsp.reset();std::vector<float> result((size_t)count);
    for(int n=0;n<count;++n){const float y=dsp.tick(inputScale*sample(n,rate),0);require(std::isfinite(y)&&std::abs(y)<64.f,"Nonfinite/extreme native sample");result[(size_t)n]=y;require(std::abs(dsp.tick(0,1))<1.e-8f,"Native stereo crosstalk");}
    return result;
}
double rms(const std::vector<float>& v,int from=2048){double s=0;for(size_t n=(size_t)from;n<v.size();++n)s+=double(v[n])*v[n];return std::sqrt(s/double(v.size()-(size_t)from));}
double difference(const std::vector<float>& a,const std::vector<float>& b,int from=2048){double s=0;for(size_t n=(size_t)from;n<a.size();++n)s+=std::pow(double(a[n])-b[n],2);return std::sqrt(s/double(a.size()-(size_t)from));}
AmpNativeState audibleControlState(int model,int control) {
    auto s=defaultAmpNativeState(model);const auto& p=ampNativePanel(model);const auto& k=p.controls[(size_t)control];
    if(!ampNativeControlVisible(model,control,s.channel))for(int ch=0;ch<(int)p.channels.size();++ch)if(ampNativeControlVisible(model,control,ch)){s.channel=ch;break;}
    s.soloEnabled=true;
    if(model==1)s.inputRoute=4;
    if(model==19)s.inputRoute=2;
    if(model==4){if(std::string_view(k.key)=="hw.geq.mode")s.channel=1;set(s,"hw.r2.presence",.8f);set(s,"hw.lead.presence",.8f);set(s,"hw.geq.mode",1);for(const auto hz:{"80","240","750","2200","6600"})set(s,std::string("hw.geq.")+hz,.72f);}
    if(model==6){set(s,"hw.biamp",1);set(s,"hw.low_master",.75f);set(s,"hw.high_master",.25f);}
    if(model==7){set(s,"hw.b7k.distortion",1);set(s,"hw.b7k.lo_mids",.8f);set(s,"hw.b7k.hi_mids",.8f);}
    if(model==5)set(s,"hw.ch1.midrange",.8f);
    if(model==10){set(s,"hw.overdrive.mid_level",.8f);set(s,"hw.overdrive.blend",.7f);}
    if(model==11){set(s,"hw.lo_mid",.8f);set(s,"hw.hi_mid",.8f);}
    if(model==12)set(s,"hw.master",.25f);
    if(model==15){set(s,"presence",.8f);for(int ch=1;ch<=4;++ch)set(s,"ch"+std::to_string(ch)+".gate",1);}
    if(model==18)set(s,"hw.midrange",.8f);
    if(model==20){set(s,"hw.gain_eq.middle",.8f);set(s,"hw.master1",.3f);set(s,"hw.master2",.8f);if(std::string_view(k.key)=="hw.master2")set(s,"hw.master2_select",1);}
    return s;
}
void catalog() {
    size_t count=0;for(int m=0;m<ampModelCount;++m){const auto& p=ampNativePanel(m);require(!p.controls.empty()&&p.controls.size()<=maxAmpNativeControls,"Native panel dimensions");require(p.defaultChannel<(int)p.channels.size(),"Native defaultchannel");
        auto s=defaultAmpNativeState(m);for(size_t c=0;c<p.controls.size();++c){const auto& k=p.controls[c];require(k.minimum<=k.initial&&k.initial<=k.maximum,"Native initial range");require(ampNativeControlIndex(m,k.key)==(int)c,"Duplicate native key");require(k.channelMask>0,"Invisible native control");require(s.values[c]==k.initial,"Native state default");++count;}}
    require(count==348,"Native control count changed without review");
    require(ampNativeContext(0,0)==0&&ampNativeContext(1,0)==1&&ampNativeContext(1,1)==2&&ampNativeContext(2,0)==3&&ampNativeContext(2,1)==4&&ampNativeContext(2,2)==5,"Six context identity");
    require(ampNativePanel(21).channels.size()==2,"Hot Lead normal route missing");
    auto bad=defaultAmpNativeState(3);bad.channel=99;bad.inputRoute=-99;bad.values[0]=NAN;bad.inputTrimDb=INFINITY;sanitiseAmpNativeState(bad);require(bad.channel==2&&bad.inputRoute==0&&bad.values[0]==ampNativePanel(3).controls[0].initial&&bad.inputTrimDb==0,"Native invalid values not sanitized");
    std::cout<<"PASS catalog 23 models, 348 controls, six contexts\n";
}
void controlResponses() {
    size_t tested=0,failed=0;double minimum=1e9;std::string least;
    for(int m=0;m<ampModelCount;++m){const auto& p=ampNativePanel(m);for(size_t c=0;c<p.controls.size();++c){auto lo=audibleControlState(m,(int)c),hi=lo;const auto& k=p.controls[c];lo.values[c]=k.minimum;hi.values[c]=k.maximum;
        const bool fx=std::string_view(k.group).find("FX")!=std::string_view::npos||std::string_view(k.key).find("reverb")!=std::string_view::npos||std::string_view(k.key).find("tremolo")!=std::string_view::npos;
        const bool gateSwitch=m==15&&std::string_view(k.key).find(".gate")!=std::string_view::npos;const float inputScale=gateSwitch?.012f:1.f;const auto a=render(lo,48000,fx?48000:16800,inputScale),b=render(hi,48000,fx?48000:16800,inputScale);const double d=difference(a,b);const std::string name=std::to_string(m)+":"+k.key;
        if(d<minimum){minimum=d;least=name;}if(d<1.e-6){++failed;std::cerr<<"UNRESPONSIVE "<<name<<" channel="<<lo.channel<<" residual="<<d<<" rms="<<rms(a)<<","<<rms(b)<<"\n";}++tested;
    }}
    std::cout<<"CONTROL_RESPONSE count="<<tested<<" failures="<<failed<<" minimum="<<minimum<<" at="<<least<<"\n";require(failed==0,"Native panel has unresponsive controls in its engaged route");
}
void routingAndRealtime() {
    size_t routes=0;float peak=0;
    for(double rate:{44100.,48000.,96000.})for(int m=0;m<ampModelCount;++m)for(int ch=0;ch<(int)ampNativePanel(m).channels.size();++ch){auto s=defaultAmpNativeState(m);s.channel=ch;AmpNativeDSP dsp;dsp.prepare(rate);dsp.set(s);dsp.reset();
        double energy=0;allocationWatch=true;for(int n=0;n<4096;++n){const float y=dsp.tick(sample(n,rate),0);energy+=double(y)*y;peak=std::max(peak,std::abs(y));}allocationWatch=false;require(energy>1.e-12&&std::isfinite(energy),"Silent/invalid native channel");
        bool extremesFinite=true;for(size_t c=0;c<ampNativePanel(m).controls.size();++c){s.values[c]=ampNativePanel(m).controls[c].maximum;allocationWatch=true;dsp.set(s);for(int n=0;n<16;++n){const float y=dsp.tick(.05f,0);extremesFinite=extremesFinite&&std::isfinite(y)&&std::abs(y)<64.f;}allocationWatch=false;}require(extremesFinite,"Native extremes invalid");++routes;
    }
    require(watchedAllocations==0,"Native set/tick allocated after prepare");
    for(int m:{0,1,5,8,10,12,19}){auto a=defaultAmpNativeState(m);const auto x=render(a);for(int r=1;r<(int)ampNativePanel(m).routes.size();++r){auto b=a;b.inputRoute=r;require(difference(x,render(b))>1.e-6,"Unresponsive input route");}}
    for(int m=0;m<ampModelCount;++m){const auto& p=ampNativePanel(m);if(p.channels.size()<2)continue;auto a=defaultAmpNativeState(m);a.channel=0;if(m==5)set(a,"hw.ch1.midrange",.8f);const auto x=render(a);for(int ch=1;ch<(int)p.channels.size();++ch){auto b=a;b.channel=ch;require(difference(x,render(b))>1.e-6,"Identical native channels "+std::to_string(m));}}
    std::cout<<"PASS sample-rates/channel routes="<<routes<<" peak="<<peak<<" allocations="<<watchedAllocations<<"\n";
}
void channelIsolationAndSoftwareLevels() {
    size_t checked=0;
    for(int m=0;m<ampModelCount;++m)for(int channel=0;channel<(int)ampNativePanel(m).channels.size();++channel){auto base=defaultAmpNativeState(m);base.channel=channel;const auto reference=render(base,48000,8192);const auto& panel=ampNativePanel(m);for(size_t c=0;c<panel.controls.size();++c)if(!ampNativeControlVisible(m,(int)c,channel)){auto changed=base;const auto& k=panel.controls[c];changed.values[c]=k.initial==k.maximum?k.minimum:k.maximum;require(difference(reference,render(changed,48000,8192))<1.e-8,"Inactive channel control leaked: "+std::to_string(m)+":"+k.key);++checked;}}
    auto base=defaultAmpNativeState(14);const auto dry=render(base);auto output=base;output.outputLevelDb=6;const auto lifted=render(output);const double ratio=rms(lifted)/rms(dry);require(std::abs(ratio-std::pow(10.,.3))<1.e-4,"Software output level not independent linear gain");
    auto input=base;input.inputTrimDb=6;const auto driven=render(input);require(difference(driven,lifted)>1.e-4,"Input trim incorrectly aliases output level");
    auto solo=defaultAmpNativeState(15);set(solo,"solo_level",.8f);const auto normal=render(solo);solo.soloEnabled=true;require(difference(normal,render(solo))>1.e-4,"Solo footswitch not connected");
    std::cout<<"PASS inactive channel controls isolated="<<checked<<" output level ratio="<<ratio<<"\n";
}
void engineTransition() {
    Amp changed,reference;for(auto* amp:{&changed,&reference}){amp->prepare({48000,128,2});amp->set(AmpModel::tight515,.5f);amp->reset();}
    juce::AudioBuffer<float> a(2,128),b(2,128);for(int block=0;block<24;++block){for(int n=0;n<128;++n)for(int c=0;c<2;++c)a.setSample(c,n,sample(block*128+n,48000));b.makeCopyOf(a,true);changed.process(a);reference.process(b);}
    auto native=defaultAmpNativeState(17);allocationWatch=true;changed.setNative(native);changed.set(AmpModel::fourChannel,.5f);allocationWatch=false;
    for(int n=0;n<128;++n)for(int c=0;c<2;++c)a.setSample(c,n,sample(24*128+n,48000));b.makeCopyOf(a,true);
    allocationWatch=true;changed.process(a);reference.process(b);allocationWatch=false;
    const float firstDifference=std::abs(a.getSample(0,0)-b.getSample(0,0));require(firstDifference<.002f,"Old to native engine switch did not preserve continuity");require(watchedAllocations==0,"Integrated engine switch allocated");
    bool finite=true;for(int block=0;block<24;++block){for(int n=0;n<128;++n)for(int c=0;c<2;++c)a.setSample(c,n,sample((25+block)*128+n,48000));allocationWatch=true;changed.process(a);allocationWatch=false;for(int n=0;n<128;++n)finite=finite&&std::isfinite(a.getSample(0,n));}require(finite,"Engine transition became invalid");
    std::cout<<"PASS old/native engine transition first difference="<<firstDifference<<" allocations="<<watchedAllocations<<"\n";
}
void integration() {
    size_t paths=0;
    for(int model=0;model<ampModelCount;++model)for(int os=0;os<4;++os){Amp amp;amp.prepare({48000,128,2});auto s=defaultAmpNativeState(model);amp.set(static_cast<AmpModel>(model),.5f);amp.setNative(s);amp.setOversampling(os);amp.reset();juce::AudioBuffer<float> b(2,128);double energy=0;for(int block=0;block<24;++block){for(int n=0;n<128;++n){b.setSample(0,n,sample(block*128+n,48000));b.setSample(1,n,0);}amp.process(b);for(int n=0;n<128;++n){const auto v=b.getSample(0,n);require(std::isfinite(v)&&std::abs(v)<64,"Integrated native sample invalid");require(std::abs(b.getSample(1,n))<1.e-8f,"Integrated stereo crossfeed");energy+=v*v;}}require(energy>1.e-12,"Integrated native signal absent");++paths;}
    std::cout<<"PASS integrated native oversampling paths="<<paths<<"\n";
}
}
int main(){try{catalog();routingAndRealtime();controlResponses();channelIsolationAndSoftwareLevels();engineTransition();require(ampNativeTransitionTests::run()==0,"Native trim/reverb/coefficient transitions failed");integration();std::cout<<"PASS AmpNativeTests\n";return 0;}catch(const std::exception& e){allocationWatch=false;std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}

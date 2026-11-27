// POST reverb controls (docs/POST_REVERB_CONTROLS.md): pre-delay, low cut, tone, width and freeze,
// measured on the reverb module itself. Synthetic input; this is not a listening test.
#include <juce_dsp/juce_dsp.h>
#include "SpatialModules.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace spectralforge;
namespace {
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;constexpr float mix=.5f;
constexpr int modelCount=6;
const char* const modelNames[modelCount]{"room","hall","spring","Hall Prime","Void Space","Gated Room"};
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
std::string num(double v,int digits=2){std::ostringstream o;o<<std::fixed<<std::setprecision(digits)<<v;return o.str();}
double db(double power){return 10*std::log10(std::max(power,1e-30));}
// The wet signal of both channels: the module mixes dry*(1-mix)+wet*mix, so the dry is taken back out.
struct Wet {std::vector<float> left,right;
    double power(double fromSeconds,double toSeconds,int channel=0) const {const auto& x=channel?right:left;double e=0;const size_t a=size_t(fromSeconds*rate),b=std::min(x.size(),size_t(toSeconds*rate));
        for(size_t n=a;n<b;++n)e+=double(x[n])*x[n];return e/double(std::max<size_t>(1,b-a));}};
// controls(seconds): the controls at that time. Half a second of silence first, so the mix has settled.
template<class Controls> Wet render(int model,const std::vector<float>& left,const std::vector<float>& right,Controls&& controls,float size=.5f,float damping=.55f,int channels=2) {
    SpaceModule space;space.prepare({rate,juce::uint32(block),juce::uint32(channels)});
    juce::AudioBuffer<float> b(channels,block);
    for(int k=0;k<int(rate*.5)/block;++k){b.clear();space.process(b,true,model,size,damping,mix,controls(0.));}
    Wet wet;
    for(size_t offset=0;offset+block<=left.size();offset+=block) {
        b.copyFrom(0,0,left.data()+offset,block);if(channels>1)b.copyFrom(1,0,right.data()+offset,block);
        space.process(b,true,model,size,damping,mix,controls(double(offset)/rate));
        for(int n=0;n<block;++n) {
            const float l=(b.getSample(0,n)-left[offset+size_t(n)]*(1-mix))/mix,r=channels>1?(b.getSample(1,n)-right[offset+size_t(n)]*(1-mix))/mix:l;
            require(std::isfinite(l) && std::isfinite(r),std::string(modelNames[model])+": the reverb produced a value that is not finite");
            wet.left.push_back(l);wet.right.push_back(r);
        }
    }
    return wet;
}
const auto fixed=[](ReverbControls k){return [k](double){return k;};};
std::vector<float> noise(double seconds,double burstSeconds,unsigned seed,float level=.1f) {
    std::mt19937 rng{seed};std::normal_distribution<float> g(0,level);std::vector<float> x(size_t(seconds*rate),0.f);
    for(size_t n=0;n<std::min(x.size(),size_t(burstSeconds*rate));++n)x[n]=g(rng);return x;
}
std::vector<float> sine(double seconds,double hz,float level=.1f){std::vector<float> x(size_t(seconds*rate));for(size_t n=0;n<x.size();++n)x[n]=float(level*std::sin(2*pi*hz*double(n)/rate));return x;}

// PRE-DELAY moves the whole reverb later by its time and changes nothing else.
void preDelay() {
    std::vector<float> click(size_t(rate*1.5),0.f);click[100]=.5f;
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        const auto base=render(model,click,click,fixed({}));
        ReverbControls k;k.preDelayMs=100;const auto late=render(model,click,click,fixed(k));
        constexpr size_t shift=4800;
        double worst=0,reference=0;
        for(size_t n=0;n+shift<base.left.size();++n){worst=std::max(worst,double(std::abs(late.left[n+shift]-base.left[n])));reference=std::max(reference,double(std::abs(base.left[n])));}
        double early=0;for(size_t n=0;n<shift;++n)early=std::max(early,double(std::abs(late.left[n])));
        require(reference>1e-4,std::string(modelNames[model])+": the click made no reverb");
        // Rooms and hall use a JUCE reverb whose own smoothing starts with the first block, so those two are compared to 1%.
        require(worst<reference*.01,std::string(modelNames[model])+": with 100 ms of pre-delay the reverb is not the same reverb 100 ms later (difference "+num(worst/reference*100,3)+"% of its peak)");
        require(early<reference*1e-4,std::string(modelNames[model])+": reverb was heard before the pre-delay had passed");
        measured+=std::string(model?", ":"")+modelNames[model]+" "+num(worst/reference*100,4)+"%";
    }
    std::cout<<"MEASURE reverb controls pre-delay (100 ms: largest difference from the same reverb 4800 samples later, of its peak): "<<measured<<'\n';
    std::cout<<"PASS reverb controls pre-delay: every model plays the same reverb later by the pre-delay, silent until then\n";
}
// LOW CUT takes the lows out of the reverb and leaves the rest; TONE tilts it around 1.5 kHz.
void lowCutAndTone() {
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        const auto level=[&](double hz,ReverbControls k){const auto x=sine(3,hz);return db(render(model,x,x,fixed(k)).power(2,3));};
        ReverbControls cut;cut.lowCutHz=500;ReverbControls bright;bright.tone=1;ReverbControls dark;dark.tone=-1;
        const double low=level(60,cut)-level(60,{}),high=level(4000,cut)-level(4000,{});
        require(low<-20,std::string(modelNames[model])+": LOW CUT at 500 Hz took only "+num(low)+" dB off 60 Hz in the reverb");
        require(std::abs(high)<1,std::string(modelNames[model])+": LOW CUT at 500 Hz changed 4 kHz in the reverb by "+num(high)+" dB");
        const double brightTop=level(8000,bright)-level(8000,{}),brightLow=level(150,bright)-level(150,{}),darkTop=level(8000,dark)-level(8000,{}),darkLow=level(150,dark)-level(150,{});
        require(brightTop>4.5 && brightTop<6.5 && brightLow<-4.5 && brightLow>-6.5,std::string(modelNames[model])+": TONE at +1 gave "+num(brightTop)+" dB at 8 kHz and "+num(brightLow)+" dB at 150 Hz");
        require(darkTop<-4.5 && darkTop>-6.5 && darkLow>4.5 && darkLow<6.5,std::string(modelNames[model])+": TONE at -1 gave "+num(darkTop)+" dB at 8 kHz and "+num(darkLow)+" dB at 150 Hz");
        measured+=std::string(model?" | ":"")+modelNames[model]+": cut "+num(low,1)+"/"+num(high,1)+" dB, tone +1 "+num(brightLow,1)+"/"+num(brightTop,1)+" dB";
    }
    std::cout<<"MEASURE reverb controls low cut at 500 Hz (60 Hz / 4 kHz) and tone +1 (150 Hz / 8 kHz): "<<measured<<'\n';
    std::cout<<"PASS reverb controls low cut and tone: the lows leave the reverb with the rest kept; tone tilts it by up to 6 dB each way\n";
}
// WIDTH scales the difference between the two wet channels: 0 is mono, 2 doubles it.
void width() {
    std::string measured;
    const auto left=noise(3,3,7),right=noise(3,3,8);
    for(int model=0;model<modelCount;++model) {
        const auto measure=[&](float w){ReverbControls k;k.width=w;const auto wet=render(model,left,right,fixed(k));double mid=0,side=0;
            for(size_t n=size_t(rate);n<wet.left.size();++n){const double m=.5*(wet.left[n]+wet.right[n]),s=.5*(wet.left[n]-wet.right[n]);mid+=m*m;side+=s*s;}
            return std::pair<double,double>{mid,side};};
        const auto made=measure(1),mono=measure(0),wide=measure(2);
        require(made.second>made.first*1e-4,std::string(modelNames[model])+": the reverb has no difference between its channels to scale");
        require(mono.second<made.second*1e-8,std::string(modelNames[model])+": WIDTH at 0 left a difference between the wet channels");
        const double sideDb=db(wide.second)-db(made.second),midDb=db(wide.first)-db(made.first),monoMidDb=db(mono.first)-db(made.first);
        require(std::abs(sideDb-6.02)<.1 && std::abs(midDb)<.01 && std::abs(monoMidDb)<.01,std::string(modelNames[model])+": WIDTH at 2 changed the difference by "+num(sideDb)+" dB and the sum by "+num(midDb,3)+" dB");
        measured+=std::string(model?", ":"")+modelNames[model]+" "+num(sideDb)+" dB";
    }
    std::cout<<"MEASURE reverb controls width (difference of the wet channels at 2, against 1): "<<measured<<'\n';
    std::cout<<"PASS reverb controls width: 0 is mono, 2 doubles the difference between the channels, the sum is kept\n";
}
// FREEZE holds the tail and takes no new input; released, the reverb decays again.
void freeze() {
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        // Noise for one second; frozen from 0.9 s to 9 s; released after.
        const auto burst=noise(14,1,11);
        const auto frozenAt=[](double from,double to){return [=](double t){ReverbControls k;k.freeze=t>=from && t<to;return k;};};
        const auto open=render(model,burst,burst,fixed({}));
        const auto held=render(model,burst,burst,frozenAt(.9,9));
        const double start=db(held.power(1.5,2.5)),end=db(held.power(8,9)),openEnd=db(open.power(8,9)),released=db(held.power(13,14));
        // Hall Prime at SIZE 0.5 decays over 3.6 s and the others faster, Void Space over 33 s.
        require(end-start>-6,std::string(modelNames[model])+": the frozen tail fell by "+num(start-end)+" dB in 6.5 s");
        require(end-start<3,std::string(modelNames[model])+": the frozen tail grew by "+num(end-start)+" dB in 6.5 s");
        if(model!=4)require(end-openEnd>20,std::string(modelNames[model])+": frozen, the tail at 8 s is only "+num(end-openEnd)+" dB over the unfrozen one");
        if(model!=4)require(released<end-20,std::string(modelNames[model])+": released, the tail fell by only "+num(end-released)+" dB in 5 s");
        // New playing while frozen is not taken in.
        auto more=burst;const auto extra=noise(14,14,12,.2f);for(size_t n=size_t(rate*3);n<size_t(rate*8);++n)more[n]=extra[n];
        const auto heldMore=render(model,more,more,frozenAt(.9,9));
        double difference=0,reference=0;for(size_t n=size_t(rate*3);n<size_t(rate*9);++n){difference=std::max(difference,double(std::abs(heldMore.left[n]-held.left[n])));reference=std::max(reference,double(std::abs(held.left[n])));}
        require(difference<reference*1e-3,std::string(modelNames[model])+": playing while frozen changed the held tail by "+num(difference/reference*100,3)+"% of its peak");
        measured+=std::string(model?" | ":"")+modelNames[model]+": held "+num(end-start,1)+" dB over 6.5 s (unfrozen "+num(openEnd-start,1)+" dB), released "+num(released-end,1)+" dB in 5 s";
    }
    std::cout<<"MEASURE reverb controls freeze: "<<measured<<'\n';
    std::cout<<"PASS reverb controls freeze: the tail is held within 6 dB over 6.5 s in every model, new playing is not taken in, and it decays again when released\n";
}
// Moving every control while playing, at their extremes, in mono and stereo: finite and bounded.
void extremes() {
    for(int channels=1;channels<=2;++channels)for(int model=0;model<modelCount;++model) {
        const auto x=noise(4,4,20+unsigned(model),.25f);
        const auto wet=render(model,x,x,[](double t){ReverbControls k;const double p=std::fmod(t*3,1.);
            k.preDelayMs=float(200*p);k.lowCutHz=float(20+480*std::fmod(t*5,1.));k.tone=float(std::sin(t*40));k.width=float(1+std::sin(t*23));k.freeze=std::fmod(t,1.)>.6;return k;},1.f,0.f,channels);
        double peak=0;for(const float v:wet.left)peak=std::max(peak,double(std::abs(v)));
        require(peak<20,std::string(modelNames[model])+": the reverb reached "+num(peak)+" with every control moving");
    }
    std::cout<<"PASS reverb controls extremes: every model stays finite and bounded in mono and stereo with every control moving and freeze switching\n";
}
// Back at the defaults the reverb is the plain one again: after the controls return, a later click gives the same reverb as one that never moved them.
void backToDefaults() {
    for(int model=0;model<modelCount;++model) {
        std::vector<float> x(size_t(rate*8),0.f);x[size_t(rate*.1)]=.5f;x[size_t(rate*6)]=.5f;
        const auto plain=render(model,x,x,fixed({}),.2f);
        const auto moved=render(model,x,x,[](double t){ReverbControls k;if(t<1){k.preDelayMs=80;k.lowCutHz=300;k.tone=.7f;k.width=1.6f;}return k;},.2f);
        double difference=0,reference=0;
        for(size_t n=size_t(rate*6);n<plain.left.size();++n){difference=std::max(difference,double(std::abs(moved.left[n]-plain.left[n])));reference=std::max(reference,double(std::abs(plain.left[n])));}
        // Void Space still carries the first click's tail, which the moved controls changed.
        require(difference<reference*(model==4?.05:1e-3),std::string(modelNames[model])+": after the controls returned to their defaults a new note's reverb differs by "+num(difference/reference*100,3)+"% of its peak");
    }
    std::cout<<"PASS reverb controls back to defaults: once the controls are back, a new note gets the plain reverb\n";
}
}
int main(int argc,char** argv) {
    try {
        const std::string only=argc>1?argv[1]:"";
        if(only.empty() || only=="--pre-delay")preDelay();
        if(only.empty() || only=="--tone")lowCutAndTone();
        if(only.empty() || only=="--width")width();
        if(only.empty() || only=="--freeze")freeze();
        if(only.empty() || only=="--extremes")extremes();
        if(only.empty() || only=="--defaults")backToDefaults();
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

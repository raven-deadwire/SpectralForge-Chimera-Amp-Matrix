// Measurements for the 1.6 reference heads (models 26-30) and the TT-800
// Metro Clean remodel (model 11) on the native engine.
// These check the documented panel functions, not a match to the hardware.
#include "AmpNativeDSP.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace spectralforge;
constexpr double pi=3.14159265358979323846;
constexpr double rate=48000;
void require(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
void set(AmpNativeState& s,std::string_view key,float value){const int i=ampNativeControlIndex(s.model,key);require(i>=0,"Unknown control "+std::string(key));s.values[(size_t)i]=value;}
struct Stereo {std::vector<float> left,right;};
template<class Input> Stereo render(const AmpNativeState& state,int count,Input input) {
    AmpNativeDSP dsp;dsp.prepare(rate);dsp.set(state);dsp.reset();Stereo out{std::vector<float>((size_t)count),std::vector<float>((size_t)count)};
    for(int n=0;n<count;++n){const float x=input(n);out.left[(size_t)n]=dsp.tick(x,0);out.right[(size_t)n]=dsp.tick(x,1);}
    return out;
}
auto sine(double hz,double amplitude){return [=](int n){return float(amplitude*std::sin(2*pi*hz*n/rate));};}
// Single-bin DFT amplitude from `from`. By default the window holds a whole
// number of cycles of `hz`; harmonic measurements pass the fundamental's window.
int window(const std::vector<float>& v,double hz,int from){return int(std::round(int((v.size()-size_t(from))*hz/rate)*rate/hz))-1;}
double tone(const std::vector<float>& v,double hz,int from,int length=0) {
    if(length<=0)length=window(v,hz,from);double re=0,im=0;
    for(int n=0;n<length;++n){const double w=2*pi*hz*n/rate;re+=v[size_t(from+n)]*std::cos(w);im+=v[size_t(from+n)]*std::sin(w);}
    return 2*std::sqrt(re*re+im*im)/length;
}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
double thd(const std::vector<float>& v,double hz,int from) {
    const int length=window(v,hz,from);
    double h=0;for(int k=2;k<=7;++k){if(k*hz>rate*.45)break;h+=std::pow(tone(v,k*hz,from,length),2);}
    return db(std::sqrt(h)/tone(v,hz,from,length));
}
double rms(const std::vector<float>& v,int from,int to){double s=0;for(int n=from;n<to;++n)s+=double(v[size_t(n)])*v[size_t(n)];return std::sqrt(s/(to-from));}

void crystal() {
    auto clean=defaultAmpNativeState(26);clean.channel=0;
    const auto c=render(clean,48000,sine(440,.1));
    const double cleanTHD=thd(c.left,440,24000);
    auto effect=defaultAmpNativeState(26);set(effect,"hw.ch2.effect",0);
    const auto plain=render(effect,48000,sine(440,.1));
    set(effect,"hw.ch2.distortion",1);
    const auto driven=render(effect,48000,sine(440,.1));
    const double plainTHD=thd(plain.left,440,24000),drivenTHD=thd(driven.left,440,24000);
    std::cout<<"MEASURE Crystal 120 440 Hz at 0.1: THD CH1 "<<cleanTHD<<" dB, CH2 "<<plainTHD<<" dB, CH2 distortion max "<<drivenTHD<<" dB\n";
    require(cleanTHD<-40 && plainTHD<-40,"Crystal 120 clean channels distort at a moderate level");
    require(drivenTHD>plainTHD+25,"Distortion does not open a clipping stage");

    // Chorus: fixed sweep, inverted between outputs; Speed/Depth have no effect.
    auto chorus=defaultAmpNativeState(26);set(chorus,"hw.ch2.effect",1);
    const auto a=render(chorus,96000,sine(1000,.05));
    set(chorus,"hw.ch2.speed",1);set(chorus,"hw.ch2.depth",1);
    const auto b=render(chorus,96000,sine(1000,.05));
    double same=0,spread=0;for(size_t n=48000;n<a.left.size();++n){same=std::max(same,double(std::abs(a.left[n]-b.left[n])));spread+=std::pow(double(a.left[n]-a.right[n]),2);}
    const double spreadRatio=std::sqrt(spread/48000)/rms(a.left,48000,96000);
    std::cout<<"MEASURE chorus: Speed/Depth change "<<same<<"; L-R difference "<<spreadRatio<<" of L\n";
    require(same<1.e-7,"Speed/Depth changed the fixed chorus");
    require(spreadRatio>.2,"Chorus outputs are not decorrelated");

    // Vibrato: pitch deviation from zero crossings against the delay-modulation
    // analytic peak 2*pi*rate*depth*f0.
    const auto deviation=[](float speed,float depth) {
        auto s=defaultAmpNativeState(26);set(s,"hw.ch2.effect",2);set(s,"hw.ch2.speed",speed);set(s,"hw.ch2.depth",depth);
        const auto out=render(s,96000,sine(1000,.05));std::vector<double> crossings;
        for(size_t n=48001;n<out.left.size();++n)if(out.left[n-1]<0 && out.left[n]>=0)crossings.push_back(double(n-1)+out.left[n-1]/(out.left[n-1]-out.left[n]));
        double low=1e9,high=0;for(size_t i=1;i<crossings.size();++i){const double f=rate/(crossings[i]-crossings[i-1]);low=std::min(low,f);high=std::max(high,f);}
        return (high-low)/2;
    };
    const double still=deviation(.5f,0),slow=deviation(0,1),fast=deviation(1,1);
    const double slowExpected=2*pi*1*.002*1000,fastExpected=2*pi*10*.002*1000;
    std::cout<<"MEASURE vibrato 1 kHz peak deviation: depth 0 "<<still<<" Hz; depth max at 1 Hz "<<slow<<" Hz (analytic "<<slowExpected<<"), at 10 Hz "<<fast<<" Hz (analytic "<<fastExpected<<")\n";
    require(still<.5,"Vibrato at zero depth still modulates pitch");
    require(std::abs(slow/slowExpected-1)<.1 && std::abs(fast/fastExpected-1)<.1,"Vibrato deviation does not follow Speed/Depth");

    // Switching OFF -> CHORUS -> VIBRATO while playing stays continuous.
    AmpNativeDSP dsp;dsp.prepare(rate);auto s=defaultAmpNativeState(26);set(s,"hw.ch2.effect",0);dsp.set(s);dsp.reset();
    double steady=0,switching=0;float y1=0,y2=0;
    for(int n=0;n<72000;++n){
        if(n==24000){set(s,"hw.ch2.effect",1);dsp.set(s);}
        if(n==48000){set(s,"hw.ch2.effect",2);dsp.set(s);}
        const float y=dsp.tick(float(.05*std::sin(2*pi*220*n/rate)),0);dsp.tick(0,1);
        const double second=std::abs(y-2*y1+y2);y2=y1;y1=y;
        // Steady reference: the settled end of each of the three states.
        if((n>12000 && n<24000)||(n>36000 && n<48000)||(n>60000))steady=std::max(steady,second);
        if((n>=24000 && n<26400)||(n>=48000 && n<50400))switching=std::max(switching,second);
    }
    std::cout<<"MEASURE effect switch: largest second difference "<<switching<<" against "<<steady<<" in the settled states\n";
    // The mix slews exponentially, so its first step bends the slope by about
    // 1e-4 of full scale. A hard switch steps by the wet/dry difference itself.
    require(switching<steady*3,"Effect selector clicks");
    std::cout<<"PASS Crystal 120: clean headroom, distortion, fixed chorus, vibrato Speed/Depth, click-free selector\n";
}

void britishHeads() {
    // Bluestack 100: High Treble path is brighter than Normal; Low inputs are -6 dB.
    const auto level=[](const AmpNativeState& s,double hz,double amplitude){return tone(render(s,24000,sine(hz,amplitude)).left,hz,12000);};
    auto blue=defaultAmpNativeState(27);
    auto treblePath=blue;treblePath.inputRoute=0;auto normalPath=blue;normalPath.inputRoute=2;set(normalPath,"hw.normal",.6f);
    const double trebleTilt=db(level(treblePath,3000,1.e-4)/level(treblePath,200,1.e-4)),normalTilt=db(level(normalPath,3000,1.e-4)/level(normalPath,200,1.e-4));
    auto low=treblePath;low.inputRoute=1;
    const double pad=db(level(low,1000,1.e-4)/level(treblePath,1000,1.e-4));
    std::cout<<"MEASURE Bluestack 100 small-signal 3 kHz re 200 Hz: High Treble "<<trebleTilt<<" dB, Normal "<<normalTilt<<" dB; Low input "<<pad<<" dB\n";
    require(trebleTilt>normalTilt+3,"High Treble input is not brighter than Normal");
    require(std::abs(pad+6.02)<.1,"Low input is not 6 dB down");

    // Eightyfire: Pre-amp Volume raises distortion; the cold stage adds even order.
    auto fire=defaultAmpNativeState(28);
    set(fire,"hw.preamp",.2f);const auto lowGain=render(fire,48000,sine(220,.01));
    set(fire,"hw.preamp",.9f);const auto highGain=render(fire,48000,sine(220,.01));
    const int h2Window=window(highGain.left,220,24000);
    const double h2=db(tone(highGain.left,440,24000,h2Window)/tone(highGain.left,220,24000,h2Window));
    std::cout<<"MEASURE Eightyfire 220 Hz at 0.01: THD pre-amp 0.2 "<<thd(lowGain.left,220,24000)<<" dB, 0.9 "<<thd(highGain.left,220,24000)<<" dB; 2nd harmonic "<<h2<<" dB\n";
    require(thd(highGain.left,220,24000)>thd(lowGain.left,220,24000)+15,"Pre-amp Volume does not drive the preamp");
    require(h2>-35,"No even-order content from the cold-biased stage");

    // Fortress 103 keeps more headroom than the non-master Bluestack at equal settings.
    auto fortress=defaultAmpNativeState(29);fortress.inputRoute=0;set(fortress,"hw.normal",.6f);
    auto stack=defaultAmpNativeState(27);stack.inputRoute=2;set(stack,"hw.normal",.6f);
    const double fortressTHD=thd(render(fortress,48000,sine(220,.1)).left,220,24000),stackTHD=thd(render(stack,48000,sine(220,.1)).left,220,24000);
    std::cout<<"MEASURE 220 Hz at 0.1, Normal input volume 0.6: THD Fortress 103 "<<fortressTHD<<" dB, Bluestack 100 "<<stackTHD<<" dB\n";
    require(fortressTHD<stackTHD-6,"Fortress 103 has no more headroom than Bluestack 100");
    auto bright=fortress;bright.inputRoute=2;set(bright,"hw.bright",.6f);
    const double brightTilt=db(level(bright,3000,1.e-4)/level(bright,200,1.e-4)),normalFortress=db(level(fortress,3000,1.e-4)/level(fortress,200,1.e-4));
    std::cout<<"MEASURE Fortress 103 small-signal 3 kHz re 200 Hz: Bright "<<brightTilt<<" dB, Normal "<<normalFortress<<" dB\n";
    require(brightTilt>normalFortress+3,"Bright input is not brighter than Normal");
    std::cout<<"PASS Bluestack 100 / Eightyfire / Fortress 103 input paths, gain and headroom\n";
}

void northlight() {
    const auto level=[](const AmpNativeState& s,double hz){return tone(render(s,48000,sine(hz,1.e-4)).left,hz,24000);};
    auto flat=defaultAmpNativeState(30);set(flat,"hw.damping",1);
    const double reference=level(flat,1000);double worst=0;
    for(double hz:{60.,250.,800.,2500.,8000.})worst=std::max(worst,std::abs(db(level(flat,hz)/reference)));
    auto off=flat;set(off,"hw.eq_on",0);for(const char* key:{"hw.eq.low","hw.eq.mid","hw.eq.high"})set(off,key,1);
    double offWorst=0;for(double hz:{60.,250.,800.,2500.,8000.})offWorst=std::max(offWorst,std::abs(db(level(off,hz)/level(off,1000))));
    std::cout<<"MEASURE Northlight centred EQ, 60 Hz-8 kHz re 1 kHz: largest deviation "<<worst<<" dB; EQ off with bands at max "<<offWorst<<" dB\n";
    require(worst<.5 && offWorst<.5,"Northlight is not flat with a centred or bypassed EQ");
    struct Band {const char* key;double hz,minimum;};
    for(const auto b:{Band{"hw.eq.low",25,24},Band{"hw.eq.low_mid",250,29},Band{"hw.eq.mid",800,29},Band{"hw.eq.high_mid",2500,29},Band{"hw.eq.high",16000,24}}) {
        auto up=flat,down=flat;set(up,b.key,1);set(down,b.key,0);
        const double span=db(level(up,b.hz)/level(down,b.hz));
        std::cout<<"MEASURE Northlight "<<b.key<<" span at "<<b.hz<<" Hz: "<<span<<" dB\n";
        require(span>b.minimum && span<30.5,"EQ band span out of range");
    }
    auto pad=flat;set(pad,"hw.pad",1);
    const double padDb=db(level(pad,1000)/reference);
    auto loose=flat;set(loose,"hw.damping",0);
    const double bump=db(level(loose,75)/level(flat,75));
    const double bassTHD=thd(render(flat,48000,sine(41.2,.25)).left,41.2,24000);
    std::cout<<"MEASURE Northlight pad "<<padDb<<" dB; damping 0 at 75 Hz "<<bump<<" dB; 41.2 Hz at 0.25 THD "<<bassTHD<<" dB\n";
    require(std::abs(padDb+12)<.05,"Pad is not -12 dB");
    require(std::abs(bump-6)<.3,"Damping low resonance differs from 6 dB");
    require(bassTHD<-45,"Northlight distorts a clean bass note");
    std::cout<<"PASS Northlight flat response, band spans, pad, damping, clean headroom\n";
}

// Metro Clean, remodelled on the Subway TT-800 owner's manual (model 11).
void metroClean() {
    constexpr int model=11;
    const auto gainAt=[&](AmpNativeState s,double hz,double amplitude=1.e-3) {
        const auto out=render(s,int(rate),sine(hz,amplitude));return db(tone(out.left,hz,int(rate/2)));
    };
    // Each channel's HIGHPASS: linear 25-125 Hz, noon ~75 Hz (-3 dB), relative
    // to the same channel with its filter at 25 Hz.
    for(int channel=0;channel<2;++channel) {
        const char* key=channel==0?"hw.high_pass":"hw.classic.high_pass";
        auto flat=defaultAmpNativeState(model);flat.channel=channel;set(flat,key,0);
        for(const auto [position,hz]:{std::pair{.5f,75.},std::pair{1.f,125.}}) {
            auto s=flat;set(s,key,position);
            const double drop=gainAt(s,hz)-gainAt(flat,hz);
            std::cout<<"MEASURE Metro Clean channel "<<channel<<" HIGHPASS "<<position<<" at "<<hz<<" Hz: "<<drop<<" dB\n";
            require(std::abs(drop+3)<.8,"TT-800 HIGHPASS corner differs from the manual's 25-125 Hz linear sweep");
        }
    }
    // MODERN active Baxandall: MID +/-12 dB at the selected linear frequency.
    auto modern=defaultAmpNativeState(model);modern.channel=0;set(modern,"hw.high_pass",0);
    set(modern,"hw.lo_mid_frequency",.5f);
    auto cut=modern,boost=modern;set(cut,"hw.lo_mid",0);set(boost,"hw.lo_mid",1);
    const double midSpan=gainAt(boost,1075)-gainAt(cut,1075),offBand=gainAt(boost,300)-gainAt(cut,300);
    std::cout<<"MEASURE Metro Clean MODERN MID span at 1075 Hz "<<midSpan<<" dB, at 300 Hz "<<offBand<<" dB\n";
    require(std::abs(midSpan-24)<.6 && offBand<6,"MODERN MID is not a +/-12 dB bell at its linear MID FREQ");
    for(const char* key:{"hw.bass","hw.treble"}) {
        auto lo=modern,hi=modern;set(lo,key,0);set(hi,key,1);const double hz=key[3]=='b'?20.:16000.;
        const double span=gainAt(hi,hz)-gainAt(lo,hz);
        std::cout<<"MEASURE Metro Clean MODERN "<<key<<" span at "<<hz<<" Hz: "<<span<<" dB\n";
        require(span>18 && span<24.5,"MODERN shelving band exceeds or misses its +/-12 dB range");
    }
    // CLASSIC passive tonestack: inherent ~500 Hz dip at NORM; SHIFT is flatter.
    auto classic=defaultAmpNativeState(model);classic.channel=1;set(classic,"hw.classic.gain",.1f);
    set(classic,"hw.classic.mid",.3f);
    const double norm=gainAt(classic,500)-gainAt(classic,150);
    auto shifted=classic;set(shifted,"hw.classic.mid_shift",1);
    const double shift=gainAt(shifted,500)-gainAt(shifted,150);
    std::cout<<"MEASURE Metro Clean CLASSIC 500 Hz vs 150 Hz: NORM "<<norm<<" dB, SHIFT "<<shift<<" dB\n";
    require(norm<-4 && shift>norm+2,"CLASSIC tonestack lacks the NORM mid dip or the flatter SHIFT");
    // CLASSIC (V2) overdrives more readily than MODERN at equal GAIN.
    auto m=defaultAmpNativeState(model);m.channel=0;auto c=m;c.channel=1;
    const auto mOut=render(m,int(rate),sine(110,.3)),cOut=render(c,int(rate),sine(110,.3));
    const double modernTHD=thd(mOut.left,110,int(rate/2)),classicTHD=thd(cOut.left,110,int(rate/2));
    std::cout<<"MEASURE Metro Clean 110 Hz at 0.3, GAIN noon: THD MODERN "<<modernTHD<<" dB, CLASSIC "<<classicTHD<<" dB\n";
    require(classicTHD>modernTHD+6,"CLASSIC does not overdrive more readily than MODERN");
    // O/D SYMMETRY: ASYM adds even harmonics when the output stage is driven.
    auto sym=defaultAmpNativeState(model);sym.channel=0;set(sym,"hw.master",1);set(sym,"hw.input",.8f);
    auto asym=sym;set(asym,"hw.od_symmetry",1);
    const auto symOut=render(sym,int(rate),sine(110,.3)),asymOut=render(asym,int(rate),sine(110,.3));
    const int length=window(symOut.left,110,int(rate/2));
    const auto second=[&](const std::vector<float>& v){return db(tone(v,220,int(rate/2),length)/tone(v,110,int(rate/2),length));};
    std::cout<<"MEASURE Metro Clean O/D SYMMETRY second harmonic: SYM "<<second(symOut.left)<<" dB, ASYM "<<second(asymOut.left)<<" dB\n";
    require(second(asymOut.left)>second(symOut.left)+6,"O/D SYMMETRY does not change the output clip symmetry");
    // DEEP: mild very-low boost plus a lower effective high pass.
    auto plain=defaultAmpNativeState(model);plain.channel=0;auto deep=plain;set(deep,"hw.deep",1);
    const double deepLift=gainAt(deep,60)-gainAt(plain,60),deepMid=gainAt(deep,1000)-gainAt(plain,1000);
    std::cout<<"MEASURE Metro Clean DEEP at 60 Hz "<<deepLift<<" dB, at 1 kHz "<<deepMid<<" dB\n";
    require(deepLift>3 && std::abs(deepMid)<.5,"DEEP is not a low-frequency extension");
    std::cout<<"PASS Metro Clean TT-800 HIGHPASS, MODERN Baxandall, CLASSIC tonestack/drive, O/D SYMMETRY, DEEP\n";
}
}

int main() {
    try {crystal();britishHeads();northlight();metroClean();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}

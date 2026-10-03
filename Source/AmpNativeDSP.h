#pragma once
#include "AmpNativeCatalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge {
namespace ampNativeDetail {
// Authored reduced-order circuits. Panel topology and control functions are
// represented explicitly; coefficients/tapers are not measured hardware fits.
inline float db(float value) noexcept { return std::pow(10.f, value * .05f); }
inline float pole(double rate, float hz) noexcept { return float(-std::expm1(-6.283185307179586 * std::min(double(hz), rate * .44) / rate)); }
inline float soft(float x) noexcept { return x / std::sqrt(1.f + x * x); }
inline float volume(float p) noexcept { return p * p * 4.f; } // centre = unity, zero = silence
inline float gain(float p) noexcept { return db((p - .5f) * 36.f); }
inline float tone(float p, float range = 12.f) noexcept { return (p - .5f) * 2.f * range; }
inline float frequency(float p, float low, float high) noexcept { return low * std::pow(high / low, p); }
template<class State> bool solo(const State& s) noexcept {
    if constexpr (requires { s.soloEnabled; }) return s.soloEnabled;
    return false;
}
struct Coeff {
    double b0{1}, b1{}, b2{}, a1{}, a2{};
    Coeff difference(const Coeff& start,double divisor) const noexcept {
        return {(b0-start.b0)/divisor,(b1-start.b1)/divisor,(b2-start.b2)/divisor,
                (a1-start.a1)/divisor,(a2-start.a2)/divisor};
    }
    void advance(const Coeff& delta) noexcept {
        b0+=delta.b0;b1+=delta.b1;b2+=delta.b2;a1+=delta.a1;a2+=delta.a2;
    }
    static Coeff make(double rate, int type, float hz, float amount = 0.f, float q = .70710678f) noexcept {
        const double w = 6.283185307179586 * std::clamp(double(hz), 5., rate * .44) / rate;
        const double c = std::cos(w), sn = std::sin(w), a = std::pow(10., double(amount) / 40.), alpha = sn / (2. * q);
        double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
        if (type == 0) { b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha; } // HP
        if (type == 1) { b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha; } // LP
        if (type == 2) { b0=1+alpha*a;b1=-2*c;b2=1-alpha*a;a0=1+alpha/a;a1=-2*c;a2=1-alpha/a; } // peak
        const double t=2*std::sqrt(a)*alpha;
        if (type == 3) { b0=a*((a+1)-(a-1)*c+t);b1=2*a*((a-1)-(a+1)*c);b2=a*((a+1)-(a-1)*c-t);a0=(a+1)+(a-1)*c+t;a1=-2*((a-1)+(a+1)*c);a2=(a+1)+(a-1)*c-t; }
        if (type == 4) { b0=a*((a+1)+(a-1)*c+t);b1=-2*a*((a-1)+(a+1)*c);b2=a*((a+1)+(a-1)*c-t);a0=(a+1)-(a-1)*c+t;a1=2*((a-1)-(a+1)*c);a2=(a+1)-(a-1)*c-t; }
        return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
    }
};
struct Filter {
    double z1{}, z2{};
    float tick(float x, const Coeff& c) noexcept { const double y=c.b0*x+z1;z1=c.b1*x-c.a1*y+z2;z2=c.b2*x-c.a2*y;return float(y); }
};
struct RC {
    float z{};
    float low(float x,float a) noexcept { z+=a*(x-z);return z; }
    float high(float x,float a) noexcept { return x-low(x,a); }
};
struct Stage {
    RC coupling, bandwidth;
    float grid{}, cathode{};
    float tick(float input,float drive,float hp,float lp,float bias,float gridAttack,float gridRelease,float cathodeRate,float blocking) noexcept {
        const float v=std::clamp(input*drive/(1.f+.12f*cathode),-32.f,32.f);
        const float demand=std::max(0.f,v-.9f);
        grid+=(demand>grid?gridAttack:gridRelease)*(demand-grid);
        const float point=bias-blocking*grid;
        float y=soft(v+point)-soft(point);
        cathode+=cathodeRate*(std::abs(y)-cathode);
        return bandwidth.low(coupling.high(y,hp),lp);
    }
};
// Four-line feedback network, evaluated near 24 kHz even inside 8x
// oversampling. This is a spring-like algorithm, not a spring impulse response.
// Delay lengths are allocated in prepare only; state changes cannot allocate.
struct Spring {
    std::array<std::vector<float>,4> delay;
    std::array<size_t,4> index{};
    std::array<float,4> damping{};
    float previous{}, next{}, inputSum{}, antiAlias{}, wetLow{};
    int divider{2}, phase{};
    double internalRate{24000};
    void prepare(double rate) {
        divider=std::max(1,int(std::round(rate/24000.)));internalRate=rate/divider;
        constexpr std::array<double,4> times{{.0271,.0337,.0419,.0503}};
        for(size_t i=0;i<4;++i)delay[i].assign(size_t(std::max(8.,std::round(internalRate*times[i]))),0.f);
        reset();
    }
    void reset() noexcept { for(auto& d:delay)std::fill(d.begin(),d.end(),0.f);index={};damping={};previous=next=inputSum=antiAlias=wetLow=0;phase=0; }
    float tick(float x,float dampingPole,float wetPole,float decay) noexcept {
        // Averaging + a one-pole input filter reduce decimation aliasing; the
        // wet path remains deliberately dark like the represented tank.
        antiAlias+=.25f*(x-antiAlias);inputSum+=antiAlias;
        if (++phase>=divider) {
            phase=0;previous=next;
            std::array<float,4> d{};
            for(size_t i=0;i<4;++i){d[i]=delay[i][index[i]];damping[i]+=dampingPole*(d[i]-damping[i]);d[i]=damping[i];}
            const std::array<float,4> h{{(d[0]+d[1]+d[2]+d[3])*.5f,(d[0]-d[1]+d[2]-d[3])*.5f,(d[0]+d[1]-d[2]-d[3])*.5f,(d[0]-d[1]-d[2]+d[3])*.5f}};
            const float excitation=inputSum/float(divider)*.22f;inputSum=0;
            for(size_t i=0;i<4;++i){delay[i][index[i]]=excitation*(i%2?-.8f:1.f)+decay*h[i];if(++index[i]>=delay[i].size())index[i]=0;}
            next=(d[0]-d[1]+d[2]+d[3])*.5f;
        }
        const float y=previous+(next-previous)*float(phase)/float(divider);
        wetLow+=wetPole*(y-wetLow);return wetLow;
    }
};
struct Config {
    int model{}, channel{}, stages{2}, eqAfter{1}, topology{};
    int preCount{}, postCount{};
    std::array<Coeff,8> pre{};
    std::array<Coeff,12> post{};
    Coeff input{}, output{}, lowPower{}, highPower{};
    std::array<float,6> stageGain{{1.8f,1.2f,1.2f,1.2f,1.1f,1.f}};
    std::array<float,6> stageHP{}, stageLP{}, stageBias{};
    std::array<float,11> scalar{{1,1,1,1,1,0,1,1,1,1,1}}; // pre/inter/channel/global/attenuation/blend/normal/bright/aux/input trim/output level
    float powerDrive{1.3f}, sag{.15f}, blocking{.05f}, powerBias{.02f};
    float reverb{}, reverbTone{.5f}, reverbDecay{.73f}, tremRate{4}, tremDepth{};
    float gateThreshold{}, crossover{800}, resonanceHz{90}, presenceHz{2700};
    float hybridDrive{1}, hybridLevel{1}, hybridBlend{.5f}, hybridMaster{1};
    bool hybridDistortion{}, biamp{}, mute{}, solo{}, reverbPresent{};
    float highMaster{1}, lowMaster{1};
};
struct Channel {
    std::array<Stage,6> stage{};
    std::array<Filter,8> pre{};
    std::array<Filter,12> post{};
    std::array<Coeff,8> preCoeff{},preDelta{};
    std::array<Coeff,12> postCoeff{},postDelta{};
    Coeff inputCoeff{},outputCoeff{},inputDelta{},outputDelta{};
    int preCount{},postCount{},coefficientRemaining{},coefficientPhase{};
    Filter input, output, powerLow, powerHigh;
    RC parallelLow, crossover, hybridLow, reservoirLow, dc;
    Spring spring;
    std::array<float,11> scalar{};
    float supply{}, detector{}, gate{1}, phase{};
    float hybridDrive{1},hybridLevel{1},hybridBlend{.5f},hybridMaster{1},highMaster{1},lowMaster{1},reverb{},tremDepth{};
    int smoothRemaining{};
    void finishCoefficients(const Config& target) noexcept {
        preCoeff=target.pre;postCoeff=target.post;inputCoeff=target.input;outputCoeff=target.output;
        preCount=target.preCount;postCount=target.postCount;coefficientRemaining=coefficientPhase=0;
    }
    void scheduleCoefficients(const Config& target,double rate) noexcept {
        // A convex interpolation between stable second-order denominators stays
        // inside the Schur stability triangle. Numerators ramp with the poles.
        // Unused target entries are identity filters: removed bands fade out
        // while their storage keeps advancing until the transition completes.
        coefficientRemaining=std::max(1,int(std::round(rate*.020/16.)));
        coefficientPhase=0;const double steps=coefficientRemaining;
        inputDelta=target.input.difference(inputCoeff,steps);outputDelta=target.output.difference(outputCoeff,steps);
        for(size_t i=0;i<preCoeff.size();++i)preDelta[i]=target.pre[i].difference(preCoeff[i],steps);
        for(size_t i=0;i<postCoeff.size();++i)postDelta[i]=target.post[i].difference(postCoeff[i],steps);
        preCount=std::max(preCount,target.preCount);postCount=std::max(postCount,target.postCount);
    }
    void advanceCoefficients(const Config& target) noexcept {
        if(coefficientRemaining==0||++coefficientPhase<16)return;
        coefficientPhase=0;
        if(--coefficientRemaining==0){finishCoefficients(target);return;}
        inputCoeff.advance(inputDelta);outputCoeff.advance(outputDelta);
        for(int i=0;i<preCount;++i)preCoeff[size_t(i)].advance(preDelta[size_t(i)]);
        for(int i=0;i<postCount;++i)postCoeff[size_t(i)].advance(postDelta[size_t(i)]);
    }
    void clear(const Config& c) noexcept {
        stage={};pre={};post={};input={};output={};powerLow={};powerHigh={};parallelLow={};crossover={};hybridLow={};reservoirLow={};dc={};
        spring.reset();scalar=c.scalar;smoothRemaining=0;supply=detector=phase=0;gate=1;hybridDrive=c.hybridDrive;hybridLevel=c.hybridLevel;hybridBlend=c.hybridBlend;hybridMaster=c.hybridMaster;highMaster=c.highMaster;lowMaster=c.lowMaster;reverb=c.reverb;tremDepth=c.tremDepth;
        finishCoefficients(c);
    }
};
} // namespace ampNativeDetail

class AmpNativeDSP {
    using D=ampNativeDetail::Coeff;
    ampNativeDetail::Config config;
    std::array<ampNativeDetail::Channel,2> channels;
    AmpNativeState current{};
    double rate{48000};
    bool valid{};
    float smooth{}, gridAttack{}, gridRelease{}, cathodeRate{}, supplyAttack{}, supplyRelease{}, gateAttack{},gateRelease{}, dcPole{}, parallelPole{}, crossoverPole{}, verbDamping{},verbOutput{}, phaseStep{};
    void add(bool pre,int type,float hz,float amount=0.f,float q=.70710678f) noexcept {
        if(pre){if(config.preCount<int(config.pre.size()))config.pre[size_t(config.preCount++)]=D::make(rate,type,hz,amount,q);}
        else {if(config.postCount<int(config.post.size()))config.post[size_t(config.postCount++)]=D::make(rate,type,hz,amount,q);}
    }
    void eq(bool pre,float bass,float middle,float treble,float lowHz=100,float midHz=650,float highHz=3300,float span=12) noexcept {
        using namespace ampNativeDetail;
        add(pre,3,lowHz,tone(bass,span));add(pre,2,midHz,tone(middle,span),.7f);add(pre,4,highHz,tone(treble,span));
    }
    void presence(float amount,float hz=2700) noexcept {add(false,4,hz,ampNativeDetail::tone(amount,9));}
    void depth(float amount,float hz=90) noexcept {add(false,2,hz,ampNativeDetail::tone(amount,9),.8f);}
    void configure(const AmpNativeState& s) noexcept {
        using namespace ampNativeDetail;
        config={};config.model=s.model;config.channel=s.channel;config.solo=solo(s);config.scalar[9]=db(s.inputTrimDb);config.scalar[10]=db(s.outputLevelDb);
        const int c=s.channel,r=s.inputRoute;
        config.reverbPresent=(s.model==0&&c==1)||s.model==4||s.model==8||s.model==9||s.model==16;
        const auto a=[&s](int i){return s.values[size_t(i)];};
        float hp=28,lp=15000,bright=0,preLow=0;
        // Every panel has explicit positional bindings below. A control is
        // inactive only when its physical channel/route/enable switch is inactive.
        switch(s.model) {
        case 0: { // Two independent Fender-style channel preamps, FX only on Vibrato.
            const int b=c*5;config.scalar[0]=volume(a(b));config.stages=2;config.stageGain={1.25f,1.1f,1,1,1,1};config.scalar[4]=.95f;
            eq(true,a(b+3),a(b+2),a(b+1),85,540,3200,11);bright=a(b+4)*(1-a(b))*7;
            hp=18;lp=17000;config.sag=.09f;config.scalar[8]=r==1?.5f:1.f;
            if(c==1){config.reverb=a(10)*.7f;config.tremRate=1.5f+a(11)*7;config.tremDepth=a(12)*.8f;}
            break; }
        case 1: { // Independent Normal / High Treble input paths, mixed when jumped.
            config.topology=1;config.scalar[6]=(r==2||r==3||r==4)?volume(a(1)):0;config.scalar[7]=(r==0||r==1||r==4)?volume(a(0)):0;
            config.scalar[8]=(r==1||r==3)?.5f:1.f;config.stageGain={3.8f,2.3f,1,1,1,1};eq(false,a(4),a(3),a(2),110,650,3200);presence(a(5),3100);hp=35;lp=11200;config.sag=.24f;config.powerDrive=1.7f;break; }
        case 2: {
            config.stages=c==1?4:(a(10)>.5f?3:2);config.stageGain={3.5f,2.3f,1.7f,1.3f,1,1};config.scalar[0]=gain(a(c==1?7:5));config.scalar[2]=volume(a(c==1?8:6));
            eq(false,a(0),a(1),a(2),110,650,3600);depth(a(3),95);presence(a(4),2300);bright=c==0?a(9)*6:0;hp=65;lp=12300;config.scalar[8]=r==1?.5f:1.f;config.sag=.08f;break; }
        case 3: {
            const int b=c*7,mode=int(a(b+6));config.stages=c==0?(mode==0?2:3):(mode==0?3:4);config.stageGain={3.4f,2.4f,1.7f,1.3f,1,1};
            config.scalar[0]=gain(a(b))*(mode==2?1.25f:1.f);config.scalar[2]=volume(a(b+5));config.scalar[3]=volume(a(21));if(config.solo)config.scalar[3]*=1.f+a(22)*1.5f;
            eq(false,a(b+3),a(b+2),a(b+1),85,520,3200);presence(a(b+4),mode==2?2100.f:3400.f);hp=c==0?24:48;lp=10500;
            config.sag=a(23)>.5f?.38f:.18f;config.powerDrive=a(23)>.5f?1.7f:1.3f;config.scalar[4]=a(23)>.5f?.85f:1.f;break; }
        case 4: {
            const int b=c==0?2:c==1?6:13;
            config.scalar[0]=gain(a(b));config.scalar[2]=volume(a(c==2?19:b+3));config.scalar[3]=volume(a(29));config.stages=c+2;config.stageGain={2.2f,2.2f,2.f,1.4f,1,1};
            if(c==2){eq(true,a(15),a(16),a(14),100,700,3200);config.scalar[1]=gain(a(17));bright=a(21)*5;preLow=a(20)*5;presence(a(18),a(22)>.5f?1600:3400);if(a(34)>.5f){config.stageGain[2]=2.7f;add(false,2,850,3.5f,.65f);}else config.stageBias[2]=.24f;}
            else {eq(true,a(0),a(1),a(b+1),100,680,3400);presence(a(b+2),c==1&&a(12)>.5f?1600:3400);if(c==0)bright=a(10)*6;else preLow=a(11)*5;}
            if(a(28)==1||(a(28)==2&&c==1)){constexpr float f[]{80,240,750,2200,6600};for(int i=0;i<5;++i)add(false,2,f[i],tone(a(23+i),12),1.1f);}
            config.mute=a(30)>.5f;config.sag=(a(31)>.5f?.27f:.10f)+(a(33)>.5f?.12f:0);config.powerDrive=a(32)>.5f?1.65f:1.18f;
            config.powerBias=a(33)>.5f?.11f:.018f;config.scalar[4]=a(31)>.5f?.82f:1;config.reverb=a(35)*.55f;hp=42;lp=a(32)>.5f?12700:9200;break; }
        case 5: {
            hp=10;lp=9000;config.sag=.2f;config.stageGain={2.7f,1.5f,1,1,1,1};config.scalar[0]=volume(a(c==0?0:7));
            if(c==0){constexpr float f[]{220,800,3000};eq(true,a(3),a(2),a(1),45,f[int(a(5))],4000,13);bright=a(4)*5;const int mode=int(a(6));if(mode==0)hp=80;else if(mode==2){preLow=6;add(true,2,450,-5,.7f);}}
            else {add(true,3,45,tone(a(9),13));add(true,4,4000,tone(a(8),13));bright=a(10)*5;preLow=a(11)*6;if(a(11)>.5f)add(true,2,450,-5,.7f);}
            if(r==1)bright+=4;config.powerDrive=1.35f;break; }
        case 6: { // GK: 4-band input EQ, boost before two separate output bands.
            config.topology=2;config.stages=2;config.scalar[0]=volume(a(0));config.scalar[1]=1+a(5)*5;config.scalar[8]=a(6)>.5f?db(-10):1;
            add(true,3,60,tone(a(4),12));add(true,2,250,tone(a(3),12),.8f);add(true,2,1000,tone(a(2),12),.8f);add(true,4,5000,tone(a(1),12));
            hp=a(7)>.5f?90:15;if(a(8)>.5f)add(true,2,600,-7,.65f);bright=a(9)*5;config.lowMaster=volume(a(10));config.highMaster=volume(a(11));config.crossover=frequency(a(12),100,2000);config.biamp=a(13)>.5f;config.sag=.015f;config.powerDrive=1.1f;lp=17000;break; }
        case 7: { // B7K drive/blend/EQ -> DB751 gain/EQ/power, in that order.
            config.topology=3;config.hybridMaster=volume(a(0));config.hybridBlend=a(1);config.hybridLevel=volume(a(2));config.hybridDrive=gain(a(3))*5;config.hybridDistortion=a(12)>.5f;
            constexpr float lo[]{250,500,1000},hi[]{750,1500,3000};
            add(true,3,70,tone(a(4),12));add(true,2,lo[int(a(10))],tone(a(5),12),.75f);add(true,2,hi[int(a(11))],tone(a(6),12),.75f);add(true,4,5000,tone(a(7),12));
            // Attack/Grunt affect only the clipped branch, never the clean blend.
            config.scalar[6]=db((a(8)-1)*6);config.scalar[7]=db((a(9)-1)*6);
            config.scalar[0]=gain(a(13));config.scalar[3]=volume(a(17));eq(false,a(14),a(15),a(16),50,650,4000,12);add(false,3,55,a(18)*5);add(false,4,3800,a(19)*5);
            config.stageGain={2.3f,1.35f,1,1,1,1};hp=12;lp=14000;config.sag=.09f;config.eqAfter=0;break; }
        case 8: {
            config.scalar[0]=volume(a(c==0?0:1));config.scalar[3]=volume(a(5));config.stages=c==0?2:3;config.stageGain={2.4f,1.8f,1.2f,1,1,1};
            if(c==1){add(true,3,100,tone(a(3),12));add(true,4,3500,tone(a(2),12));add(true,2,700,-3,.6f);}
            add(false,1,frequency(1-a(4),1800,16000));config.reverb=a(7)*.65f;config.reverbTone=a(6);config.tremRate=1+a(8)*9;config.tremDepth=a(9)*.85f;
            config.scalar[8]=r==1?.5f:1;config.sag=.26f;config.powerDrive=1.6f;hp=45;lp=15500;break; }
        case 9: {
            config.stages=c==0?2:4;config.stageGain={2.5f,2.4f,1.7f,1.3f,1,1};config.scalar[0]=c==0?volume(a(0)):gain(a(3));
            if(c==0){add(true,3,100,tone(a(2),12));add(true,4,3400,tone(a(1),12));}else {eq(false,a(6),a(5),a(4),100,450,3000);config.scalar[2]=volume(a(7));}
            config.reverb=a(8)*.65f;config.scalar[4]=volume(a(9));config.sag=a(10)>.5f?.34f:.2f;config.powerDrive=a(10)>.5f?1.9f:1.5f;hp=32;lp=9600;break; }
        case 10: {
            config.topology=c==1?4:0;config.scalar[3]=volume(a(15));config.mute=a(16)>.5f;config.scalar[8]=r==1?.501187f:1;config.stageGain={2.4f,1.65f,1.2f,1,1,1};
            if(c==0){config.scalar[0]=volume(a(0));eq(true,a(1),a(2),a(3),55,380,3600);preLow=a(11)*6;bright=a(12)*6;}
            else {config.scalar[0]=gain(a(4));config.scalar[5]=a(5);config.scalar[2]=volume(a(6));eq(false,a(7),a(9),a(10),55,frequency(a(8),150,2500),3500);preLow=a(13)*6;bright=a(14)*6;config.stages=3;}
            hp=10;lp=9000;config.sag=.17f;break; }
        case 11: {
            config.scalar[0]=gain(a(0));hp=frequency(a(1),25,150);const float voice=a(2);add(true,3,65,voice*5);add(true,2,550,-voice*8,.65f);add(true,4,3500,voice*4);
            add(false,3,40,tone(a(3),12));add(false,2,frequency(a(4),150,1800),tone(a(5),15),.85f);add(false,2,frequency(a(6),300,5000),tone(a(7),15),.85f);add(false,4,5000,tone(a(8),12));
            config.scalar[3]=volume(a(9));preLow=a(10)*5;bright=a(11)*5;config.mute=a(12)>.5f;config.scalar[8]=a(13)>.5f?db(-10):1;config.stageGain={1.15f,1.02f,1,1,1,1};config.sag=.008f;lp=18000;break; }
        case 12: {
            config.scalar[0]=volume(a(c==0?0:3));config.scalar[3]=a(7)>.5f?1.6f:volume(a(6));
            config.stageGain=c==0?std::array<float,6>{{2.3f,1.75f,1,1,1,1}}:std::array<float,6>{{3.7f,1.3f,1,1,1,1}};
            if(c==0){add(true,3,110,tone(a(1),12));add(true,4,3300,tone(a(2),12));}else {constexpr float lowcuts[]{280,190,125,80,45,22};hp=lowcuts[int(a(4))];add(true,4,2400,2.f-a(4)*.7f);}
            add(false,1,frequency(1-a(5),1700,16000));config.scalar[8]=r==1?.5f:1;config.powerDrive=a(8)>.5f?1.9f:1.45f;config.sag=a(8)>.5f?.28f:.16f;config.powerBias=c==1?.08f:.035f;lp=14500;break; }
        case 13: {
            config.scalar[0]=volume(a(0));config.scalar[3]=volume(a(4));config.stages=c==0?2:4;config.stageGain={2.2f,1.45f,2.2f,1.3f,1,1};config.eqAfter=1;
            if(a(10)<.5f)eq(true,a(3),a(2),a(1),90,a(7)>.5f?850:500,a(7)>.5f?3800:2800,11);else config.scalar[0]*=1.8f;
            presence(a(5),2300);bright=a(6)*5;if(a(7)>.5f){preLow=-3;config.scalar[0]*=.76f;}
            if(c==1){config.scalar[1]=gain(a(8));config.scalar[2]=volume(a(9));}hp=32;lp=10300;config.sag=.12f;break; }
        case 14: {
            config.scalar[0]=gain(a(0));const float taste=(a(1)-.5f)*2;
            add(true,3,90,-taste*4.5f);add(true,2,700,taste*5,.65f);add(true,4,3500,taste*3.5f);
            add(false,3,30,tone(a(2),15));add(false,2,250,tone(a(3),12),.8f);add(false,2,800,tone(a(4),12),.8f);add(false,4,8000,tone(a(5),15));
            config.scalar[3]=volume(a(6));config.mute=a(7)>.5f;config.stageGain={1.1f,1.f,1,1,1,1};config.sag=.004f;hp=8;lp=21000;break; }
        case 15: {
            const int b=c*9;config.scalar[0]=gain(a(b));config.scalar[2]=volume(a(b+1));config.stages=c+2;config.stageGain={2.2f,1.75f,1.65f,1.45f,1.2f,1};
            eq(c==0,a(b+2),a(b+3),a(b+4),95,650,3600);presence(a(36),a(41)>.5f?1700:3400);depth(a(37),85);config.scalar[3]=volume(a(38));if(config.solo)config.scalar[3]*=1+a(40)*1.5f;
            if(a(b+7)>.5f)config.gateThreshold=db(-75+a(39)*55);
            if(c==0){hp=a(b+5)>.5f?110:32;config.scalar[0]*=a(b+6)>.5f?2:1;bright=a(b+8)*6;}
            else if(c<3){config.sag=a(b+5)>.5f?.26f:.09f;config.stageGain[1]=a(b+5)>.5f?1.35f:2.1f;config.scalar[0]*=a(b+6)>.5f?1.8f:1;bright=a(b+8)*5;hp=48;}
            else {preLow=a(b+5)*5;hp=a(b+6)>.5f?130:55;if(a(b+8)>.5f)add(false,2,780,5,.8f);}
            lp=14500;break; }
        case 16: {
            config.scalar[0]=gain(a(c==0?0:1))*(a(9)>.5f?2.f:1);config.scalar[2]=c==1?volume(a(2)):1;config.scalar[3]=volume(a(7))*(a(10)>.5f?1.41254f:1);
            eq(c==0,a(3),a(4),a(5),100,650,3500);if(c==1)presence(a(6),2750);config.stages=c==0?2:4;config.stageGain={2.1f,2.2f,1.7f,1.3f,1,1};config.reverb=a(8)*.55f;
            const int soak=int(a(11));constexpr float attenuation[]{1,.5f,.223607f,0};config.scalar[4]=attenuation[soak];config.sag=.31f;config.powerDrive=1.5f;hp=48;lp=12100;break; }
        case 17: {
            const int b=c*5;config.scalar[0]=gain(a(b));config.scalar[2]=volume(a(b+4));config.scalar[3]=volume(a(20));config.stages=c+2;config.stageGain={2.1f,1.9f,1.65f,1.4f,1.2f,1};
            eq(c==0,a(b+3),a(b+2),a(b+1),85,480,3200);presence(a(21),2300);depth(a(22),80);config.sag=.075f;hp=38+c*13.f;lp=14400-c*850.f;config.powerDrive=1.35f;break; }
        case 18: {
            constexpr float mids[]{220,450,800,1600,3000};config.scalar[0]=gain(a(0));eq(true,a(1),a(2),a(3),45,mids[int(a(5))],4000,14);config.scalar[3]=volume(a(4));
            bright=a(6)*5;preLow=a(7)*6;if(a(7)>.5f)add(true,2,500,-5,.7f);hp=10;lp=9000;config.stageGain={2.6f,1.7f,1,1,1,1};config.sag=.23f;config.powerDrive=1.45f;break; }
        case 19: {
            config.topology=1;config.scalar[6]=r==1?0:volume(a(0));config.scalar[7]=r==0?0:volume(a(1));config.scalar[3]=volume(a(5));config.stages=3;config.stageGain={3.8f,1.9f,1.3f,1,1,1};
            eq(false,a(2),a(3),a(4),80,500,2900);hp=18;lp=11500;config.sag=.29f;config.powerDrive=1.85f;break; }
        case 20: {
            config.scalar[3]=volume(a(a(17)>.5f?16:15));presence(a(14),3300);depth(a(13),85);config.sag=.09f;config.powerDrive=1.45f;
            if(c==2){config.scalar[0]=volume(a(9));eq(true,a(10),a(11),a(12),85,700,3500);config.stages=2;config.stageGain={1.7f,1.2f,1,1,1,1};hp=25;}
            else {config.stages=5;config.stageGain={2.9f,2.3f,1.8f,1.45f,1.15f,1};eq(false,a(5),a(6),a(8),95,frequency(a(7),250,1400),3700);hp=58;
                if(c==0){config.topology=5;config.scalar[0]=gain(a(2));config.scalar[6]=gain(a(0));config.scalar[7]=gain(a(1));}
                else {config.scalar[0]=gain(a(3));config.scalar[1]=gain(a(4));config.stageBias[1]=-.36f;}}
            lp=14300;break; }
        case 21: {
            config.scalar[0]=gain(a(c==0?0:2));config.scalar[2]=volume(a(c==0?1:3));config.stages=c==1?4:(a(5)>.5f?3:2);config.stageGain={2.8f,2.4f,1.7f,1.25f,1,1};
            eq(false,a(6),a(7),a(8),90,650,3300);presence(a(9),2700);depth(a(10),90);if(c==0)bright=a(4)*6;else config.stageBias[1]=-.45f;
            hp=40;lp=12800;config.sag=.18f;break; }
        case 22: {
            const int b=c==0?0:5;config.scalar[0]=gain(a(b));config.scalar[2]=volume(a(c==0?4:10));config.scalar[3]=volume(a(11));
            eq(c==0,a(b+1),a(b+2),a(b+3),70,450,3000);if(c==1){presence(a(9),1700);add(false,2,70,3,.7f);}
            config.stages=c==0?2:4;config.stageGain={2.3f,2.5f,1.8f,1.4f,1,1};config.sag=.27f;config.powerDrive=1.5f;hp=c==0?24:34;lp=c==0?14400:10300;break; }
        case 23: { // E670FE functional prototype, NOT a measured circuit fit.
            const auto v=[&s](std::string_view key){return ampNativeValue(s,key);};
            const bool driver=c==4, classic=!driver && v("hw.character")>.5f;
            config.sag=classic?.18f:.07f;config.powerDrive=1.4f;
            config.scalar[3]=volume(v(v("hw.master_select")>.5f?"hw.master_b":"hw.master_a"));
            presence(v(v("hw.presence_select")>.5f?"hw.presence_b":"hw.presence_a"),2800);
            if(v("hw.depth_boost")>.5f)add(false,2,85,4,.8f);
            if(driver) {
                // Preamp defeat has no main-channel gain/volume or voicing.
                config.stages=1;config.eqAfter=0;config.stageGain={1.15f,1,1,1,1,1};
                hp=18;lp=18000;config.sag=.07f;
                // Fixed passive-style insertion response is explicitly an
                // authored placeholder until T.D. EQ ownership is verified.
                if(v("hw.tube_eq")>.5f){add(true,2,600,-3,.65f);add(true,4,4200,-1.5f);}
            } else {
                constexpr const char* gains[]{"hw.clean.gain","hw.crunch.gain","hw.lead1.gain","hw.lead2.gain"};
                constexpr const char* trebles[]{"hw.clean.treble","hw.crunch.treble","hw.lead1.treble","hw.lead2.treble"};
                constexpr const char* volumes[]{"hw.clean.volume","hw.crunch.volume","hw.lead1.volume","hw.lead2.volume"};
                const float g=v(gains[c]);config.scalar[0]=gain(g);config.scalar[2]=volume(v(volumes[c]));
                config.stages=c==0?2:c==1?3:c==2?4:5;
                config.stageGain={1.8f,1.65f,1.8f,1.4f,1.15f,1};
                hp=c==0?24.f:classic?38.f:65.f;lp=classic?11500.f:15000.f;
                config.stageBias[1]=classic?-.15f:-.27f;
                if(c<2) {
                    config.scalar[0]*=v("hw.gain_boost")>.5f?1.9f:1.f;
                    eq(true,v("hw.clean_eq.bass"),v("hw.clean_eq.middle"),v(trebles[c]),95,650,3500);
                    bright=v("hw.bright")*(1-g)*7;
                    if(v("hw.mid_shift")>.5f){add(true,2,400,-3,.65f);add(true,2,1350,2,.8f);}
                } else {
                    config.scalar[1]=v("hw.hi_gain")>.5f?2.1f:1.f;
                    eq(false,v("hw.lead_eq.bass"),v("hw.lead_eq.middle"),v(trebles[c]),90,650,3500);
                    if(v("hw.contour")>.5f)add(false,2,300,4,.7f);
                    if(v("hw.mid_edge")>.5f)add(false,2,1200,3.5f,.7f);
                }
                if(v("hw.mega_lo_punch")>.5f){add(true,3,95,3);add(false,2,85,1.5f,.8f);}
            }
            break; }
        default:break;
        }
        if(bright!=0)add(true,4,1700,bright);
        if(preLow!=0)add(true,3,85,preLow);
        config.input=D::make(rate,0,hp);config.output=D::make(rate,1,lp);
        config.lowPower=D::make(rate,3,85,0);config.highPower=D::make(rate,4,3000,0);
        for(int i=0;i<6;++i){config.stageHP[size_t(i)]=pole(rate,hp*(i==0?.65f:(1+.25f*i)));config.stageLP[size_t(i)]=pole(rate,lp*(1.45f-.06f*i));if(config.stageBias[size_t(i)]==0)config.stageBias[size_t(i)]=(i%2?-.07f:.06f)+float(s.model%4)*.013f;}
        parallelPole=pole(rate,s.model==20?480.f:800.f);crossoverPole=pole(rate,config.crossover);
        verbDamping=pole(channels[0].spring.internalRate,1800+config.reverbTone*6500);verbOutput=pole(rate,1500+config.reverbTone*5500);phaseStep=float(6.283185307179586*config.tremRate/rate);
    }
public:
    void prepare(double sampleRate) {
        rate=std::isfinite(sampleRate)?std::max(8000.,sampleRate):48000.;
        smooth=float(-std::expm1(-1./(rate*.012)));gridAttack=float(-std::expm1(-1./(rate*.0008)));gridRelease=float(-std::expm1(-1./(rate*.045)));cathodeRate=float(-std::expm1(-1./(rate*.012)));
        supplyAttack=float(-std::expm1(-1./(rate*.006)));supplyRelease=float(-std::expm1(-1./(rate*.15)));gateAttack=float(-std::expm1(-1./(rate*.002)));gateRelease=float(-std::expm1(-1./(rate*.075)));dcPole=ampNativeDetail::pole(rate,5);
        for(auto& c:channels)c.spring.prepare(rate);
        if(!valid)current=defaultAmpNativeState(0);
        configure(current);valid=true;reset();
    }
    void reset() noexcept {for(auto& c:channels)c.clear(config);}
    void set(const AmpNativeState& state) noexcept {
        AmpNativeState s=state;sanitiseAmpNativeState(s);if(valid&&s==current)return;
        const bool changedModel=!valid||s.model!=current.model;current=s;configure(s);valid=true;
        // Model replacement clears the previous circuit's reactive storage.
        // Ordinary knob/channel edits preserve storage and slew gains.
        if(changedModel)reset();else for(auto& c:channels){c.smoothRemaining=int(rate*.12);c.scheduleCoefficients(config,rate);}
    }
    float tick(float sample,int stereoChannel) noexcept {
        using namespace ampNativeDetail;
        if(!std::isfinite(sample))sample=0;
        auto& c=channels[size_t(std::clamp(stereoChannel,0,1))];const auto& p=config;
        c.advanceCoefficients(p);
        if(c.smoothRemaining>0){
        for(size_t i=0;i<c.scalar.size();++i)c.scalar[i]+=smooth*(p.scalar[i]-c.scalar[i]);
        const auto slew=[this](float& v,float target){v+=smooth*(target-v);};
        slew(c.hybridDrive,p.hybridDrive);slew(c.hybridLevel,p.hybridLevel);slew(c.hybridBlend,p.hybridBlend);slew(c.hybridMaster,p.hybridMaster);slew(c.highMaster,p.highMaster);slew(c.lowMaster,p.lowMaster);slew(c.reverb,p.reverb);slew(c.tremDepth,p.tremDepth);
        if(--c.smoothRemaining==0){c.scalar=p.scalar;c.hybridDrive=p.hybridDrive;c.hybridLevel=p.hybridLevel;c.hybridBlend=p.hybridBlend;c.hybridMaster=p.hybridMaster;c.highMaster=p.highMaster;c.lowMaster=p.lowMaster;c.reverb=p.reverb;c.tremDepth=p.tremDepth;}
        }
        const float trimmed=std::clamp(sample*c.scalar[9],-8.f,8.f);
        const float absolute=std::abs(trimmed);c.detector+=(absolute>c.detector?gateAttack:gateRelease)*(absolute-c.detector);
        const float gateTarget=p.gateThreshold<=0?1.f:std::clamp((c.detector/p.gateThreshold-.35f)/.65f,0.f,1.f);
        c.gate+=(gateTarget>c.gate?gateAttack:gateRelease)*(gateTarget-c.gate);
        float x=c.input.tick(trimmed*c.scalar[8],c.inputCoeff)*c.gate;
        const float dry=x;
        if(p.topology==1){const float low=c.parallelLow.low(x,parallelPole);x=low*c.scalar[6]+(x+1.5f*(x-low))*c.scalar[7];}
        if(p.topology==5){const float low=c.parallelLow.low(x,parallelPole);x=low*c.scalar[6]+(x-low)*c.scalar[7];}
        if(p.topology==3){
            if(p.hybridDistortion){const float lo=c.hybridLow.low(x,parallelPole);const float voiced=lo*c.scalar[7]+(x-lo)*c.scalar[6];const float driven=soft(voiced*c.hybridDrive+.13f)-soft(.13f);x=(1-c.hybridBlend)*x+c.hybridBlend*driven*c.hybridLevel;}
            for(int i=0;i<c.preCount;++i)x=c.pre[size_t(i)].tick(x,c.preCoeff[size_t(i)]);
            x*=c.hybridMaster;
        }
        for(int stage=0;stage<p.stages;++stage){
            if(stage==p.eqAfter&&p.topology!=3)for(int i=0;i<c.preCount;++i)x=c.pre[size_t(i)].tick(x,c.preCoeff[size_t(i)]);
            const float drive=p.stageGain[size_t(stage)]*(stage==0?c.scalar[0]:(stage==2||(p.model==6&&stage==1)?c.scalar[1]:1));
            x=c.stage[size_t(stage)].tick(x,drive,p.stageHP[size_t(stage)],p.stageLP[size_t(stage)],p.stageBias[size_t(stage)],gridAttack,gridRelease,cathodeRate,p.blocking);
        }
        if(p.topology==4)x=dry*(1-c.scalar[5])+x*c.scalar[5];
        for(int i=0;i<c.postCount;++i)x=c.post[size_t(i)].tick(x,c.postCoeff[size_t(i)]);
        x*=c.scalar[2]*c.scalar[3];
        if(p.topology==2){if(p.biamp){const float low=c.crossover.low(x,crossoverPole);x=low*c.lowMaster+(x-low)*c.highMaster;}else x*=c.lowMaster;}
        const float demand=std::abs(x);c.supply+=(demand>c.supply?supplyAttack:supplyRelease)*(demand-c.supply);
        const float supply=1.f/(1.f+p.sag*c.supply);
        x=supply*(soft(x*p.powerDrive/(std::max(.3f,supply))+p.powerBias)-soft(p.powerBias));
        x=c.output.tick(x,c.outputCoeff);x=c.dc.high(x,dcPole);
        if(p.reverbPresent)x+=c.reverb*c.spring.tick(x,verbDamping,verbOutput,p.reverbDecay);
        c.phase+=phaseStep;if(c.phase>=6.283185307179586f)c.phase-=6.283185307179586f;
        if(c.tremDepth>1.e-6f)x*=1.f-c.tremDepth*(.5f+.5f*std::sin(c.phase));
        x*=c.scalar[4];
        if(p.mute)return 0;
        return std::isfinite(x)?std::clamp(x,-4.f,4.f)*c.scalar[10]:0.f;
    }
};
} // namespace spectralforge

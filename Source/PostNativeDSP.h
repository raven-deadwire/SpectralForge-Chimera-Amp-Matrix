#pragma once
#include "PostNativeCatalog.h"
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
// Authored digital models with the reviewed control topology. No claim of
// measured circuit, component, potentiometer taper, or electrical I/O fidelity.
namespace post_native_detail {
inline float dbGain(float x) noexcept {return std::pow(10.f,juce::jlimit(-120.f,72.f,x)*.05f);}
inline float db(float x) noexcept {return 20.f*std::log10(juce::jmax(1.e-9f,std::abs(x)));}
inline float saturate(float x,float headroom) noexcept {const float t=x/headroom;return x/std::sqrt(1.f+t*t);}
// Memoise pure transforms only. DSP state, smoothing and meters still advance
// every sample, including bypass, and a changed argument recomputes immediately.
struct GainCache {
    float argument{},value{};bool valid{};
    float get(float x) noexcept {if(!valid||x!=argument){argument=x;value=dbGain(x);valid=true;}return value;}
};
struct ExpCache {
    double argument{};float value{};bool valid{};
    float get(double x) noexcept {if(!valid||x!=argument){argument=x;value=static_cast<float>(std::exp(x));valid=true;}return value;}
};
// Fixed-state TDF2 filters; interpolated coefficients, no RefCounted allocation.
struct Biquad {
    std::array<double,5> a{{1,0,0,0,0}},target{{1,0,0,0,0}},delta{};
    std::array<double,2> z1{},z2{};int left{};bool configured{};
    void reset() noexcept {z1={};z2={};}
    void set(const std::array<float,6>& c) noexcept {
        std::array<double,5> next{{c[0]/c[3],c[1]/c[3],c[2]/c[3],c[4]/c[3],c[5]/c[3]}};
        if(!configured){a=target=next;configured=true;left=0;return;}
        if(next==target)return;target=next;left=32;for(size_t i=0;i<5;++i)delta[i]=(target[i]-a[i])/32.;
    }
    void advance() noexcept {if(left>0){for(size_t i=0;i<5;++i)a[i]+=delta[i];if(--left==0)a=target;}}
    float sample(int channel,float x) noexcept {
        const auto c=static_cast<size_t>(channel);double y=a[0]*x+z1[c];z1[c]=a[1]*x-a[3]*y+z2[c];z2[c]=a[2]*x-a[4]*y;
        if(!std::isfinite(y)||std::abs(y)>1.e6){z1[c]=z2[c]=0;return 0;}if(std::abs(z1[c])<1.e-25)z1[c]=0;if(std::abs(z2[c])<1.e-25)z2[c]=0;return static_cast<float>(y);
    }
};
struct Model {
    int section{},index{},clock{};double rate{48000};
    std::array<Biquad,6> filters{};
    std::array<float,postNativeMaxControls> v{},targets{};
    std::array<float,2> detectorLow{},dc{};
    float controlPole{},power{},gr{},slowMemory{},fade{1},meterDb{-120},meterOutput{-120},meterInput{-120};
    float trim{1},level{1},targetTrim{1},targetLevel{1},wet{},targetWet{},reduction{};bool ready{};float gainMemory{1},offMemory{};
    GainCache makeupGain,inputGain,preampGain;
    std::array<ExpCache,2> envelopePoles;
    float detectorPole{},detectorLowPole{},memoryPole{},dcPole{};
    std::array<float,postNativeMaxControls> filterValues{},filterTargets{};bool filterInputsReady{};
    // 1.7 expansion dynamics: per-channel detectors and a second gain stage.
    std::array<float,2> channelPower{},channelGr{},lastOutput{},sidechainLow{};float limitGr{},limitMemory{};
    int integer(int n) const noexcept {return juce::roundToInt(targets[static_cast<size_t>(n)]);}
    void prepare(double sampleRate,int sectionNumber,int modelNumber) {
        section=sectionNumber;index=modelNumber;rate=sampleRate;controlPole=static_cast<float>(1.-std::exp(-1./(rate*.012)));ready=false;filters={};
        const float detectorTime=index==0||index>2?.003f:index==1?.00003f:.002f;
        detectorPole=static_cast<float>(std::exp(-1./(rate*detectorTime)));
        detectorLowPole=static_cast<float>(1.-std::exp(-juce::MathConstants<double>::twoPi*1200./rate));
        memoryPole=static_cast<float>(std::exp(-1./(rate*(index==2?1.2:.5))));
        dcPole=static_cast<float>(1.-std::exp(-juce::MathConstants<double>::twoPi*4./rate));reset();
    }
    void reset() noexcept {filterInputsReady=false;filters={};power=gr=slowMemory=0;fade=1;detectorLow={};dc={};clock=0;channelPower={};channelGr={};lastOutput={};sidechainLow={};limitGr=limitMemory=0;gainMemory=1;offMemory=0;wet=targetWet=0;reduction=0;meterDb=meterOutput=meterInput=-120;ready=false;}
    void configure(const PostNativeBank& bank) noexcept {
        targets=bank.values;targetTrim=dbGain(bank.trimDb);targetLevel=dbGain(bank.levelDb);targetWet=bank.bypass?0.f:1.f;
        if(!ready){v=targets;trim=targetTrim;level=targetLevel;ready=true;updateFilters();}
    }
    float hz(float value) const noexcept {return juce::jlimit(10.f,static_cast<float>(rate*.44),value);}
    void updateFilters() noexcept {
        // Check on the original 32-sample control clock: no change to coefficient
        // interpolation or the instant at which automation reaches the filters.
        if(filterInputsReady && v==filterValues && targets==filterTargets)return;
        filterValues=v;filterTargets=targets;filterInputsReady=true;
        using C=juce::dsp::IIR::ArrayCoefficients<float>;
        const auto low=[&](int f,float frequency,float gain,float q=.707f){filters[f].set(C::makeLowShelf(rate,hz(frequency),q,dbGain(gain)));};
        const auto high=[&](int f,float frequency,float gain,float q=.707f){filters[f].set(C::makeHighShelf(rate,hz(frequency),q,dbGain(gain)));};
        const auto peak=[&](int f,float frequency,float gain,float q){filters[f].set(C::makePeakFilter(rate,hz(frequency),juce::jlimit(.25f,8.f,q),dbGain(gain)));};
        if(section==1) {
            if(index==0) {filters[0].set(C::makeHighPass(rate,hz(18),.65f));low(1,95,.8f);high(2,11000,-.7f);}
            else if(index==1) {
                // Ten independent authored contours; original V5 transfer data
                // unavailable. Tone OFF is a true filter bypass.
                static constexpr float lf[]{2,3,4,1,-2,-3,2,0,-4,4};
                static constexpr float mf[]{-2,-4,-1,2,3,-2,-4,3,1,-3};
                static constexpr float hf[]{1,2,0,-2,2,4,-2,0,3,4};
                const int tone=juce::jlimit(0,9,integer(2));const float on=targets[3]>.5f?1.f:0.f;
                low(0,100,lf[tone]*on);peak(1,250.f+tone*180.f,mf[tone]*on,.65f);high(2,4200,hf[tone]*on);
                filters[3].set(C::makeLowPass(rate,hz(targets[4]>.5f?7000.f:22000.f),.707f));
                filters[4].set(C::makeHighPass(rate,hz(integer(0)>=2?65.f:12.f),.707f));
            } else if(index==2) {filters[0].set(C::makeHighPass(rate,hz(targets[7]>.5f?75.f:10.f),.707f));high(1,12000,-.35f);}
            // 1.7 expansion voicings: authored contours, not measured responses.
            else if(index==3) {filters[0].set(C::makeHighPass(rate,hz(12),.707f));low(1,80,.6f);high(2,16000,-.5f);}
            else if(index==4) {filters[0].set(C::makeHighPass(rate,hz(18),.707f));low(1,100,1.2f);high(2,12000,-1.5f);}
            else {filters[0].set(C::makeHighPass(rate,hz(targets[3]>.5f?75.f:8.f),.707f));}
        } else if(section==2) {
            if(index==0) {
                // LF/HF switches really change shelf <-> bell; Brown and Black
                // use different gain-dependent bandwidth, not a label-only mode.
                const bool black=targets[12]>.5f;const float qh=black?.85f:.6f;const float gainScale=black?1.f:15.f/18.f;
                if(targets[2]>.5f)peak(0,v[1],v[0]*gainScale,qh);else high(0,v[1],v[0]*gainScale,.707f);
                peak(1,v[4],v[3]*gainScale,v[5]*(black?1.f+.028f*std::abs(v[3]):.8f));
                peak(2,v[7],v[6]*gainScale,v[8]*(black?1.f+.028f*std::abs(v[6]):.8f));
                if(targets[11]>.5f)peak(3,v[10],v[9]*gainScale,qh);else low(3,v[10],v[9]*gainScale,.707f);
            } else if(index==1) {
                static constexpr float mids[]{360,700,1600,3200,4800,7200},lows[]{35,60,110,220},hp[]{10,50,80,160,300};
                high(0,12000,v[0]);peak(1,mids[juce::jlimit(0,5,integer(2))],v[1],1.0f+.025f*std::abs(v[1]));low(2,lows[juce::jlimit(0,3,integer(4))],v[3]);filters[3].set(C::makeHighPass(rate,hz(hp[juce::jlimit(0,4,integer(5))]),.707f));
            } else if(index==3) {
                // Stepped gains; bandwidth narrows as the gain grows (proportional Q).
                static constexpr float steps[]{-12,-9,-6,-4,-2,0,2,4,6,9,12},lows[]{30,40,50,100,200,300,400},mids[]{200,400,600,800,1500,3000,5000},highs[]{2500,5000,7000,10000,12500,15000,20000};
                const auto gain=[&](int n){return steps[juce::jlimit(0,10,integer(n))];};
                const auto q=[](float g){return .4f+.09f*std::abs(g);};
                const float lg=gain(1),mg=gain(4),hg=gain(6);
                if(targets[2]>.5f)low(0,lows[juce::jlimit(0,6,integer(0))],lg,.707f);else peak(0,lows[juce::jlimit(0,6,integer(0))],lg,q(lg));
                peak(1,mids[juce::jlimit(0,6,integer(3))],mg,q(mg));
                if(targets[7]>.5f)high(2,highs[juce::jlimit(0,6,integer(5))],hg,.707f);else peak(2,highs[juce::jlimit(0,6,integer(5))],hg,q(hg));
                filters[3].set(C::makeHighPass(rate,hz(50),.707f));filters[4].set(C::makeLowPass(rate,hz(15000),.707f));
            } else if(index==4) {
                // Broad bells (Q 0.4, about three octaves), a 2.5 kHz shelf and the AIR shelf.
                static constexpr float air[]{0,2500,5000,10000,15000,20000,40000};
                peak(0,10,v[0],.4f);peak(1,40,v[1],.4f);peak(2,160,v[2],.4f);peak(3,650,v[3],.4f);high(4,2500,v[4],.5f);
                const int a=juce::jlimit(0,6,integer(6));high(5,a==0?20000.f:air[a],a==0?0.f:v[5],.5f);
            } else if(index==5) {
                static constexpr float trebles[]{5000,7000,10000,12000,15000,20000},p1[]{800,1200,1800,2800,3600,4200,6500,8100},p2[]{300,400,500,800,1200,1800,2800,3600},bass[]{35,50,70,91,150,200,300};
                static constexpr float hp[]{10,20,30,40,50,60,80,100,160,200,320},lp[]{22000,20000,15000,12000,10000,8000,6000,4000};
                const float tf=trebles[juce::jlimit(0,5,integer(1))],bf=bass[juce::jlimit(0,6,integer(8))];
                if(targets[2]>.5f)peak(0,tf,v[0],.9f);else high(0,tf,v[0],.707f);
                peak(1,p1[juce::jlimit(0,7,integer(4))],v[3],1.f);peak(2,p2[juce::jlimit(0,7,integer(6))],v[5],1.f);
                if(targets[9]>.5f)peak(3,bf,v[7],.9f);else low(3,bf,v[7],.707f);
                filters[4].set(C::makeHighPass(rate,hz(hp[juce::jlimit(0,10,integer(10))]),.707f));
                filters[5].set(C::makeLowPass(rate,hz(lp[juce::jlimit(0,7,integer(11))]),.707f));
            } else {
                static constexpr float lows[]{20,30,60,100},highs[]{3000,4000,5000,8000,10000,12000,16000},atten[]{5000,10000,20000};
                const float lf=lows[juce::jlimit(0,3,integer(0))];
                low(0,lf,v[1],.7f);low(1,lf*1.85f,-v[2],.65f);
                peak(2,highs[juce::jlimit(0,6,integer(5))],v[4],2.8f-2.35f*v[3]);
                high(3,atten[juce::jlimit(0,2,integer(7))],-v[6],.65f);
            }
        }
    }
    void frame() noexcept {
        for(size_t c=0;c<v.size();++c)v[c]+=controlPole*(targets[c]-v[c]);trim+=controlPole*(targetTrim-trim);level+=controlPole*(targetLevel-level);wet+=controlPole*(targetWet-wet);
        if((clock++&31)==0)updateFilters();for(auto& f:filters)f.advance();
    }
    // Gain computers for the 1.7 dynamics models. Thresholds are panel dB
    // re -18 dBFS; laws and time constants are authored, not measured.
    float pole(float seconds) noexcept {return static_cast<float>(std::exp(-1./(rate*juce::jmax(1.e-5f,seconds))));}
    void expansionDynamics(std::array<float,2>& x,int channels) noexcept {
        if(index==3) {
            // Polar Press: one slope across the threshold, from gating (+3)
            // through neutral (0) and limiting (-1) to reversal (-2), held
            // inside the attenuation and gain limits.
            const float p=v[1],slope=p>=.5f?-4.f*(p-.5f):6.f*(.5f-p);
            const float threshold=v[2]-18.f+10.f*integer(0);
            float detector=0;
            for(int c=0;c<channels;++c){float d=x[c];if(targets[7]>.5f){sidechainLow[c]+=detectorLowPole*.12f*(d-sidechainLow[c]);d-=sidechainLow[c];}detector=juce::jmax(detector,d*d);}
            const float coefficient=detector>power?pole(v[3]*.001f):pole(v[4]*.001f);
            power=coefficient*power+(1-coefficient)*detector;
            const float level=10.f*std::log10(juce::jmax(power,1.e-12f));
            float gainDb=targets[10]>.5f?juce::jlimit(-v[5],v[6],slope*(level-threshold)):0.f;
            gr=-gainDb;reduction=juce::jmax(0.f,gr)*wet;
            const float gain=dbGain(gainDb+v[8]);
            for(int c=0;c<channels;++c)x[c]*=gain;
        } else if(index==4) {
            // Thrust Bus: per-channel RMS detectors from the input (NEW, feed-
            // forward) or the output (OLD, feedback); THRUST tilts the detector
            // away from the lows; LINK blends each detector towards the louder.
            static constexpr float attacks[]{.00003f,.0001f,.0003f,.001f,.003f,.01f,.03f},releases[]{.05f,.1f,.2f,.5f,1.f,2.f},ratios[]{1.5f,2,3,4,6,10,1000},knees[]{0,6,12};
            const float threshold=v[0]-18.f,ratio=ratios[juce::jlimit(0,6,integer(2))],knee=knees[juce::jlimit(0,2,integer(5))];
            const float attack=attacks[juce::jlimit(0,6,integer(1))],release=integer(3)>=6?v[4]:releases[juce::jlimit(0,5,integer(3))];
            const int thrust=juce::jlimit(0,2,integer(6));const bool feedback=integer(7)==1;const float link=v[8]*.01f;
            for(int c=0;c<channels;++c){float d=feedback?lastOutput[c]:x[c];sidechainLow[c]+=detectorLowPole*.1f*(d-sidechainLow[c]);d-=thrust*.5f*sidechainLow[c];channelPower[c]=detectorPole*channelPower[c]+(1-detectorPole)*d*d;}
            const float loudest=channels>1?juce::jmax(channelPower[0],channelPower[1]):channelPower[0];
            const bool on=targets[10]>.5f;float most=0;
            for(int c=0;c<channels;++c) {
                const float linked=link*loudest+(1-link)*channelPower[c];
                const float over=10.f*std::log10(juce::jmax(linked,1.e-18f))-threshold;
                const float soft=knee<=0?juce::jmax(0.f,over):over<-knee*.5f?0.f:over>knee*.5f?over:(over+knee*.5f)*(over+knee*.5f)/(2*knee);
                const float desired=on?juce::jlimit(0.f,72.f,soft*(1.f-1.f/ratio)):0.f;
                const float k=desired>channelGr[c]?pole(attack):pole(release);channelGr[c]=k*channelGr[c]+(1-k)*desired;
                most=juce::jmax(most,channelGr[c]);
                x[c]*=dbGain(-channelGr[c]+v[9]);lastOutput[c]=x[c];
            }
            gr=most;reduction=most*wet;
        } else {
            // Diode Bridge: compressor and limiter in series on one linked
            // detector. A1/A2 recover fast after short peaks and slowly after
            // sustained reduction. Distortion grows with the gain reduction.
            static constexpr float ratios[]{1.5f,2,3,4,6},compRecovery[]{.1f,.4f,.8f,1.5f},limitRecovery[]{.1f,.2f,.8f};
            float detector=0;for(int c=0;c<channels;++c)detector=juce::jmax(detector,x[c]*x[c]);
            power=detectorPole*power+(1-detectorPole)*detector;
            const float level=10.f*std::log10(juce::jmax(power,1.e-18f));
            const auto recovery=[&](int choice,const float* fixed,int count,float memory){return choice<count?fixed[choice]:(choice==count?.1f+1.9f*juce::jlimit(0.f,1.f,memory/10.f):.04f+4.96f*juce::jlimit(0.f,1.f,memory/10.f));};
            const float compDesired=targets[5]>.5f?juce::jmax(0.f,level-(v[0]-18.f))*(1.f-1.f/ratios[juce::jlimit(0,4,integer(1))]):0.f;
            const float ck=compDesired>gr?pole(integer(3)==0?.003f:.015f):pole(recovery(juce::jlimit(0,5,integer(2)),compRecovery,4,slowMemory));
            gr=ck*gr+(1-ck)*compDesired;slowMemory=memoryPole*slowMemory+(1-memoryPole)*gr;
            const float limitDesired=targets[9]>.5f?juce::jmax(0.f,level-gr-(v[6]-18.f))*.99f:0.f;
            const float lk=limitDesired>limitGr?pole(integer(8)==0?.002f:.004f):pole(recovery(juce::jlimit(0,4,integer(7)),limitRecovery,3,limitMemory));
            limitGr=lk*limitGr+(1-lk)*limitDesired;limitMemory=memoryPole*limitMemory+(1-memoryPole)*limitGr;
            const float total=gr+limitGr;reduction=total*wet;
            const float gain=dbGain(-total+v[4]),colour=.12f*juce::jlimit(0.f,1.f,total/20.f);
            for(int c=0;c<channels;++c){float y=x[c]*gain;y-=colour*y*y*y/(1.f+y*y);x[c]=y;}
        }
    }
    // `key`: the external sidechain frame, used by Console VCA's EXTERNAL SIDE-CHAIN switch.
    std::array<float,2> process(std::array<float,2> dry,int channels,const std::array<float,2>* key=nullptr) noexcept {
        frame();std::array<float,2> x=dry;for(int c=0;c<channels;++c)x[c]*=trim;
        const float inputPeak=juce::jmax(std::abs(x[0]),channels>1?std::abs(x[1]):0.f);meterInput=db(inputPeak);
        if(section==0 && index>2) {expansionDynamics(x,channels);}
        else if(section==0) {
            float threshold=-18,ratio=4,attack=.01f,release=.1f,makeup=1,input=1,knee=6;bool compOn=true;float preGain=1;
            if(index==0) {static constexpr float attacks[]{.0001f,.0003f,.001f,.003f,.01f,.03f},releases[]{.1f,.3f,.6f,1.2f,.1f},ratios[]{2,4,10};threshold=v[0];makeup=makeupGain.get(v[1]);attack=attacks[juce::jlimit(0,5,integer(2))];release=releases[juce::jlimit(0,4,integer(3))];ratio=ratios[juce::jlimit(0,2,integer(4))];compOn=targets[5]>.5f;if(integer(3)==4)release=.1f+.08f*slowMemory;const float step=1.f/static_cast<float>(rate*juce::jmax(1.f,v[7]));fade=juce::jlimit(0.f,1.f,fade+(targets[8]>.5f?-step:step));}
            else if(index==1) {static constexpr float ratios[]{4,8,12,20,12};input=inputGain.get(v[0]);makeup=makeupGain.get(v[1]);attack=.0008f*std::pow(.025f,v[2]);release=1.1f*std::pow(.05f/1.1f,v[3]);const int r=juce::jlimit(0,4,integer(5));ratio=ratios[r];threshold=-18.f+1.5f*r;compOn=targets[4]<.5f;knee=r==4?12.f:2.f;if(r==4){attack*=1.f+.8f*slowMemory/12.f;release*=1.f+.12f*slowMemory;ratio=8.f+12.f*juce::jlimit(0.f,1.f,slowMemory/12.f);}preGain=input;}
            else {threshold=-8.f-.48f*v[1];ratio=integer(2)==0?3.f:20.f;attack=.01f;release=.06f+.04f*slowMemory;makeup=makeupGain.get(v[0]);knee=12;}
            float detector=0;
            const bool external=index==0 && key!=nullptr && targets[6]>.5f;
            for(int c=0;c<channels;++c) {float d=(external?(*key)[c]:x[c])*preGain;if(index==2){detectorLow[c]+=detectorLowPole*(d-detectorLow[c]);d+=(1.f-v[4])*3.f*(d-detectorLow[c]);}detector=juce::jmax(detector,d*d);}
            power=detectorPole*power+(1-detectorPole)*detector;
            const float over=10.f*std::log10(juce::jmax(power,1.e-18f))-threshold;
            const float soft=over<-knee*.5f?0.f:over>knee*.5f?over:(over+knee*.5f)*(over+knee*.5f)/(2*knee);
            const float desired=compOn?juce::jlimit(0.f,72.f,soft*(1.f-1.f/ratio)):0.f;
            const float coefficient=envelopePoles[desired>gr?0:1].get(-1./(rate*(desired>gr?attack:juce::jmax(.02f,release))));gr=coefficient*gr+(1-coefficient)*desired;
            slowMemory=memoryPole*slowMemory+(1-memoryPole)*gr;
            reduction=gr*wet;const float gain=dbGain(-gr)*(index==0?1.f+v[5]*(makeup-1.f):makeup)*(index==0?fade:1.f);
            offMemory+=controlPole*((index==1&&integer(6)==3?1.f:0.f)-offMemory);
            for(int c=0;c<channels;++c){float y=x[c]*input*gain;if(index==1)y=saturate(y,integer(5)==4?1.1f:6.f);if(index==1)y+=(x[c]-y)*offMemory;x[c]=y;}if(index==1)reduction*=1.f-offMemory;
        } else if(section==1) {
            float gain=1,headroom=6;bool off=false;int count=3;
            if(index==0){const int g=juce::jlimit(0,20,integer(0));off=g==7;const float gainDb=g<7?-10.f+5.f*g:20.f+5.f*(g-8)-40.f;gain=preampGain.get(gainDb);headroom=2.6f;}
            else if(index==1){const int input=integer(0);gain=preampGain.get(2.f*v[1]+(input>=2?(input==3?18.f:12.f):0.f)-(targets[6]>.5f?20.f:0.f));headroom=input>=2?5.f:12.f;count=5;}
            else if(index==2){const int input=integer(0);const float g=input==2?v[9]-10.f:10.f*integer(1)+(input==0?(targets[2]>.5f?30.f:0.f)-30.f:-20.f)+v[3];gain=preampGain.get(g);headroom=input==0?4.f:8.f;count=2;}
            // Expansion gain staging: the panel gain is offset so a nominal setting is unity on recorded audio.
            else if(index==3){gain=preampGain.get(v[1]-(integer(0)==0?50.f:25.f)-(targets[2]>.5f?20.f:0.f));headroom=3.5f;}
            else if(index==4){const int input=integer(0);gain=preampGain.get(20.f+5.f*integer(1)+v[2]-(input==0?60.f:40.f));headroom=2.2f;}
            else {const bool mic=integer(0)==0;const float stepped=mic?10.f+5.f*integer(1)-40.f:-10.f+5.f*integer(1)-10.f;gain=preampGain.get(stepped+v[2]);headroom=12.f;count=1;}
            gainMemory+=controlPole*((off?0.f:gain)-gainMemory);
            const float phaseMix=index==5?1.f:1.f-2.f*v[index==0?1:index==1?5:index==2?6:index==3?3:5];
            // Asymmetric bias (even harmonics) for the discrete and transformer voicings.
            const float bias=index==3?.05f:index==4?.12f:0.f,biasOffset=saturate(bias,headroom);
            const float outputTrim=index==4?dbGain(v[3]):1.f;
            for(int c=0;c<channels;++c){float y=x[c]*gainMemory;for(int f=0;f<count;++f)y=filters[f].sample(c,y);y=saturate(y+bias,headroom)-biasOffset;dc[c]+=dcPole*(y-dc[c]);x[c]=phaseMix*(y-dc[c])*outputTrim;}
        } else {
            const int count=index==3?5:index>3?6:4;const float in=v[index==0?13:index==1?6:index==2?8:index==3?9:index==4?7:12];
            for(int c=0;c<channels;++c){float y=x[c];for(int f=0;f<count;++f){if(index==1&&f==3&&integer(5)==0)continue;if(index==3&&f>=3&&targets[8]<.5f)continue;if(index==4&&f==5&&integer(6)==0)continue;y=filters[f].sample(c,y);}x[c]+=in*(y-x[c]);}
        }
        float peak=0;
        for(int c=0;c<channels;++c){x[c]*=level;x[c]=juce::jlimit(-64.f,64.f,std::isfinite(x[c])?x[c]:0.f);x[c]=dry[c]+wet*(x[c]-dry[c]);peak=juce::jmax(peak,std::abs(x[c]));}
        const float outDb=db(peak);meterOutput=.99f*meterOutput+.01f*outDb;
        if(section==0&&index>2){if(index==3&&integer(9)==0)meterDb=meterInput;else if(index==3&&integer(9)==2)meterDb=meterOutput;else meterDb=-reduction;}
        else if(section==0){if(index==0)meterDb=-reduction;else if(index==1){const int mode=integer(6);meterDb=mode==0?-reduction:mode==3?-120.f:meterOutput+18.f-(mode==2?4.f:0.f);}else meterDb=integer(3)==1?-reduction:meterOutput+18.f-(integer(3)==2?6.f:0.f);}
        else if(section==1&&index==2)meterDb=targets[11]>.5f?meterOutput:meterInput;else meterDb=meterOutput;
        return x;
    }
};
} // namespace post_native_detail
class PostNativeDSP {
    std::array<std::array<post_native_detail::Model,postNativeModelsPerSection>,3> engines{};
    std::array<std::array<juce::SmoothedValue<float>,postNativeModelsPerSection>,3> morph{};
    std::array<bool,3> selectedReady{};std::array<float,3> readings{{-120,-120,-120}},reductions{};
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> preampAlignment{128};
    const juce::AudioBuffer<float>* key{};
public:
    void prepare(const juce::dsp::ProcessSpec& spec,int preampLatency=0) {
        preampAlignment.prepare(spec);preampAlignment.setDelay(static_cast<float>(juce::jlimit(0,128,preampLatency)));
        for(int s=0;s<3;++s)for(int m=0;m<postNativeModelsPerSection;++m){engines[s][m].prepare(spec.sampleRate,s,m);morph[s][m].reset(spec.sampleRate,.02);morph[s][m].setCurrentAndTargetValue(m==0?1.f:0.f);}reset();
    }
    void reset() noexcept {for(auto& section:engines)for(auto& model:section)model.reset();preampAlignment.reset();selectedReady={};readings={{-120,-120,-120}};reductions={};}
    float meter(int section) const noexcept {return readings[juce::jlimit(0,2,section)];}
    // External sidechain for the block about to be processed (or none).
    void setKey(const juce::AudioBuffer<float>* externalKey) noexcept {key=externalKey;}
    float reduction(int section=0) const noexcept {return reductions[juce::jlimit(0,2,section)];}
    void processSection(juce::AudioBuffer<float>& buffer,int section,const PostNativeSectionState& state) noexcept {
        const int s=juce::jlimit(0,2,section),selected=juce::jlimit(0,postNativeModelsPerSection-1,state.selected),channels=juce::jmin(2,buffer.getNumChannels());
        const bool keyed=key!=nullptr && key->getNumChannels()>0 && key->getNumSamples()==buffer.getNumSamples();
        for(int m=0;m<postNativeModelsPerSection;++m){if(selectedReady[s]&&m==selected&&morph[s][m].getCurrentValue()<=0.f)engines[s][m].reset();engines[s][m].configure(state.banks[m]);if(!selectedReady[s])morph[s][m].setCurrentAndTargetValue(m==selected?1.f:0.f);else morph[s][m].setTargetValue(m==selected?1.f:0.f);}selectedReady[s]=true;
        for(int n=0;n<buffer.getNumSamples();++n) {
            std::array<float,2> dry{},output{},frame{};for(int c=0;c<channels;++c)dry[c]=buffer.getSample(c,n);
            if(keyed)for(int c=0;c<2;++c)frame[(size_t)c]=key->getSample(juce::jmin(c,key->getNumChannels()-1),n);
            for(int m=0;m<postNativeModelsPerSection;++m){const float weight=morph[s][m].getNextValue();if(weight>0.f || m==selected){const auto processed=engines[s][m].process(dry,channels,keyed?&frame:nullptr);for(int c=0;c<channels;++c)output[c]+=weight*processed[c];}}
            for(int c=0;c<channels;++c){float value=output[c];if(s==1){preampAlignment.pushSample(c,value);value=preampAlignment.popSample(c);}buffer.setSample(c,n,value);}
        }
        readings[s]=engines[s][selected].meterDb;reductions[s]=engines[s][selected].reduction;
    }
};
} // namespace spectralforge

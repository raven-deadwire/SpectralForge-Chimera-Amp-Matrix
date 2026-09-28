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
    int integer(int n) const noexcept {return juce::roundToInt(targets[static_cast<size_t>(n)]);}
    void prepare(double sampleRate,int sectionNumber,int modelNumber) {
        section=sectionNumber;index=modelNumber;rate=sampleRate;controlPole=static_cast<float>(1.-std::exp(-1./(rate*.012)));ready=false;filters={};reset();
    }
    void reset() noexcept {filters={};power=gr=slowMemory=0;fade=1;detectorLow={};dc={};clock=0;gainMemory=1;offMemory=0;wet=targetWet=0;reduction=0;meterDb=meterOutput=meterInput=-120;ready=false;}
    void configure(const PostNativeBank& bank) noexcept {
        targets=bank.values;targetTrim=dbGain(bank.trimDb);targetLevel=dbGain(bank.levelDb);targetWet=bank.bypass?0.f:1.f;
        if(!ready){v=targets;trim=targetTrim;level=targetLevel;ready=true;updateFilters();}
    }
    float hz(float value) const noexcept {return juce::jlimit(10.f,static_cast<float>(rate*.44),value);}
    void updateFilters() noexcept {
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
            } else {filters[0].set(C::makeHighPass(rate,hz(targets[7]>.5f?75.f:10.f),.707f));high(1,12000,-.35f);}
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
    std::array<float,2> process(std::array<float,2> dry,int channels) noexcept {
        frame();std::array<float,2> x=dry;for(int c=0;c<channels;++c)x[c]*=trim;
        const float inputPeak=juce::jmax(std::abs(x[0]),channels>1?std::abs(x[1]):0.f);meterInput=db(inputPeak);
        if(section==0) {
            float threshold=-18,ratio=4,attack=.01f,release=.1f,makeup=1,input=1,knee=6;bool compOn=true;float preGain=1;
            if(index==0) {static constexpr float attacks[]{.0001f,.0003f,.001f,.003f,.01f,.03f},releases[]{.1f,.3f,.6f,1.2f,.1f},ratios[]{2,4,10};threshold=v[0];makeup=dbGain(v[1]);attack=attacks[juce::jlimit(0,5,integer(2))];release=releases[juce::jlimit(0,4,integer(3))];ratio=ratios[juce::jlimit(0,2,integer(4))];compOn=targets[5]>.5f;if(integer(3)==4)release=.1f+.08f*slowMemory;const float step=1.f/static_cast<float>(rate*juce::jmax(1.f,v[7]));fade=juce::jlimit(0.f,1.f,fade+(targets[8]>.5f?-step:step));}
            else if(index==1) {static constexpr float ratios[]{4,8,12,20,12};input=dbGain(v[0]);makeup=dbGain(v[1]);attack=.0008f*std::pow(.025f,v[2]);release=1.1f*std::pow(.05f/1.1f,v[3]);const int r=juce::jlimit(0,4,integer(5));ratio=ratios[r];threshold=-18.f+1.5f*r;compOn=targets[4]<.5f;knee=r==4?12.f:2.f;if(r==4){attack*=1.f+.8f*slowMemory/12.f;release*=1.f+.12f*slowMemory;ratio=8.f+12.f*juce::jlimit(0.f,1.f,slowMemory/12.f);}preGain=input;}
            else {threshold=-8.f-.48f*v[1];ratio=integer(2)==0?3.f:20.f;attack=.01f;release=.06f+.04f*slowMemory;makeup=dbGain(v[0]);knee=12;}
            float detector=0;
            for(int c=0;c<channels;++c) {float d=x[c]*preGain;if(index==2){const float p=static_cast<float>(1.-std::exp(-juce::MathConstants<double>::twoPi*1200./rate));detectorLow[c]+=p*(d-detectorLow[c]);d+=(1.f-v[4])*3.f*(d-detectorLow[c]);}detector=juce::jmax(detector,d*d);}
            const float detectorTime=index==0?.003f:index==1?.00003f:.002f;const float dp=static_cast<float>(std::exp(-1./(rate*detectorTime)));power=dp*power+(1-dp)*detector;
            const float over=10.f*std::log10(juce::jmax(power,1.e-18f))-threshold;
            const float soft=over<-knee*.5f?0.f:over>knee*.5f?over:(over+knee*.5f)*(over+knee*.5f)/(2*knee);
            const float desired=compOn?juce::jlimit(0.f,72.f,soft*(1.f-1.f/ratio)):0.f;
            const float coefficient=static_cast<float>(std::exp(-1./(rate*(desired>gr?attack:juce::jmax(.02f,release)))));gr=coefficient*gr+(1-coefficient)*desired;
            const float memoryPole=static_cast<float>(std::exp(-1./(rate*(index==2?1.2:.5))));slowMemory=memoryPole*slowMemory+(1-memoryPole)*gr;
            reduction=gr*wet;const float gain=dbGain(-gr)*(index==0?1.f+v[5]*(makeup-1.f):makeup)*(index==0?fade:1.f);
            offMemory+=controlPole*((index==1&&integer(6)==3?1.f:0.f)-offMemory);
            for(int c=0;c<channels;++c){float y=x[c]*input*gain;if(index==1)y=saturate(y,integer(5)==4?1.1f:6.f);if(index==1)y+=(x[c]-y)*offMemory;x[c]=y;}if(index==1)reduction*=1.f-offMemory;
        } else if(section==1) {
            float gain=1,headroom=6;bool off=false;int count=3;
            if(index==0){const int g=juce::jlimit(0,20,integer(0));off=g==7;const float gainDb=g<7?-10.f+5.f*g:20.f+5.f*(g-8)-40.f;gain=dbGain(gainDb);headroom=2.6f;}
            else if(index==1){const int input=integer(0);gain=dbGain(2.f*v[1]+(input>=2?(input==3?18.f:12.f):0.f)-(targets[6]>.5f?20.f:0.f));headroom=input>=2?5.f:12.f;count=5;}
            else {const int input=integer(0);const float g=input==2?v[9]-10.f:10.f*integer(1)+(input==0?(targets[2]>.5f?30.f:0.f)-30.f:-20.f)+v[3];gain=dbGain(g);headroom=input==0?4.f:8.f;count=2;}
            gainMemory+=controlPole*((off?0.f:gain)-gainMemory);const float phaseMix=1.f-2.f*v[index==0?1:index==1?5:6];
            for(int c=0;c<channels;++c){float y=x[c]*gainMemory;for(int f=0;f<count;++f)y=filters[f].sample(c,y);y=saturate(y,headroom);const float pole=static_cast<float>(1.-std::exp(-juce::MathConstants<double>::twoPi*4./rate));dc[c]+=pole*(y-dc[c]);x[c]=phaseMix*(y-dc[c]);}
        } else {
            for(int c=0;c<channels;++c){float y=x[c];for(int f=0;f<4;++f){if(index==1&&f==3&&integer(5)==0)continue;y=filters[f].sample(c,y);}x[c]+=v[index==0?13:index==1?6:8]*(y-x[c]);}
        }
        float peak=0;
        for(int c=0;c<channels;++c){x[c]*=level;x[c]=juce::jlimit(-64.f,64.f,std::isfinite(x[c])?x[c]:0.f);x[c]=dry[c]+wet*(x[c]-dry[c]);peak=juce::jmax(peak,std::abs(x[c]));}
        const float outDb=db(peak);meterOutput=.99f*meterOutput+.01f*outDb;
        if(section==0){if(index==0)meterDb=-reduction;else if(index==1){const int mode=integer(6);meterDb=mode==0?-reduction:mode==3?-120.f:meterOutput+18.f-(mode==2?4.f:0.f);}else meterDb=integer(3)==1?-reduction:meterOutput+18.f-(integer(3)==2?6.f:0.f);}
        else if(section==1&&index==2)meterDb=targets[11]>.5f?meterOutput:meterInput;else meterDb=meterOutput;
        return x;
    }
};
} // namespace post_native_detail
class PostNativeDSP {
    std::array<std::array<post_native_detail::Model,3>,3> engines{};
    std::array<std::array<juce::SmoothedValue<float>,3>,3> morph{};
    std::array<bool,3> selectedReady{};std::array<float,3> readings{{-120,-120,-120}},reductions{};
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> preampAlignment{128};
public:
    void prepare(const juce::dsp::ProcessSpec& spec,int preampLatency=0) {
        preampAlignment.prepare(spec);preampAlignment.setDelay(static_cast<float>(juce::jlimit(0,128,preampLatency)));
        for(int s=0;s<3;++s)for(int m=0;m<3;++m){engines[s][m].prepare(spec.sampleRate,s,m);morph[s][m].reset(spec.sampleRate,.02);morph[s][m].setCurrentAndTargetValue(m==0?1.f:0.f);}reset();
    }
    void reset() noexcept {for(auto& section:engines)for(auto& model:section)model.reset();preampAlignment.reset();selectedReady={};readings={{-120,-120,-120}};reductions={};}
    float meter(int section) const noexcept {return readings[juce::jlimit(0,2,section)];}
    float reduction(int section=0) const noexcept {return reductions[juce::jlimit(0,2,section)];}
    void processSection(juce::AudioBuffer<float>& buffer,int section,const PostNativeSectionState& state) noexcept {
        const int s=juce::jlimit(0,2,section),selected=juce::jlimit(0,2,state.selected),channels=juce::jmin(2,buffer.getNumChannels());
        for(int m=0;m<3;++m){if(selectedReady[s]&&m==selected&&morph[s][m].getCurrentValue()<=0.f)engines[s][m].reset();engines[s][m].configure(state.banks[m]);if(!selectedReady[s])morph[s][m].setCurrentAndTargetValue(m==selected?1.f:0.f);else morph[s][m].setTargetValue(m==selected?1.f:0.f);}selectedReady[s]=true;
        for(int n=0;n<buffer.getNumSamples();++n) {
            std::array<float,2> dry{},output{};for(int c=0;c<channels;++c)dry[c]=buffer.getSample(c,n);
            for(int m=0;m<3;++m){const float weight=morph[s][m].getNextValue();if(weight>0.f || m==selected){const auto processed=engines[s][m].process(dry,channels);for(int c=0;c<channels;++c)output[c]+=weight*processed[c];}}
            for(int c=0;c<channels;++c){float value=output[c];if(s==1){preampAlignment.pushSample(c,value);value=preampAlignment.popSample(c);}buffer.setSample(c,n,value);}
        }
        readings[s]=engines[s][selected].meterDb;reductions[s]=engines[s][selected].reduction;
    }
};
} // namespace spectralforge

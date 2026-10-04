#include "PostRigGate.h"
#include "LegacyNoiseGate.h"
#include "FXChain.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
std::vector<float> envelope(const std::vector<float>& input,double rate,int block,float release,float hold,float range)
{
    spectralforge::NoiseGate gate;gate.prepare(rate);
    std::vector<float> result(input.size());
    for(size_t offset=0;offset<input.size();offset+=size_t(block)) {
        const int n=int(std::min(size_t(block),input.size()-offset));
        juce::AudioBuffer<float> audio(2,n);
        for(int i=0;i<n;++i) {audio.setSample(0,i,input[offset+size_t(i)]);audio.setSample(1,i,-.7f*input[offset+size_t(i)]);}
        gate.detect(audio,result.data()+offset,true,-40,release,hold,range);
    }
    return result;
}
void timingAndRange(double rate,int block)
{
    // A measured step response: opening threshold -40 dBFS, closing -46 dBFS.
    std::vector<float> step(size_t(rate*1.3),0.f);
    const int note=int(rate*.05);
    std::fill(step.begin(),step.begin()+note,.1f);
    for(float release:{5.f,80.f,500.f})for(float hold:{0.f,20.f,150.f}) {
        const auto g=envelope(step,rate,block,release,hold,96);
        size_t close=size_t(note);while(close<g.size() && g[close]==1.f)++close;
        const double expected=.020*std::log(.1/juce::Decibels::decibelsToGain(-46.f))+hold*.001;
        require(std::abs((double(close)-note)/rate-expected)<4./rate,"Detector decay / hold timing changed");
        const size_t tau=close+size_t(rate*release*.001)-1;
        require(std::abs(g[tau]-std::exp(-1.f))<.002f,"Release is not a one-pole time constant");
    }
    std::vector<float> silence(size_t(rate*.25),0.f);
    for(float range:{0.f,6.f,12.f,24.f,48.f,72.f,95.9f,96.f}) {
        const auto g=envelope(silence,rate,block,5,0,range);
        if(range<96) {
            require(std::abs(juce::Decibels::gainToDecibels(g.back())+range)<.03f,"Limited Range does not settle at the specified dB floor");
            if(range==0)for(float value:g)require(value==1,"Zero Range must be unity");
        } else require(g.back()<1e-18f,"Full must decay toward zero, not a finite -96 dB floor");
        for(size_t i=1;i<g.size();++i)require(g[i]<=g[i-1] && std::isfinite(g[i]),"Closing gain pumps or is non-finite");
    }
    const auto a=envelope(step,rate,64,80,20,24),b=envelope(step,rate,block,80,20,24);
    require(a==b,"Gate envelope depends on block partition");
}
void hysteresisAndPlaying(double rate,int block)
{
    std::vector<float> input(size_t(rate*.6),.008f); // between -40 and -46 dBFS
    std::fill(input.begin(),input.begin()+int(rate*.15),0.f);
    std::fill(input.begin()+int(rate*.3),input.begin()+int(rate*.35),.02f);
    const auto g=envelope(input,rate,block,5,0,24);
    require(g[size_t(rate*.29)]<.064f && g.back()>.999f,"6 dB hysteresis no longer retains open/closed state");
    const int period=int(rate*.12),burst=int(rate*.02);
    std::vector<float> palm(size_t(period*9),0.f);
    for(size_t i=size_t(period);i<palm.size();++i)
        if(int(i)%period<burst)palm[i]=.08f*std::cos(float(juce::MathConstants<double>::twoPi*110*i/rate));
    const auto pg=envelope(palm,rate,block,5,0,24);
    for(int hit=1;hit<9;++hit) {
        const size_t onset=size_t(hit*period);
        require(pg[onset+size_t(rate*.003)]>.995f,"Palm mute attack recovery exceeded 3 ms");
        require(pg[onset+size_t(period-1)]<.064f,"Fast palm mute gap did not settle to Range floor");
        for(size_t i=onset+size_t(burst);i<onset+size_t(period-1);++i)
            require(pg[i+1]<=pg[i]+1e-7f,"Palm mute gap chatters or pumps");
    }
    for(double decay:{1.5,4.0}) {
        std::vector<float> sustain(size_t(rate*3));
        for(size_t i=0;i<sustain.size();++i)
            sustain[i]=float(.08*std::exp(-decay*i/rate)*std::cos(juce::MathConstants<double>::twoPi*110*i/rate));
        const auto sg=envelope(sustain,rate,block,80,20,24);
        bool closing=false;
        for(size_t i=1;i<sg.size();++i) {
            if(sg[i]<.999f)closing=true;
            if(closing)require(sg[i]<=sg[i-1]+1e-7f,"Sustain / volume rolloff chatters after closing");
        }
        require(sg[size_t(rate*.2)]>.999f && sg.back()<.064f,"Sustain open region or rolloff floor changed");
    }
}
void alignmentAndHiss(double rate,int block)
{
    for(int delay:{0,37,2048})for(float range:{0.f,24.f,96.f}) {
        spectralforge::NoiseGate reference;reference.prepare(rate);
        spectralforge::PostRigGate gate;gate.prepare({rate,juce::uint32(block),2},4096);
        juce::AudioBuffer<float> input(2,block),output(2,block);
        std::vector<float> history;
        std::vector<float> gains(size_t(block),1);
        double before=0,after=0;
        for(int b=0;b<int(rate*.8/block);++b) {
            for(int i=0;i<block;++i) {
                const float x=b*block>rate*.2 && b*block<rate*.3 ? .1f : 0.f;
                input.setSample(0,i,0);input.setSample(1,i,x); // right input must trigger both sides
                // Independent residual hiss inserted AFTER a rig, cannot open the input detector.
                const float hiss=.01f*float(std::sin((b*block+i)*1.71));
                output.setSample(0,i,hiss);output.setSample(1,i,-.7f*hiss);
                if(b*block>rate*.6)before+=hiss*hiss;
            }
            reference.detect(input,gains.data(),true,-40,5,0,range);
            history.insert(history.end(),gains.begin(),gains.end());
            gate.detect(input,true,-40,5,0,delay,range);gate.apply(output);
            for(int i=0;i<block;++i) {
                const int index=b*block+i-delay;
                const float expected=index<0 ? 0 : history[size_t(index)];
                const float hiss=.01f*float(std::sin((b*block+i)*1.71));
                require(std::abs(output.getSample(0,i)-hiss*expected)<1e-8f,"Range envelope misaligned with rig latency");
                require(std::abs(output.getSample(1,i)+.7f*output.getSample(0,i))<1e-8f,"Post gate is not stereo linked");
                if(b*block>rate*.6)after+=output.getSample(0,i)*output.getSample(0,i);
            }
        }
        if(range<96)require(std::abs(10*std::log10(after/before)+range)<.03,"Post-rig hiss attenuation differs from Range");
        else require(after/before<1e-20,"Full post gate fails to suppress synthetic residual hiss");
    }
}
void legacyAndAutomation()
{
    gateBaseline::NoiseGate oldSignature;spectralforge::NoiseGate explicitFull;oldSignature.prepare(48000);explicitFull.prepare(48000);
    juce::AudioBuffer<float> a(2,128),b(2,128);
    for(int block=0;block<600;++block) {
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)a.setSample(c,i,block%100<20 ? .1f*std::sin(float(block*128+i)*.03f) : 1e-7f);
        b.makeCopyOf(a);oldSignature.process(a,true,-40,80,20);explicitFull.process(b,true,-40,80,20,96);
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)require(a.getSample(c,i)==b.getSample(c,i),"Default Range changed legacy full-close output");
    }
    spectralforge::NoiseGate gate;gate.prepare(48000);juce::AudioBuffer<float> silence(2,128);silence.clear();std::array<float,128> g{};
    float previous=1;
    for(int b=0;b<900;++b) {
        const float range=b<300 ? 96.f : b<600 ? 0.f : 24.f;
        gate.detect(silence,g.data(),b<800,-40,5,0,range);
        for(float value:g) {require(std::isfinite(value) && value>=0 && value<=1,"Range automation produced invalid gain");require(std::abs(value-previous)<.042f,"Range/bypass automation made an unsmoothed gain jump");previous=value;}
        if(b==599 || b==899)require(previous>.999f,"Zero Range / bypass did not recover unity");
    }
}
}
int main()
{
    try {
        for(double rate:{44100.,48000.,96000.})for(int block:{64,128,256}) {
            timingAndRange(rate,block);hysteresisAndPlaying(rate,block);alignmentAndHiss(rate,block);
            std::cout<<"PASS Gate DSP "<<rate<<" Hz / "<<block<<": timing, range, hysteresis, palm mute, sustain/rolloff, hiss, stereo and latency alignment\n";
        }
        legacyAndAutomation();std::cout<<"PASS default Full equivalence and smoothed Range/bypass automation\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

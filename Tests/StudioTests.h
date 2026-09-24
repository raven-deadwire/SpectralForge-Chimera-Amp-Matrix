#pragma once
#include "FXChain.h"
#include "PerformanceUtilities.h"
namespace studioTests {
inline void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
inline std::vector<float> effects(int selected)
{
    spectralforge::PreFXChain pre;spectralforge::PostFXChain post;pre.prepare({48000,127,2});post.prepare({48000,127,2});
    spectralforge::FXState fx;fx.preCompOn=selected==0;fx.filterOn=selected==1;fx.fuzzOn=selected==2;fx.boostOn=selected==3;fx.driveOn=selected==4;
    fx.busCompOn=selected==5;fx.preampOn=selected==6;fx.eqOn=selected==7;fx.chorusOn=selected==8;fx.delayOn=selected==9;fx.reverbOn=selected==10;fx.eqLow=6;fx.eqMid=-6;fx.eqHigh=6;
    juce::AudioBuffer<float> buffer(2,127);std::vector<float> output;
    for(int b=0;b<400;++b) {
        for(int n=0;n<127;++n) {const double t=(b*127+n)/48000.0;const float x=float(.3*std::sin(juce::MathConstants<double>::twoPi*83*t)+.13*std::sin(juce::MathConstants<double>::twoPi*1301*t));buffer.setSample(0,n,x);buffer.setSample(1,n,-x*.6f);}
        pre.process(buffer,false,-60,80,20,false,0,fx);post.process(buffer,fx);
        if(b>150) for(int n=0;n<127;++n) {const float x=buffer.getSample(0,n);require(std::isfinite(x) && std::abs(x)<10,"Studio FX output is unstable");output.push_back(x);}
    }
    return output;
}
inline std::vector<float> dual(bool cross,float blend,int classic=-1)
{
    spectralforge::Engine engine;engine.prepare({48000,127,1});std::array<spectralforge::LaneState,3> lanes{};lanes[0].amp=0;lanes[1].amp=3;for(auto& lane:lanes)lane.cab=false;
    if(classic==1)lanes[0]=lanes[1];juce::AudioBuffer<float> b(1,127);std::vector<float> result;
    for(int block=0;block<360;++block) {
        for(int n=0;n<127;++n)b.setSample(0,n,.1f*float(std::sin(juce::MathConstants<double>::twoPi*220*(block*127+n)/48000)));
        engine.process(b,classic>=0 ? spectralforge::RoutingMode::classic : spectralforge::RoutingMode::dual,350,1200,lanes,nullptr,cross,blend);
        if(block>120)result.insert(result.end(),b.getReadPointer(0),b.getReadPointer(0)+127);
    }
    return result;
}
inline std::vector<float> dualSum(int blockSize,bool reference)
{
    spectralforge::DualCrossover xo;xo.prepare({48000,(juce::uint32)blockSize,1});
    juce::dsp::LinkwitzRileyFilter<float> allpass;allpass.prepare({48000,(juce::uint32)blockSize,1});allpass.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> cutoff;cutoff.reset(48000,.05);cutoff.setCurrentAndTargetValue(350);
    std::array<juce::AudioBuffer<float>,3> bands;for(auto& b:bands)b.setSize(1,blockSize);juce::AudioBuffer<float> input(1,blockSize);std::vector<float> result;unsigned seed=19838;
    for(int offset=0;offset<48000;) {
        const float hz=(offset/4000)%2 ? 4000.f : 60.f;cutoff.setTargetValue(hz);const int count=juce::jmin(blockSize,4000-offset%4000,48000-offset);input.setSize(1,count,false,false,true);
        for(int n=0;n<count;++n){seed=seed*1664525u+1013904223u;input.setSample(0,n,(float(seed>>8)/8388608.f-1)*.1f);}
        if(reference)for(int n=0;n<count;++n){const float hz=cutoff.getNextValue();if((offset+n)%16==0)allpass.setCutoffFrequency(hz);result.push_back(allpass.processSample(0,input.getSample(0,n)));}
        else{xo.split(input,bands,hz);for(int n=0;n<count;++n)result.push_back(bands[0].getSample(0,n)+bands[1].getSample(0,n));}
        offset+=count;
    }
    return result;
}
inline void run()
{
    {spectralforge::PreFXChain pre;spectralforge::PostFXChain post;pre.prepare({48000,127,2});post.prepare({48000,127,2});spectralforge::FXState state;juce::AudioBuffer<float> impulse(2,127);impulse.clear();impulse.setSample(0,0,1);impulse.setSample(1,0,-.5f);
     pre.process(impulse,false,-60,80,20,false,0,state);post.process(impulse,state);const int delay=pre.latency(false)+post.latency();
     for(int n=0;n<127;++n){require(std::abs(impulse.getSample(0,n)-(n==delay?1.f:0.f))<1e-6,"Bypassed pedal/rack startup is not an exact latency-aligned dry path");require(std::abs(impulse.getSample(0,n)+2*impulse.getSample(1,n))<1e-6,"Bypassed FX changed stereo polarity");}
     std::cout<<"MEASURE all FX bypass at startup: exact dry impulse at "<<delay<<" samples\n";
    }

    const auto off=effects(-1);for(int i=0;i<11;++i) {
        const auto on=effects(i);double residual=0;for(size_t n=0;n<off.size();++n)residual+=std::pow(off[n]-on[n],2);residual=std::sqrt(residual/off.size());
        std::cout<<"MEASURE effect module "<<i<<": enabled/bypass residual RMS "<<residual<<"\n";require(residual>1e-4,"A studio FX module is not connected");
    }
    const auto left=dual(false,0),right=dual(false,1),a=dual(false,0,0),b=dual(false,1,1),blend=dual(false,.25f);
    double endpoint=0,blendError=0;for(size_t n=0;n<a.size();++n){endpoint=juce::jmax(endpoint,std::abs(double(left[n]-a[n])),std::abs(double(right[n]-b[n])));blendError=juce::jmax(blendError,std::abs(double(blend[n]-(.75f*a[n]+.25f*b[n]))));}
    require(endpoint<1e-6 && blendError<1e-6,"Dual blend endpoints or balance are incorrect");
    const auto moving=dualSum(127,false),other=dualSum(511,false),reference=dualSum(127,true);double sum=0,blocks=0;
    for(size_t n=0;n<moving.size();++n){sum=juce::jmax(sum,std::abs(double(moving[n]-reference[n])));blocks=juce::jmax(blocks,std::abs(double(moving[n]-other[n])));}
    std::cout<<"MEASURE Dual blend null "<<blendError<<", crossover/allpass null "<<sum<<", block-size null "<<blocks<<"\n";require(sum<1e-5 && blocks<1e-6,"Dual crossover sum/phase is inconsistent");
    spectralforge::PerformanceUtilities utilities;utilities.prepare({48000,127,2});juce::AudioBuffer<float> buffer(2,127);buffer.clear();utilities.process(buffer,120,false,6,false,false);require(buffer.getMagnitude(0,127)==0,"Performance utilities generate sound while off");
    buffer.clear();utilities.process(buffer,120,false,6,true,true);require(buffer.getMagnitude(0,127)>.01f,"Metronome is not audible");
    std::cout<<"PASS: eleven independent FX modules, Dual blend/crossover and metronome\n";
}
}

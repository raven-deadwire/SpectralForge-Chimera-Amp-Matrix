#pragma once
#include "FXChain.h"
#include <complex>

namespace alignmentTests {
inline void require(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
using Complex=std::complex<double>;
inline Complex allpass(double rate,double cut,double hz) {
    const Complex s(0,std::tan(juce::MathConstants<double>::pi*hz/rate));const double g=std::tan(juce::MathConstants<double>::pi*cut/rate);
    return (s*s-std::sqrt(2.0)*g*s+g*g)/(s*s+std::sqrt(2.0)*g*s+g*g);
}
inline void response(double rate,float low,float high)
{
    int order=14;while((1<<order)<rate*.5)++order;const int length=1<<order;
    spectralforge::Crossover crossover;crossover.prepare({rate,257,1});crossover.set(low,high);crossover.reset();
    std::array<juce::AudioBuffer<float>,3> bands;for(auto& b:bands)b.setSize(1,257);
    juce::AudioBuffer<float> input(1,257);
    std::vector<std::complex<float>> impulse((size_t)length),spectrum((size_t)length);
    for(int offset=0;offset<length;) {
        const int count=juce::jmin(257,length-offset);input.setSize(1,count,false,false,true);input.clear();if(offset==0)input.setSample(0,0,1);
        crossover.split(input,bands);
        for(int n=0;n<count;++n)impulse[(size_t)(offset+n)]={bands[0].getSample(0,n)+bands[1].getSample(0,n)+bands[2].getSample(0,n),0};
        offset+=count;
    }
    juce::dsp::FFT fft(order);fft.perform(impulse.data(),spectrum.data(),false);
    double maxDb=0,maxDegrees=0;
    for(int probe=0;probe<160;++probe) {
        const double hz=20*std::pow(juce::jmin(20000.0,rate*.45)/20.0,probe/159.0);
        const int bin=juce::jmax(1,juce::roundToInt(hz*length/rate));const double exact=bin*rate/length;
        const Complex measured(spectrum[(size_t)bin].real(),spectrum[(size_t)bin].imag());
        const auto expected=allpass(rate,low,exact)*allpass(rate,high,exact);
        maxDb=juce::jmax(maxDb,std::abs(20*std::log10(std::abs(measured))));
        maxDegrees=juce::jmax(maxDegrees,std::abs(std::arg(measured/expected))*180/juce::MathConstants<double>::pi);
    }
    std::cout<<"MEASURE crossover "<<rate<<" Hz / "<<low<<" / "<<high<<": sum ripple "<<maxDb<<" dB; allpass phase error "<<maxDegrees<<" deg\n";
    require(maxDb<.03,"Static crossover sum is not flat");require(maxDegrees<.15,"Crossover sum has an unexpected phase error");
}
inline std::vector<float> moving(int blockSize,bool serialReference)
{
    constexpr double rate=48000;constexpr int total=48000;
    spectralforge::Crossover crossover;crossover.prepare({rate,(juce::uint32)blockSize,1});
    std::array<juce::AudioBuffer<float>,3> bands;for(auto& b:bands)b.setSize(1,blockSize);
    juce::AudioBuffer<float> input(1,blockSize);
    juce::dsp::LinkwitzRileyFilter<float> a,b;
    for(auto* filter:{&a,&b}) {filter->prepare({rate,(juce::uint32)blockSize,1});filter->setType(juce::dsp::LinkwitzRileyFilterType::allpass);}
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> low,high;
    low.reset(rate,.05);high.reset(rate,.05);low.setCurrentAndTargetValue(150);high.setCurrentAndTargetValue(1200);
    std::vector<float> output;unsigned seed=38191;
    for(int offset=0;offset<total;) {
        // Sample-accurate automation schedule, independent of the host block size.
        if(offset%4000==0) {const bool upper=(offset/4000)%2;const float l=upper?350.f:60.f,h=upper?4000.f:500.f;crossover.set(l,h);low.setTargetValue(l);high.setTargetValue(h);}
        const int count=juce::jmin(blockSize,4000-offset%4000,total-offset);input.setSize(1,count,false,false,true);
        for(int n=0;n<count;++n) {seed=seed*1664525u+1013904223u;input.setSample(0,n,(float(seed>>8)/8388608.f-1)*.15f);}
        if(serialReference) {
            for(int n=0;n<count;++n) {const float l=low.getNextValue(),h=high.getNextValue();if((offset+n)%16==0){a.setCutoffFrequency(l);b.setCutoffFrequency(h);}output.push_back(b.processSample(0,a.processSample(0,input.getSample(0,n))));}
        } else {
            crossover.split(input,bands);for(int n=0;n<count;++n)output.push_back(bands[0].getSample(0,n)+bands[1].getSample(0,n)+bands[2].getSample(0,n));
        }
        offset+=count;
    }
    return output;
}
inline double nullPeak(const std::vector<float>& a,const std::vector<float>& b) {double peak=0;for(size_t i=0;i<a.size();++i)peak=juce::jmax(peak,std::abs(double(a[i]-b[i])));return peak;}
inline std::vector<float> rig(spectralforge::RoutingMode mode,int quality,bool transparent,bool post)
{
    spectralforge::Engine engine;engine.prepare({48000,127,2});engine.setOversampling(quality);
    spectralforge::PostFXChain effects;effects.prepare({48000,127,2});
    spectralforge::FXState fx;fx.delayOn=fx.reverbOn=fx.busCompOn=fx.preampOn=fx.eqOn=fx.chorusOn=post;fx.eqMid=4;fx.delayMs=71;fx.delayMix=.3f;fx.reverbMix=.15f;
    std::array<spectralforge::LaneState,3> states{};for(auto& lane:states) {lane.cab=false;lane.amp=2;lane.drive=.6f;lane.ampEnabled=!transparent;}
    juce::AudioBuffer<float> input(2,127);std::vector<float> result;
    for(int block=0;block<380;++block) {
        for(int n=0;n<127;++n) {const double t=(block*127+n)/48000.0;const float x=float(.07*std::sin(juce::MathConstants<double>::twoPi*73*t)+.03*std::sin(juce::MathConstants<double>::twoPi*1031*t));input.setSample(0,n,x);input.setSample(1,n,-x*.5f);}
        engine.process(input,mode,150,1200,states);effects.process(input,fx);
        if(block>120)result.insert(result.end(),input.getReadPointer(0),input.getReadPointer(0)+127);
    }
    return result;
}
inline void lowDI()
{
    auto render=[](bool driveOn,int os) {
        spectralforge::PreFXChain pre;pre.prepare({48000,127,2});spectralforge::Engine engine;engine.prepare({48000,127,2});engine.setOversampling(os);
        spectralforge::FXState fx;fx.driveOn=fx.fuzzOn=fx.boostOn=driveOn;fx.drive=1;fx.driveLevel=12;
        std::array<spectralforge::LaneState,3> states{};states[0].solo=true;states[0].cab=driveOn;states[0].drive=driveOn?1.f:0.f;states[0].amp=driveOn?3:0;
        juce::AudioBuffer<float> input(2,127);std::vector<float> result;
        for(int block=0;block<320;++block) {
            for(int n=0;n<127;++n) {const double t=(block*127+n)/48000.0;const float x=float(.16*std::sin(juce::MathConstants<double>::twoPi*61.73*t)+.07*std::sin(juce::MathConstants<double>::twoPi*1201*t));input.setSample(0,n,x);input.setSample(1,n,-x*.5f);}
            pre.process(input,false,-60,80,20,false,0,fx);
            const float x1=block<160 ? 60.f : 350.f,x2=block<160 ? 500.f : 4000.f;
            engine.process(input,spectralforge::RoutingMode::matrix,x1,x2,states,&pre.cleanOutput());
            if(block>60) {for(int n=0;n<127;++n) require(std::abs(input.getSample(0,n)+2*input.getSample(1,n))<1e-6f,"LOW DI lost linked stereo polarity");result.insert(result.end(),input.getReadPointer(0),input.getReadPointer(0)+127);}
        }
        return result;
    };
    const auto clean=render(false,0);
    for(int os=0;os<4;++os) {
        const double error=nullPeak(clean,render(true,os));std::cout<<"MEASURE LOW DI vs maximum pre drive / amp / cab, "<<(1<<os)<<"x: null "<<error<<"\n";
        require(error<1e-6,"Matrix LOW DI is affected by drive, amp, cab or oversampling");
    }
    for(double rate:{44100.0,48000.0,96000.0,192000.0}) for(int os=0;os<4;++os) {
        spectralforge::Engine engine;engine.prepare({rate,127,1});engine.setOversampling(os);
        spectralforge::PreFXChain pre;pre.prepare({rate,127,1});spectralforge::FXState fx;
        std::array<spectralforge::LaneState,3> states{};states[0].ampEnabled=false;states[0].cab=false;
        juce::AudioBuffer<float> block(1,127);
        for(int b=0;b<40;++b) {block.clear();pre.process(block,false,-60,80,20,false,0,fx);engine.process(block,spectralforge::RoutingMode::classic,150,1200,states);}
        block.clear();block.setSample(0,0,1);pre.process(block,false,-60,80,20,false,0,fx);engine.process(block,spectralforge::RoutingMode::classic,150,1200,states);
        const int delay=engine.latency()+pre.latency(false);
        for(int n=0;n<127;++n) require(std::abs(block.getSample(0,n)-(n==delay ? 1.f : 0.f))<1e-6,"Actual dry impulse latency disagrees with reported module latency");
        std::cout<<"MEASURE dry impulse "<<rate<<" Hz / "<<(1<<os)<<"x: "<<delay<<" samples (reported = measured)\n";
    }
}
inline void lowBlend()
{
    const auto render=[](double rate,int os,float mix,float drive,int model,bool visitClassic) {
        spectralforge::Engine engine;engine.prepare({rate,127,2});engine.setOversampling(os);
        std::array<spectralforge::LaneState,3> states{};
        states[0].solo=true;states[0].cab=false;states[0].lowComp=.35f;
        states[0].lowAmpMix=mix;states[0].drive=drive;states[0].amp=model;
        juce::AudioBuffer<float> buffer(2,127);std::vector<float> result;
        if(visitClassic) for(int b=0;b<30;++b) {
            for(int c=0;c<2;++c) for(int n=0;n<127;++n) buffer.setSample(c,n,.2f);
            engine.process(buffer,spectralforge::RoutingMode::classic,150,1200,states);
        }
        for(int b=0;b<180;++b) {
            for(int n=0;n<127;++n) {const float x=.18f*float(std::sin(juce::MathConstants<double>::twoPi*73*(b*127+n)/rate));buffer.setSample(0,n,x);buffer.setSample(1,n,-.37f*x);}
            engine.process(buffer,spectralforge::RoutingMode::matrix,b<90?150.f:280.f,1200,states);
            if(b>40) for(int c=0;c<2;++c) for(int n=0;n<127;++n) {const float x=buffer.getSample(c,n);require(std::isfinite(x),"LOW blend produced invalid audio");result.push_back(x);}
        }
        return result;
    };
    for(double rate:{44100.0,96000.0}) for(int os=0;os<4;++os) {
        const auto di=render(rate,os,0,0,5,false),wet=render(rate,os,1,0,5,false),half=render(rate,os,.5f,0,5,false);
        double error=0;for(size_t i=0;i<half.size();++i) error=std::max(error,std::abs(double(half[i]-(di[i]+wet[i])*.5f)));
        require(error<2e-6,"LOW DI/amp merge has inconsistent timing or blend gain");
        require(nullPeak(wet,render(rate,os,1,1,5,false))<1e-6,"Hidden Classic drive changes Matrix LOW amp");
        require(nullPeak(wet,render(rate,os,1,1,5,true))<1e-5,"Classic drive state leaks into Matrix LOW");
        require(nullPeak(di,wet)>.001,"LOW amp blend is not audible");
        require(nullPeak(wet,render(rate,os,1,0,7,false))>.0001,"LOW head selector is not connected");
        std::cout<<"MEASURE LOW DI/amp blend "<<rate<<" Hz / "<<(1<<os)<<"x: endpoint interpolation null "<<error<<"; drive fixed at zero\n";
    }
}
inline void compressor()
{
    for(float knob:{0.f,.35f,.8f}) {
        spectralforge::LowCompressor comp;comp.prepare(48000);juce::AudioBuffer<float> input(2,127);double before=0,after=0;
        for(int b=0;b<1000;++b) {
            for(int n=0;n<127;++n) {const float x=.25f*float(std::sin(juce::MathConstants<double>::twoPi*61.73*(b*127+n)/48000));input.setSample(0,n,x);input.setSample(1,n,-x*.5f);if(b>600) before+=x*x;}
            comp.process(input,knob);
            for(int n=0;n<127;++n) {const float x=input.getSample(0,n);require(std::isfinite(x),"Compressor output is non-finite");require(std::abs(x+2*input.getSample(1,n))<1e-7,"Compressor is not stereo linked");if(b>600)after+=x*x;}
        }
        const double measured=-10*std::log10(after/before),threshold=-12-36*knob,ratio=1+7*knob;
        const double predicted=juce::jmax(0.0,20*std::log10(.25/std::sqrt(2.0))-threshold)*(1-1/ratio);
        std::cout<<"MEASURE LOW COMP "<<knob<<": reduction "<<measured<<" dB, steady RMS law "<<predicted<<" dB\n";
        require(std::abs(measured-predicted)<.35,"Compressor threshold/ratio law is inconsistent");
        if(knob==0) require(std::abs(measured)<1e-8,"Zero COMP is not unity");
    }
}
inline void run()
{
    lowDI();lowBlend();compressor();
    for(double rate:{44100.0,48000.0,96000.0,192000.0})for(auto cross:{std::pair<float,float>{60,500},{150,1200},{350,4000}})response(rate,cross.first,cross.second);
    const auto a=moving(127,false),b=moving(511,false),reference=moving(127,true);
    const double blockError=nullPeak(a,b),sumError=nullPeak(a,reference);
    double levelA=0,levelReference=0;for(size_t i=0;i<a.size();++i){levelA+=a[i]*a[i];levelReference+=reference[i]*reference[i];}
    const double levelDb=10*std::log10(levelA/levelReference);
    std::cout<<"MEASURE moving crossovers: block-size null "<<blockError<<"; serial allpass null "<<sumError<<"; level difference "<<levelDb<<" dB\n";
    require(blockError<1e-6 && sumError<.00005 && std::abs(levelDb)<.01,"Moving crossover sum/phase/level is inconsistent");
    int latency=-1;
    for(int os=0;os<4;++os) {
        spectralforge::Amp amp;amp.prepare({48000,127,2});amp.setOversampling(os);
        if(latency<0)latency=amp.latency();require(amp.latency()==latency,"Oversampling reports inconsistent latency");
        for(bool post:{false,true}) {
            const auto classic=rig(spectralforge::RoutingMode::classic,os,false,post),dual=rig(spectralforge::RoutingMode::dual,os,false,post);
            const double error=nullPeak(classic,dual);std::cout<<"MEASURE "<<(1<<os)<<"x Classic vs identical Dual merge, post="<<post<<": null "<<error<<"\n";
            require(error<1e-6,"Identical parallel lanes are not aligned or post FX ran per lane");
        }
    }
    const auto transparent1=rig(spectralforge::RoutingMode::matrix,0,true,false),transparent8=rig(spectralforge::RoutingMode::matrix,3,true,false);
    require(nullPeak(transparent1,transparent8)<1e-6,"Bypassed lane delay is not aligned across oversampling factors");
    std::cout<<"PASS: crossover sum/phase/level automation, block independence, lane/oversampling/merge alignment and global post scope\n";
}
}

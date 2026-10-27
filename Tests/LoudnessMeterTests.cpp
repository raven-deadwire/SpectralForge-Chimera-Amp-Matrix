// Studio loudness / true-peak meter (docs/LOUDNESS_METER_1_5.md): the
// BS.1770-4 48 kHz K-weighting coefficients, EBU Tech 3341 minimum
// requirements cases 1-5 (momentary, short-term, gated integrated loudness)
// at 44.1/48/96 kHz, true peak against analytic inter-sample peaks, mono,
// reset and no allocation; the processor meters exactly its output.
#include "PluginProcessor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using spectralforge::LoudnessMeter;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Segment{double dbfs,seconds;};
// Stereo (identical channels) or mono 1 kHz sine segments, phase continuous,
// fed in 512-sample blocks.
void feed(LoudnessMeter& m,double rate,const std::vector<Segment>& program,int channels=2,double hz=1000.,double phase=0.,double resetAt=-1) {
    std::vector<float> l(512),r(512);const float* data[2]{l.data(),r.data()};long t=0;
    for(const auto& s:program){
        const long end=t+long(std::llround(s.seconds*rate));const double a=std::pow(10.,s.dbfs/20.);
        while(t<end){const int n=int(std::min<long>(512,end-t));
            if(resetAt>=0 && t>=long(resetAt*rate)){m.requestReset();resetAt=-1;}
            for(int i=0;i<n;++i){l[size_t(i)]=r[size_t(i)]=float(a*std::sin(juce::MathConstants<double>::twoPi*hz*double(t+i)/rate+phase));}
            m.process(data,channels,n);t+=n;}
    }
}
void coefficients() {
    // ITU-R BS.1770-4, Table 1 and Table 2 (48 kHz).
    constexpr double reference[10]{1.53512485958697,-2.69169618940638,1.19839281085285,-1.69065929318241,.73248077421585,
                                   1.,-2.,1.,-1.99004745483398,.99007225036621};
    const auto k=LoudnessMeter::kWeighting(48000.);double worst=0;
    for(int i=0;i<10;++i)worst=std::max(worst,std::abs(k[size_t(i)]-reference[i]));
    std::cout<<"MEASURE K-weighting at 48 kHz vs BS.1770-4 tables: largest coefficient difference "<<worst<<"\n";
    require(worst<1e-8,"K-weighting differs from the BS.1770-4 coefficients");
    std::cout<<"PASS K-weighting coefficients\n";
}
void ebu3341() {
    for(const double rate:{44100.,48000.,96000.}) {
        double worst=0;
        const auto check=[&](const std::vector<Segment>& program,double expected,bool steady,const char* message){
            LoudnessMeter m;m.prepare(rate);feed(m,rate,program);
            double e=std::abs(m.integrated()-expected);
            if(steady)e=std::max({e,std::abs(m.momentary()-expected),std::abs(m.shortTerm()-expected)});
            worst=std::max(worst,e);require(e<=.1,message);};
        check({{-23,20}},-23,true,"EBU 3341 case 1: stereo -23 dBFS sine is not -23.0 LUFS");
        check({{-33,20}},-33,true,"EBU 3341 case 2: stereo -33 dBFS sine is not -33.0 LUFS");
        check({{-36,10},{-23,60},{-36,10}},-23,false,"EBU 3341 case 3: relative gate");
        check({{-72,10},{-36,10},{-23,60},{-36,10},{-72,10}},-23,false,"EBU 3341 case 4: absolute and relative gates");
        check({{-26,20},{-20,20.1},{-26,20}},-23,false,"EBU 3341 case 5: level steps");
        std::cout<<"MEASURE EBU Tech 3341 cases 1-5 at "<<rate<<" Hz: largest error "<<worst<<" LU (limit 0.1)\n";
    }
    // Mono: one channel at -23 dBFS is 3 dB below the same tone on two.
    {LoudnessMeter m;m.prepare(48000);feed(m,48000,{{-23,10}},1);
     require(std::abs(m.integrated()+26.01)<.1,"Mono loudness is not the single-channel sum");}
    std::cout<<"PASS EBU Tech 3341 cases 1-5 (44.1/48/96 kHz), mono channel sum\n";
}
void truePeak() {
    double worst=0,worstSampleGap=0;
    for(const double rate:{44100.,48000.,96000.})
        for(const double hz:{997.,rate/4.,5000.,11025.,15000.})
            for(const double phase:{0.,.7853981633974483,1.3}) {
                LoudnessMeter m;m.prepare(rate);feed(m,rate,{{-6,1.2}},2,hz,phase,.2);   // reset after the onset step
                const double error=m.truePeak()+6.;   // the analytic peak is -6 dBTP
                worst=std::max(worst,std::abs(error));
                require(error<=.2 && error>=-.4,"True peak outside EBU Tech 3341's +0.2/-0.4 dB");
                if(std::abs(hz-rate/4)<1 && phase>.7 && phase<.8)worstSampleGap=std::max(worstSampleGap,double(m.truePeak())+6.);
            }
    // fs/4 at 45 degrees: every sample sits 3 dB below the true peak.
    std::cout<<"MEASURE true peak, sines at -6 dBTP (997 Hz-15 kHz, fs/4 at 45 deg, three phases, 44.1/48/96 kHz): largest error "<<worst<<" dB; fs/4 at 45 deg reads "<<worstSampleGap<<" dB from the true peak (samples alone: -3.01 dB)\n";
    require(worst<.1,"True peak is less accurate than designed");
    // High tone over 48 start phases. 4x points are 30.6 degrees apart on a
    // 15 kHz sine at 44.1 kHz: one cycle alone could read up to 0.31 dB short,
    // but a steady tone's points drift across every phase, so the maximum
    // converges on the true peak.
    {double lowest=0,highest=-1;
     for(int step=0;step<48;++step){LoudnessMeter m;m.prepare(44100);feed(m,44100,{{-6,.5}},2,15000.,juce::MathConstants<double>::halfPi*step/48.,.2);
         lowest=std::min(lowest,double(m.truePeak())+6.);highest=std::max(highest,double(m.truePeak())+6.);}
     std::cout<<"MEASURE true peak, 15 kHz at 44.1 kHz over 48 phases: "<<lowest<<" to "<<highest<<" dB (single-cycle geometric limit -0.31 dB)\n";
     require(lowest>=-.4 && highest<=.2,"True peak at its worst phase exceeds EBU Tech 3341's tolerance");}
    std::cout<<"PASS true peak within 0.1 dB (EBU allows +0.2/-0.4)\n";
}
void resetAndSilence() {
    LoudnessMeter m;m.prepare(48000);
    require(m.integrated()==LoudnessMeter::silent && m.truePeak()==LoudnessMeter::silent,"A new meter does not read silent");
    feed(m,48000,{{-60,5}});require(std::abs(m.integrated()+60)<.1,"-60 dBFS sine misread");
    feed(m,48000,{{-90,5}});require(std::abs(m.momentary()+90)<.1,"Momentary does not follow a quiet passage");
    require(std::abs(m.integrated()+60)<.2,"Blocks under -70 LUFS reached the integrated value");
    m.requestReset();feed(m,48000,{{-30,.05}});
    require(m.integrated()==LoudnessMeter::silent && m.momentary()==LoudnessMeter::silent,"Reset did not clear the meter");
    feed(m,48000,{{-30,3}});require(std::abs(m.integrated()+30)<.1 && std::abs(m.truePeak()+30)<.1,"The meter does not measure again after reset");
    std::cout<<"PASS silence, absolute gate, reset\n";
}
void noAllocation() {
    LoudnessMeter m;m.prepare(48000);std::vector<float> l(256),r(256);const float* data[2]{l.data(),r.data()};
    allocations=0;countAllocations=true;
    for(int b=0;b<4000;++b){for(int i=0;i<256;++i)l[size_t(i)]=r[size_t(i)]=.3f*float(std::sin(.07*(b*256+i)));m.process(data,2,256);if(b==2000)m.requestReset();}
    countAllocations=false;require(allocations==0,"The meter allocates while processing");
    std::cout<<"PASS no allocation while metering (21 s, with a reset)\n";
}

// The processor's meter reads its final output: a separate meter fed the
// same output blocks gives identical values; reset clears it.
void processorOutput() {
    ChimeraProcessor p;p.prepareToPlay(48000,256);juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    LoudnessMeter reference;reference.prepare(48000);long t=0;
    for(int block=0;block<int(6*48000/256);++block){
        for(int n=0;n<256;++n,++t){const float x=.2f*float(std::sin(juce::MathConstants<double>::twoPi*220.*double(t)/48000.));audio.setSample(0,n,x);audio.setSample(1,n,x);}
        p.processBlock(audio,midi);reference.process(audio.getArrayOfReadPointers(),2,256);
    }
    const auto& m=p.outputLoudness();
    std::cout<<"MEASURE processor output meter: I "<<m.integrated()<<" LUFS, TP "<<m.truePeak()<<" dBTP; separate meter on the same output: I "<<reference.integrated()<<", TP "<<reference.truePeak()<<"\n";
    require(m.integrated()>LoudnessMeter::silent && m.integrated()==reference.integrated() && m.truePeak()==reference.truePeak()
            && m.momentary()==reference.momentary() && m.shortTerm()==reference.shortTerm(),"The processor does not meter its final output");
    p.resetOutputLoudness();audio.clear();p.processBlock(audio,midi);
    require(m.integrated()==LoudnessMeter::silent && m.truePeak()==LoudnessMeter::silent,"Processor meter reset does not clear it");
    std::cout<<"PASS processor meters its final output; reset clears it\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {coefficients();ebu3341();truePeak();resetAndSilence();noAllocation();processorOutput();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}

#pragma once
#include "PedalOctaverDSP.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace pedalOctaverTests {
inline void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
template<class Engine> std::vector<float> render(double rate,int channelCount,const std::vector<double>& notes,const std::array<float,3>& levels,int block=256,bool impulse=false) {
    Engine e;e.prepare({rate,(juce::uint32)block,(juce::uint32)channelCount});
    const int length=int(rate*1.5);std::vector<float> result((size_t)length);juce::AudioBuffer<float> b(channelCount,block);
    for(int pos=0;pos<length;pos+=block) {
        const int count=juce::jmin(block,length-pos);b.setSize(channelCount,count,false,false,true);b.clear();
        for(int n=0;n<count;++n) {
            float x=0;if(impulse)x=pos+n==0?1.f:0.f;
            else for(const double f:notes)x+=float(.15/notes.size()*std::sin(juce::MathConstants<double>::twoPi*f*(pos+n)/rate));
            b.setSample(0,n,x);
        }
        e.process(b,levels);
        for(int n=0;n<count;++n) {
            const float x=b.getSample(0,n);require(std::isfinite(x)&&std::abs(x)<4,"Octaver output unstable");result[(size_t)(pos+n)]=x;
            if(channelCount>1)require(b.getSample(1,n)==0,"Octaver stereo channel crossfeed");
        }
    }
    return result;
}
inline double magnitude(const std::vector<float>& x,double rate,double f) {
    const int begin=int(rate*.7),end=int(x.size());double real=0,imaginary=0;
    for(int n=begin;n<end;++n) {
        const double w=.5-.5*std::cos(juce::MathConstants<double>::twoPi*(n-begin)/(end-begin));
        const double p=juce::MathConstants<double>::twoPi*f*n/rate;
        real+=x[(size_t)n]*w*std::cos(p);imaginary+=x[(size_t)n]*w*std::sin(p);
    }
    return std::hypot(real,imaginary)*4/(end-begin);
}
inline double peak(const std::vector<float>& x,double rate,double expected) {
    double best=0,frequency=0;
    for(double f=expected-2;f<=expected+2;f+=.25){const double m=magnitude(x,rate,f);if(m>best){best=m;frequency=f;}}
    return frequency;
}
inline void run() {
    using namespace spectralforge;
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2}) {
        PedalMonoOctaver mono;mono.prepare({rate,256,(juce::uint32)channels});require(mono.latency()==0,"Divider invented transport latency");
        PedalPolyOctaver poly;poly.prepare({rate,256,(juce::uint32)channels});const int latency=poly.latency();
        require(latency==(rate>64000?4096:2048),"Spectral latency contract changed");
        for(bool isPoly:{false,true}) {
            const auto impulse=isPoly?render<PedalPolyOctaver>(rate,channels,{}, {1,0,0},256,true):render<PedalMonoOctaver>(rate,channels,{}, {1,0,0},256,true);
            const int expected=isPoly?latency:0;
            for(size_t n=0;n<impulse.size();++n)require(impulse[n]==(n==(size_t)expected?1.f:0.f),"Octaver dry impulse latency mismatch");
            const auto zero=isPoly?render<PedalPolyOctaver>(rate,channels,{110}, {0,0,0}):render<PedalMonoOctaver>(rate,channels,{110}, {0,0,0});
            for(float x:zero)require(x==0,"Zero octave controls did not mute exactly");
        }
        for(double f:{55.,110.,220.,440.}) {
            for(int voice:{1,2}) {
                std::array<float,3> controls{};controls[(size_t)voice]=1;
                const auto monoResult=render<PedalMonoOctaver>(rate,channels,{f},controls);
                const double expected=f/(voice==1?2:4),actual=peak(monoResult,rate,expected),level=magnitude(monoResult,rate,expected);
                require(std::abs(actual-expected)<=.5 && level>.015,"Mono octave pitch/amplitude mismatch");
                const auto polyResult=render<PedalPolyOctaver>(rate,channels,{f},controls);
                const double expectedPoly=f*(voice==1?.5:2),actualPoly=peak(polyResult,rate,expectedPoly),polyLevel=magnitude(polyResult,rate,expectedPoly);
                std::cout<<"pitch rate="<<rate<<" channels="<<channels<<" input="<<f<<" voice="<<voice<<" mono="<<actual<<" poly="<<actualPoly<<" polyLevel="<<polyLevel<<'\n';
                require(std::abs(actualPoly-expectedPoly)<=.75 && polyLevel>.006,"Poly octave pitch/amplitude mismatch");
            }
        }
        const std::vector<double> chord{220.,277.1826,329.6276};
        for(int voice:{1,2}) {
            std::array<float,3> controls{};controls[(size_t)voice]=1;
            const auto result=render<PedalPolyOctaver>(rate,channels,chord,controls);
            for(double f:chord) {
                const double expected=f*(voice==1?.5:2),actual=peak(result,rate,expected),level=magnitude(result,rate,expected);
                std::cout<<"chord rate="<<rate<<" channels="<<channels<<" target="<<expected<<" peak="<<actual<<" amplitude="<<level<<'\n';
                require(std::abs(actual-expected)<=1.25 && level>.002,"Poly chord note missing or displaced");
            }
        }
        // Equivalent streams split at different callback boundaries produce the
        // same sample sequence. This tests persistent hop and tracking state.
        for(bool isPoly:{false,true}) {
            const auto a=isPoly?render<PedalPolyOctaver>(rate,channels,{110,440},{.3f,.5f,.5f},256):render<PedalMonoOctaver>(rate,channels,{110},{.3f,.5f,.5f},256);
            const auto b=isPoly?render<PedalPolyOctaver>(rate,channels,{110,440},{.3f,.5f,.5f},111):render<PedalMonoOctaver>(rate,channels,{110},{.3f,.5f,.5f},111);
            require(a==b,"Octaver depends on host block segmentation");
        }
        std::cout<<"PASS octavers "<<rate<<" Hz / "<<channels<<" ch; spectral latency="<<latency<<" samples\n";
    }
}
}

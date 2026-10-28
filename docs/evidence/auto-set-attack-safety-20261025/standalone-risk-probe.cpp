#include "AutoSet.h"
#include "MultibandDynamicsCore.h"
#include <iostream>
#include <random>
using namespace spectralforge::autoset;
std::vector<float> targetNoise(bool bass,double rate,double seconds,std::mt19937& random) {
    constexpr int order=15,size=1<<order;
    juce::dsp::FFT fft(order);std::vector<float> spectrum(size_t(2*size));
    std::uniform_real_distribution<double> phase(0,2*juce::MathConstants<double>::pi);
    for(int k=1;k<size/2;++k) {
        const double f=k*rate/size;
        // Band power is the sum of bins; third-octave width grows with f.
        const double magnitude=std::sqrt(std::pow(10.,toneTargetDb(bass,float(std::clamp(f,20.,20000.)))/10.)/f)*(f<35?0:1);
        const double ph=phase(random);spectrum[size_t(2*k)]=float(magnitude*std::cos(ph));spectrum[size_t(2*k+1)]=float(magnitude*std::sin(ph));
    }
    fft.performRealOnlyInverseTransform(spectrum.data());
    float peak=0;for(int n=0;n<size;++n)peak=std::max(peak,std::abs(spectrum[size_t(n)]));
    std::vector<float> out(size_t(seconds*rate));
    for(size_t n=0;n<out.size();++n)out[n]=.25f*spectrum[n%size_t(size)]/peak;
    return out;
}
void shelve(std::vector<float>& x,double frequency,double gain,double rate) {
    const auto c=spectralforge::eqCoefficients(2,frequency,gain,.707,rate);double z1=0,z2=0;
    for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}
// Plucked notes: fundamentals in the instrument's range, harmonics falling at 6 dB/octave.
std::vector<float> notes(double rate,double seconds,std::array<double,3> fundamentals,int harmonics) {
    std::vector<float> out(size_t(seconds*rate));
    for(size_t n=0;n<out.size();++n) {
        const double t=double(n)/rate,cycle=std::fmod(t,1.),f=fundamentals[size_t(int(t)%3)];double x=0;
        for(int h=1;h<=harmonics && f*h<rate*.45;++h)x+=std::sin(2*juce::MathConstants<double>::pi*f*h*t+h)/h;
        out[n]=float(.2*std::exp(-cycle*2)*x);
    }
    return out;
}
float maximumReduction(const std::vector<float>& samples,double rate,const FinishResult& result) {
    spectralforge::DeEsserCore core;core.prepare(rate);
    core.setSettings({result.deEsser,result.frequency,result.thresholdDb,result.rangeDb,result.attackMs,result.releaseMs,result.ratio,result.kneeDb,false});
    core.reset();float maximum=0;
    for(float x:samples){float* channels[]{&x};core.process(channels,1,1);maximum=std::max(maximum,core.reduction());}
    return maximum;
}
void riskProbe() {
    for(double rate:{44100.,48000.,96000.}) {
        for(const auto& f:std::vector<std::pair<const char*,std::array<double,3>>>{{"dropA_guitar",{55,65.406,82.407}},{"bass5_lowB",{30.868,41.203,55}},{"bass6_highC",{130.813,164.814,196}}}) {
            const auto x=notes(rate,6,f.second,40);
            const auto t=analyseTone(x,rate);
            std::cout<<"CLASS "<<rate<<' '<<f.first<<" bass="<<t.bass<<"\n";
        }
        for(const bool muted:{false,true})for(const bool clipped:{false,true}) {
            std::mt19937 random{417};auto x=targetNoise(false,rate,6,random);
            for(size_t n=0;n<x.size();++n) {
                const double t=double(n)/rate,cycle=std::fmod(t,.25);
                const double env=std::exp(-cycle*(muted?45.:12.));
                const double attack=cycle<.008?.16*std::sin(2*juce::MathConstants<double>::pi*6000*t)*std::exp(-cycle*500):0;
                double y=x[n]*env+attack;
                x[n]=float(clipped?.25*std::tanh(y*12):y);
            }
            for(auto style:{Style::rock,Style::clean,Style::metal}){
                const auto f=analyseFinish(x,{},rate,false,style);
                std::cout<<"ATTACK "<<rate<<" mute="<<muted<<" clip="<<clipped<<' '<<styleName(style)<<" on="<<f.deEsser<<" spec="<<f.spectralExcessDb<<" burst="<<f.burstExcessDb<<" split="<<f.frequency<<" range="<<f.rangeDb<<" maxGR="<<maximumReduction(x,rate,f)<<"\n";
            }
        }
    }
}

int main(){riskProbe();}

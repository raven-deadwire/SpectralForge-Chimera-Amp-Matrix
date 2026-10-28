#include <juce_dsp/juce_dsp.h>
#include "PerformanceUtilities.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

// Doubler mono-sum probe. The Doubler mixes 35 % of a delayed copy into each
// channel (left 0.55 x spread, right 1 x spread, +/-0.25 ms modulation) after
// the amp. Mixing a delayed copy combs the spectrum; summing L and R to mono
// combs it again. This measures, per spread setting, the 1/3-octave response
// of L, R and the mono sum against the dry signal, the low band where Drop A
// fundamentals and palm mutes live, L/R correlation, pick-attack level, and
// that the dry path stays undelayed. Definitions: docs/DOUBLER_MONO_SUM.md.
namespace {
void require(bool ok,const std::string& message) {if(!ok)throw std::runtime_error(message);}
constexpr double rate=48000;constexpr int block=256;

struct Output {std::vector<float> left,right;};
Output run(const std::vector<float>& x,bool on,float spread) {
    spectralforge::PerformanceUtilities doubler;doubler.prepare({rate,(juce::uint32)block,2});
    Output out;out.left.reserve(x.size());out.right.reserve(x.size());
    juce::AudioBuffer<float> b(2,block);
    for(size_t offset=0;offset<x.size();offset+=block) {
        const int n=int(std::min<size_t>(block,x.size()-offset));
        b.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i){b.setSample(0,i,x[offset+size_t(i)]);b.setSample(1,i,x[offset+size_t(i)]);}
        doubler.process(b,120,on,spread,false,false);
        for(int i=0;i<n;++i){
            require(std::isfinite(b.getSample(0,i)) && std::isfinite(b.getSample(1,i)),"Non-finite doubler output");
            out.left.push_back(b.getSample(0,i));out.right.push_back(b.getSample(1,i));
        }
    }
    return out;
}

// Welch power spectrum, 8192-point Hann, 50 % overlap; skips the first 0.1 s
// (mix ramp, delay line fill).
std::vector<double> spectrum(const std::vector<float>& x) {
    constexpr int order=13,size=1<<order;
    juce::dsp::FFT fft(order);std::vector<float> window(size),frame(2*size);std::vector<double> power(size/2+1,0.);
    for(int i=0;i<size;++i)window[size_t(i)]=float(.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/(size-1)));
    int frames=0;
    for(size_t start=size_t(rate*.1);start+size<=x.size();start+=size/2,++frames) {
        std::fill(frame.begin(),frame.end(),0.f);
        for(int i=0;i<size;++i)frame[size_t(i)]=x[start+size_t(i)]*window[size_t(i)];
        fft.performFrequencyOnlyForwardTransform(frame.data());
        for(int k=0;k<=size/2;++k)power[size_t(k)]+=double(frame[size_t(k)])*frame[size_t(k)];
    }
    require(frames>0,"Fixture too short");
    return power;
}
struct Band {double centre,gainDb;};
// Output/input power per 1/3 octave, 31.5 Hz to 16 kHz.
std::vector<Band> bands(const std::vector<double>& out,const std::vector<double>& in) {
    std::vector<Band> result;const double binHz=rate/8192;
    for(int k=-15;k<=12;++k) {
        const double centre=1000*std::pow(2.,k/3.),lo=centre*std::pow(2.,-1/6.),hi=centre*std::pow(2.,1/6.);
        double o=0,i=0;
        for(size_t bin=size_t(std::ceil(lo/binHz));bin<=size_t(hi/binHz) && bin<in.size();++bin){o+=out[bin];i+=in[bin];}
        if(i>0)result.push_back({centre,10*std::log10(o/i)});
    }
    return result;
}
double correlation(const std::vector<float>& a,const std::vector<float>& b) {
    double ab=0,aa=0,bb=0;for(size_t i=size_t(rate*.1);i<a.size();++i){ab+=double(a[i])*b[i];aa+=double(a[i])*a[i];bb+=double(b[i])*b[i];}
    return ab/std::sqrt(aa*bb+1e-30);
}
std::vector<float> monoSum(const Output& o) {std::vector<float> m(o.left.size());for(size_t i=0;i<m.size();++i)m[i]=.5f*(o.left[i]+o.right[i]);return m;}

// A1 palm-mute plucks every 160 ms (as in the switch probe).
std::vector<float> plucks(double seconds) {
    std::vector<float> x(size_t(seconds*rate));const int period=int(rate*.16);
    for(size_t n=0;n<x.size();++n){const double t=double(int(n)%period)/rate,env=.3*std::exp(-t*9);double v=0;for(int h=1;h<=6;++h)v+=std::sin(juce::MathConstants<double>::twoPi*55*h*t)/h;x[n]=float(env*v);}
    return x;
}
// Mean peak of the first 3 ms of every pluck after the first 0.2 s, in dB.
double attackDb(const std::vector<float>& x) {
    const int period=int(rate*.16),span=int(rate*.003);double sum=0;int count=0;
    for(int start=period*2;start+span<int(x.size());start+=period){float peak=0;for(int i=0;i<span;++i)peak=std::max(peak,std::abs(x[size_t(start+i)]));sum+=20*std::log10(peak+1e-12);++count;}
    return sum/std::max(1,count);
}
}

int main()
{
    try {
        std::mt19937 random{5};std::normal_distribution<float> gauss(0.f,.1f);
        std::vector<float> noise(size_t(rate*8));for(auto& v:noise)v=gauss(random);
        const auto inSpectrum=spectrum(noise);
        // Off is the dry signal, sample for sample.
        {
            const auto off=run(noise,false,6);
            for(size_t i=0;i<noise.size();++i)require(off.left[i]==noise[i] && off.right[i]==noise[i],"Doubler off changed the signal");
        }
        const auto pluck=plucks(4);const double dryAttack=attackDb(pluck);
        std::cout<<"DOUBLER_PROTOCOL rate=48000 mix=0.35 left=0.55*spread right=spread mod=+/-0.25ms@0.13Hz noise=white8s welch=8192 bands=1/3oct mean_db=arithmetic_band_db rms_db=unweighted_broadband\n";
        for(const float spread:{1.f,3.f,6.f,10.f,15.f,20.f}) {
            const auto out=run(noise,true,spread);const auto mono=monoSum(out);
            // The dry path is not delayed: the strongest lag between input and
            // left output is 0 (65 % dry against 35 % delayed).
            {
                double best=-1;int bestLag=-1;
                for(int lag=0;lag<int(rate*.03);++lag){double c=0;for(size_t i=size_t(rate*.1);i+size_t(lag)<noise.size() && i<size_t(rate*1.1);++i)c+=double(noise[i])*out.left[i+size_t(lag)];if(c>best){best=c;bestLag=lag;}}
                require(bestLag==0,"The doubler delayed the dry path");
            }
            for(const auto& [name,signal]:std::vector<std::pair<const char*,std::vector<float>>>{{"left",out.left},{"right",out.right},{"mono",mono}}) {
                const auto b=bands(spectrum(signal),inSpectrum);
                double inputPower=0,outputPower=0;
                for(size_t i=size_t(rate*.1);i<signal.size();++i){inputPower+=double(noise[i])*noise[i];outputPower+=double(signal[i])*signal[i];}
                const double rmsDb=10*std::log10(outputPower/inputPower);
                Band worst=b.front();double low=0;int lowCount=0;double total=0;
                for(const auto& band:b){if(band.gainDb<worst.gainDb)worst=band;if(band.centre>=40 && band.centre<=160){low+=band.gainDb;++lowCount;}total+=band.gainDb;}
                double spreadDb=0;for(const auto& band:b)spreadDb=std::max(spreadDb,std::abs(band.gainDb-total/double(b.size())));
                std::cout<<"DOUBLER spread_ms="<<spread<<" signal="<<name<<" worst_band_hz="<<juce::roundToInt(worst.centre)<<" worst_db="<<juce::String(worst.gainDb,2)
                         <<" low_40_160_db="<<juce::String(low/lowCount,2)<<" mean_db="<<juce::String(total/double(b.size()),2)<<" rms_db="<<juce::String(rmsDb,2)<<" ripple_db="<<juce::String(spreadDb,2)<<'\n';
            }
            const auto pluckOut=run(pluck,true,spread);
            std::cout<<"DOUBLER spread_ms="<<spread<<" correlation_lr="<<juce::String(correlation(out.left,out.right),3)
                     <<" attack_left_db="<<juce::String(attackDb(pluckOut.left)-dryAttack,2)<<" attack_mono_db="<<juce::String(attackDb(monoSum(pluckOut))-dryAttack,2)
                     <<" first_notch_right_hz="<<juce::roundToInt(1000./(2*spread))<<" first_notch_left_hz="<<juce::roundToInt(1000./(2*.55*spread))<<'\n';
        }
        std::cout<<"PASS doubler mono-sum probe: off is bit-identical, dry path undelayed, all spreads measured\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    return 0;
}

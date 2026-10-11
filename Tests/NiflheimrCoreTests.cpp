#include "NiflheimrDSP.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace { bool watch=false; std::size_t allocations=0; }
void* operator new(std::size_t n) {
    if(watch) ++allocations;
    if(auto* p=std::malloc(std::max(n,std::size_t{1}))) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace spectralforge::niflheimr;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
float stimulus(int n,double rate) {
    const double t=n/rate,local=std::fmod(t,.14);
    const double envelope=local<.105?std::exp(-18*local):0.;
    return float(.075*envelope*(std::sin(2*detail::pi*30.8677*t)
           +.6*std::sin(2*detail::pi*92.6031*t)+.3*std::sin(2*detail::pi*463.0155*t)
           +.18*std::sin(2*detail::pi*1975.5328*t)));
}
std::vector<float> render(State state,int count=24000,double rate=48000,double inputScale=1) {
    NiflheimrDSP dsp; dsp.prepare(rate); dsp.set(state); dsp.reset();
    std::vector<float> out(std::size_t(count),0.f);
    for(int n=0;n<count;++n) out[std::size_t(n)]=dsp.tick(float(inputScale*stimulus(n,rate)),0);
    return out;
}
double rms(const std::vector<float>& x) {
    double e=0; for(auto v:x) e+=double(v)*v; return std::sqrt(e/x.size());
}
double matchedDifference(const std::vector<float>& a,const std::vector<float>& b) {
    const double ar=rms(a),br=rms(b); double e=0;
    require(ar>1.e-12&&br>1.e-12,"Unexpected silent render");
    for(std::size_t n=0;n<a.size();++n) e+=std::pow(a[n]/ar-b[n]/br,2);
    return std::sqrt(e/a.size());
}
void cleanAndBlend() {
    State clean; clean[Control::blend]=0;
    const auto reference=render(clean),scaled=render(clean,24000,48000,.25);
    for(std::size_t n=0;n<reference.size();++n)
        require(std::abs(reference[n]*.25f-scaled[n])<1.e-8f,"Clean endpoint is not linear");
    for(int ch=0;ch<channelCount;++ch) {
        auto s=channelState(ch); s[Control::blend]=0;
        s[Control::gain]=1;
        for(auto c:{Control::mass,Control::split,Control::fang,Control::fold,Control::thrust}) s[c]=1;
        require(render(s)==reference,"Gain/channel/macros contaminate clean endpoint");
        auto dirty=channelState(ch); dirty[Control::blend]=1;
        const auto d=render(dirty);
        for(float mix:{.25f,.5f,.65f}) {
            auto middle=dirty; middle[Control::blend]=mix;
            const auto m=render(middle);
            for(std::size_t n=0;n<m.size();++n)
                require(std::abs(m[n]-((1-mix)*reference[n]+mix*d[n]))<1.e-7f,
                        "BLEND is not a linear clean/dirty interpolation");
        }
    }
    // Dirty automation is ignored even while coefficients are moving.
    NiflheimrDSP changing,baseline; changing.set(clean); changing.reset(); baseline.set(clean); baseline.reset();
    for(int n=0;n<12000;++n) {
        if(n%137==0) {
            clean.channel=(n/137)%channelCount; clean[Control::gain]=float((n/137)%2);
            for(auto c:{Control::mass,Control::split,Control::fang,Control::fold,Control::thrust}) clean[c]=float((n/137)%2);
            changing.set(clean);
        }
        require(changing.tick(stimulus(n,48000),0)==baseline.tick(stimulus(n,48000),0),
                "Moving dirty settings modulate clean endpoint");
    }
    std::cout<<"PASS clean linearity, moving macro isolation, 15 linear BLEND comparisons\n";
}
void channelStructureAndGain() {
    std::array<std::vector<float>,channelCount> audio;
    for(int ch=0;ch<channelCount;++ch) {
        auto s=channelState(ch); audio[std::size_t(ch)]=render(s);
        s[Control::gain]=.75f; const auto upper=render(s);
        s[Control::gain]=1; const auto full=render(s);
        const double first=matchedDifference(audio[std::size_t(ch)],upper),second=matchedDifference(upper,full);
        require(first>.02&&second>.02,"Upper gain travel is only volume or exhausted");
        std::cout<<"GAIN "<<channelKeys[ch]<<" 5_to_7.5="<<first<<" 7.5_to_10="<<second<<'\n';
        for(auto control:{Control::mass,Control::split,Control::fang,Control::fold,Control::thrust}) {
            auto a=channelState(ch),b=a; a[control]=0; b[control]=1;
            const double difference=matchedDifference(render(a),render(b));
            std::cout<<"MACRO "<<channelKeys[ch]<<'/'<<controls[std::size_t(control)].id<<" residual="<<difference<<'\n';
            require(difference>.002,"A channel macro has no level-independent response");
        }
    }
    double minimum=100;
    for(int a=0;a<channelCount;++a) for(int b=a+1;b<channelCount;++b) {
        const double difference=matchedDifference(audio[std::size_t(a)],audio[std::size_t(b)]);
        minimum=std::min(minimum,difference);
        std::cout<<"PAIR "<<channelKeys[a]<<'/'<<channelKeys[b]<<" matched_residual="<<difference<<'\n';
        require(difference>.12,"Channel prototypes collapse after level matching");
    }
    std::cout<<"PAIR_MINIMUM "<<minimum<<" (synthetic probe; not musical/EQ-matched acceptance)\n";
}
double fundamental(State state,double hz,double amplitude) {
    constexpr double rate=48000;
    NiflheimrDSP dsp; dsp.prepare(rate); dsp.set(state); dsp.reset();
    double real=0,imaginary=0;
    // Identical analysis window for clean/dirty; exact musical note frequencies
    // matter here because the original cancellation was level/phase dependent.
    for(int n=0;n<48000;++n) {
        const double phase=2*detail::pi*hz*n/rate;
        const auto y=dsp.tick(float(amplitude*std::sin(phase)),0);
        if(n>=12000) { real+=y*std::cos(phase); imaginary+=y*std::sin(phase); }
    }
    return 2*std::hypot(real,imaginary)/36000;
}
void protectedLow() {
    double minimum=100,maximum=0; int probes=0;
    for(double hz:{27.5,30.8677,41.2034,55.,65.4064,82.4069}) for(double amplitude:{.01,.2,.5}) {
        State clean; clean[Control::blend]=0;
        const double reference=fundamental(clean,hz,amplitude);
        for(int ch=0;ch<channelCount;++ch) for(float gain:{.5f,.75f,1.f}) for(float split:{0.f,.5f,1.f}) {
            auto dirty=channelState(ch); dirty[Control::blend]=1; dirty[Control::split]=split;
            dirty[Control::gain]=gain;
            const double ratio=fundamental(dirty,hz,amplitude)/reference;
            minimum=std::min(minimum,ratio); maximum=std::max(maximum,ratio); ++probes;
            if(ratio<.89125||ratio>1.12203)
                std::cout<<"LOW_FAILURE hz="<<hz<<" amplitude="<<amplitude<<" channel="<<ch
                         <<" gain="<<gain<<" split="<<split<<" ratio="<<ratio<<'\n';
            require(ratio>.89125&&ratio<1.12203,"Protected 27.5-82.4 Hz fundamental deviates over 1 dB from clean");
        }
    }
    std::cout<<"PROTECTED_LOW probes="<<probes<<" min_ratio="<<minimum<<" max_ratio="<<maximum<<'\n';
}
// Synthetic probes isolate three mechanisms that a waveform-difference test
// cannot: bass-driven intermodulation, generated low-mid harmonics, and the
// first hit versus a following gap. They are technical regressions only.
enum class ClarityProbe { twoTone, upperOnly, bassOnly, mutedRiff };
std::vector<float> clarityRender(State state,ClarityProbe probe) {
    constexpr double rate=48000;
    NiflheimrDSP dsp; dsp.prepare(rate); dsp.set(state); dsp.reset();
    std::vector<float> audio(96000);
    for(int n=0;n<96000;++n) {
        const double t=n/rate;
        const double bass=.16*std::sin(2*detail::pi*40*t);
        const double upper=.025*std::sin(2*detail::pi*1000*t);
        double x=probe==ClarityProbe::twoTone?bass+upper:
                 probe==ClarityProbe::upperOnly?upper:bass;
        if(probe==ClarityProbe::mutedRiff) {
            const double local=std::fmod(t,.125);
            const double envelope=local<.075?std::exp(-40*local):0.;
            x=envelope*(.75*bass+.05*std::sin(2*detail::pi*240*t)+upper);
        }
        audio[std::size_t(n)]=dsp.tick(float(x),0);
    }
    return audio;
}
double settledMagnitude(const std::vector<float>& audio,double hz) {
    double real=0,imaginary=0;
    for(std::size_t n=48000;n<audio.size();++n) {
        const double phase=2*detail::pi*hz*double(n)/48000;
        real+=audio[n]*std::cos(phase); imaginary+=audio[n]*std::sin(phase);
    }
    return 2*std::hypot(real,imaginary)/48000;
}
void clarityWithoutGainLoss() {
    for(int ch:{1,3,4}) for(float gain:{.5f,.75f,1.f}) {
        auto state=channelState(ch); state[Control::blend]=1; state[Control::gain]=gain;
        const auto together=clarityRender(state,ClarityProbe::twoTone);
        const auto upper=clarityRender(state,ClarityProbe::upperOnly);
        const auto bass=clarityRender(state,ClarityProbe::bassOnly);
        auto interaction=together;
        for(std::size_t n=0;n<interaction.size();++n)
            interaction[n]-=upper[n]+bass[n];
        double sidebands=0,lowMid=0,harmonics=0,upperEnergy=0;
        for(int hz=600;hz<=1400;hz+=40) if(hz!=1000)
            sidebands+=std::pow(settledMagnitude(interaction,hz),2);
        for(int hz=160;hz<=480;hz+=40)
            lowMid+=std::pow(settledMagnitude(bass,hz),2);
        for(int hz=2000;hz<=20000;hz+=1000)
            harmonics+=std::pow(settledMagnitude(upper,hz),2);
        for(std::size_t n=48000;n<upper.size();++n)
            upperEnergy+=double(upper[n])*upper[n];
        const double imd=std::sqrt(sidebands)/settledMagnitude(together,1000);
        const double mud=std::sqrt(lowMid)/settledMagnitude(bass,40);
        const double thd=std::sqrt(harmonics)/settledMagnitude(upper,1000);
        const double upperRms=std::sqrt(upperEnergy/48000);
        std::cout<<"CLARITY "<<channelKeys[ch]<<" gain="<<gain<<" imd="<<imd
                 <<" bass_lowmid="<<mud<<" upper_thd="<<thd<<" upper_rms="<<upperRms<<'\n';
        // These tolerances leave headroom beyond the measured correction.
        // The previous full-residual drive fails the IMD and low-mid bounds.
        require(imd<.035,"Bass fundamental again modulates the driven upper band");
        require(mud<(gain==.5f?.003:.015),"Bass-only probe generates excess 160-480 Hz energy");
        // Prevent satisfying clarity bounds by muting or linearising the voice.
        require(thd>.07&&upperRms>.085,"Clarity correction loses nonlinear drive or output strength");
        if(gain!=.5f) continue;
        const auto riff=clarityRender(state,ClarityProbe::mutedRiff);
        double hitEnergy=0,sustainEnergy=0,tailEnergy=0;
        int hits=0,sustains=0,tails=0;
        for(std::size_t n=24000;n<riff.size();++n) {
            const auto local=n%6000; const double e=double(riff[n])*riff[n];
            if(local<576) { hitEnergy+=e; ++hits; }
            if(local>=1440&&local<2880) { sustainEnergy+=e; ++sustains; }
            if(local>=3840) { tailEnergy+=e; ++tails; }
        }
        const double hit=std::sqrt(hitEnergy/hits),sustain=std::sqrt(sustainEnergy/sustains);
        const double tail=std::sqrt(tailEnergy/tails);
        std::cout<<"TRANSIENT "<<channelKeys[ch]<<" hit_sustain="<<hit/sustain
                 <<" gap_hit="<<tail/hit<<'\n';
        const double minimumHit=ch==3?2.5:1.5;
        require(hit/sustain>minimumHit,"First-hit energy regressed relative to sustained body");
        require(tail/hit<.016,"Muted gap retains excessive nonlinear/filter tail");
    }
    std::cout<<"PASS synthetic clarity/drive regressions; instrument-DI listening remains pending\n";
}
void highInternalRateTiming() {
    // This caught a real 768 kHz cap: prepare(192000*8) silently doubled every
    // filter corner and changed every time constant at the production rate.
    for(double rate:{768000.,1536000.,3072000.}) {
        NiflheimrDSP dsp; State s; s[Control::blend]=0;
        dsp.prepare(rate); dsp.set(s); dsp.reset(); double energy=0; int measured=0;
        const int settle=int(rate*.3),window=int(rate*.4);
        for(int n=0;n<settle+window;++n) {
            const auto y=dsp.tick(float(.1*std::sin(2*detail::pi*10*n/rate)),0);
            if(n>=settle) { energy+=double(y)*y; ++measured; }
        }
        const double ratio=std::sqrt(2*energy/measured)/.1;
        require(std::abs(ratio-std::sqrt(.5))<.001,"True internal sample rate did not preserve 10 Hz input filter timing");
        std::cout<<"INNER_RATE "<<rate<<" clean_10hz_gain="<<ratio<<'\n';
    }
}
void realtimeAndStability() {
    double peak=0;
    for(double hostRate:{44100.,48000.,96000.}) for(int factor:{1,2,4,8}) {
        NiflheimrDSP dsp; const double rate=hostRate*factor; dsp.prepare(rate);
        State s;
        watch=true;
        for(int n=0;n<int(rate*.13);++n) {
            if(n%173==0) {
                s.channel=(n/173)%channelCount;
                for(std::size_t i=0;i<controlCount;++i)
                    s.values[i]=((n/173+int(i))%2)?controls[i].minimum:controls[i].maximum;
                dsp.set(s);
            }
            const double y=dsp.tick(4*stimulus(n,rate),0),right=dsp.tick(0,1);
            if(!std::isfinite(y)||std::abs(y)>64||right!=0) {
                watch=false; throw std::runtime_error("Automation unstable or stereo crosstalk");
            }
            peak=std::max(peak,std::abs(y));
        }
        dsp.reset(); watch=false;
        for(int n=0;n<512;++n) require(dsp.tick(0,0)==0,"Reset retains audio");
    }
    require(allocations==0,"set/reset/tick allocated");
    for(int ch=0;ch<channelCount;++ch) {
        NiflheimrDSP dsp; State s=channelState(ch);
        for(std::size_t i=0;i<controlCount;++i) s.values[i]=controls[i].maximum;
        dsp.set(s); dsp.reset(); double tail=0;
        for(int n=0;n<144000;++n) {
            const double y=dsp.tick(n<24000?8*stimulus(n,48000):0,0);
            require(std::isfinite(y)&&std::abs(y)<=64,"Extreme state exceeds numerical rail");
            if(n>140000) tail=std::max(tail,std::abs(y));
        }
        require(tail<1.e-7,"Nonlinear/envelope/filter memory does not release to silence");
    }
    State bad; bad.values.fill(std::numeric_limits<float>::quiet_NaN()); bad.channel=-1;
    NiflheimrDSP dsp; dsp.set(bad); dsp.reset();
    require(dsp.state()==State{},"Nonfinite state does not restore defaults");
    require(std::isfinite(dsp.tick(std::numeric_limits<float>::infinity(),0)),"Nonfinite input poisoned state");
    require(dsp.tick(1,-1)==0&&dsp.tick(1,2)==0,"Invalid stereo index not rejected");
    require(render(State{})==render(State{}),"Render nondeterministic");
    std::cout<<"REALTIME rates=12 allocations="<<allocations<<" moving_peak="<<peak<<'\n';
}
int main() {
    try {
        cleanAndBlend(); channelStructureAndGain(); protectedLow(); clarityWithoutGainLoss();
        highInternalRateTiming(); realtimeAndStability();
        std::cout<<"PASS Niflheimr prototype contracts; real DI, alias, CPU and musical acceptance pending\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

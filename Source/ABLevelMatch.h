#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace spectralforge {

// A/B comparison level match. Measures the output loudness of each A/B slot
// while it plays (BS.1770 K-weighting, about 3 s short-term, silence gated) and,
// when enabled, turns the louder slot down to the quieter one so the comparison
// is about sound, not level. It never boosts, so it cannot clip or push the
// limiter. It is a monitoring aid: not a parameter, not part of either slot and
// not saved, so a project always opens with it off.
class ABLevelMatch {
public:
    static constexpr double windowSeconds=3,holdoffSeconds=.25,minimumSeconds=1.5;
    static constexpr double gateLufs=-60,maximumCutDb=24;

    // Message thread.
    void setEnabled(bool on) noexcept {enabledFlag.store(on);}
    bool enabled() const noexcept {return enabledFlag.load();}
    // Forget both slots (a new project or reference was loaded).
    void forget() noexcept {forgetRequested.store(true);}
    // The active slot was copied to the other one: it now sounds the same.
    void copied(int from) noexcept {copyRequested.store(from);}
    // Current cut on the output in dB (0 or negative), for display.
    float appliedDb() const noexcept {return applied.load();}
    // Measured loudness of a slot in LUFS, or NaN until it has been heard long enough.
    float loudness(int slot) const noexcept {return slots[size_t(slot&1)].lufs.load();}

    void prepare(double sampleRate,int channels) {
        rate=sampleRate;holdoffSamples=int(holdoffSeconds*rate);
        designKWeighting();
        state.assign(size_t(juce::jmax(1,channels)),{});
        gain.reset(rate,.05);gain.setCurrentAndTargetValue(1.f);applied.store(0);
        lastSlot=-1;holdoff=0;
    }
    // Audio thread, on the final output. slot is the active A/B slot.
    void process(juce::AudioBuffer<float>& buffer,int slot) noexcept {
        slot&=1;
        if(forgetRequested.exchange(false))for(auto& s:slots){s.power=0;s.seconds=0;s.lufs.store(std::nanf(""));}
        if(const int from=copyRequested.exchange(-1);from>=0) {
            auto& source=slots[size_t(from&1)];auto& target=slots[size_t(1-(from&1))];
            target.power=source.power;target.seconds=source.seconds;target.lufs.store(source.lufs.load());
        }
        if(slot!=lastSlot){lastSlot=slot;holdoff=holdoffSamples;} // skip the switch transient
        const int n=buffer.getNumSamples(),channels=juce::jmin(buffer.getNumChannels(),int(state.size()));
        if(n<=0)return;
        measure(buffer,channels,n,slots[size_t(slot)]);

        float target=1.f;
        const auto& self=slots[size_t(slot)];const auto& other=slots[size_t(1-slot)];
        if(enabledFlag.load() && self.seconds>=minimumSeconds && other.seconds>=minimumSeconds) {
            const double cut=std::clamp(10*std::log10(other.power/self.power),-maximumCutDb,0.);
            target=float(juce::Decibels::decibelsToGain(cut));
        }
        gain.setTargetValue(target);
        if(!gain.isSmoothing() && gain.getCurrentValue()==1.f){applied.store(0);return;} // off: output untouched
        if(!gain.isSmoothing())buffer.applyGain(gain.getCurrentValue());
        else for(int i=0;i<n;++i){const float g=gain.getNextValue();for(int c=0;c<buffer.getNumChannels();++c)buffer.getWritePointer(c)[i]*=g;}
        applied.store(juce::Decibels::gainToDecibels(gain.getCurrentValue(),-100.f));
    }

private:
    struct Biquad {double b0{1},b1{},b2{},a1{},a2{};};
    struct Channel {std::array<double,2> z1{},z2{};};
    struct Slot {double power{},seconds{};std::atomic<float> lufs{std::nanf("")};};

    void designKWeighting() {
        // ITU-R BS.1770 pre-filter (high shelf) and RLB high-pass at any rate.
        double K=std::tan(juce::MathConstants<double>::pi*1681.974450955533/rate);
        const double Vh=std::pow(10.,3.999843853973347/20),Vb=std::pow(Vh,.4996667741545416),Q=.7071752369554196;
        double a0=1+K/Q+K*K;
        stages[0]={(Vh+Vb*K/Q+K*K)/a0,2*(K*K-Vh)/a0,(Vh-Vb*K/Q+K*K)/a0,2*(K*K-1)/a0,(1-K/Q+K*K)/a0};
        K=std::tan(juce::MathConstants<double>::pi*38.13547087602444/rate);const double Q2=.5003270373238773;
        a0=1+K/Q2+K*K;
        stages[1]={1,-2,1,2*(K*K-1)/a0,(1-K/Q2+K*K)/a0};
    }
    void measure(const juce::AudioBuffer<float>& buffer,int channels,int n,Slot& s) noexcept {
        double energy=0;
        for(int c=0;c<channels;++c) {
            auto& ch=state[size_t(c)];const float* x=buffer.getReadPointer(c);
            for(int i=0;i<n;++i) {
                double v=x[i];
                for(size_t k=0;k<stages.size();++k) {
                    const auto& f=stages[k];const double y=f.b0*v+ch.z1[k];
                    ch.z1[k]=f.b1*v-f.a1*y+ch.z2[k];ch.z2[k]=f.b2*v-f.a2*y;v=y;
                }
                energy+=v*v;
            }
        }
        for(auto& ch:state)for(auto& z:ch.z1)if(!std::isfinite(z))ch={};
        if(holdoff>0){holdoff-=n;return;}
        const double meanSquare=energy/n; // BS.1770: channel mean squares sum
        if(!std::isfinite(meanSquare) || -.691+10*std::log10(meanSquare+1e-30)<gateLufs)return;
        // A running mean until a full window is heard, then a 3 s exponential window.
        const double seconds=n/rate,weight=s.seconds+seconds<windowSeconds?seconds/(s.seconds+seconds):1-std::exp(-seconds/windowSeconds);
        s.power+=weight*(meanSquare-s.power);s.seconds+=seconds;
        if(s.seconds>=minimumSeconds)s.lufs.store(float(-.691+10*std::log10(s.power)));
    }

    double rate{48000};int holdoffSamples{},holdoff{},lastSlot{-1};
    std::array<Biquad,2> stages;std::vector<Channel> state;
    std::array<Slot,2> slots;
    juce::SmoothedValue<float> gain;
    std::atomic<bool> enabledFlag{false},forgetRequested{false};std::atomic<int> copyRequested{-1};
    std::atomic<float> applied{0};
};

}

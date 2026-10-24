#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Auto Set listens to the player for a few seconds and proposes input gain,
// gate and clean-compressor settings from rule-based measurements. The audio
// thread only reduces samples to 10 ms windows; all decisions run elsewhere.

struct Window {float peakDb{-120.f},rmsDb{-120.f};};
enum class Tap {input=0,compressor=1};

inline float toDb(double linear) noexcept {return float(20.0*std::log10(std::max(1e-6,linear)));}

// Single producer (audio thread) / single consumer (message thread) window
// queue per tap. Allocation-free after construction; overflow drops windows.
class Capture {
public:
    static constexpr int capacity=4096;
    void prepare(double sampleRate) noexcept {
        windowSamples=std::max(1,int(sampleRate*.010));
        for(auto& t:taps){t.count=0;t.peak=0;t.energy=0;}
    }
    void listen(std::optional<Tap> tap) noexcept {active.store(tap?int(*tap):-1,std::memory_order_release);}
    bool listening(Tap tap) const noexcept {return active.load(std::memory_order_acquire)==int(tap);}
    // Audio thread. Mirrors the clean compressor detector: max of the channels.
    void push(Tap tap,const float* const* channels,int channelCount,int samples) noexcept {
        if(!listening(tap))return;
        auto& t=taps[size_t(tap)];
        for(int n=0;n<samples;++n) {
            double x=0;
            for(int c=0;c<std::min(2,channelCount);++c)x=std::max(x,double(std::abs(channels[c][n])));
            if(!std::isfinite(x))x=0;
            t.peak=std::max(t.peak,x);t.energy+=x*x;
            if(++t.count>=windowSamples) {
                const int w=t.write.load(std::memory_order_relaxed),next=(w+1)%capacity;
                if(next!=t.read.load(std::memory_order_acquire)) {
                    t.ring[size_t(w)]={toDb(t.peak),toDb(std::sqrt(t.energy/t.count))};
                    t.write.store(next,std::memory_order_release);
                }
                t.count=0;t.peak=0;t.energy=0;
            }
        }
    }
    // Consumer thread.
    void drain(Tap tap,std::vector<Window>& out) noexcept {
        auto& t=taps[size_t(tap)];
        int r=t.read.load(std::memory_order_relaxed);
        const int w=t.write.load(std::memory_order_acquire);
        while(r!=w){out.push_back(t.ring[size_t(r)]);r=(r+1)%capacity;}
        t.read.store(r,std::memory_order_release);
    }
private:
    struct TapState {
        std::array<Window,capacity> ring{};std::atomic<int> write{0},read{0};
        int count{};double peak{},energy{};
    };
    std::array<TapState,2> taps;
    std::atomic<int> active{-1};
    int windowSamples{480};
};

inline float percentile(std::vector<float> values,float fraction) {
    if(values.empty())return -120.f;
    std::sort(values.begin(),values.end());
    const auto index=size_t(std::clamp(fraction,0.f,1.f)*float(values.size()-1)+.5f);
    return values[index];
}

// Rule targets. A DI around -9 dBFS peak leaves headroom for hot pickups while
// giving the amp models the level their factory voicings were tuned for.
inline constexpr float targetPeakDb=-9.f,minimumPlayingDb=-60.f;

struct InputGateResult {
    bool playingDetected{},pausesDetected{};
    float inputGainDb{},gateThresholdDb{},gateRangeDb{};
    float noisePeakDb{-120.f},playingPeakDb{-120.f},gapDb{};
};

// windows: measured after the current input gain, where the gate detects.
inline InputGateResult analyseInputGate(const std::vector<Window>& windows,float currentInputDb) {
    InputGateResult r;r.inputGainDb=currentInputDb;
    std::vector<float> peaks,rms;
    for(const auto& w:windows){peaks.push_back(w.peakDb);rms.push_back(w.rmsDb);}
    const float loud=percentile(rms,.90f),quiet=percentile(rms,.10f);
    r.playingDetected=windows.size()>=50 && loud>minimumPlayingDb;
    if(!r.playingDetected)return r;
    // Windows within 12 dB of the loud level count as playing.
    std::vector<float> playingPeaks,quietPeaks;
    for(const auto& w:windows) {
        if(w.rmsDb>=loud-12.f)playingPeaks.push_back(w.peakDb);
        else if(w.rmsDb<=quiet+3.f)quietPeaks.push_back(w.peakDb);
    }
    r.playingPeakDb=percentile(playingPeaks,.99f);
    const float shift=std::clamp(targetPeakDb-r.playingPeakDb,-12.f,12.f);
    // Ignore small deviations so repeated runs do not chase noise.
    const float proposed=std::abs(shift)>=2.f?currentInputDb+shift:currentInputDb;
    r.inputGainDb=std::round(std::clamp(proposed,-24.f,24.f)*2.f)*.5f;
    const float delta=r.inputGainDb-currentInputDb;
    r.noisePeakDb=percentile(quietPeaks,.90f)+delta;
    r.playingPeakDb+=delta;
    const float playingFloor=percentile(playingPeaks,.10f)+delta;
    r.gapDb=playingFloor-r.noisePeakDb;
    r.pausesDetected=quietPeaks.size()>=10 && loud-quiet>=12.f;
    if(!r.pausesDetected)return r;
    // Open above the noise including the gate's 6 dB close hysteresis plus
    // 3 dB margin, but stay below the quietest playing level.
    float threshold=r.noisePeakDb+9.f;
    if(threshold>playingFloor-6.f)threshold=.5f*(r.noisePeakDb+playingFloor);
    r.gateThresholdDb=std::round(std::clamp(threshold,-90.f,-15.f));
    r.gateRangeDb=r.gapDb>=30.f?96.f:std::clamp(std::round(r.gapDb-6.f),12.f,60.f);
    return r;
}

struct CompressorResult {
    bool playingDetected{};
    float thresholdDb{-24},ratio{4},attackMs{10},releaseMs{120},kneeDb{6},makeupDb{};
    float crestDb{},dynamicRangeDb{};
};

// windows: measured at the clean compressor input (after Tone EQ).
inline CompressorResult analyseCompressor(const std::vector<Window>& windows) {
    CompressorResult r;
    std::vector<float> rms;for(const auto& w:windows)rms.push_back(w.rmsDb);
    const float loud=percentile(rms,.90f);
    r.playingDetected=windows.size()>=50 && loud>minimumPlayingDb;
    if(!r.playingDetected)return r;
    std::vector<float> playingRms,playingPeaks;
    for(const auto& w:windows)if(w.rmsDb>=loud-20.f){playingRms.push_back(w.rmsDb);playingPeaks.push_back(w.peakDb);}
    const float median=percentile(playingRms,.50f);
    r.dynamicRangeDb=percentile(playingRms,.90f)-percentile(playingRms,.20f);
    r.crestDb=percentile(playingPeaks,.99f)-median;
    // Compress the louder half of the playing; wider dynamics get more ratio.
    r.thresholdDb=std::round(std::clamp(median,-60.f,0.f));
    r.ratio=r.dynamicRangeDb<6.f?2.f:r.dynamicRangeDb<12.f?3.f:4.f;
    // Strong pick transients keep a slower attack so the note front survives.
    r.attackMs=r.crestDb>16.f?15.f:r.crestDb>10.f?8.f:3.f;
    double reduction=0;
    for(const float level:playingRms)reduction+=std::max(0.f,level-r.thresholdDb)*(1.f-1.f/r.ratio);
    const float average=float(reduction/double(playingRms.size()));
    r.makeupDb=std::round(std::clamp(average*.75f,0.f,12.f)*2.f)*.5f;
    return r;
}

}

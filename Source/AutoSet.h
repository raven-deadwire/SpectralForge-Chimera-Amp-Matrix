#pragma once
#include "GraphicalEQ.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Auto Set listens to the player for a few seconds and proposes input gain,
// gate, Tone EQ and clean-compressor settings from rule-based measurements.
// The audio thread only reduces or copies samples; all decisions run elsewhere.

struct Window {float peakDb{-120.f},rmsDb{-120.f};};
// input/compressor are reduced to 10 ms windows; tone keeps mono samples for
// spectral analysis of the amp/cab output before the Tone EQ.
enum class Tap {input=0,compressor=1,tone=2};

inline float toDb(double linear) noexcept {return float(20.0*std::log10(std::max(1e-6,linear)));}

// Single producer (audio thread) / single consumer (message thread) queue
// per tap. Allocation-free after construction; overflow drops data.
class Capture {
public:
    static constexpr int capacity=4096;
    static constexpr uint32_t toneCapacity=1u<<17; // 0.68 s at 192 kHz between drains
    void prepare(double sampleRate) noexcept {
        windowSamples=std::max(1,int(sampleRate*.010));
        for(auto& t:taps){t.count=0;t.peak=0;t.energy=0;}
    }
    void listen(std::optional<Tap> tap) noexcept {active.store(tap?int(*tap):-1,std::memory_order_release);}
    bool listening(Tap tap) const noexcept {return active.load(std::memory_order_acquire)==int(tap);}
    // Audio thread. Mirrors the clean compressor detector: max of the channels.
    void push(Tap tap,const float* const* channels,int channelCount,int samples) noexcept {
        if(!listening(tap) || channelCount<=0)return;
        if(tap==Tap::tone){pushTone(channels,channelCount,samples);return;}
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
        if(tap==Tap::tone)return;
        auto& t=taps[size_t(tap)];
        int r=t.read.load(std::memory_order_relaxed);
        const int w=t.write.load(std::memory_order_acquire);
        while(r!=w){out.push_back(t.ring[size_t(r)]);r=(r+1)%capacity;}
        t.read.store(r,std::memory_order_release);
    }
    void drainTone(std::vector<float>& out) {
        const auto r=toneRead.load(std::memory_order_relaxed),w=toneWrite.load(std::memory_order_acquire);
        for(auto i=r;i!=w;++i)out.push_back(toneRing[i%toneCapacity]);
        toneRead.store(w,std::memory_order_release);
    }
private:
    void pushTone(const float* const* channels,int channelCount,int samples) noexcept {
        const int used=std::min(2,channelCount);
        auto w=toneWrite.load(std::memory_order_relaxed);
        for(int n=0;n<samples;++n) {
            if(w-toneRead.load(std::memory_order_acquire)>=toneCapacity)break;
            float x=0;for(int c=0;c<used;++c)x+=channels[c][n];
            x/=float(used);toneRing[w%toneCapacity]=std::isfinite(x)?x:0.f;++w;
        }
        toneWrite.store(w,std::memory_order_release);
    }
    struct TapState {
        std::array<Window,capacity> ring{};std::atomic<int> write{0},read{0};
        int count{};double peak{},energy{};
    };
    std::array<TapState,2> taps;
    std::array<float,toneCapacity> toneRing{};
    std::atomic<uint32_t> toneWrite{0},toneRead{0}; // unsigned difference works across wrap
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

// Tone EQ (phase 2). The long-term spectrum of the amp/cab output (before the
// Tone EQ) is compared, in third-octave bands, with a target curve for the
// detected instrument; a few broad moves cover part of the difference.
inline constexpr std::array<float,24> toneBandCentres{50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000};
inline constexpr int toneBandCount=int(toneBandCentres.size());
struct ToneMove {int type{};float frequency{},gainDb{},q{};};
// Low shelf, five bells an octave apart, high shelf. Broad on purpose: Auto
// Set corrects balance, it does not chase single resonances.
inline constexpr std::array<ToneMove,7> toneLayout{{{1,80,0,.707f},{0,160,0,1},{0,315,0,1},{0,630,0,1},{0,1250,0,1},{0,2500,0,1},{2,5000,0,.707f}}};
inline constexpr float toneStrength=.7f,toneMaxMoveDb=6.f,toneMinMoveDb=1.f;

// Starting target shapes (third-octave level relative to 250 Hz-1 kHz) for a
// mic'd cabinet. Not yet tuned against reference recordings.
inline float toneTargetDb(bool bass,float frequency) {
    static constexpr std::array<std::pair<float,float>,11> guitar{{{50,-16},{63,-12},{100,-5},{160,-1},{250,0},{1000,-1},{2000,-3},{3150,-5},{5000,-10},{8000,-18},{12000,-26}}};
    static constexpr std::array<std::pair<float,float>,10> bassCurve{{{40,-4},{63,0},{100,0},{160,-1},{250,-3},{500,-6},{1000,-10},{2000,-15},{4000,-22},{8000,-32}}};
    const auto interpolate=[frequency](const auto& curve) {
        if(frequency<=curve.front().first)return curve.front().second;
        for(size_t i=1;i<curve.size();++i)if(frequency<=curve[i].first) {
            const float t=std::log(frequency/curve[i-1].first)/std::log(curve[i].first/curve[i-1].first);
            return curve[i-1].second+t*(curve[i].second-curve[i-1].second);
        }
        return curve.back().second;
    };
    return bass?interpolate(bassCurve):interpolate(guitar);
}

struct ToneResult {
    bool playingDetected{},bass{};
    std::array<float,toneBandCount> measuredDb{},targetDb{},weight{},correctionDb{},responseDb{};
    std::array<ToneMove,7> moves{toneLayout};
    float errorBeforeDb{},errorAfterDb{};
    int activeMoves() const noexcept {int n=0;for(const auto& m:moves)n+=m.gainDb!=0.f;return n;}
};

inline float toneResponseDb(const std::array<ToneMove,7>& moves,float frequency,double rate) {
    double db=0;
    for(const auto& m:moves)if(m.gainDb!=0.f)db+=20*std::log10(std::max(1e-9,eqCoefficients(m.type,m.frequency,m.gainDb,m.q,rate).magnitude(frequency,rate)));
    return float(db);
}

// samples: mono amp/cab output before the Tone EQ, at the current input gain.
// bass: the instrument when the player chose it; detected otherwise.
inline ToneResult analyseTone(const std::vector<float>& samples,double rate,std::optional<bool> bass={}) {
    ToneResult r;
    if(rate<8000)return r;
    const int order=std::clamp(int(std::round(std::log2(rate/6.))),12,15),size=1<<order,hop=size/2; // ~6 Hz bins
    juce::dsp::FFT fft(order);
    std::vector<float> window(static_cast<size_t>(size)),frame(static_cast<size_t>(2*size));
    for(int n=0;n<size;++n)window[size_t(n)]=float(.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/size));
    std::array<std::pair<int,int>,toneBandCount> bins{};
    for(int b=0;b<toneBandCount;++b) {
        const double edge=std::pow(2.,1./6.),fc=toneBandCentres[size_t(b)];
        bins[size_t(b)]={std::max(1,int(std::ceil(fc/edge*size/rate))),std::min(size/2,int(std::ceil(fc*edge*size/rate)))};
    }
    // Bass detection compares 35-71 Hz (below a guitar's low E, where guitar
    // cabinets also roll off) with the 71 Hz-1.4 kHz body.
    const int lowestStart=std::max(1,int(35.*size/rate)),bodyStart=int(71.*size/rate),bodyEnd=int(1400.*size/rate);
    struct Frame {float levelDb;std::array<double,toneBandCount> power;double lowest,body;};
    std::vector<Frame> frames;
    for(size_t start=0;start+size_t(size)<=samples.size();start+=size_t(hop)) {
        double energy=0;
        for(int n=0;n<size;++n){const float x=samples[start+size_t(n)];energy+=double(x)*x;frame[size_t(n)]=x*window[size_t(n)];}
        std::fill(frame.begin()+size,frame.end(),0.f);
        fft.performFrequencyOnlyForwardTransform(frame.data(),true);
        Frame f{toDb(std::sqrt(energy/size)),{},0,0};
        for(int k=lowestStart;k<bodyEnd;++k)(k<bodyStart?f.lowest:f.body)+=double(frame[size_t(k)])*frame[size_t(k)];
        for(int b=0;b<toneBandCount;++b)for(int k=bins[size_t(b)].first;k<bins[size_t(b)].second;++k)f.power[size_t(b)]+=double(frame[size_t(k)])*frame[size_t(k)];
        frames.push_back(f);
    }
    std::vector<float> levels;for(const auto& f:frames)levels.push_back(f.levelDb);
    const float loud=percentile(levels,.90f);
    std::array<double,toneBandCount> power{};int used=0;double lowest=0,body=0;
    for(const auto& f:frames)if(f.levelDb>=loud-15.f && f.levelDb>minimumPlayingDb) {
        ++used;lowest+=f.lowest;body+=f.body;
        for(int b=0;b<toneBandCount;++b)power[size_t(b)]+=f.power[size_t(b)];
    }
    r.playingDetected=used>=8;
    if(!r.playingDetected)return r;
    float strongest=-300;
    for(int b=0;b<toneBandCount;++b){r.measuredDb[size_t(b)]=float(10*std::log10(std::max(1e-30,power[size_t(b)]/used)));strongest=std::max(strongest,r.measuredDb[size_t(b)]);}
    r.bass=bass.value_or(10*std::log10(std::max(1e-30,lowest)/std::max(1e-30,body))>-12);
    // Align target and measurement over 250 Hz-1 kHz, the body of both curves.
    double offset=0;int reference=0;
    for(int b=0;b<toneBandCount;++b) {
        const float fc=toneBandCentres[size_t(b)];r.targetDb[size_t(b)]=toneTargetDb(r.bass,fc);
        if(fc>=250 && fc<=1000){offset+=r.measuredDb[size_t(b)]-r.targetDb[size_t(b)];++reference;}
    }
    offset/=reference;
    // Bands with no real content (cab roll-off, below the lowest note: far
    // under the strongest band or 20 dB short of the target) are left out:
    // there is nothing there to bring up.
    std::array<float,toneBandCount> deviation{};
    for(int b=0;b<toneBandCount;++b) {
        deviation[size_t(b)]=r.targetDb[size_t(b)]+float(offset)-r.measuredDb[size_t(b)];
        r.weight[size_t(b)]=r.measuredDb[size_t(b)]>strongest-45.f && deviation[size_t(b)]<20.f?1.f:0.f;
    }
    // One-octave smoothing (1-2-1 over third-octave neighbours) so the fit
    // follows the balance, not individual notes or cabinet resonances.
    double before=0,weights=0;
    for(int b=0;b<toneBandCount;++b) {
        double sum=0,count=0;
        for(int k=std::max(0,b-1);k<=std::min(toneBandCount-1,b+1);++k){const double w=(k==b?2.:1.)*r.weight[size_t(k)];sum+=w*deviation[size_t(k)];count+=w;}
        const float w=r.weight[size_t(b)];
        r.correctionDb[size_t(b)]=count>0?std::clamp(toneStrength*float(sum/count),-9.f,9.f):0.f;
        before+=w*r.correctionDb[size_t(b)]*r.correctionDb[size_t(b)];weights+=w;
    }
    // Each move answers the bands nearest to it; iterate because neighbours overlap.
    const auto region=[](const ToneMove& m,float fc){return m.type==1?fc<=100.f:m.type==2?fc>=5000.f:std::abs(std::log2(fc/m.frequency))<=.5f;};
    auto& moves=r.moves;
    for(int iteration=0;iteration<60;++iteration) {
        for(int b=0;b<toneBandCount;++b)r.responseDb[size_t(b)]=toneResponseDb(moves,toneBandCentres[size_t(b)],rate);
        for(auto& m:moves) {
            double error=0,count=0;
            for(int b=0;b<toneBandCount;++b)if(region(m,toneBandCentres[size_t(b)])){const float w=r.weight[size_t(b)];error+=w*(r.correctionDb[size_t(b)]-r.responseDb[size_t(b)]);count+=w;}
            if(count>0)m.gainDb=std::clamp(m.gainDb+float(.5*error/count),-toneMaxMoveDb,toneMaxMoveDb);
        }
    }
    for(auto& m:moves){m.gainDb=std::round(m.gainDb*2.f)*.5f;if(std::abs(m.gainDb)<toneMinMoveDb)m.gainDb=0;}
    double after=0;
    for(int b=0;b<toneBandCount;++b) {
        r.responseDb[size_t(b)]=toneResponseDb(moves,toneBandCentres[size_t(b)],rate);
        const float e=r.correctionDb[size_t(b)]-r.responseDb[size_t(b)];after+=r.weight[size_t(b)]*e*e;
    }
    r.errorBeforeDb=float(std::sqrt(before/std::max(1.,weights)));r.errorAfterDb=float(std::sqrt(after/std::max(1.,weights)));
    return r;
}

}

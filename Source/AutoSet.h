#pragma once
#include "GraphicalEQ.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <initializer_list>
#include <cmath>
#include <optional>
#include <vector>

namespace spectralforge::autoset {

// Auto Set listens to the player for a few seconds and proposes input gain,
// gate, Tone EQ and clean-compressor settings from rule-based measurements.
// The audio thread only reduces or copies samples; all decisions run elsewhere.

struct Window {float peakDb{-120.f},rmsDb{-120.f};};
// input/compressor/output/rig are reduced to 10 ms windows; tone, multiband,
// deesser and master keep mono samples for spectral analysis (amp/cab output
// before the Tone EQ, the multiband input, the de-esser input, the limiter
// input). The sample taps share one ring.
// rig: the amp/cab sum before any post-rig gate. master: the same point as
// output (after the Final EQ and output trim), as samples.
enum class Tap {input=0,compressor=1,output=2,tone=3,deesser=4,multiband=5,rig=6,master=7};
inline constexpr bool sampleTap(Tap tap) noexcept {return tap==Tap::tone || tap==Tap::deesser || tap==Tap::multiband || tap==Tap::master;}

inline float toDb(double linear) noexcept {return float(20.0*std::log10(std::max(1e-6,linear)));}

// Single producer (audio thread) / single consumer (message thread) queue
// per tap. Allocation-free after construction; overflow drops data.
class Capture {
public:
    static constexpr int capacity=4096;
    static constexpr uint32_t sampleCapacity=1u<<17; // 0.68 s at 192 kHz between drains
    void prepare(double sampleRate) noexcept {
        windowSamples=std::max(1,int(sampleRate*.010));
        for(auto& t:taps){t.count=0;t.peak=0;t.energy=0;}
    }
    // At most one sample tap at a time; window taps can listen alongside.
    void listen(std::initializer_list<Tap> listened) noexcept {
        int mask=0;for(const auto tap:listened)mask|=1<<int(tap);
        active.store(mask,std::memory_order_release);
    }
    void listen(std::optional<Tap> tap) noexcept {active.store(tap?1<<int(*tap):0,std::memory_order_release);}
    bool listening(Tap tap) const noexcept {return (active.load(std::memory_order_acquire)>>int(tap))&1;}
    // Audio thread. Mirrors the clean compressor detector: max of the channels.
    void push(Tap tap,const float* const* channels,int channelCount,int samples) noexcept {
        if(!listening(tap) || channelCount<=0)return;
        if(sampleTap(tap)){pushSamples(channels,channelCount,samples);return;}
        auto& t=taps[windowIndex(tap)];
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
        if(sampleTap(tap))return;
        auto& t=taps[windowIndex(tap)];
        int r=t.read.load(std::memory_order_relaxed);
        const int w=t.write.load(std::memory_order_acquire);
        while(r!=w){out.push_back(t.ring[size_t(r)]);r=(r+1)%capacity;}
        t.read.store(r,std::memory_order_release);
    }
    void drainSamples(std::vector<float>& out) {
        const auto r=sampleRead.load(std::memory_order_relaxed),w=sampleWrite.load(std::memory_order_acquire);
        for(auto i=r;i!=w;++i)out.push_back(sampleRing[i%sampleCapacity]);
        sampleRead.store(w,std::memory_order_release);
    }
private:
    static size_t windowIndex(Tap tap) noexcept {return tap==Tap::rig?3:size_t(tap);}
    void pushSamples(const float* const* channels,int channelCount,int samples) noexcept {
        const int used=std::min(2,channelCount);
        auto w=sampleWrite.load(std::memory_order_relaxed);
        for(int n=0;n<samples;++n) {
            if(w-sampleRead.load(std::memory_order_acquire)>=sampleCapacity)break;
            float x=0;for(int c=0;c<used;++c)x+=channels[c][n];
            x/=float(used);sampleRing[w%sampleCapacity]=std::isfinite(x)?x:0.f;++w;
        }
        sampleWrite.store(w,std::memory_order_release);
    }
    struct TapState {
        std::array<Window,capacity> ring{};std::atomic<int> write{0},read{0};
        int count{};double peak{},energy{};
    };
    std::array<TapState,4> taps;
    std::array<float,sampleCapacity> sampleRing{};
    std::atomic<uint32_t> sampleWrite{0},sampleRead{0}; // unsigned difference works across wrap
    std::atomic<int> active{0};
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

// Style shifts the instrument target (and the de-esser and limiter, below).
enum class Style {rock=0,clean=1,metal=2};
inline const char* styleName(Style style) noexcept {return style==Style::clean?"clean":style==Style::metal?"metal":"rock";}

// Phase 5: Gate 2 and the Expander from the noise the rig adds. Measured in the
// input pass with every gate held open: the input windows say when the player
// pauses, the rig windows (amp/cab sum, before post-rig gates) how loud the
// amp's hiss is then and how quiet the playing gets.
inline constexpr float audibleRigHissDb=-66.f;
struct GateStagesResult {
    enum class Plan {none,soft,hard};
    bool measured{};Plan plan{Plan::none};
    float rigNoiseDb{-120.f},rigPlayingFloorDb{-120.f},rigGapDb{};
    // soft: Expander at the main threshold, main gate deeper. hard: post-rig main gate, gentle PRE Gate 2.
    bool mainAfterRig{};float mainThresholdDb{};
    bool expanderAfterRig{};float expanderThresholdDb{},expanderRatio{2},expanderRangeDb{},expanderReleaseMs{};
    float gate2ThresholdDb{},gate2RangeDb{};
};
// rigLagWindows: plug-in latency between the two taps, in 10 ms windows.
inline GateStagesResult analyseGateStages(const std::vector<Window>& input,const std::vector<Window>& rig,int rigLagWindows,
                                          float gateThresholdDb,bool gateAfterRig,Style style) {
    GateStagesResult r;r.mainThresholdDb=gateThresholdDb;r.mainAfterRig=gateAfterRig;
    std::vector<float> rms;for(const auto& w:input)rms.push_back(w.rmsDb);
    if(rms.size()<50)return r;
    const float loud=percentile(rms,.90f),quiet=percentile(rms,.10f);
    if(loud<=minimumPlayingDb || loud-quiet<12.f)return r; // needs playing and a pause, like the gate
    std::vector<float> noise,playing;
    for(size_t i=0;i<input.size();++i) {
        const size_t j=i+size_t(std::max(0,rigLagWindows));if(j>=rig.size())break;
        // Skip windows next to a level change so the latency guess does not matter.
        const auto steady=[&](auto test){for(size_t k=i>2?i-2:0;k<std::min(input.size(),i+3);++k)if(!test(input[k].rmsDb))return false;return true;};
        if(steady([&](float x){return x<=quiet+3.f;}))noise.push_back(rig[j].peakDb);
        else if(steady([&](float x){return x>=loud-12.f;}))playing.push_back(rig[j].peakDb);
    }
    if(noise.size()<10 || playing.size()<10)return r;
    r.measured=true;
    r.rigNoiseDb=percentile(noise,.90f);r.rigPlayingFloorDb=percentile(playing,.10f);r.rigGapDb=r.rigPlayingFloorDb-r.rigNoiseDb;
    if(r.rigNoiseDb<=audibleRigHissDb && style!=Style::metal)return r; // the amp adds little hiss
    if(style==Style::metal && r.rigGapDb>=24.f) {
        // Tight: hard main gate after the rig cuts the amp's hiss; a gentle
        // Gate 2 before it keeps the hiss from feeding the amp.
        r.plan=GateStagesResult::Plan::hard;r.mainAfterRig=true;
        r.gate2ThresholdDb=std::max(-90.f,gateThresholdDb-6.f);
        r.gate2RangeDb=std::clamp(std::round(.5f*r.rigGapDb),9.f,18.f);
        return r;
    }
    // Smooth: the Expander takes the gate threshold so tails fade instead of
    // chopping; the main gate closes only once the Expander's range is used.
    r.plan=GateStagesResult::Plan::soft;
    r.expanderAfterRig=r.rigNoiseDb>audibleRigHissDb || gateAfterRig;
    r.expanderRatio=style==Style::clean?1.5f:style==Style::metal?2.5f:2.f;
    // Enough range to take the hiss toward -78 dBFS; the main gate removes the rest.
    r.expanderRangeDb=std::clamp(std::round(r.rigNoiseDb+78.f),6.f,15.f);
    r.expanderThresholdDb=gateThresholdDb;
    r.expanderReleaseMs=style==Style::clean?200.f:style==Style::metal?80.f:120.f;
    const float deeper=std::clamp(r.expanderRangeDb/(r.expanderRatio-1.f)+3.f,9.f,24.f);
    r.mainThresholdDb=std::max(-90.f,gateThresholdDb-deeper);
    return r;
}

template<size_t N> float interpolateCurve(const std::array<std::pair<float,float>,N>& curve,float frequency) {
    if(frequency<=curve.front().first)return curve.front().second;
    for(size_t i=1;i<N;++i)if(frequency<=curve[i].first) {
        const float t=std::log(frequency/curve[i-1].first)/std::log(curve[i].first/curve[i-1].first);
        return curve[i-1].second+t*(curve[i].second-curve[i-1].second);
    }
    return curve.back().second;
}

// Starting target shapes (third-octave level relative to 250 Hz-1 kHz) for a
// mic'd cabinet, plus style offsets. Not yet tuned against reference recordings.
inline float toneTargetDb(bool bass,float frequency,Style style=Style::rock) {
    static constexpr std::array<std::pair<float,float>,11> guitar{{{50,-16},{63,-12},{100,-5},{160,-1},{250,0},{1000,-1},{2000,-3},{3150,-5},{5000,-10},{8000,-18},{12000,-26}}};
    static constexpr std::array<std::pair<float,float>,10> bassCurve{{{40,-4},{63,0},{100,0},{160,-1},{250,-3},{500,-6},{1000,-10},{2000,-15},{4000,-22},{8000,-32}}};
    // Clean: more air. Metal: tighter lows, slight mid scoop, presence up, fizz down.
    static constexpr std::array<std::pair<float,float>,6> clean{{{50,0},{1000,0},{2500,1},{5000,2},{8000,2},{12000,2}}};
    static constexpr std::array<std::pair<float,float>,11> metalGuitar{{{50,-3},{80,-3},{125,0},{315,0},{500,-2},{800,-2},{1250,0},{2500,1.5f},{4000,1.5f},{6300,-1},{8000,-3}}};
    static constexpr std::array<std::pair<float,float>,6> metalBass{{{50,-2},{80,0},{500,-2},{1000,0},{2000,2},{4000,2}}};
    const float base=bass?interpolateCurve(bassCurve,frequency):interpolateCurve(guitar,frequency);
    if(style==Style::clean)return base+interpolateCurve(clean,frequency);
    if(style==Style::metal)return base+(bass?interpolateCurve(metalBass,frequency):interpolateCurve(metalGuitar,frequency));
    return base;
}

// Long-term third-octave spectrum of the playing (frames within 15 dB of the
// 90th-percentile frame level), shared by the Tone EQ and de-esser analyses.
struct Spectrum {
    bool playing{},bass{};
    std::array<float,toneBandCount> bandsDb{};
    float strongestDb{-300};
};
inline Spectrum measureSpectrum(const std::vector<float>& samples,double rate,std::optional<bool> bass={},
                                const std::vector<bool>* excludedWindows=nullptr) {
    Spectrum r;
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
        // Finish analysis can exclude note attacks without concatenating audio
        // (which would introduce new FFT discontinuities). Tone analysis is unchanged.
        if(excludedWindows) {
            const auto windowSize=size_t(std::max(1,int(rate*.010)));
            bool excluded=false;
            for(size_t i=start/windowSize;i<=(start+size_t(size)-1)/windowSize;++i)
                if(i>=excludedWindows->size() || (*excludedWindows)[i]){excluded=true;break;}
            if(excluded)continue;
        }
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
    r.playing=used>=8;
    if(!r.playing)return r;
    for(int b=0;b<toneBandCount;++b){r.bandsDb[size_t(b)]=float(10*std::log10(std::max(1e-30,power[size_t(b)]/used)));r.strongestDb=std::max(r.strongestDb,r.bandsDb[size_t(b)]);}
    r.bass=bass.value_or(10*std::log10(std::max(1e-30,lowest)/std::max(1e-30,body))>-12);
    return r;
}
// Target aligned to a measurement over 250 Hz-1 kHz, the body of both curves.
inline std::array<float,toneBandCount> alignedTarget(const Spectrum& measured,Style style) {
    std::array<float,toneBandCount> target{};double offset=0;int reference=0;
    for(int b=0;b<toneBandCount;++b) {
        const float fc=toneBandCentres[size_t(b)];target[size_t(b)]=toneTargetDb(measured.bass,fc,style);
        if(fc>=250 && fc<=1000){offset+=measured.bandsDb[size_t(b)]-target[size_t(b)];++reference;}
    }
    for(auto& t:target)t+=float(offset/reference);
    return target;
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
inline ToneResult analyseTone(const std::vector<float>& samples,double rate,std::optional<bool> bass={},Style style=Style::rock) {
    ToneResult r;
    const auto spectrum=measureSpectrum(samples,rate,bass);
    r.playingDetected=spectrum.playing;
    if(!r.playingDetected)return r;
    r.bass=spectrum.bass;r.measuredDb=spectrum.bandsDb;r.targetDb=alignedTarget(spectrum,style);
    const float strongest=spectrum.strongestDb;
    // Bands with no real content (cab roll-off, below the lowest note: far
    // under the strongest band or 20 dB short of the target) are left out:
    // there is nothing there to bring up.
    std::array<float,toneBandCount> deviation{};
    for(int b=0;b<toneBandCount;++b) {
        deviation[size_t(b)]=r.targetDb[size_t(b)]-r.measuredDb[size_t(b)];
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

// Finish (phase 3): de-esser from the de-esser input, limiter from the
// limiter input (after the output trim).
struct FinishResult {
    bool playingDetected{},bass{},deEsser{};
    float frequency{6000},thresholdDb{-24},rangeDb{6},attackMs{1},releaseMs{80},ratio{4},kneeDb{6};
    float spectralExcessDb{},burstExcessDb{},overallSpectralExcessDb{};
    bool sustainedEvidence{};
    int protectedAttackWindows{};
    bool limiterPlaying{};
    float ceilingDb{-1},limiterReleaseMs{100},outputPeakDb{-120},overCeilingPercent{};
};
inline constexpr float limiterCeilingDb=-1.f;

// A post-rig low-frequency ratio cannot identify an instrument unambiguously.
// In auto mode, spectral de-essing must exceed BOTH plausible targets. Explicit
// Guitar/Bass keeps its original style curve; Tone EQ detection is unchanged.
inline std::array<float,toneBandCount> finishTarget(const Spectrum& measured,Style style,bool instrumentChosen) {
    auto target=alignedTarget(measured,style);
    if(!instrumentChosen) {
        auto alternative=measured;alternative.bass=!measured.bass;
        const auto other=alignedTarget(alternative,style);
        for(size_t b=0;b<target.size();++b)target[b]=std::max(target[b],other[b]);
    }
    return target;
}

// samples: mono de-esser input. output: 10 ms windows at the limiter input.
inline FinishResult analyseFinish(const std::vector<float>& samples,const std::vector<Window>& output,double rate,std::optional<bool> bass={},Style style=Style::rock) {
    FinishResult r;
    // Limiter: sample-peak safety at -1 dBFS; release follows the style.
    r.limiterReleaseMs=style==Style::clean?150.f:style==Style::metal?60.f:100.f;
    std::vector<float> peaks,rms;for(const auto& w:output){peaks.push_back(w.peakDb);rms.push_back(w.rmsDb);}
    r.limiterPlaying=output.size()>=50 && percentile(rms,.90f)>minimumPlayingDb;
    if(r.limiterPlaying) {
        r.outputPeakDb=percentile(peaks,.999f);
        int over=0;for(const float p:peaks)over+=p>r.ceilingDb;
        r.overCeilingPercent=100.f*float(over)/float(peaks.size());
    }
    // De-esser: harsh when the top end sits above the style target, or when it
    // bursts (pick click, fizz) much more than the whole signal does.
    const auto spectrum=measureSpectrum(samples,rate,bass);
    r.playingDetected=spectrum.playing;r.bass=spectrum.bass;
    if(!r.playingDetected)return r;
    const auto target=finishTarget(spectrum,style,bass.has_value());
    int worst=-1;float worstExcess=-100;
    for(int b=0;b<toneBandCount;++b){const float fc=toneBandCentres[size_t(b)];if(fc>=2500 && fc<=8000 && fc<rate*.4){const float e=spectrum.bandsDb[size_t(b)]-target[size_t(b)];if(e>worstExcess){worstExcess=e;worst=b;}}}
    if(worst<0)return r;
    // Split at the lower edge of the most excessive band.
    r.frequency=std::clamp(std::round(toneBandCentres[size_t(worst)]/std::pow(2.f,1.f/6.f)/100.f)*100.f,2500.f,9000.f);
    double excess=0;int count=0;
    for(int b=0;b<toneBandCount;++b){const float fc=toneBandCentres[size_t(b)];if(fc>=r.frequency && fc<=10000 && fc<rate*.4){excess+=spectrum.bandsDb[size_t(b)]-target[size_t(b)];++count;}}
    r.overallSpectralExcessDb=count?float(excess/count):0.f;
    // High band through two 12 dB/oct high-passes (the de-esser splits at 24 dB/oct).
    const auto hp=eqCoefficients(3,r.frequency,0,.70710678,rate);
    // A note onset raises the body as well as the full band. HF-only clicks
    // over a sustained note must not acquire this protection.
    const auto lp=eqCoefficients(4,1500,0,.70710678,rate);
    std::array<double,4> z{};const int windowSamples=std::max(1,int(rate*.010));
    std::array<double,4> bodyZ{};
    std::vector<float> fullRms,bodyRms,highRms,highPeak;double fullEnergy=0,bodyEnergy=0,highEnergy=0,peak=0;int n=0;
    for(const float x:samples) {
        double y=x;
        for(int stage=0;stage<2;++stage){double& z1=z[size_t(2*stage)];double& z2=z[size_t(2*stage+1)];const double out=hp.b0*y+z1;z1=hp.b1*y-hp.a1*out+z2;z2=hp.b2*y-hp.a2*out;y=out;}
        double body=x;
        for(int stage=0;stage<2;++stage){double& z1=bodyZ[size_t(2*stage)];double& z2=bodyZ[size_t(2*stage+1)];const double out=lp.b0*body+z1;z1=lp.b1*body-lp.a1*out+z2;z2=lp.b2*body-lp.a2*out;body=out;}
        fullEnergy+=double(x)*x;highEnergy+=y*y;peak=std::max(peak,std::abs(y));
        bodyEnergy+=body*body;
        if(++n==windowSamples){fullRms.push_back(toDb(std::sqrt(fullEnergy/n)));bodyRms.push_back(toDb(std::sqrt(bodyEnergy/n)));highRms.push_back(toDb(std::sqrt(highEnergy/n)));highPeak.push_back(toDb(peak));fullEnergy=bodyEnergy=highEnergy=peak=0;n=0;}
    }
    // Transients: each window against the median of its +-50 ms neighbours, so
    // a note's ordinary decay does not count, a pick click or fizz spike does.
    const auto spikes=[](const std::vector<float>& level,size_t i) {
        std::vector<float> around;
        for(size_t k=i>=5?i-5:0;k<=std::min(level.size()-1,i+5);++k)if(k!=i)around.push_back(level[k]);
        return level[i]-percentile(around,.5f);
    };
    const float loud=percentile(fullRms,.90f);
    std::vector<bool> attack(fullRms.size(),false);
    for(size_t i=0;i<fullRms.size();++i) {
        std::vector<float> previousFull,previousBody;
        for(size_t k=i>=5?i-5:0;k<i;++k){previousFull.push_back(fullRms[k]);previousBody.push_back(bodyRms[k]);}
        if(i==0 || (fullRms[i]>std::max(minimumPlayingDb,loud-15.f)
                    && fullRms[i]-percentile(previousFull,.5f)>=3.f
                    && bodyRms[i]-percentile(previousBody,.5f)>=6.f))
            for(size_t k=i>0?i-1:0;k<std::min(attack.size(),i+6);++k)attack[k]=true;
    }
    r.protectedAttackWindows=int(std::count(attack.begin(),attack.end(),true));
    // Require actual post-attack frames for the spectral route. If a rapid
    // muted passage leaves no evidence, abstain rather than infer harshness.
    const auto sustained=measureSpectrum(samples,rate,spectrum.bass,&attack);
    r.sustainedEvidence=sustained.playing;
    if(r.sustainedEvidence) {
        const auto sustainedTarget=finishTarget(sustained,style,bass.has_value());double sum=0;int bands=0;
        for(int b=0;b<toneBandCount;++b){const float fc=toneBandCentres[size_t(b)];if(fc>=r.frequency && fc<=10000 && fc<rate*.4){sum+=sustained.bandsDb[size_t(b)]-sustainedTarget[size_t(b)];++bands;}}
        r.spectralExcessDb=bands?float(sum/bands):0.f;
    }
    std::vector<float> burstExcess,playingPeak;
    for(size_t i=0;i<fullRms.size();++i)if(fullRms[i]>=loud-15.f && fullRms[i]>minimumPlayingDb) {
        playingPeak.push_back(highPeak[i]);
        if(!attack[i] && highRms[i]>loud-45.f)
            burstExcess.push_back(spikes(highRms,i)-std::max(0.f,spikes(fullRms,i)));
    }
    if(playingPeak.size()<50)return r;
    // Pair the two rises in time BEFORE taking a percentile. Subtracting two
    // independent percentiles can compare entirely different musical events.
    r.burstExcessDb=burstExcess.size()>=20?std::max(0.f,percentile(burstExcess,.95f)):0.f;
    const float spectralLimit=style==Style::clean?3.f:style==Style::metal?1.5f:2.f;
    r.deEsser=r.spectralExcessDb>=spectralLimit || r.burstExcessDb>=4.f;
    if(!r.deEsser)return r;
    // Act on the loudest fifth of the high band; deeper cuts for harsher input.
    r.thresholdDb=std::round(std::clamp(percentile(playingPeak,.80f),-60.f,-6.f));
    r.rangeDb=std::clamp(std::round(std::max(r.spectralExcessDb,r.burstExcessDb-2.f)+2.f),3.f,9.f);
    r.releaseMs=style==Style::clean?120.f:style==Style::metal?50.f:80.f;
    return r;
}

// Multiband (phase 4): compress only the bands that misbehave relative to the
// rest of the signal, at the multiband input (after the clean compressor).
struct MultibandBand {bool active{};float thresholdDb{-24},ratio{2},rangeDb{6},attackMs{10},releaseMs{120},kneeDb{6};float swingDb{},excessDb{};};
struct MultibandResult {
    bool playingDetected{},bass{};
    std::array<float,3> crossovers{};
    std::array<MultibandBand,4> bands{};
    int activeBands() const noexcept {int n=0;for(const auto& b:bands)n+=b.active;return n;}
};
// Crossovers per instrument and style: boom / body / mids / presence and fizz.
inline std::array<float,3> multibandCrossovers(bool bass,Style style) noexcept {
    if(bass)return {80,300,2000};
    if(style==Style::metal)return {120,800,4500};
    if(style==Style::clean)return {150,600,3000};
    return {150,700,3500};
}

// samples: mono multiband input. A band acts when its level swings at least
// 4 dB more than the other bands typically do (a booming note, a harsh pick
// peak; the shared note decay cancels out). The reference excludes the band
// itself, so a band that dominates the mix cannot hide its own swing. Static
// balance is the Tone EQ's job: excess over the style target only deepens the
// range of a band that already acts.
inline MultibandResult analyseMultiband(const std::vector<float>& samples,double rate,std::optional<bool> bass={},Style style=Style::rock) {
    MultibandResult r;
    const auto spectrum=measureSpectrum(samples,rate,bass);
    r.playingDetected=spectrum.playing;r.bass=spectrum.bass;
    if(!r.playingDetected)return r;
    r.crossovers=multibandCrossovers(r.bass,style);
    for(auto& c:r.crossovers)c=std::min(c,float(rate*.4));
    const auto target=alignedTarget(spectrum,style);
    // Four bands through 24 dB/oct splits (two cascaded 12 dB/oct sections).
    struct Section {EQCoefficients c;double z1{},z2{};double tick(double x){const double y=c.b0*x+z1;z1=c.b1*x-c.a1*y+z2;z2=c.b2*x-c.a2*y;return y;}};
    const auto pair=[&](int type,float f){return std::array<Section,2>{Section{eqCoefficients(type,f,0,.70710678,rate)},Section{eqCoefficients(type,f,0,.70710678,rate)}};};
    std::array<std::array<Section,2>,4> low{pair(4,r.crossovers[0]),pair(4,r.crossovers[1]),pair(4,r.crossovers[2]),{}};
    std::array<std::array<Section,2>,4> high{std::array<Section,2>{},pair(3,r.crossovers[0]),pair(3,r.crossovers[1]),pair(3,r.crossovers[2])};
    const int windowSamples=std::max(1,int(rate*.010));
    std::array<std::vector<float>,4> bandRms;std::vector<float> fullRms;
    std::array<double,4> energy{};double full=0;int n=0;
    for(const float x:samples) {
        for(int b=0;b<4;++b) {
            double y=x;
            if(b>0)for(auto& s:high[size_t(b)])y=s.tick(y);
            if(b<3)for(auto& s:low[size_t(b)])y=s.tick(y);
            energy[size_t(b)]+=y*y;
        }
        full+=double(x)*x;
        if(++n==windowSamples) {
            for(int b=0;b<4;++b){bandRms[size_t(b)].push_back(toDb(std::sqrt(energy[size_t(b)]/n)));energy[size_t(b)]=0;}
            fullRms.push_back(toDb(std::sqrt(full/n)));full=0;n=0;
        }
    }
    const float loud=percentile(fullRms,.90f);
    std::vector<size_t> playing;for(size_t i=0;i<fullRms.size();++i)if(fullRms[i]>=loud-15.f && fullRms[i]>minimumPlayingDb)playing.push_back(i);
    if(playing.size()<50){r.playingDetected=false;return r;}
    const auto pick=[&](const std::vector<float>& v){std::vector<float> out;for(const auto i:playing)out.push_back(v[i]);return out;};
    const auto playingFull=pick(fullRms);
    // Swing over 100 ms (ten windows, playing ones only): a 10 ms window holds
    // too few low-frequency cycles, so noise alone would spread its level.
    const auto swingOf=[&](const std::vector<float>& level) {
        std::vector<float> slow;double sum=0;int count=0;
        for(const auto i:playing){sum+=std::pow(10.,level[i]/10.);if(++count==10){slow.push_back(float(10*std::log10(std::max(1e-12,sum/10))));sum=0;count=0;}}
        return percentile(slow,.95f)-percentile(slow,.2f);
    };
    std::array<float,4> swing{};
    for(int b=0;b<4;++b)swing[size_t(b)]=swingOf(bandRms[size_t(b)]);
    const float releaseScale=style==Style::metal?.7f:style==Style::clean?1.3f:1.f;
    constexpr std::array<float,4> attacks{30,20,10,5},releases{200,150,100,80};
    for(int b=0;b<4;++b) {
        auto& band=r.bands[size_t(b)];
        const auto levels=pick(bandRms[size_t(b)]);
        std::vector<float> others;for(int o=0;o<4;++o)if(o!=b)others.push_back(swing[size_t(o)]);
        band.swingDb=swing[size_t(b)]-percentile(others,.5f);
        const float lo=b==0?0.f:r.crossovers[size_t(b-1)],hi=b==3?1e9f:r.crossovers[size_t(b)];
        double excess=0;int count=0;
        for(int k=0;k<toneBandCount;++k){const float fc=toneBandCentres[size_t(k)];if(fc>=lo && fc<hi && fc<rate*.4 && spectrum.bandsDb[size_t(k)]>spectrum.strongestDb-45.f){excess+=spectrum.bandsDb[size_t(k)]-target[size_t(k)];++count;}}
        band.excessDb=count?float(excess/count):0.f;
        // Bands with almost no energy (a bass's top band) are left alone.
        const bool audible=percentile(levels,.5f)>percentile(playingFull,.5f)-40.f;
        band.active=audible && band.swingDb>=4.f;
        if(!band.active)continue;
        band.thresholdDb=std::round(std::clamp(percentile(levels,.75f),-72.f,0.f));
        band.ratio=band.swingDb<6.f?2.f:band.swingDb<10.f?3.f:4.f;
        band.rangeDb=std::clamp(std::round(.5f*band.swingDb+.5f*std::max(0.f,band.excessDb)+2.f),3.f,9.f);
        band.attackMs=attacks[size_t(b)];band.releaseMs=std::round(releases[size_t(b)]*releaseScale);
    }
    return r;
}

// Dynamic EQ (phase 6): narrow resonances that come and go, at the Tone EQ
// input (where the Dynamic EQ detector listens). A third-octave band acts when
// its loudest moments stand above its neighbours' loudest moments (2/3 to 4/3
// octave away on both sides, so a broad tilt cancels) and it also drops back
// between them: a note that rings louder than the rest, a cabinet resonance a
// few notes excite. A band that never drops back is static balance, the Tone
// EQ's job.
struct DynamicEQNode {float frequency{},q{3},thresholdDb{-24},ratio{2},rangeDb{6},attackMs{10},releaseMs{120},excessDb{};};
struct DynamicEQResult {
    bool playingDetected{},bass{};
    std::vector<DynamicEQNode> nodes;          // strongest first
    std::array<float,toneBandCount> excessDb{}; // per third-octave band, 0 where not evaluated
    std::array<float,toneBandCount> relativeJumpDb{}; // time-paired band/full-band contrast
};
inline constexpr float dynamicEQQ=3.f,dynamicEQMinExcessDb=4.f,dynamicEQMinJumpDb=6.f;
// avoid: centres of Dynamic EQ nodes already on the Tone EQ; maxNodes: free slots.
inline DynamicEQResult analyseDynamicEQ(const std::vector<float>& samples,double rate,std::optional<bool> bass={},Style style=Style::rock,
                                        const std::vector<float>& avoid={},int maxNodes=2) {
    DynamicEQResult r;
    const auto spectrum=measureSpectrum(samples,rate,bass);
    r.playingDetected=spectrum.playing;r.bass=spectrum.bass;
    if(!r.playingDetected)return r;
    const float lo=r.bass?50.f:80.f,hi=std::min(r.bass?2500.f:4000.f,float(rate*.4));
    std::vector<int> bands;for(int b=0;b<toneBandCount;++b)if(toneBandCentres[size_t(b)]<rate*.45)bands.push_back(b);
    // The same band pass as the Dynamic EQ detector, read as 20 ms peaks
    // (the detector's envelope decays over 20 ms).
    struct Detector {EQCoefficients c;double z1{},z2{};double tick(double x){const double y=c.b0*x+z1;z1=c.b1*x-c.a1*y+z2;z2=c.b2*x-c.a2*y;return y;}};
    std::vector<Detector> detectors;for(const int b:bands)detectors.push_back({eqCoefficients(6,toneBandCentres[size_t(b)],0,dynamicEQQ,rate)});
    const int blockSamples=std::max(1,int(rate*.020));
    // Band RMS decides (a pick's broadband click lights every band's peak);
    // band peaks place the threshold the peak detector will compare with.
    std::vector<std::vector<float>> peak(bands.size()),rms(bands.size());std::vector<float> fullPeak,fullRms;
    std::vector<double> held(bands.size()),bandEnergy(bands.size());double fullHeld=0,energy=0;int n=0;
    for(const float x:samples) {
        for(size_t k=0;k<detectors.size();++k){const double y=detectors[k].tick(x);held[k]=std::max(held[k],std::abs(y));bandEnergy[k]+=y*y;}
        fullHeld=std::max(fullHeld,double(std::abs(x)));energy+=double(x)*x;
        if(++n==blockSamples) {
            for(size_t k=0;k<held.size();++k){peak[k].push_back(toDb(held[k]));rms[k].push_back(toDb(std::sqrt(bandEnergy[k]/n)));held[k]=bandEnergy[k]=0;}
            fullPeak.push_back(toDb(fullHeld));fullRms.push_back(toDb(std::sqrt(energy/n)));fullHeld=energy=0;n=0;
        }
    }
    const float loud=percentile(fullRms,.90f);
    std::vector<size_t> playing;for(size_t i=0;i<fullRms.size();++i)if(fullRms[i]>=loud-15.f && fullRms[i]>minimumPlayingDb)playing.push_back(i);
    if(playing.size()<50){r.playingDetected=false;return r;}
    std::vector<float> loudRms;for(const auto i:playing)loudRms.push_back(fullRms[i]);
    const float fullTop=percentile(loudRms,.95f);
    std::vector<std::vector<float>> levels(bands.size());std::vector<float> top(bands.size()),typical(bands.size());
    for(size_t k=0;k<bands.size();++k) {
        std::vector<float> energyLevels;
        for(const auto i:playing){levels[k].push_back(peak[k][i]);energyLevels.push_back(rms[k][i]);}
        top[k]=percentile(energyLevels,.95f);typical[k]=percentile(energyLevels,.5f);
    }
    struct Candidate {size_t k;float excess;};std::vector<Candidate> candidates;
    for(size_t k=2;k+2<bands.size();++k) {
        const float fc=toneBandCentres[size_t(bands[k])];
        if(fc<lo || fc>hi)continue;
        std::vector<float> neighbours;
        for(size_t d=k>=4?k-4:0;d<=std::min(bands.size()-1,k+4);++d)if((d>k?d-k:k-d)>=2)neighbours.push_back(top[d]);
        const float excess=top[k]-percentile(neighbours,.5f);
        r.excessDb[size_t(bands[k])]=excess;
        // A decaying fundamental or a louder pick changes every band together.
        // Require the band's share of the SAME frame to change as well. Two
        // independently ranked percentiles would compare different notes.
        std::vector<float> contrast;
        for(const auto i:playing)contrast.push_back(rms[k][i]-fullRms[i]);
        const float jump=percentile(contrast,.95f)-percentile(contrast,.5f);
        r.relativeJumpDb[size_t(bands[k])]=jump;
        if(excess>=dynamicEQMinExcessDb && top[k]-typical[k]>=dynamicEQMinJumpDb
           && jump>=dynamicEQMinJumpDb && top[k]>=fullTop-24.f)candidates.push_back({k,excess});
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.excess>b.excess;});
    const auto apart=[](float a,float b){return std::abs(std::log2(a/b))>=2.f/3.f-1e-3f;};
    std::vector<float> taken(avoid.begin(),avoid.end());
    const float releaseScale=style==Style::metal?.7f:style==Style::clean?1.3f:1.f;
    for(const auto& c:candidates) {
        if(int(r.nodes.size())>=maxNodes)break;
        const float fc=toneBandCentres[size_t(bands[c.k])];
        if(!std::all_of(taken.begin(),taken.end(),[&](float f){return apart(f,fc);}))continue;
        taken.push_back(fc);
        DynamicEQNode node;node.frequency=fc;node.q=dynamicEQQ;node.excessDb=c.excess;
        // Act on the loudest fifth of the band; deeper and firmer for stronger rings.
        node.thresholdDb=std::round(std::clamp(percentile(levels[c.k],.80f),-72.f,0.f));
        node.ratio=c.excess<6.f?2.f:c.excess<9.f?3.f:4.f;
        node.rangeDb=std::clamp(std::round(c.excess-1.f),3.f,9.f);
        node.attackMs=fc<250.f?20.f:fc<1500.f?10.f:5.f;
        node.releaseMs=std::round((fc<250.f?250.f:fc<1500.f?150.f:80.f)*releaseScale);
        r.nodes.push_back(node);
    }
    return r;
}

// Final EQ and output level (phase 7), at the limiter input: after the Final
// EQ and the output trim. Everything before has been set by then, so the
// moves are small finishing corrections toward the style target and the trim
// brings the loudness to a working level below the limiter ceiling.
struct FinalMove {int type{};float frequency{},gainDb{},q{.70710678f};bool active{};};
struct OutputResult {
    bool playingDetected{},bass{};
    // Rumble high pass (guitar), low-mid bell, air shelf; inactive moves are skipped.
    std::array<FinalMove,3> moves{};
    float rumbleExcessDb{},mudExcessDb{},airExcessDb{};
    bool loudnessMeasured{};
    float loudnessLufs{-70},peakDb{-120},eqLoudnessDeltaDb{},eqPeakDeltaDb{},trimChangeDb{};
    bool peakLimited{}; // the trim was held back so the peaks stay below the limiter ceiling
    int activeMoves() const noexcept {int n=0;for(const auto& m:moves)n+=m.active;return n;}
};
inline constexpr float outputTargetLufs=-18.f,outputMaxTrimChangeDb=12.f,outputMinTrimChangeDb=1.f,finalMaxMoveDb=3.f,finalMinMoveDb=1.f;
// ITU-R BS.1770 K-weighting (pre-filter shelf and RLB high pass), redesigned
// for any rate from the standard's 48 kHz analogue parameters.
inline std::array<EQCoefficients,2> kWeighting(double rate) {
    const double pi=juce::MathConstants<double>::pi;
    double k=std::tan(pi*1681.974450955533/rate);const double q=.7071752369554196,vh=std::pow(10.,3.999843853973347/20.),vb=std::pow(vh,.4996667741545416);
    double a0=1+k/q+k*k;
    const EQCoefficients shelf{(vh+vb*k/q+k*k)/a0,2*(k*k-vh)/a0,(vh-vb*k/q+k*k)/a0,2*(k*k-1)/a0,(1-k/q+k*k)/a0};
    k=std::tan(pi*38.13547087602444/rate);const double qh=.5003270373238773;a0=1+k/qh+k*k;
    const EQCoefficients highPass{1,-2,1,2*(k*k-1)/a0,(1-k/qh+k*k)/a0};
    return {shelf,highPass};
}
inline float finalResponseDb(const std::array<FinalMove,3>& moves,float frequency,double rate) {
    double db=0;
    for(const auto& m:moves)if(m.active)db+=20*std::log10(std::max(1e-9,eqCoefficients(m.type,m.frequency,m.gainDb,m.q,rate).magnitude(frequency,rate)));
    return float(db);
}
// Gated integrated loudness (400 ms blocks, 75% overlap, -70 LUFS absolute
// and -10 LU relative gates) of the mono channel mean, reported for the
// stereo output it stands for (both channels equal: +3 dB).
inline std::optional<float> integratedLoudness(const std::vector<float>& samples,double rate) {
    auto k=kWeighting(rate);std::array<double,4> z{};
    std::vector<double> squared(samples.size());
    for(size_t n=0;n<samples.size();++n) {
        double y=samples[n];
        for(int s=0;s<2;++s){auto& c=k[size_t(s)];double& z1=z[size_t(2*s)];double& z2=z[size_t(2*s+1)];const double out=c.b0*y+z1;z1=c.b1*y-c.a1*out+z2;z2=c.b2*y-c.a2*out;y=out;}
        squared[n]=y*y;
    }
    const size_t block=size_t(rate*.4),hop=size_t(rate*.1);
    if(block==0 || samples.size()<block)return std::nullopt;
    const auto lufs=[](double meanSquare){return -.691+10*std::log10(std::max(1e-30,2*meanSquare));};
    std::vector<double> blocks;double sum=0;
    for(size_t n=0;n<block;++n)sum+=squared[n];
    for(size_t start=0;;start+=hop) {
        if(lufs(sum/double(block))>-70)blocks.push_back(sum/double(block));
        if(start+hop+block>squared.size())break;
        for(size_t n=start;n<start+hop;++n)sum-=squared[n];
        for(size_t n=start+block;n<start+block+hop;++n)sum+=squared[n];
    }
    if(blocks.size()<10)return std::nullopt;
    double mean=0;for(const auto b:blocks)mean+=b;mean/=double(blocks.size());
    const double relative=lufs(mean)-10;double gated=0;int count=0;
    for(const auto b:blocks)if(lufs(b)>relative){gated+=b;++count;}
    return count>=10?std::optional<float>(float(lufs(gated/count))):std::nullopt;
}
// samples: mono limiter input, at the current output trim.
inline OutputResult analyseOutput(const std::vector<float>& samples,double rate,std::optional<bool> bass={},Style style=Style::rock) {
    OutputResult r;
    const auto spectrum=measureSpectrum(samples,rate,bass);
    r.playingDetected=spectrum.playing;r.bass=spectrum.bass;
    if(!r.playingDetected)return r;
    // Align the style target on 630 Hz-2 kHz, above the low-mid region this
    // pass judges, so a low-mid bump does not make the top look dark.
    std::array<float,toneBandCount> target{};double offset=0;int reference=0;
    for(int b=0;b<toneBandCount;++b) {
        const float fc=toneBandCentres[size_t(b)];target[size_t(b)]=toneTargetDb(spectrum.bass,fc,style);
        if(fc>=630 && fc<=2000){offset+=spectrum.bandsDb[size_t(b)]-target[size_t(b)];++reference;}
    }
    for(auto& t:target)t+=float(offset/reference);
    const auto excess=[&](float lo,float hi) {
        double sum=0;int count=0;
        for(int b=0;b<toneBandCount;++b){const float fc=toneBandCentres[size_t(b)];
            if(fc>=lo && fc<=hi && fc<rate*.4 && spectrum.bandsDb[size_t(b)]>spectrum.strongestDb-45.f){sum+=spectrum.bandsDb[size_t(b)]-target[size_t(b)];++count;}}
        return count?std::optional<float>(float(sum/count)):std::nullopt;
    };
    const auto step=[](float db){const float g=std::round(std::clamp(db,-finalMaxMoveDb,finalMaxMoveDb)*2.f)*.5f;return std::abs(g)<finalMinMoveDb?0.f:g;};
    // Rumble below a guitar's lowest note: a 12 dB/oct high pass at 70 Hz.
    if(!r.bass)if(const auto e=excess(50,63)) {
        r.rumbleExcessDb=*e;
        if(*e>=3.f)r.moves[0]={3,70,0,.70710678f,true};
    }
    // Low-mid weight against the body above it: half the difference, up to 3 dB.
    if(const auto mud=excess(250,400))if(const auto body=excess(630,1000)) {
        r.mudExcessDb=*mud-*body;
        if(const float g=step(-.5f*r.mudExcessDb);g!=0.f)r.moves[1]={0,315,g,1.f,true};
    }
    // Air: the top octave against the target, half the difference, up to 3 dB.
    const float airLow=r.bass?4000.f:6300.f,airHigh=r.bass?6300.f:10000.f;
    if(const auto air=excess(airLow,airHigh)) {
        r.airExcessDb=*air;
        if(const float g=step(-.5f*r.airExcessDb);g!=0.f)r.moves[2]={2,r.bass?5000.f:8000.f,g,.70710678f,true};
    }
    // Loudness and peaks as heard, then the change the moves make: band powers
    // weighted by K-weighting, through the moves' response.
    const auto loudness=integratedLoudness(samples,rate);
    float peak=0;for(const float x:samples)peak=std::max(peak,std::abs(x));
    r.peakDb=toDb(peak);
    if(!loudness)return r;
    r.loudnessMeasured=true;r.loudnessLufs=*loudness;
    const auto k=kWeighting(rate);double before=0,after=0,peakGain=1;
    for(int b=0;b<toneBandCount;++b) {
        const float fc=toneBandCentres[size_t(b)];if(fc>=rate*.45)continue;
        const double kw=k[0].magnitude(fc,rate)*k[1].magnitude(fc,rate),power=std::pow(10.,spectrum.bandsDb[size_t(b)]/10.)*kw*kw;
        const double gain=std::pow(10.,finalResponseDb(r.moves,fc,rate)/20.);
        before+=power;after+=power*gain*gain;
        if(spectrum.bandsDb[size_t(b)]>spectrum.strongestDb-20.f)peakGain=std::max(peakGain,gain);
    }
    r.eqLoudnessDeltaDb=before>0?float(10*std::log10(after/before)):0.f;
    r.eqPeakDeltaDb=float(20*std::log10(peakGain));
    float change=outputTargetLufs-(r.loudnessLufs+r.eqLoudnessDeltaDb);
    // Leave the peaks below the limiter's ceiling: the trim sets the level,
    // the limiter only catches the occasional peak.
    const float headroom=limiterCeilingDb-(r.peakDb+r.eqPeakDeltaDb);
    if(change>headroom){change=headroom;r.peakLimited=true;}
    change=std::round(std::clamp(change,-outputMaxTrimChangeDb,outputMaxTrimChangeDb)*2.f)*.5f;
    r.trimChangeDb=std::abs(change)<outputMinTrimChangeDb?0.f:change;
    return r;
}

}

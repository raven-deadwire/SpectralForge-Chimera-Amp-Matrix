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
// input/compressor/output are reduced to 10 ms windows; tone and deesser keep
// mono samples for spectral analysis (amp/cab output before the Tone EQ, and
// the de-esser input). The two sample taps share one ring.
enum class Tap {input=0,compressor=1,output=2,tone=3,deesser=4};

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
        if(tap==Tap::tone || tap==Tap::deesser){pushSamples(channels,channelCount,samples);return;}
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
        if(int(tap)>=int(taps.size()))return;
        auto& t=taps[size_t(tap)];
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
    std::array<TapState,3> taps;
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

}

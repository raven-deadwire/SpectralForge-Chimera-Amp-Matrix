#pragma once
#include "AutoSetAmp.h"
#include "EasyMode.h"

namespace spectralforge::autoset {

// Phase 12: the amp head's PRESENCE and RESONANCE (or DEPTH), the power amp's
// upper-mid bite and low-end weight. A result step of the amp pass: the same
// capture (the playing entering the amp and the amp/cab output) is rendered
// offline through the native amp at the knobs the amp step left, with each
// candidate PRESENCE/RESONANCE value. As in phase 8, the amp/cab output minus
// the amp rendered at the settings that were playing gives the cabinet, so a
// candidate's render plus that difference predicts what is heard.
// - Bite: the 2-5 kHz level relative to 400 Hz-1.6 kHz (bass: 1.25-3.15 kHz),
//   against the same difference of the style's target.
// - Weight: the 63-160 Hz level relative to 400 Hz-1.6 kHz (bass: 25-100 Hz),
//   against the target's. With guitar rock or metal gain, too much weight
//   counts double: a loose low end smears the distortion.
enum PowerRole {powerPresence=0,powerResonance=1};
inline constexpr const char* powerRoleNames[]{"PRESENCE","RESONANCE"};
struct PowerResult {
    bool playingDetected{},bass{};
    std::array<AmpKnob,2> before{},proposed{};
    std::array<float,2> excessBeforeDb{},excessAfterDb{};
    bool changed(int role) const noexcept {return proposed[size_t(role)].present && proposed[size_t(role)].value!=before[size_t(role)].value;}
    int changedKnobs() const noexcept {return int(changed(powerPresence))+int(changed(powerResonance));}
};
inline constexpr float powerToleranceDb=1.5f,powerMinGainDb=1.f,powerMaxMove=.3f,powerStep=.05f,powerLooseWeight=2.f;
// The front-panel names of each role; the first one the channel shows wins.
inline std::span<const char* const> powerRoleLabels(int role) noexcept {
    static constexpr const char* presence[]{"PRESENCE","LEAD PRESENCE"};
    static constexpr const char* resonance[]{"RESONANCE","DEPTH"};
    return role==powerPresence?std::span<const char* const>(presence):std::span<const char* const>(resonance);
}
// The knob a role edits on a head's channel, or -1.
inline int powerControl(int model,int channel,int role) noexcept {
    const auto& panel=ampNativePanel(model);
    for(const auto* wanted:powerRoleLabels(role))
        for(int c=0;c<(int)panel.controls.size();++c) {
            const auto& control=panel.controls[(size_t)c];
            if(control.kind==AmpNativeControlKind::knob && ampNativeControlVisible(model,c,channel)
               && juce::String::fromUTF8(control.label).upToFirstOccurrenceOf(" / ",false,false)==wanted)return c;
        }
    return -1;
}
inline float powerBandDb(const std::array<float,toneBandCount>& bands,float from,float to) {
    double power=0;
    for(int b=0;b<toneBandCount;++b)if(toneBandCentres[size_t(b)]>=from && toneBandCentres[size_t(b)]<=to)power+=std::pow(10.,bands[size_t(b)]/10.);
    return float(10*std::log10(std::max(power,1e-30)));
}
// How far a band's balance against 400 Hz-1.6 kHz sits above the style target's (dB).
// This shared 24-band helper preserves the guitar calculation and the legacy
// bass comparison (effectively 50-100 Hz). Production Phase 12 bass analysis
// uses powerBassExcessDb below, whose spectrum includes the fundamentals.
inline float powerExcessDb(const std::array<float,toneBandCount>& bands,int role,bool bass,Style style) {
    std::array<float,toneBandCount> target{};
    for(int b=0;b<toneBandCount;++b)target[size_t(b)]=toneTargetDb(bass,toneBandCentres[size_t(b)],style);
    const float from=role==powerPresence?(bass?1250.f:2000.f):(bass?40.f:63.f),to=role==powerPresence?(bass?3150.f:5000.f):(bass?100.f:160.f);
    const auto balance=[&](const std::array<float,toneBandCount>& x){return powerBandDb(x,from,to)-powerBandDb(x,400,1600);};
    return balance(bands)-balance(target);
}
// Phase 12 needs the fundamental of extended-range basses. Do not add these
// bands to the shared Tone EQ spectrum: that would also retune other phases.
inline constexpr int powerBassBandCount=toneBandCount+3;
inline constexpr auto powerBassBandCentres=[] {
    std::array<float,powerBassBandCount> centres{25.f,31.5f,40.f};
    for(int b=0;b<toneBandCount;++b)centres[size_t(b+3)]=toneBandCentres[size_t(b)];
    return centres;
}();
struct PowerBassSpectrum {
    bool measured{}; // spectral data available; the shared playing gate is separate
    std::array<float,powerBassBandCount> bandsDb{};
};
inline PowerBassSpectrum measurePowerBassSpectrum(const std::vector<float>& samples,double rate) {
    PowerBassSpectrum result;
    if(!std::isfinite(rate) || rate<8000)return result;
    // About 1.5 Hz bins resolve 25/31.5 Hz without the rate-dependent loss at
    // the low edge of the old ~6 Hz FFT. Quarter hops overlap measurements in
    // the three-second render. This helper measures a spectrum; analysePower
    // keeps the original short-window playing gate for duration/confidence.
    const int order=std::clamp(int(std::round(std::log2(rate/1.5))),12,18),size=1<<order,hop=size/4;
    if(samples.size()<size_t(size))return result;
    juce::dsp::FFT fft(order);
    std::vector<float> window(static_cast<size_t>(size)),frame(static_cast<size_t>(2*size));
    for(int n=0;n<size;++n)window[size_t(n)]=float(.5-.5*std::cos(2*juce::MathConstants<double>::pi*n/size));
    std::array<std::pair<int,int>,powerBassBandCount> bins{};
    const double edge=std::pow(2.,1./6.);
    for(int b=0;b<powerBassBandCount;++b) {
        const double fc=powerBassBandCentres[size_t(b)];
        bins[size_t(b)]={std::max(1,int(std::ceil(fc/edge*size/rate))),std::min(size/2,int(std::ceil(fc*edge*size/rate)))};
    }
    struct Frame {float levelDb;std::array<double,powerBassBandCount> power;};
    std::vector<Frame> frames;
    for(size_t start=0;start+size_t(size)<=samples.size();start+=size_t(hop)) {
        double energy=0;
        for(int n=0;n<size;++n) {
            const float x=samples[start+size_t(n)];
            if(!std::isfinite(x))return result;
            energy+=double(x)*x;frame[size_t(n)]=x*window[size_t(n)];
        }
        std::fill(frame.begin()+size,frame.end(),0.f);
        fft.performFrequencyOnlyForwardTransform(frame.data(),true);
        Frame f{toDb(std::sqrt(energy/size)),{}};
        for(int b=0;b<powerBassBandCount;++b)
            for(int k=bins[size_t(b)].first;k<bins[size_t(b)].second;++k)f.power[size_t(b)]+=double(frame[size_t(k)])*frame[size_t(k)];
        frames.push_back(f);
    }
    std::vector<float> levels;for(const auto& f:frames)levels.push_back(f.levelDb);
    const float loud=percentile(levels,.90f);
    std::array<double,powerBassBandCount> power{};int used=0;
    for(const auto& f:frames)if(f.levelDb>=loud-15.f && f.levelDb>minimumPlayingDb) {
        ++used;for(int b=0;b<powerBassBandCount;++b)power[size_t(b)]+=f.power[size_t(b)];
    }
    result.measured=used>0;
    if(result.measured)for(int b=0;b<powerBassBandCount;++b)
        result.bandsDb[size_t(b)]=float(10*std::log10(std::max(1e-30,power[size_t(b)]/used)));
    return result;
}
inline float powerBassBandDb(const std::array<float,powerBassBandCount>& bands,float from,float to) {
    double power=0;
    for(int b=0;b<powerBassBandCount;++b)if(powerBassBandCentres[size_t(b)]>=from && powerBassBandCentres[size_t(b)]<=to)
        power+=std::pow(10.,bands[size_t(b)]/10.);
    return float(10*std::log10(std::max(power,1e-30)));
}
inline float powerBassExcessDb(const std::array<float,powerBassBandCount>& bands,int role,Style style) {
    std::array<float,powerBassBandCount> target{};
    // Reuse the existing curve, including its constant extension below 40 Hz.
    // That sub-bass target remains provisional pending DI and listening tests.
    for(int b=0;b<powerBassBandCount;++b)target[size_t(b)]=toneTargetDb(true,powerBassBandCentres[size_t(b)],style);
    const float from=role==powerPresence?1250.f:25.f,to=role==powerPresence?3150.f:100.f;
    const auto balance=[&](const auto& x){return powerBassBandDb(x,from,to)-powerBassBandDb(x,400,1600);};
    return balance(bands)-balance(target);
}
// What a role's excess costs: too much low end counts double on driven guitar.
inline float powerCost(float excess,int role,bool bass,Style style) noexcept {
    const bool loose=role==powerResonance && !bass && style!=Style::clean && excess>0;
    return std::abs(excess)*(loose?powerLooseWeight:1.f);
}
// PRESENCE, RESONANCE values -> amp output for the analysed input (the amp's
// other knobs as the amp step left them).
using PowerRenderer=std::function<std::vector<float>(const std::array<float,2>&)>;
// input: the signal entering the amp (at most ampRenderSeconds is rendered);
// post: the amp/cab output heard; playedAmp: the amp rendered at the settings
// that were playing while post was heard.
inline PowerResult analysePower(const std::vector<float>& input,const std::vector<float>& post,const std::vector<float>& playedAmp,double rate,
                                const std::array<AmpKnob,2>& knobs,const PowerRenderer& render,std::optional<bool> bass={},Style style=Style::rock) {
    PowerResult r;r.before=knobs;r.proposed=knobs;
    if(!std::isfinite(rate) || rate<8000)return r;
    const auto heard=measureSpectrum(post,rate,bass);
    std::vector<float> segment(input.begin(),input.begin()+std::min(input.size(),size_t(rate*ampRenderSeconds)));
    r.playingDetected=heard.playing && measureSpectrum(segment,rate,true).playing;
    if(!r.playingDetected)return r;
    r.bass=heard.bass;
    const auto played=measureSpectrum(playedAmp,rate,r.bass);
    if(!played.playing){r.playingDetected=false;return r;}
    std::array<float,toneBandCount> after{};
    for(int b=0;b<toneBandCount;++b)after[size_t(b)]=heard.bandsDb[size_t(b)]-played.bandsDb[size_t(b)];
    std::array<float,powerBassBandCount> afterBass{};
    if(r.bass) {
        const auto heardBass=measurePowerBassSpectrum(post,rate),playedBass=measurePowerBassSpectrum(playedAmp,rate);
        if(!heardBass.measured || !playedBass.measured){r.playingDetected=false;return r;}
        for(int b=0;b<powerBassBandCount;++b)afterBass[size_t(b)]=heardBass.bandsDb[size_t(b)]-playedBass.bandsDb[size_t(b)];
    }
    std::array<float,2> values{knobs[0].value,knobs[1].value};
    const auto excesses=[&](const std::array<float,2>& v) {
        std::array<float,2> e{};
        if(r.bass) {
            auto s=measurePowerBassSpectrum(render(v),rate);
            if(!s.measured){e.fill(1e9f);return e;}
            for(int b=0;b<powerBassBandCount;++b)s.bandsDb[size_t(b)]+=afterBass[size_t(b)];
            for(int role=0;role<2;++role)e[size_t(role)]=powerBassExcessDb(s.bandsDb,role,style);
            return e;
        }
        auto s=measureSpectrum(render(v),rate,r.bass);
        if(!s.playing){e.fill(1e9f);return e;}
        for(int b=0;b<toneBandCount;++b)s.bandsDb[size_t(b)]+=after[size_t(b)];
        for(int role=0;role<2;++role)e[size_t(role)]=powerExcessDb(s.bandsDb,role,r.bass,style);
        return e;
    };
    auto now=excesses(values);
    r.excessBeforeDb=now;
    const auto snap=[](const AmpKnob& k,float v){const float range=k.maximum-k.minimum;return std::clamp(k.minimum+std::round((v-k.minimum)/range*100.f)/100.f*range,k.minimum,k.maximum);};
    // Each knob in turn (PRESENCE first, then RESONANCE with the new PRESENCE):
    // the candidate within +-30% of the range that brings its balance closest
    // to the target, if that is a clear improvement. Both balances share the
    // 400 Hz-1.6 kHz reference, so a candidate that pushes the other balance
    // past the tolerance (or past where it was) pays for that too.
    const auto score=[&](const std::array<float,2>& e,const std::array<float,2>& from,int role) {
        const int other=1-role;
        const float allowed=std::max(powerCost(from[size_t(other)],other,r.bass,style),powerToleranceDb);
        return powerCost(e[size_t(role)],role,r.bass,style)+std::max(0.f,powerCost(e[size_t(other)],other,r.bass,style)-allowed);
    };
    const auto totalCost=[&](const std::array<float,2>& e) {
        return powerCost(e[0],0,r.bass,style)+powerCost(e[1],1,r.bass,style);
    };
    for(int role=0;role<2;++role) {
        const auto& knob=knobs[size_t(role)];
        if(!knob.present || knob.maximum<=knob.minimum)continue;
        if(powerCost(now[size_t(role)],role,r.bass,style)<=powerToleranceDb)continue;
        const auto from=now;const float start=score(now,from,role);
        const float range=knob.maximum-knob.minimum;
        float best=start;auto bestValues=values;auto bestExcess=now;
        const float low=std::max(knob.minimum,knob.value-powerMaxMove*range),high=std::min(knob.maximum,knob.value+powerMaxMove*range);
        // Only the direction that moves the balance toward the target.
        const float direction=now[size_t(role)]>0?-1.f:1.f;
        float previous=knob.value;
        for(int i=1;float(i)*powerStep<=powerMaxMove+1e-6f;++i) {
            auto probe=values;
            probe[size_t(role)]=snap(knob,std::clamp(knob.value+direction*float(i)*powerStep*range,low,high));
            if(probe[size_t(role)]==previous)break; // reached the end of the knob
            previous=probe[size_t(role)];
            const auto e=excesses(probe);
            // The other balance's tolerance is not permission to make the
            // combined error worse, even when that other error stays <1.5 dB.
            if(totalCost(e)>totalCost(from)+1e-5f)continue;
            if(const float cost=score(e,from,role);cost<best){best=cost;bestValues=probe;bestExcess=e;}
        }
        if(best<start-powerMinGainDb){values=bestValues;now=bestExcess;}
    }
    r.excessAfterDb=now;
    for(int role=0;role<2;++role)r.proposed[size_t(role)].value=values[size_t(role)];
    return r;
}
// Renders the input through a fresh native amp: `state` with the amp step's
// knobs (`fixed`: control, value) and the candidate PRESENCE/RESONANCE.
inline PowerRenderer nativePowerRenderer(const AmpNativeState& state,std::vector<std::pair<int,float>> fixed,const std::array<int,2>& controls,std::vector<float> input,double rate) {
    return [state,fixed=std::move(fixed),controls,input=std::move(input),rate](const std::array<float,2>& values) {
        auto s=state;
        for(const auto& [control,value]:fixed)s.values[size_t(control)]=value;
        for(int k=0;k<2;++k)if(controls[size_t(k)]>=0)s.values[size_t(controls[size_t(k)])]=values[size_t(k)];
        auto dsp=std::make_unique<AmpNativeDSP>();dsp->prepare(rate);dsp->set(s);dsp->reset();
        std::vector<float> out(input.size());
        for(size_t n=0;n<input.size();++n)out[n]=dsp->tick(input[n],0);
        return out;
    };
}
} // namespace spectralforge::autoset

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "AmpCatalog.h"
#include "NewAmpDSP.h"
#include "AmpNativeDSP.h"

namespace spectralforge {
// Voiced nonlinear models, not component-level replicas or measured captures.
// The first 15 rows are the frozen Legacy path. Appended array entries are not
// used: models 15-22 execute the distinct circuits in NewAmpDSP.h instead.
struct AmpVoice { float inputHP, couplingHP, bandwidth, gain, stage2, stage3, bias, sag, dry, output; };
inline constexpr std::array<AmpVoice, ampModelCount> ampVoices{{
    {18,  25, 18000,  2.0f, 1.0f, 0.0f, .015f, .02f, .80f, 1.00f},
    {55, 110, 10500, 12.0f, 2.0f, 0.0f, .090f, .15f, .00f, .70f},
    {90, 180, 12500, 24.0f, 3.2f, 1.4f, .045f, .06f, .00f, .55f},
    {38,  65,  9000, 20.0f, 2.7f, 1.2f, .120f, .25f, .00f, .62f},
    {65, 120,  7600, 18.0f, 3.8f, 1.6f, .070f, .12f, .00f, .55f},
    {12,  24,  7500,  8.0f, 1.6f, 0.0f, .140f, .22f, .38f, .85f},
    {16,  30, 15000,  5.0f, 1.0f, 0.0f, .005f, .01f, .25f, .88f},
    {14,  55, 11500, 11.0f, 2.1f, 0.0f, .060f, .08f, .48f, .78f},
    // The appended voices are original designs informed by product manuals;
    // six later receive the documented broad capture contour below; no circuit equivalence is claimed.
    {65, 125, 13500,  9.5f, 1.8f, 0.0f, .055f, .18f, .08f, .86f},
    {32,  82,  8200, 22.0f, 2.8f, 0.9f, .110f, .18f, .00f, .65f},
    { 9,  18,  6800,  6.0f, 1.35f,0.0f, .095f, .16f, .40f, .95f},
    {12,  20, 18000,  2.8f, 1.0f, 0.0f, .003f, .005f,.78f,1.10f},
    {48,  95, 15500,  7.5f, 2.2f, 0.0f, .035f, .10f, .18f, .85f},
    {45, 100,  7000, 12.0f, 2.6f, 0.4f, .075f, .11f, .06f, .76f},
    { 8,  14, 20000,  1.4f, 1.0f, 0.0f, .001f, .003f,.90f,1.20f}
}};

// First eight contours were estimated against one documented NAM capture per family.
// Fitted on synthetic single notes; separate chords were held out. These are
// original EQ coefficients, not NAM weights or a claim of circuit equivalence.
// Appended contours are hand-authored designs, not NAM reference results.
inline constexpr std::array<std::array<float,3>,ampModelCount> referenceContours{{
    {-9.f,-12.f,12.f},{-3.33f,-9.45f,12.f},{-7.74f,-12.f,12.f},{-1.46f,-12.f,12.f},
    {-5.53f,-5.13f,12.f},{-1.83f,3.23f,5.47f},{3.19f,-.93f,12.f},{6.2f,-12.f,12.f},
    {-4.f,2.0f,5.f},{1.f,2.0f,2.f},{2.f,-1.5f,3.f},{2.f,-1.8f,1.f},
    {-1.5f,3.f,2.5f},{-2.f,4.f,.5f},{0.f,1.1f,-.4f}
}};

// Additional broad filters fitted on one synthetic pluck fixture per downloaded
// reference, then checked on a separate chord fixture. Indices 12 and 13 use
// explicitly identified Ceriatone/DC-30 and ODS #102 clone captures, respectively.
// This does not identify a circuit or establish physical hardware equivalence.
// Existing voices and EICH retain their prior response without these filters.
inline constexpr std::array<std::array<float,3>,ampModelCount> additionalCaptureContours{{
    {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
    {9.f,-1.04f,8.46f},{.45f,-5.1f,12.f},{-9.f,-12.f,3.56f},
    {4.57f,-1.7f,7.94f},{9.f,-2.46f,12.f},{3.53f,-.17f,9.89f},{0,0,0}
}};

class Amp {
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    struct Path {
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler,nativeOversampler;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> compensation{64},nativeCompensation{64};
        struct Channel { float inputLow{}, couplingLow{}, outputLow{}, dcLow{}, envelope{}; };
        std::array<Channel, 2> channels{};
        NewAmpDSP newAmp;
        std::array<AmpNativeDSP,2> nativeAmps;
        AmpNativeState nativeState{};
        bool nativeReady{};
        int nativeActive{},nativePrevious{},nativeFade{},nativeFadeLength{1};
        juce::SmoothedValue<float> drive;
        double rate{};
        void reset() { oversampler->reset();nativeOversampler->reset(); compensation.reset();nativeCompensation.reset(); channels = {}; newAmp.reset(); for(auto& amp:nativeAmps)amp.reset(); nativeFade=0; drive.setCurrentAndTargetValue(drive.getTargetValue()); }
        void setNative(const AmpNativeState& state)
        {
            if(!nativeReady) {
                for(auto& amp:nativeAmps)amp.set(state);
                nativeReady=true;nativeState=state;nativeFade=0;return;
            }
            if(state.model!=nativeState.model || state.channel!=nativeState.channel || state.inputRoute!=nativeState.inputRoute) {
                nativePrevious=nativeActive;nativeActive=1-nativeActive;
                nativeAmps[(size_t)nativeActive].set(state);
                nativeAmps[(size_t)nativeActive].reset();
                nativeFade=nativeFadeLength;
            } else nativeAmps[(size_t)nativeActive].set(state);
            nativeState=state;
        }
        void processNative(juce::AudioBuffer<float>& buffer)
        {
            juce::dsp::AudioBlock<float> base(buffer);
            auto block=nativeOversampler->processSamplesUp(base);
            for(size_t n=0;n<block.getNumSamples();++n) {
                const float mix=nativeFade>0?1.f-float(nativeFade)/float(nativeFadeLength):1.f;
                for(size_t c=0;c<block.getNumChannels();++c) {
                    const float input=block.getSample((int)c,(int)n);
                    float output=nativeAmps[(size_t)nativeActive].tick(input,(int)c);
                    if(nativeFade>0)output=mix*output+(1.f-mix)*nativeAmps[(size_t)nativePrevious].tick(input,(int)c);
                    block.setSample((int)c,(int)n,output);
                }
                if(nativeFade>0)--nativeFade;
            }
            nativeOversampler->processSamplesDown(base);
            juce::dsp::ProcessContextReplacing<float> context(base);
            nativeCompensation.process(context);
        }
        void processNew(juce::AudioBuffer<float>& buffer, float amount)
        {
            juce::dsp::AudioBlock<float> base(buffer);
            auto block = oversampler->processSamplesUp(base);
            drive.setTargetValue(amount);
            newAmp.process(block, drive);
            oversampler->processSamplesDown(base);
            juce::dsp::ProcessContextReplacing<float> context(base);
            compensation.process(context);
        }
        void process(juce::AudioBuffer<float>& buffer, const AmpVoice& voice, float amount)
        {
            juce::dsp::AudioBlock<float> base(buffer);
            auto block = oversampler->processSamplesUp(base);
            const auto pole = [this](float hz) { return float(1.0 - std::exp(-juce::MathConstants<double>::twoPi * hz / rate)); };
            const float hp = pole(voice.inputHP), coupling = pole(voice.couplingHP);
            const float lp = pole(voice.bandwidth), dc = pole(8.0f);
            const float attack = float(1.0 - std::exp(-1.0 / (.008 * rate)));
            const float release = float(1.0 - std::exp(-1.0 / (.090 * rate)));
            drive.setTargetValue(amount);
            for (size_t n = 0; n < block.getNumSamples(); ++n)
            {
                const float d = drive.getNextValue();
                for (size_t c = 0; c < block.getNumChannels(); ++c)
                {
                    auto& s = channels[c];
                    const float input = block.getSample((int)c, (int)n);
                    s.inputLow += hp * (input - s.inputLow);
                    float x = input - s.inputLow;
                    s.envelope += (std::abs(x) > s.envelope ? attack : release) * (std::abs(x) - s.envelope);
                    const float supply = 1.0f / (1.0f + voice.sag * s.envelope * 6.0f);
                    x = std::tanh(x * (1.0f + d * voice.gain) * supply + voice.bias) - std::tanh(voice.bias);
                    s.couplingLow += coupling * (x - s.couplingLow);
                    x -= s.couplingLow;
                    x = std::tanh(x * (1.0f + d * (voice.stage2 - 1.0f)));
                    if (voice.stage3 > 0.0f) x = std::tanh(x * (1.0f + d * voice.stage3));
                    x = voice.output * ((1.0f - voice.dry) * x + voice.dry * input);
                    s.outputLow += lp * (x - s.outputLow);
                    s.dcLow += dc * (s.outputLow - s.dcLow);
                    block.setSample((int)c, (int)n, s.outputLow - s.dcLow);
                }
            }
            oversampler->processSamplesDown(base);
            juce::dsp::ProcessContextReplacing<float> context(base);
            compensation.process(context);
        }
    };
    // Circuit storage is prepared once off the audio thread. Keeping these
    // fixed arrays off the stack avoids Windows' 1 MiB thread-stack ceiling
    // when a host/test constructs several routing engines at once.
    std::unique_ptr<std::array<Path,4>> pathStorage=std::make_unique<std::array<Path,4>>();
    std::array<Path,4>& paths=*pathStorage;
    Filter lo, lm, hm, hi, pres, res,voiceLow,voiceMid,voiceHigh,captureLow,captureMid,captureHigh;
    juce::AudioBuffer<float> alternate, dry, engineAlternate;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> bypassDelay{64};
    juce::SmoothedValue<float> enabled,nativeMix;
    double sr{48000};
    AmpModel model{AmpModel::glass};
    int nativeChannelValue{};
    bool useNativeControls{};
    float drive{.35f};
    int selected{2}, previous{2}, fadeRemaining{}, fadeLength{1}, delaySamples{};
    void voiceTone() {
        const auto& shape=referenceContours[(size_t)model];using C=juce::dsp::IIR::ArrayCoefficients<float>;
        *voiceLow.state=C::makeLowShelf(sr,juce::jmin(100.,sr*.4),.707f,juce::Decibels::decibelsToGain(shape[0]));
        *voiceMid.state=C::makePeakFilter(sr,juce::jmin(500.,sr*.4),.65f,juce::Decibels::decibelsToGain(shape[1]));
        *voiceHigh.state=C::makeHighShelf(sr,juce::jmin(2000.,sr*.4),.707f,juce::Decibels::decibelsToGain(shape[2]));
        const auto& capture=additionalCaptureContours[(size_t)model];
        *captureLow.state=C::makeLowShelf(sr,juce::jmin(100.,sr*.4),.707f,juce::Decibels::decibelsToGain(capture[0]));
        *captureMid.state=C::makePeakFilter(sr,juce::jmin(500.,sr*.4),.65f,juce::Decibels::decibelsToGain(capture[1]));
        *captureHigh.state=C::makeHighShelf(sr,juce::jmin(2000.,sr*.4),.707f,juce::Decibels::decibelsToGain(capture[2]));
    }
    void processLegacyTone(juce::AudioBuffer<float>& buffer,bool fullRange) {
        juce::dsp::AudioBlock<float> block(buffer);juce::dsp::ProcessContextReplacing<float> context(block);
        if(!isNewAmpModel(static_cast<int>(model))){voiceLow.process(context);voiceMid.process(context);voiceHigh.process(context);buffer.applyGain(.5f);}
        if(static_cast<int>(model)>=8 && static_cast<int>(model)<=13){captureLow.process(context);captureMid.process(context);captureHigh.process(context);}
        if(fullRange)for(auto* filter:{&lo,&lm,&hm,&hi,&res,&pres})filter->process(context);
    }
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sr = spec.sampleRate;
        delaySamples = 0;
        for (int i = 0; i < 4; ++i)
        {
            auto& p = paths[i];
            p.oversampler = std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels, i,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            p.oversampler->initProcessing(spec.maximumBlockSize);
            p.nativeOversampler=std::make_unique<juce::dsp::Oversampling<float>>(spec.numChannels,i,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
            p.nativeOversampler->initProcessing(spec.maximumBlockSize);
            p.rate = sr * (1 << i);
            p.newAmp.prepare(p.rate);
            for(auto& amp:p.nativeAmps)amp.prepare(p.rate);
            p.nativeFadeLength=juce::jmax(1,int(p.rate*.020));
            p.drive.reset(p.rate, .020);
            p.drive.setCurrentAndTargetValue(drive);
            delaySamples = juce::jmax(delaySamples, juce::roundToInt(p.oversampler->getLatencyInSamples()));
        }
        for (auto& p : paths)
        {
            p.compensation.prepare(spec);
            p.compensation.setDelay(float(delaySamples - juce::roundToInt(p.oversampler->getLatencyInSamples())));
            p.nativeCompensation.prepare(spec);
            p.nativeCompensation.setDelay(float(delaySamples-juce::roundToInt(p.nativeOversampler->getLatencyInSamples())));
        }
        bypassDelay.prepare(spec);bypassDelay.setDelay(float(delaySamples));
        enabled.reset(sr,.015);enabled.setCurrentAndTargetValue(1);
        nativeMix.reset(sr,.020);nativeMix.setCurrentAndTargetValue(useNativeControls?1.f:0.f);
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        alternate.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
        engineAlternate.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        fadeLength = juce::jmax(1, int(sr * .020));
        tone(0,0,0,0,0,0);voiceTone();
        for (auto* f : {&lo, &lm, &hm, &hi, &pres, &res,&voiceLow,&voiceMid,&voiceHigh,&captureLow,&captureMid,&captureHigh}) f->prepare(spec);
        reset();
    }
    void reset()
    {
        for (auto* f : {&lo, &lm, &hm, &hi, &pres, &res,&voiceLow,&voiceMid,&voiceHigh,&captureLow,&captureMid,&captureHigh}) f->reset();
        for (auto& p : paths) if (p.oversampler) p.reset();
        previous = selected; fadeRemaining = 0; nativeMix.setCurrentAndTargetValue(nativeMix.getTargetValue());bypassDelay.reset();enabled.setCurrentAndTargetValue(enabled.getTargetValue());
    }
    void setDriveImmediately(float value) { drive=juce::jlimit(0.f,1.f,value);for(auto& path:paths)path.drive.setCurrentAndTargetValue(drive); }
    void set(AmpModel m, float d) {
        if(useNativeControls)return; // Retain the previous legacy circuit during its exit fade.
        m = static_cast<AmpModel>(juce::jlimit(0, ampModelCount - 1, static_cast<int>(m)));
        if(model!=m){model=m;nativeChannelValue=newAmpDefaultChannel(static_cast<int>(model));voiceTone();captureLow.reset();captureMid.reset();captureHigh.reset();}
        if(isNewAmpModel(static_cast<int>(model)))for(auto& path:paths)path.newAmp.set(static_cast<int>(model),nativeChannelValue);
        drive = juce::jlimit(0.f, 1.f, d);
    }
    void set(AmpModel m, float d, int channel) { if(!useNativeControls){set(m, d); setNativeChannel(channel);} }
    void setNativeChannel(int channel)
    {
        nativeChannelValue = juce::jlimit(0, newAmpChannelCount(static_cast<int>(model)) - 1, channel);
        if(isNewAmpModel(static_cast<int>(model)))for(auto& path:paths)path.newAmp.set(static_cast<int>(model),nativeChannelValue);
    }
    int nativeChannel() const { return nativeChannelValue; }
    void setNative(const AmpNativeState& state) {
        useNativeControls=state.enabled;nativeMix.setTargetValue(useNativeControls?1.f:0.f);
        if(useNativeControls)for(auto& path:paths)path.setNative(state);
    }
    void setOversampling(int choice)
    {
        choice = juce::jlimit(0, 3, choice);
        if (choice == selected || fadeRemaining > 0) return;
        previous = selected; selected = choice;
        paths[selected].reset(); fadeRemaining = fadeLength;
    }
    int latency() const { return delaySamples; }
    void tone(float bass, float lowmid, float highmid, float treble, float presence, float resonance)
    {
        if(useNativeControls)return;
        float bf=80, lmf=450, hmf=1500, hf=4000, pf=2500, rf=90;
        if (model==AmpModel::ironTube) { bf=40; lmf=220; hmf=800; rf=70; }
        else if (model==AmpModel::solidPunch) { bf=60; lmf=250; hmf=1000; hf=5000; pf=4500; }
        else if (model==AmpModel::tight515) { bf=110; lmf=500; hmf=1200; hf=3800; pf=2000; rf=95; }
        else if (model==AmpModel::chime30) { bf=95; lmf=380; hmf=1800; hf=4500; pf=3200; rf=100; }
        else if (model==AmpModel::orangeCrown) { bf=90; lmf=320; hmf=1100; hf=3400; pf=2600; rf=85; }
        else if (model==AmpModel::bassmanValve) { bf=50; lmf=200; hmf=750; hf=3800; pf=3000; rf=55; }
        else if (model==AmpModel::subwayClean) { bf=40; lmf=250; hmf=1000; hf=6000; pf=4500; rf=60; }
        else if (model==AmpModel::matchChime) { bf=85; lmf=420; hmf=1350; hf=4300; pf=3700; rf=95; }
        else if (model==AmpModel::silkODS) { bf=70; lmf=500; hmf=1800; hf=3800; pf=2400; rf=90; }
        else if (model==AmpModel::tastePunch) { bf=30; lmf=250; hmf=800; hf=8000; pf=5000; rf=50; }
        // New designs retain the existing global six-band control contract;
        // this is not a claim that these are the original hardware tapers.
        else if (model==AmpModel::zutaCinder) { bf=90; lmf=430; hmf=1400; hf=4000; pf=3000; rf=94; }
        else if (model==AmpModel::ironCompact) { bf=100; lmf=500; hmf=1500; hf=3900; pf=2700; rf=125; }
        else if (model==AmpModel::fourChannel) { bf=85; lmf=400; hmf=1300; hf=3800; pf=2200; rf=82; }
        else if (model==AmpModel::classicTube) { bf=40; lmf=220; hmf=800; hf=4000; pf=2100; rf=48; }
        else if (model==AmpModel::sunMonolith) { bf=70; lmf=320; hmf=1200; hf=3500; pf=1900; rf=62; }
        else if (model==AmpModel::evilHarvest) { bf=95; lmf=500; hmf=1600; hf=4200; pf=3300; rf=89; }
        else if (model==AmpModel::hotLead) { bf=90; lmf=400; hmf=1500; hf=4000; pf=2700; rf=90; }
        else if (model==AmpModel::blueStorm) { bf=70; lmf=350; hmf=1200; hf=3400; pf=1800; rf=68; }
        using C = juce::dsp::IIR::ArrayCoefficients<float>;
        auto hz = [this](float f) { return juce::jmin(f, float(sr * .45)); };
        *lo.state=C::makeLowShelf(sr,hz(bf),.707f,juce::Decibels::decibelsToGain(bass));
        *lm.state=C::makePeakFilter(sr,hz(lmf),.8f,juce::Decibels::decibelsToGain(lowmid));
        *hm.state=C::makePeakFilter(sr,hz(hmf),.8f,juce::Decibels::decibelsToGain(highmid));
        *hi.state=C::makeHighShelf(sr,hz(hf),.707f,juce::Decibels::decibelsToGain(treble));
        *pres.state=C::makeHighShelf(sr,hz(pf),.8f,juce::Decibels::decibelsToGain(presence));
        *res.state=C::makePeakFilter(sr,hz(rf),1.1f,juce::Decibels::decibelsToGain(resonance));
    }
    void process(juce::AudioBuffer<float>& buffer, bool useFullRangeTone = true, bool ampEnabled = true)
    {
        dry.makeCopyOf(buffer,true);juce::dsp::AudioBlock<float> dryBlock(dry);juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);bypassDelay.process(dryContext);
        const auto& voice = ampVoices[(size_t)model];
        const bool extended = isNewAmpModel(static_cast<int>(model));
        const bool blendEngines=nativeMix.isSmoothing();
        if(blendEngines) {
            engineAlternate.makeCopyOf(buffer,true);
            if(useNativeControls) {
                if(extended)paths[selected].processNew(engineAlternate,drive);
                else paths[selected].process(engineAlternate,voice,drive);
                processLegacyTone(engineAlternate,useFullRangeTone);
            } else paths[selected].processNative(engineAlternate);
        }
        if (fadeRemaining > 0)
        {
            alternate.makeCopyOf(buffer, true);
            if(useNativeControls)paths[previous].processNative(alternate);
            else if(extended)paths[previous].processNew(alternate, drive);
            else paths[previous].process(alternate, voice, drive);
        }
        if(useNativeControls)paths[selected].processNative(buffer);
        else if(extended)paths[selected].processNew(buffer, drive);
        else paths[selected].process(buffer, voice, drive);
        for (int n = 0; n < buffer.getNumSamples() && fadeRemaining > 0; ++n, --fadeRemaining)
        {
            const float wet = 1.0f - float(fadeRemaining) / float(fadeLength);
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.setSample(c,n,wet*buffer.getSample(c,n)+(1.0f-wet)*alternate.getSample(c,n));
        }
        if(!useNativeControls)processLegacyTone(buffer,useFullRangeTone);
        if(blendEngines)for(int n=0;n<buffer.getNumSamples();++n) {
            const float nativeWeight=nativeMix.getNextValue();
            const float primaryWeight=useNativeControls?nativeWeight:1.f-nativeWeight;
            for(int c=0;c<buffer.getNumChannels();++c)
                buffer.setSample(c,n,primaryWeight*buffer.getSample(c,n)+(1.f-primaryWeight)*engineAlternate.getSample(c,n));
        }
        enabled.setTargetValue(ampEnabled ? 1.f : 0.f);
        for(int n=0;n<buffer.getNumSamples();++n) {const float mix=enabled.getNextValue();for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,buffer.getSample(c,n)*mix+dry.getSample(c,n)*(1-mix));}
    }
};
}

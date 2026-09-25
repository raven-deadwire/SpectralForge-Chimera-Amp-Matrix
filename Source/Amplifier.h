#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "AmpCatalog.h"

namespace spectralforge {
// Voiced nonlinear models, not component-level replicas or measured captures.
// Each row defines input/interstage coupling, bandwidth, bias and supply sag.
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
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> compensation{64};
        struct Channel { float inputLow{}, couplingLow{}, outputLow{}, dcLow{}, envelope{}; };
        std::array<Channel, 2> channels{};
        juce::SmoothedValue<float> drive;
        double rate{};
        void reset() { oversampler->reset(); compensation.reset(); channels = {}; drive.setCurrentAndTargetValue(drive.getTargetValue()); }
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
    std::array<Path, 4> paths;
    Filter lo, lm, hm, hi, pres, res,voiceLow,voiceMid,voiceHigh,captureLow,captureMid,captureHigh;
    juce::AudioBuffer<float> alternate, dry;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::None> bypassDelay{64};
    juce::SmoothedValue<float> enabled;
    double sr{48000};
    AmpModel model{AmpModel::glass};
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
            p.rate = sr * (1 << i);
            p.drive.reset(p.rate, .020);
            p.drive.setCurrentAndTargetValue(drive);
            delaySamples = juce::jmax(delaySamples, juce::roundToInt(p.oversampler->getLatencyInSamples()));
        }
        for (auto& p : paths)
        {
            p.compensation.prepare(spec);
            p.compensation.setDelay(float(delaySamples - juce::roundToInt(p.oversampler->getLatencyInSamples())));
        }
        bypassDelay.prepare(spec);bypassDelay.setDelay(float(delaySamples));
        enabled.reset(sr,.015);enabled.setCurrentAndTargetValue(1);
        dry.setSize((int)spec.numChannels,(int)spec.maximumBlockSize);
        alternate.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
        fadeLength = juce::jmax(1, int(sr * .020));
        tone(0,0,0,0,0,0);voiceTone();
        for (auto* f : {&lo, &lm, &hm, &hi, &pres, &res,&voiceLow,&voiceMid,&voiceHigh,&captureLow,&captureMid,&captureHigh}) f->prepare(spec);
        reset();
    }
    void reset()
    {
        for (auto* f : {&lo, &lm, &hm, &hi, &pres, &res,&voiceLow,&voiceMid,&voiceHigh,&captureLow,&captureMid,&captureHigh}) f->reset();
        for (auto& p : paths) if (p.oversampler) p.reset();
        previous = selected; fadeRemaining = 0; bypassDelay.reset();enabled.setCurrentAndTargetValue(enabled.getTargetValue());
    }
    void setDriveImmediately(float value) { drive=juce::jlimit(0.f,1.f,value);for(auto& path:paths)path.drive.setCurrentAndTargetValue(drive); }
    void set(AmpModel m, float d) {
        m = static_cast<AmpModel>(juce::jlimit(0, ampModelCount - 1, static_cast<int>(m)));
        if(model!=m){model=m;voiceTone();captureLow.reset();captureMid.reset();captureHigh.reset();} drive = juce::jlimit(0.f, 1.f, d);
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
        if (fadeRemaining > 0)
        {
            alternate.makeCopyOf(buffer, true);
            paths[previous].process(alternate, voice, drive);
        }
        paths[selected].process(buffer, voice, drive);
        for (int n = 0; n < buffer.getNumSamples() && fadeRemaining > 0; ++n, --fadeRemaining)
        {
            const float wet = 1.0f - float(fadeRemaining) / float(fadeLength);
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.setSample(c,n,wet*buffer.getSample(c,n)+(1.0f-wet)*alternate.getSample(c,n));
        }
        {juce::dsp::AudioBlock<float> block(buffer);juce::dsp::ProcessContextReplacing<float> context(block);voiceLow.process(context);voiceMid.process(context);voiceHigh.process(context);buffer.applyGain(.5f);}
        if(static_cast<int>(model)>=8 && static_cast<int>(model)<=13) {
            juce::dsp::AudioBlock<float> block(buffer);juce::dsp::ProcessContextReplacing<float> context(block);
            captureLow.process(context);captureMid.process(context);captureHigh.process(context);
        }
        if (useFullRangeTone)
        {
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            for (auto* f : {&lo, &lm, &hm, &hi, &res, &pres}) f->process(context);
        }
        enabled.setTargetValue(ampEnabled ? 1.f : 0.f);
        for(int n=0;n<buffer.getNumSamples();++n) {const float mix=enabled.getNextValue();for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,n,buffer.getSample(c,n)*mix+dry.getSample(c,n)*(1-mix));}
    }
};
}

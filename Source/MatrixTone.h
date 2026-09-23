#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace spectralforge {

// Audible-band reference points for the tilt, not additional crossover cuts.
inline float matrixTonePivot(int lane, float lowMid, float midHigh, double sampleRate)
{
    const auto top = juce::jmin(20000.0f, static_cast<float>(sampleRate * 0.45));
    const auto low = juce::jlimit(30.0f, top * 0.5f, lowMid);
    const auto high = juce::jlimit(low + 1.0f, top, midHigh);
    if (lane == 0) return std::sqrt(20.0f * low);
    if (lane == 1) return std::sqrt(low * high);
    return std::sqrt(high * top);
}

// A band-relative tilt placed AFTER splitting and BEFORE amp distortion.
// Positive values shift the band's balance toward its upper frequencies.
class MatrixTone {
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                 juce::dsp::IIR::Coefficients<float>>;
    using Coefficients = juce::dsp::IIR::ArrayCoefficients<float>;
    Filter low, high;
    double sampleRate{48000.0};
    juce::SmoothedValue<float> amount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> pivot;

    void update(float frequency, float tilt)
    {
        // Update the shared coefficient objects in place. Replacing state pointers
        // would leave ProcessorDuplicator's channel filters on stale coefficients.
        *low.state = Coefficients::makeLowShelf(sampleRate, frequency, 0.7071f,
                                              juce::Decibels::decibelsToGain(-tilt * 0.5f));
        *high.state = Coefficients::makeHighShelf(sampleRate, frequency, 0.7071f,
                                                juce::Decibels::decibelsToGain(tilt * 0.5f));
    }

public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        amount.reset(sampleRate, 0.025);
        pivot.reset(sampleRate, 0.025);
        amount.setCurrentAndTargetValue(0.0f);
        pivot.setCurrentAndTargetValue(1000.0f);
        update(1000.0f, 0.0f);
        low.prepare(spec);
        high.prepare(spec);
    }

    void reset()
    {
        low.reset(); high.reset();
        amount.setCurrentAndTargetValue(amount.getTargetValue());
        pivot.setCurrentAndTargetValue(pivot.getTargetValue());
    }

    void set(float frequency, float tilt)
    {
        pivot.setTargetValue(juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45), frequency));
        amount.setTargetValue(juce::jlimit(-12.0f, 12.0f, tilt));
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        for (size_t offset = 0; offset < block.getNumSamples(); offset += 32)
        {
            const auto count = juce::jmin(size_t{32}, block.getNumSamples() - offset);
            update(pivot.skip(static_cast<int>(count)), amount.skip(static_cast<int>(count)));
            auto part = block.getSubBlock(offset, count);
            juce::dsp::ProcessContextReplacing<float> context(part);
            low.process(context);
            high.process(context);
        }
    }
};
}

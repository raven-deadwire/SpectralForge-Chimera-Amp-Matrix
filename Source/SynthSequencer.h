#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// SYNTH: a step sequencer and four macros as modulation sources, and eight
// more matrix rows that know them and the newer targets
// (docs/SYNTH_SEQUENCER.md). Forty-eight parameters appended at version hint
// 40. The released matrix rows keep their lists: a longer list would change
// what their automation values mean.
namespace spectralforge {

inline constexpr int synthSequencerParameterVersion = 40;
inline constexpr int synthSeqMaxSteps = 16, synthMacroCount = 4, synthModSlotsB = 8;
inline constexpr int synthSequencerParameterCount = 4 + synthSeqMaxSteps + synthMacroCount + 3 * synthModSlotsB;

inline juce::StringArray synthSeqRateNames() { return {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T"}; }
// Beats (quarter notes) per step.
inline constexpr std::array<double, 8> synthSeqRateBeats{4., 2., 1., .5, .25, .125, 1. / 3., 1. / 6.};

// Sources and targets of the second row bank: the released lists (same
// numbers), then the new entries.
enum SynthModSourceB { modSeq = 8, modMacro1, modMacro2, modMacro3, modMacro4, synthModSourceCountB };
inline juce::StringArray synthModSourceNamesB() {
    return {"Off", "Env 2", "LFO 1", "LFO 2", "Velocity", "Guitar level", "Note", "Mod wheel", "Step seq", "Macro 1", "Macro 2", "Macro 3", "Macro 4"};
}
enum SynthModTargetB { targetCutoff2 = 18, targetResonance2, targetFilterBalance, targetDistDrive, targetDistMix, targetWarpA, targetWarpB, targetWarpC, synthModTargetCountB };
inline juce::StringArray synthModTargetNamesB() {
    return {"Off", "Pitch A", "Pitch B", "Pitch C", "Wave A", "Wave B", "Wave C", "Level A", "Level B", "Level C",
            "FM", "Cutoff", "Resonance", "Sub level", "Noise level", "Amp", "Detune", "Character",
            "Cutoff 2", "Resonance 2", "Filter balance", "Dist drive", "Dist mix", "Warp A", "Warp B", "Warp C"};
}
// Full-scale depth of each target (the released ones as in synthModDepth):
// cutoff 2 in octaves, the rest as a share of the control's range.
inline constexpr std::array<double, synthModTargetCountB> synthModDepthB{0, 24, 24, 24, 4, 4, 4, 1, 1, 1, 8, 6, 1, 1, 1, 1, 1, 1, 6, 1, 1, 1, 1, 1, 1, 1};

inline juce::String synthSeqStepID(int step) { return "synthSeqStep" + juce::String(step + 1); }
inline juce::String synthMacroID(int macro) { return "synthMacro" + juce::String(macro + 1); }
inline juce::String synthModBID(int slot, const char* field) { return "synthModB" + juce::String(slot + 1) + field; }

inline void appendSynthSequencerParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = synthSequencerParameterVersion;
    const auto percent = [&](const juce::String& id, const juce::String& name, float lo, float initial) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, v}, name, juce::NormalisableRange<float>{lo, 100.f, 1.f}, initial,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value)) + " %"; })));
    };
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"synthSeqSteps", v}, "Synth sequencer steps", 1, synthSeqMaxSteps, synthSeqMaxSteps));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"synthSeqRate", v}, "Synth sequencer step length", synthSeqRateNames(), 4));
    percent("synthSeqSmooth", "Synth sequencer smoothing", 0.f, 0.f);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"synthSeqRetrigger", v}, "Synth sequencer restarts on a new phrase", true));
    for (int step = 0; step < synthSeqMaxSteps; ++step) percent(synthSeqStepID(step), "Synth sequencer step " + juce::String(step + 1), -100.f, 0.f);
    for (int macro = 0; macro < synthMacroCount; ++macro) percent(synthMacroID(macro), "Synth macro " + juce::String(macro + 1), 0.f, 0.f);
    for (int slot = 0; slot < synthModSlotsB; ++slot) {
        const juce::String n = "Synth mod " + juce::String(slot + 9) + " ";
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{synthModBID(slot, "Source"), v}, n + "source", synthModSourceNamesB(), 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{synthModBID(slot, "Target"), v}, n + "target", synthModTargetNamesB(), 0));
        percent(synthModBID(slot, "Amount"), n + "amount", -100.f, 0.f);
    }
}
} // namespace spectralforge

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// External sidechain and the SYNTH bus (docs/SIDECHAIN_SYNTH_BUS.md).
// The plugin has an optional stereo "Sidechain" input bus and an optional
// stereo "Synth" output bus; both are off until the host enables them. The
// key toggles choose which detectors listen to the sidechain; with no
// sidechain connected every detector keeps its own input. Version hint 28.
namespace spectralforge {
inline constexpr int sidechainParameterVersion = 28;
inline juce::StringArray synthOutputNames() { return {"Main mix", "Synth bus", "Main + synth bus"}; }
inline void appendSidechainParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    constexpr int v = sidechainParameterVersion;
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"synthOutput", v}, "Synth output", synthOutputNames(), 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"scKeyComp", v}, "Clean compressor external key", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"scKeyGates", v}, "Gates external key", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"scKeyMultiband", v}, "Multiband external key", false));
}
inline constexpr int sidechainParameterCount = 4;
} // namespace spectralforge

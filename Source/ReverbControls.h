#pragma once

// What the POST reverb's added controls ask of the DSP (docs/POST_REVERB_CONTROLS.md).
// No JUCE types: SpatialModules.h is built by targets that link the DSP module only.
namespace spectralforge {
inline constexpr float reverbPreDelayMaxMs = 200.f, reverbLowCutMinHz = 20.f, reverbLowCutMaxHz = 500.f, reverbWidthMax = 2.f;

// What the audio thread reads for one slot's reverb.
struct ReverbControls {
    float preDelayMs{0.f};   // 0 = no pre-delay
    float lowCutHz{20.f};    // 20 Hz = off
    float tone{0.f};         // -1 dark .. +1 bright, 0 = flat
    float width{1.f};        // 0 mono .. 1 as made .. 2 wide
    bool freeze{false};      // hold the tail, take no new input
};

} // namespace spectralforge

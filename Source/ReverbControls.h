#pragma once
#include <cmath>
#include <limits>

// What the POST reverb's added controls ask of the DSP (docs/POST_REVERB_CONTROLS.md).
// No JUCE types: SpatialModules.h is built by targets that link the DSP module only.
namespace spectralforge {
inline constexpr float reverbPreDelayMaxMs = 200.f, reverbLowCutMinHz = 20.f, reverbLowCutMaxHz = 500.f, reverbWidthMax = 2.f;
// DECAY (docs/POST_REVERB_DECAY.md): below the minimum SIZE sets the decay, as it always did.
inline constexpr float reverbDecayMinSeconds = .2f, reverbDecayMaxSeconds = 20.f;

// What the audio thread reads for one slot's reverb.
struct ReverbControls {
    float preDelayMs{0.f};   // 0 = no pre-delay
    float lowCutHz{20.f};    // 20 Hz = off
    float tone{0.f};         // -1 dark .. +1 bright, 0 = flat
    float width{1.f};        // 0 mono .. 1 as made .. 2 wide
    bool freeze{false};      // hold the tail, take no new input
    // The tempo shape (docs/POST_REVERB_SHAPE.md): the reverb's level follows a shape that repeats with the tempo.
    int shape{0};            // reverbShapeOff .. reverbShapeDuck
    float shapeDepth{1.f};   // 0 = no effect .. 1 = the whole shape
    int shapeLength{2};      // index into reverbShapeBeats
    double tempo{120.};      // beats per minute
    double ppq{std::numeric_limits<double>::quiet_NaN()};   // the host's quarter-note position at the block's first frame while it plays, else NaN
    float decaySeconds{0.f}; // below reverbDecayMinSeconds: SIZE sets the decay; from there up: the time to fall 60 dB
};
enum ReverbShape {reverbShapeOff=0,reverbShapeRise,reverbShapeRewind,reverbShapeChop,reverbShapeTide,reverbShapeDuck,reverbShapeCount};
inline constexpr int reverbShapeLengthCount=4;
inline constexpr double reverbShapeBeats[reverbShapeLengthCount]{1.,2.,4.,8.};   // quarter notes: 1/4, 1/2, one bar and two bars of 4/4
// The level a shape asks for at `phase` (0 to 1) through its length, 0 to 1.
inline float reverbShapeLevel(int shape,double phase) noexcept {
    const double p=phase-std::floor(phase);
    switch(shape) {
        case reverbShapeRise:return float(p*p*(3.-2.*p));                        // rises smoothly through the length
        case reverbShapeRewind:return float((std::exp(4.*p)-1.)/(std::exp(4.)-1.));   // a decay played backwards, cut at the end
        case reverbShapeChop:return p<.5?1.f:0.f;                                  // open for the first half
        case reverbShapeTide:return float(.5-.5*std::cos(2.*3.14159265358979323846*p));   // one rise and fall
        case reverbShapeDuck:return float(1.-std::exp(-5.*p));                     // ducked at the start, recovering
        default:return 1.f;
    }
}

} // namespace spectralforge

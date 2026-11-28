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
    // More of the tempo shape (docs/POST_REVERB_SHAPE_MORE.md).
    bool shapeInSeconds{false};  // the pass is shapeSeconds long and runs freely, whatever the tempo
    float shapeSeconds{1.f};     // reverbShapeMinSeconds .. reverbShapeMaxSeconds
    float shapeCurve{0.f};       // -1 soft .. 0 as drawn .. +1 hard
    float shapeMorph{0.f};       // -1 .. +1: the reverb's tone follows the shape; 0 = not at all
    // Character (docs/POST_REVERB_CHARACTER.md): the sound of an early digital reverb, on the reverb only.
    int converter{0};        // reverbConverterOff .. reverbConverterLate: the bandwidth and sample rate of an older converter
    float dirt{0.f};         // 0 = none .. 1: fewer bits
    float drift{0.f};        // 0 = none .. 1: slow pitch drift
    // Shimmer (docs/POST_REVERB_SHIMMER.md): what the reverb hears gains a copy of itself moved up or down in pitch.
    float shimmer{0.f};      // 0 = none .. 1
    int shimmerInterval{0};  // index into reverbShimmerRatios
    float decaySeconds{0.f}; // below reverbDecayMinSeconds: SIZE sets the decay; from there up: the time to fall 60 dB
};
enum ReverbShape {reverbShapeOff=0,reverbShapeRise,reverbShapeRewind,reverbShapeChop,reverbShapeTide,reverbShapeDuck,reverbShapeCount};
inline constexpr int reverbShimmerIntervalCount=3;
inline constexpr double reverbShimmerRatios[reverbShimmerIntervalCount]{2.,1.5,.5};   // an octave up, a fifth up, an octave down
// The shifter's grain, the share of its output fed back into it at full (so octaves stack), and its low-pass.
inline constexpr double reverbShimmerGrainMs=60.,reverbShimmerFeedback=.45,reverbShimmerCutHz=7000.;
enum ReverbConverter {reverbConverterOff=0,reverbConverterEarly,reverbConverterMiddle,reverbConverterLate,reverbConverterCount};
// The low-pass and the sample rate each converter stands for. Off is not read.
inline constexpr double reverbConverterCutHz[reverbConverterCount]{20000.,6000.,9000.,13000.},reverbConverterRateHz[reverbConverterCount]{48000.,16000.,24000.,32000.};
// DIRT takes the word length from 16 bits down to 7.
inline double reverbDirtBits(float dirt) noexcept {return 16.-9.*double(dirt<0.f?0.f:dirt>1.f?1.f:dirt);}
// DRIFT moves the reverb's pitch by a delay that swings up to this far each way, around this far behind.
inline constexpr double reverbDriftSwingMs=1.5,reverbDriftBaseMs=2.,reverbDriftHz=.7;
inline constexpr float reverbShapeMinSeconds = .1f, reverbShapeMaxSeconds = 10.f;
// CURVE bends a shape's level around its middle: hard (above 0) makes it nearer a switch, soft (below 0)
// keeps it nearer the middle for longer. 0 and 1 stay where they are; at 0 the level is returned untouched.
inline float reverbShapeCurved(float level,float curve) noexcept {
    if(curve==0.f || level<=0.f || level>=1.f)return level;
    const double k=curve>0.f?1.+4.*double(curve):1./(1.-4.*double(curve)),up=std::pow(double(level),k),down=std::pow(1.-double(level),k);
    return float(up/(up+down));
}
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

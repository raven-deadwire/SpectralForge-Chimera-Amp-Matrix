#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace spectralforge {
// UI names describe original algorithms; references identify design influences.
struct ModelInfo { const char* name; const char* reference; const char* character; juce::uint32 colour; };
struct ModelFamily { const char* parameter; const char* category; int count; std::array<ModelInfo,6> models; };
inline constexpr std::array<ModelFamily,11> modelFamilies{{
 {"drivemodel","DRIVE",3,{{{"Green 808","Ibanez TS808","Mid-forward soft clipping",0xff28634d},{"Gold Drive","Klon Centaur","Clean blend / asymmetric drive",0xff937343},{"Rodent","Pro Co RAT","Hard clipping / variable low-pass",0xff333438}}}},
 {"delaymodel","DELAY",3,{{{"Digital 229","TC Electronic 2290","Clear repeats",0xff28353d},{"Tape Echo","Roland RE-201","Filtered repeats / wow and flutter",0xff254c40},{"Analog Memory","EHX Deluxe Memory Man","Dark repeats / bucket-brigade character",0xff514735}}}},
 {"reverbmodel","REVERB",3,{{{"Studio Plate","EMT 140","Dense plate diffusion",0xff655348},{"Concert Hall","Lexicon 224","Modulated long hall",0xff334254},{"Spring Tank","Fender spring reverb","Dispersive spring resonance",0xff514331}}}},
 {"compmodel","COMPRESSOR",3,{{{"Studio VCA","MXR M87 Bass Compressor","Linked RMS / controlled transients",0xff697377},{"Red OTA","MXR Dyna Comp","Fast detector / sustain",0xff8b3436},{"Optical","Diamond Compressor","Soft knee / program-dependent recovery",0xff907a3c}}}},
 {"filtermodel","ENVELOPE",3,{{{"Q Sweep","EHX Q-Tron","Upward low-pass envelope",0xff345b42},{"Tron Band","Mu-Tron III","Band-pass envelope",0xff65706a},{"Reverse Sweep","Mu-Tron III Down mode","Downward low-pass envelope",0xff4e5269}}}},
 {"fuzzmodel","FUZZ",3,{{{"Big Sustain","EHX Big Muff Pi","Cascaded clipping / dense sustain",0xff6b6b65},{"Round Face","Dunlop Fuzz Face","Asymmetric / volume-sensitive",0xff923d36},{"Bender","Sola Sound Tone Bender","Gated edge / sharp attack",0xff9c8753}}}},
 {"boostmodel","BOOST",3,{{{"RC Clean","Xotic RC Booster","Broad bass and treble shaping",0xff7b7b70},{"Treble Lift","Dallas Rangemaster","Low-cut / upper-mid emphasis",0xff496a68},{"Micro Lift","MXR Micro Amp","Full-band level lift",0xff787267}}}},
 {"busmodel","BUS COMP",3,{{{"Console VCA","SSL Bus Compressor","RMS / 6 dB knee",0xff414c54},{"FET 76","UREI 1176","Peak / fast attack and release",0xff34383c},{"Opto Level","Teletronix LA-2A","Soft knee / program-dependent recovery",0xff716e5f}}}},
 {"preampmodel","PREAMP",3,{{{"N73 Colour","Neve 1073","Class-A-inspired saturation / low-mid body",0xff354d64},{"V5 Pure","Avalon V5","High-headroom DI / gentle shaping",0xff75766e},{"ISA Blue","Focusrite ISA","Transformer-inspired presence / open top",0xff345685}}}},
 {"eqmodel","EQUALIZER",3,{{{"Console E","SSL E Series","Variable-Q mid / broad shelves",0xff414c54},{"N73 Shelves","Neve 1073","110 Hz / 12 kHz shelves",0xff354d64},{"Passive Tube","Pultec EQP-1A","Broad shelves / gentle mid contour",0xff586270}}}},
 {"modmodel","MODULATION",6,{{{"Classic Chorus","Boss CE-2","Single modulated delay",0xff477f97},{"Dimension","Roland Dimension D","Opposing stereo delay voices",0xff3c456b},{"Stone Phase","EHX Small Stone","Four swept allpass stages",0xff79635a},{"Mistress Flange","EHX Electric Mistress","Short delay / comb feedback",0xff517159},{"Eddy Vibrato","EHX Eddy","Pitch modulation / wet blend",0xff425883},{"Pulsar Tremolo","EHX Pulsar","Amplitude modulation / stereo motion",0xff71595c}}}}
}};
inline juce::StringArray modelNames(int family) {juce::StringArray names;const auto& f=modelFamilies[(size_t)family];for(int i=0;i<f.count;++i)names.add(f.models[(size_t)i].name);return names;}
inline const ModelInfo& modelInfo(int family,int model) {const auto& f=modelFamilies[(size_t)family];return f.models[(size_t)juce::jlimit(0,f.count-1,model)];}
}

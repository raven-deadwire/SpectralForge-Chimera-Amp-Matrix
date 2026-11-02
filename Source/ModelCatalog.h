#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace spectralforge {
// Public names describe Chimera processors. Reference fields are internal design
// provenance and must never be rendered or used as host-facing choice text.
struct ModelInfo { const char* name; const char* reference; const char* character; juce::uint32 colour; };
struct ModelFamily { const char* parameter; const char* category; int count; std::array<ModelInfo,6> models; };
inline constexpr std::array<ModelFamily,11> modelFamilies{{
 {"drivemodel","DRIVE",5,{{{"Green Drive","Ibanez TS808","Mid-forward soft clipping",0xff28634d},{"Gold Drive","Klon Centaur","Clean blend / asymmetric drive",0xff937343},{"Rodent","Pro Co RAT","Hard clipping / variable low-pass",0xff333438},{"Bass DI","Tech 21 SansAmp Bass Driver DI","Low-end blend / scooped tube-style drive",0xffb99a54},{"Micro Bass","Darkglass Microtubes B3K","Clean lows / aggressive upper-band clipping",0xff44464b}}}},
 {"delaymodel","DELAY",3,{{{"Precision Delay","TC Electronic 2290","Clear repeats",0xff28353d},{"Tape Echo","Roland RE-201","Filtered repeats / wow and flutter",0xff254c40},{"Analog Echo","EHX Deluxe Memory Man","Dark repeats / bucket-brigade character",0xff514735}}}},
 {"reverbmodel","REVERB",3,{{{"Studio Plate","EMT 140","Dense plate diffusion",0xff655348},{"Concert Hall","Lexicon 224","Modulated long hall",0xff334254},{"Spring Tank","Fender spring reverb","Dispersive spring resonance",0xff514331}}}},
 {"compmodel","COMPRESSOR",5,{{{"Studio VCA","MXR M87 Bass Compressor","Linked RMS / controlled transients",0xff697377},{"Red OTA","MXR Dyna Comp","Fast detector / sustain",0xff8b3436},{"Optical","Diamond Compressor","Soft knee / program-dependent recovery",0xff907a3c},{"Studio FET","Origin Effects Cali76 / 1176","Fast peak control / parallel FET response",0xff8c8d88},{"Variable Mu","Manley Variable Mu","Progressive ratio / dual recovery",0xff665078}}}},
 {"filtermodel","ENVELOPE",5,{{{"Q Sweep","EHX Q-Tron","Upward low-pass envelope",0xff345b42},{"Band Sweep","Mu-Tron III","Band-pass envelope",0xff65706a},{"Reverse Sweep","Mu-Tron III Down mode","Downward low-pass envelope",0xff4e5269},{"Bass Envelope","MXR M82 Bass Envelope Filter","Band-pass sweep / retained dry lows",0xff34799e},{"Dynamic Wah","Boss AW-3 Dynamic Wah","Vocal wah band / quick tracking",0xffbab9ad}}}},
 {"fuzzmodel","FUZZ",5,{{{"Big Sustain","EHX Big Muff Pi","Cascaded clipping / dense sustain",0xff6b6b65},{"Round Fuzz","Dunlop Fuzz Face","Asymmetric / volume-sensitive",0xff923d36},{"Vintage Fuzz","Sola Sound Tone Bender","Gated edge / sharp attack",0xff9c8753},{"Wool Bass","ZVEX Woolly Mammoth","Bass-forward gated fuzz",0xff8da87a},{"Gated Fuzz","ZVEX Fuzz Factory","Gated asymmetric texture",0xffb4a04f}}}},
 {"boostmodel","BOOST",5,{{{"Clean Lift","Xotic RC Booster","Broad bass and treble shaping",0xff7b7b70},{"Treble Lift","Dallas Rangemaster","Low-cut / upper-mid emphasis",0xff496a68},{"Micro Lift","MXR Micro Amp","Full-band level lift",0xff787267},{"Echo Lift","Xotic EP Booster","Low-mid weight / softened top",0xff675535},{"Linear Power","EHX LPB-1","Broadband transistor-style boost",0xffb17b3c}}}},
 {"busmodel","BUS COMP",3,{{{"Console VCA","SSL Bus Compressor","RMS / 6 dB knee",0xff414c54},{"Studio FET","UREI 1176","Peak / fast attack and release",0xff34383c},{"Opto Level","Teletronix LA-2A","Soft knee / program-dependent recovery",0xff716e5f}}}},
 {"preampmodel","PREAMP",3,{{{"Console Colour","Neve 1073","Class-A-inspired saturation / low-mid body",0xff354d64},{"Pure DI","Avalon V5","High-headroom DI / gentle shaping",0xff75766e},{"Transformer Blue","Focusrite ISA","Transformer-inspired presence / open top",0xff345685}}}},
 {"eqmodel","EQUALIZER",3,{{{"Console EQ","SSL E Series","Variable-Q mid / broad shelves",0xff414c54},{"Colour Shelves","Neve 1073","110 Hz / 12 kHz shelves",0xff354d64},{"Passive Tube","Pultec EQP-1A","Broad shelves / gentle mid contour",0xff586270}}}},
 {"modmodel","MODULATION",6,{{{"Classic Chorus","Boss CE-2","Single modulated delay",0xff477f97},{"Spatial Chorus","Roland Dimension D","Opposing stereo delay voices",0xff3c456b},{"Stone Phase","EHX Small Stone","Four swept allpass stages",0xff79635a},{"Tidal Flange","EHX Electric Mistress","Short delay / comb feedback",0xff517159},{"Drift Vibrato","EHX Eddy","Pitch modulation / wet blend",0xff425883},{"Pulse Tremolo","EHX Pulsar","Amplitude modulation / stereo motion",0xff71595c}}}}
}};
// Host choices keep their released counts: modelNames() feeds the released
// AudioParameterChoice lists and must never grow.
inline juce::StringArray modelNames(int family) {juce::StringArray names;const auto& f=modelFamilies[(size_t)family];for(int i=0;i<f.count;++i)names.add(f.models[(size_t)i].name);return names;}
// 1.8 POST motion/space (docs/POST_MOTION_SPACE_1_8.md): three models appended
// to DELAY, REVERB and MODULATION, selected through an appended *Ext parameter.
inline constexpr int effectExtensionParameterVersion=23, effectExtensionModels=3, effectExtensionParameterCount=3;
inline constexpr std::array<std::array<ModelInfo,effectExtensionModels>,3> effectExtensionCatalog{{
 {{{"Tap Cloud","Eventide UltraTap","Six spread taps / taper and spread",0xff3e6b7a},
   {"Bloom Delay","Eventide Rose","Modulated repeats / drifting time",0xff8a4b6a},
   {"Dual Digital","Strymon DIG","Two linked delay lines / clean repeats",0xff2f4f68}}},
 {{{"Hall Prime","Bricasti M7 reference","Dense modulated hall / smooth decay",0xff3a4a66},
   {"Void Space","Eventide Blackhole","Very long dark decay / wide modulation",0xff221c38},
   {"Gated Room","Non-linear gated reverb reference","Dense burst / abrupt close",0xff6b5a3a}}},
 {{{"Spin Cabinet","Leslie rotary speaker","Horn and drum rotation / Doppler and tremolo",0xff7a5a3a},
   {"Throb Vibe","Uni-Vibe","Four lamp-swept phase stages",0xff5a3a6b},
   {"Ring Bell","Ring modulator","Carrier multiply / bell-like sidebands",0xff6b6b3a}}}
}};
inline constexpr int effectExtensionSlot(int family) noexcept {return family==1?0:family==2?1:family==10?2:-1;}
inline constexpr bool effectFamilyExtended(int family) noexcept {return effectExtensionSlot(family)>=0;}
inline int effectSelectableCount(int family) {return modelFamilies[(size_t)family].count+(effectFamilyExtended(family)?effectExtensionModels:0);}
inline juce::String effectExtensionID(int family) {return juce::String(modelFamilies[(size_t)family].parameter)+"Ext";}
// Extension 0 defers to the released choice; 1-3 select the appended models.
inline int effectSelectedModel(int family,float choice,float extension) noexcept {
    const int count=modelFamilies[(size_t)family].count;
    const int ext=std::isfinite(extension)?juce::roundToInt(extension):0;
    if(effectFamilyExtended(family) && ext>0)return count+juce::jlimit(1,effectExtensionModels,ext)-1;
    return juce::jlimit(0,count-1,std::isfinite(choice)?juce::roundToInt(choice):0);
}
inline const ModelInfo& modelInfo(int family,int model) {
    const auto& f=modelFamilies[(size_t)family];
    if(effectFamilyExtended(family) && model>=f.count)
        return effectExtensionCatalog[(size_t)effectExtensionSlot(family)][(size_t)juce::jlimit(0,effectExtensionModels-1,model-f.count)];
    return f.models[(size_t)juce::jlimit(0,f.count-1,model)];
}
}

#pragma once
#include "OriginalCabParameters.h"
#include <array>

namespace spectralforge {
// Release 1.3 factory recalls use authored cabinet / speaker / microphone
// recipes. These are model starting points, not measured hardware captures.
// Stable preset ordinals and user project state are deliberately independent:
// only the four factory snapshot builders call voiceFactoryCab().
enum class FactoryCabTone {
    dry, dryBass, glass112, bell212, chime212, crunch212, rhythm412, wide412,
    dark412, lead212, clean212, steel412, nocturne412, ruin412,
    round115, compact210, punch410, clean410, grind610, modern610,
    clarity112, depth212, alloy410, impact610, count
};
struct FactoryCabSpec {
    int layout, driver, mic, bodyMic, rear, unit;
    float position, distanceCm, tweeter;
};
// Layout/driver/microphone values are the existing append-only host ordinals.
// A 6x10 pickup uses the middle row (unit 3), not a removed 8x10 bottom unit.
inline constexpr std::array<FactoryCabSpec,size_t(FactoryCabTone::count)> factoryCabSpecs{{
    {1, 5,14,10,1,0,.38f,8,0},    // dry: an editable parked cabinet, bypassed
    {9,14, 5,16,0,0,.30f,6,0},    // dry bass: parked 2x12 / Depth, bypassed
    {1, 5,14,10,1,0,.38f,8,0},    // Glass: open 1x12 / Silver / Prism
    {2, 5,15,10,1,0,.28f,8,0},    // Bell: open 2x12 / Silver / Pencil
    {2, 3, 3,11,1,0,.32f,8,0},    // Chime: open 2x12 / Verdant / Spear
    {2, 3, 1,11,0,0,.38f,6,0},    // Crunch: 2x12 / Verdant / Needle
    {3, 1, 1,10,0,0,.35f,6,0},    // Rhythm: 4x12 / Ember / Needle
    {3, 4, 2,10,0,0,.40f,8,0},    // Wide: 4x12 / Granite / Hammer
    {3, 6, 6,10,0,0,.42f,7,0},    // Dark: 4x12 / Crimson / Veil
    {2, 1,11, 1,0,0,.30f,8,0},    // Lead: 2x12 / Ember / Focus
    {2, 5,14,10,1,0,.38f,8,0},    // Ambient: open 2x12 / Silver / Prism
    {3, 2, 1,11,0,0,.32f,6,0},    // Steel: 4x12 / Steel / Needle
    {3, 7, 1,10,0,0,.35f,6,0},    // Nocturne: 4x12 / Nocturne / Needle
    {3, 8,20, 2,0,0,.36f,6,0},    // Ruin: 4x12 / Ruin / Strike
    {4,13, 5,16,0,0,.38f,6,0},    // Round: 1x15 / Monolith / Anchor
    {5, 9, 5, 2,0,0,.35f,6,0},    // Compact: 2x10 / Foundry / Anchor
    {6,12, 1, 5,0,0,.28f,6,.06f}, // Pick: 4x10 / Alloy / Needle
    {6,10, 5,14,0,0,.30f,6,.10f}, // Clean: 4x10 / Vector / Anchor
    {7, 9, 2, 5,0,2,.25f,6,0},    // Grind: 6x10 / Foundry / Hammer
    {7,10, 3, 5,0,2,.30f,6,.08f}, // Modern: 6x10 / Vector / Spear
    {8,11,14, 5,0,0,.25f,6,.06f}, // Clarity: 1x12 / Clarity / Prism
    {9,14, 5,16,0,0,.30f,6,.04f}, // Depth: 2x12 / Depth / Anchor
    {6,12, 3, 5,0,0,.25f,6,.08f}, // Alloy: 4x10 / Alloy / Spear
    {7, 9, 5, 2,0,2,.28f,6,0},    // Impact: 6x10 / Foundry / Anchor
}};
using FactoryCabRig=std::array<FactoryCabTone,3>;
inline constexpr std::array<FactoryCabRig,48> factoryCabRigs{{
    {FactoryCabTone::glass112},                                                   //  0 Clean Sustain
    {FactoryCabTone::rhythm412},                                                  //  1 Tight Rhythm
    {FactoryCabTone::dryBass,FactoryCabTone::modern610,FactoryCabTone::punch410},   //  2 Bass Matrix
    {FactoryCabTone::lead212},                                                    //  3 Filter Lead
    {FactoryCabTone::dark412},                                                    //  4 Fuzz Texture
    {FactoryCabTone::bell212},                                                    //  5 Bell Clean
    {FactoryCabTone::chime212},                                                   //  6 Edge Chime
    {FactoryCabTone::crunch212},                                                  //  7 Classic Crunch
    {FactoryCabTone::dark412},                                                    //  8 Orange Heavy
    {FactoryCabTone::rhythm412},                                                  //  9 Melodic Death Rhythm
    {FactoryCabTone::lead212},                                                    // 10 Melodic Lead
    {FactoryCabTone::clean212},                                                   // 11 Ambient Clean
    {FactoryCabTone::round115},                                                   // 12 Finger Round
    {FactoryCabTone::punch410},                                                   // 13 Pick Punch
    {FactoryCabTone::clean410},                                                   // 14 Slap Studio
    {FactoryCabTone::grind610},                                                   // 15 Modern Grind
    {FactoryCabTone::round115},                                                   // 16 Vintage Bass DI
    {FactoryCabTone::grind610},                                                   // 17 Wool Bass Fuzz
    {FactoryCabTone::compact210},                                                 // 18 Bass Envelope
    {FactoryCabTone::glass112,FactoryCabTone::crunch212},                          // 19 G+G Clean / Crunch
    {FactoryCabTone::rhythm412,FactoryCabTone::wide412},                           // 20 G+G Tight / Wide
    {FactoryCabTone::modern610,FactoryCabTone::rhythm412},                         // 21 G+B Low Anchor
    {FactoryCabTone::round115,FactoryCabTone::chime212},                           // 22 G+B Air / Weight
    {FactoryCabTone::round115,FactoryCabTone::clean410},                           // 23 B+B Warm / Definition
    {FactoryCabTone::depth212,FactoryCabTone::grind610},                           // 24 B+B Clean / Grind
    {FactoryCabTone::dryBass,FactoryCabTone::grind610,FactoryCabTone::clean410},    // 25 Low B Foundation
    {FactoryCabTone::dryBass,FactoryCabTone::grind610,FactoryCabTone::punch410},    // 26 Pick Attack Matrix
    {FactoryCabTone::dryBass,FactoryCabTone::chime212,FactoryCabTone::dark412},     // 27 Spectral Texture
    {FactoryCabTone::chime212},                                                   // 28 Matchless Edge
    {FactoryCabTone::lead212},                                                    // 29 Dumble Smooth Lead
    {FactoryCabTone::clarity112},                                                 // 30 Modern Clean
    {FactoryCabTone::depth212,FactoryCabTone::grind610,FactoryCabTone::nocturne412},// 31 Crom Cruach
    {FactoryCabTone::clean410,FactoryCabTone::alloy410,FactoryCabTone::nocturne412},// 32 Wild Hunt
    {FactoryCabTone::impact610,FactoryCabTone::grind610,FactoryCabTone::steel412},  // 33 Azhi Dahaka
    {FactoryCabTone::lead212,FactoryCabTone::rhythm412},                           // 34 A Path To Alsatia
    {FactoryCabTone::rhythm412,FactoryCabTone::steel412},                          // 35 Feel My Wrath
    {FactoryCabTone::dry,FactoryCabTone::rhythm412,FactoryCabTone::nocturne412},    // 36 Blackhearted
    {FactoryCabTone::dry,FactoryCabTone::steel412,FactoryCabTone::nocturne412},     // 37 Dark Matters of Throne
    {FactoryCabTone::ruin412},                                                    // 38 Thall Rhythm
    {FactoryCabTone::lead212},                                                    // 39 Molten Lead
    {FactoryCabTone::dark412,FactoryCabTone::ruin412},                             // 40 Rotten Grind
    {FactoryCabTone::wide412},                                                    // 41 Sludge Mass
    {FactoryCabTone::ruin412,FactoryCabTone::dark412,FactoryCabTone::nocturne412},  // 42 Slam Impact
    {FactoryCabTone::clean410},                                                   // 43 Frostline Precision
    {FactoryCabTone::grind610},                                                   // 44 Carrion Barrage
    {FactoryCabTone::alloy410},                                                   // 45 Foundry Pulse
    {FactoryCabTone::impact610},                                                  // 46 Jotunn Hammer
    {FactoryCabTone::round115},                                                   // 47 Mirebound Monolith
}};
// Visible mic-stage makeup, after the amp's distortion. Per-rig calibration is
// measured through the full processor; global OUTPUT remains at 0 dB.
inline constexpr std::array<std::array<float,3>,48> factoryCabLevelDb{{
    {1.7f,0.f,0.f}, // 0: Clean Sustain
    {-1.2f,0.f,0.f}, // 1: Tight Rhythm
    {0.f,3.7f,3.7f}, // 2: Bass Matrix
    {-3.6f,0.f,0.f}, // 3: Filter Lead
    {-9.6f,0.f,0.f}, // 4: Fuzz Texture
    {0.3f,0.f,0.f}, // 5: Bell Clean
    {-0.9f,0.f,0.f}, // 6: Edge Chime
    {-5.6f,0.f,0.f}, // 7: Classic Crunch
    {2.4f,0.f,0.f}, // 8: Orange Heavy
    {2.3f,0.f,0.f}, // 9: Melodic Death Rhythm
    {-5.7f,0.f,0.f}, // 10: Melodic Lead
    {1.6f,0.f,0.f}, // 11: Ambient Clean
    {9.5f,0.f,0.f}, // 12: Finger Round
    {3.6f,0.f,0.f}, // 13: Pick Punch
    {5.8f,0.f,0.f}, // 14: Slap Studio
    {-0.7f,0.f,0.f}, // 15: Modern Grind
    {10.4f,0.f,0.f}, // 16: Vintage Bass DI
    {0.6f,0.f,0.f}, // 17: Wool Bass Fuzz
    {6.8f,0.f,0.f}, // 18: Bass Envelope
    {-9.0f,-9.0f,0.f}, // 19: G+G Clean / Crunch
    {2.9f,2.9f,0.f}, // 20: G+G Tight / Wide
    {-1.5f,-1.5f,0.f}, // 21: G+B Low Anchor
    {7.1f,7.1f,0.f}, // 22: G+B Air / Weight
    {9.9f,9.9f,0.f}, // 23: B+B Warm / Definition
    {4.7f,4.7f,0.f}, // 24: B+B Clean / Grind
    {0.f,0.7f,0.7f}, // 25: Low B Foundation
    {0.f,1.9f,1.9f}, // 26: Pick Attack Matrix
    {0.f,3.7f,3.7f}, // 27: Spectral Texture
    {-0.7f,0.f,0.f}, // 28: Matchless Edge
    {-5.7f,0.f,0.f}, // 29: Dumble Smooth Lead
    {9.8f,0.f,0.f}, // 30: Modern Clean
    {4.3f,4.3f,4.3f}, // 31: Crom Cruach
    {6.2f,6.2f,6.2f}, // 32: Wild Hunt
    {4.7f,4.7f,4.7f}, // 33: Azhi Dahaka
    {-1.4f,-1.4f,0.f}, // 34: A Path To Alsatia
    {-7.3f,-7.3f,0.f}, // 35: Feel My Wrath
    {0.f,-4.8f,-4.8f}, // 36: Blackhearted
    {0.f,-4.1f,-4.1f}, // 37: Dark Matters of Throne
    {-3.0f,0.f,0.f}, // 38: Thall Rhythm
    {-6.5f,0.f,0.f}, // 39: Molten Lead
    {-5.7f,-5.7f,0.f}, // 40: Rotten Grind
    {-13.7f,0.f,0.f}, // 41: Sludge Mass
    {-5.0f,-5.0f,-5.0f}, // 42: Slam Impact
    {4.8f,0.f,0.f}, // 43: Frostline Precision
    {4.4f,0.f,0.f}, // 44: Carrion Barrage
    {5.6f,0.f,0.f}, // 45: Foundry Pulse
    {3.3f,0.f,0.f}, // 46: Jötunn Hammer
    {10.1f,0.f,0.f}, // 47: Mirebound Monolith
}};
inline constexpr const char* factoryCabPolicy="chimera-modeled-cab-v3; factory-voicing-1.3";

template<class Getter,class Setter>
void voiceFactoryCab(int index,Getter get,Setter set) {
    if(index<0 || index>=int(factoryCabRigs.size()))return;
    const int mode=int(get("mode"));const int lanes=mode==0?1:mode==1?2:3;
    for(int lane=0;lane<3;++lane) {
        const auto suffix=juce::String(lane+1);
        const auto tone=factoryCabRigs[size_t(index)][size_t(lane)];
        const auto& cab=factoryCabSpecs[size_t(tone)];
        const bool active=lane<lanes && tone!=FactoryCabTone::dry && tone!=FactoryCabTone::dryBass;
        const bool bass=cabLayout::layouts[size_t(cab.layout-1)].bass;
        // Source ordinals stay unchanged for host/user IR compatibility. A/B
        // files are not resolved or loaded by a factory recall.
        set("cab"+suffix,active?1.f:0.f);set("cabtype"+suffix,0);
        set("cabBtype"+suffix,0);set("cabblend"+suffix,0);
        set("cabBlow"+suffix,get("cablow"+suffix));set("cabBhigh"+suffix,get("cabhigh"+suffix));
        set(originalCabID(lane,"design"),bass?1.f:0.f);
        set(originalCabID(lane,"rear"),float(cab.rear));
        set(originalCabID(lane,"tweeter"),cab.tweeter);
        set(cabLayoutID(lane,"layout"),float(cab.layout));
        set(cabExpansionID(lane,"driver"),float(cab.driver));
        set(cabExpansionID(lane,"tweeter"),bass?2.f:0.f);
        for(int slot=0;slot<2;++slot) {
            const auto letter=juce::String(slot?"B":"A");
            const auto mic=slot?cab.bodyMic:cab.mic;
            set(originalCabID(lane,(letter+"on").toRawUTF8()),!slot&&active?1.f:0.f);
            set(originalCabID(lane,(letter+"mic").toRawUTF8()),float(cabExpansion::microphones[size_t(mic-1)].kind));
            set(originalCabID(lane,(letter+"unit").toRawUTF8()),float(std::min(cab.unit,3)));
            set(originalCabID(lane,(letter+"position").toRawUTF8()),slot?.5f:cab.position);
            set(originalCabID(lane,(letter+"distance").toRawUTF8()),slot?10.f:cab.distanceCm);
            set(cabLayoutID(lane,(letter+"unit").toRawUTF8()),float(cab.unit));
            set(cabExpansionID(lane,(letter+"mic").toRawUTF8()),float(mic));
            set("cab"+letter+"gain"+suffix,active?factoryCabLevelDb[size_t(index)][size_t(lane)]:0.f);
            set("cab"+letter+"delay"+suffix,0);set("cab"+letter+"invert"+suffix,0);
        }
    }
}
} // namespace spectralforge

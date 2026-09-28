#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace spectralforge {
// Append new voices: raw project indices 0-7 are the original beta voices.
enum class AmpModel : int {
    glass, britEdge, tight515, wideRect, liquidLead, ironTube, solidPunch, modernBass,
    chime30, orangeCrown, bassmanValve, subwayClean, matchChime, silkODS, tastePunch, count
};
inline constexpr int ampModelCount = static_cast<int>(AmpModel::count);
struct AmpInfo { const char* name; const char* reference; const char* character; bool bass; };
inline constexpr std::array<AmpInfo, ampModelCount> ampCatalog{{
    {"Glass", "Fender '65 Twin Reverb", "Open clean / bright American voicing", false},
    {"Brit Edge", "Marshall JTM45", "Dynamic British edge / soft breakup", false},
    {"Tight 515", "Peavey 6505 / 5150 family", "Tight lows / focused high gain", false},
    {"Wide Rect", "Mesa Dual Rectifier", "Broad lows / saturated modern rhythm", false},
    {"Liquid Lead", "Mesa Mark IV Lead", "Focused mids / sustained lead", false},
    {"Iron Tube", "Ampeg SVT-VR", "Weighty low mids / soft tube drive", true},
    {"Solid Punch", "Gallien-Krueger 800RB", "Fast bass attack / clear upper mids", true},
    {"Modern Bass", "Darkglass B7K Ultra + Aguilar DB751", "Blended bass drive / reference chain", true},
    {"Chime 30", "VOX AC30 Top Boost", "Bright edge breakup / responsive upper mids", false},
    {"Orange Crown", "Orange Rockerverb 50 MKIII", "Dense low mids / thick high gain", false},
    {"Bassman Valve", "Fender Super Bassman", "Rounded tube bass / retained fundamentals", true},
    {"Subway Clean", "Mesa Subway D-800+", "High-headroom bass / broad clean bandwidth", true},
    {"Match Chime", "Matchless DC-30", "Complex chime / firm mids and responsive breakup", false},
    {"Silk ODS", "Dumble Overdrive Special", "Smooth singing drive / rounded high mids", false},
    {"Taste Punch", "EICH T900", "Open bass dynamics / clear mids and extended lows", true}
}};
// Browse tags describe current voicing use, not newly implemented hardware
// channels. They are separate from the stable 15-value host enumeration above.
enum class AmpRole : unsigned { any=0, clean=1, crunch=2, lead=4, highGain=8, bass=16 };
enum class AmpInstrument { any, guitar, bass };
constexpr unsigned roleBits(AmpRole role) { return static_cast<unsigned>(role); }
constexpr unsigned operator|(AmpRole left,AmpRole right) { return roleBits(left)|roleBits(right); }
constexpr unsigned operator|(unsigned left,AmpRole right) { return left|roleBits(right); }
struct AmpRoleInfo { AmpRole role; const char* label; };
inline constexpr std::array<AmpRoleInfo,5> ampRoleChoices{{
    {AmpRole::clean,"Clean"},{AmpRole::crunch,"Crunch"},{AmpRole::lead,"Lead"},
    {AmpRole::highGain,"High-Gain"},{AmpRole::bass,"Bass"}
}};
inline constexpr std::array<unsigned,ampModelCount> ampRoleTags{{
    roleBits(AmpRole::clean),                                      // Glass
    AmpRole::clean|AmpRole::crunch,                               // Brit Edge
    AmpRole::lead|AmpRole::highGain,                              // Tight 515
    roleBits(AmpRole::highGain),                                  // Wide Rect
    AmpRole::lead|AmpRole::highGain,                              // Liquid Lead
    AmpRole::clean|AmpRole::crunch|AmpRole::bass,                  // Iron Tube
    AmpRole::clean|AmpRole::crunch|AmpRole::bass,                  // Solid Punch
    AmpRole::crunch|AmpRole::highGain|AmpRole::bass,               // Modern Bass
    AmpRole::clean|AmpRole::crunch,                               // Chime 30
    AmpRole::crunch|AmpRole::lead|AmpRole::highGain,               // Orange Crown
    AmpRole::clean|AmpRole::crunch|AmpRole::bass,                  // Bassman Valve
    AmpRole::clean|AmpRole::bass,                                 // Subway Clean
    AmpRole::clean|AmpRole::crunch,                               // Match Chime
    AmpRole::clean|AmpRole::crunch|AmpRole::lead,                  // Silk ODS
    AmpRole::clean|AmpRole::bass                                  // Taste Punch
}};
constexpr bool ampMatches(int index,AmpRole role=AmpRole::any,AmpInstrument instrument=AmpInstrument::any) {
    if(index<0 || index>=ampModelCount)return false;
    const auto& info=ampCatalog[static_cast<size_t>(index)];
    return (role==AmpRole::any || (ampRoleTags[static_cast<size_t>(index)]&roleBits(role))!=0)
        && (instrument==AmpInstrument::any || (instrument==AmpInstrument::bass ? info.bass : !info.bass));
}
constexpr int ampIndexFromMenuId(int id) { return id>=1 && id<=ampModelCount ? id-1 : -1; }
struct PendingAmpInfo { const char* name; const char* scope; bool required; };
inline constexpr std::array<PendingAmpInfo,8> pendingAmpTargets{{
    {"ZUTA GBG120","CH1-4 / DSP pending",true},
    {"ENGL","Ironball E606 study; Savage 120 Mk II separate candidate",true},
    {"Diezel VH4","Head/channel scope unresolved / DSP pending",true},
    {"Ampeg SVT-CL","Separate from SVT-VR / DSP pending",true},
    {"SUNN","Model T first study; exact generation unresolved",true},
    {"Fortin Evil Pumpkin","Candidate / DSP pending",false},
    {"Soldano SLO-100 LTD OD","OD reference only; other channels unresolved",false},
    {"Bogner Uberschall Rev Blue","Clean/Lead source review / DSP pending",false}
}};
inline const AmpInfo& ampInfo(int index) { return ampCatalog[(size_t)juce::jlimit(0, ampModelCount - 1, index)]; }
inline juce::StringArray ampNames() { juce::StringArray names; for (const auto& info : ampCatalog) names.add(info.name); return names; }
}

#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace spectralforge {
// Append new voices: raw project indices 0-7 are the original beta voices.
enum class AmpModel : int {
    glass, britEdge, tight515, wideRect, liquidLead, ironTube, solidPunch, modernBass,
    chime30, orangeCrown, bassmanValve, subwayClean, matchChime, silkODS, tastePunch,
    zutaCinder, ironCompact, fourChannel, classicTube, sunMonolith, evilHarvest, hotLead, blueStorm, specialEdition, nastrond, count
};
inline constexpr int legacyAmpModelCount = 15;
inline constexpr int ampModelCount = static_cast<int>(AmpModel::count);
// Preserve every released model/automation identity. E670FE is native-only;
// Ironball remains recallable but is retired from the active selection menu.
inline constexpr int releasedNativeAmpModelCount = 23;
inline constexpr int firstOriginalAmpModel = static_cast<int>(AmpModel::nastrond);
inline constexpr int legacyHiddenAmpIndex = static_cast<int>(AmpModel::ironCompact);
inline constexpr int activeAmpModelCount = ampModelCount - 1;
constexpr bool ampIsActive(int model) { return model>=0 && model<ampModelCount && model!=legacyHiddenAmpIndex; }
constexpr bool ampRequiresNative(int model) { return model>=releasedNativeAmpModelCount && model<ampModelCount; }
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
    {"Vintage Valve", "Fender Super Bassman", "Rounded tube bass / retained fundamentals", true},
    {"Metro Clean", "Mesa Subway D-800+", "High-headroom bass / broad clean bandwidth", true},
    {"Prism Chime", "Matchless DC-30", "Complex chime / firm mids and responsive breakup", false},
    {"Silk Lead", "Dumble Overdrive Special", "Smooth singing drive / rounded high mids", false},
    {"Taste Punch", "EICH T900", "Open bass dynamics / clear mids and extended lows", true},
    {"Cinder 120", "ZUTA GBG120", "Four-channel drive / layered saturation", false},
    {"Iron Compact", "ENGL Ironball E606", "Clean and lead / tight attack", false},
    {"Fourfold", "Diezel VH4", "Four channels / controlled low end", false},
    {"Classic Tube", "Ampeg SVT-CL", "Single-channel tube bass / firm fundamentals", true},
    {"Monolith", "SUNN Model T", "Normal and bright input paths / broad breakup", false},
    {"Night Harvest", "Fortin Evil Pumpkin", "Two gain paths and clean / aggressive attack", false},
    {"Hot Lead", "Soldano SLO-100", "Overdrive voice / singing sustain", false},
    {"Blue Storm", "Bogner Uberschall Rev Blue", "Clean and lead / dense low mids", false},
    {"Special Edition", "ENGL E670FE Founders Edition", "Five paths / Modern and Classic voicing", false},
    {"Náströnd", "SpectralForge Original", "Five-character high gain / tight attack and broad sustain", false}
}};
// Search metadata stays separate from the single primary category shown in the menu.
// The original host parameter continues to use exactly 15 choices.
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
    AmpRole::clean|AmpRole::bass,                                 // Taste Punch
    AmpRole::clean|AmpRole::crunch|AmpRole::lead|AmpRole::highGain,
    AmpRole::clean|AmpRole::highGain,
    AmpRole::clean|AmpRole::crunch|AmpRole::lead|AmpRole::highGain,
    AmpRole::clean|AmpRole::crunch|AmpRole::bass,
    AmpRole::clean|AmpRole::crunch,
    AmpRole::clean|AmpRole::lead|AmpRole::highGain,
    AmpRole::lead|AmpRole::highGain,
    AmpRole::clean|AmpRole::highGain,
    AmpRole::clean|AmpRole::crunch|AmpRole::lead|AmpRole::highGain,
    AmpRole::lead|AmpRole::highGain
}};
constexpr bool ampMatches(int index,AmpRole role=AmpRole::any,AmpInstrument instrument=AmpInstrument::any) {
    if(index<0 || index>=ampModelCount)return false;
    const auto& info=ampCatalog[static_cast<size_t>(index)];
    return (role==AmpRole::any || (ampRoleTags[static_cast<size_t>(index)]&roleBits(role))!=0)
        && (instrument==AmpInstrument::any || (instrument==AmpInstrument::bass ? info.bass : !info.bass));
}
constexpr int ampIndexFromMenuId(int id) { return id>=1 && id<=ampModelCount ? id-1 : -1; }
// One primary category per model prevents repeated entries while leaving all
// heads available to Classic, Dual and every Matrix lane.
inline constexpr std::array<AmpRole,ampModelCount> ampPrimaryRoles{{
    AmpRole::clean, AmpRole::crunch, AmpRole::highGain, AmpRole::highGain,
    AmpRole::lead, AmpRole::bass, AmpRole::bass, AmpRole::bass,
    AmpRole::crunch, AmpRole::highGain, AmpRole::bass, AmpRole::bass,
    AmpRole::clean, AmpRole::lead, AmpRole::bass,
    AmpRole::highGain, AmpRole::highGain, AmpRole::highGain, AmpRole::bass,
    AmpRole::crunch, AmpRole::highGain, AmpRole::lead, AmpRole::highGain, AmpRole::highGain, AmpRole::highGain
}};
inline const char* ampPrimaryRoleName(int model) {
    const auto role=ampPrimaryRoles[(size_t)juce::jlimit(0,ampModelCount-1,model)];
    for(const auto& item:ampRoleChoices)if(item.role==role)return item.label;
    return "";
}
inline juce::StringArray legacyAmpNames() {
    return {"Glass","Brit Edge","Tight 515","Wide Rect","Liquid Lead","Iron Tube","Solid Punch","Modern Bass",
            "Chime 30","Orange Crown","Bassman Valve","Subway Clean","Match Chime","Silk ODS","Taste Punch"};
}
inline const AmpInfo& ampInfo(int index) { return ampCatalog[(size_t)juce::jlimit(0, ampModelCount - 1, index)]; }
inline juce::StringArray ampNames() { juce::StringArray names; for (const auto& info : ampCatalog) names.add(juce::String::fromUTF8(info.name)); return names; }
}

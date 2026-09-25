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
inline const AmpInfo& ampInfo(int index) { return ampCatalog[(size_t)juce::jlimit(0, ampModelCount - 1, index)]; }
inline juce::StringArray ampNames() { juce::StringArray names; for (const auto& info : ampCatalog) names.add(info.name); return names; }
}

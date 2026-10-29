#pragma once
#include "GuitarSignaturePresets.h"
#include <array>

namespace spectralforge {
// One plain sentence per selectable preset, in menu index order: what it
// sounds like and what it suits, without model names or crossover numbers.
// Easy view shows it; the full editor's tooltip adds the technical
// description from the preset tables. Keep each under 100 characters so it
// fits Easy view on one line.
inline constexpr std::array<const char*,48> presetSummaries{{
    // Factory 0-30
    "Clean, smooth guitar with a little room. Good for chords and arpeggios.",
    "Heavy, tight distortion for fast metal rhythm guitar.",
    "Full bass: clean, solid lows with a growling middle and a clear attack.",
    "Guitar lead with a wah-like sweep as you pick, plus echoes in time with the song.",
    "Thick, fuzzy guitar that sustains for a long time, with a gentle shimmer.",
    "Bright, bell-like clean guitar with a short spring echo.",
    "Clean guitar that breaks up when you dig in: soft for clean, harder for grit.",
    "Classic hard-rock crunch guitar for riffs and chords.",
    "Thick, heavy distorted guitar for big rhythm parts.",
    "Saturated, cutting metal rhythm guitar for melodic death metal.",
    "Smooth, singing guitar lead with echo and room. Good for melodic solos.",
    "Wide, spacious clean guitar with chorus, echoes and a big room.",
    "Warm, round bass for fingerstyle playing.",
    "Punchy, clear bass for playing rock with a pick.",
    "Bright, tight bass for slap and modern funk or pop.",
    "Distorted bass that keeps its low end. For heavy rock and metal.",
    "Warm, slightly driven bass with an old-school rock feel.",
    "Fuzzy, sustaining bass with a darker top.",
    "Funky bass with an auto-wah that follows your playing.",
    "Two guitar amps at once: mostly clean, with some crunch underneath.",
    "Two heavy guitar amps blended for a tight but wide metal rhythm.",
    "For bass or low-tuned guitar: clean lows with distorted grit on top.",
    "For bass or guitar: warm, heavy lows with a bright, airy top.",
    "Two bass amps blended: warm body plus clear definition, for fingers or pick.",
    "Bass with clean lows and distorted grind on top; low notes stay clear.",
    "For 5- and 6-string bass: steady clean lows, a growling middle and a soft attack.",
    "Bass with clean lows, a warm middle and a bright pick attack.",
    "Experimental: clean lows under chiming and gritty layers that slowly move.",
    "Chiming, clear guitar on the edge of breakup, with a little spring echo.",
    "Smooth, expressive guitar lead with soft echoes.",
    "Clean, natural bass for modern styles that stays out of the way.",
    // Deadwire bass signatures 31-33
    "Signature bass: full lows, a growling middle and a sharp attack. Balanced and heavy.",
    "Signature bass: lighter lows, a forward middle and an open, lively top.",
    "Signature bass: dense, compressed low end with a focused bite.",
    // Guitar signatures 34-37
    "Power-metal guitar solo: a singing, sustained lead with echoes and room.",
    "Groove-metal rhythm guitar: heavy, tight and dry.",
    "Death-metal rhythm guitar: crushing, focused and dry.",
    "Death-metal rhythm guitar: dark, heavy and dry.",
    // Nastrond guitar 38-42
    "Very heavy, dry, djent-style metal rhythm guitar.",
    "Hot, sustained guitar lead with echo and room.",
    "Raw, aggressive grind guitar with a sharp bite on top.",
    "Thick, dark fuzz guitar for slow sludge riffs, with a small dark room.",
    "Huge, layered metal rhythm guitar with a hard-hitting attack.",
    // Niflheimr bass 43-47
    "Metal bass with controlled lows and a clear, clanky string attack.",
    "Death and grind bass: solid lows with a biting, distorted top.",
    "Heavy bass with a clean low anchor, driven middle and a sharp attack.",
    "Brutal slam bass: mostly clean lows with a hard, punchy middle and top.",
    "Slow doom and sludge bass with a dark, sustained fuzz."
}};
static_assert(presetSummaries.size()==size_t(selectablePresetCount),"Every selectable preset needs a plain summary");

inline const char* selectablePresetSummary(int index) {
    return index>=0 && index<selectablePresetCount ? presetSummaries[size_t(index)] : "";
}
}

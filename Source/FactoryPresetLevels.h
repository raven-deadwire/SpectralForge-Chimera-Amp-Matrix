#pragma once
#include <array>
namespace spectralforge {
// End-of-chain factory trims: six-note harmonic plucks at 48 kHz / 128.
// Target about -26 dBFS RMS, with at least 10 dB nominal peak headroom.
// These are conservative synthetic gain-staging values, not DI/LUFS acceptance.
// Kept separate from legacy recipes and saved-project parameter defaults.
inline constexpr std::array<float,38> factoryOutputDb{{
    6.5f, // 0: Clean Sustain
    -5.0f, // 1: Tight Rhythm
    -4.5f, // 2: Bass Matrix
    -4.0f, // 3: Filter Lead
    6.5f, // 4: Fuzz Texture
    -1.5f, // 5: Bell Clean
    -5.5f, // 6: Edge Chime
    -2.5f, // 7: Classic Crunch
    -5.0f, // 8: Orange Heavy
    -7.0f, // 9: Melodic Death Rhythm
    -3.0f, // 10: Melodic Lead
    11.5f, // 11: Ambient Clean
    -12.0f, // 12: Finger Round
    -6.0f, // 13: Pick Punch
    2.0f, // 14: Slap Studio
    -10.5f, // 15: Modern Grind
    -10.5f, // 16: Vintage Bass DI
    -7.5f, // 17: Wool Bass Fuzz
    7.0f, // 18: Bass Envelope
    8.5f, // 19: G+G Clean / Crunch
    -6.0f, // 20: G+G Tight / Wide
    -6.0f, // 21: G+B Low Anchor
    -11.0f, // 22: G+B Air / Weight
    -4.5f, // 23: B+B Warm / Definition
    -7.0f, // 24: B+B Clean / Grind
    -5.5f, // 25: Low B Foundation
    -7.5f, // 26: Pick Attack Matrix
    0.5f, // 27: Spectral Texture
    -2.5f, // 28: Matchless Edge
    -4.0f, // 29: Dumble Smooth Lead
    4.0f, // 30: EICH Modern Clean
    -1.5f, // 31: Crom Cruach
    -12.5f, // 32: Wild Hunt
    -2.0f, // 33: Azhi Dahaka
    -4.5f, // 34: A Path To Alsatia
    -5.5f, // 35: Feel My Wrath
    -5.5f, // 36: Blackhearted
    -2.0f, // 37: Dark Matters of Throne
}};
} // namespace spectralforge

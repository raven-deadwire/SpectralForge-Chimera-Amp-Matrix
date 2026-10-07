#pragma once
#include <array>
namespace spectralforge {
// Every factory recall starts at unity OUTPUT. Musical level is authored in
// the visible amp / POST stages; saved projects retain their stored OUTPUT.
inline constexpr std::array<float,48> factoryOutputDb{{
    0.0f, // 0: Clean Sustain
    0.0f, // 1: Tight Rhythm
    0.0f, // 2: Bass Matrix
    0.0f, // 3: Filter Lead
    0.0f, // 4: Fuzz Texture
    0.0f, // 5: Bell Clean
    0.0f, // 6: Edge Chime
    0.0f, // 7: Classic Crunch
    0.0f, // 8: Orange Heavy
    0.0f, // 9: Melodic Death Rhythm
    0.0f, // 10: Melodic Lead
    0.0f, // 11: Ambient Clean
    0.0f, // 12: Finger Round
    0.0f, // 13: Pick Punch
    0.0f, // 14: Slap Studio
    0.0f, // 15: Modern Grind
    0.0f, // 16: Vintage Bass DI
    0.0f, // 17: Wool Bass Fuzz
    0.0f, // 18: Bass Envelope
    0.0f, // 19: G+G Clean / Crunch
    0.0f, // 20: G+G Tight / Wide
    0.0f, // 21: G+B Low Anchor
    0.0f, // 22: G+B Air / Weight
    0.0f, // 23: B+B Warm / Definition
    0.0f, // 24: B+B Clean / Grind
    0.0f, // 25: Low B Foundation
    0.0f, // 26: Pick Attack Matrix
    0.0f, // 27: Spectral Texture
    0.0f, // 28: Matchless Edge
    0.0f, // 29: Dumble Smooth Lead
    0.0f, // 30: Modern Clean
    0.0f, // 31: Crom Cruach
    0.0f, // 32: Wild Hunt
    0.0f, // 33: Azhi Dahaka
    0.0f, // 34: A Path To Alsatia
    0.0f, // 35: Feel My Wrath
    0.0f, // 36: Blackhearted
    0.0f, // 37: Dark Matters of Throne
    0.0f, // 38: Thall Rhythm (Original)
    0.0f, // 39: Molten Lead (Original)
    0.0f, // 40: Rotten Grind (Original)
    0.0f, // 41: Sludge Mass (Original)
    0.0f, // 42: Slam Impact (Original)
    0.0f, // 43: Frostline Precision (Niflheimr Original)
    0.0f, // 44: Carrion Barrage (Niflheimr Original)
    0.0f, // 45: Foundry Pulse (Niflheimr Original)
    0.0f, // 46: Jötunn Hammer (Niflheimr Original)
    0.0f, // 47: Mirebound Monolith (Niflheimr Original)
}};
} // namespace spectralforge

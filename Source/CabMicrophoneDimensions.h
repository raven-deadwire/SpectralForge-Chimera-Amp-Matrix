#pragma once
#include <array>
#include <cstddef>

namespace spectralforge::cabPhysical {

// Upright, unrotated microphone body width/height in metres. Presentation data
// only: these values are not acoustic parameters, capture provenance or a claim
// that an original Chimera response reproduces another manufacturer's product.
// Excludes cables, stands and detachable shock mounts. See the source/revision
// and integral-mount notes in docs/CAB_MICROPHONE_DIMENSIONS.md.
struct MicDimensions {
    float width;
    float height;
    bool authored; // true means a Chimera design dimension, not a hardware spec
};

// Order follows micCatalog::models; never use these positions as host IDs.
inline constexpr std::array<MicDimensions,20> microphones{{
    { .0320f, .1570f, false }, // dynamic-57: Shure SM57
    { .0490f, .2150f, false }, // dynamic-421: Sennheiser MD 421-II
    { .0360f, .2700f, false }, // dynamic-441: Sennheiser MD 441-U
    { .0550f, .1340f, false }, // dynamic-906: Sennheiser e 906
    { .0544f, .2167f, false }, // dynamic-20: Electro-Voice RE20
    { .0625f, .1971f, false }, // dynamic-7: SM7B body/standard foam, no yoke
    { .0240f, .1470f, false }, // dynamic-201: current beyerdynamic M 201
    { .0485f, .1730f, false }, // dynamic-88: current beyerdynamic M 88
    { .0700f, .1260f, false }, // dynamic-112: D112 MKII, integral swivel included
    { .0250f, .1557f, false }, // ribbon-121: Royer R-121
    { .0380f, .1640f, false }, // ribbon-160: current beyerdynamic M 160
    { .0830f, .1970f, false }, // ribbon-4038: Coles 4038 complete mic envelope
    { .0560f, .2000f, false }, // condenser-87: Neumann U 87 Ai
    { .0500f, .1600f, false }, // condenser-414: AKG C414 XLS/XLII
    { .0220f, .1070f, false }, // condenser-184: Neumann KM 184
    { .0630f, .1600f, false }, // condenser-47-fet: U 47 fet body, no side arm
    { .0534f, .1880f, false }, // condenser-4050: Audio-Technica AT4050
    { .0510f, .1940f, false }, // condenser-201-fet: Mojave MA-201fet
    { .0560f, .2000f, false }, // condenser-67: Neumann U 67 reissue
    { .0500f, .2000f, true  }  // chimera-strike: authored original enclosure
}};

// Legacy PNGs have a baked authored orientation. These are their projected
// image envelopes, not the upright body diameters used by the catalog above.
inline constexpr std::array<MicDimensions,3> legacyMicrophones{{
    { .160f, .135f, true }, // Attack Dynamic, diagonal end-address projection
    { .055f, .200f, true }, // Body Ribbon, authored projected envelope
    { .090f, .200f, true }  // Detail Condenser, authored projected envelope
}};

inline constexpr MicDimensions legacyMicrophone(int role) noexcept {
    return legacyMicrophones[static_cast<std::size_t>(role>=0 && role<3 ? role : 0)];
}

inline constexpr MicDimensions microphone(int catalogIndex) noexcept {
    return catalogIndex>=0 && catalogIndex<static_cast<int>(microphones.size())
        ? microphones[static_cast<std::size_t>(catalogIndex)] : legacyMicrophone(0);
}

} // namespace spectralforge::cabPhysical

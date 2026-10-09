#pragma once
#include "AmpCatalog.h"
#include <array>

namespace spectralforge::cabPhysical {
// Presentation dimensions in metres, ordered width / height / depth. These
// describe the physical front/enclosure, never the alpha bounds of its artwork.
// Published product envelopes are retained as supplied; handle/foot allowances
// and the explicitly authored conversions are documented in CAB_HEAD_DIMENSIONS.
struct HeadDimensions {
    float width, height, depth;
    bool authored; // true if any dimension is an authored display assumption
};

// Keep the released AmpModel ordinal order. Use CTAD so a new amp cannot silently
// acquire a zero-initialised dimension record by growing ampModelCount.
inline constexpr std::array headDimensions{
    HeadDimensions{.67310f, .22500f, .26353f, true }, // Glass: Twin-width authored head conversion
    HeadDimensions{.66500f, .26500f, .20500f, false}, // Brit Edge: JTM45 2245
    HeadDimensions{.676275f,.25400f, .29845f, false}, // Tight 515: 6505 II family envelope
    HeadDimensions{.64770f, .25400f, .250825f,false}, // Wide Rect: Dual Rectifier
    HeadDimensions{.47625f, .24000f, .27000f, true }, // Liquid Lead: Mark IV short-head width
    HeadDimensions{.61000f, .29200f, .32400f, false}, // Iron Tube: SVT-VR
    HeadDimensions{.44000f, .13500f, .24000f, true }, // Solid Punch: authored compact metal enclosure
    HeadDimensions{.43180f, .13335f, .35560f, false}, // Modern Bass: DB751, without rack ears
    HeadDimensions{.70500f, .28400f, .26600f, false}, // Chime 30: AC30CH head format
    HeadDimensions{.55000f, .27000f, .28000f, false}, // Orange Crown: Rockerverb 50 MKIII
    HeadDimensions{.62230f, .25400f, .34290f, false}, // Vintage Valve: Super Bassman
    HeadDimensions{.33655f, .066675f,.25781f, false}, // Metro Clean: Subway D-800+
    HeadDimensions{.54610f, .27305f, .26670f, false}, // Prism Chime: HC-30 head format
    HeadDimensions{.58000f, .26000f, .27000f, true }, // Silk Lead: authored ODS-style head envelope
    HeadDimensions{.27000f, .04500f, .21000f, false}, // Taste Punch: T900 (not T900 Classic)
    HeadDimensions{.48300f, .13300f, .22000f, false}, // Cinder 120: GBG120 front incl. rack ears
    HeadDimensions{.34000f, .17000f, .22000f, false}, // Iron Compact: E606 incl. raised handle
    HeadDimensions{.74000f, .30000f, .28000f, false}, // Fourfold: VH4, maker's labelled axes
    HeadDimensions{.61000f, .29200f, .33000f, false}, // Classic Tube: SVT-CL
    HeadDimensions{.67000f, .25000f, .27000f, true }, // Monolith: authored vintage head envelope
    HeadDimensions{.73000f, .28000f, .25500f, true }, // Night Harvest: authored full-size head envelope
    HeadDimensions{.63500f, .26035f, .22225f, false}, // Hot Lead: SLO-100 current wooden head
    HeadDimensions{.69000f, .28000f, .27000f, true }, // Blue Storm: authored legacy head envelope
    HeadDimensions{.71000f, .27000f, .29000f, false}, // Special Edition: E670FE
    HeadDimensions{.70000f, .27000f, .28500f, true }, // Nastroend: SpectralForge design envelope
    HeadDimensions{.61000f, .24500f, .30000f, true }  // Niflheimr: SpectralForge design envelope
};
static_assert(headDimensions.size() == static_cast<std::size_t>(ampModelCount),
              "Every AmpCatalog entry needs explicit physical display dimensions");

constexpr const HeadDimensions& head(int model) noexcept {
    const auto index = model < 0 ? 0 : (model >= ampModelCount ? ampModelCount - 1 : model);
    return headDimensions[static_cast<std::size_t>(index)];
}
} // namespace spectralforge::cabPhysical

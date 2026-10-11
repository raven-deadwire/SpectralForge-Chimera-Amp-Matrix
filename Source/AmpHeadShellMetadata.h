#pragma once
#include "AmpCatalog.h"
#include <array>

namespace spectralforge::headShell {
// Raw coordinates in each complete, unmodified RGBA production image. The
// control plane is an explicit art landmark, not an alpha-row classification.
// The whole image uses one uniform transform; roof, shell, handles and feet are
// never extracted, independently stretched, or completed with synthetic pieces.
struct PixelRect {
    int x{},y{},width{},height{};
    constexpr bool valid() const noexcept {return width>0 && height>0;}
};
struct Definition {
    const char* resource;
    int pixelWidth,pixelHeight;
    PixelRect alphaBounds,frontPlane;
    int supportY;
    bool topHandle;
    // Painted switch interiors are authored separately from the control plane;
    // an enlarged touch target must not paint across the surrounding bezel.
    std::array<PixelRect,5> channelWindows{};
    constexpr bool valid() const noexcept {
        return pixelWidth>0 && pixelHeight>0 && alphaBounds.valid() && frontPlane.valid()
            && supportY>alphaBounds.y && supportY<=pixelHeight;
    }
};
// Entries are reviewed against the named PNG, including channel windows and
// blank control plates. Alpha bounds use alpha>32; right and bottom are exclusive.
// The support baseline includes the complete authored feet. No alpha-derived
// row threshold is allowed to invent a different shell/front relationship.
inline constexpr std::array definitions{
    Definition{"headshell00_png",1774,887,{26,115,1721,677},{26,235,1721,527},792,true}, // 0: Glass
    Definition{"headshell01_png",1536,1024,{68,210,1401,665},{68,291,1401,558},875,true}, // 1: Brit Edge
    Definition{"headshell02_png",1774,887,{31,90,1712,689},{31,211,1712,540},779,true}, // 2: Tight 515
    Definition{"headshell03_png",1536,1024,{26,188,1485,719},{28,279,1481,603},907,true}, // 3: Wide Rect
    Definition{"headshell04_png",1536,1024,{47,207,1443,673},{48,273,1442,577},880,true}, // 4: Liquid Lead
    Definition{"headshell05_png",1448,1086,{12,199,1424,717},{42,302,1365,587},916,true}, // 5: Iron Tube
    Definition{"headshell06_png",1448,1086,{24,242,1400,603},{24,329,1382,514},845,false}, // 6: Solid Punch
    Definition{"headshell07_png",1672,941,{16,218,1640,522},{16,218,1640,501},740,false}, // 7: Modern Bass
    Definition{"headshell08_png",1672,941,{18,130,1636,682},{18,234,1636,548},812,true}, // 8: Chime 30
    Definition{"headshell09_png",1536,1024,{27,89,1481,851},{28,185,1479,719},940,true}, // 9: Orange Crown
    Definition{"headshell10_png",1448,1086,{19,258,1411,631},{20,344,1409,518},889,true}, // 10: Vintage Valve
    Definition{"headshell11_png",1672,941,{50,288,1572,387},{50,288,1572,357},675,false}, // 11: Metro Clean
    Definition{"headshell12_png",1837,856,{45,10,1747,834},{45,119,1747,697},844,true}, // 12: Prism Chime
    Definition{"headshell13_png",1448,1086,{16,176,1416,755},{17,308,1414,595},931,true}, // 13: Silk Lead
    Definition{"headshell14_png",1672,941,{43,289,1586,362},{43,289,1586,335},651,false}, // 14: Taste Punch
    Definition{"headshell15_png",1672,941,{21,140,1630,660},{21,234,1630,540},800,true}, // 15: Cinder 120
    Definition{"headshell16_png",1448,1086,{21,124,1404,850},{21,233,1404,704},974,true}, // 16: Iron Compact
    Definition{"headshell17_png",1774,887,{43,94,1688,697},{43,184,1688,574},791,true}, // 17: Fourfold
    Definition{"headshell18_png",1448,1086,{13,214,1423,687},{15,307,1420,565},901,true}, // 18: Classic Tube
    Definition{"headshell19_png",1774,887,{47,141,1679,636},{47,245,1679,500},777,true}, // 19: Monolith
    Definition{"headshell20_png",1448,1086,{30,266,1392,594},{30,348,1392,454},860,true}, // 20: Night Harvest, two rows centred inside the blank plate
    Definition{"headshell21_png",1448,1086,{31,282,1388,558},{31,373,1388,440},840,true}, // 21: Hot Lead
    Definition{"headshell22_png",1448,1086,{21,228,1406,628},{21,333,1406,493},856,true}, // 22: Blue Storm
    Definition{"headshell23_png",1536,1024,{26,189,1485,679},{26,306,1485,526},868,true}, // 23: Special Edition
    Definition{"headshell24_png",1774,887,{11,60,1753,773},{11,173,1753,641},833,true,
        {{{722,475,45,21},{790,475,51,21},{862,475,51,21},{934,475,51,21},{1006,475,49,21}}}}, // 24: Nastroend
    Definition{"headshell25_png",1672,941,{13,73,1646,791},{13,205,1646,634},864,true,
        {{{682,474,44,20},{745,474,46,20},{810,474,46,20},{875,474,46,20},{940,474,44,20}}}}, // 25: Niflheimr
};
static_assert(definitions.size()==static_cast<size_t>(ampModelCount),
              "Every amplifier needs an explicitly mapped complete shell");
static_assert([] {for(const auto& shell:definitions)if(!shell.valid())return false;return true;}(),
              "Complete shell metadata must cover every released amplifier");
inline constexpr const Definition& definition(int model) noexcept {
    return definitions[static_cast<size_t>(model<0 ? 0 : (model>=ampModelCount ? ampModelCount-1 : model))];
}
} // namespace spectralforge::headShell

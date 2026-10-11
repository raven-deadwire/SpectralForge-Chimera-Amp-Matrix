#pragma once
#include "AmpNativeCatalog.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <span>
#include <vector>

namespace spectralforge::ampHead {
// Coordinates are relative to the authored control plane in AmpHeadShellMetadata.
// These are authored Chimera front-panel arrangements, not claims of exact
// hardware-panel reproduction. The native catalog remains the sole parameter
// identity / channel-mask authority; changing the presentation never reorders it.
struct Knob {const char* key; const char* label; float x,y;};
struct FaceStyle {
    float knobDiameter,cellWidth; // fractions of the complete front width
    juce::uint32 ink;
    juce::Rectangle<float> controlField,channelRegion,routeRegion;
};
// Empty channel/route regions use the workspace rail. The original two heads
// retain their dedicated on-face route / channel locations.
// 0 — Glass
inline constexpr Knob model0[]{
    {"hw.normal.volume","VOLUME",0.1200f,0.7950f},
    {"hw.normal.treble","TREBLE",0.2350f,0.7950f},
    {"hw.normal.middle","MIDDLE",0.3500f,0.7950f},
    {"hw.normal.bass","BASS",0.4650f,0.7950f},
    {"hw.vibrato.volume","VOLUME",0.1200f,0.7950f},
    {"hw.vibrato.treble","TREBLE",0.2350f,0.7950f},
    {"hw.vibrato.middle","MIDDLE",0.3500f,0.7950f},
    {"hw.vibrato.bass","BASS",0.4650f,0.7950f},
    {"hw.vibrato.reverb","REVERB",0.6050f,0.7950f},
    {"hw.vibrato.speed","SPEED",0.7300f,0.7950f},
    {"hw.vibrato.intensity","INTENSITY",0.8550f,0.7950f},
};
// 1 — Brit Edge
inline constexpr Knob model1[]{
    {"hw.presence","PRESENCE",0.1400f,0.7500f},
    {"hw.bass","BASS",0.2500f,0.7500f},
    {"hw.middle","MIDDLE",0.3600f,0.7500f},
    {"hw.treble","TREBLE",0.4700f,0.7500f},
    {"hw.high_treble","LOUDNESS I",0.5800f,0.7500f},
    {"hw.normal","LOUDNESS II",0.6900f,0.7500f},
};
// 2 — Tight 515
inline constexpr Knob model2[]{
    {"hw.low","LOW",0.3450f,0.7850f},
    {"hw.mid","MID",0.4600f,0.7850f},
    {"hw.high","HIGH",0.5750f,0.7850f},
    {"hw.resonance","RESONANCE",0.7200f,0.7850f},
    {"hw.presence","PRESENCE",0.8500f,0.7850f},
    {"hw.rhythm.pre_gain","PRE GAIN",0.1100f,0.7850f},
    {"hw.rhythm.post_gain","POST GAIN",0.2250f,0.7850f},
    {"hw.lead.pre_gain","PRE GAIN",0.1100f,0.7850f},
    {"hw.lead.post_gain","POST GAIN",0.2250f,0.7850f},
};
// 3 — Wide Rect
inline constexpr Knob model3[]{
    {"hw.ch1.gain","GAIN",0.0950f,0.7750f},
    {"hw.ch1.treble","TREBLE",0.2000f,0.7750f},
    {"hw.ch1.mid","MID",0.3050f,0.7750f},
    {"hw.ch1.bass","BASS",0.4100f,0.7750f},
    {"hw.ch1.presence","PRESENCE",0.5150f,0.7750f},
    {"hw.ch1.master","MASTER",0.6200f,0.7750f},
    {"hw.ch2.gain","GAIN",0.0950f,0.7750f},
    {"hw.ch2.treble","TREBLE",0.2000f,0.7750f},
    {"hw.ch2.mid","MID",0.3050f,0.7750f},
    {"hw.ch2.bass","BASS",0.4100f,0.7750f},
    {"hw.ch2.presence","PRESENCE",0.5150f,0.7750f},
    {"hw.ch2.master","MASTER",0.6200f,0.7750f},
    {"hw.ch3.gain","GAIN",0.0950f,0.7750f},
    {"hw.ch3.treble","TREBLE",0.2000f,0.7750f},
    {"hw.ch3.mid","MID",0.3050f,0.7750f},
    {"hw.ch3.bass","BASS",0.4100f,0.7750f},
    {"hw.ch3.presence","PRESENCE",0.5150f,0.7750f},
    {"hw.ch3.master","MASTER",0.6200f,0.7750f},
    {"hw.output","OUTPUT",0.7650f,0.7750f},
    {"hw.solo","SOLO",0.8800f,0.7750f},
};
// 4 — Liquid Lead
inline constexpr Knob model4[]{
    {"hw.r1.gain","GAIN",0.1000f,0.6650f},
    {"hw.r1.treble","TREBLE",0.2250f,0.6650f},
    {"hw.r1.presence","PRESENCE",0.7250f,0.6650f},
    {"hw.r1.master","MASTER",0.8500f,0.6650f},
    {"hw.r2.gain","GAIN",0.1000f,0.6650f},
    {"hw.r2.treble","TREBLE",0.2250f,0.6650f},
    {"hw.r2.presence","PRESENCE",0.7250f,0.6650f},
    {"hw.r2.master","MASTER",0.8500f,0.6650f},
    {"hw.rhythm.bass","BASS",0.3500f,0.6650f},
    {"hw.rhythm.middle","MIDDLE",0.4750f,0.6650f},
    {"hw.lead.gain","GAIN",0.1000f,0.6650f},
    {"hw.lead.treble","TREBLE",0.2250f,0.6650f},
    {"hw.lead.bass","BASS",0.3500f,0.6650f},
    {"hw.lead.middle","MIDDLE",0.4750f,0.6650f},
    {"hw.lead.drive","DRIVE",0.6000f,0.6650f},
    {"hw.lead.presence","PRESENCE",0.7250f,0.6650f},
    {"hw.lead.master","MASTER",0.8500f,0.6650f},
    {"hw.geq.80","80 Hz",0.1000f,0.8550f},
    {"hw.geq.240","240 Hz",0.2250f,0.8550f},
    {"hw.geq.750","750 Hz",0.3500f,0.8550f},
    {"hw.geq.2200","2.2 kHz",0.4750f,0.8550f},
    {"hw.geq.6600","6.6 kHz",0.6000f,0.8550f},
    {"hw.output","OUTPUT",0.7250f,0.8550f},
    {"hw.reverb","REVERB",0.8500f,0.8550f},
};
// 5 — Iron Tube
inline constexpr Knob model5[]{
    {"hw.ch1.volume","VOLUME",0.2000f,0.7900f},
    {"hw.ch1.treble","TREBLE",0.3900f,0.7900f},
    {"hw.ch1.midrange","MIDRANGE",0.5800f,0.7900f},
    {"hw.ch1.bass","BASS",0.7700f,0.7900f},
    {"hw.ch2.volume","VOLUME",0.2000f,0.7900f},
    {"hw.ch2.treble","TREBLE",0.3900f,0.7900f},
    {"hw.ch2.bass","BASS",0.7700f,0.7900f},
};
// 6 — Solid Punch
inline constexpr Knob model6[]{
    {"hw.volume","VOLUME",0.0900f,0.8400f},
    {"hw.treble","TREBLE",0.1925f,0.8400f},
    {"hw.hi_mid","HI MID",0.2950f,0.8400f},
    {"hw.lo_mid","LO MID",0.3975f,0.8400f},
    {"hw.bass","BASS",0.5000f,0.8400f},
    {"hw.boost","BOOST",0.6025f,0.8400f},
    {"hw.low_master","LOW MSTR",0.7050f,0.8400f},
    {"hw.high_master","HIGH MSTR",0.8075f,0.8400f},
    {"hw.crossover","CROSSOVER",0.9100f,0.8400f},
};
// 7 — Modern Bass
inline constexpr Knob model7[]{
    {"hw.b7k.master","ULTRA MSTR",0.1050f,0.6400f},
    {"hw.b7k.blend","ULTRA MIX",0.2175f,0.6400f},
    {"hw.b7k.level","ULTRA LEVEL",0.3300f,0.6400f},
    {"hw.b7k.drive","ULTRA DRIVE",0.4425f,0.6400f},
    {"hw.b7k.bass","ULTRA BASS",0.5550f,0.6400f},
    {"hw.b7k.lo_mids","ULTRA LO",0.6675f,0.6400f},
    {"hw.b7k.hi_mids","ULTRA HI",0.7800f,0.6400f},
    {"hw.b7k.treble","ULTRA TREB",0.8925f,0.6400f},
    {"hw.db751.gain","AMP GAIN",0.1900f,0.8650f},
    {"hw.db751.bass","AMP BASS",0.3450f,0.8650f},
    {"hw.db751.mid","AMP MID",0.5000f,0.8650f},
    {"hw.db751.treble","AMP TREBLE",0.6550f,0.8650f},
    {"hw.db751.master","AMP MASTER",0.8100f,0.8650f},
};
// 8 — Chime 30
inline constexpr Knob model8[]{
    {"hw.normal.volume","VOLUME",0.0950f,0.8000f},
    {"hw.top_boost.volume","VOLUME",0.0950f,0.8000f},
    {"hw.top_boost.treble","TREBLE",0.1963f,0.8000f},
    {"hw.top_boost.bass","BASS",0.2975f,0.8000f},
    {"hw.tone_cut","TONE CUT",0.3988f,0.8000f},
    {"hw.master_volume","MASTER",0.5000f,0.8000f},
    {"hw.reverb_tone","REV TONE",0.6013f,0.8000f},
    {"hw.reverb_level","REVERB",0.7025f,0.8000f},
    {"hw.tremolo_speed","TREM SPEED",0.8037f,0.8000f},
    {"hw.tremolo_depth","TREM DEPTH",0.9050f,0.8000f},
};
// 9 — Orange Crown
inline constexpr Knob model9[]{
    {"hw.clean.volume","VOLUME",0.1100f,0.7300f},
    {"hw.clean.treble","TREBLE",0.2350f,0.7300f},
    {"hw.clean.bass","BASS",0.4850f,0.7300f},
    {"hw.dirty.gain","GAIN",0.1100f,0.7300f},
    {"hw.dirty.treble","TREBLE",0.2350f,0.7300f},
    {"hw.dirty.middle","MIDDLE",0.3600f,0.7300f},
    {"hw.dirty.bass","BASS",0.4850f,0.7300f},
    {"hw.dirty.volume","VOLUME",0.6100f,0.7300f},
    {"hw.reverb","REVERB",0.7550f,0.7300f},
    {"hw.attenuator","ATTENUATOR",0.8800f,0.7300f},
};
// 10 — Vintage Valve
inline constexpr Knob model10[]{
    {"hw.vintage.volume","VOLUME",0.1150f,0.7900f},
    {"hw.vintage.bass","BASS",0.3350f,0.7900f},
    {"hw.vintage.mid","MID",0.5550f,0.7900f},
    {"hw.vintage.treble","TREBLE",0.6650f,0.7900f},
    {"hw.overdrive.gain","GAIN",0.1150f,0.7900f},
    {"hw.overdrive.blend","BLEND",0.2250f,0.7900f},
    {"hw.overdrive.volume","VOLUME",0.3350f,0.7900f},
    {"hw.overdrive.bass","BASS",0.4450f,0.7900f},
    {"hw.overdrive.mid_frequency","MID FREQ",0.5550f,0.7900f},
    {"hw.overdrive.mid_level","MID LEVEL",0.6650f,0.7900f},
    {"hw.overdrive.treble","TREBLE",0.7750f,0.7900f},
    {"hw.master","MASTER",0.8850f,0.7900f},
};
// 11 — Metro Clean
inline constexpr Knob model11[]{
    {"hw.input","INPUT",0.1150f,0.5500f},
    {"hw.high_pass","HIGH PASS",0.3050f,0.5500f},
    {"hw.voicing","VOICING",0.4950f,0.5500f},
    {"hw.bass","BASS",0.6850f,0.5500f},
    {"hw.lo_mid_frequency","LO MID FREQUENCY",0.8750f,0.5500f},
    {"hw.lo_mid","LO MID",0.1150f,0.8400f},
    {"hw.hi_mid_frequency","HI MID FREQUENCY",0.3050f,0.8400f},
    {"hw.hi_mid","HI MID",0.4950f,0.8400f},
    {"hw.treble","TREBLE",0.6850f,0.8400f},
    {"hw.master","MASTER",0.8750f,0.8400f},
};
// 12 — Prism Chime
inline constexpr Knob model12[]{
    {"hw.ch1.volume","VOLUME",0.1450f,0.7600f},
    {"hw.ch1.bass","BASS",0.3150f,0.7600f},
    {"hw.ch1.treble","TREBLE",0.4850f,0.7600f},
    {"hw.cut","CUT",0.6550f,0.7600f},
    {"hw.master","MASTER",0.8250f,0.7600f},
    {"hw.ch2.volume","VOLUME",0.1450f,0.7600f},
};
// 13 — Silk Lead
inline constexpr Knob model13[]{
    {"hw.volume","VOLUME",0.0950f,0.8050f},
    {"hw.treble","TREBLE",0.2100f,0.8050f},
    {"hw.middle","MIDDLE",0.3250f,0.8050f},
    {"hw.bass","BASS",0.4400f,0.8050f},
    {"hw.od.drive","DRIVE",0.5550f,0.8050f},
    {"hw.od.ratio","RATIO",0.6700f,0.8050f},
    {"hw.master","MASTER",0.7850f,0.8050f},
    {"hw.presence","PRESENCE",0.9000f,0.8050f},
};
// 14 — Taste Punch
inline constexpr Knob model14[]{
    {"hw.gain","GAIN",0.1250f,0.6300f},
    {"hw.taste","TASTE",0.2500f,0.6300f},
    {"hw.lo","LO",0.3750f,0.6300f},
    {"hw.lo_mid","LO MID",0.5000f,0.6300f},
    {"hw.hi_mid","HI MID",0.6250f,0.6300f},
    {"hw.hi","HI",0.7500f,0.6300f},
    {"hw.master","MASTER",0.8750f,0.6300f},
};
// 15 — Cinder 120
inline constexpr Knob model15[]{
    {"ch1.gain","GAIN",0.0900f,0.7950f},
    {"ch1.low","LOW",0.1800f,0.7950f},
    {"ch1.mid","MID",0.2700f,0.7950f},
    {"ch1.high","HIGH",0.3600f,0.7950f},
    {"ch1.volume","VOLUME",0.4500f,0.7950f},
    {"ch2.gain","GAIN",0.0900f,0.7950f},
    {"ch2.low","LOW",0.1800f,0.7950f},
    {"ch2.mid","MID",0.2700f,0.7950f},
    {"ch2.high","HIGH",0.3600f,0.7950f},
    {"ch2.volume","VOLUME",0.4500f,0.7950f},
    {"ch3.gain","GAIN",0.0900f,0.7950f},
    {"ch3.low","LOW",0.1800f,0.7950f},
    {"ch3.mid","MID",0.2700f,0.7950f},
    {"ch3.high","HIGH",0.3600f,0.7950f},
    {"ch3.volume","VOLUME",0.4500f,0.7950f},
    {"ch4.gain","GAIN",0.0900f,0.7950f},
    {"ch4.low","LOW",0.1800f,0.7950f},
    {"ch4.mid","MID",0.2700f,0.7950f},
    {"ch4.high","HIGH",0.3600f,0.7950f},
    {"ch4.volume","VOLUME",0.4500f,0.7950f},
    {"presence","PRESENCE",0.5400f,0.7950f},
    {"depth","DEPTH",0.6300f,0.7950f},
    {"master","MASTER",0.7200f,0.7950f},
    {"gate_threshold","GATE",0.8100f,0.7950f},
    {"solo_level","SOLO",0.9000f,0.7950f},
};
// 16 — Iron Compact
inline constexpr Knob model16[]{
    {"hw.clean.gain","GAIN",0.0950f,0.7900f},
    {"hw.lead.gain","GAIN",0.0950f,0.7900f},
    {"hw.bass","BASS",0.2100f,0.7900f},
    {"hw.middle","MIDDLE",0.3250f,0.7900f},
    {"hw.treble","TREBLE",0.4400f,0.7900f},
    {"hw.lead.volume","VOLUME",0.5550f,0.7900f},
    {"hw.presence","PRESENCE",0.6700f,0.7900f},
    {"hw.master","MASTER",0.7850f,0.7900f},
    {"hw.reverb","REVERB",0.9000f,0.7900f},
};
// 17 — Fourfold
inline constexpr Knob model17[]{
    {"hw.ch1.gain","GAIN",0.1200f,0.8000f},
    {"hw.ch1.treble","TREBLE",0.2250f,0.8000f},
    {"hw.ch1.middle","MIDDLE",0.3300f,0.8000f},
    {"hw.ch1.bass","BASS",0.4350f,0.8000f},
    {"hw.ch1.volume","VOLUME",0.5400f,0.8000f},
    {"hw.ch2.gain","GAIN",0.1200f,0.8000f},
    {"hw.ch2.treble","TREBLE",0.2250f,0.8000f},
    {"hw.ch2.middle","MIDDLE",0.3300f,0.8000f},
    {"hw.ch2.bass","BASS",0.4350f,0.8000f},
    {"hw.ch2.volume","VOLUME",0.5400f,0.8000f},
    {"hw.ch3.gain","GAIN",0.1200f,0.8000f},
    {"hw.ch3.treble","TREBLE",0.2250f,0.8000f},
    {"hw.ch3.middle","MIDDLE",0.3300f,0.8000f},
    {"hw.ch3.bass","BASS",0.4350f,0.8000f},
    {"hw.ch3.volume","VOLUME",0.5400f,0.8000f},
    {"hw.ch4.gain","GAIN",0.1200f,0.8000f},
    {"hw.ch4.treble","TREBLE",0.2250f,0.8000f},
    {"hw.ch4.middle","MIDDLE",0.3300f,0.8000f},
    {"hw.ch4.bass","BASS",0.4350f,0.8000f},
    {"hw.ch4.volume","VOLUME",0.5400f,0.8000f},
    {"hw.presence","PRESENCE",0.6650f,0.8000f},
    {"hw.deep","DEEP",0.7700f,0.8000f},
    {"hw.master","MASTER",0.8750f,0.8000f},
};
// 18 — Classic Tube
inline constexpr Knob model18[]{
    {"hw.gain","GAIN",0.1600f,0.7900f},
    {"hw.bass","BASS",0.3300f,0.7900f},
    {"hw.midrange","MIDRANGE",0.5000f,0.7900f},
    {"hw.treble","TREBLE",0.6700f,0.7900f},
    {"hw.master","MASTER",0.8400f,0.7900f},
};
// 19 — Monolith
inline constexpr Knob model19[]{
    {"hw.normal","NORMAL",0.1500f,0.7800f},
    {"hw.brilliant","BRILLIANT",0.2900f,0.7800f},
    {"hw.bass","BASS",0.4300f,0.7800f},
    {"hw.midrange","MIDRANGE",0.5700f,0.7800f},
    {"hw.treble","TREBLE",0.7100f,0.7800f},
    {"hw.master","MASTER",0.8500f,0.7800f},
};
// 20 — Night Harvest
inline constexpr Knob model20[]{
    {"hw.ep.girth","GIRTH",0.0950f,0.6800f},
    {"hw.ep.grind","GRIND",0.2300f,0.6800f},
    {"hw.ep.gain","GAIN",0.3650f,0.6800f},
    {"hw.gain_eq.bass","BASS",0.5000f,0.6800f},
    {"hw.gain_eq.middle","MIDDLE",0.6350f,0.6800f},
    {"hw.gain_eq.sweep","SWEEP",0.7700f,0.6800f},
    {"hw.gain_eq.treble","TREBLE",0.9050f,0.6800f},
    {"hw.kk.gain1","GAIN 1",0.0950f,0.6800f},
    {"hw.kk.gain2","GAIN 2",0.3650f,0.6800f},
    {"hw.clean.volume","VOLUME",0.0950f,0.6800f},
    {"hw.clean.bass","BASS",0.3650f,0.6800f},
    {"hw.clean.middle","MIDDLE",0.6350f,0.6800f},
    {"hw.clean.treble","TREBLE",0.9050f,0.6800f},
    {"hw.depth","DEPTH",0.2300f,0.8650f},
    {"hw.presence","PRESENCE",0.4100f,0.8650f},
    {"hw.master1","MASTER 1",0.5900f,0.8650f},
    {"hw.master2","MASTER 2",0.7700f,0.8650f},
};
// 21 — Hot Lead
inline constexpr Knob model21[]{
    {"hw.normal.preamp","PREAMP",0.1200f,0.7900f},
    {"hw.normal.master","MASTER",0.2500f,0.7900f},
    {"hw.overdrive.preamp","PREAMP",0.1200f,0.7900f},
    {"hw.overdrive.master","MASTER",0.2500f,0.7900f},
    {"hw.bass","BASS",0.3800f,0.7900f},
    {"hw.middle","MIDDLE",0.5100f,0.7900f},
    {"hw.treble","TREBLE",0.6400f,0.7900f},
    {"hw.presence","PRESENCE",0.7700f,0.7900f},
    {"hw.depth","DEPTH",0.9000f,0.7900f},
};
// 22 — Blue Storm
inline constexpr Knob model22[]{
    {"hw.clean.gain","GAIN",0.1150f,0.7800f},
    {"hw.clean.bass","BASS",0.2400f,0.7800f},
    {"hw.clean.middle","MIDDLE",0.3650f,0.7800f},
    {"hw.clean.treble","TREBLE",0.4900f,0.7800f},
    {"hw.clean.volume","VOLUME",0.7400f,0.7800f},
    {"hw.lead.gain","GAIN",0.1150f,0.7800f},
    {"hw.lead.bass","BASS",0.2400f,0.7800f},
    {"hw.lead.middle","MIDDLE",0.3650f,0.7800f},
    {"hw.lead.treble","TREBLE",0.4900f,0.7800f},
    {"hw.lead.presence","PRESENCE",0.6150f,0.7800f},
    {"hw.lead.volume","VOLUME",0.7400f,0.7800f},
    {"hw.master","MASTER",0.8650f,0.7800f},
};
// 23 — Special Edition
inline constexpr Knob model23[]{
    {"hw.clean.gain","GAIN",0.0900f,0.7900f},
    {"hw.clean.treble","TREBLE",0.3900f,0.7900f},
    {"hw.clean.volume","VOLUME",0.4900f,0.7900f},
    {"hw.crunch.gain","GAIN",0.0900f,0.7900f},
    {"hw.crunch.treble","TREBLE",0.3900f,0.7900f},
    {"hw.crunch.volume","VOLUME",0.4900f,0.7900f},
    {"hw.lead1.gain","GAIN",0.0900f,0.7900f},
    {"hw.lead1.treble","TREBLE",0.3900f,0.7900f},
    {"hw.lead1.volume","VOLUME",0.4900f,0.7900f},
    {"hw.lead2.gain","GAIN",0.0900f,0.7900f},
    {"hw.lead2.treble","TREBLE",0.3900f,0.7900f},
    {"hw.lead2.volume","VOLUME",0.4900f,0.7900f},
    {"hw.clean_eq.bass","BASS",0.1900f,0.7900f},
    {"hw.clean_eq.middle","MIDDLE",0.2900f,0.7900f},
    {"hw.lead_eq.bass","BASS",0.1900f,0.7900f},
    {"hw.lead_eq.middle","MIDDLE",0.2900f,0.7900f},
    {"hw.presence_a","PRESENCE A",0.6100f,0.7900f},
    {"hw.presence_b","PRESENCE B",0.7100f,0.7900f},
    {"hw.master_a","MASTER A",0.8100f,0.7900f},
    {"hw.master_b","MASTER B",0.9100f,0.7900f},
};
// 24 — Náströnd
inline constexpr Knob model24[]{
    {"gain","GAIN",0.1900f,0.7040f},
    {"bass","BASS",0.2800f,0.7040f},
    {"middle","MID",0.3700f,0.7040f},
    {"mid_frequency","MID FREQ",0.4600f,0.7040f},
    {"treble","TREBLE",0.5500f,0.7040f},
    {"presence","PRESENCE",0.6400f,0.7040f},
    {"depth","DEPTH",0.7300f,0.7040f},
    {"master","MASTER",0.8200f,0.7040f},
    {"clank","CLANK",0.3100f,0.8450f},
    {"crush","CRUSH",0.4200f,0.8450f},
    {"impact","IMPACT",0.5300f,0.8450f},
    {"rot","ROT",0.6400f,0.8450f},
    {"bloom","BLOOM",0.7500f,0.8450f},
};
// 25 — Niflheimr
inline constexpr Knob model25[]{
    {"gain","GAIN",0.2050f,0.6050f},
    {"bass","BASS",0.3030f,0.6050f},
    {"middle","MID",0.4010f,0.6050f},
    {"mid_frequency","MID FREQ",0.4990f,0.6050f},
    {"treble","TREBLE",0.5970f,0.6050f},
    {"presence","PRESENCE",0.6950f,0.6050f},
    {"depth","DEPTH",0.7930f,0.6050f},
    {"mass","MASS",0.2050f,0.8100f},
    {"split","SPLIT",0.3030f,0.8100f},
    {"fang","FANG",0.4010f,0.8100f},
    {"fold","FOLD",0.4990f,0.8100f},
    {"thrust","THRUST",0.5970f,0.8100f},
    {"blend","BLEND",0.6950f,0.8100f},
    {"master","MASTER",0.7930f,0.8100f},
};
inline constexpr std::array<std::span<const Knob>,ampModelCount> modelKnobs{{
    model0, // Glass
    model1, // Brit Edge
    model2, // Tight 515
    model3, // Wide Rect
    model4, // Liquid Lead
    model5, // Iron Tube
    model6, // Solid Punch
    model7, // Modern Bass
    model8, // Chime 30
    model9, // Orange Crown
    model10, // Vintage Valve
    model11, // Metro Clean
    model12, // Prism Chime
    model13, // Silk Lead
    model14, // Taste Punch
    model15, // Cinder 120
    model16, // Iron Compact
    model17, // Fourfold
    model18, // Classic Tube
    model19, // Monolith
    model20, // Night Harvest
    model21, // Hot Lead
    model22, // Blue Storm
    model23, // Special Edition
    model24, // Náströnd
    model25 // Niflheimr
}};
inline const std::array<FaceStyle,ampModelCount> faceStyles{{
    {0.0470f,0.1100f,0xfff1ece0,{0.0350f,0.6250f,0.9300f,0.2850f},{},{}}, // Glass
    {0.0500f,0.1050f,0xff191b18,{0.0300f,0.5750f,0.9400f,0.3250f},{},{.755f,.71f,.195f,.10f}}, // Brit Edge
    {0.0450f,0.1100f,0xfff1ece0,{0.0420f,0.6000f,0.9150f,0.3100f},{},{}}, // Tight 515
    {0.0450f,0.1000f,0xfff1ece0,{0.0500f,0.6100f,0.9000f,0.2800f},{},{}}, // Wide Rect
    {0.0350f,0.1170f,0xfff1ece0,{0.0470f,0.5550f,0.9100f,0.3400f},{},{}}, // Liquid Lead
    {0.0580f,0.1550f,0xff191b18,{0.0600f,0.6000f,0.8800f,0.3000f},{},{}}, // Iron Tube
    {0.0430f,0.0970f,0xfff1ece0,{0.0570f,0.6300f,0.8850f,0.3450f},{},{}}, // Solid Punch
    {0.0380f,0.1080f,0xfff1ece0,{0.0650f,0.4900f,0.8700f,0.4600f},{},{}}, // Modern Bass
    {0.0400f,0.0960f,0xff191b18,{0.0450f,0.6400f,0.9100f,0.2800f},{},{}}, // Chime 30
    {0.0520f,0.1150f,0xff191b18,{0.0650f,0.4250f,0.8700f,0.4350f},{},{}}, // Orange Crown
    {0.0410f,0.1040f,0xff191b18,{0.0480f,0.6350f,0.9100f,0.2350f},{},{}}, // Vintage Valve
    {0.0360f,0.1750f,0xfff1ece0,{0.0650f,0.3400f,0.8700f,0.6100f},{},{}}, // Metro Clean
    {0.0550f,0.1550f,0xff191b18,{0.0400f,0.5600f,0.9200f,0.3350f},{},{}}, // Prism Chime
    {0.0430f,0.1090f,0xff191b18,{0.0400f,0.6500f,0.9250f,0.2650f},{},{}}, // Silk Lead
    {0.0400f,0.1180f,0xff191b18,{0.0550f,0.2550f,0.9000f,0.6800f},{},{}}, // Taste Punch
    {0.0350f,0.0860f,0xfff1ece0,{0.0400f,0.5700f,0.9200f,0.3250f},{},{}}, // Cinder 120
    {0.0440f,0.1080f,0xfff1ece0,{0.0400f,0.6000f,0.9200f,0.3000f},{},{}}, // Iron Compact
    {0.0430f,0.1010f,0xff191b18,{0.0550f,0.6500f,0.8850f,0.2350f},{},{}}, // Fourfold
    {0.0550f,0.1550f,0xfff1ece0,{0.0400f,0.6250f,0.9200f,0.2800f},{},{}}, // Classic Tube
    {0.0520f,0.1280f,0xff191b18,{0.0400f,0.5900f,0.9200f,0.3200f},{},{}}, // Monolith
    {0.0330f,0.1200f,0xfff1ece0,{0.0400f,0.5900f,0.9250f,0.3200f},{},{}}, // Night Harvest
    {0.0450f,0.1180f,0xff191b18,{0.0450f,0.6000f,0.9100f,0.2850f},{},{}}, // Hot Lead
    {0.0450f,0.1180f,0xfff1ece0,{0.0400f,0.6050f,0.9200f,0.2850f},{},{}}, // Blue Storm
    {0.0410f,0.0940f,0xfff1ece0,{0.0460f,0.6100f,0.9100f,0.2800f},{},{}}, // Special Edition
    {0.0320f,0.0860f,0xfff1ece0,{0.1700f,0.6180f,0.7030f,0.2830f},{.4007f,.5447f,.1981f,.0432f},{}}, // Náströnd
    // The complete Niflheimr shell has a taller plate: size its two rows to
    // that usable area instead of retaining the old miniature 3.2% dials.
    {0.0560f,0.0950f,0xfff1ece0,{0.1700f,0.4850f,0.7030f,0.4070f},{.4053f,.5637f,.1894f,.0468f},{}}, // Niflheimr
}};
static_assert(modelKnobs.size()==ampModelCount && faceStyles.size()==ampModelCount);
inline bool supported(int model) noexcept {return model>=0 && model<ampModelCount;}
inline const FaceStyle& faceStyle(int model) noexcept {
    return faceStyles[size_t(juce::jlimit(0,ampModelCount-1,model))];
}
inline int controlIndex(int model,const char* key) {
    return supported(model)?ampNativeControlIndex(model,key):-1;
}
inline std::vector<Knob> knobs(int model,int channel=-1) {
    std::vector<Knob> result;
    if(!supported(model))return result;
    for(const auto& knob:modelKnobs[size_t(model)]) {
        const int index=controlIndex(model,knob.key);
        if(index>=0 && (channel<0 || ampNativeControlVisible(model,index,channel)))result.push_back(knob);
    }
    return result;
}
// Large switch / route labels stay below the enclosure. They are real native
// controls, not decorative fascia markings. Full wording remains in the catalog.
inline const char* switchLabel(const AmpNativeControl& control) {
    const std::string_view key=control.key;
    if(key=="hw.r1.bright")return "GAIN BRIGHT";
    if(key=="hw.r2.fat")return "GAIN FAT";
    if(key=="hw.r2.presence_shift")return "PRESENCE SHIFT";
    if(key=="hw.lead.fat")return "GAIN FAT";
    if(key=="hw.lead.bright")return "DRIVE BRIGHT";
    if(key=="hw.lead.presence_shift")return "PRESENCE SHIFT";
    if(key=="hw.silent_recording")return "SILENT RECORDING";
    if(key=="presence_frequency_shift")return "PRESENCE SHIFT";
    if(key=="hw.mvb")return "MASTER BOOST";
    return control.label;
}
inline std::vector<Knob> switches(int model,int channel=-1) {
    std::vector<Knob> result;
    if(!supported(model))return result;
    const auto& panel=ampNativePanel(model);
    for(size_t i=0;i<panel.controls.size();++i) {
        const auto& control=panel.controls[i];
        if(control.kind==AmpNativeControlKind::knob || (channel>=0 && !ampNativeControlVisible(model,int(i),channel)))continue;
        const auto slot=result.size();
        result.push_back({control.key,switchLabel(control),.1f+.2f*float(slot%5),.25f+.5f*float(slot/5)});
    }
    return result;
}
}

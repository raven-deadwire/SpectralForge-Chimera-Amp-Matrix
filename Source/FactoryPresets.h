#pragma once
#include "FXParameters.h"
#include <array>
#include <string>

namespace spectralforge {
struct PresetParameter { const char* id; float value; };
enum class PresetKind { factory, signature };
struct FactoryPreset {
    const char* name;
    const char* category;
    const char* instrument;
    const char* description;
    PresetKind kind{PresetKind::factory};
    std::array<const char*,3> irTargets{};
    // Own the values instead of retaining initializer-list backing-array pointers.
    // Signature presets also carry five-slot/native state, so reserve more than
    // the legacy factory table while keeping all values constexpr and bounded.
    std::array<PresetParameter,192> parameters{};
    size_t parameterCount;

    template<size_t N>
    constexpr FactoryPreset(const char* presetName, const char* presetCategory,
                            const char* presetInstrument, const char* presetDescription,
                            const PresetParameter (&values)[N])
        : name(presetName), category(presetCategory), instrument(presetInstrument),
          description(presetDescription), parameterCount(N)
    {
        static_assert(N <= 192, "Increase preset parameter capacity");
        for(size_t i=0;i<N;++i) parameters[i]=values[i];
    }

    template<size_t N>
    constexpr FactoryPreset(const char* presetName, const char* presetCategory,
                            const char* presetInstrument, const char* presetDescription,
                            const PresetParameter (&values)[N], PresetKind presetKind,
                            const char* ir0, const char* ir1, const char* ir2)
        : name(presetName), category(presetCategory), instrument(presetInstrument),
          description(presetDescription), kind(presetKind), irTargets{ir0,ir1,ir2},
          parameterCount(N)
    {
        static_assert(N <= 192, "Increase preset parameter capacity");
        for(size_t i=0;i<N;++i) parameters[i]=values[i];
    }
};

// The original five ordinals and display names are retained. The remaining
// entries are appended; UI category grouping must keep these selection IDs.
// Bass presets use the built-in filter cabinet until a redistributable bass IR
// is part of the application. No preset resolves a personal/user IR pathname.
inline constexpr std::array<FactoryPreset,34> factoryPresets{{
    {"Clean Sustain", "Guitar / Clean & Ambient", "Guitar", "Soft optical compression, Glass clean and a short plate; balanced arpeggios.",
     {{"amp1",0},{"drive1",.15f},{"cabtype1",2},{"cablow1",75},{"cabhigh1",8000},{"precompon",1},{"compmodel",2},{"precomp",.25f},{"reverbon",1},{"reverbmix",.12f}}},
    {"Tight Rhythm", "Guitar / High Gain", "Guitar", "Tight 515 with a low-cut clean boost; dry, controlled metal rhythm.",
     {{"amp1",2},{"drive1",.45f},{"booston",1},{"boostgain",6},{"boostbass",-4},{"cablow1",85},{"cabhigh1",7200},{"buscompon",1},{"busratio",2},{"busthreshold",-14},{"output",-10}}},
    {"Bass Matrix", "Matrix / Bass", "Bass", "Clean LOW foundation, Modern Bass mids and Solid Punch attack; 180 Hz / 1.2 kHz splits.",
     {{"mode",2},{"amp1",5},{"amp2",7},{"amp3",6},{"drive1",0},{"drive2",.45f},{"drive3",.25f},{"lowcomp",.4f},{"lowampmix",0},{"x1",180},{"x2",1200},{"cab1",0},{"cab2",0},{"cab3",0},{"cabtype1",0},{"cabtype2",0},{"cabtype3",0},{"level2",-6},{"level3",-9}}},
    {"Filter Lead", "Guitar / Lead & Texture", "Guitar", "Pick-responsive wah into mild Brit Edge drive, with tempo-following repeats.",
     {{"amp1",1},{"drive1",.3f},{"filteron",1},{"filtersense",.6f},{"filtermix",.8f},{"preon",1},{"predrive",.2f},{"prelevel",-3},{"delayon",1},{"delaysync",1},{"delaymix",.18f},{"delayfeedback",.22f},{"cabhigh1",7200}}},
    {"Fuzz Texture", "Guitar / Lead & Texture", "Guitar", "Big Sustain fuzz into Glass, light chorus and plate; sustaining texture.",
     {{"amp1",0},{"drive1",.12f},{"fuzzon",1},{"fuzzdrive",22},{"fuzzlevel",-15},{"fuzztone",.4f},{"choruson",1},{"chorusmix",.15f},{"reverbon",1},{"reverbmix",.15f},{"cabhigh1",6500}}},
    {"Bell Clean", "Guitar / Clean & Ambient", "Guitar", "Chime 30 at low gain with a bright cabinet and a restrained spring tail.",
     {{"amp1",8},{"drive1",.07f},{"cabtype1",2},{"cablow1",80},{"cabhigh1",8500},{"bass1",-1},{"treble1",1},{"reverbon",1},{"reverbmodel",2},{"reverbmix",.09f},{"reverbsize",.25f}}},
    {"Edge Chime", "Guitar / Edge & Rock", "Guitar", "Chime 30 edge-of-breakup; EP-style lift for touch-sensitive picking.",
     {{"amp1",8},{"drive1",.3f},{"cabtype1",2},{"cablow1",85},{"cabhigh1",8200},{"booston",1},{"boostmodel",3},{"boostgain",2},{"boostbass",-1},{"reverbon",1},{"reverbmodel",2},{"reverbmix",.07f}}},
    {"Classic Crunch", "Guitar / Edge & Rock", "Guitar", "Brit Edge crunch, a mild treble lift and a compact spring; classic hard rock.",
     {{"amp1",1},{"drive1",.4f},{"lowmid1",1.5f},{"treble1",-1},{"presence1",1.5f},{"booston",1},{"boostmodel",1},{"boostgain",2},{"cablow1",85},{"cabhigh1",7000},{"reverbon",1},{"reverbmodel",2},{"reverbmix",.06f}}},
    {"Orange Heavy", "Guitar / High Gain", "Guitar", "Orange Crown thick low mids with modest drive; heavy rock and slower riffs.",
     {{"amp1",9},{"drive1",.42f},{"bass1",-1.5f},{"lowmid1",1},{"highmid1",1.5f},{"treble1",-.5f},{"presence1",1},{"cablow1",85},{"cabhigh1",6800},{"output",-10}}},
    {"Melodic Death Rhythm", "Guitar / High Gain", "Guitar", "Tight 515, low-drive Green 808 and focused upper mids; fast low-tuned rhythm.",
     {{"amp1",2},{"drive1",.4f},{"bass1",-2},{"lowmid1",-.5f},{"highmid1",1.8f},{"treble1",-1},{"presence1",2},{"resonance1",1},{"preon",1},{"drivemodel",0},{"predrive",.05f},{"pretone",5200},{"prelevel",-1},{"cablow1",80},{"cabhigh1",6900},{"gatethreshold",-58},{"gaterelease",65},{"gatehold",12},{"output",-11}}},
    {"Melodic Lead", "Guitar / Lead & Texture", "Guitar", "Liquid Lead with fuller mids, a dark delay and plate; articulate melodic solos.",
     {{"amp1",4},{"drive1",.44f},{"bass1",-1},{"lowmid1",1},{"highmid1",2},{"treble1",-1},{"presence1",1},{"cablow1",95},{"cabhigh1",7000},{"delayon",1},{"delaymodel",2},{"delaytime",360},{"delayfeedback",.24f},{"delaymix",.15f},{"reverbon",1},{"reverbmix",.09f},{"gatethreshold",-70},{"gaterelease",180},{"output",-10}}},
    {"Ambient Clean", "Guitar / Clean & Ambient", "Guitar", "Glass clean with stereo Dimension, tape repeats and hall; spacious layered parts.",
     {{"amp1",0},{"drive1",.1f},{"cabtype1",2},{"cabhigh1",8500},{"precompon",1},{"compmodel",4},{"precomp",.18f},{"choruson",1},{"modmodel",1},{"chorusdepth",.22f},{"chorusmix",.18f},{"delayon",1},{"delaymodel",1},{"delaytime",440},{"delayfeedback",.32f},{"delaymix",.22f},{"reverbon",1},{"reverbmodel",1},{"reverbsize",.66f},{"reverbmix",.22f},{"gatethreshold",-75}}},
    {"Finger Round", "Bass / Clean & Dynamics", "Bass", "Warm Bassman Valve with optical levelling and a 35 Hz low cut; fingerstyle foundation.",
     {{"amp1",10},{"drive1",.12f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",5800},{"bass1",1},{"lowmid1",1.5f},{"treble1",-1.5f},{"precompon",1},{"compmodel",2},{"precomp",.2f},{"precompattack",20},{"gatethreshold",-75}}},
    {"Pick Punch", "Bass / Clean & Dynamics", "Bass", "Solid Punch with FET peak control and upper-mid definition; clear picked rock bass.",
     {{"amp1",6},{"drive1",.2f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",6500},{"bass1",.5f},{"lowmid1",-.5f},{"highmid1",2},{"treble1",-.5f},{"precompon",1},{"compmodel",3},{"precomp",.2f},{"precompattack",12},{"gatethreshold",-70}}},
    {"Slap Studio", "Bass / Clean & Dynamics", "Bass", "Subway Clean, controlled peaks and a restrained low-mid scoop; modern slap.",
     {{"amp1",11},{"drive1",.06f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",10000},{"bass1",1.5f},{"lowmid1",-2.5f},{"highmid1",.5f},{"treble1",1},{"precompon",1},{"compmodel",0},{"precomp",.24f},{"precompattack",8},{"gatethreshold",-75}}},
    {"Modern Grind", "Bass / Drive & Texture", "Bass", "Modern Bass plus Micro Bass drive; retained lows with aggressive upper harmonics.",
     {{"amp1",7},{"drive1",.25f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",6200},{"bass1",.5f},{"lowmid1",-1},{"highmid1",1.5f},{"preon",1},{"drivemodel",4},{"predrive",.3f},{"pretone",6200},{"prelevel",-5},{"precompon",1},{"compmodel",3},{"precomp",.15f},{"precompattack",15},{"output",-10}}},
    {"Vintage Bass DI", "Bass / Drive & Texture", "Bass", "Bass DI overdrive into Iron Tube with soft compression; warm rock DI character.",
     {{"amp1",5},{"drive1",.17f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",5000},{"lowmid1",1},{"treble1",-1.5f},{"preon",1},{"drivemodel",3},{"predrive",.18f},{"pretone",4800},{"prelevel",-5},{"precompon",1},{"compmodel",4},{"precomp",.15f},{"precompattack",24},{"output",-10}}},
    {"Wool Bass Fuzz", "Bass / Drive & Texture", "Bass", "Wool Bass fuzz into Subway Clean; gated sustain with a filtered top end.",
     {{"amp1",11},{"drive1",.08f},{"cabtype1",0},{"cablow1",30},{"cabhigh1",4600},{"fuzzon",1},{"fuzzmodel",3},{"fuzzdrive",16},{"fuzztone",.35f},{"fuzzlevel",-17},{"gatethreshold",-76},{"gaterelease",150},{"output",-10}}},
    {"Bass Envelope", "Bass / Drive & Texture", "Bass", "Bass Envelope before a gentle VCA compressor; dry-low blend keeps the foundation.",
     {{"amp1",11},{"drive1",.06f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",7500},{"filteron",1},{"filtermodel",3},{"filtersense",.48f},{"filterq",1.35f},{"filtermix",.65f},{"preorder",1},{"precompon",1},{"compmodel",0},{"precomp",.12f},{"precompattack",20},{"gatethreshold",-75}}},
    {"G+G Clean / Crunch", "Dual / Blend", "Guitar", "Parallel Glass and Brit Edge, weighted toward clean; one input feeding two rigs.",
     {{"mode",1},{"dualtype",0},{"dualblend",.35f},{"amp1",0},{"drive1",.1f},{"cabtype1",2},{"amp2",1},{"drive2",.32f},{"level2",-3},{"cablow1",80},{"cablow2",85},{"cabhigh1",8200},{"cabhigh2",7200},{"reverbon",1},{"reverbmix",.07f}}},
    {"G+G Tight / Wide", "Dual / Blend", "Guitar", "Parallel Tight 515 and Wide Rect; tight centre with a lower-level broad second rig.",
     {{"mode",1},{"dualtype",0},{"dualblend",.35f},{"amp1",2},{"drive1",.38f},{"amp2",3},{"drive2",.3f},{"level2",-2},{"bass1",-1.5f},{"bass2",-2},{"highmid1",1},{"cablow1",85},{"cablow2",95},{"cabhigh1",7000},{"cabhigh2",6500},{"preon",1},{"predrive",.04f},{"pretone",5000},{"prelevel",-2},{"output",-11}}},
    {"G+B Low Anchor", "Dual / Crossover", "Bass / Low-tuned Guitar", "Crossover at 220 Hz: clean Subway lows and Tight 515 upper-band grit from one input.",
     {{"mode",1},{"dualtype",1},{"dualcross",220},{"dualblend",.5f},{"amp1",11},{"drive1",.06f},{"cabtype1",0},{"cablow1",30},{"cabhigh1",7000},{"amp2",2},{"drive2",.3f},{"level2",-6},{"cabtype2",1},{"cablow2",100},{"cabhigh2",6500},{"output",-10}}},
    {"G+B Air / Weight", "Dual / Crossover", "Bass / Guitar", "Crossover at 320 Hz: Bassman Valve weight and Chime 30 air; shared input, complementary bands.",
     {{"mode",1},{"dualtype",1},{"dualcross",320},{"dualblend",.5f},{"amp1",10},{"drive1",.12f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",6000},{"amp2",8},{"drive2",.24f},{"level2",-3},{"cabtype2",2},{"cablow2",90},{"cabhigh2",8000},{"highmid2",1},{"output",-10}}},
    {"B+B Warm / Definition", "Dual / Blend", "Bass", "Parallel Bassman Valve body and Subway Clean definition; finger or pick articulation.",
     {{"mode",1},{"dualtype",0},{"dualblend",.4f},{"amp1",10},{"drive1",.15f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",4800},{"amp2",11},{"drive2",.06f},{"cabtype2",0},{"cablow2",35},{"cabhigh2",8000},{"highmid2",1.5f},{"level2",-1},{"precompon",1},{"compmodel",0},{"precomp",.16f},{"precompattack",18},{"gatethreshold",-73}}},
    {"B+B Clean / Grind", "Dual / Crossover", "Bass", "Crossover at 250 Hz: Subway Clean lows and Modern Bass grind; low B stays defined.",
     {{"mode",1},{"dualtype",1},{"dualcross",250},{"dualblend",.5f},{"amp1",11},{"drive1",.05f},{"cabtype1",0},{"cablow1",28},{"cabhigh1",6500},{"amp2",7},{"drive2",.5f},{"level2",-5},{"cabtype2",0},{"cablow2",70},{"cabhigh2",6000},{"highmid2",1.5f},{"output",-10}}},
    {"Low B Foundation", "Matrix / Bass", "5-6 String Bass", "Clean compressed LOW below 140 Hz; Modern Bass mids and subdued pick attack above 1.4 kHz.",
     {{"mode",2},{"x1",140},{"x2",1400},{"lowcomp",.35f},{"lowampmix",0},{"amp1",11},{"drive1",0},{"amp2",7},{"drive2",.32f},{"level2",-5},{"amp3",6},{"drive3",.15f},{"level3",-8},{"cab1",0},{"cab2",1},{"cab3",1},{"cabtype1",0},{"cabtype2",0},{"cabtype3",0},{"cablow2",40},{"cabhigh2",6000},{"cablow3",40},{"cabhigh3",6000},{"bandtone2",1},{"bandtone3",-2},{"gatethreshold",-72}}},
    {"Pick Attack Matrix", "Matrix / Bass", "Bass", "Clean LOW below 180 Hz, warm Iron Tube mids and brighter Modern Bass above 1.6 kHz.",
     {{"mode",2},{"x1",180},{"x2",1600},{"lowcomp",.3f},{"lowampmix",0},{"amp1",11},{"drive1",0},{"amp2",5},{"drive2",.25f},{"level2",-4},{"amp3",7},{"drive3",.3f},{"level3",-7},{"cab1",0},{"cabtype1",0},{"cabtype2",0},{"cabtype3",0},{"cablow2",40},{"cabhigh2",6000},{"cablow3",40},{"cabhigh3",6800},{"bandtone2",.5f},{"bandtone3",1.5f},{"output",-10}}},
    {"Spectral Texture", "Matrix / Experimental", "Guitar / Bass", "Clean LOW anchor with Chime mids, Orange upper grit and slow phase; exploratory texture.",
     {{"mode",2},{"x1",200},{"x2",1800},{"lowcomp",.15f},{"lowampmix",0},{"amp1",11},{"drive1",0},{"amp2",8},{"drive2",.28f},{"level2",-5},{"amp3",9},{"drive3",.3f},{"level3",-9},{"cab1",0},{"cabtype1",0},{"cabtype2",2},{"cabtype3",1},{"cablow2",80},{"cabhigh2",7500},{"cablow3",100},{"cabhigh3",6000},{"choruson",1},{"modmodel",2},{"chorusrate",.18f},{"chorusdepth",.35f},{"chorusmix",.18f},{"reverbon",1},{"reverbmodel",1},{"reverbmix",.14f},{"output",-11}}},
    {"Matchless Edge", "Guitar / Edge & Rock", "Guitar", "Match Chime / Matchless DC-30-inspired edge, light optical control and spring; articulate boutique chime.",
     {{"amp1",12},{"drive1",.28f},{"cabtype1",2},{"cablow1",85},{"cabhigh1",8000},{"bass1",-1},{"lowmid1",.5f},{"treble1",-.5f},{"precompon",1},{"compmodel",2},{"precomp",.1f},{"precompattack",25},{"reverbon",1},{"reverbmodel",2},{"reverbsize",.25f},{"reverbmix",.07f}}},
    {"Dumble Smooth Lead", "Guitar / Lead & Texture", "Guitar", "Silk ODS / Dumble-inspired smooth mids, Gold Drive then gentle boost, and dark repeats; expressive lead.",
     {{"amp1",13},{"drive1",.38f},{"bass1",-1},{"lowmid1",1.5f},{"highmid1",1},{"treble1",-1},{"presence1",.5f},{"cablow1",95},{"cabhigh1",7000},{"preon",1},{"drivemodel",1},{"predrive",.08f},{"pretone",5800},{"prelevel",-4},{"booston",1},{"boostmodel",2},{"boostgain",2},{"gainorder",1},{"delayon",1},{"delaymodel",1},{"delaytime",330},{"delayfeedback",.22f},{"delaymix",.12f},{"reverbon",1},{"reverbmix",.1f},{"gatethreshold",-73},{"gaterelease",160},{"output",-10}}},
    {"EICH Modern Clean", "Bass / Clean & Dynamics", "Bass", "Taste Punch / EICH T900-inspired clean bass, mild VCA control and broad bandwidth; transparent modern foundation.",
     {{"amp1",14},{"drive1",.07f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",9000},{"bass1",.5f},{"lowmid1",-.5f},{"highmid1",1},{"treble1",.5f},{"precompon",1},{"compmodel",0},{"precomp",.16f},{"precompattack",18},{"gatethreshold",-75}}},

    {"Crom Cruach", "SIGNATURE / Deadwire", "Bass", "Deadwire signature: balanced Matrix foundation with EICH low body, Modern Bass grind and Tight 515 attack.",
     {
      {"mode",2},{"x1",200},{"x2",1800},{"output",-10},{"gatethreshold",-62},{"gaterelease",90},{"gatehold",15},
      {"lowcomp",.15f},{"lowampmix",.50f},
      {"amp1",14},{"ampext1",0},{"drive1",0},{"level1",0},{"bandtone1",0},{"cab1",1},{"cablow1",70},{"cabhigh1",8000},{"cabtype1",0},
      {"amp2",7},{"ampext2",0},{"drive2",.50f},{"level2",-4},{"bandtone2",0},{"cab2",1},{"cablow2",80},{"cabhigh2",7500},{"cabtype2",0},
      {"amp3",2},{"ampext3",0},{"drive3",.50f},{"level3",-8},{"bandtone3",0},{"cab3",1},{"cablow3",100},{"cabhigh3",6000},{"cabtype3",0},
      {"nativeAmp_c3_enabled",1},{"nativeAmp_c3_model",14},{"nativeAmp_c3_m14_channel",0},{"nativeAmp_c3_m14_route",0},{"nativeAmp_c3_inputTrim",0},{"nativeAmp_c3_outputLevel",0},
      {"nativeAmp_c3_m14_hw_gain",.50f},{"nativeAmp_c3_m14_hw_taste",.50f},{"nativeAmp_c3_m14_hw_lo",.50f},{"nativeAmp_c3_m14_hw_lo_mid",.50f},{"nativeAmp_c3_m14_hw_hi_mid",.50f},{"nativeAmp_c3_m14_hw_hi",.50f},{"nativeAmp_c3_m14_hw_master",.50f},{"nativeAmp_c3_m14_hw_mute",0},
      {"nativeAmp_c4_enabled",1},{"nativeAmp_c4_model",7},{"nativeAmp_c4_m7_channel",0},{"nativeAmp_c4_m7_route",0},{"nativeAmp_c4_inputTrim",0},{"nativeAmp_c4_outputLevel",-4},
      {"nativeAmp_c4_m7_hw_b7k_master",.50f},{"nativeAmp_c4_m7_hw_b7k_blend",.50f},{"nativeAmp_c4_m7_hw_b7k_level",.50f},{"nativeAmp_c4_m7_hw_b7k_drive",.50f},{"nativeAmp_c4_m7_hw_b7k_bass",.50f},{"nativeAmp_c4_m7_hw_b7k_lo_mids",.50f},{"nativeAmp_c4_m7_hw_b7k_hi_mids",.50f},{"nativeAmp_c4_m7_hw_b7k_treble",.50f},{"nativeAmp_c4_m7_hw_b7k_attack",1},{"nativeAmp_c4_m7_hw_b7k_grunt",1},{"nativeAmp_c4_m7_hw_b7k_lo_frequency",0},{"nativeAmp_c4_m7_hw_b7k_hi_frequency",0},{"nativeAmp_c4_m7_hw_b7k_distortion",1},
      {"nativeAmp_c4_m7_hw_db751_gain",.50f},{"nativeAmp_c4_m7_hw_db751_bass",.50f},{"nativeAmp_c4_m7_hw_db751_mid",.50f},{"nativeAmp_c4_m7_hw_db751_treble",.50f},{"nativeAmp_c4_m7_hw_db751_master",.50f},{"nativeAmp_c4_m7_hw_db751_deep",0},{"nativeAmp_c4_m7_hw_db751_bright",0},
      {"nativeAmp_c5_enabled",1},{"nativeAmp_c5_model",2},{"nativeAmp_c5_m2_channel",0},{"nativeAmp_c5_m2_route",0},{"nativeAmp_c5_inputTrim",0},{"nativeAmp_c5_outputLevel",-8},
      {"nativeAmp_c5_m2_hw_low",.50f},{"nativeAmp_c5_m2_hw_mid",.50f},{"nativeAmp_c5_m2_hw_high",.50f},{"nativeAmp_c5_m2_hw_resonance",.50f},{"nativeAmp_c5_m2_hw_presence",.50f},{"nativeAmp_c5_m2_hw_rhythm_pre_gain",.50f},{"nativeAmp_c5_m2_hw_rhythm_post_gain",.50f},{"nativeAmp_c5_m2_hw_rhythm_bright",0},{"nativeAmp_c5_m2_hw_rhythm_crunch",0},
      {"boardEnabled",1},{"boardLowTap",2},
      {"board0Model",11},{"boardOrder0",0},{"board0_m11_bypass",1},
      {"board0_m11_hw_mode",0},{"board0_m11_hw_sweep",0},{"board0_m11_hw_range",1},{"board0_m11_hw_peak",.50f},{"board0_m11_hw_gain",.50f},{"board0_m11_hw_boost",0},
      {"board1Model",9},{"boardOrder1",1},{"board1_m9_bypass",0},
      {"board1_m9_hw_in",.50f},{"board1_m9_hw_out",.50f},{"board1_m9_hw_dry",.50f},{"board1_m9_hw_ratio",.50f},{"board1_m9_hw_attack",.50f},{"board1_m9_hw_release",.50f},
      {"board2Model",16},{"boardOrder2",2},{"board2_m16_bypass",1},{"board2_m16_hw_volume",.50f},{"board2_m16_hw_tone",.50f},{"board2_m16_hw_sustain",.50f},
      {"board3Model",21},{"boardOrder3",3},{"board3_m21_bypass",1},{"board3_m21_hw_gain",.50f},{"board3_m21_hw_volume",.50f},{"board3_m21_hw_treble",.50f},{"board3_m21_hw_bass",.50f},
      {"board4Model",5},{"boardOrder4",4},{"board4_m5_bypass",0},{"board4_m5_hw_blend",.50f},{"board4_m5_hw_tone",.50f},{"board4_m5_hw_level",.50f},{"board4_m5_hw_drive",.50f},{"board4_m5_hw_grunt",1},{"board4_m5_hw_mid_boost",0},
      {"pn_bus_native",1},{"pn_bus_model",1},{"pn_bus_m1_bypass",0},{"pn_bus_m1_input",2},{"pn_bus_m1_output",0},{"pn_bus_m1_attack",.50f},{"pn_bus_m1_release",.50f},{"pn_bus_m1_attack_off",0},{"pn_bus_m1_ratio",0},{"pn_bus_m1_meter",0},
      {"pn_preamp_native",1},{"pn_preamp_model",0},{"pn_preamp_m0_bypass",0},{"pn_preamp_m0_gain",2},{"pn_preamp_m0_phase",1},
      {"pn_eq_native",1},{"pn_eq_model",0},{"pn_eq_m0_bypass",0},{"pn_eq_m0_hf_gain",0},{"pn_eq_m0_hmf_gain",0},{"pn_eq_m0_lmf_gain",0},{"pn_eq_m0_lf_gain",0},{"pn_eq_m0_black",0},{"pn_eq_m0_eq_in",1},
      {"choruson",0},{"modmodel",2},{"delayon",0},{"delaymodel",0},{"delaytime",250},{"delayfeedback",.25f},{"delaymix",.20f},
      {"reverbon",1},{"reverbmodel",1},{"reverbsize",.35f},{"reverbdamping",.55f},{"reverbmix",.14f}
     }, PresetKind::signature, "", "DYN 421.wav", "Mar1960_Raven_SM57_In.wav"},

    {"Wild Hunt", "SIGNATURE / Deadwire", "Bass", "Deadwire signature: lighter low foundation, forward mids and open upper attack with restrained stereo motion.",
     {
      {"mode",2},{"x1",165},{"x2",1500},{"output",-10.5f},{"gatethreshold",-59},{"gaterelease",65},{"gatehold",10},
      {"lowcomp",.14f},{"lowampmix",.30f},
      {"amp1",14},{"ampext1",0},{"drive1",0},{"level1",-1.5f},{"bandtone1",-0.5f},{"cab1",1},{"cablow1",35},{"cabhigh1",6500},{"cabtype1",0},
      {"amp2",8},{"ampext2",0},{"drive2",.24f},{"level2",-2.5f},{"bandtone2",1.2f},{"cab2",1},{"cablow2",90},{"cabhigh2",7800},{"cabtype2",2},
      {"amp3",2},{"ampext3",0},{"drive3",.32f},{"level3",-5.5f},{"bandtone3",1.8f},{"cab3",1},{"cablow3",105},{"cabhigh3",7000},{"cabtype3",0},
      {"nativeAmp_c3_enabled",1},{"nativeAmp_c3_model",14},{"nativeAmp_c3_m14_channel",0},{"nativeAmp_c3_m14_route",0},{"nativeAmp_c3_inputTrim",0},{"nativeAmp_c3_outputLevel",-1.5f},
      {"nativeAmp_c3_m14_hw_gain",.38f},{"nativeAmp_c3_m14_hw_taste",.52f},{"nativeAmp_c3_m14_hw_lo",.48f},{"nativeAmp_c3_m14_hw_lo_mid",.48f},{"nativeAmp_c3_m14_hw_hi_mid",.54f},{"nativeAmp_c3_m14_hw_hi",.54f},{"nativeAmp_c3_m14_hw_master",.50f},{"nativeAmp_c3_m14_hw_mute",0},
      {"nativeAmp_c4_enabled",1},{"nativeAmp_c4_model",8},{"nativeAmp_c4_m8_channel",1},{"nativeAmp_c4_m8_route",0},{"nativeAmp_c4_inputTrim",0},{"nativeAmp_c4_outputLevel",-2.5f},
      {"nativeAmp_c4_m8_hw_normal_volume",.50f},{"nativeAmp_c4_m8_hw_top_boost_volume",.35f},{"nativeAmp_c4_m8_hw_top_boost_treble",.60f},{"nativeAmp_c4_m8_hw_top_boost_bass",.42f},{"nativeAmp_c4_m8_hw_tone_cut",.42f},{"nativeAmp_c4_m8_hw_master_volume",.50f},{"nativeAmp_c4_m8_hw_reverb_tone",.50f},{"nativeAmp_c4_m8_hw_reverb_level",0},{"nativeAmp_c4_m8_hw_tremolo_speed",.50f},{"nativeAmp_c4_m8_hw_tremolo_depth",0},
      {"nativeAmp_c5_enabled",1},{"nativeAmp_c5_model",2},{"nativeAmp_c5_m2_channel",0},{"nativeAmp_c5_m2_route",0},{"nativeAmp_c5_inputTrim",0},{"nativeAmp_c5_outputLevel",-5.5f},
      {"nativeAmp_c5_m2_hw_low",.40f},{"nativeAmp_c5_m2_hw_mid",.52f},{"nativeAmp_c5_m2_hw_high",.56f},{"nativeAmp_c5_m2_hw_resonance",.50f},{"nativeAmp_c5_m2_hw_presence",.62f},{"nativeAmp_c5_m2_hw_rhythm_pre_gain",.38f},{"nativeAmp_c5_m2_hw_rhythm_post_gain",.50f},{"nativeAmp_c5_m2_hw_rhythm_bright",0},{"nativeAmp_c5_m2_hw_rhythm_crunch",0},
      {"boardEnabled",1},{"boardLowTap",1},
      {"board0Model",9},{"boardOrder0",0},{"board0_m9_bypass",0},{"board0_m9_hw_in",.46f},{"board0_m9_hw_out",.50f},{"board0_m9_hw_dry",.30f},{"board0_m9_hw_ratio",.42f},{"board0_m9_hw_attack",.62f},{"board0_m9_hw_release",.56f},
      {"board1Model",2},{"boardOrder1",1},{"board1_m2_bypass",0},{"board1_m2_hw_gain",.12f},{"board1_m2_hw_treble",.58f},{"board1_m2_hw_output",.68f},
      {"board2Model",5},{"boardOrder2",2},{"board2_m5_bypass",0},{"board2_m5_hw_blend",.32f},{"board2_m5_hw_tone",.58f},{"board2_m5_hw_level",.48f},{"board2_m5_hw_drive",.20f},{"board2_m5_hw_grunt",0},{"board2_m5_hw_mid_boost",1},
      {"board3Model",0},{"boardOrder3",3},{"board4Model",0},{"boardOrder4",4},
      {"pn_bus_native",1},{"pn_bus_model",0},{"pn_bus_m0_bypass",0},{"pn_bus_m0_threshold",-16},{"pn_bus_m0_makeup",1},{"pn_bus_m0_attack",4},{"pn_bus_m0_release",4},{"pn_bus_m0_ratio",0},{"pn_bus_m0_compressor_in",1},
      {"pn_preamp_native",1},{"pn_preamp_model",2},{"pn_preamp_m2_bypass",0},{"pn_preamp_m2_input",1},{"pn_preamp_m2_gain",0},{"pn_preamp_m2_trim",0},{"pn_preamp_m2_phase",0},{"pn_preamp_m2_high_pass",0},
      {"pn_eq_native",1},{"pn_eq_model",0},{"pn_eq_m0_bypass",0},
      {"pn_eq_m0_hf_gain",.5f},{"pn_eq_m0_hf_frequency",7500},{"pn_eq_m0_hmf_gain",1.8f},{"pn_eq_m0_hmf_frequency",2200},{"pn_eq_m0_hmf_q",1.1f},
      {"pn_eq_m0_lmf_gain",-1.2f},{"pn_eq_m0_lmf_frequency",450},{"pn_eq_m0_lmf_q",.8f},{"pn_eq_m0_lf_gain",.3f},{"pn_eq_m0_lf_frequency",90},{"pn_eq_m0_eq_in",1},
      {"choruson",1},{"modmodel",1},{"chorusrate",.18f},{"chorusdepth",.18f},{"chorusmix",.10f},
      {"delayon",0},{"delaymodel",0},{"reverbon",1},{"reverbmodel",0},{"reverbsize",.24f},{"reverbdamping",.62f},{"reverbmix",.06f}
     }, PresetKind::signature, "", "", "Mar1960_Raven_SM57_In.wav"},

    {"Azhi Dahaka", "SIGNATURE / Deadwire", "Bass", "Deadwire signature: dense centre low-end, compressed foundation and focused upper-mid bite with restrained fizz.",
     {
      {"mode",2},{"x1",135},{"x2",1200},{"output",-11},{"gatethreshold",-62},{"gaterelease",100},{"gatehold",15},
      {"lowcomp",.30f},{"lowampmix",.30f},
      {"amp1",0},{"ampext1",4},{"ampchannel1_m18",0},{"drive1",0},{"level1",1.5f},{"bandtone1",0},{"cab1",1},{"cablow1",30},{"cabhigh1",5000},{"cabtype1",0},
      {"amp2",7},{"ampext2",0},{"drive2",.55f},{"level2",-5.5f},{"bandtone2",.5f},{"cab2",1},{"cablow2",70},{"cabhigh2",6000},{"cabtype2",0},
      {"amp3",0},{"ampext3",6},{"ampchannel3_m20",0},{"drive3",.38f},{"level3",-8},{"bandtone3",-2.0f},{"cab3",1},{"cablow3",110},{"cabhigh3",5500},{"cabtype3",0},
      {"nativeAmp_c3_enabled",1},{"nativeAmp_c3_model",18},{"nativeAmp_c3_m18_channel",0},{"nativeAmp_c3_m18_route",0},{"nativeAmp_c3_inputTrim",0},{"nativeAmp_c3_outputLevel",1.5f},
      {"nativeAmp_c3_m18_hw_gain",.38f},{"nativeAmp_c3_m18_hw_bass",.56f},{"nativeAmp_c3_m18_hw_midrange",.55f},{"nativeAmp_c3_m18_hw_treble",.44f},{"nativeAmp_c3_m18_hw_master",.50f},{"nativeAmp_c3_m18_hw_mid_frequency",2},{"nativeAmp_c3_m18_hw_ultra_hi",0},{"nativeAmp_c3_m18_hw_ultra_lo",0},
      {"nativeAmp_c4_enabled",1},{"nativeAmp_c4_model",7},{"nativeAmp_c4_m7_channel",0},{"nativeAmp_c4_m7_route",0},{"nativeAmp_c4_inputTrim",0},{"nativeAmp_c4_outputLevel",-5.5f},
      {"nativeAmp_c4_m7_hw_b7k_master",.50f},{"nativeAmp_c4_m7_hw_b7k_blend",.50f},{"nativeAmp_c4_m7_hw_b7k_level",.50f},{"nativeAmp_c4_m7_hw_b7k_drive",.55f},{"nativeAmp_c4_m7_hw_b7k_bass",.48f},{"nativeAmp_c4_m7_hw_b7k_lo_mids",.55f},{"nativeAmp_c4_m7_hw_b7k_hi_mids",.56f},{"nativeAmp_c4_m7_hw_b7k_treble",.43f},{"nativeAmp_c4_m7_hw_b7k_attack",1},{"nativeAmp_c4_m7_hw_b7k_grunt",1},{"nativeAmp_c4_m7_hw_b7k_lo_frequency",1},{"nativeAmp_c4_m7_hw_b7k_hi_frequency",1},{"nativeAmp_c4_m7_hw_b7k_distortion",1},
      {"nativeAmp_c4_m7_hw_db751_gain",.50f},{"nativeAmp_c4_m7_hw_db751_bass",.52f},{"nativeAmp_c4_m7_hw_db751_mid",.55f},{"nativeAmp_c4_m7_hw_db751_treble",.45f},{"nativeAmp_c4_m7_hw_db751_master",.50f},{"nativeAmp_c4_m7_hw_db751_deep",0},{"nativeAmp_c4_m7_hw_db751_bright",0},
      {"nativeAmp_c5_enabled",1},{"nativeAmp_c5_model",20},{"nativeAmp_c5_m20_channel",0},{"nativeAmp_c5_m20_route",0},{"nativeAmp_c5_inputTrim",0},{"nativeAmp_c5_outputLevel",-8},
      {"nativeAmp_c5_m20_hw_ep_girth",.55f},{"nativeAmp_c5_m20_hw_ep_grind",.62f},{"nativeAmp_c5_m20_hw_ep_gain",.42f},{"nativeAmp_c5_m20_hw_kk_gain1",.50f},{"nativeAmp_c5_m20_hw_kk_gain2",.50f},
      {"nativeAmp_c5_m20_hw_gain_eq_bass",.45f},{"nativeAmp_c5_m20_hw_gain_eq_middle",.58f},{"nativeAmp_c5_m20_hw_gain_eq_sweep",.55f},{"nativeAmp_c5_m20_hw_gain_eq_treble",.42f},
      {"nativeAmp_c5_m20_hw_clean_volume",.50f},{"nativeAmp_c5_m20_hw_clean_bass",.50f},{"nativeAmp_c5_m20_hw_clean_middle",.50f},{"nativeAmp_c5_m20_hw_clean_treble",.50f},
      {"nativeAmp_c5_m20_hw_depth",.55f},{"nativeAmp_c5_m20_hw_presence",.50f},{"nativeAmp_c5_m20_hw_master1",.50f},{"nativeAmp_c5_m20_hw_master2",.50f},{"nativeAmp_c5_m20_hw_master2_select",0},
      {"boardEnabled",1},{"boardLowTap",1},
      {"board0Model",9},{"boardOrder0",0},{"board0_m9_bypass",0},{"board0_m9_hw_in",.52f},{"board0_m9_hw_out",.48f},{"board0_m9_hw_dry",.22f},{"board0_m9_hw_ratio",.58f},{"board0_m9_hw_attack",.54f},{"board0_m9_hw_release",.62f},
      {"board1Model",5},{"boardOrder1",1},{"board1_m5_bypass",0},{"board1_m5_hw_blend",.44f},{"board1_m5_hw_tone",.43f},{"board1_m5_hw_level",.50f},{"board1_m5_hw_drive",.42f},{"board1_m5_hw_grunt",1},{"board1_m5_hw_mid_boost",1},
      {"board2Model",4},{"boardOrder2",2},{"board2_m4_bypass",0},{"board2_m4_hw_level",.48f},{"board2_m4_hw_blend",.35f},{"board2_m4_hw_treble",.40f},{"board2_m4_hw_mid",.60f},{"board2_m4_hw_bass",.46f},{"board2_m4_hw_presence",.38f},{"board2_m4_hw_drive",.20f},{"board2_m4_hw_mid_shift",1},{"board2_m4_hw_bass_shift",0},
      {"board3Model",0},{"boardOrder3",3},{"board4Model",0},{"boardOrder4",4},
      {"pn_bus_native",1},{"pn_bus_model",1},{"pn_bus_m1_bypass",0},{"pn_bus_m1_input",3},{"pn_bus_m1_output",-2},{"pn_bus_m1_attack",.65f},{"pn_bus_m1_release",.72f},{"pn_bus_m1_attack_off",0},{"pn_bus_m1_ratio",1},{"pn_bus_m1_meter",0},
      {"pn_preamp_native",1},{"pn_preamp_model",0},{"pn_preamp_m0_bypass",0},{"pn_preamp_m0_gain",3},{"pn_preamp_m0_phase",0},{"pn_preamp_m0_softwareTrim",-3},
      {"pn_eq_native",1},{"pn_eq_model",2},{"pn_eq_m2_bypass",0},{"pn_eq_m2_lf_frequency",2},{"pn_eq_m2_lf_boost",2},{"pn_eq_m2_lf_atten",1},{"pn_eq_m2_bandwidth",.65f},{"pn_eq_m2_hf_boost",1.5f},{"pn_eq_m2_hf_frequency",2},{"pn_eq_m2_hf_atten",2},{"pn_eq_m2_hf_atten_frequency",1},{"pn_eq_m2_eq_in",1},
      {"choruson",0},{"delayon",0},{"reverbon",1},{"reverbmodel",0},{"reverbsize",.20f},{"reverbdamping",.65f},{"reverbmix",.05f}
     }, PresetKind::signature, "", "DYN 421.wav", "Mar1960_Raven_SM57_Out.wav"}
}};
inline constexpr int factoryPresetCount = static_cast<int>(factoryPresets.size());
inline bool isSignaturePreset(int index) {
    return index>=0 && index<factoryPresetCount && factoryPresets[(size_t)index].kind==PresetKind::signature;
}
inline const char* presetIRTarget(int index,int lane) {
    return index>=0 && index<factoryPresetCount && lane>=0 && lane<3 ? factoryPresets[(size_t)index].irTargets[(size_t)lane] : "";
}

// Full sound initialization keeps preset recall independent of the previous
// sound. Performance controls (input gain/routing, tempo, click and tuner),
// external IR assets, MIDI mapping and A/B snapshots remain the caller's state.
inline constexpr std::array<PresetParameter,16> factoryGlobalDefaults{{
    {"mode",0},{"x1",150},{"x2",1200},{"output",-9},{"gateon",1},
    {"gatethreshold",-65},{"gaterelease",80},{"gatehold",20},
    {"transposeon",0},{"transpose",0},{"oversampling",2},{"lowcomp",0},
    {"lowampmix",0},{"dualtype",0},{"dualblend",.5f},{"dualcross",350}
}};
inline constexpr std::array<PresetParameter,18> factoryLaneDefaults{{
    {"amp",0},{"drive",.15f},{"level",0},{"bass",0},{"lowmid",0},
    {"highmid",0},{"treble",0},{"presence",0},{"resonance",0},
    {"bandtone",0},{"mute",0},{"solo",0},{"polarity",0},{"cab",1},
    {"cablow",70},{"cabhigh",8000},{"cabtype",1},{"ampon",1}
}};

template<class Setter>
bool applyFactoryPreset(int index, Setter&& set)
{
    if(index < 0 || index >= factoryPresetCount) return false;
    for(const auto& value : factoryGlobalDefaults) set(value.id,value.value);
    for(int lane=1;lane<=3;++lane)
        for(const auto& value : factoryLaneDefaults) {
            const auto id=std::string(value.id)+std::to_string(lane);
            set(id.c_str(),value.value);
        }
    for(const auto& spec : fxSpecs) set(spec.id,spec.initial);
    for(const auto& family : modelFamilies) set(family.parameter,0.f);
    set("preorder",1.f);set("gainorder",0.f);
    set("doubleron",0.f);set("doublertime",6.f);
    const auto& preset=factoryPresets[static_cast<size_t>(index)];
    if(preset.kind==PresetKind::signature) {
        set("boardEnabled",1.f);set("boardLowTap",2.f);
        for(int owner=0;owner<5;++owner) {
            const auto model="board"+std::to_string(owner)+"Model";
            const auto order="boardOrder"+std::to_string(owner);
            set(model.c_str(),0.f);set(order.c_str(),float(owner));
        }
        for(int lane=1;lane<=3;++lane) {
            const auto ext="ampext"+std::to_string(lane);
            set(ext.c_str(),0.f);
        }
        for(int context=3;context<=5;++context) {
            const auto enabled="nativeAmp_c"+std::to_string(context)+"_enabled";
            set(enabled.c_str(),0.f);
        }
        set("pn_bus_native",0.f);set("pn_preamp_native",0.f);set("pn_eq_native",0.f);
    }
    for(size_t i=0;i<preset.parameterCount;++i) {
        const auto& value=preset.parameters[i];
        set(value.id,value.value);
    }
    return true;
}
}

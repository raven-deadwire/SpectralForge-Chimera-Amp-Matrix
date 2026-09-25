#pragma once
#include "FXParameters.h"
#include <array>
#include <initializer_list>
#include <string>

namespace spectralforge {
struct PresetParameter { const char* id; float value; };
struct FactoryPreset {
    const char* name;
    const char* category;
    const char* instrument;
    const char* description;
    std::initializer_list<PresetParameter> parameters;
};

// The original five ordinals and display names are retained. The remaining
// entries are appended; UI category grouping must keep these selection IDs.
// Bass presets use the built-in filter cabinet until a redistributable bass IR
// is part of the application. No preset resolves a personal/user IR pathname.
inline constexpr std::array<FactoryPreset,31> factoryPresets{{
    {"Clean Sustain", "Guitar / Clean", "Guitar", "Soft optical compression, Glass clean and a short plate; balanced arpeggios.",
     {{"amp1",0},{"drive1",.15f},{"cabtype1",2},{"cablow1",75},{"cabhigh1",8000},{"precompon",1},{"compmodel",2},{"precomp",.25f},{"reverbon",1},{"reverbmix",.12f}}},
    {"Tight Rhythm", "Guitar / High Gain", "Guitar", "Tight 515 with a low-cut clean boost; dry, controlled metal rhythm.",
     {{"amp1",2},{"drive1",.45f},{"booston",1},{"boostgain",6},{"boostbass",-4},{"cablow1",85},{"cabhigh1",7200},{"buscompon",1},{"busratio",2},{"busthreshold",-14},{"output",-10}}},
    {"Bass Matrix", "Matrix / Bass", "Bass", "Clean LOW foundation, Modern Bass mids and Solid Punch attack; 180 Hz / 1.2 kHz splits.",
     {{"mode",2},{"amp1",5},{"amp2",7},{"amp3",6},{"drive1",0},{"drive2",.45f},{"drive3",.25f},{"lowcomp",.4f},{"lowampmix",0},{"x1",180},{"x2",1200},{"cab1",0},{"cab2",0},{"cab3",0},{"cabtype1",0},{"cabtype2",0},{"cabtype3",0},{"level2",-6},{"level3",-9}}},
    {"Filter Lead", "Texture / Envelope", "Guitar", "Pick-responsive wah into mild Brit Edge drive, with tempo-following repeats.",
     {{"amp1",1},{"drive1",.3f},{"filteron",1},{"filtersense",.6f},{"filtermix",.8f},{"preon",1},{"predrive",.2f},{"prelevel",-3},{"delayon",1},{"delaysync",1},{"delaymix",.18f},{"delayfeedback",.22f},{"cabhigh1",7200}}},
    {"Fuzz Texture", "Texture / Fuzz", "Guitar", "Big Sustain fuzz into Glass, light chorus and plate; sustaining texture.",
     {{"amp1",0},{"drive1",.12f},{"fuzzon",1},{"fuzzdrive",22},{"fuzzlevel",-15},{"fuzztone",.4f},{"choruson",1},{"chorusmix",.15f},{"reverbon",1},{"reverbmix",.15f},{"cabhigh1",6500}}},
    {"Bell Clean", "Guitar / Clean", "Guitar", "Chime 30 at low gain with a bright cabinet and a restrained spring tail.",
     {{"amp1",8},{"drive1",.07f},{"cabtype1",2},{"cablow1",80},{"cabhigh1",8500},{"bass1",-1},{"treble1",1},{"reverbon",1},{"reverbmodel",2},{"reverbmix",.09f},{"reverbsize",.25f}}},
    {"Edge Chime", "Guitar / Edge", "Guitar", "Chime 30 edge-of-breakup; EP-style lift for touch-sensitive picking.",
     {{"amp1",8},{"drive1",.3f},{"cabtype1",2},{"cablow1",85},{"cabhigh1",8200},{"booston",1},{"boostmodel",3},{"boostgain",2},{"boostbass",-1},{"reverbon",1},{"reverbmodel",2},{"reverbmix",.07f}}},
    {"Classic Crunch", "Guitar / Rock", "Guitar", "Brit Edge crunch, a mild treble lift and a compact spring; classic hard rock.",
     {{"amp1",1},{"drive1",.4f},{"lowmid1",1.5f},{"treble1",-1},{"presence1",1.5f},{"booston",1},{"boostmodel",1},{"boostgain",2},{"cablow1",85},{"cabhigh1",7000},{"reverbon",1},{"reverbmodel",2},{"reverbmix",.06f}}},
    {"Orange Heavy", "Guitar / High Gain", "Guitar", "Orange Crown thick low mids with modest drive; heavy rock and slower riffs.",
     {{"amp1",9},{"drive1",.42f},{"bass1",-1.5f},{"lowmid1",1},{"highmid1",1.5f},{"treble1",-.5f},{"presence1",1},{"cablow1",85},{"cabhigh1",6800},{"output",-10}}},
    {"Melodic Death Rhythm", "Guitar / High Gain", "Guitar", "Tight 515, low-drive Green 808 and focused upper mids; fast low-tuned rhythm.",
     {{"amp1",2},{"drive1",.4f},{"bass1",-2},{"lowmid1",-.5f},{"highmid1",1.8f},{"treble1",-1},{"presence1",2},{"resonance1",1},{"preon",1},{"drivemodel",0},{"predrive",.05f},{"pretone",5200},{"prelevel",-1},{"cablow1",80},{"cabhigh1",6900},{"gatethreshold",-58},{"gaterelease",65},{"gatehold",12},{"output",-11}}},
    {"Melodic Lead", "Guitar / Lead", "Guitar", "Liquid Lead with fuller mids, a dark delay and plate; articulate melodic solos.",
     {{"amp1",4},{"drive1",.44f},{"bass1",-1},{"lowmid1",1},{"highmid1",2},{"treble1",-1},{"presence1",1},{"cablow1",95},{"cabhigh1",7000},{"delayon",1},{"delaymodel",2},{"delaytime",360},{"delayfeedback",.24f},{"delaymix",.15f},{"reverbon",1},{"reverbmix",.09f},{"gatethreshold",-70},{"gaterelease",180},{"output",-10}}},
    {"Ambient Clean", "Guitar / Clean", "Guitar", "Glass clean with stereo Dimension, tape repeats and hall; spacious layered parts.",
     {{"amp1",0},{"drive1",.1f},{"cabtype1",2},{"cabhigh1",8500},{"precompon",1},{"compmodel",4},{"precomp",.18f},{"choruson",1},{"modmodel",1},{"chorusdepth",.22f},{"chorusmix",.18f},{"delayon",1},{"delaymodel",1},{"delaytime",440},{"delayfeedback",.32f},{"delaymix",.22f},{"reverbon",1},{"reverbmodel",1},{"reverbsize",.66f},{"reverbmix",.22f},{"gatethreshold",-75}}},
    {"Finger Round", "Bass / Clean", "Bass", "Warm Bassman Valve with optical levelling and a 35 Hz low cut; fingerstyle foundation.",
     {{"amp1",10},{"drive1",.12f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",5800},{"bass1",1},{"lowmid1",1.5f},{"treble1",-1.5f},{"precompon",1},{"compmodel",2},{"precomp",.2f},{"precompattack",20},{"gatethreshold",-75}}},
    {"Pick Punch", "Bass / Pick", "Bass", "Solid Punch with FET peak control and upper-mid definition; clear picked rock bass.",
     {{"amp1",6},{"drive1",.2f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",6500},{"bass1",.5f},{"lowmid1",-.5f},{"highmid1",2},{"treble1",-.5f},{"precompon",1},{"compmodel",3},{"precomp",.2f},{"precompattack",12},{"gatethreshold",-70}}},
    {"Slap Studio", "Bass / Slap", "Bass", "Subway Clean, controlled peaks and a restrained low-mid scoop; modern slap.",
     {{"amp1",11},{"drive1",.06f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",10000},{"bass1",1.5f},{"lowmid1",-2.5f},{"highmid1",.5f},{"treble1",1},{"precompon",1},{"compmodel",0},{"precomp",.24f},{"precompattack",8},{"gatethreshold",-75}}},
    {"Modern Grind", "Bass / Drive", "Bass", "Modern Bass plus Micro Bass drive; retained lows with aggressive upper harmonics.",
     {{"amp1",7},{"drive1",.25f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",6200},{"bass1",.5f},{"lowmid1",-1},{"highmid1",1.5f},{"preon",1},{"drivemodel",4},{"predrive",.3f},{"pretone",6200},{"prelevel",-5},{"precompon",1},{"compmodel",3},{"precomp",.15f},{"precompattack",15},{"output",-10}}},
    {"Vintage Bass DI", "Bass / Drive", "Bass", "Bass DI overdrive into Iron Tube with soft compression; warm rock DI character.",
     {{"amp1",5},{"drive1",.17f},{"cabtype1",0},{"cablow1",35},{"cabhigh1",5000},{"lowmid1",1},{"treble1",-1.5f},{"preon",1},{"drivemodel",3},{"predrive",.18f},{"pretone",4800},{"prelevel",-5},{"precompon",1},{"compmodel",4},{"precomp",.15f},{"precompattack",24},{"output",-10}}},
    {"Wool Bass Fuzz", "Bass / Fuzz", "Bass", "Wool Bass fuzz into Subway Clean; gated sustain with a filtered top end.",
     {{"amp1",11},{"drive1",.08f},{"cabtype1",0},{"cablow1",30},{"cabhigh1",4600},{"fuzzon",1},{"fuzzmodel",3},{"fuzzdrive",16},{"fuzztone",.35f},{"fuzzlevel",-17},{"gatethreshold",-76},{"gaterelease",150},{"output",-10}}},
    {"Bass Envelope", "Bass / Envelope", "Bass", "Bass Envelope before a gentle VCA compressor; dry-low blend keeps the foundation.",
     {{"amp1",11},{"drive1",.06f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",7500},{"filteron",1},{"filtermodel",3},{"filtersense",.48f},{"filterq",1.35f},{"filtermix",.65f},{"preorder",1},{"precompon",1},{"compmodel",0},{"precomp",.12f},{"precompattack",20},{"gatethreshold",-75}}},
    {"G+G Clean / Crunch", "Dual / Guitar + Guitar", "Guitar", "Parallel Glass and Brit Edge, weighted toward clean; one input feeding two rigs.",
     {{"mode",1},{"dualtype",0},{"dualblend",.35f},{"amp1",0},{"drive1",.1f},{"cabtype1",2},{"amp2",1},{"drive2",.32f},{"level2",-3},{"cablow1",80},{"cablow2",85},{"cabhigh1",8200},{"cabhigh2",7200},{"reverbon",1},{"reverbmix",.07f}}},
    {"G+G Tight / Wide", "Dual / Guitar + Guitar", "Guitar", "Parallel Tight 515 and Wide Rect; tight centre with a lower-level broad second rig.",
     {{"mode",1},{"dualtype",0},{"dualblend",.35f},{"amp1",2},{"drive1",.38f},{"amp2",3},{"drive2",.3f},{"level2",-2},{"bass1",-1.5f},{"bass2",-2},{"highmid1",1},{"cablow1",85},{"cablow2",95},{"cabhigh1",7000},{"cabhigh2",6500},{"preon",1},{"predrive",.04f},{"pretone",5000},{"prelevel",-2},{"output",-11}}},
    {"G+B Low Anchor", "Dual / Guitar + Bass", "Bass / Low-tuned Guitar", "Crossover at 220 Hz: clean Subway lows and Tight 515 upper-band grit from one input.",
     {{"mode",1},{"dualtype",1},{"dualcross",220},{"dualblend",.5f},{"amp1",11},{"drive1",.06f},{"cabtype1",0},{"cablow1",30},{"cabhigh1",7000},{"amp2",2},{"drive2",.3f},{"level2",-6},{"cabtype2",1},{"cablow2",100},{"cabhigh2",6500},{"output",-10}}},
    {"G+B Air / Weight", "Dual / Guitar + Bass", "Bass / Guitar", "Crossover at 320 Hz: Bassman Valve weight and Chime 30 air; shared input, complementary bands.",
     {{"mode",1},{"dualtype",1},{"dualcross",320},{"dualblend",.5f},{"amp1",10},{"drive1",.12f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",6000},{"amp2",8},{"drive2",.24f},{"level2",-3},{"cabtype2",2},{"cablow2",90},{"cabhigh2",8000},{"highmid2",1},{"output",-10}}},
    {"B+B Warm / Definition", "Dual / Bass + Bass", "Bass", "Parallel Bassman Valve body and Subway Clean definition; finger or pick articulation.",
     {{"mode",1},{"dualtype",0},{"dualblend",.4f},{"amp1",10},{"drive1",.15f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",4800},{"amp2",11},{"drive2",.06f},{"cabtype2",0},{"cablow2",35},{"cabhigh2",8000},{"highmid2",1.5f},{"level2",-1},{"precompon",1},{"compmodel",0},{"precomp",.16f},{"precompattack",18},{"gatethreshold",-73}}},
    {"B+B Clean / Grind", "Dual / Bass + Bass", "Bass", "Crossover at 250 Hz: Subway Clean lows and Modern Bass grind; low B stays defined.",
     {{"mode",1},{"dualtype",1},{"dualcross",250},{"dualblend",.5f},{"amp1",11},{"drive1",.05f},{"cabtype1",0},{"cablow1",28},{"cabhigh1",6500},{"amp2",7},{"drive2",.5f},{"level2",-5},{"cabtype2",0},{"cablow2",70},{"cabhigh2",6000},{"highmid2",1.5f},{"output",-10}}},
    {"Low B Foundation", "Matrix / Bass", "5-6 String Bass", "Clean compressed LOW below 140 Hz; Modern Bass mids and subdued pick attack above 1.4 kHz.",
     {{"mode",2},{"x1",140},{"x2",1400},{"lowcomp",.35f},{"lowampmix",0},{"amp1",11},{"drive1",0},{"amp2",7},{"drive2",.32f},{"level2",-5},{"amp3",6},{"drive3",.15f},{"level3",-8},{"cab1",0},{"cab2",1},{"cab3",1},{"cabtype1",0},{"cabtype2",0},{"cabtype3",0},{"cablow2",40},{"cabhigh2",6000},{"cablow3",40},{"cabhigh3",6000},{"bandtone2",1},{"bandtone3",-2},{"gatethreshold",-72}}},
    {"Pick Attack Matrix", "Matrix / Bass", "Bass", "Clean LOW below 180 Hz, warm Iron Tube mids and brighter Modern Bass above 1.6 kHz.",
     {{"mode",2},{"x1",180},{"x2",1600},{"lowcomp",.3f},{"lowampmix",0},{"amp1",11},{"drive1",0},{"amp2",5},{"drive2",.25f},{"level2",-4},{"amp3",7},{"drive3",.3f},{"level3",-7},{"cab1",0},{"cabtype1",0},{"cabtype2",0},{"cabtype3",0},{"cablow2",40},{"cabhigh2",6000},{"cablow3",40},{"cabhigh3",6800},{"bandtone2",.5f},{"bandtone3",1.5f},{"output",-10}}},
    {"Spectral Texture", "Matrix / Experimental", "Guitar / Bass", "Clean LOW anchor with Chime mids, Orange upper grit and slow phase; exploratory texture.",
     {{"mode",2},{"x1",200},{"x2",1800},{"lowcomp",.15f},{"lowampmix",0},{"amp1",11},{"drive1",0},{"amp2",8},{"drive2",.28f},{"level2",-5},{"amp3",9},{"drive3",.3f},{"level3",-9},{"cab1",0},{"cabtype1",0},{"cabtype2",2},{"cabtype3",1},{"cablow2",80},{"cabhigh2",7500},{"cablow3",100},{"cabhigh3",6000},{"choruson",1},{"modmodel",2},{"chorusrate",.18f},{"chorusdepth",.35f},{"chorusmix",.18f},{"reverbon",1},{"reverbmodel",1},{"reverbmix",.14f},{"output",-11}}},
    {"Matchless Edge", "Guitar / Edge", "Guitar", "Match Chime / Matchless DC-30-inspired edge, light optical control and spring; articulate boutique chime.",
     {{"amp1",12},{"drive1",.28f},{"cabtype1",2},{"cablow1",85},{"cabhigh1",8000},{"bass1",-1},{"lowmid1",.5f},{"treble1",-.5f},{"precompon",1},{"compmodel",2},{"precomp",.1f},{"precompattack",25},{"reverbon",1},{"reverbmodel",2},{"reverbsize",.25f},{"reverbmix",.07f}}},
    {"Dumble Smooth Lead", "Guitar / Lead", "Guitar", "Silk ODS / Dumble-inspired smooth mids, Gold Drive then gentle boost, and dark repeats; expressive lead.",
     {{"amp1",13},{"drive1",.38f},{"bass1",-1},{"lowmid1",1.5f},{"highmid1",1},{"treble1",-1},{"presence1",.5f},{"cablow1",95},{"cabhigh1",7000},{"preon",1},{"drivemodel",1},{"predrive",.08f},{"pretone",5800},{"prelevel",-4},{"booston",1},{"boostmodel",2},{"boostgain",2},{"gainorder",1},{"delayon",1},{"delaymodel",1},{"delaytime",330},{"delayfeedback",.22f},{"delaymix",.12f},{"reverbon",1},{"reverbmix",.1f},{"gatethreshold",-73},{"gaterelease",160},{"output",-10}}},
    {"EICH Modern Clean", "Bass / Clean", "Bass", "Taste Punch / EICH T900-inspired clean bass, mild VCA control and broad bandwidth; transparent modern foundation.",
     {{"amp1",14},{"drive1",.07f},{"cabtype1",0},{"cablow1",32},{"cabhigh1",9000},{"bass1",.5f},{"lowmid1",-.5f},{"highmid1",1},{"treble1",.5f},{"precompon",1},{"compmodel",0},{"precomp",.16f},{"precompattack",18},{"gatethreshold",-75}}}
}};
inline constexpr int factoryPresetCount = static_cast<int>(factoryPresets.size());

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
    for(const auto& value : factoryPresets[static_cast<size_t>(index)].parameters)
        set(value.id,value.value);
    return true;
}
}

#pragma once
#include "FactoryPresetLevels.h"
#include <algorithm>
#include <array>
#include <string>

namespace spectralforge {
// Authored factory recall values, not changes to parameter defaults or saved
// user sounds. The existing PRE/amp/CAB/POST recipes still supply the character.
// Dynamic correction uses Bell nodes: the released shelf detector is band-pass
// based, so a shelf is used only for a small static finishing adjustment here.
enum class FactoryFinishingRole { clean, edge, rhythm, lead, finger, pick, slap, matrix, envelope, fuzz };

struct FactoryFinishingDynamics {
    float threshold{-18},ratio{1.5f},attack{30},release{180},knee{6},mix{.5f},detectorHP{90};
    int detector{1}; // 0: Peak, 1: RMS. Detector HP never filters the audio path.
    int gateMode{1}; // Gentle downward expansion for exposed notes and tails.
    float gateRatio{2},gateAttack{.7f},minimumHold{25},minimumRelease{140},maximumRange{42};
    float dynamicAttack{15},dynamicRelease{160},dynamicRatio{2},bodyQ{1},harshQ{1.3f};
    // Expander-mode roles move the gentle expansion to the Expander stage, at
    // the authored gate threshold; the main gate then closes only well below it.
    float expanderRange{15};
    // Hard-gate roles add a gentle PRE/POST Gate 2 on the other side of the rig,
    // below the main threshold, so less hiss reaches the amp between phrases.
    float gate2Offset{-6},gate2Ratio{2},gate2Range{18};
};

inline constexpr FactoryFinishingDynamics factoryFinishingDynamics(FactoryFinishingRole role) {
    FactoryFinishingDynamics d;
    switch(role) {
        case FactoryFinishingRole::clean:
            d.threshold=-24;d.detectorHP=100;d.minimumHold=35;d.minimumRelease=180;break;
        case FactoryFinishingRole::edge:
            d.threshold=-21;d.ratio=1.4f;d.attack=35;d.release=150;d.mix=.4f;
            d.minimumRelease=120;d.maximumRange=48;break;
        case FactoryFinishingRole::rhythm:
            d.threshold=-14;d.ratio=1.4f;d.attack=30;d.release=110;d.mix=.35f;d.detectorHP=120;
            d.gateMode=0;d.gateAttack=.5f;d.minimumHold=12;d.minimumRelease=65;d.maximumRange=72;
            d.dynamicAttack=10;d.dynamicRelease=110;d.bodyQ=1.1f;break;
        case FactoryFinishingRole::lead:
            d.threshold=-21;d.attack=25;d.release=180;d.mix=.4f;d.detectorHP=100;
            d.minimumHold=35;d.minimumRelease=180;d.dynamicAttack=18;d.dynamicRelease=180;break;
        case FactoryFinishingRole::finger:
            d.threshold=-22;d.ratio=1.6f;d.attack=35;d.release=200;d.mix=.55f;d.detectorHP=70;
            d.minimumHold=35;d.minimumRelease=180;d.dynamicAttack=25;d.dynamicRelease=200;break;
        case FactoryFinishingRole::pick:
            d.threshold=-22;d.ratio=1.6f;d.attack=25;d.release=150;d.mix=.45f;d.detectorHP=85;
            d.minimumHold=20;d.minimumRelease=100;d.maximumRange=54;
            d.dynamicAttack=14;d.dynamicRelease=140;break;
        case FactoryFinishingRole::slap:
            d.threshold=-24;d.ratio=1.8f;d.attack=20;d.release=130;d.knee=4;d.mix=.45f;d.detectorHP=80;d.detector=0;
            d.gateRatio=2.5f;d.minimumHold=20;d.minimumRelease=120;d.maximumRange=48;
            d.dynamicAttack=12;d.dynamicRelease=140;break;
        case FactoryFinishingRole::matrix:
            d.threshold=-20;d.ratio=1.4f;d.attack=35;d.release=180;d.mix=.35f;d.detectorHP=90;
            d.gateRatio=2.2f;d.minimumHold=20;d.minimumRelease=100;d.maximumRange=54;
            d.dynamicAttack=20;d.dynamicRelease=160;d.bodyQ=1.1f;break;
        case FactoryFinishingRole::envelope:
            d.attack=40;d.release=220;d.mix=.25f;d.detectorHP=80;
            d.gateRatio=1.6f;d.minimumHold=45;d.minimumRelease=200;d.maximumRange=36;d.expanderRange=9;
            d.dynamicAttack=30;d.dynamicRelease=220;d.dynamicRatio=1.5f;break;
        case FactoryFinishingRole::fuzz:
            d.attack=40;d.release=240;d.mix=.25f;d.detectorHP=90;
            d.gateRatio=1.5f;d.minimumHold=65;d.minimumRelease=240;d.maximumRange=30;d.expanderRange=9;
            d.dynamicAttack=30;d.dynamicRelease=220;d.dynamicRatio=1.7f;d.bodyQ=.9f;break;
    }
    return d;
}

struct FactoryFinishingRecipe {
    const char* name;
    FactoryFinishingRole role;
    bool compressor;
    float bodyHz,bodyGainDb,bodyRangeDb;
    float definitionHz,definitionGainDb;
    float harshHz,harshRangeDb,highShelfDb;
    float bodyThresholdDb,harshThresholdDb;
};

// Stable menu ordinals. Cuts are deliberately bounded; there is no additional
// audio high-pass, automatic makeup or hidden OUTPUT trim. Body/harsh detector
// thresholds belong to their respective Tone/Final stages, not to input dBFS.
// Calibrated against the existing pluck fixture's stage envelopes. Quiet upper
// bands retain their conservative thresholds; absent fixture harmonics are not
// an instruction to lower the threshold until every detector reports a cut.
// The eleven presets with an authored POST compressor keep common COMP off.
inline constexpr std::array<FactoryFinishingRecipe,48> factoryFinishingRecipes{{
    {"Clean Sustain",FactoryFinishingRole::clean,true,       300,-.5f,1.0f, 1800,.5f, 3800,1.0f,-.5f, -19,-26},
    {"Tight Rhythm",FactoryFinishingRole::rhythm,false,       160,-.5f,2.0f, 1600,.5f, 4200,2.0f,-.5f, -12,-13},
    {"Bass Matrix",FactoryFinishingRole::matrix,false,       240,-.5f,1.5f, 1400,.5f, 3200,1.5f,-.5f, -15,-26},
    {"Filter Lead",FactoryFinishingRole::envelope,false,     320,-.5f,1.0f, 1400,  0, 4000,1.0f,-.5f, -19,-15},
    {"Fuzz Texture",FactoryFinishingRole::fuzz,false,         350,-.5f,1.5f, 1200,.5f, 3500,1.5f,-.5f, -20,-25},
    {"Bell Clean",FactoryFinishingRole::clean,true,           280,-.5f,1.0f, 2200,.5f, 4000,1.0f,   0, -18.5f,-26},
    {"Edge Chime",FactoryFinishingRole::edge,true,            300,-.5f,1.0f, 1600,.5f, 3800,1.0f,-.5f, -17.5f,-25},
    {"Classic Crunch",FactoryFinishingRole::edge,true,        250,-.5f,1.5f, 1500,.5f, 3600,1.5f,-.5f, -25.5f,-20.5f},
    {"Amber Heavy",FactoryFinishingRole::rhythm,false,        200,-.5f,2.0f, 1400,.5f, 3800,1.5f,-.5f, -13,-15},
    {"Melodic Death Rhythm",FactoryFinishingRole::rhythm,false,170,-.5f,2.0f,1800,.5f, 4300,2.0f,-.5f, -16,-13.5f},
    {"Melodic Lead",FactoryFinishingRole::lead,true,           280,-.5f,1.5f, 1200,.5f, 3800,1.5f,-.5f, -17.5f,-25},
    {"Ambient Clean",FactoryFinishingRole::clean,false,       350,-.5f,1.0f, 1800,.5f, 4200,1.0f,-.5f, -24.5f,-26},
    {"Finger Round",FactoryFinishingRole::finger,true,        280,-.5f,1.0f,  900,.5f, 2800,1.0f,-.5f, -15.5f,-26},
    {"Pick Punch",FactoryFinishingRole::pick,true,            300,-.5f,1.5f, 1500,.5f, 3200,1.5f,-.5f, -18.5f,-25},
    {"Slap Studio",FactoryFinishingRole::slap,true,           400,-.5f,1.5f, 2200,.5f, 4200,2.0f,   0, -26,-24},
    {"Modern Grind",FactoryFinishingRole::pick,false,         250,-.5f,1.5f, 1400,.5f, 3400,1.5f,-.5f, -15,-25},
    {"Vintage Bass DI",FactoryFinishingRole::finger,true,     250,-.5f,1.0f,  800,.5f, 2600,1.0f,-.5f, -15.5f,-26},
    {"Wool Bass Fuzz",FactoryFinishingRole::fuzz,false,       320,-.5f,1.5f, 1000,.5f, 2800,1.0f,-.5f, -21,-25},
    {"Bass Envelope",FactoryFinishingRole::envelope,false,   320,-.5f,1.0f, 1200,  0, 3000,1.0f,   0, -24.5f,-24},
    {"G+G Clean / Crunch",FactoryFinishingRole::edge,true,    280,-.5f,1.0f, 1800,.5f, 3900,1.5f,-.5f, -21,-25},
    {"G+G Tight / Wide",FactoryFinishingRole::rhythm,false,   160,-.5f,2.0f, 1700,.5f, 4400,2.0f,-.5f, -18.5f,-15.5f},
    {"G+B Low Anchor",FactoryFinishingRole::matrix,false,     220,-.5f,1.5f, 1400,.5f, 3400,1.5f,-.5f, -18.5f,-23.5f},
    {"G+B Air / Weight",FactoryFinishingRole::matrix,true,    280,-.5f,1.0f, 1600,.5f, 3800,1.0f,-.5f, -14.5f,-26},
    {"B+B Warm / Definition",FactoryFinishingRole::finger,true,280,-.5f,1.0f,1100,.5f,3000,1.0f,-.5f, -18,-26},
    {"B+B Clean / Grind",FactoryFinishingRole::matrix,true,   250,-.5f,1.5f, 1500,.5f, 3400,1.5f,-.5f, -13.5f,-25},
    {"Low B Foundation",FactoryFinishingRole::matrix,true,   200,   0,1.0f,  900,.5f, 2800,1.0f,-.5f, -14,-26},
    {"Pick Attack Matrix",FactoryFinishingRole::pick,true,    280,-.5f,1.5f, 1600,.5f, 3500,1.5f,-.5f, -12,-25},
    {"Spectral Texture",FactoryFinishingRole::fuzz,false,     350,-.5f,1.0f, 1400,.5f, 3600,1.0f,-.5f, -18,-26},
    {"Prism Edge",FactoryFinishingRole::edge,true,            300,-.5f,1.0f, 1900,.5f, 4000,1.0f,-.5f, -18,-26},
    {"Silk Smooth Lead",FactoryFinishingRole::lead,true,      300,-.5f,1.0f, 1100,.5f, 3600,1.5f,-.5f, -14,-22.5f},
    {"Modern Clean",FactoryFinishingRole::finger,true,        300,-.5f,1.0f, 1600,.5f, 3600,1.0f,   0, -22,-26},
    {"Crom Cruach",FactoryFinishingRole::matrix,false,        240,-.5f,1.5f, 1400,.5f, 3400,1.5f,-.5f, -9,-25},
    {"Wild Hunt",FactoryFinishingRole::matrix,false,          280,-.5f,1.0f, 1800,.5f, 3800,1.0f,   0, -8.5f,-19.5f},
    {"Azhi Dahaka",FactoryFinishingRole::matrix,false,        220,-.5f,1.5f, 1200,.5f, 3000,1.5f,-.5f, -9,-25},
    {"A Path To Alsatia",FactoryFinishingRole::lead,false,    250,-.5f,1.0f, 1200,.5f, 4000,1.5f,-.5f, -12.5f,-22},
    {"Feel My Wrath",FactoryFinishingRole::rhythm,false,      170,-.5f,2.0f, 1500,.5f, 4200,2.0f,-.5f, -18.5f,-21.5f},
    {"Blackhearted",FactoryFinishingRole::rhythm,false,       180,-.5f,1.5f, 1800,.5f, 4300,2.0f,-.5f, -16.5f,-19},
    {"Dark Matters of Throne",FactoryFinishingRole::rhythm,false,200,-.5f,1.5f,1500,.5f,3800,1.5f,-.5f, -15,-22.5f},
    {"Thall Rhythm",FactoryFinishingRole::rhythm,false,       160,   0,2.0f, 1600,  0, 4300,2.0f,-.5f, -13,-14.5f},
    {"Molten Lead",FactoryFinishingRole::lead,true,           280,-.5f,1.5f, 1100,.5f, 3800,1.5f,-.5f, -14,-19.5f},
    {"Rotten Grind",FactoryFinishingRole::rhythm,false,       200,   0,1.5f, 1800,  0, 4000,1.5f,-.5f, -7.5f,-15.5f},
    {"Sludge Mass",FactoryFinishingRole::fuzz,false,          350,-.5f,1.5f, 1100,.5f, 3000,1.0f,-.5f, -21,-26},
    {"Slam Impact",FactoryFinishingRole::rhythm,false,        180,   0,2.0f, 1400,  0, 4200,2.0f,-.5f, -9.5f,-19},
    {"Metal Clank",FactoryFinishingRole::pick,true,           280,-.5f,1.0f, 1600,  0, 3400,1.5f,-.5f, -18,-25},
    {"Death Grind",FactoryFinishingRole::matrix,false,        240,-.5f,1.5f, 1200,  0, 3200,1.5f,-.5f, -20.5f,-25},
    {"Heavy Pulse",FactoryFinishingRole::matrix,false,        220,-.5f,1.0f, 1800,  0, 3600,1.5f,-.5f, -17.5f,-25},
    {"Slam Punch",FactoryFinishingRole::matrix,false,         200,   0,1.5f,  900,  0, 3000,1.5f,-.5f, -21,-25},
    {"Doom Fuzz",FactoryFinishingRole::fuzz,false,            320,-.5f,1.5f, 1000,.5f, 2800,1.0f,-.5f, -22.5f,-26}
}};
static_assert(factoryFinishingRecipes.size()==factoryOutputDb.size(),"Every selectable factory preset needs a finishing recipe");

template<class Getter,class Setter>
bool voiceFactoryFinishing(int index,Getter get,Setter set) {
    if(index<0 || index>=int(factoryFinishingRecipes.size()))return false;
    const auto& r=factoryFinishingRecipes[size_t(index)];
    const auto d=factoryFinishingDynamics(r.role);
    const auto band=[&](const char* bank,int b,int type,float hz,float gain,float q,bool enabled) {
        const auto prefix=std::string(bank)+"EQ_b"+std::to_string(b)+"_";
        set((prefix+"enabled").c_str(),enabled?1.f:0.f);set((prefix+"type").c_str(),float(type));
        set((prefix+"frequency").c_str(),hz);set((prefix+"gain").c_str(),gain);set((prefix+"q").c_str(),q);
    };
    set("toneEQ_bypass",0);set("finalEQ_bypass",0);
    band("tone",1,0,r.bodyHz,r.bodyGainDb,d.bodyQ,true);
    band("tone",2,0,r.definitionHz,r.definitionGainDb,.8f,r.definitionGainDb!=0);
    band("final",1,2,std::min(10000.f,r.harshHz*1.6f),r.highShelfDb,.70710678f,r.highShelfDb!=0);
    band("final",2,0,r.harshHz,0,d.harshQ,true);
    const auto dynamic=[&](int slot,int target,float threshold,float range) {
        const auto prefix="dynamicEQ"+std::to_string(slot)+"_";
        set((prefix+"target").c_str(),float(target));set((prefix+"threshold").c_str(),threshold);
        set((prefix+"range").c_str(),range);set((prefix+"attack").c_str(),d.dynamicAttack);
        set((prefix+"release").c_str(),d.dynamicRelease);set((prefix+"ratio").c_str(),d.dynamicRatio);
    };
    dynamic(1,1,r.bodyThresholdDb,r.bodyRangeDb); // Tone band 1, before common COMP and POST.
    dynamic(2,14,r.harshThresholdDb,r.harshRangeDb); // Final band 2, after POST effects.
    set("dynamicEQ3_target",0);set("dynamicEQ4_target",0);
    set("cleanCompOn",r.compressor?1.f:0.f);set("cleanCompThreshold",d.threshold);set("cleanCompRatio",d.ratio);
    set("cleanCompAttack",d.attack);set("cleanCompRelease",d.release);set("cleanCompKnee",d.knee);
    set("cleanCompMakeup",0);set("cleanCompMix",d.mix);set("cleanCompHP",d.detectorHP);set("cleanCompDetector",float(d.detector));
    // Preserve longer authored tails and gentler finite attenuation (notably Doom).
    // Input calibration and LOW routing stay intact.
    const float hold=std::max(get("gatehold"),d.minimumHold),release=std::max(get("gaterelease"),d.minimumRelease);
    const float range=std::min(get("gateRangeDb"),d.maximumRange),threshold=get("gatethreshold");
    const bool afterRig=get("gateAfterRig")>.5f;
    set("gatehold",hold);set("gaterelease",release);set("gateRangeDb",range);set("gateRatio",d.gateRatio);set("gateAttack",d.gateAttack);
    if(d.gateMode==1) {
        // Gate + Expander: the Expander takes the authored threshold and ratio
        // up to its range; the main gate closes only where that range is used up.
        const float expanderRange=std::min(range,d.expanderRange);
        const float deep=std::clamp(-expanderRange/(d.gateRatio-1.f)-3.f,-24.f,-9.f);
        set("gateMode",0);set("gatethreshold",std::max(-90.f,threshold+deep));
        set("expOn",1);set("expAfterRig",afterRig?1.f:0.f);set("expThreshold",threshold);set("expRatio",d.gateRatio);
        set("expAttack",std::max(1.f,d.gateAttack*2));set("expHold",hold);set("expRelease",release);set("expRangeDb",expanderRange);
        set("gate2On",0);
    } else {
        // Hard gate plus a gentle Gate 2 on the other side of the rig.
        set("gateMode",0);set("expOn",0);
        set("gate2On",1);set("gate2Mode",1);set("gate2Threshold",std::max(-90.f,threshold+d.gate2Offset));set("gate2Ratio",d.gate2Ratio);
        set("gate2Attack",d.gateAttack);set("gate2Hold",hold);set("gate2Release",release);set("gate2RangeDb",std::min(range,d.gate2Range));
    }
    set("liveLimiterOn",1);set("liveLimiterCeiling",-1);
    set("liveLimiterRelease",r.role==FactoryFinishingRole::fuzz?180.f:100.f);
    return true;
}
} // namespace spectralforge

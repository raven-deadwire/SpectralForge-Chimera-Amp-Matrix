#pragma once
#include "NiflheimrDefinition.h"
#include "OriginalPresets.h"
#include "FactoryCabVoicing.h"

namespace spectralforge {
// Full rigs append after the released Factory/Bass/Guitar/Nastrond banks.
// Preset display names are distinct from native channel names. The historical
// channel-based IDs remain stable serialization keys across display renames.
// These authored settings are genre-informed starting points, not measured
// artist settings or a clone claim. See docs/NIFLHEIMR_GENRE_REFERENCES.md.
inline constexpr int niflheimrPresetStart=originalPresetStart+originalPresetCount;
struct NiflheimrRigPreset {const char* id;const char* name;const char* description;const char* fileName;};
inline constexpr std::array<NiflheimrRigPreset,5> niflheimrRigPresets{{
    {"original.rig.niflheimr.hrimfaxi.v1","Frostline Precision","Modern technical / progressive metal: light Bass Comp, focused PRE EQ, Hrímfaxi clean-low blend and dry Console Four contour.","Niflheimr-01-Frostline-Precision.chimera"},
    {"original.rig.niflheimr.garmr.v1","Carrion Barrage","Death / grind: controlled PRE low mids, Bass Comp, Garmr asymmetric grind with clean-low weight and an articulate dry POST midrange.","Niflheimr-02-Carrion-Barrage.chimera"},
    {"original.rig.niflheimr.nidavellir.v1","Foundry Pulse","Industrial metal: parallel Studio FET compression, PRE mid focus, Nidavellir upper-band bite and dry POST separation for repeating pulses.","Niflheimr-03-Foundry-Pulse.chimera"},
    {"original.rig.niflheimr.ymir.v1","Jötunn Hammer","Slam / brutal death: Bass Comp controls levels, PRE EQ restrains sub build-up, Ymir delivers short-recovery impact over a clean-low foundation.","Niflheimr-04-Jotunn-Hammer.chimera"},
    {"original.rig.niflheimr.hel.v1","Mirebound Monolith","Sludge / doom: gentle Diamond compression, restrained PRE low mids, Hel sustained mid-body fuzz with clean-low blend and dark Passive Tube contour.","Niflheimr-05-Mirebound-Monolith.chimera"}
}};
inline constexpr int niflheimrPresetCount=int(niflheimrRigPresets.size());
inline bool isNiflheimrPreset(int index) {return index>=niflheimrPresetStart && index<niflheimrPresetStart+niflheimrPresetCount;}

inline niflheimr::State niflheimrRigVoice(int preset) {
    // GAIN, BASS, MID, TREBLE, MID FREQ, PRESENCE, DEPTH, MASTER,
    // MASS, SPLIT, FANG, FOLD, THRUST, BLEND. Every active native knob is authored.
    constexpr std::array<std::array<float,niflheimr::controlCount>,5> values{{
        {{.56f,.46f,.54f,.57f,950,.55f,.47f,.50f,.42f,.65f,.55f,.37f,.58f,.66f}},
        {{.67f,.46f,.61f,.50f,1050,.51f,.45f,.50f,.44f,.76f,.67f,.57f,.63f,.72f}},
        {{.59f,.45f,.56f,.57f,1400,.61f,.42f,.50f,.38f,.72f,.68f,.70f,.58f,.70f}},
        {{.69f,.56f,.54f,.44f,650,.42f,.60f,.50f,.56f,.63f,.58f,.60f,.72f,.66f}},
        {{.70f,.53f,.54f,.35f,650,.33f,.55f,.50f,.61f,.48f,.32f,.74f,.36f,.73f}}
    }};
    auto voice=niflheimr::channelState(preset);
    voice.values=values[size_t(voice.channel)];return voice;
}

inline juce::ValueTree niflheimrPresetSnapshot(juce::AudioProcessorValueTreeState& state,int preset) {
    if(preset<0 || preset>=niflheimrPresetCount)return {};
    auto snapshot=state.copyState();
    snapshot.removeChild(snapshot.getChildWithName("GUITAR_SIGNATURE"),nullptr);
    snapshot.removeChild(snapshot.getChildWithName("ORIGINAL_PRESET"),nullptr);
    // A factory full-rig recall must not inherit another sound's bypasses,
    // latent native banks, pitch shift, solo/mute or a guitar cabinet choice.
    for(auto* raw:state.processor.getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
        auto node=snapshot.getChildWithProperty("id",p->paramID);
        if(!node.isValid()){node=juce::ValueTree("PARAM");node.setProperty("id",p->paramID,nullptr);snapshot.appendChild(node,nullptr);}
        node.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);
    }
    const auto set=[&](const juce::String& id,float value){
        auto* p=state.getParameter(id);jassert(p);
        if(p)snapshot.getChildWithProperty("id",id).setProperty("value",p->convertFrom0to1(p->convertTo0to1(value)),nullptr);
    };
    const auto pedal=[&](int owner,int model,std::initializer_list<float> values){
        set(pedalModelID(owner),float(model));set(pedalBypassID(owner,model),0);
        int c=0;for(float value:values)set(pedalControlID(owner,model,c++),value);
    };
    const auto post=[&](int section,int model){set(postNativeModelID(section),float(model));set(postNativeModeID(section),1);set(postNativeBypassID(section,model),0);};
    const auto postKnob=[&](int section,int model,const char* key,float value){
        const auto& spec=postNativeModel(section,model);
        for(int c=0;c<spec.controlCount;++c)if(juce::String(key)==spec.controls[size_t(c)].id){set(postNativeControlID(section,model,c),value);return;}
        jassertfalse;
    };
    set("mode",0);set("input",0);set("output",factoryOutputDb[size_t(niflheimrPresetStart+preset)]);
    set("boardEnabled",1);set("boardLowTap",0);set("doubleron",0);
    set("gateon",1);set("gateAfterRig",1);set("gateRangeDb",preset==4?30.f:54.f);
    set("gatethreshold",preset==4?-76.f:preset==3?-66.f:-69.f);
    set("gatehold",preset==4?65.f:preset==3?12.f:18.f);
    set("gaterelease",preset==4?280.f:preset==3?55.f:preset==2?65.f:85.f);
    const auto voice=niflheimrRigVoice(preset);
    set(ampNativeEnabledID(0),1);set(ampNativeModelID(0),float(niflheimrAmpModel));
    set(ampNativeChannelID(0,niflheimrAmpModel),float(voice.channel));
    // Preserve the authored native amp makeup after distortion. Release 1.3
    // cabinet compensation lives at the visible mic level in FactoryCabVoicing;
    // the complete rig is checked with the low-B/E and +6 dB input fixtures.
    constexpr float ampOutputDb[]{7.5f,-2.f,1.f,1.f,-4.5f};
    set(ampNativeInputTrimID(0),0);set(ampNativeOutputLevelID(0),ampOutputDb[preset]);
    for(size_t c=0;c<niflheimr::controlCount;++c)
        set(ampNativeControlID(0,niflheimrAmpModel,int(c),voice.channel),voice.values[c]);
    // Preserve each channel's bass-safe cuts. The modeled cabinet recipe is
    // applied below after the complete amp / PRE / POST snapshot is authored.
    constexpr float lowCuts[]{30,32,32,27,28},highCuts[]{6700,5900,6100,5100,3900};
    for(int lane=1;lane<=3;++lane){
        const auto suffix=juce::String(lane);set("cabtype"+suffix,0);set("cab"+suffix,1);
        set("cablow"+suffix,lowCuts[preset]);set("cabhigh"+suffix,highCuts[preset]);
        set("ampon"+suffix,lane==1?1.f:0.f);
    }
    // PRE contains compression/EQ only: full-band pre-distortion would dirty
    // the amp's own clean blend and defeat its protected bass foundation.
    switch(preset){
        case 0:
            pedal(0,6,{.30f,.66f,.50f,0,.42f});
            pedal(1,31,{-1.5f,0,0,-1.5f,-.5f,1,1.5f,0,-1,-3,0,0});
            break;
        case 1:
            pedal(0,31,{-1.5f,0,-.5f,-2,-.5f,1.5f,1,0,-1.5f,-4,0,0});
            pedal(1,6,{.25f,.70f,.50f,0,.40f});
            break;
        case 2:
            pedal(0,9,{.32f,.50f,.58f,.26f,.35f,.66f});
            pedal(1,31,{-2,0,-.5f,-1.5f,0,1,1.5f,-.5f,-2,-4,0,0});
            break;
        case 3:
            pedal(0,6,{.18f,.72f,.50f,0,.44f});
            pedal(1,31,{-2.5f,.5f,.5f,-2,-.5f,1,.5f,-1,-3,-5,0,0});
            break;
        case 4:
            pedal(0,8,{.24f,.48f,.50f,1,0});
            pedal(1,31,{-2,0,0,-1.5f,0,0,-1,-2,-4,-6,0,0});
            break;
    }
    if(preset<4){
        post(2,0);
        constexpr float lowMidFreq[]{300,320,280,250},lowMidGain[]{-1.8f,-2.5f,-2.f,-2.2f};
        constexpr float highMidFreq[]{1600,1150,1900,850},highMidGain[]{1.2f,1.8f,1.5f,1.1f};
        postKnob(2,0,"lf_frequency",65);postKnob(2,0,"lf_gain",preset==3?.5f:0.f);
        postKnob(2,0,"lmf_frequency",lowMidFreq[preset]);postKnob(2,0,"lmf_gain",lowMidGain[preset]);postKnob(2,0,"lmf_q",.85f);
        postKnob(2,0,"hmf_frequency",highMidFreq[preset]);postKnob(2,0,"hmf_gain",highMidGain[preset]);postKnob(2,0,"hmf_q",.8f);
        postKnob(2,0,"hf_frequency",4500);postKnob(2,0,"hf_gain",preset==2?-1.f:-1.5f);
    }else{
        // Density comes from Hel's long response and mid-body fuzz, not from
        // stacking broad 250–500 Hz boosts over its wet-only cleanup filter.
        post(2,2);postKnob(2,2,"lf_frequency",2);postKnob(2,2,"lf_boost",0);
        postKnob(2,2,"lf_atten",1.5f);postKnob(2,2,"hf_atten_frequency",0);postKnob(2,2,"hf_atten",2.5f);
    }
    // Rhythm-focused rigs stay dry. Hel's slower amp response supplies sustain
    // without adding a reverb tail that would hide the next low note.
    set("delayon",0);set("reverbon",0);set("choruson",0);
    const auto& rig=niflheimrRigPresets[size_t(preset)];
    juce::ValueTree metadata("ORIGINAL_PRESET");
    metadata.setProperty("id",rig.id,nullptr);metadata.setProperty("name",juce::String::fromUTF8(rig.name),nullptr);
    metadata.setProperty("family","niflheimr",nullptr);metadata.setProperty("formatVersion",2,nullptr);
    metadata.setProperty("acceptance","GENRE_AUTHORED_AUDITION_PENDING",nullptr);
    metadata.setProperty("cabinetPolicy",factoryCabPolicy,nullptr);snapshot.appendChild(metadata,nullptr);
    const auto get=[&](const juce::String& id){return float(snapshot.getChildWithProperty("id",id).getProperty("value"));};
    voiceFactoryCab(niflheimrPresetStart+preset,get,set);
    return snapshot;
}
} // namespace spectralforge

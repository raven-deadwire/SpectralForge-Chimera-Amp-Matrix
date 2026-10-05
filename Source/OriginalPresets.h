#pragma once
#include "OriginalAmpDefinition.h"
#include "FactoryPresets.h"
#include "FactoryPresetLevels.h"
#include "AmpNativeParameters.h"
#include "PedalBoardParameters.h"
#include "PostNativeParameters.h"

namespace spectralforge {
inline constexpr int originalPresetStart=factoryPresetCount+4;
struct OriginalRigPreset {const char* id;const char* name;const char* role;int mode;};
// Replace the five preview examples in place. Channel names belong in CHANNEL;
// Owner references informed the hot drive, dry crossover rigs and POST contour.
// Embedded user IRs are not copied; every factory rig uses the bundled V30.
// These are complete PRE / amp / cabinet / POST rigs with separate identities.
inline constexpr std::array<OriginalRigPreset,5> originalRigPresets{{
    {"original.rig.nastrond.thall.v3","Thall Rhythm","Fenrir: hot TS808 / Diamond, Console VCA and sculpted Console Four EQ; dry reference-inspired rhythm.",0},
    {"original.rig.nastrond.lead.v3","Molten Lead","Surtr: Studio FET / Gold Drive, Inductor EQ, 229 delay and plate for sustained lead.",0},
    {"original.rig.nastrond.grind.v3","Rotten Grind","Níðhöggr / Ragnarök crossover Dual: Obsession / Diamond, Iron Colour and a 2 kHz POST bite.",1},
    {"original.rig.nastrond.sludge.v3","Sludge Mass","Fimbulvetr: Vintage Fuzz / PRE Graphic EQ, Passive Tube EQ and a small dark hall.",0},
    {"original.rig.nastrond.slam.v3","Slam Impact","Fenrir LOW / Fimbulvetr MID / Ragnarök HIGH: reference-inspired Matrix, hot TS808 / Diamond and full POST chain.",2}
}};
inline constexpr int originalPresetCount=int(originalRigPresets.size());
inline bool isOriginalPreset(int index) {return index>=originalPresetStart && index<originalPresetStart+originalPresetCount;}
inline original::State originalRigVoice(int preset,int lane) {
    using C=original::Control;
    int channel=preset;
    if(preset==2 && lane==1)channel=4;
    if(preset==4)channel=lane==0?0:lane==1?3:4;
    auto s=original::channelState(channel);
    switch(preset) {
        case 0:s[C::gain]=.68f;s[C::bass]=.42f;s[C::middle]=.56f;s[C::treble]=.53f;s[C::clank]=.62f;s[C::bloom]=.75f;break;
        case 1:s[C::gain]=.65f;s[C::middle]=.64f;s[C::midFrequency]=950;s[C::presence]=.42f;s[C::crush]=.60f;s[C::rot]=.75f;s[C::bloom]=.43f;break;
        case 2:s[C::gain]=lane==0?.72f:.68f;s[C::bass]=.43f;s[C::middle]=lane==0?.47f:.56f;s[C::rot]=lane==0?.68f:.28f;s[C::bloom]=.40f;break;
        case 3:s[C::gain]=.68f;s[C::bass]=.54f;s[C::middle]=.58f;s[C::treble]=.44f;s[C::presence]=.38f;s[C::clank]=.28f;s[C::bloom]=.65f;break;
        case 4:s[C::gain]=lane==1?.68f:.72f;s[C::bass]=lane==0?.58f:.42f;s[C::middle]=.52f;s[C::depth]=lane==0?.65f:.44f;s[C::impact]=lane==2?.75f:.65f;s[C::crush]=.72f;s[C::clank]=lane==1?.50f:.78f;s[C::rot]=lane==1?.70f:.60f;s[C::bloom]=lane==1?.70f:.45f;break;
        default:break;
    }
    return s;
}
inline juce::ValueTree originalPresetSnapshot(juce::AudioProcessorValueTreeState& state,int preset) {
    if(preset<0 || preset>=originalPresetCount)return {};
    auto snapshot=state.copyState();
    snapshot.removeChild(snapshot.getChildWithName("GUITAR_SIGNATURE"),nullptr);
    snapshot.removeChild(snapshot.getChildWithName("ORIGINAL_PRESET"),nullptr);
    for(auto* raw:state.processor.getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
        auto node=snapshot.getChildWithProperty("id",p->paramID);
        if(!node.isValid()){node=juce::ValueTree("PARAM");node.setProperty("id",p->paramID,nullptr);snapshot.appendChild(node,nullptr);}
        node.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);
    }
    const auto set=[&](const juce::String& id,float value){auto* p=state.getParameter(id);jassert(p);if(p)snapshot.getChildWithProperty("id",id).setProperty("value",p->convertFrom0to1(p->convertTo0to1(value)),nullptr);};
    const auto& rig=originalRigPresets[size_t(preset)];
    set("mode",float(rig.mode));set("boardEnabled",1);set("boardLowTap",preset==4?1.f:0.f);
    set("input",0);set("output",factoryOutputDb[size_t(originalPresetStart+preset)]);
    set("gateAfterRig",1);set("gatethreshold",preset==1?-72.f:preset==3?-74.f:-66.f);
    set("gatehold",preset==3?45.f:20.f);set("gaterelease",preset==1?170.f:preset==3?240.f:65.f);
    set("dualtype",preset==2?1.f:0.f);set("dualcross",350);set("dualblend",.5f);set("doubleron",0);
    const int lanes=rig.mode+1;
    for(int lane=0;lane<lanes;++lane) {
        const int context=ampNativeContext(rig.mode,lane);const auto voice=originalRigVoice(preset,lane);
        set(ampNativeModelID(context),float(firstOriginalAmpModel));set(ampNativeEnabledID(context),1);
        set(ampNativeChannelID(context,firstOriginalAmpModel),float(voice.channel));
        set(originalResponseID(context,voice.channel),1);
        for(size_t c=0;c<original::controlCount;++c)set(ampNativeControlID(context,firstOriginalAmpModel,int(c),voice.channel),voice.values[c]);
        const auto suffix=juce::String(lane+1);set("ampon"+suffix,1);set("cabtype"+suffix,1);set("cab"+suffix,1);
        set("cablow"+suffix,preset==3?48.f:preset==4?45.f:75.f);set("cabhigh"+suffix,preset==3?5900.f:preset==1?7400.f:7000.f);
    }
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
    switch(preset) {
        case 0:
            pedal(0,1,{.35f,.90f,.85f});
            pedal(1,8,{.40f,.50f,.50f,1,1});
            post(0,0);postKnob(0,0,"threshold",-18);postKnob(0,0,"ratio",1);
            postKnob(0,0,"attack",4);postKnob(0,0,"release",1);postKnob(0,0,"makeup",0);
            post(2,0);postKnob(2,0,"lf_frequency",80);postKnob(2,0,"lf_gain",-6);
            postKnob(2,0,"lmf_frequency",600);postKnob(2,0,"lmf_gain",-3.36f);
            postKnob(2,0,"hmf_frequency",2000);postKnob(2,0,"hmf_gain",4.5f);
            break;
        case 1:
            pedal(0,9,{.34f,.52f,.45f,.22f,.18f,.48f});
            pedal(1,2,{.27f,.48f,.56f});
            post(2,1);postKnob(2,1,"mf_frequency",2);postKnob(2,1,"mf_gain",1.5f);
            postKnob(2,1,"lf_gain",-1);postKnob(2,1,"high_pass",1);
            set("delayon",1);set("delaymodel",0);set("delaytime",320);set("delaysync",0);set("delaymix",.16f);set("delayfeedback",.24f);
            set("reverbon",1);set("reverbmodel",0);set("reverbmix",.10f);set("reverbsize",.34f);
            break;
        case 2:
            pedal(0,27,{.72f,.28f,.65f,0});
            pedal(1,8,{.50f,.50f,.50f,1,1});
            post(1,0);postKnob(1,0,"gain",2);
            post(2,0);set(postNativeLevelID(2,0),-1.5f);postKnob(2,0,"lf_frequency",80);postKnob(2,0,"lf_gain",-6);
            postKnob(2,0,"lmf_frequency",600);postKnob(2,0,"lmf_gain",-3.36f);
            postKnob(2,0,"hmf_frequency",2000);postKnob(2,0,"hmf_gain",4.5f);
            break;
        case 3:
            pedal(0,18,{.38f,.26f});
            pedal(1,31,{-7,-1,0,0,1.5f,0,-1.5f,-2,-3,-6,0,0});
            post(2,2);postKnob(2,2,"lf_frequency",3);postKnob(2,2,"lf_boost",1.5f);
            postKnob(2,2,"lf_atten",1);postKnob(2,2,"hf_atten",2);
            set("reverbon",1);set("reverbmodel",1);set("reverbmix",.045f);set("reverbsize",.48f);
            break;
        case 4:
            pedal(0,1,{.50f,1,1});
            pedal(1,8,{.50f,.50f,.50f,1,1});
            set("x1",350);set("x2",1200);set("lowcomp",.5f);set("lowampmix",.75f);
            set("level1",0);set("level2",0);set("level3",-1.5f);
            post(0,0);postKnob(0,0,"threshold",-18);postKnob(0,0,"ratio",1);
            postKnob(0,0,"attack",4);postKnob(0,0,"release",1);postKnob(0,0,"makeup",0);
            post(1,0);postKnob(1,0,"gain",2);
            post(2,0);postKnob(2,0,"lf_frequency",80);postKnob(2,0,"lf_gain",-8.89f);
            postKnob(2,0,"lmf_frequency",600);postKnob(2,0,"lmf_gain",-3.36f);
            postKnob(2,0,"hmf_frequency",2000);postKnob(2,0,"hmf_gain",6.02f);
            break;
    }
    juce::ValueTree metadata("ORIGINAL_PRESET");metadata.setProperty("id",rig.id,nullptr);metadata.setProperty("name",juce::String::fromUTF8(rig.name),nullptr);
    metadata.setProperty("formatVersion",3,nullptr);snapshot.appendChild(metadata,nullptr);
    return snapshot;
}
}

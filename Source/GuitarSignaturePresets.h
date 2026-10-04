#pragma once
#include "FactoryPresets.h"
#include "AmpNativeParameters.h"
#include "PedalBoardParameters.h"
#include "PostNativeParameters.h"

namespace spectralforge {
struct GuitarSignature { const char* id; const char* name; const char* description; };
// A separate append-only bank preserves Factory31/Bass3 and every menu ID.
inline constexpr std::array<GuitarSignature,4> guitarSignatures{{
    {"raven.alsatia.v1","A Path To Alsatia","Power-metal solo: Hot Lead / Liquid Lead Dual, Studio FET and Gold Drive, FET / Inductor EQ, 229 repeats and plate."},
    {"raven.wrath.v1","Feel My Wrath","Groove-metal rhythm: Fourfold / Tight 515 Dual, Yellow Asym, dry POST; VCA available in bypass for optional glue."},
    {"raven.blackhearted.v1","Blackhearted","Death-metal rhythm: clean LOW, Fourfold MID and Night Harvest HIGH; Yellow Asym after the LOW tap, focused EQ and dry POST."},
    {"raven.throne.v1","Dark Matters of Throne","Death-metal rhythm: clean LOW, Blue Storm MID and Night Harvest HIGH; Obsession after the LOW tap, Iron Colour and Passive Tube EQ."}
}};
inline constexpr int selectablePresetCount=factoryPresetCount+int(guitarSignatures.size());
inline bool isGuitarSignature(int index) { return index>=factoryPresetCount && index<selectablePresetCount; }
inline const char* selectablePresetDescription(int index) {
    if(isGuitarSignature(index))return guitarSignatures[size_t(index-factoryPresetCount)].description;
    return index>=0 && index<factoryPresetCount ? factoryPresets[size_t(index)].description : "";
}

// Complete APVTS sound snapshot, including inactive model banks. No previous
// native controls, pedal bypasses, POST values or performance settings leak in.
// External files/MIDI/A-B are session resources; IR metadata selects built-in V30.
inline juce::ValueTree guitarSignatureSnapshot(juce::AudioProcessorValueTreeState& state,int song) {
    if(song<0 || song>=int(guitarSignatures.size()))return {};
    auto snapshot=state.copyState();
    for(auto* raw:state.processor.getParameters()) {
        auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw);
        if(!p)continue;
        auto node=snapshot.getChildWithProperty("id",p->paramID);
        if(!node.isValid()) {node=juce::ValueTree("PARAM");node.setProperty("id",p->paramID,nullptr);snapshot.appendChild(node,nullptr);}
        node.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);
    }
    auto set=[&](const juce::String& id,float value) {
        auto* p=state.getParameter(id);
        jassert(p!=nullptr);
        if(p) snapshot.getChildWithProperty("id",id).setProperty("value",p->convertFrom0to1(p->convertTo0to1(value)),nullptr);
    };
    const bool matrix=song>=2;
    set("mode",matrix?2.f:1.f);set("input",0);set("output",-6);set("gateAfterRig",1);
    set("gatethreshold",song==0?-72.f:-65.f);set("gaterelease",song==0?140.f:70.f);set("gatehold",20);
    set("boardEnabled",1);set("boardLowTap",0);set("dualtype",0);set("dualblend",song==0?.38f:.30f);
    set("x1",song==2?140.f:160.f);set("x2",song==2?1800.f:1500.f);
    set("lowcomp",0);set("lowampmix",0);
    for(int lane=1;lane<=3;++lane) {const auto suffix=juce::String(lane);set("cabtype"+suffix,1);set("cablow"+suffix,matrix?55.f:85.f);set("cabhigh"+suffix,song==0?7800.f:7000.f);}
    if(matrix) {set("cab1",0);set("ampon1",0);}
    auto amp=[&](int context,int model,int channel,float output) {
        set(ampNativeEnabledID(context),1);set(ampNativeModelID(context),float(model));
        set(ampNativeChannelID(context,model),float(channel));set(ampNativeOutputLevelID(context),output);
    };
    auto knob=[&](int context,int model,const char* key,float value) {
        const auto& controls=ampNativePanel(model).controls;
        for(size_t c=0;c<controls.size();++c)if(juce::String(key)==controls[c].key){set(ampNativeControlID(context,model,int(c)),value);return;}
        jassertfalse;
    };
    auto pedal=[&](int owner,int model) {set(pedalModelID(owner),float(model));set(pedalBypassID(owner,model),0);};
    auto control=[&](int owner,int model,int c,float value) {set(pedalControlID(owner,model,c),value);};
    auto post=[&](int section,int model,bool bypass=false) {set(postNativeModelID(section),float(model));set(postNativeModeID(section),1);set(postNativeBypassID(section,model),bypass?1.f:0.f);};
    auto postControl=[&](int section,int model,const char* key,float value) {
        const auto& spec=postNativeModel(section,model);
        for(int c=0;c<spec.controlCount;++c)if(juce::String(key)==spec.controls[size_t(c)].id){set(postNativeControlID(section,model,c),value);return;}
        jassertfalse;
    };
    if(song==0) {
        amp(1,21,1,0);amp(2,4,2,0);knob(1,21,"hw.overdrive.preamp",.58f);
        pedal(0,9);pedal(1,2);control(1,2,0,.16f);control(1,2,1,.52f);control(1,2,2,.57f);
        post(0,1);postControl(0,1,"input",-8);postControl(0,1,"attack",.2f);postControl(0,1,"release",.55f);set(postNativeLevelID(0,1),11.f);
        post(2,1);postControl(2,1,"mf_frequency",2);postControl(2,1,"mf_gain",1.5f);postControl(2,1,"high_pass",2);
        set("delayon",1);set("delaymodel",0);set("delaysync",0);set("delaytime",320);set("delaymix",.16f);set("delayfeedback",.22f);
        set("reverbon",1);set("reverbmodel",0);set("reverbmix",.10f);set("reverbsize",.35f);
    } else if(song==1) {
        amp(1,17,2,0);amp(2,2,0,0);knob(1,17,"hw.ch3.gain",.48f);
        pedal(0,26);control(0,26,0,.10f);control(0,26,1,.55f);control(0,26,2,.58f);
        post(0,0,true);postControl(0,0,"threshold",-6);postControl(0,0,"ratio",0);postControl(0,0,"attack",5);
    } else {
        amp(3,0,0,0);amp(4,song==2?17:22,song==2?2:1,-2);amp(5,20,0,-4);
        knob(5,20,"hw.ep.gain",.43f);knob(5,20,"hw.gain_eq.bass",.36f);knob(5,20,"hw.presence",.44f);
        if(song==2) {knob(4,17,"hw.ch3.gain",.50f);pedal(0,26);control(0,26,0,.08f);control(0,26,1,.55f);control(0,26,2,.56f);
            post(2,0);postControl(2,0,"lmf_frequency",350);postControl(2,0,"lmf_gain",-2);postControl(2,0,"hmf_frequency",2200);postControl(2,0,"hmf_gain",1);set(postNativeLevelID(2,0),-3.5f);
        } else {knob(4,22,"hw.lead.gain",.48f);pedal(0,27);control(0,27,0,.52f);control(0,27,1,.18f);control(0,27,2,.48f);
            post(1,0);postControl(1,0,"gain",3);set(postNativeLevelID(1,0),-5);
            post(2,2);postControl(2,2,"lf_frequency",3);postControl(2,2,"lf_boost",1);postControl(2,2,"lf_atten",1.5f);postControl(2,2,"hf_atten",2);set(postNativeLevelID(2,2),-6.5f);
        }
    }
    auto metadata=juce::ValueTree("GUITAR_SIGNATURE");
    metadata.setProperty("id",guitarSignatures[size_t(song)].id,nullptr);metadata.setProperty("name",guitarSignatures[size_t(song)].name,nullptr);
    metadata.setProperty("formatVersion",1,nullptr);metadata.setProperty("acceptance","CANDIDATE_SYNTHETIC_ONLY",nullptr);
    metadata.setProperty("ir","embedded:guitar_v30_sm57.wav",nullptr);
    metadata.setProperty("irSha256","ef8d258eee57b2f0fa1e678b4bdf9c8f535f03039172b0831dcc87b0e7d99bd2",nullptr);
    snapshot.removeChild(snapshot.getChildWithName("GUITAR_SIGNATURE"),nullptr);snapshot.appendChild(metadata,nullptr);
    return snapshot;
}
}

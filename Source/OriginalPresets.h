#pragma once
#include "OriginalAmpDefinition.h"
#include "FactoryPresets.h"
#include "FactoryPresetLevels.h"
#include "AmpNativeParameters.h"
#include "PedalBoardParameters.h"

namespace spectralforge {
inline constexpr int originalPresetStart=factoryPresetCount+4;
inline constexpr int originalPresetCount=int(original::presets.size());
inline bool isOriginalPreset(int index) {return index>=originalPresetStart && index<originalPresetStart+originalPresetCount;}
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
    set("mode",preset==2?1.f:0.f);
    for(int context=0;context<3;++context) {
        set(ampNativeModelID(context),float(firstOriginalAmpModel));set(ampNativeEnabledID(context),1);
        set(ampNativeChannelID(context,firstOriginalAmpModel),float(preset));
    }
    // Full-rig examples are optional entry points into the real channel bank.
    // PRE matches the owner's operating chain; channels also work without it.
    set(pedalModelID(0),22);set(pedalBypassID(0,22),0);set(pedalControlID(0,22,0),.6f);
    set(pedalModelID(1),1);set(pedalBypassID(1,1),0);
    for(int knob=0;knob<3;++knob)set(pedalControlID(1,1,knob),.5f);
    set("boardEnabled",1);set("input",0);set("output",factoryOutputDb[size_t(originalPresetStart+preset)]);
    set("gateAfterRig",1);set("gatethreshold",preset==1?-72.f:-68.f);set("gatehold",25);set("gaterelease",preset==1?140.f:90.f);
    set("cabtype1",1);set("cab1",1);set("cablow1",preset==3?55.f:75.f);set("cabhigh1",preset==1?7800.f:7200.f);
    const auto& voice=original::presets[size_t(preset)];
    for(int context=0;context<3;++context)for(size_t c=0;c<original::controlCount;++c)
        set(ampNativeControlID(context,firstOriginalAmpModel,int(c),preset),original::channelState(preset).values[c]);
    if(preset==2) {
        set("dualblend",.5f);set("dualtype",0);set("cabtype2",1);set("cab2",1);set("cablow2",75);set("cabhigh2",7200);
        set(ampNativeControlID(2,firstOriginalAmpModel,int(original::Control::midFrequency),preset),1800);
    }
    if(preset==1){set("delayon",1);set("delaytime",310);set("delaymix",.12f);set("delayfeedback",.18f);set("reverbon",1);set("reverbmix",.08f);set("reverbsize",.3f);}
    juce::ValueTree metadata("ORIGINAL_PRESET");metadata.setProperty("id",voice.id,nullptr);metadata.setProperty("name",juce::String::fromUTF8(voice.name),nullptr);
    metadata.setProperty("formatVersion",2,nullptr);snapshot.appendChild(metadata,nullptr);
    return snapshot;
}
}

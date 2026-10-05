#pragma once
#include "OriginalAmpDefinition.h"
#include "FactoryPresets.h"
#include "FactoryPresetLevels.h"
#include "AmpNativeParameters.h"

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
    set("mode",0);set(ampNativeModelID(0),float(firstOriginalAmpModel));set(ampNativeEnabledID(0),1);
    set("boardEnabled",1);set("input",0);set("output",factoryOutputDb[size_t(originalPresetStart+preset)]);
    set("gateAfterRig",1);set("gatethreshold",preset==1?-72.f:-68.f);set("gatehold",25);set("gaterelease",preset==1?140.f:90.f);
    set("cabtype1",1);set("cab1",1);set("cablow1",preset==3?55.f:75.f);set("cabhigh1",preset==1?7800.f:7200.f);
    const auto& voice=original::presets[size_t(preset)];
    for(size_t c=0;c<original::controlCount;++c)set(ampNativeControlID(0,firstOriginalAmpModel,int(c)),voice.state.values[c]);
    if(preset==1){set("delayon",1);set("delaytime",310);set("delaymix",.12f);set("delayfeedback",.18f);set("reverbon",1);set("reverbmix",.08f);set("reverbsize",.3f);}
    juce::ValueTree metadata("ORIGINAL_PRESET");metadata.setProperty("id",voice.id,nullptr);metadata.setProperty("name",juce::String::fromUTF8(voice.name),nullptr);
    metadata.setProperty("formatVersion",1,nullptr);snapshot.appendChild(metadata,nullptr);
    return snapshot;
}
}

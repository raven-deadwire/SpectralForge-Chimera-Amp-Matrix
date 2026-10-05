#pragma once
#include "AmpNativeCatalog.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectralforge {
// These IDs are appended after every released parameter. None reuses amp1/2/3
// or the first corrective release's extension/channel parameter indices.
inline juce::String ampNativePrefix(int context) { return "nativeAmp_c"+juce::String(context); }
inline juce::String ampNativeEnabledID(int context) { return ampNativePrefix(context)+"_enabled"; }
inline juce::String ampNativeSoloID(int context) { return ampNativePrefix(context)+"_solo"; }
inline juce::String ampNativeModelID(int context) { return ampNativePrefix(context)+"_model"; }
inline juce::String ampNativeChannelID(int context,int model) { return ampNativePrefix(context)+"_m"+juce::String(model)+"_channel"; }
inline juce::String ampNativeRouteID(int context,int model) { return ampNativePrefix(context)+"_m"+juce::String(model)+"_route"; }
inline juce::String ampNativeControlID(int context,int model,int control) { if(model==firstOriginalAmpModel)return "originalAmp_c"+juce::String(context)+"_nastrond_"+original::controls[(size_t)control].id;return ampNativePrefix(context)+"_m"+juce::String(model)+"_"+juce::String(ampNativePanel(model).controls[(size_t)control].key).replaceCharacter('.','_'); }
inline juce::String ampNativeInputTrimID(int context) { return ampNativePrefix(context)+"_inputTrim"; }
inline juce::String ampNativeOutputLevelID(int context) { return ampNativePrefix(context)+"_outputLevel"; }
inline constexpr const char* ampNativeContextNames[]{"Classic","Dual A","Dual B","Matrix Low","Matrix Mid","Matrix High"};
inline void addAmpNativeParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto structural=juce::AudioParameterIntAttributes().withAutomatable(false);
    for(int context=0;context<ampNativeContextCount;++context) {
        const auto prefix=juce::String(ampNativeContextNames[context])+" native ";
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ampNativeEnabledID(context),1},prefix+"engine",true,juce::AudioParameterBoolAttributes().withAutomatable(false)));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ampNativeSoloID(context),1},prefix+"software solo footswitch",false));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ampNativeModelID(context),1},prefix+"model",0,255,2,structural));
        for(auto pair:{std::pair{ampNativeInputTrimID(context),"software input trim"},std::pair{ampNativeOutputLevelID(context),"software output level"}})
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{pair.first,1},prefix+pair.second,juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
        // Freeze the released six-context ordering. New models must be added
        // AFTER the complete released layout (including POST), not here.
        for(int model=0;model<releasedNativeAmpModelCount;++model) {
            const auto& panel=ampNativePanel(model);const auto name=prefix+juce::String::fromUTF8(ampInfo(model).name)+" ";
            // Fixed raw reserved range does not renormalize host data when a
            // panel revision gains another channel or an input route.
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ampNativeChannelID(context,model),1},name+"channel",0,15,panel.defaultChannel,structural));
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ampNativeRouteID(context,model),1},name+"input route",0,15,0,structural));
            for(size_t c=0;c<panel.controls.size();++c) {
                const auto& k=panel.controls[c];
                layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ampNativeControlID(context,model,(int)c),1},name+k.label,
                    juce::NormalisableRange<float>(k.minimum,k.maximum,k.kind==AmpNativeControlKind::knob?.001f:1.f),k.initial));
            }
        }
    }
}
inline void appendNewAmpNativeParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int first=releasedNativeAmpModelCount, int last=firstOriginalAmpModel) {
    const auto structural=juce::AudioParameterIntAttributes().withAutomatable(false);
    for(int model=first;model<last;++model)
        for(int context=0;context<ampNativeContextCount;++context) {
            const auto& panel=ampNativePanel(model);
            const auto name=juce::String(ampNativeContextNames[context])+" native "+juce::String::fromUTF8(ampInfo(model).name)+" ";
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ampNativeChannelID(context,model),model>=firstOriginalAmpModel?3:1},name+"channel",0,15,panel.defaultChannel,structural));
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ampNativeRouteID(context,model),model>=firstOriginalAmpModel?3:1},name+"input route",0,15,0,structural));
            for(size_t c=0;c<panel.controls.size();++c) {
                const auto& k=panel.controls[c];
                auto range=juce::NormalisableRange<float>(k.minimum,k.maximum,k.kind==AmpNativeControlKind::knob?.001f:1.f);
                if(model==firstOriginalAmpModel && c==size_t(original::Control::midFrequency))range.setSkewForCentre(850);
                layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ampNativeControlID(context,model,int(c)),model>=firstOriginalAmpModel?3:1},name+k.label,range,k.initial));
            }
        }
}
struct AmpNativeParameterCache {
    std::array<std::atomic<float>*,ampNativeContextCount> enabled{},models{},inputTrim{},outputLevel{},solo{};
    using ModelBank=std::array<std::atomic<float>*,ampModelCount>;
    std::array<ModelBank,ampNativeContextCount> channels{},routes{};
    std::array<std::array<std::array<std::atomic<float>*,maxAmpNativeControls>,ampModelCount>,ampNativeContextCount> controls{};
    void bind(juce::AudioProcessorValueTreeState& state) {
        for(int context=0;context<ampNativeContextCount;++context) {
            const auto c=(size_t)context; enabled[c]=state.getRawParameterValue(ampNativeEnabledID(context));models[c]=state.getRawParameterValue(ampNativeModelID(context));
            solo[c]=state.getRawParameterValue(ampNativeSoloID(context));
            inputTrim[c]=state.getRawParameterValue(ampNativeInputTrimID(context));outputLevel[c]=state.getRawParameterValue(ampNativeOutputLevelID(context));
            for(int model=0;model<ampModelCount;++model) {
                const auto m=(size_t)model; channels[c][m]=state.getRawParameterValue(ampNativeChannelID(context,model));routes[c][m]=state.getRawParameterValue(ampNativeRouteID(context,model));
                for(size_t k=0;k<ampNativePanel(model).controls.size();++k)controls[c][m][k]=state.getRawParameterValue(ampNativeControlID(context,model,(int)k));
            }
        }
    }
    static float value(const std::atomic<float>* p,float fallback) noexcept { if(!p)return fallback;const float v=p->load(std::memory_order_relaxed);return std::isfinite(v)?v:fallback; }
    AmpNativeState read(int context) const noexcept {
        context=juce::jlimit(0,ampNativeContextCount-1,context);const auto c=(size_t)context;
        const int raw=(int)std::round(juce::jlimit(0.f,255.f,value(models[c],2.f)));
        auto s=defaultAmpNativeState(raw<ampModelCount?raw:2);const auto m=(size_t)s.model;
        s.soloEnabled=value(solo[c],0.f)>.5f;s.enabled=ampRequiresNative(s.model)||value(enabled[c],0.f)>.5f;s.channel=(int)std::round(juce::jlimit(0.f,15.f,value(channels[c][m],(float)s.channel)));
        s.inputRoute=(int)std::round(juce::jlimit(0.f,15.f,value(routes[c][m],0)));
        s.inputTrimDb=juce::jlimit(-24.f,24.f,value(inputTrim[c],0));s.outputLevelDb=juce::jlimit(-24.f,24.f,value(outputLevel[c],0));
        for(size_t k=0;k<ampNativePanel(s.model).controls.size();++k)s.values[k]=value(controls[c][m][k],s.values[k]);
        sanitiseAmpNativeState(s);return s;
    }
};
} // namespace spectralforge

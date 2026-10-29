#pragma once
#include "PostNativeCatalog.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace spectralforge {
// Append after every previously shipped parameter; never repurpose old IDs.
inline void addPostNativeParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    using ID=juce::ParameterID;
    for(int s=0;s<3;++s) {
        layout.add(std::make_unique<juce::AudioParameterBool>(ID{postNativeModeID(s),1},"POST native section "+juce::String(s+1),true,juce::AudioParameterBoolAttributes().withAutomatable(false)));
        layout.add(std::make_unique<juce::AudioParameterInt>(ID{postNativeModelID(s),1},"POST section "+juce::String(s+1)+" model",0,2,0,juce::AudioParameterIntAttributes().withAutomatable(false)));
        for(int m=0;m<postNativeReleasedModelsPerSection;++m) {
            const auto& model=postNativeModel(s,m);const auto prefix="POST "+juce::String::fromUTF8(model.name)+" ";
            layout.add(std::make_unique<juce::AudioParameterBool>(ID{postNativeBypassID(s,m),1},prefix+"bypass",true));
            layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeTrimID(s,m),1},prefix+"software trim",juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeLevelID(s,m),1},prefix+"software level",juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
            for(int c=0;c<model.controlCount;++c) {
                const auto& p=model.controls[c];
                // Released automation flags are fixed. EXTERNAL SIDE-CHAIN was
                // released unconnected (not automatable) and keeps that flag now
                // that it works (docs/SIDECHAIN_SYNTH_BUS.md).
                const bool automatable=p.connected && juce::String(p.id)!="external_sidechain";
                layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeControlID(s,m,c),1},prefix+juce::String::fromUTF8(p.label),
                    juce::NormalisableRange<float>(p.minimum,p.maximum,p.interval),p.initial,
                    juce::AudioParameterFloatAttributes().withAutomatable(automatable)));
            }
        }
    }
}
// 1.7 POST expansion (docs/POST_EXPANSION_1_7.md): models 3-5 of each section
// and an extension selector, appended after every released parameter.
inline constexpr int postExpansionParameterVersion=22;
inline int postExpansionParameterCount() noexcept {
    int count=0;
    for(int s=0;s<postNativeSectionCount;++s){++count;for(int m=postNativeReleasedModelsPerSection;m<postNativeModelsPerSection;++m)count+=3+postNativeModel(s,m).controlCount;}
    return count;
}
inline void appendPostExpansionParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    using ID=juce::ParameterID;constexpr int v=postExpansionParameterVersion;
    for(int s=0;s<postNativeSectionCount;++s) {
        // A fixed raw range: later models never renormalise saved selections.
        layout.add(std::make_unique<juce::AudioParameterInt>(ID{postNativeModelExtensionID(s),v},"POST section "+juce::String(s+1)+" model extension",0,15,0,juce::AudioParameterIntAttributes().withAutomatable(false)));
        for(int m=postNativeReleasedModelsPerSection;m<postNativeModelsPerSection;++m) {
            const auto& model=postNativeModel(s,m);const auto prefix="POST "+juce::String::fromUTF8(model.name)+" ";
            layout.add(std::make_unique<juce::AudioParameterBool>(ID{postNativeBypassID(s,m),v},prefix+"bypass",true));
            layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeTrimID(s,m),v},prefix+"software trim",juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeLevelID(s,m),v},prefix+"software level",juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
            for(int c=0;c<model.controlCount;++c) {
                const auto& p=model.controls[c];
                layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeControlID(s,m,c),v},prefix+juce::String::fromUTF8(p.label),
                    juce::NormalisableRange<float>(p.minimum,p.maximum,p.interval),p.initial,
                    juce::AudioParameterFloatAttributes().withAutomatable(p.connected)));
            }
        }
    }
}
struct PostNativeParameterCache {
    struct Bank { std::atomic<float>* bypass{};std::atomic<float>* trim{};std::atomic<float>* level{};std::array<std::atomic<float>*,postNativeMaxControls> values{}; };
    std::array<std::atomic<float>*,3> modes{},models{},extensions{};std::array<std::array<Bank,postNativeModelsPerSection>,3> banks{};
    void bind(juce::AudioProcessorValueTreeState& apvts) {
        for(int s=0;s<3;++s) {
            modes[s]=apvts.getRawParameterValue(postNativeModeID(s));models[s]=apvts.getRawParameterValue(postNativeModelID(s));extensions[s]=apvts.getRawParameterValue(postNativeModelExtensionID(s));
            for(int m=0;m<postNativeModelsPerSection;++m) {auto& b=banks[s][m];b.bypass=apvts.getRawParameterValue(postNativeBypassID(s,m));b.trim=apvts.getRawParameterValue(postNativeTrimID(s,m));b.level=apvts.getRawParameterValue(postNativeLevelID(s,m));for(int c=0;c<postNativeModel(s,m).controlCount;++c)b.values[c]=apvts.getRawParameterValue(postNativeControlID(s,m,c));}
        }
    }
    static float value(const std::atomic<float>* p,float fallback) noexcept {if(!p)return fallback;const float x=p->load();return std::isfinite(x)?x:fallback;}
    PostNativeState read() const noexcept {
        auto state=defaultPostNativeState(false);
        for(int s=0;s<3;++s) {auto& section=state.sections[s];section.nativeEnabled=value(modes[s],0)>.5f;section.selected=postNativeSelectedModel(value(models[s],0),value(extensions[s],0));for(int m=0;m<postNativeModelsPerSection;++m){auto& b=section.banks[m];const auto& cache=banks[s][m];b.bypass=value(cache.bypass,1)>.5f;b.trimDb=value(cache.trim,0);b.levelDb=value(cache.level,0);for(int c=0;c<postNativeModel(s,m).controlCount;++c)b.values[c]=value(cache.values[c],postNativeModel(s,m).controls[c].initial);}}
        sanitisePostNativeState(state);return state;
    }
};
} // namespace spectralforge

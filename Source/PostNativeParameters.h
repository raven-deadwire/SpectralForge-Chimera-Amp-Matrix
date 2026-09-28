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
        for(int m=0;m<3;++m) {
            const auto& model=postNativeModel(s,m);const auto prefix="POST "+juce::String(model.name)+" ";
            layout.add(std::make_unique<juce::AudioParameterBool>(ID{postNativeBypassID(s,m),1},prefix+"bypass",true));
            layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeTrimID(s,m),1},prefix+"software trim",juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeLevelID(s,m),1},prefix+"software level",juce::NormalisableRange<float>(-24.f,24.f,.01f),0.f));
            for(int c=0;c<model.controlCount;++c) {
                const auto& p=model.controls[c];
                layout.add(std::make_unique<juce::AudioParameterFloat>(ID{postNativeControlID(s,m,c),1},prefix+p.label,
                    juce::NormalisableRange<float>(p.minimum,p.maximum,p.interval),p.initial,
                    juce::AudioParameterFloatAttributes().withAutomatable(p.connected)));
            }
        }
    }
}
struct PostNativeParameterCache {
    struct Bank { std::atomic<float>* bypass{};std::atomic<float>* trim{};std::atomic<float>* level{};std::array<std::atomic<float>*,postNativeMaxControls> values{}; };
    std::array<std::atomic<float>*,3> modes{},models{};std::array<std::array<Bank,3>,3> banks{};
    void bind(juce::AudioProcessorValueTreeState& apvts) {
        for(int s=0;s<3;++s) {
            modes[s]=apvts.getRawParameterValue(postNativeModeID(s));models[s]=apvts.getRawParameterValue(postNativeModelID(s));
            for(int m=0;m<3;++m) {auto& b=banks[s][m];b.bypass=apvts.getRawParameterValue(postNativeBypassID(s,m));b.trim=apvts.getRawParameterValue(postNativeTrimID(s,m));b.level=apvts.getRawParameterValue(postNativeLevelID(s,m));for(int c=0;c<postNativeModel(s,m).controlCount;++c)b.values[c]=apvts.getRawParameterValue(postNativeControlID(s,m,c));}
        }
    }
    static float value(const std::atomic<float>* p,float fallback) noexcept {if(!p)return fallback;const float x=p->load();return std::isfinite(x)?x:fallback;}
    PostNativeState read() const noexcept {
        auto state=defaultPostNativeState(false);
        for(int s=0;s<3;++s) {auto& section=state.sections[s];section.nativeEnabled=value(modes[s],0)>.5f;section.selected=juce::roundToInt(juce::jlimit(0.f,2.f,value(models[s],0)));for(int m=0;m<3;++m){auto& b=section.banks[m];const auto& cache=banks[s][m];b.bypass=value(cache.bypass,1)>.5f;b.trimDb=value(cache.trim,0);b.levelDb=value(cache.level,0);for(int c=0;c<postNativeModel(s,m).controlCount;++c)b.values[c]=value(cache.values[c],postNativeModel(s,m).controls[c].initial);}}
        sanitisePostNativeState(state);return state;
    }
};
} // namespace spectralforge

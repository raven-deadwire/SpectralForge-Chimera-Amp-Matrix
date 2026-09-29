#pragma once
#include "PedalBoardCatalog.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectralforge {
// Model-specific parameter banks never repurpose an old ID for another function.
// The model integer reserves 0..255 from the first version: adding a model does
// not change the normalized host position of any existing model selection.
inline void addPedalBoardParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    using ID=juce::ParameterID;
    // New instances open the production five-slot board. Legacy state restore
    // explicitly writes zero when the old project has no boardEnabled field.
    layout.add(std::make_unique<juce::AudioParameterBool>(ID{"boardEnabled",1},"PRE five-slot board",true,juce::AudioParameterBoolAttributes().withAutomatable(false)));
    layout.add(std::make_unique<juce::AudioParameterInt>(ID{"boardLowTap",1},"PRE LOW tap after position",0,5,2,juce::AudioParameterIntAttributes().withAutomatable(false)));
    for(int owner=0;owner<5;++owner) {
        layout.add(std::make_unique<juce::AudioParameterInt>(ID{pedalModelID(owner),1},"PRE owner "+juce::String(owner+1)+" model",0,255,0,juce::AudioParameterIntAttributes().withAutomatable(false)));
        layout.add(std::make_unique<juce::AudioParameterInt>(ID{pedalOrderID(owner),1},"PRE position "+juce::String(owner+1)+" owner",0,4,owner,juce::AudioParameterIntAttributes().withAutomatable(false)));
        for(int model=0;model<pedalModelCount;++model) {
            const auto& m=pedalModel(model);
            layout.add(std::make_unique<juce::AudioParameterBool>(ID{pedalBypassID(owner,model),1},"PRE owner "+juce::String(owner+1)+" "+m.name+" bypass",false));
            for(int c=0;c<m.controlCount;++c) {
                const auto& p=m.controls[(size_t)c];
                layout.add(std::make_unique<juce::AudioParameterFloat>(ID{pedalControlID(owner,model,c),1},
                    "PRE "+juce::String(owner+1)+" "+m.name+" "+p.label,
                    juce::NormalisableRange<float>(p.minimum,p.maximum,p.interval),p.initial,juce::AudioParameterFloatAttributes().withAutomatable(p.connected)));
            }
        }
    }
}
struct PedalBoardParameterCache {
    std::atomic<float>* enabled{}; std::atomic<float>* lowTap{};
    std::array<std::atomic<float>*,5> models{},order{};
    std::array<std::array<std::atomic<float>*,pedalModelCount>,5> bypass{};
    std::array<std::array<std::array<std::atomic<float>*,pedalMaxControls>,pedalModelCount>,5> controls{};
    void bind(juce::AudioProcessorValueTreeState& state) {
        enabled=state.getRawParameterValue("boardEnabled"); lowTap=state.getRawParameterValue("boardLowTap");
        for(int owner=0;owner<5;++owner) {
            models[(size_t)owner]=state.getRawParameterValue(pedalModelID(owner));
            for(int model=0;model<pedalModelCount;++model)bypass[(size_t)owner][(size_t)model]=state.getRawParameterValue(pedalBypassID(owner,model));
            order[(size_t)owner]=state.getRawParameterValue(pedalOrderID(owner));
            for(int model=1;model<pedalModelCount;++model) for(int c=0;c<pedalModel(model).controlCount;++c)
                controls[(size_t)owner][(size_t)model][(size_t)c]=state.getRawParameterValue(pedalControlID(owner,model,c));
        }
    }
    PedalBoardState read() const noexcept {
        PedalBoardState s;
        s.enabled=enabled!=nullptr && enabled->load()>.5f;
        if(lowTap)s.lowTap=juce::roundToInt(lowTap->load());
        for(int owner=0;owner<5;++owner) {
            const auto index=(size_t)owner;
            if(order[index])s.order[index]=juce::roundToInt(order[index]->load());
            const int raw=models[index]?juce::roundToInt(models[index]->load()):0;
            auto& instance=s.instances[index];instance=defaultPedalInstance(raw>=0 && raw<pedalModelCount?raw:0);
            const auto* bypassValue=bypass[index][(size_t)instance.model];
            instance.bypass=bypassValue!=nullptr && bypassValue->load()>.5f;
            for(int c=0;c<pedalModel(instance.model).controlCount;++c)
                if(const auto* value=controls[index][(size_t)instance.model][(size_t)c])instance.controls[(size_t)c]=value->load();
        }
        sanitisePedalBoard(s);return s;
    }
};
} // namespace spectralforge

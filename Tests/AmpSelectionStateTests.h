#pragma once
#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace ampSelectionStateTests {
inline void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }
inline void raw(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);
    require(parameter!=nullptr,"Amp state test parameter missing");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
inline void run() {
    using namespace spectralforge;
    const auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;
    const auto restoredStorage=std::make_unique<ChimeraProcessor>();auto& restored=*restoredStorage;
    const auto legacyNames=legacyAmpNames();
    require(legacyNames.size()==15,"Legacy amp names no longer contain exactly 15 host choices");
    for(int lane=0;lane<3;++lane) {
        auto* legacy=dynamic_cast<juce::AudioParameterChoice*>(p.parameters().getParameter("amp"+juce::String(lane+1)));
        require(legacy && legacy->choices==legacyNames,"Legacy host amp labels or choices changed");
        require(legacy->getParameterIndex()==3+15*lane,"Legacy amp host parameter index moved");
        require(legacy->isAutomatable(),"Original amp automation was disabled");
        require(!p.parameters().getParameter(ampExtensionID(lane))->isAutomatable(),"Extension selection must be an explicit structural edit");
        for(int index=0;index<15;++index) {
            const float normalized=float(index)/14.f;
            require(std::abs(legacy->convertTo0to1(float(index))-normalized)<1.e-6f,"Legacy amp normalized value was rescaled");
            legacy->setValueNotifyingHost(normalized);
            require(p.selectedAmpModel(lane)==index,"Legacy normalized automation resolves to the wrong model");
        }
    }

    for(int lane=0;lane<3;++lane) for(int model=15;model<23;++model) {
        p.setAmpModel(lane,model);
        require(p.selectedAmpModel(lane)==model,"An appended amp is not selectable");
        require(p.selectedAmpChannel(lane)==newAmpDefaultChannel(model),"New amp did not use its declared default channel");
        auto* old=p.parameters().getParameter("amp"+juce::String(lane+1));
        old->setValueNotifyingHost(0.f);
        require(p.selectedAmpModel(lane)==model,"Old host automation silently overrode the explicit new amp");
        require(!p.parameters().getParameter(ampChannelID(lane,model))->isAutomatable(),"Channel structure must not be an old automation alias");
        const int last=newAmpChannelCount(model)-1;
        p.setAmpChannel(lane,last);
        p.setAmpChannel(lane,last+1);
        require(p.selectedAmpChannel(lane)==last,"Invalid channel selection changed active state");
    }
    for(int lane=0;lane<3;++lane) for(int model=15;model<23;++model) {
        p.setAmpModel(lane,model);
        require(p.selectedAmpChannel(lane)==newAmpChannelCount(model)-1,"Switching amp lost its per-model channel bank");
    }
    p.setAmpModel(0,15);p.setAmpChannel(0,3);
    p.setAmpModel(1,15);p.setAmpChannel(1,1);
    p.setAmpModel(2,22);p.setAmpChannel(2,0);
    require(p.selectedAmpChannel(0)==3 && p.selectedAmpChannel(1)==1 && p.selectedAmpChannel(2)==0,"Amp channel banks leak between lanes");
    p.setAmpModel(0,23);p.setAmpModel(-1,1);p.setAmpChannel(3,0);
    require(p.selectedAmpModel(0)==15 && p.selectedAmpChannel(0)==3,"Invalid amp API input mutated a valid lane");

    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    restored.setStateInformation(saved.getData(),int(saved.getSize()));
    for(int lane=0;lane<3;++lane) {
        require(restored.selectedAmpModel(lane)==p.selectedAmpModel(lane),"Binary project restore lost appended amp choice");
        require(restored.selectedAmpChannel(lane)==p.selectedAmpChannel(lane),"Binary project restore lost active channel");
        for(int model=15;model<23;++model)
            require(restored.parameters().getRawParameterValue(ampChannelID(lane,model))->load()==p.parameters().getRawParameterValue(ampChannelID(lane,model))->load(),"Binary state lost an inactive channel bank");
    }
    p.copyComparison();p.selectComparison(1);
    p.setAmpModel(0,16);p.setAmpChannel(0,0);p.selectComparison(0);
    require(p.selectedAmpModel(0)==15 && p.selectedAmpChannel(0)==3,"A/B lost amp model or channel");
    p.selectComparison(1);
    require(p.selectedAmpModel(0)==16 && p.selectedAmpChannel(0)==0,"A/B recalled the wrong channel bank");

    auto legacyTree=restored.parameters().copyState();
    for(int i=legacyTree.getNumChildren()-1;i>=0;--i) {
        const auto id=legacyTree.getChild(i).getProperty("id").toString();
        if(id.startsWith("ampext") || id.startsWith("ampchannel"))legacyTree.removeChild(i,nullptr);
    }
    for(int lane=0;lane<3;++lane)
        legacyTree.getChildWithProperty("id","amp"+juce::String(lane+1)).setProperty("value",lane+10,nullptr);
    const auto xml=legacyTree.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,saved);
    restored.setStateInformation(saved.getData(),int(saved.getSize()));
    for(int lane=0;lane<3;++lane) {
        require(restored.selectedAmpModel(lane)==lane+10,"Old project inherited a new amp bank from the preceding project");
        for(int model=15;model<23;++model) {
            restored.setAmpModel(lane,model);
            require(restored.selectedAmpChannel(lane)==newAmpDefaultChannel(model),"Old project inherited a new channel bank");
        }
    }
    restored.setAmpModel(0,15);restored.setAmpChannel(0,3);
    restored.loadFactoryPreset(-1);
    require(restored.selectedAmpModel(0)==15 && restored.selectedAmpChannel(0)==3,"Invalid preset changed amp state");
    restored.loadFactoryPreset(0);
    for(int lane=0;lane<3;++lane) {
        require(restored.parameters().getRawParameterValue(ampExtensionID(lane))->load()==0,"Factory preset inherited the extension selector");
        for(int model=15;model<23;++model)
            require(restored.parameters().getRawParameterValue(ampChannelID(lane,model))->load()==newAmpDefaultChannel(model),"Factory preset inherited channel settings from another sound");
    }
    std::cout<<"PASS: legacy amp normalized/index/label compatibility; 8 appended models; independent lane/channel banks; binary and A/B recall; old-state and factory migration\n";
}
} // namespace ampSelectionStateTests

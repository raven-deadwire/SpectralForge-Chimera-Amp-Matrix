#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <stdexcept>
#include <set>

namespace nativeStateTests {
inline void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
inline void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Native state parameter missing");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
inline float get(ChimeraProcessor& p,const juce::String& id) {
    const auto* parameter=p.parameters().getRawParameterValue(id);require(parameter!=nullptr,"Native state raw parameter missing");return parameter->load();
}
inline void run(const juce::File& directory) {
    using namespace spectralforge;
    const auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;
    const auto recalledStorage=std::make_unique<ChimeraProcessor>();auto& recalled=*recalledStorage;
    juce::StringArray checked;
    std::set<std::string> hostIDs;
    for(auto* parameter:p.getParameters()) {
        const auto* identified=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        require(identified!=nullptr,"Host parameter has no stable ID");
        if(!hostIDs.insert(identified->paramID.toStdString()).second)
            throw std::runtime_error("Duplicate actual APVTS parameter ID: "+identified->paramID.toStdString());
    }
    checked.add("Every actual host parameter ID is unique across released, pedal, native amp and POST parameters");
    for(int mode=0;mode<3;++mode) {
        set(p,"mode",float(mode));
        for(int lane=0;lane<mode+1;++lane) {
            const int context=ampNativeContext(mode,lane),model=context+15;
            p.setAmpModel(lane,model);
            const auto& panel=ampNativePanel(model);const int channel=(int)panel.channels.size()-1;
            p.setAmpChannel(lane,channel);
            p.setAmpNativeRoute(lane,(int)panel.routes.size()-1);
            const auto& knob=panel.controls[0];
            set(p,ampNativeControlID(context,model,0),knob.minimum+(knob.maximum-knob.minimum)*(.1f+.13f*context));
            set(p,ampNativeInputTrimID(context),float(context-3));
            set(p,ampNativeOutputLevelID(context),float(3-context));
        }
    }
    for(int mode=0;mode<3;++mode) {
        set(p,"mode",float(mode));
        for(int lane=0;lane<mode+1;++lane) {
            const int context=ampNativeContext(mode,lane),model=context+15;
            require(p.selectedAmpModel(lane)==model,"Mode switch lost one of six independent amp choices");
            require(p.selectedAmpChannel(lane)==(int)ampNativePanel(model).channels.size()-1,"Mode switch lost channel bank");
            require(p.selectedAmpNativeRoute(lane)==(int)ampNativePanel(model).routes.size()-1,"Mode switch lost input route");
            p.setAmpModel(lane,0);p.setAmpModel(lane,model);
            require(p.selectedAmpChannel(lane)==(int)ampNativePanel(model).channels.size()-1,"Model change reset an inactive native bank");
        }
    }
    checked.add("Six Classic/Dual/Matrix amp contexts preserve model, channel, input route and controls independently");
    for(int section=0;section<3;++section) {
        p.activateNativePost(section);
        for(int model=0;model<3;++model) {
            set(p,postNativeModelID(section),float(model));
            set(p,postNativeBypassID(section,model),float((section+model)%2));
            set(p,postNativeTrimID(section,model),float(section*3+model-4));
            set(p,postNativeLevelID(section,model),float(4-section*3-model));
            for(int c=0;c<postNativeModel(section,model).controlCount;++c) {
                const auto& control=postNativeModel(section,model).controls[c];
                if(control.connected)set(p,postNativeControlID(section,model,c),control.maximum);
                else require(!p.parameters().getParameter(postNativeControlID(section,model,c))->isAutomatable(),"Unavailable hardware-only POST function is host-automatable");
            }
        }
    }
    const auto softwareTrim=postNativeTrimID(1,2),hardwareTrim=postNativeControlID(1,2,3);
    require(softwareTrim!=hardwareTrim,"ISA panel TRIM collides with software input trim");
    require(p.parameters().getRawParameterValue(softwareTrim)!=p.parameters().getRawParameterValue(hardwareTrim),"ISA and software trims share one APVTS storage cell");
    set(p,softwareTrim,-7.f);set(p,hardwareTrim,12.2f);
    PostNativeParameterCache postCache;postCache.bind(p.parameters());
    const auto isa=postCache.read().sections[1].banks[2];
    require(std::abs(isa.trimDb+7.f)<1.e-4f && std::abs(isa.values[3]-12.2f)<1.e-4f,"ISA and software trims are not independent through the actual DSP parameter cache");
    checked.add("ISA hardware TRIM 12.2 and software input trim -7 retain independent APVTS values and DSP cache entries");
    juce::MemoryBlock bytes;p.getStateInformation(bytes);recalled.setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(auto child:p.parameters().copyState()) {
        const auto id=child.getProperty("id").toString();
        if(id.startsWith("nativeAmp_") || id.startsWith("pn_"))
            require(std::abs(get(p,id)-get(recalled,id))<1.e-4f,"Binary project recall changed an active or inactive native amp/POST bank");
    }
    PostNativeParameterCache recalledPost;recalledPost.bind(recalled.parameters());
    const auto recalledISA=recalledPost.read().sections[1].banks[2];
    require(std::abs(recalledISA.trimDb+7.f)<1.e-4f && std::abs(recalledISA.values[3]-12.2f)<1.e-4f,"Binary recall merged the separate ISA and software trims");
    checked.add("Binary project recall preserves every native amp and POST bank, including inactive models");
    set(p,"mode",0);p.copyComparison();p.selectComparison(1);
    p.setAmpModel(0,22);set(p,postNativeModelID(2),0);set(p,postNativeControlID(2,0,0),7.f);
    p.selectComparison(0);require(p.selectedAmpModel(0)==15 && get(p,postNativeModelID(2))==2,"A/B native A was overwritten by B");
    p.selectComparison(1);require(p.selectedAmpModel(0)==22 && get(p,postNativeControlID(2,0,0))==7.f,"A/B failed to restore native B controls");
    checked.add("A/B retains distinct native amp and POST selections and parameter values");
    p.setPedalModel(0,26);set(p,pedalControlID(0,26,0),.237f);p.setPedalModel(0,27);p.setPedalModel(0,26);
    require(std::abs(get(p,pedalControlID(0,26,0))-.237f)<1.e-4f,"Immediate pedal selection discarded the remembered model bank");
    checked.add("Immediate pedal loading retains its model bank without an approval dialog");
    auto older=p.parameters().copyState();
    for(int i=older.getNumChildren()-1;i>=0;--i)if(older.getChild(i).getProperty("id").toString().startsWith("nativeAmp_"))older.removeChild(i,nullptr);
    const auto oldValue=[&](const juce::String& id,float value){older.getChildWithProperty("id",id).setProperty("value",value,nullptr);};
    oldValue("mode",2);oldValue(ampExtensionID(0),5);oldValue(ampChannelID(0,19),2);
    oldValue(ampExtensionID(1),7);oldValue(ampChannelID(1,21),0);
    const auto xml=older.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,bytes);recalled.setStateInformation(bytes.getData(),int(bytes.getSize()));
    require(recalled.selectedAmpModel(0)==19 && recalled.selectedAmpChannel(0)==0 && recalled.selectedAmpNativeRoute(0)==2,"Old Model T input route was incorrectly treated as a native channel");
    require(recalled.selectedAmpModel(1)==21 && recalled.selectedAmpChannel(1)==1,"Old SLO overdrive circuit migrated to Normal");
    require(get(recalled,postNativeControlID(2,0,0))==7.f,"Amp-only migration changed an existing POST bank");
    recalled.activateNativeAmp(0);recalled.activateNativeAmp(1);
    require(recalled.selectedAmpChannel(0)==0 && recalled.selectedAmpNativeRoute(0)==2 && recalled.selectedAmpChannel(1)==1,"First native edit lost the old circuit/input meaning");
    recalled.setAmpNativeRoute(0,1);recalled.setAmpChannel(0,0);
    require(get(recalled,ampChannelID(0,19))==1.f,"Single-channel selection overwrote the Model T compatibility input route");
    checked.add("Old Model T input routes and SLO overdrive project to the correct native channel/input without changing POST banks");
    juce::DynamicObject::Ptr report=new juce::DynamicObject();report->setProperty("checks",checked);report->setProperty("ampContexts",6);report->setProperty("ampModels",ampModelCount);report->setProperty("postModels",9);
    require(directory.getChildFile("Native-State-Verification.json").replaceWithText(juce::JSON::toString(juce::var(report.get()),true)),"Could not write native state evidence");
    std::cout<<"PASS: six native amp contexts, all native banks, POST nine models, binary/A-B recall and immediate pedal bank recall\n";
}
} // namespace nativeStateTests

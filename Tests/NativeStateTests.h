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
    // Released host ordinals (including POST) precede all new E670FE banks.
    bool sawAppended=false;int appended=0,originalCount=0,channelCount=0,niflheimrCount=0,cabCount=0,originalCabCount=0,expandedCabCount=0;
    for(auto* parameter:p.getParameters()) {
        const auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        require(id!=nullptr,"Parameter lacks a stable ID");
        const bool isNew=id->paramID.startsWith("nativeAmp_") && id->paramID.contains("_m23_");
        const bool isNiflheimr=(id->paramID.startsWith("originalAmp_") && id->paramID.contains("_niflheimr_ch"))
            || (id->paramID.startsWith("nativeAmp_") && id->paramID.contains("_m25_"));
        const bool isCab=id->paramID.startsWith("cabA") || id->paramID.startsWith("cabB") || id->paramID.startsWith("cabblend");
        if(id->paramID.startsWith("xcab")){require(parameter->getParameterIndex()==4824+expandedCabCount && parameter->getVersionHint()==8,"Expanded CAB append-only ordinal");++expandedCabCount;}
        else if(id->paramID.startsWith("ocab")){require(parameter->getParameterIndex()==4785+originalCabCount && parameter->getVersionHint()==7,"Original CAB append-only ordinal");++originalCabCount;}
        else if(isCab){require(parameter->getParameterIndex()==4323+6*(2+5*14)+cabCount && parameter->getVersionHint()==6,"CAB controls must append after Niflheimr");++cabCount;}
        else if(isNiflheimr){require(parameter->getParameterIndex()==4323+niflheimrCount && parameter->getVersionHint()==5,"Niflheimr must append after all released channel parameters");++niflheimrCount;}
        else if(isNew){require(parameter->getParameterIndex()==3686+appended,"E670FE appended ordinal moved");sawAppended=true;++appended;}
        else if(id->paramID=="gateRangeDb")require(appended==204 && parameter->getParameterIndex()==3890,"Gate Range must follow the complete E670FE bank");
        else if(id->paramID.startsWith("originalAmp_") && id->paramID.contains("_ch")){require(parameter->getParameterIndex()==3981+channelCount && parameter->getVersionHint()==4,"Original channel bank moved released ordinals");++channelCount;}
        else if(id->paramID.startsWith("originalAmp_") || (id->paramID.startsWith("nativeAmp_") && id->paramID.contains("_m24_"))){require(parameter->getParameterIndex()==3891+originalCount && parameter->getVersionHint()==3,"Original parameter moved ahead of released parameters");++originalCount;}
        else require(!sawAppended,"E670FE inserted ahead of a released host parameter");
    }
    require(channelCount==342,"Original channel bank incomplete");
    require(niflheimrCount==6*(2+5*14),"Niflheimr six-context five-channel bank incomplete");
    require(originalCabCount==spectralforge::originalCabParameterCount,"Original CAB bank incomplete");
    require(expandedCabCount==spectralforge::cabExpansionParameterCount,"Expanded CAB bank incomplete");
    require(cabCount==3*10,"CAB three-lane Mic A/B parameter bank incomplete");
    require(originalCount==90,"Original six-context bank incomplete");
    require(appended==6*(32+2),"E670FE six-context parameter bank incomplete");
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
    // Exercise every E670FE channel in all six contexts through binary recall.
    for(int channel=0;channel<5;++channel) {
        for(int mode=0;mode<3;++mode) {
            set(p,"mode",float(mode));
            for(int lane=0;lane<=mode;++lane) {
                const int context=ampNativeContext(mode,lane);
                p.setAmpModel(lane,23);p.setAmpChannel(lane,channel);
                for(int control=0;control<32;++control) {
                    const auto& k=ampNativePanel(23).controls[(size_t)control];
                    set(p,ampNativeControlID(context,23,control),k.kind==AmpNativeControlKind::knob ? .12f+.1f*context : float((channel+context)%2));
                }
            }
        }
        p.getStateInformation(bytes);recalled.setStateInformation(bytes.getData(),int(bytes.getSize()));
        for(int mode=0;mode<3;++mode) {
            set(recalled,"mode",float(mode));
            for(int lane=0;lane<=mode;++lane) {
                const int context=ampNativeContext(mode,lane);
                require(recalled.selectedAmpModel(lane)==23 && recalled.selectedAmpChannel(lane)==channel,"E670FE binary recall lost model/channel");
                for(int control=0;control<32;++control) {
                    const auto id=ampNativeControlID(context,23,control);
                    require(std::abs(get(p,id)-get(recalled,id))<1.e-5f,"E670FE recall lost a control bank");
                }
                recalled.setAmpModel(lane,16);recalled.setAmpModel(lane,23);
                require(recalled.selectedAmpChannel(lane)==channel,"Ironball return overwrote E670FE bank");
                set(recalled,ampNativeEnabledID(context),0);
                require(recalled.selectedAmpModel(lane)==23,"Native-only model fell through to legacy engine");
            }
        }
    }
    set(p,"mode",0);p.setAmpModel(0,16);p.setAmpChannel(0,1);
    set(p,ampNativeControlID(0,16,0),.219f);
    p.getStateInformation(bytes);recalled.setStateInformation(bytes.getData(),int(bytes.getSize()));
    require(recalled.selectedAmpModel(0)==16 && recalled.selectedAmpChannel(0)==1,"Retired Ironball session changed model/channel");
    require(std::abs(get(recalled,ampNativeControlID(0,16,0))-.219f)<1.e-5f,"Ironball bank changed on recall");
    p.selectComparison(0);p.setAmpModel(0,16);p.copyComparison();
    p.selectComparison(1);p.setAmpModel(0,23);p.setAmpChannel(0,4);
    p.selectComparison(0);require(p.selectedAmpModel(0)==16,"A/B lost legacy Ironball");
    p.selectComparison(1);require(p.selectedAmpModel(0)==23 && p.selectedAmpChannel(0)==4,"A/B lost E670FE Tube Driver");
    checked.add("E670FE append-only host ordering, 6 contexts x 5 channels binary recall, control banks, Ironball return/recall and A/B");
    // Every Niflheimr channel owns fourteen memories in all routing contexts.
    // Use distinct values to expose accidentally shared or aliased storage.
    AmpNativeParameterCache niflheimrCache;niflheimrCache.bind(p.parameters());
    for(int mode=0;mode<3;++mode) {
        set(p,"mode",float(mode));
        for(int lane=0;lane<=mode;++lane) {
            const int context=ampNativeContext(mode,lane);p.setAmpModel(lane,niflheimrAmpModel);
            for(int channel=0;channel<niflheimr::channelCount;++channel) {
                p.setAmpChannel(lane,channel);
                for(size_t control=0;control<niflheimr::controlCount;++control) {
                    const auto& k=niflheimr::controls[control];
                    const float fraction=.1f+.09f*context+.05f*channel+.003f*float(control);
                    set(p,ampNativeControlID(context,niflheimrAmpModel,int(control),channel),k.minimum+(k.maximum-k.minimum)*fraction);
                }
            }
            for(int channel=0;channel<niflheimr::channelCount;++channel) {
                p.setAmpChannel(lane,channel);const auto current=niflheimrCache.read(context);
                require(current.model==niflheimrAmpModel && current.channel==channel,"Niflheimr selection did not reach DSP cache");
                for(size_t control=0;control<niflheimr::controlCount;++control)
                    require(std::abs(current.values[control]-get(p,ampNativeControlID(context,niflheimrAmpModel,int(control),channel)))<1.e-4f,"Niflheimr DSP cache reads another channel's memory");
            }
            p.setAmpModel(lane,firstOriginalAmpModel);p.setAmpModel(lane,niflheimrAmpModel);
            require(p.selectedAmpChannel(lane)==4,"Niflheimr inactive model lost its selected channel");
        }
    }
    p.getStateInformation(bytes);recalled.setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(int context=0;context<ampNativeContextCount;++context)for(int channel=0;channel<niflheimr::channelCount;++channel)
        for(size_t control=0;control<niflheimr::controlCount;++control) {
            const auto id=ampNativeControlID(context,niflheimrAmpModel,int(control),channel);
            require(std::abs(get(p,id)-get(recalled,id))<1.e-4f,"Niflheimr binary project recall changed a channel memory");
        }
    set(p,"mode",0);p.selectComparison(0);p.setAmpModel(0,niflheimrAmpModel);p.setAmpChannel(0,2);
    const auto blendID=ampNativeControlID(0,niflheimrAmpModel,int(niflheimr::Control::blend),2);
    set(p,blendID,.31f);p.copyComparison();p.selectComparison(1);set(p,blendID,.87f);p.setAmpChannel(0,4);
    p.selectComparison(0);require(p.selectedAmpModel(0)==niflheimrAmpModel && p.selectedAmpChannel(0)==2 && std::abs(get(p,blendID)-.31f)<1.e-4f,"Niflheimr A/B lost channel A or its blend");
    p.selectComparison(1);require(p.selectedAmpChannel(0)==4 && std::abs(get(p,blendID)-.87f)<1.e-4f,"Niflheimr A/B lost inactive channel B memory");
    auto preNiflheimr=p.parameters().copyState();
    for(int i=preNiflheimr.getNumChildren()-1;i>=0;--i) {
        const auto id=preNiflheimr.getChild(i).getProperty("id").toString();
        if((id.startsWith("originalAmp_") && id.contains("_niflheimr_ch"))
            || (id.startsWith("nativeAmp_") && id.contains("_m25_")))preNiflheimr.removeChild(i,nullptr);
    }
    const auto preNiflheimrXml=preNiflheimr.createXml();juce::AudioProcessor::copyXmlToBinary(*preNiflheimrXml,bytes);
    recalled.setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(int context=0;context<ampNativeContextCount;++context)for(int channel=0;channel<niflheimr::channelCount;++channel)
        for(size_t control=0;control<niflheimr::controlCount;++control)
            require(std::abs(get(recalled,ampNativeControlID(context,niflheimrAmpModel,int(control),channel))-niflheimr::controls[control].initial)<.0011f,"Pre-Niflheimr state inherited stale channel values");
    checked.add("Niflheimr appended 432 parameters preserve all prior host ordinals; 6 contexts x 5 channels x 14 independent controls survive DSP-cache selection, binary/A-B recall and missing-bank migration");
    juce::DynamicObject::Ptr report=new juce::DynamicObject();report->setProperty("checks",checked);report->setProperty("ampContexts",6);report->setProperty("ampModels",ampModelCount);report->setProperty("postModels",9);
    require(directory.getChildFile("Native-State-Verification.json").replaceWithText(juce::JSON::toString(juce::var(report.get()),true)),"Could not write native state evidence");
    std::cout<<"PASS: six native amp contexts, all native banks, POST nine models, binary/A-B recall and immediate pedal bank recall\n";
}
} // namespace nativeStateTests

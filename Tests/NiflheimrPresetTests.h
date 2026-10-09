#pragma once
#include "NiflheimrPresets.h"
#include "FactoryCabVoicing.h"

// Included by the full production-processor test, reusing its deterministic
// bass fixture. These checks prove recall/render safety, not musical approval.
namespace niflheimrPresetTests {
inline void run(const juce::File& directory) {
    using namespace spectralforge;
    require(niflheimrPresetStart==43 && selectablePresetCount==48,"Niflheimr changed existing preset ordinals");
    constexpr std::array<const char*,5> stableIds{{
        "original.rig.niflheimr.hrimfaxi.v1","original.rig.niflheimr.garmr.v1",
        "original.rig.niflheimr.nidavellir.v1","original.rig.niflheimr.ymir.v1",
        "original.rig.niflheimr.hel.v1"
    }};
    constexpr std::array<int,5> cabLayouts{{6,7,6,7,4}},cabDrivers{{10,9,12,9,13}},cabMics{{5,2,3,5,5}};
    require(directory.createDirectory().wasOk(),"Cannot create Niflheimr preset output directory");
    juce::Array<juce::var> results;
    for(int preset=0;preset<niflheimrPresetCount;++preset){
        const int index=niflheimrPresetStart+preset;
        const auto& definition=niflheimrRigPresets[size_t(preset)];
        require(juce::String(definition.id)==stableIds[size_t(preset)],"Niflheimr display rename changed a serialized preset identity");
        const auto presetName=juce::String::fromUTF8(selectablePresetName(index));
        require(presetName==juce::String::fromUTF8(definition.name),"Niflheimr selectable preset name is stale");
        for(const auto* channelName:niflheimr::channelNames)
            require(presetName!=juce::String::fromUTF8(channelName),"Niflheimr full-rig preset name collides with an amp channel");
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        for(auto* raw:a->getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw))parameter->setValueNotifyingHost(.83f);
        a->loadFactoryPreset(index);b->loadFactoryPreset(index);
        factoryCabContract(*a,index);factoryCabContract(*b,index);
        for(int parameter=0;parameter<a->getParameters().size();++parameter)
            require(std::abs(a->getParameters()[parameter]->getValue()-b->getParameters()[parameter]->getValue())<1.e-6f,"Niflheimr recall inherited previous rig state");
        const auto value=[&](const juce::String& id){auto* p=a->parameters().getRawParameterValue(id);require(p!=nullptr,"Niflheimr preset parameter missing");return p->load();};
        require(int(value("mode"))==0 && a->selectedAmpModel(0)==niflheimrAmpModel && a->selectedAmpChannel(0)==preset,"Niflheimr preset did not select its real native channel");
        const auto voice=niflheimrRigVoice(preset);
        for(size_t control=0;control<niflheimr::controlCount;++control)
            require(std::abs(value(ampNativeControlID(0,niflheimrAmpModel,int(control),preset))-voice.values[control])<.0011f,"Niflheimr active native control not recalled");
        require(value(ampNativeControlID(0,niflheimrAmpModel,int(niflheimr::Control::blend),preset))<1.f,"Niflheimr preset discarded its clean blend");
        require(int(value(cabLayoutID(0,"layout")))==cabLayouts[size_t(preset)]
                && int(value(cabExpansionID(0,"driver")))==cabDrivers[size_t(preset)]
                && int(value(cabExpansionID(0,"Amic")))==cabMics[size_t(preset)],"Niflheimr full-rig cabinet recipe changed or was not recalled");
        for(int lane=1;lane<=3;++lane){
            const auto suffix=juce::String(lane);
            require(value("cabtype"+suffix)==0 && value("cabBtype"+suffix)==0 && value("cablow"+suffix)<=35.f,"Niflheimr preset depends on guitar/personal IR or cuts its bass foundation");
            require(value("mute"+suffix)==0 && value("solo"+suffix)==0,"Niflheimr recall retained stale solo/mute");
        }
        require(value("boardEnabled")==1 && value("delayon")==0 && value("reverbon")==0,"Niflheimr rhythm rig routing is incomplete");
        const auto board=a->pedalBoardState();
        require(board.instances[0].model!=0 && board.instances[1].model!=0,"Niflheimr PRE chain missing");
        require(value(postNativeBypassID(2,preset==4?2:0))==0,"Niflheimr POST contour missing");
        require(selectablePresetCategory(index)=="Original / Niflheimr","Niflheimr preset has no Original bank");
        require(adjacentPreset(adjacentPreset(index,1),-1)==index,"Niflheimr preset navigation is broken");
        require(a->parameters().state.getChildWithName("ORIGINAL_PRESET")["id"].toString()==definition.id,"Niflheimr full-rig identity missing");
        require(a->parameters().state.getChildWithName("ORIGINAL_PRESET")["name"].toString()==presetName,"Niflheimr full-rig metadata retained an old display name");
        require(a->parameters().state.getChildWithName("ORIGINAL_PRESET")["cabinetPolicy"].toString()==factoryCabPolicy,"Niflheimr state omitted its modeled CAB authoring policy");

        // Export using the same binary state API as the plugin's Save action.
        juce::MemoryBlock bytes;a->getStateInformation(bytes);
        require(directory.getChildFile(definition.fileName).replaceWithData(bytes.getData(),bytes.getSize()),"Cannot export Niflheimr .chimera preset");
        b->setStateInformation(bytes.getData(),int(bytes.getSize()));
        for(int parameter=0;parameter<a->getParameters().size();++parameter)
            require(std::abs(a->getParameters()[parameter]->getValue()-b->getParameters()[parameter]->getValue())<1.e-6f,"Niflheimr binary restore changed a parameter");
        require(b->parameters().state.getChildWithName("ORIGINAL_PRESET")["id"].toString()==definition.id,"Niflheimr binary restore lost full-rig identity");
        require(b->parameters().state.getChildWithName("ORIGINAL_PRESET")["name"].toString()==presetName,"Niflheimr binary restore lost full-rig display name");
        const auto audio=render(*a,true,450,index),restored=render(*b,true,450,index);
        double energy=0,residual=0;float peak=0;
        for(size_t n=0;n<audio.size();++n){energy+=double(audio[n])*audio[n];peak=std::max(peak,std::abs(audio[n]));residual=std::max(residual,std::abs(double(audio[n])-restored[n]));}
        const double rmsDb=10*std::log10(energy/audio.size());
        require(residual<1.e-6,"Niflheimr restored audio differs from the authored rig");
        require(peak<.95f && rmsDb>-30,"Niflheimr preset has unsafe or unusably quiet gain staging");
        set(*b,"input",6);const auto hot=render(*b,true,450,index);float hotPeak=0;
        for(float sample:hot)hotPeak=std::max(hotPeak,std::abs(sample));
        require(hotPeak<.95f,"Niflheimr preset overloads the +6 dB input fixture");
        // A/B has a separate saved full-rig state, independent of channel banks.
        // A legacy source change alone is inaudible while the modeled A slot
        // owns the path. Exercise the actual CAB enable/layout/driver/mic state.
        std::map<juce::String,float> cabinetBefore;
        for(auto* raw:a->getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw))
            if(parameter->paramID.startsWith("cab") || parameter->paramID.startsWith("ocab")
                || parameter->paramID.startsWith("xcab") || parameter->paramID.startsWith("lcab"))
                cabinetBefore[parameter->paramID]=parameter->getValue();
        a->copyComparison();a->selectComparison(1);
        set(*a,originalCabID(0,"Aon"),0);set(*a,cabLayoutID(0,"layout"),1);
        set(*a,cabExpansionID(0,"driver"),5);set(*a,cabExpansionID(0,"Amic"),14);
        set(*a,originalCabID(0,"Aposition"),.95f);set(*a,"cabAgain1",-9);
        const auto changed=render(*a,true,120);float changedDelta=0;
        for(size_t n=0;n<changed.size();++n)changedDelta=std::max(changedDelta,std::abs(changed[n]-audio[n]));
        require(changedDelta>1.e-5f,"Niflheimr A/B CAB fixture did not change the rendered sound");
        a->selectComparison(0);
        for(const auto& [id,before]:cabinetBefore)require(std::abs(a->parameters().getParameter(id)->getValue()-before)<1.e-6f,"Niflheimr A/B recall changed a CAB parameter");
        factoryCabContract(*a,index);
        require(a->parameters().state.getChildWithName("ORIGINAL_PRESET")["id"].toString()==definition.id,"Niflheimr A/B recall lost bass cabinet identity");
        const auto comparison=render(*a,true,450,index);float comparisonDelta=0;size_t comparisonAt=0;
        for(size_t n=0;n<audio.size();++n)if(const float delta=std::abs(comparison[n]-audio[n]);delta>comparisonDelta){comparisonDelta=delta;comparisonAt=n;}
        if(comparisonDelta>=1.e-6f)std::cerr<<"NIFLHEIMR_COMPARISON preset="<<index<<" max_delta="<<comparisonDelta<<" at="<<comparisonAt
                                        <<" before="<<audio[comparisonAt]<<" after="<<comparison[comparisonAt]<<'\n';
        require(comparisonDelta<1.e-6f,"Niflheimr A/B CAB recall changed the authored rig audio");
        a->loadFactoryPreset(0);
        require(!a->parameters().state.getChildWithName("ORIGINAL_PRESET").isValid(),"Unrelated factory recall retained Niflheimr identity");

        auto* row=new juce::DynamicObject();row->setProperty("id",definition.id);row->setProperty("name",juce::String::fromUTF8(definition.name));
        row->setProperty("ordinal",index);row->setProperty("channel",preset);row->setProperty("file",definition.fileName);
        row->setProperty("rms_dbfs",rmsDb);row->setProperty("peak_dbfs",20*std::log10(peak));row->setProperty("hot_peak_dbfs",20*std::log10(hotPeak));
        row->setProperty("restore_max_error",residual);row->setProperty("comparison_max_error",comparisonDelta);
        row->setProperty("cab_layout",cabLayouts[size_t(preset)]);row->setProperty("cab_driver",cabDrivers[size_t(preset)]);row->setProperty("cab_mic",cabMics[size_t(preset)]);results.add(juce::var(row));
        std::cout<<"PASS NIFLHEIMR_PRESET,"<<index<<","<<definition.name<<","<<rmsDb<<","<<20*std::log10(peak)<<","<<20*std::log10(hotPeak)<<","<<residual<<'\n';
    }
    auto* report=new juce::DynamicObject();report->setProperty("schema","spectralforge.niflheimr.original-presets.v1");
    report->setProperty("fixture","deterministic synthetic bass plucks, 48 kHz, production full-rig processor");
    report->setProperty("musical_acceptance",false);report->setProperty("personal_ir_required",false);
    report->setProperty("cabinet_policy",factoryCabPolicy);report->setProperty("presets",results);
    require(directory.getChildFile("niflheimr-presets-validation.json").replaceWithText(juce::JSON::toString(juce::var(report),true)),"Cannot save Niflheimr preset validation report");
}
} // namespace niflheimrPresetTests

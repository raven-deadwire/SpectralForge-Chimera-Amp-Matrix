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
    constexpr std::array<const char*,5> names{{"Progressive Clank","Death Grind","Industrial Pulse","Slam Punch","Doom Fuzz"}};
    constexpr std::array<const char*,5> legacyNames{{"Frostline Precision","Carrion Barrage","Foundry Pulse","Jötunn Hammer","Mirebound Monolith"}};
    constexpr std::array<int,5> modes{{0,1,2,2,0}};
    constexpr std::array<std::array<int,3>,5> channels{{{{0,0,0}},{{0,1,0}},{{0,2,0}},{{3,3,1}},{{4,0,0}}}};
    constexpr std::array<std::array<int,3>,5> cabLayouts{{{{6,0,0}},{{9,7,0}},{{9,6,8}},{{7,7,6}},{{4,0,0}}}};
    constexpr std::array<std::array<int,3>,5> cabDrivers{{{{10,0,0}},{{14,9,0}},{{14,12,11}},{{9,9,12}},{{13,0,0}}}};
    constexpr std::array<std::array<int,3>,5> cabMics{{{{5,0,0}},{{5,2,0}},{{5,3,14}},{{5,5,3}},{{5,0,0}}}};
    require(directory.createDirectory().wasOk(),"Cannot create Niflheimr preset output directory");
    juce::Array<juce::var> results;
    for(int preset=0;preset<niflheimrPresetCount;++preset){
        const int index=niflheimrPresetStart+preset;
        const auto& definition=niflheimrRigPresets[size_t(preset)];
        require(juce::String(definition.id)==stableIds[size_t(preset)],"Niflheimr display rename changed a serialized preset identity");
        const auto presetName=juce::String::fromUTF8(selectablePresetName(index));
        require(presetName==juce::String::fromUTF8(definition.name),"Niflheimr selectable preset name is stale");
        require(presetName==names[size_t(preset)],"Niflheimr preset name does not describe its musical use");
        for(const auto* channelName:niflheimr::channelNames)
            require(presetName!=juce::String::fromUTF8(channelName),"Niflheimr full-rig preset name collides with an amp channel");
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        for(auto* raw:a->getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw))parameter->setValueNotifyingHost(.83f);
        a->loadFactoryPreset(index);b->loadFactoryPreset(index);
        factoryCabContract(*a,index);factoryCabContract(*b,index);
        for(int parameter=0;parameter<a->getParameters().size();++parameter)
            require(std::abs(a->getParameters()[parameter]->getValue()-b->getParameters()[parameter]->getValue())<1.e-6f,"Niflheimr recall inherited previous rig state");
        const auto value=[&](const juce::String& id){auto* p=a->parameters().getRawParameterValue(id);require(p!=nullptr,"Niflheimr preset parameter missing");return p->load();};
        const int mode=modes[size_t(preset)],laneCount=mode+1;
        require(int(value("mode"))==mode && definition.mode==mode,"Niflheimr preset did not recall its authored routing mode");
        require(std::abs(value("output"))<1.e-5f,"Niflheimr preset must recall OUTPUT at 0 dB");
        require(value("boardLowTap")==float(mode==2?1:0),"Niflheimr Matrix LOW tap does not follow the clean compressor");
        if(mode==1)require(value("dualtype")==1 && std::abs(value("dualcross")-180.f)<.1f,
            "Death Grind must split its clean lows and upper grind at 180 Hz");
        if(mode==2) {
            require(std::abs(value("x1")-(preset==2?140.f:110.f))<.1f
                    && std::abs(value("x2")-(preset==2?1600.f:900.f))<.1f,"Niflheimr Matrix crossover recipe is missing");
            require(std::abs(value("lowcomp")-(preset==2?.25f:.5f))<.0011f
                    && std::abs(value("lowampmix")-(preset==2?0.f:.25f))<.0011f,"Niflheimr Matrix DI/amp blend is unauthored");
        }
        juce::Array<juce::var> routeRows;
        for(int lane=0;lane<laneCount;++lane) {
            const int context=ampNativeContext(mode,lane),channel=channels[size_t(preset)][size_t(lane)];
            const bool dryLow=preset==2 && lane==0;const auto suffix=juce::String(lane+1);
            require(value(ampNativeEnabledID(context))==1 && a->selectedAmpModel(lane)==niflheimrAmpModel
                    && a->selectedAmpChannel(lane)==channel,"Niflheimr active lane did not recall its real native context/channel");
            const auto voice=niflheimrRigVoice(preset,lane);
            require(voice.channel==channel,"Niflheimr channel recipe disagrees with the documented routing role");
            for(size_t control=0;control<niflheimr::controlCount;++control)
                require(std::abs(value(ampNativeControlID(context,niflheimrAmpModel,int(control),channel))-voice.values[control])<.0011f,
                        "Niflheimr active native control not recalled in the selected mode");
            require(value(ampNativeControlID(context,niflheimrAmpModel,int(niflheimr::Control::blend),channel))<1.f,
                    "Niflheimr preset discarded its clean native blend");
            require(int(value(cabLayoutID(lane,"layout")))==cabLayouts[size_t(preset)][size_t(lane)]
                    && int(value(cabExpansionID(lane,"driver")))==cabDrivers[size_t(preset)][size_t(lane)]
                    && int(value(cabExpansionID(lane,"Amic")))==cabMics[size_t(preset)][size_t(lane)],
                    "Niflheimr lane cabinet recipe changed or was not recalled");
            require(value("ampon"+suffix)==(dryLow?0.f:1.f) && value("cab"+suffix)==(dryLow?0.f:1.f),
                    "Niflheimr active lane AMP/CAB bypass differs from its DI role");
            require(value("level"+suffix)==0,"Niflheimr native rig contains an ineffective legacy output correction");
            auto* laneRow=new juce::DynamicObject();laneRow->setProperty("lane",lane);
            laneRow->setProperty("native_context",context);laneRow->setProperty("channel",channel);
            laneRow->setProperty("channel_name",juce::String::fromUTF8(niflheimr::channelNames[channel]));
            laneRow->setProperty("amp_enabled",!dryLow);laneRow->setProperty("cab_enabled",!dryLow);
            laneRow->setProperty("cab_layout",cabLayouts[size_t(preset)][size_t(lane)]);
            laneRow->setProperty("cab_driver",cabDrivers[size_t(preset)][size_t(lane)]);
            laneRow->setProperty("cab_mic",cabMics[size_t(preset)][size_t(lane)]);
            laneRow->setProperty("native_output_db",value(ampNativeOutputLevelID(context)));
            laneRow->setProperty("cab_mic_level_db",value("cabAgain"+suffix));
            if(mode==2 && lane==0)laneRow->setProperty("low_di_share",1.f-value("lowampmix"));
            routeRows.add(juce::var(laneRow));
        }
        for(int lane=1;lane<=3;++lane){
            const auto suffix=juce::String(lane);
            require(value("cabtype"+suffix)==0 && value("cabBtype"+suffix)==0 && value("cablow"+suffix)<=35.f,"Niflheimr preset depends on guitar/personal IR or cuts its bass foundation");
            require(value("mute"+suffix)==0 && value("solo"+suffix)==0,"Niflheimr recall retained stale solo/mute");
            if(lane>laneCount)require(value("ampon"+suffix)==0 && value("cab"+suffix)==0,
                "Niflheimr recall left an unused rig enabled");
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
        // A route label is insufficient: muting each of the authored lanes
        // must remove a real contribution from the production bass render.
        if(mode!=0)for(int lane=0;lane<laneCount;++lane) {
            b->setStateInformation(bytes.getData(),int(bytes.getSize()));set(*b,"mute"+juce::String(lane+1),1);
            const auto muted=render(*b,true,120);float delta=0;
            for(size_t n=0;n<muted.size();++n)delta=std::max(delta,std::abs(muted[n]-audio[n]));
            require(delta>1.e-5f,"Niflheimr multi-rig preset contains an inaudible active lane");
        }
        if(preset==2) {
            b->setStateInformation(bytes.getData(),int(bytes.getSize()));
            set(*b,ampNativeOutputLevelID(ampNativeContext(2,0)),24);
            set(*b,cabExpansionID(0,"driver"),9);set(*b,cabLayoutID(0,"layout"),7);
            const auto parked=render(*b,true,120);float delta=0;
            for(size_t n=0;n<parked.size();++n)delta=std::max(delta,std::abs(parked[n]-audio[n]));
            require(delta<1.e-6f,"Industrial Pulse parked LOW amp/cabinet leaked into its clean DI");
        }
        b->setStateInformation(bytes.getData(),int(bytes.getSize()));
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
        const int changedLane=preset==2?1:0;
        set(*a,originalCabID(changedLane,"Aon"),0);set(*a,cabLayoutID(changedLane,"layout"),1);
        set(*a,cabExpansionID(changedLane,"driver"),5);set(*a,cabExpansionID(changedLane,"Amic"),14);
        set(*a,originalCabID(changedLane,"Aposition"),.95f);set(*a,"cabAgain"+juce::String(changedLane+1),-9);
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

        // Old .chimera/project snapshots remain actual stored sounds. A new
        // factory name/route must not migrate an existing Classic reference.
        b->setStateInformation(bytes.getData(),int(bytes.getSize()));
        set(*b,"mode",0);set(*b,ampNativeEnabledID(0),1);set(*b,ampNativeModelID(0),float(niflheimrAmpModel));
        set(*b,ampNativeChannelID(0,niflheimrAmpModel),float(preset));set(*b,"output",-2.25f);
        auto legacy=b->parameters().state.getChildWithName("ORIGINAL_PRESET");
        legacy.setProperty("name",juce::String::fromUTF8(legacyNames[size_t(preset)]),nullptr);
        legacy.setProperty("formatVersion",2,nullptr);
        juce::MemoryBlock legacyBytes;b->getStateInformation(legacyBytes);
        a->setStateInformation(legacyBytes.getData(),int(legacyBytes.getSize()));
        require(value("mode")==0 && a->selectedAmpChannel(0)==preset && std::abs(value("output")+2.25f)<.001f,
                "Niflheimr preset rename migrated the routing/channel/output of an existing saved sound");
        require(a->parameters().state.getChildWithName("ORIGINAL_PRESET")["name"].toString()
                ==juce::String::fromUTF8(legacyNames[size_t(preset)]),"Niflheimr preset rename overwrote a saved reference name");
        for(int parameter=0;parameter<a->getParameters().size();++parameter)
            require(std::abs(a->getParameters()[parameter]->getValue()-b->getParameters()[parameter]->getValue())<1.e-6f,
                    "Niflheimr legacy reference restore changed a stored parameter");

        auto* row=new juce::DynamicObject();row->setProperty("id",definition.id);row->setProperty("name",juce::String::fromUTF8(definition.name));
        row->setProperty("ordinal",index);row->setProperty("primary_channel",preset);row->setProperty("file",definition.fileName);
        row->setProperty("mode",mode);row->setProperty("lanes",routeRows);row->setProperty("output_db",0);
        row->setProperty("rms_dbfs",rmsDb);row->setProperty("peak_dbfs",20*std::log10(peak));row->setProperty("hot_peak_dbfs",20*std::log10(hotPeak));
        row->setProperty("restore_max_error",residual);row->setProperty("comparison_max_error",comparisonDelta);
        results.add(juce::var(row));
        std::cout<<"PASS NIFLHEIMR_PRESET,"<<index<<","<<definition.name<<","<<rmsDb<<","<<20*std::log10(peak)<<","<<20*std::log10(hotPeak)<<","<<residual<<'\n';
    }
    auto* report=new juce::DynamicObject();report->setProperty("schema","spectralforge.niflheimr.original-presets.v2");
    report->setProperty("fixture","deterministic synthetic bass plucks, 48 kHz, production full-rig processor");
    report->setProperty("musical_acceptance",false);report->setProperty("personal_ir_required",false);
    report->setProperty("cabinet_policy",factoryCabPolicy);report->setProperty("presets",results);
    require(directory.getChildFile("niflheimr-presets-validation.json").replaceWithText(juce::JSON::toString(juce::var(report),true)),"Cannot save Niflheimr preset validation report");
}
} // namespace niflheimrPresetTests

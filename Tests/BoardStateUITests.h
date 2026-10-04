#pragma once
#include "PluginProcessor.h"
#include <stdexcept>
#include <iostream>

// Run with the real Processor/APVTS on the Windows UI-test target. These are
// state/identity assertions, not hardware-tone or DAW certification.
namespace boardStateTests {
inline void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
inline void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Board test parameter absent");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
inline float raw(ChimeraProcessor& p,const juce::String& id) {
    auto* value=p.parameters().getRawParameterValue(id);require(value!=nullptr,"Board test raw parameter absent");return value->load();
}
inline void equal(const spectralforge::PedalBoardState& a,const spectralforge::PedalBoardState& b,const char* message) {
    require(a.enabled==b.enabled && a.lowTap==b.lowTap && a.order==b.order,message);
    for(int owner=0;owner<spectralforge::pedalBoardCapacity;++owner) {
        const auto& x=a.instances[(size_t)owner];const auto& y=b.instances[(size_t)owner];
        require(x.model==y.model && x.bypass==y.bypass,message);
        for(int c=0;c<spectralforge::pedalModel(x.model).controlCount;++c)
            require(std::abs(x.controls[(size_t)c]-y.controls[(size_t)c])<1.e-5f,message);
    }
}
inline void sendCC(ChimeraProcessor& p,int cc,int value) {
    juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(1,cc,value),0);
    juce::AudioBuffer<float> audio(2,256);audio.clear();p.processBlock(audio,midi);
}
inline juce::MemoryBlock save(ChimeraProcessor& p) {juce::MemoryBlock bytes;p.getStateInformation(bytes);return bytes;}
inline void load(ChimeraProcessor& p,const juce::MemoryBlock& bytes) {p.setStateInformation(bytes.getData(),(int)bytes.getSize());}

inline void run(const juce::File& outputDirectory) {
    using namespace spectralforge;
    juce::StringArray passed;
    std::cout<<"BEGIN universal PRE state checks; Processor size "<<sizeof(ChimeraProcessor)<<" bytes (heap-owned like plugin instances)\n";
    const auto mark=[&](const juce::String& name){passed.add(name);std::cout<<"PASS board state: "<<name<<"\n";};
    {
        const auto pStorage=std::make_unique<ChimeraProcessor>();auto& p=*pStorage;
        require(p.pedalBoardState().enabled,"New plugin instances must open with the five-slot board active");
        for(const auto& id:juce::StringArray{"boardEnabled","boardLowTap",pedalModelID(0),pedalOrderID(0)})
            require(!p.parameters().getParameter(id)->isAutomatable(),"Structural board edit is still host-automatable");
        require(!p.parameters().getParameter(pedalControlID(0,10,1))->isAutomatable(),
                "Disconnected Variable Mu LINK control is still host-automatable");
        set(p,"boardEnabled",1);p.setPedalModel(0,26);p.setPedalModel(1,31);
        const auto driveID=pedalControlID(0,26,0),eqID=pedalControlID(1,31,5);
        auto* driveParameter=p.parameters().getParameter(driveID);
        set(p,driveID,.173f);set(p,eqID,7.2f);set(p,"boardLowTap",1);set(p,pedalBypassID(1,31),1);
        const auto initial=p.pedalBoardState();
        p.setPedalModel(0,27);set(p,pedalControlID(0,27,1),.682f);
        p.setPedalModel(0,26);
        require(std::abs(raw(p,driveID)-.173f)<1.e-5f,"Selecting an already used pedal silently reset its saved parameter bank");
        p.setPedalModel(0,27);
        require(std::abs(raw(p,pedalControlID(0,27,1))-.682f)<1.e-5f,"Returning to a pedal did not retain its own settings");
        p.setPedalModel(0,26);
        p.movePedal(0,1);const auto moved=p.pedalBoardState();
        require(moved.order[0]==1 && moved.order[1]==0,"Processor move did not exchange positions");
        require(p.parameters().getParameter(driveID)==driveParameter && std::abs(raw(p,driveID)-.173f)<1.e-5f,
                "Move repurposed the host parameter object or owner value");
        require(moved.instances[0].model==26 && moved.instances[1].model==31,"Move changed owner model identity");
        p.undoPedalEdit();equal(p.pedalBoardState(),initial,"Undo failed to restore order and controls");
        p.undoPedalEdit(true);equal(p.pedalBoardState(),moved,"Redo failed to restore order and controls");
        require(p.duplicatePedal(0),"Duplicate into an empty owner failed");
        auto duplicated=p.pedalBoardState();
        require(duplicated.instances[2].model==26 && std::abs(duplicated.instances[2].controls[0]-.173f)<1.e-5f,
                "Duplicate did not copy the source control state");
        const auto duplicateID=pedalControlID(2,26,0);
        require(duplicateID!=driveID && p.parameters().getParameter(duplicateID)!=driveParameter,
                "Duplicate aliased the source host parameter identity");
        set(p,duplicateID,.821f);
        require(std::abs(raw(p,driveID)-.173f)<1.e-5f,"Editing duplicate altered source instance");
        p.setPedalModel(3,6);p.setPedalModel(4,7);const auto full=p.pedalBoardState();
        require(!p.duplicatePedal(0),"A sixth pedal was admitted");
        equal(p.pedalBoardState(),full,"Rejected sixth pedal still mutated the board");
        for(int invalid:{-1,40,255,1000}) {p.setPedalModel(0,invalid);equal(p.pedalBoardState(),full,"Invalid model changed the board");}
        p.setPedalModel(-1,26);p.setPedalModel(5,26);equal(p.pedalBoardState(),full,"Invalid owner changed the board");
        p.setPedalModel(4,0);require(p.pedalBoardState().instances[4].model==0,"Delete did not create Empty");
        require(p.duplicatePedal(0),"Empty owner could not be reused");
        require(p.pedalBoardState().instances[4].model==26,"Duplicate reused the wrong owner");
        mark("APVTS owner identity, move, independent duplicate, Empty, capacity and invalid-model guards");
        mark("Processor undo and redo restore structural snapshots");

        const auto expected=p.pedalBoardState();const auto bytes=save(p);const auto recalledStorage=std::make_unique<ChimeraProcessor>();auto& recalled=*recalledStorage;load(recalled,bytes);
        equal(recalled.pedalBoardState(),expected,"Project recall lost enabled/order/tap/model/bypass/control state");
        require(std::abs(raw(recalled,duplicateID)-.821f)<1.e-5f,"Project recall lost an independent duplicate control");
        require(std::abs(raw(recalled,pedalControlID(0,27,1))-.682f)<1.e-5f,"Project recall lost an inactive pedal model bank");
        mark("Actual Processor binary state round trip of the universal board");

        p.selectComparison(1);p.setPedalModel(3,27);set(p,pedalControlID(3,27,1),.682f);
        set(p,"boardLowTap",4);set(p,pedalBypassID(0,26),1);p.movePedal(0,1);const auto b=p.pedalBoardState();
        p.selectComparison(0);equal(p.pedalBoardState(),expected,"A/B failed to restore board A");
        p.selectComparison(1);equal(p.pedalBoardState(),b,"A/B failed to restore board B");
        const auto comparisons=save(p);const auto recalledABStorage=std::make_unique<ChimeraProcessor>();auto& recalledAB=*recalledABStorage;load(recalledAB,comparisons);
        require(recalledAB.comparisonSlot()==1,"Project recall lost selected comparison");
        equal(recalledAB.pedalBoardState(),b,"Project recall lost selected board B");
        recalledAB.selectComparison(0);equal(recalledAB.pedalBoardState(),expected,"Project recall lost inactive board A");
        mark("A/B board snapshots and both saved comparison slots");
    }
    {
        const auto pStorage=std::make_unique<ChimeraProcessor>();auto& p=*pStorage;set(p,"preon",1);set(p,"predrive",.27f);set(p,"gainorder",1);
        set(p,"boardEnabled",1);p.setPedalModel(0,26);set(p,"boardLowTap",5);
        auto legacy=p.parameters().copyState();
        for(int i=legacy.getNumChildren();--i>=0;) {
            const auto id=legacy.getChild(i).getProperty("id").toString();
            if(id.startsWith("board") || id.startsWith("nativeAmp_") || id.startsWith("pn_"))legacy.removeChild(i,nullptr);
        }
        legacy.removeProperty("schemaVersion",nullptr);
        juce::MemoryBlock data;auto xml=legacy.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,data);
        load(p,data);const auto state=p.pedalBoardState();
        require(!state.enabled,"A pre-board legacy project activated the new board");
        require(state.order==std::array<int,5>{1,0,2,4,3} && state.lowTap==2,"Old project order was not represented on the five-slot board");
        constexpr int seededModels[]{6,11,16,21,1};
        constexpr const char* enabled[]{"precompon","filteron","fuzzon","booston","preon"};
        for(int owner=0;owner<5;++owner) {
            require(state.instances[(size_t)owner].model==seededModels[owner],"Old project did not seed its pedal families into the current board");
            require(state.instances[(size_t)owner].bypass==(raw(p,enabled[owner])<.5f),"Old project pedal ON/OFF state changed during visible-board migration");
        }
        require(raw(p,"preon")==1 && std::abs(raw(p,"predrive")-.27f)<1.e-5f && raw(p,"gainorder")==1,
                "Board migration changed legacy sound controls");
        p.undoPedalEdit();equal(p.pedalBoardState(),state,"Loading another project retained stale board undo history");
        p.undoPedalEdit(true);equal(p.pedalBoardState(),state,"Loading another project retained stale board redo history");
        mark("Old project seeds current five-slot models/order/ON-OFF, preserves compatibility audio and raw parameters");
    }
    {
        const auto pStorage=std::make_unique<ChimeraProcessor>();auto& p=*pStorage;p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);
        set(p,"boardEnabled",1);p.setPedalModel(0,26);const auto id=pedalControlID(0,26,0);
        p.learnMidi(id);sendCC(p,40,127);
        require(std::abs(raw(p,id)-1)<1.e-5f && !p.learningMidi(),"Board MIDI learn did not reach the selected owner/control");
        p.movePedal(0,1);sendCC(p,40,0);require(std::abs(raw(p,id))<1.e-5f,"MIDI binding did not follow moved owner");
        require(p.duplicatePedal(0),"MIDI test duplicate failed");
        set(p,pedalControlID(1,26,0),.777f);sendCC(p,40,127);
        require(std::abs(raw(p,pedalControlID(1,26,0))-.777f)<1.e-5f,"Duplicated instance inherited the source MIDI binding");
        const auto saved=save(p);const auto recalledStorage=std::make_unique<ChimeraProcessor>();auto& recalled=*recalledStorage;load(recalled,saved);
        recalled.setRateAndBufferSizeDetails(48000,256);recalled.prepareToPlay(48000,256);sendCC(recalled,40,0);
        require(std::abs(raw(recalled,id))<1.e-5f,"Board MIDI binding did not survive project recall");
        require(std::abs(raw(recalled,pedalControlID(1,26,0))-.777f)<1.e-5f,"Restored MIDI binding targeted the duplicate");
        p.setPedalModel(0,27);const float retired=raw(p,id);sendCC(p,40,0);
        require(raw(p,id)==retired,"Explicit replacement retained the retired control MIDI map");
        p.learnMidi(pedalControlID(0,27,0));require(p.learningMidi(),"Pending board MIDI learn was not armed");
        p.setPedalModel(0,26);require(!p.learningMidi(),"Replacement retained pending MIDI learn for the retired model");
        mark("MIDI learn, moved owner, independent duplication, binary recall and explicit replacement retirement");
    }
    {
        const auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;
        p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);
        const int originalTotal=p.getLatencySamples(),originalBoard=p.pedalBoardLatencySamples();
        p.setPedalModel(0,39);sendCC(p,119,0);
        const int selectedTotal=originalTotal-originalBoard+p.pedalBoardLatencySamples();
        require(p.getLatencySamples()==selectedTotal && selectedTotal>originalTotal+1000,"Poly octave delay is not reported to the host");
        set(p,pedalBypassID(0,39),1);sendCC(p,119,0);
        require(p.getLatencySamples()==selectedTotal,"Poly bypass unexpectedly changed host latency");
        p.setPedalModel(0,0);sendCC(p,119,0);
        require(p.getLatencySamples()==originalTotal,"Deleting a poly octave failed to remove its extra latency");
        p.setPedalModel(0,38);sendCC(p,119,0);
        require(p.getLatencySamples()==originalTotal,"Mono octave inherited the poly frame delay");
        set(p,pedalControlID(0,38,1),.613f);
        set(p,pedalBypassID(0,38),1);
        set(p,"boardEnabled",0);
        p.loadFactoryPreset(0);
        require(p.pedalBoardState().enabled,"Native factory sound did not activate the five-slot board");
        constexpr int familyFirst[]{6,11,16,21,1};
        constexpr const char* familyModel[]{"compmodel","filtermodel","fuzzmodel","boostmodel","drivemodel"};
        constexpr const char* enabled[]{"precompon","filteron","fuzzon","booston","preon"};
        const auto factoryBoard=p.pedalBoardState();
        std::array<int,5> expectedOrder{0,1,2,3,4};
        if(raw(p,"preorder")>.5f)std::swap(expectedOrder[0],expectedOrder[1]);
        if(raw(p,"gainorder")>.5f)std::swap(expectedOrder[3],expectedOrder[4]);
        require(factoryBoard.order==expectedOrder && factoryBoard.lowTap==2,"Native factory sound did not initialize visible order/LOW tap");
        for(int owner=0;owner<5;++owner) {
            const auto& instance=factoryBoard.instances[(size_t)owner];
            require(instance.model==familyFirst[owner]+int(raw(p,familyModel[owner])),"Factory sound did not populate the visible pedal selection");
            require(instance.bypass==(raw(p,enabled[owner])<.5f),"Factory sound did not populate the visible pedal ON/OFF state");
        }
        require(std::abs(raw(p,pedalControlID(0,38,1))-.613f)<1.e-5f,"Factory sound erased an inactive user's pedal bank");
        require(raw(p,pedalBypassID(0,38))>.5f,"Factory sound erased an inactive user's pedal bypass");
        p.setPedalModel(0,38);
        // Explicit user selection intentionally turns the chosen pedal on.
        require(p.pedalBoardState().enabled && !p.pedalBoardState().instances[0].bypass && std::abs(p.pedalBoardState().instances[0].controls[1]-.613f)<1.e-5f,"Direct selection did not activate and recall the saved inactive pedal bank");
        mark("Poly host latency/bypass/delete, zero-frame mono, native factory activation/model/bypass/order and inactive bank recall");
    }
    auto* report=new juce::DynamicObject();report->setProperty("status","PASS");
    report->setProperty("scope","Native Processor/APVTS state and identity assertions; no external DAW or hardware-fidelity claim");
    juce::Array<juce::var> rows;for(const auto& name:passed)rows.add(name);report->setProperty("passed_groups",rows);
    report->setProperty("known_boundary","Host automation IDs remain fixed owner/model/control IDs; replacement does not erase host-owned automation lanes.");
    require(outputDirectory.getChildFile("Board-State-Verification.json").replaceWithText(juce::JSON::toString(juce::var(report),true)),"Cannot write board state verification report");
    std::cout<<"PASS universal PRE Processor/APVTS: state, A/B, MIDI, legacy, owner identity, capacity, undo/redo\n";
}
} // namespace boardStateTests

inline void runBoardStateTests(const juce::File& outputDirectory) {boardStateTests::run(outputDirectory);}

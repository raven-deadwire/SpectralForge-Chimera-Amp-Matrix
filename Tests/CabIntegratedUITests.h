#pragma once
#include "CabMicrophoneUITests.h"
#include "CabRoom.h"

namespace cabIntegratedUITests {
using namespace cabMicrophoneUITests;
inline void artworkResources() {
    using namespace spectralforge::cabArt;
    const juce::SharedResourcePointer<Bank> artwork;
    require(assetCount==27,"Scenery changed the released equipment artwork enumeration");
    size_t retainedBytes=0;
    for(size_t i=0;i<assetCount;++i) {
        const auto& image=artwork->images[i];
        if(!image.isValid())throw std::runtime_error("Missing CAB artwork resource: "+std::string(keys[i]));
        require(image.hasAlphaChannel(),"CAB product cutouts lost alpha transparency");
        retainedBytes+=size_t(image.getWidth())*size_t(image.getHeight())*4;
    }
    const juce::SharedResourcePointer<spectralforge::cabRoom::ArtworkBank> room;
    require(room->room.isValid() && room->room.getWidth()>0 && room->room.getHeight()>0
        && room->room.getWidth()<=1152 && room->room.getHeight()<=576,
        "CAB room scenery is missing, undecodable or retained above its display budget");
    retainedBytes+=size_t(room->room.getWidth())*size_t(room->room.getHeight())*4;
    for(const auto& image:artwork->frontSpeakers)retainedBytes+=size_t(image.getWidth())*size_t(image.getHeight())*4;
    require(artwork->hornMouth.isValid() && artwork->hornMouth.hasAlphaChannel(),
        "HF mouth cache is missing or has lost its transparent flange boundary");
    retainedBytes+=size_t(artwork->hornMouth.getWidth())*size_t(artwork->hornMouth.getHeight())*4;
    for(const auto& skin:artwork->enclosureSkins) {
        for(const auto& image:{skin.grille,skin.roof,skin.grilleOverlay})
            retainedBytes+=size_t(image.getWidth())*size_t(image.getHeight())*4;
        for(size_t i=1;i<skin.perimeter.size();i+=2)
            retainedBytes+=size_t(skin.perimeter[i].getWidth())*size_t(skin.perimeter[i].getHeight())*4;
    }
    require(retainedBytes<=16*1024*1024,"CAB equipment and room artwork exceed the combined editor RGBA budget");
    std::vector<Asset> matched;
    for(const auto& model:spectralforge::micCatalog::models) {
        const auto asset=capturedMicrophone(&model);
        require(asset!=Asset::count && juce::String(key(asset))=="mic-"+juce::String(model.id),"Microphone artwork no longer follows stable catalog identity");
        require(std::find(matched.begin(),matched.end(),asset)==matched.end(),"Distinct catalog microphones share an unrelated image");
        matched.push_back(asset);
    }
    require(capturedMicrophone(nullptr)==Asset::count,"Unknown/mixed capture was assigned a microphone model");
    require(originalMicrophone(2)!=capturedMicrophone(spectralforge::micCatalog::byId("chimera-strike")),"Detail response uses Chimera Strike artwork");
    std::cout<<"PASS CAB artwork: 27 embedded transparent assets, stable catalog mapping and separate room scenery, combined retained RGBA bytes="<<retainedBytes<<'\n';
}

inline void layout(CabPanel& panel) {
    std::function<void(juce::Component&)> inspect=[&](juce::Component& parent) {
        for(auto* child:parent.getChildren())if(child->isVisible()) {
            if(child->getBounds().isEmpty() || !parent.getLocalBounds().contains(child->getBoundsInParent()))
                throw std::runtime_error("CAB artwork/control outside its visible panel: "+child->getComponentID().toStdString());
            if(child->getComponentID().startsWith("originalCabControls") || child->getComponentID().startsWith("cabScene"))
                inspect(*child);
        }
    };
    inspect(panel);
    if(panel.getView()==CabPanel::View::irLoader)for(const auto* slot:{"A","B"}) {
        const auto prefix="cab"+juce::String(slot);
        auto* status=findComponent(panel,prefix+"status1");
        auto* high=findComponent(panel,slot[0]=='A' ? "cabhigh1" : "cabBhigh1");
        require(status && high && status->isVisible() && high->isVisible()
            && !panel.getLocalArea(status,status->getLocalBounds()).intersects(panel.getLocalArea(high,high->getLocalBounds())),
            "CAB capture status overlaps the last filter control or is hidden in IR LOADER");
    }
}

inline void run(const juce::File& folder,const juce::File& screenshots) {
    using Instrument=spectralforge::IRMetadata::Instrument;
    using Asset=spectralforge::cabArt::Asset;
    using Image=spectralforge::cabArt::View;
    const auto capture=folder.getChildFile("Chimera_Strike.wav");
    writeFixture(capture,"Chimera Strike","Synthetic integration cabinet",Instrument::guitar,"12",13);
    juce::MemoryBlock originalBytes;require(capture.loadFileAsData(originalBytes),"Read integration fixture");
    auto processor=std::make_unique<ChimeraProcessor>();
    auto& state=processor->parameters();
    struct Frozen {const char* suffix;float low,high,step,initial;};
    const std::array<Frozen,13> frozen{{{"design",0,1,1,0},{"rear",0,1,1,0},{"tweeter",0,1,.01f,0},
        {"Aon",0,1,1,0},{"Amic",0,2,1,0},{"Aunit",0,3,1,0},{"Aposition",0,1,.001f,.25f},{"Adistance",2,60,.1f,10},
        {"Bon",0,1,1,0},{"Bmic",0,2,1,0},{"Bunit",0,3,1,0},{"Bposition",0,1,.001f,.25f},{"Bdistance",2,60,.1f,10}}};
    require(processor->getParameters().size()==4824+spectralforge::cabExpansionParameterCount+spectralforge::cabLayoutParameterCount+spectralforge::graphicalEQParameterCount,"Integrated host parameter count changed");
    for(int lane=0;lane<3;++lane)for(size_t n=0;n<frozen.size();++n) {
        const auto& expected=frozen[n];auto* parameter=state.getParameter(spectralforge::originalCabID(lane,expected.suffix));
        require(parameter && parameter->getParameterIndex()==4785+lane*13+int(n) && parameter->getVersionHint()==7
            && parameter->isAutomatable(),"Original parameter ID, ordinal or AU hint changed");
        const auto& range=parameter->getNormalisableRange();
        require(range.start==expected.low && range.end==expected.high && range.interval==expected.step,
            "Original automation range or quantization changed");
        require(std::abs(parameter->convertFrom0to1(parameter->getDefaultValue())-expected.initial)<1e-5,
            "Original automation default changed");
    }
    const auto raw=[&](const juce::String& id){return state.getRawParameterValue(id)->load();};
    const auto automate=[&](const juce::String& id,float value) {
        auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    require(processor->loadMicIR(0,0,capture).wasOk() && processor->loadMicIR(0,1,capture).wasOk(),"Load catalog fixtures");
    CabPanel panel(*processor,0,{folder},folder.getChildFile("library.json"));
    artworkResources();
    auto* controls=panel.findChildWithID("originalCabControls1");
    require(controls!=nullptr,"Unified panel missing original controls");
    auto& design=component<juce::ComboBox>(*controls,"ocab1_design");
    auto& rear=component<juce::ComboBox>(*controls,"ocab1_rear");
    auto& tweeter=component<juce::Slider>(*controls,"ocab1_tweeter");
    require(!design.isEnabled() && !rear.isEnabled() && !tweeter.isEnabled(),"Captured IR must not expose active geometry controls");
    for(const auto* slot:{"A","B"}) {
        const auto prefix="ocab1_"+juce::String(slot);
        auto& enabled=component<juce::ToggleButton>(*controls,(prefix+"on").toRawUTF8());
        auto& mic=component<juce::ComboBox>(*controls,(prefix+"mic").toRawUTF8());
        auto& unit=component<juce::ComboBox>(*controls,(prefix+"unit").toRawUTF8());
        auto& position=component<juce::Slider>(*controls,(prefix+"position").toRawUTF8());
        auto& distance=component<juce::Slider>(*controls,(prefix+"distance").toRawUTF8());
        require(position.getTextFromValue(.637)=="63.7%"
            && std::abs(position.getValueFromText(position.getTextFromValue(.637))-.637)<1e-6,
            "Position text entry loses the host parameter's 0.001 resolution");
        require(mic.getNumItems()==3,"Catalog identities must not silently become DSP roles");
        for(int i=0;i<mic.getNumItems();++i)require(!mic.getItemText(i).contains("Strike"),"Detail condenser is not Strike");
        enabled.setToggleState(true,juce::sendNotificationSync);
        require(raw(prefix+"on")==1,"Model enable UI not host-bound");
        require(dispatchUntil([&]{return mic.isEnabled() && unit.isEnabled() && position.isEnabled() && distance.isEnabled();}),"Enabled model controls remain unavailable");
        mic.setSelectedId(slot[0]=='A' ? 3 : 2,juce::sendNotificationSync);
        unit.setSelectedId(slot[0]=='A' ? 2 : 4,juce::sendNotificationSync);
        position.setValue(slot[0]=='A' ? .63 : .12,juce::sendNotificationSync);
        distance.setValue(slot[0]=='A' ? 27.4 : 8.2,juce::sendNotificationSync);
        require(raw(prefix+"mic")==mic.getSelectedId()-1 && raw(prefix+"unit")==unit.getSelectedId()-1
            && std::abs(raw(prefix+"position")-position.getValue())<.0011
            && std::abs(raw(prefix+"distance")-distance.getValue())<.11,"Mic geometry UI did not update host parameters");
    }
    require(design.isEnabled() && rear.isEnabled() && tweeter.isEnabled(),"Enabled shared original controls remain unavailable");
    auto& cabinetImage=component<Image>(*controls,"ocab1_cabinetImage");
    auto& speakerImage=component<Image>(*controls,"ocab1_speakerImage");
    auto& micImageA=component<Image>(*controls,"ocab1_Aimage");
    auto& micImageB=component<Image>(*controls,"ocab1_Bimage");
    require(dispatchUntil([&]{return cabinetImage.asset()==Asset::guitarCabinet && speakerImage.asset()==Asset::guitarSpeaker
        && micImageA.asset()==Asset::detailCondenser && micImageB.asset()==Asset::bodyRibbon;}),"Guitar cabinet/unit or independent original microphone artwork is stale");
    require(cabinetImage.hasImage() && speakerImage.hasImage() && micImageA.hasImage() && micImageB.hasImage(),"Original CAB images were not decoded before drawing");
    require(dispatchUntil([&]{return component<juce::Label>(panel,"cabAtitle1").getText().contains("CABINET MIC")
        && component<juce::Label>(panel,"cabBtitle1").getText().contains("CABINET MIC");}),
        "Original microphone artwork and captured-slot status did not converge before the screenshot");
    layout(panel);snapshot(panel,screenshots,"cab-visual-guitar.png");
    design.setSelectedId(2,juce::sendNotificationSync);rear.setSelectedId(2,juce::sendNotificationSync);tweeter.setValue(.42,juce::sendNotificationSync);
    require(raw("ocab1_design")==1 && raw("ocab1_rear")==1 && std::abs(raw("ocab1_tweeter")-.42f)<.011f,"Enclosure controls not host-bound");
    require(dispatchUntil([&]{return cabinetImage.asset()==Asset::bassCabinet && speakerImage.asset()==Asset::bassSpeaker;}),"Bass cabinet selection retained guitar cabinet/unit artwork");
    layout(panel);snapshot(panel,screenshots,"cab-visual-bass.png");
    processor->prepareToPlay(48000,128);
    require(processor->micMetadata(0,0).values[3].contains("Detail"),"Original response metadata not active");
    require(processor->micCaptureMetadata(0,0).values[3]=="Chimera Strike","Modeled metadata overwrote retained capture identity");
    require(dispatchUntil([&]{return component<juce::Label>(panel,"cabAtitle1").getText().contains("CABINET MIC");}),"Host model change not displayed");
    require(component<juce::ComboBox>(panel,"cabAmic1").getText().contains("Chimera Strike")
        && component<juce::Label>(panel,"cabAprovenance1").getText().contains("Stored capture"),"Stored Strike capture presented as the active model");
    require(component<juce::ComboBox>(panel,"cabAmic1").getTooltip().contains("independent original response"),"Strike capture tooltip must distinguish its separate original response");
    // Host automation must update the real controls without a user gesture.
    automate("ocab1_Amic",0);automate("ocab1_Adistance",51.2f);
    require(dispatchUntil([&]{return component<juce::ComboBox>(*controls,"ocab1_Amic").getSelectedId()==1
        && std::abs(component<juce::Slider>(*controls,"ocab1_Adistance").getValue()-51.2)<.11;}),"Host automation did not refresh model UI");
    require(dispatchUntil([&]{return micImageA.asset()==Asset::attackDynamic && micImageB.asset()==Asset::bodyRibbon;}),"Mic A artwork automation changed Mic B or retained the old microphone");
    require(processor->loadMicIR(0,1,folder.getChildFile("missing.wav")).failed() && raw("ocab1_Bon")==1,"Rejected import changed active model");
    require(processor->loadMicIR(0,1,capture).wasOk(),"Clear failed import with valid capture");automate("ocab1_Bon",1);
    processor->releaseResources();
    // Selecting either a factory capture or a personal capture switches only
    // that slot, even when the engine still reports its previous model request.
    panel.setView(CabPanel::View::irLoader);
    auto& cabinet=component<juce::ComboBox>(panel,"cabAcabinet1");
    auto& microphone=component<juce::ComboBox>(panel,"cabAmic1");
    cabinet.setSelectedId(itemContaining(cabinet,"Factory V30"),juce::sendNotificationSync);
    microphone.setSelectedId(itemContaining(microphone,"Dynamic 57"),juce::sendNotificationSync);
    require(raw("ocab1_Aon")==0 && raw("ocab1_Bon")==1 && raw("cabtype1")==1,"Factory selection changed another slot or source mapping");
    require(component<juce::Label>(panel,"cabAreference1").getText()=="Shure SM57","Stale model metadata leaked into factory selection");
    require(component<Image>(panel,"cabAmicImage1").asset()==Asset::dynamic57,"Factory capture retained the original microphone illustration");
    require(component<juce::Label>(panel,"cabAstatus1").getText().contains("Captured IR selected"),
        "Stopped playback must not report the old original model as the selected capture");
    cabinet.setSelectedId(itemContaining(cabinet,"Synthetic integration cabinet"),juce::sendNotificationSync);
    microphone.setSelectedId(itemContaining(microphone,"Chimera Strike"),juce::sendNotificationSync);
    require(raw("ocab1_Aon")==0 && raw("ocab1_Bon")==1 && raw("cabtype1")==3,"Personal capture selection lost mixed-mode independence");
    require(component<juce::Label>(panel,"cabAtitle1").getText().contains("CAPTURED IR"),"Captured source not visibly identified");
    require(component<juce::Label>(panel,"cabAreference1").getText().contains("CABINET MIC"),"Strike response status must be visible without a tooltip");
    require(component<Image>(panel,"cabAmicImage1").asset()==Asset::strike,"Chimera Strike capture artwork was not independently selected");
    require(component<juce::Slider>(panel,"cabBhigh1").getTextFromValue(9000)=="9000", "Frequency readout must fit its text box");
    automate("cabblend1",.5f);processor->prepareToPlay(48000,128);
    require(dispatchUntil([&]{return !component<juce::ComboBox>(*controls,"ocab1_Amic").isEnabled()
        && component<juce::ComboBox>(*controls,"ocab1_Bmic").isEnabled()
        && component<juce::Label>(panel,"cabBstatus1").getText().contains("Ready");}),"Mixed panel readiness or control availability incorrect");
    snapshot(panel,screenshots,"cab-integrated-panel.png");
    processor->releaseResources();
    juce::MemoryBlock saved;require(processor->tryGetStateInformation(saved),"Integrated state save failed");
    auto tree=juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize())));
    require(tree.getChildWithName("IR_ASSETS").getNumChildren()==1,"Modeled slot duplicated or dropped retained personal IR");
    require(spectralforge::irState::unpack(tree),"Resolve integrated state IR table");
    for(const auto& ir:tree.getChildWithName("USER_IRS")) {
        juce::MemoryBlock stored;require(stored.fromBase64Encoding(ir.getProperty("data").toString()) && stored==originalBytes,"Model switching mutated personal IR bytes");
    }
    require(capture.deleteFile(),"Delete integration source before restore");
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    CabPanel reopened(*restored,0,{folder},folder.getChildFile("library.json"));
    reopened.setView(CabPanel::View::irLoader);
    require(component<juce::Label>(reopened,"cabAtitle1").getText().contains("CAPTURED IR")
        && component<juce::Label>(reopened,"cabBtitle1").getText().contains("CABINET MIC"),"Mixed source identities lost on project reopen");
    require(component<juce::ComboBox>(reopened,"cabAmic1").getText().contains("Chimera Strike"),"Deleted-source catalog identity lost on project reopen");
    for(const auto& parameter:state.copyState()) {
        const auto id=parameter.getProperty("id").toString();
        if(id.startsWith("ocab"))require(restored->parameters().getRawParameterValue(id)->load()==raw(id),"Original automation value changed on restore");
    }
    require(component<Image>(reopened,"cabAmicImage1").asset()==Asset::strike,"Captured image identity lost after deleting source files and reopening");
    {
        const auto dynamic=folder.getChildFile("Dynamic_SM57.wav"),mixed=folder.getChildFile("Mixed.wav");
        writeFixture(dynamic,"Shure SM57","Synthetic 4x12",Instrument::guitar,"12",3);
        writeFixture(mixed,"Shure SM57 + Royer R-121","Synthetic 4x12",Instrument::guitar,"12",17);
        auto captured=std::make_unique<ChimeraProcessor>();
        require(captured->loadMicIR(0,0,dynamic).wasOk() && captured->loadMicIR(0,1,mixed).wasOk(),"Visual capture fixture load failed");
        CabPanel captures(*captured,0,{folder},folder.getChildFile("library.json"));
        captures.setView(CabPanel::View::irLoader);
        require(component<Image>(captures,"cabAmicImage1").asset()==Asset::dynamic57
            && component<Image>(captures,"cabBmicImage1").asset()==Asset::count,"Mixed capture falsely displays a single microphone model");
        layout(captures);snapshot(captures,screenshots,"cab-visual-captured.png");
        captures.setSize(900,748);layout(captures);snapshot(captures,screenshots,"cab-visual-compact.png");
    }
    std::cout<<"PASS integrated CAB UI: capture/model provenance, all geometry bindings, host automation, independent transitions, Strike disclosure and deleted-source restoration\n";
}
}

#pragma once
#include "CabPanel.h"
#include "IRStateAssets.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace cabMicrophoneUITests {
inline void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

inline juce::Component* findComponent(juce::Component& parent,const juce::String& id) {
    juce::Component* result=parent.getComponentID()==id ? &parent : nullptr;
    for(auto* child:parent.getChildren())if(auto* found=findComponent(*child,id)) {
        if(result)throw std::runtime_error("Duplicate CAB component identity: "+id.toStdString());
        result=found;
    }
    return result;
}

template<class T> T& component(juce::Component& parent,const char* id) {
    auto* result=dynamic_cast<T*>(findComponent(parent,id));
    if(!result)throw std::runtime_error("Missing microphone UI component: "+std::string(id));
    return *result;
}

inline int itemContaining(const juce::ComboBox& menu,const juce::String& text,
                          const juce::String& capture={}) {
    for(int i=0;i<menu.getNumItems();++i)
        if(menu.getItemText(i).contains(text) && (capture.isEmpty() || menu.getItemText(i).contains(capture)))
            return menu.getItemId(i);
    throw std::runtime_error("Missing microphone menu choice: "+text.toStdString()+" / "+capture.toStdString());
}

template<class Predicate> bool dispatchUntil(Predicate ready,int timeoutMs=2000) {
    auto* messages=juce::MessageManager::getInstance();
    require(messages->isThisTheMessageThread(),"Microphone UI test must dispatch on the message thread");
    // JUCE decrements timer countdowns only on its timer thread. That thread
    // can be waiting for an older queued timer message when CAB registers its
    // timer, so sleeping and synchronously calling due timers once can miss
    // CAB entirely. Pump the real platform queue until the production update
    // arrives, with a deadline rather than an assumed callback time.
    const auto started=juce::Time::getMillisecondCounterHiRes();
    while(!ready()) {
        if(juce::Time::getMillisecondCounterHiRes()-started>=timeoutMs)return false;
        if(!messages->runDispatchLoopUntil(10))return false;
    }
    return true;
}

inline void snapshot(juce::Component& component,const juce::File& directory,const char* name) {
    if(directory==juce::File{})return;
    require(directory.createDirectory().wasOk(),"Cannot create synthetic microphone screenshot directory");
    auto image=component.createComponentSnapshot(component.getLocalBounds());
    require(image.isValid(),"Cannot capture synthetic microphone UI");
    auto stream=directory.getChildFile(name).createOutputStream();
    require(stream!=nullptr && stream->setPosition(0) && stream->truncate().wasOk(),
            "Cannot write synthetic microphone screenshot");
    juce::PNGImageFormat png;
    require(png.writeImageToStream(image,*stream),"Cannot encode synthetic microphone screenshot");
}

inline void writeFixture(const juce::File& file,const char* microphone,
                         const char* cabinet,spectralforge::IRMetadata::Instrument instrument,
                         const char* diameter,int offset) {
    require(file.getParentDirectory().createDirectory().wasOk(),"Cannot create microphone fixture directory");
    juce::WavAudioFormat format;
    auto stream=file.createOutputStream();
    require(stream!=nullptr,"Cannot create microphone fixture WAV");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.release(),48000,1,24,{},0));
    require(writer!=nullptr,"Cannot encode microphone fixture WAV");
    juce::AudioBuffer<float> audio(1,128);audio.clear();
    audio.setSample(0,offset,.4f);audio.setSample(0,offset+3,.12f);
    require(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()),"Cannot write microphone impulse fixture");
    writer.reset();
    spectralforge::IRMetadata metadata;metadata.instrument=instrument;
    metadata.values[1]=cabinet;metadata.values[2]=diameter;metadata.values[3]=microphone;
    metadata.values[11]="Synthetic impulse fixture. Microphone labels test selection and metadata, not measured microphone responses.";
    require(juce::File(file.getFullPathName()+".json").replaceWithText(juce::JSON::toString(metadata.json())),
            "Cannot write microphone fixture metadata");
}

inline void sameNameReplacement(const juce::File& folder,const juce::File& screenshots) {
    using Instrument=spectralforge::IRMetadata::Instrument;
    const auto settings=folder.getChildFile("library.json");
    const auto a=folder.getChildFile("A.wav");
    const auto first=folder.getChildFile("first/capture.wav");
    const auto second=folder.getChildFile("second/capture.wav");
    writeFixture(a,"Shure SM57","Synthetic recall cabinet",Instrument::guitar,"12",0);
    writeFixture(first,"Audio-Technica AT4050","Synthetic recall cabinet",Instrument::guitar,"12",7);
    writeFixture(second,"Royer R-121","Synthetic recall cabinet",Instrument::guitar,"12",19);
    juce::MemoryBlock state;
    {
        auto processor=std::make_unique<ChimeraProcessor>();
        require(processor->loadMicIR(0,0,a).wasOk() && processor->loadMicIR(0,1,first).wasOk(),
                "Cannot load independent microphone metadata fixtures");
        CabPanel panel(*processor,0,{folder},settings);
        panel.setView(CabPanel::View::irLoader);
        auto& micA=component<juce::ComboBox>(panel,"cabAmic1");
        auto& micB=component<juce::ComboBox>(panel,"cabBmic1");
        auto& referenceA=component<juce::Label>(panel,"cabAreference1");
        auto& referenceB=component<juce::Label>(panel,"cabBreference1");
        require(micA.getText().contains("Dynamic 57") && referenceA.getText()=="Shure SM57",
                "Mic A catalog alias/reference missing");
        require(micB.getText().contains("Condenser 4050") && referenceB.getText()=="Audio-Technica AT4050",
                "Mic B condenser alias/reference missing");
        const auto revisionA=processor->micDisplayRevision(0,0);
        const auto revisionB=processor->micDisplayRevision(0,1);
        require(processor->loadMicIR(0,1,second).wasOk(),"Cannot replace Mic B with equal-basename capture");
        require(processor->micName(0,1)==first.getFileName()
                    && processor->parameters().getRawParameterValue("cabBtype1")->load()==3,
                "Equal-basename replacement changed stable source or name");
        require(processor->micDisplayRevision(0,1)[0]!=revisionB[0]
                    && processor->micDisplayRevision(0,0)[0]==revisionA[0],
                "Mic B metadata revision did not change independently");
        // Optional screenshots show the actual prepared synthetic captures,
        // rather than the decoder's pending status before audio preparation.
        if(screenshots!=juce::File{})processor->prepareToPlay(48000,128);
        const bool updated=dispatchUntil([&] {
            return micB.getText().contains("Ribbon 121") && referenceB.getText()=="Royer R-121";
        });
        if(!updated)std::cerr<<"Mic B display deadline: alias='"<<micB.getText()
            <<"', reference='"<<referenceB.getText()<<"', metadata='"<<processor->micMetadata(0,1).values[3]
            <<"', source="<<processor->parameters().getRawParameterValue("cabBtype1")->load()
            <<", revision="<<processor->micDisplayRevision(0,1)[0]<<'\n';
        require(micB.getText().contains("Ribbon 121") && referenceB.getText()=="Royer R-121",
                "Same-name Mic B replacement left stale alias/reference");
        require(micA.getText().contains("Dynamic 57") && referenceA.getText()=="Shure SM57",
                "Mic B metadata replacement changed Mic A display");
        require(processor->micMetadata(0,1).values[3]=="Royer R-121",
                "Equal-basename selection resolved another capture's metadata");
        snapshot(panel,screenshots,"cab-microphone-panel.png");
        if(screenshots!=juce::File{})processor->releaseResources();
        require(processor->tryGetStateInformation(state),"Cannot save microphone metadata fixture state");
    }
    require(a.deleteFile() && first.deleteFile() && second.deleteFile(),
            "Cannot remove external microphone fixture WAVs before recall");
    {
        auto restored=std::make_unique<ChimeraProcessor>();
        restored->setStateInformation(state.getData(),int(state.getSize()));
        CabPanel panel(*restored,0,{folder},settings);
        panel.setView(CabPanel::View::irLoader);
        require(restored->micName(0,1)=="capture.wav" && restored->micMetadata(0,1).values[3]=="Royer R-121",
                "Embedded Mic B metadata lost after deleting equal-basename sources");
        require(component<juce::ComboBox>(panel,"cabBmic1").getText().contains("Ribbon 121")
                    && component<juce::Label>(panel,"cabBreference1").getText()=="Royer R-121",
                "Reopened CAB panel inferred metadata from missing external files");
        require(component<juce::Label>(panel,"cabAreference1").getText()=="Shure SM57",
                "Embedded microphone metadata crossed A/B slots on recall");
    }
    require(!settings.existsAsFile(),"Microphone recall test wrote shared-library preferences");
    std::cout<<"PASS: same-basename Mic B replacement refreshes alias/reference independently; embedded metadata survives deleted source files\n";
}

inline void groupedSelectionAndFilter(const juce::File& folder,const juce::File& screenshots) {
    using Instrument=spectralforge::IRMetadata::Instrument;
    struct Capture {const char* file;const char* microphone;const char* alias;Instrument instrument;const char* diameter;};
    const std::array<Capture,5> captures{{
        {"dynamic.wav","Shure SM57","Dynamic 57",Instrument::guitar,"12"},
        {"condenser.wav","Audio-Technica AT4050","Condenser 4050",Instrument::bass,"10"},
        {"ribbon.wav","Royer R-121","Ribbon 121",Instrument::guitar,"12"},
        {"other.wav","Audix i5","",Instrument::unspecified,"15"},
        {"mixed.wav","Shure SM57 + Royer R-121","",Instrument::guitar,"12"}
    }};
    for(size_t i=0;i<captures.size();++i) {
        const auto& c=captures[i];
        writeFixture(folder.getChildFile(c.file),c.microphone,"Synthetic grouped cabinet",c.instrument,c.diameter,int(i)*7);
    }
    const auto settings=folder.getChildFile("library.json");
    {
        auto processor=std::make_unique<ChimeraProcessor>();
        CabPanel panel(*processor,0,{folder},settings);
        panel.setView(CabPanel::View::irLoader);
        // Browsing another cabinet clears its mic reference. Choosing the
        // already-active factory capture must restore it even if source=1 did
        // not change and no user-asset revision was generated.
        auto& cabinetA=component<juce::ComboBox>(panel,"cabAcabinet1");
        auto& microphoneA=component<juce::ComboBox>(panel,"cabAmic1");
        cabinetA.setSelectedId(itemContaining(cabinetA,"Synthetic grouped cabinet"),juce::sendNotificationSync);
        cabinetA.setSelectedId(itemContaining(cabinetA,"Factory V30"),juce::sendNotificationSync);
        microphoneA.setSelectedId(itemContaining(microphoneA,"Dynamic 57"),juce::sendNotificationSync);
        require(processor->parameters().getRawParameterValue("cabtype1")->load()==1
                    && component<juce::Label>(panel,"cabAreference1").getText()=="Shure SM57",
                "Reselecting active factory capture left its reference blank");
        auto& cabinet=component<juce::ComboBox>(panel,"cabBcabinet1");
        auto& microphone=component<juce::ComboBox>(panel,"cabBmic1");
        for(const auto& c:captures) {
            cabinet.setSelectedId(itemContaining(cabinet,"Synthetic grouped cabinet"),juce::sendNotificationSync);
            require(microphone.getNumItems()==int(captures.size()),
                    "Captured microphone menu lost imports or added unavailable models");
            microphone.setSelectedId(itemContaining(microphone,c.alias,c.file),juce::sendNotificationSync);
            require(processor->micName(0,1)==c.file && processor->micMetadata(0,1).values[3]==c.microphone,
                    "Grouped microphone selection loaded the wrong capture");
            require(processor->parameters().getRawParameterValue("cabBtype1")->load()==3,
                    "Grouped file selection changed the host source enumeration");
        }
    }
    {
        juce::File picked;int committed=0;
        IRBrowserPanel browser(folder,[&](juce::File file){picked=file;++committed;},settings);
        auto& microphones=component<juce::ComboBox>(browser,"irmicrophone");
        auto& reference=component<juce::Label>(browser,"irmicrophonereference");
        auto& list=component<juce::ListBox>(browser,"irlist");
        auto& load=component<juce::TextButton>(browser,"irload");
        auto& kind=component<juce::ComboBox>(browser,"irkind");
        auto& diameter=component<juce::ComboBox>(browser,"irdiameter");
        const auto rows=[&]{return list.getListBoxModel()->getNumRows();};
        require(rows()==int(captures.size()),"Microphone browser omitted valid synthetic captures");
        const int dynamic=itemContaining(microphones,"Dynamic 57");
        const int condenser=itemContaining(microphones,"Condenser 4050");
        const int ribbon=itemContaining(microphones,"Ribbon 121");
        const int strike=itemContaining(microphones,"Chimera Strike");
        microphones.setSelectedId(dynamic,juce::sendNotificationSync);
        require(rows()==1 && reference.getText()=="Shure SM57","Dynamic catalog filter/reference incorrect");
        list.selectRow(0);require(load.isEnabled(),"Ready dynamic capture cannot be loaded");load.onClick();
        require(committed==1 && picked==folder.getChildFile("dynamic.wav"),"Microphone filter loaded a different file");

        microphones.setSelectedId(condenser,juce::sendNotificationSync);
        require(rows()==1 && list.getSelectedRow()==-1 && !load.isEnabled(),
                "Changing microphone filter retained a stale actionable row");
        kind.setSelectedId(2,juce::sendNotificationSync);
        diameter.setSelectedId(4,juce::sendNotificationSync);
        require(rows()==0 && microphones.getSelectedId()==condenser && kind.getSelectedId()==2,
                "Diameter filtering reset or ignored the microphone/instrument filter");
        diameter.setSelectedId(3,juce::sendNotificationSync);
        require(rows()==1,"Microphone, bass and 10-inch filters did not combine");
        kind.setSelectedId(3,juce::sendNotificationSync);
        require(rows()==0 && microphones.getSelectedId()==condenser && diameter.getSelectedId()==3,
                "Instrument filtering reset or ignored the microphone/diameter filter");
        kind.setSelectedId(1,juce::sendNotificationSync);diameter.setSelectedId(1,juce::sendNotificationSync);
        require(rows()==1,"Clearing other filters did not retain the selected condenser");
        list.selectRow(0);load.onClick();
        require(committed==2 && picked==folder.getChildFile("condenser.wav"),"Condenser filter loaded a different file");
        snapshot(browser,screenshots,"cab-microphone-library.png");
        microphones.setSelectedId(ribbon,juce::sendNotificationSync);
        require(rows()==1,"Ribbon filter incorrectly included a prepared microphone blend");
        microphones.setSelectedId(2,juce::sendNotificationSync);
        require(rows()==2,"Other/mixed filter lost non-roster or prepared-mix captures");

        microphones.setSelectedId(dynamic,juce::sendNotificationSync);list.selectRow(0);
        require(load.isEnabled(),"Fixture selection was not restored before empty-filter check");
        microphones.setSelectedId(strike,juce::sendNotificationSync);
        require(rows()==0 && list.getSelectedRow()==-1 && !load.isEnabled(),
                "Uncaptured Chimera Strike exposed a ghost or stale load selection");
        require(reference.getText()=="Chimera original / Condenser",
                "Chimera Strike lost its original condenser identity");
        require(component<juce::Label>(browser,"irmicrophonesupport").getText().contains("independent original response"),
                "Strike filter must distinguish captured IR from the separately selectable original response");
        require(component<juce::Label>(browser,"irstatus").getText().containsIgnoreCase("import"),
                "Empty model filter did not explain capture import");
        load.onClick();require(committed==2,"Empty microphone filter loaded the previous capture");
        microphones.setSelectedId(1,juce::sendNotificationSync);
        require(rows()==int(captures.size()),"Catalog filtering changed the underlying IR collection");
    }
    require(!settings.existsAsFile(),"Microphone selection/filter test wrote library preferences");
    std::cout<<"PASS: categorized capture selections resolve real files; other/mixed imports remain usable; combined filters and empty Strike selection cannot load stale audio\n";
}

inline void duplicateLabels(const juce::File& folder) {
    using Instrument=spectralforge::IRMetadata::Instrument;
    const std::array<juce::File,2> files{{folder.getChildFile("first/capture.wav"),folder.getChildFile("second/capture.wav")}};
    std::array<juce::MemoryBlock,2> originals;
    for(size_t i=0;i<files.size();++i) {
        writeFixture(files[i],"Shure SM57","Synthetic duplicate cabinet",Instrument::guitar,"12",int(i)*17);
        require(files[i].loadFileAsData(originals[i]),"Cannot read duplicate-capture fixture");
    }
    require(originals[0]!=originals[1],"Duplicate-name fixtures must have distinct audio payloads");
    auto processor=std::make_unique<ChimeraProcessor>();
    CabPanel panel(*processor,0,{folder},folder.getChildFile("library.json"));
    panel.setView(CabPanel::View::irLoader);
    auto& cabinet=component<juce::ComboBox>(panel,"cabBcabinet1");
    auto& microphone=component<juce::ComboBox>(panel,"cabBmic1");
    std::array<bool,2> selected{};
    for(int i=0;i<2;++i) {
        cabinet.setSelectedId(itemContaining(cabinet,"Synthetic duplicate cabinet"),juce::sendNotificationSync);
        require(microphone.getNumItems()==2 && microphone.getItemText(0)!=microphone.getItemText(1),
                "Equal microphone/capture names are ambiguous in the CAB menu");
        const auto label=microphone.getItemText(i);
        require(label.contains("Dynamic 57") && label.contains("capture.wav")
                    && label.contains("["+juce::String(i+1)+"]") && !label.contains(folder.getFullPathName()),
                "Duplicate capture labels need distinct ordinals and complete filenames without paths");
        microphone.setSelectedId(microphone.getItemId(i),juce::sendNotificationSync);
        juce::MemoryBlock state;require(processor->tryGetStateInformation(state),"Cannot serialize selected duplicate capture");
        auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
        require(xml!=nullptr,"Cannot decode duplicate-capture state");
        auto tree=juce::ValueTree::fromXml(*xml);
        require(spectralforge::irState::unpack(tree),"Cannot expand duplicate-capture asset reference");
        juce::MemoryBlock loaded;
        for(const auto& ir:tree.getChildWithName("USER_IRS"))
            if(int(ir.getProperty("lane",-1))==0 && int(ir.getProperty("slot",0))==1)
                require(loaded.fromBase64Encoding(ir.getProperty("data").toString()),"Selected duplicate payload missing");
        bool found=false;
        for(size_t j=0;j<originals.size();++j)if(loaded==originals[j]) {
            require(!selected[j],"Distinct duplicate menu items loaded the same capture");
            selected[j]=true;found=true;
        }
        require(found,"Duplicate capture choice loaded unrelated audio");
    }
    require(selected[0] && selected[1],"Duplicate-name captures are not independently selectable");
    std::cout<<"PASS: duplicate microphone/filename labels have private-path-free ordinals and load distinct original audio payloads\n";
}

inline void run(const juce::File& folder,const juce::File& screenshots={}) {
    sameNameReplacement(folder.getChildFile("same-name"),screenshots);
    groupedSelectionAndFilter(folder.getChildFile("grouped"),screenshots);
    duplicateLabels(folder.getChildFile("duplicates"));
}
}

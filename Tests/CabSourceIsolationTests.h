#pragma once

// Included after the production render/equality helpers in CabPanelStateTests.
// Exercise the actual panel actions and audio path with non-neutral saved
// filters; a view change must never reset those values to make the UI look off.
inline void cabSourceIsolationContracts(const juce::File& folder,const juce::File& screenshots) {
    using namespace cabMicrophoneUITests;
    using cabSceneUITests::navigationUnchanged;
    using cabSceneUITests::parameterValues;
    using cabSceneUITests::unchangedExcept;
    using cabSceneUITests::click;
    require(folder.createDirectory().wasOk(),"Cannot create CAB source isolation fixture");
    const auto capture=folder.getChildFile("Independent_capture.wav");
    writeFixture(capture,"Shure SM57","Independent capture cabinet",spectralforge::IRMetadata::Instrument::guitar,"12",11);
    auto processor=std::make_unique<ChimeraProcessor>();
    auto& state=processor->parameters();
    const auto raw=[&](const juce::String& id){return state.getRawParameterValue(id)->load();};
    for(const auto* id:{"gateon","transposeon","tuneron","boardEnabled","doubleron","metronome","oversampling"})set(*processor,id,0);
    for(const auto& spec:spectralforge::fxSpecs)if(spec.toggle)set(*processor,spec.id,0);
    for(int section=0;section<3;++section)set(*processor,spectralforge::postNativeModeID(section),0);
    set(*processor,"lowampmix",1);
    const std::array<int,3> retainedSources{{0,1,3}};
    for(int lane=0;lane<3;++lane) {
        const auto n=juce::String(lane+1);
        set(*processor,"mode",float(lane));
        for(int active=0;active<3;++active) {
            const auto a=juce::String(active+1);
            set(*processor,"ampon"+a,0);set(*processor,"cab"+a,1);
            set(*processor,"solo"+a,active==lane ? 1.f : 0.f);
        }
        require(processor->loadMicIR(lane,0,capture).wasOk() && processor->loadMicIR(lane,1,capture).wasOk(),
            "Cannot load independent stored capture fixtures");
        set(*processor,"cabtype"+n,float(retainedSources[size_t(lane)]));
        set(*processor,"cablow"+n,137);set(*processor,"cabhigh"+n,6430);
        set(*processor,"cabBlow"+n,91);set(*processor,"cabBhigh"+n,8120);
        set(*processor,"cabAgain"+n,-2.3f);set(*processor,"cabBgain"+n,-4.7f);
        set(*processor,"cabAdelay"+n,.37f);set(*processor,"cabBdelay"+n,.81f);
        set(*processor,"cabAinvert"+n,1);set(*processor,"cabblend"+n,.38f);
        CabPanel panel(*processor,lane,{folder},folder.getChildFile("library.json"));
        cabSceneUITests::Showing showing(panel);
        const auto aOn=spectralforge::originalCabID(lane,"Aon");
        const auto bOn=spectralforge::originalCabID(lane,"Bon");
        const auto source="cabtype"+n;
        const auto enabled=[&](const char* suffix)->juce::ToggleButton& {
            return component<juce::ToggleButton>(panel,spectralforge::originalCabID(lane,suffix).toRawUTF8());
        };
        const auto slotControls=[&](int slot,bool expected) {
            const auto prefix=juce::String(slot ? "cabB" : "cabA");
            for(const auto& id:std::array<juce::String,5>{{prefix+"gain"+n,prefix+"delay"+n,
                    prefix+"invert"+n,(slot ? "cabBlow" : "cablow")+n,(slot ? "cabBhigh" : "cabhigh")+n}}) {
                auto& control=component<juce::Component>(panel,id.toRawUTF8());
                require(control.isVisible() && control.isEnabled()==expected,
                    "IR LOADER exposes modeled-only processing controls or disables the captured slot");
            }
        };
        const auto blendEnabled=[&](bool expected) {
            require(component<juce::Slider>(panel,("cabblend"+n).toRawUTF8()).isEnabled()==expected,
                "IR LOADER shared blend does not reflect its active capture paths");
        };
        const auto beforeModel=parameterValues(*processor);
        click(enabled("Aon"),[&]{return raw(aOn)==1;});
        unchangedExcept(*processor,beforeModel,{aOn});
        // Select an actual modeled enclosure after enabling the mic. This is
        // the reported user path; it must not activate a stored filter source.
        auto& design=component<juce::ComboBox>(panel,spectralforge::originalCabID(lane,"design").toRawUTF8());
        require(dispatchUntil([&]{return design.isEnabled();}),"Modeled cabinet selector is unavailable");
        const auto beforeCabinet=parameterValues(*processor);
        design.setSelectedId(2,juce::sendNotificationSync);
        unchangedExcept(*processor,beforeCabinet,{spectralforge::originalCabID(lane,"design"),spectralforge::cabExpansionID(lane,"driver")});
        require(raw(aOn)==1 && raw(source)==retainedSources[size_t(lane)] && raw(bOn)==0,
            "Modeled cabinet selection changed a retained capture or the other microphone");
        slotControls(0,true);slotControls(1,true);blendEnabled(true);
        const auto audioBefore=render(*processor);
        navigationUnchanged(*processor,[&] {
            click(component<juce::Button>(panel,"cabViewIRLoader"),[&]{return panel.getView()==CabPanel::View::irLoader;});
            require(dispatchUntil([&]{return !component<juce::Slider>(panel,("cablow"+n).toRawUTF8()).isEnabled();}),
                "Modeled Mic A still presents an active filter in IR LOADER");
            slotControls(0,false);slotControls(1,true);blendEnabled(true);
            const auto prefix="cabA";
            auto& stored=component<juce::ComboBox>(panel,(juce::String(prefix)+"cabinet"+n).toRawUTF8());
            require(stored.isEnabled() && stored.getText().startsWith("Stored / ")
                && component<juce::Label>(panel,(juce::String(prefix)+"status"+n).toRawUTF8()).getText().contains("IR inactive"),
                "Stored filter/capture is presented as simultaneously active with the modeled cabinet");
            require(!component<juce::Label>(panel,("cabBstatus"+n).toRawUTF8()).getText().contains("IR inactive"),
                "Active captured Mic B is presented as inactive");
            if(lane==0)snapshot(panel,screenshots,"cab-ir-isolation-stored-filters.png");
            click(component<juce::Button>(panel,"cabViewCabinet"),[&]{return panel.getView()==CabPanel::View::cabinet;});
            slotControls(0,true);slotControls(1,true);blendEnabled(true);
        });
        equal(audioBefore,render(*processor));
        // Turning the model off restores the exact previous capture and cuts.
        const auto beforeRestore=parameterValues(*processor);
        click(enabled("Aon"),[&]{return raw(aOn)==0;});
        unchangedExcept(*processor,beforeRestore,{aOn});
        navigationUnchanged(*processor,[&] {
            panel.setView(CabPanel::View::irLoader);slotControls(0,true);slotControls(1,true);
            require(raw(source)==retainedSources[size_t(lane)]
                && !component<juce::ComboBox>(panel,("cabAcabinet"+n).toRawUTF8()).getText().startsWith("Stored / "),
                "Returning to captured IR did not restore its exact retained source");
            panel.setView(CabPanel::View::cabinet);
        });
        click(enabled("Aon"),[&]{return raw(aOn)==1;});
        click(enabled("Bon"),[&]{return raw(bOn)==1;});
        navigationUnchanged(*processor,[&] {
            panel.setView(CabPanel::View::irLoader);
            slotControls(0,false);slotControls(1,false);blendEnabled(false);
            panel.setView(CabPanel::View::cabinet);
            slotControls(0,true);slotControls(1,true);blendEnabled(true);
            panel.setView(CabPanel::View::irLoader);
        });
        // Merely browsing another cabinet must not switch the active model.
        auto& cabinet=component<juce::ComboBox>(panel,("cabAcabinet"+n).toRawUTF8());
        auto& microphone=component<juce::ComboBox>(panel,("cabAmic"+n).toRawUTF8());
        navigationUnchanged(*processor,[&] {
            cabinet.setSelectedId(itemContaining(cabinet,"Factory V30"),juce::sendNotificationSync);
        });
        const auto beforeCapture=parameterValues(*processor);
        microphone.setSelectedId(itemContaining(microphone,"Dynamic 57"),juce::sendNotificationSync);
        unchangedExcept(*processor,beforeCapture,{aOn,source});
        require(raw(aOn)==0 && raw(source)==1 && raw(bOn)==1,"Deliberate capture selection did not switch only Mic A");
        slotControls(0,true);slotControls(1,false);blendEnabled(true);
        require(component<juce::ComboBox>(panel,("cabAmic"+n).toRawUTF8()).getText().contains("Dynamic 57"),
            "Deliberate factory capture retained a stale model label");
        cabinet.setSelectedId(itemContaining(cabinet,"Independent capture cabinet"),juce::sendNotificationSync);
        const int personalID=itemContaining(microphone,"Dynamic 57",capture.getFileName());
        microphone.setSelectedId(personalID,juce::sendNotificationSync);
        require(raw(source)==3 && raw(aOn)==0 && raw(bOn)==1,"Personal capture did not preserve the mixed path");
        panel.setView(CabPanel::View::cabinet);
        click(enabled("Aon"),[&]{return raw(aOn)==1;});
        panel.setView(CabPanel::View::irLoader);
        require(microphone.getText().startsWith("Stored / ") && microphone.getSelectedId()==0,
            "Retained personal capture appears selected while the modeled microphone is active");
        const auto beforeReselection=parameterValues(*processor);
        microphone.setSelectedId(personalID,juce::sendNotificationSync);
        unchangedExcept(*processor,beforeReselection,{aOn});
        require(raw(aOn)==0 && raw(bOn)==1 && raw(source)==3,
            "Reselecting the same retained personal IR failed to return only that mic to capture mode");
        slotControls(0,true);slotControls(1,false);blendEnabled(true);
    }
    // The main rig selector opens a different library callback from CabPanel.
    // A factory choice there must perform the same single-slot source switch.
    set(*processor,"mode",0);set(*processor,"ocab1_Aon",1);
    {
        auto editor=std::make_unique<ChimeraEditor>(*processor);cabSceneUITests::Showing showing(*editor);
        auto& canvas=component<juce::Component>(*editor,"surface");
        const auto libraryWindow=[]()->juce::DialogWindow* {
            for(int index=0;index<juce::TopLevelWindow::getNumTopLevelWindows();++index)
                if(auto* window=dynamic_cast<juce::DialogWindow*>(juce::TopLevelWindow::getTopLevelWindow(index)))
                    if(dynamic_cast<IRBrowserPanel*>(window->getContentComponent()))return window;
            return nullptr;
        };
        navigationUnchanged(*processor,[&] {
            click(component<juce::TextButton>(canvas,"cabRigControls"),[&]{return component<juce::Component>(canvas,"ampNativePanel1").isVisible();});
            auto& cabinet=component<CabinetSelector>(canvas,"cabinet1");
            require(cabinet.isVisible() && cabinet.isEnabled(),"Main cabinet selector is unavailable");
            cabinet.setSelectedId(9000,juce::sendNotificationSync);
            require(dispatchUntil([&]{return libraryWindow()!=nullptr;}),"Main rig selector did not open the IR library");
        });
        juce::Component::SafePointer<juce::DialogWindow> window(libraryWindow());
        auto* browser=dynamic_cast<IRBrowserPanel*>(window->getContentComponent());
        require(browser!=nullptr,"Main rig source selector opened an unrelated panel");
        auto& availability=component<juce::ComboBox>(*browser,"iravailability");
        auto& search=component<juce::TextEditor>(*browser,"irsearch");
        auto& list=component<juce::ListBox>(*browser,"irlist");
        navigationUnchanged(*processor,[&] {
            availability.setSelectedId(3,juce::sendNotificationSync);
            search.setText("Celestion V30",false);search.onTextChange();
            require(list.getListBoxModel()->getNumRows()==1,"Factory library fixture did not isolate its expected V30 capture");
            list.selectRow(0);
        });
        const auto beforeFactory=parameterValues(*processor);
        click(component<juce::TextButton>(*browser,"irload"),[&]{return raw("ocab1_Aon")==0 && raw("cabtype1")==1;});
        unchangedExcept(*processor,beforeFactory,{"ocab1_Aon","cabtype1"});
        require(raw("ocab1_Bon")==1,"Main library factory selection disabled the independent modeled Mic B");
        require(dispatchUntil([&]{return window==nullptr;}),"Factory library selection retained a live source dialog");
    }
    juce::MemoryBlock saved;require(processor->tryGetStateInformation(saved),"Cannot save isolated capture/model project");
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    require(parameterValues(*restored)==parameterValues(*processor),"Project recall changed isolated cabinet/capture parameters");
    for(int lane=0;lane<3;++lane) {
        CabPanel reopened(*restored,lane,{folder},folder.getChildFile("library.json"));
        reopened.setView(CabPanel::View::irLoader);const auto n=juce::String(lane+1);
        require(component<juce::Slider>(reopened,("cablow"+n).toRawUTF8()).isEnabled()
            && !component<juce::Slider>(reopened,("cabBlow"+n).toRawUTF8()).isEnabled(),
            "Project recall lost the independent active capture and modeled filter states");
        require(restored->micName(lane,0)==capture.getFileName() && restored->micName(lane,1)==capture.getFileName(),
            "Changing cabinet/model views discarded stored personal IR identities");
    }
    std::cout<<"PASS CAB source isolation: 3 lanes, retained filter/factory/personal sources, independent A/B controls, cabinet-to-IR navigation with exact state/host and audio invariance, retained-IR reselection, main-library factory switch and project recall\n";
}

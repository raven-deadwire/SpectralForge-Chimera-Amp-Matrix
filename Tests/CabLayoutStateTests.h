#pragma once
void layoutStateContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    auto p=std::make_unique<ChimeraProcessor>();
    const auto raw=[&](const juce::String& id){return p->parameters().getRawParameterValue(id)->load();};
    const auto delta=[](const auto& a,const auto& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(double(a[n]-b[n])));return d;};
    check(p->getParameters().size()==4845+spectralforge::graphicalEQParameterCount,"v3 append-only parameter count");
    for(int lane=0;lane<3;++lane) {
        const std::array<const char*,4> old{"driver","Amic","Bmic","tweeter"};const std::array<int,4> ends{14,20,20,3};
        for(int i=0;i<4;++i) {
            auto* q=p->parameters().getParameter(cabExpansionID(lane,old[size_t(i)]));
            check(q->getParameterIndex()==4824+lane*4+i && q->getVersionHint()==8 && q->getDefaultValue()==0 && q->isAutomatable(),"v2 ordinal/default/hint");
            check(q->convertFrom0to1(0)==0 && q->convertFrom0to1(1)==float(ends[size_t(i)]) && q->getNumSteps()==ends[size_t(i)]+1,"v2 normalized mapping changed");
        }
        const std::array<const char*,3> added{"layout","Aunit","Bunit"};
        for(int i=0;i<3;++i) {
            auto* q=p->parameters().getParameter(cabLayoutID(lane,added[size_t(i)]));
            check(q->getParameterIndex()==4836+lane*3+i && q->getVersionHint()==9 && q->getDefaultValue()==0 && q->isAutomatable(),"v3 host contract");
            check(q->getNumSteps()==(i ? 8 : 10),"v3 fixed choice count");
            for(int n=0;n<q->getNumSteps();++n)check(std::abs(q->convertFrom0to1(q->convertTo0to1(float(n)))-float(n))<1e-5,"v3 host mapping roundtrip");
        }
        set(*p,"ampon"+juce::String(lane+1),0);set(*p,originalCabID(lane,"Aon"),1);set(*p,originalCabID(lane,"Bon"),1);
        set(*p,cabExpansionID(lane,"driver"),9);set(*p,cabExpansionID(lane,"Amic"),20);set(*p,cabExpansionID(lane,"Bmic"),10);
        set(*p,originalCabID(lane,"Aposition"),.63f);set(*p,"cabblend"+juce::String(lane+1),.4f);
        set(*p,cabLayoutID(lane,"Bunit"),7);
    }
    set(*p,"gateon",0);set(*p,"oversampling",0);set(*p,"lowampmix",1);set(*p,"mode",0);
    const auto legacy=render(*p);std::vector<std::vector<float>> audio;
    for(int layout=1;layout<=9;++layout) {
        set(*p,"lcab1_layout",float(layout));audio.push_back(render(*p));check(delta(legacy,audio.back())>1e-5,"array selection has no production effect");
        for(size_t n=0;n+1<audio.size();++n)check(delta(audio[n],audio.back())>1e-5,"different array templates duplicate production audio");
    }
    for(int lane=0;lane<3;++lane)set(*p,cabLayoutID(lane,"layout"),float(7-lane));
    set(*p,"cabblend1",0);auto first=render(*p);set(*p,"lcab1_Aunit",1);check(delta(first,render(*p))>1e-5,"A unit index ignored");
    set(*p,"cabblend1",1);auto last=render(*p);set(*p,"lcab1_Bunit",4);check(delta(last,render(*p))>1e-5,"B unit index ignored");set(*p,"cabblend1",.4f);
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<(mode==1 ? 2 : 1);++dual) {
        set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));const auto a=render(*p);
        juce::MemoryBlock data;check(p->tryGetStateInformation(data),"array state serialization");auto recall=std::make_unique<ChimeraProcessor>();
        recall->setStateInformation(data.getData(),int(data.getSize()));equal(a,render(*recall));
        for(int lane=0;lane<3;++lane)for(const char* suffix:{"layout","Aunit","Bunit"}) {
            const auto id=cabLayoutID(lane,suffix);check(raw(id)==recall->parameters().getRawParameterValue(id)->load(),"array state recall");
        }
    }
    // A changed LOW array has no contribution at the actual DI endpoint.
    set(*p,"ampon1",1);set(*p,"mode",2);set(*p,"lowampmix",0);const auto di=render(*p);set(*p,"lcab1_layout",1);equal(di,render(*p));
    set(*p,"lowampmix",1);const auto wet=render(*p);set(*p,"lcab1_layout",7);check(delta(wet,render(*p))>1e-5,"LOW amp+cab branch ignores layout");
    for(float mix:{0.f,.5f,1.f}) {
        set(*p,"lowampmix",mix);juce::MemoryBlock data;check(p->tryGetStateInformation(data),"LOW save");auto recall=std::make_unique<ChimeraProcessor>();
        recall->setStateInformation(data.getData(),int(data.getSize()));equal(render(*p),render(*recall));
    }
    set(*p,"ampon1",0);set(*p,"mode",0);set(*p,"lowampmix",1);const int other=1-p->comparisonSlot();p->copyComparison();const auto before=render(*p);
    set(*p,"lcab1_layout",1);set(*p,"lcab1_Aunit",0);p->selectComparison(other);equal(before,render(*p));
    // Captured A / modeled B survives source deletion, project and comparison recall.
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("layout-ir-fixture",".wav",false);
    fixture(file,7);check(p->loadMicIR(0,0,file).wasOk() && file.deleteFile(),"array IR import");
    check(raw("ocab1_Aon")==0 && raw("ocab1_Bon")==1,"IR import changed wrong mic");
    const auto mixed=render(*p);juce::MemoryBlock saved;check(p->tryGetStateInformation(saved),"mixed save");auto recall=std::make_unique<ChimeraProcessor>();
    recall->setStateInformation(saved.getData(),int(saved.getSize()));equal(mixed,render(*recall));
    const int otherMix=1-p->comparisonSlot();p->copyComparison();set(*p,"ocab1_Aon",1);p->selectComparison(otherMix);equal(mixed,render(*p));
    set(*p,"ocab1_Aon",1);
    // Remove the new bank from a v2 snapshot, then restore into dirty v3 controls.
    for(int lane=0;lane<3;++lane)set(*p,cabLayoutID(lane,"layout"),0);
    const auto v2=render(*p);auto tree=p->parameters().copyState();
    std::function<void(juce::ValueTree)> strip=[&](juce::ValueTree node){for(int n=node.getNumChildren();--n>=0;) {
        const auto child=node.getChild(n);if(child.getProperty("id").toString().startsWith("lcab"))node.removeChild(n,nullptr);else strip(child);
    }};strip(tree);juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*tree.createXml(),old);
    for(int lane=0;lane<3;++lane)for(const char* suffix:{"layout","Aunit","Bunit"})set(*p,cabLayoutID(lane,suffix),7);
    p->setStateInformation(old.getData(),int(old.getSize()));equal(v2,render(*p));
    for(int lane=0;lane<3;++lane)for(const char* suffix:{"layout","Aunit","Bunit"})check(raw(cabLayoutID(lane,suffix))==0,"legacy load inherited dirty v3 selector");
    CabPanel panel(*p,0);panel.setView(CabPanel::View::cabinet);auto& scene=cabMicrophoneUITests::component<CabScene>(panel,"cabScene1");
    auto& layout=cabMicrophoneUITests::component<juce::ComboBox>(panel,"lcab1_layout");auto& unitA=cabMicrophoneUITests::component<juce::ComboBox>(panel,"lcab1_Aunit");
    auto& controls=cabMicrophoneUITests::component<OriginalCabControls>(panel,"originalCabControls1");
    auto& driverChoice=cabMicrophoneUITests::component<juce::ComboBox>(panel,"xcab1_driver");
    auto& family=cabMicrophoneUITests::component<juce::ComboBox>(panel,"ocab1_design");
    check(layout.getNumItems()==9,"cabinet menu exposes the internal four-unit entry as a duplicate");
    for(int item=0;item<layout.getNumItems();++item) {
        check(layout.getItemId(item)==item+2,"unique cabinet menu remaps a released host ordinal");
        check(!layout.getItemText(item).contains("8x10"),"unshipped 8x10 remains in the current cabinet menu");
        for(int previous=0;previous<item;++previous)
            check(layout.getItemText(item)!=layout.getItemText(previous),"cabinet menu contains duplicate type labels");
    }
    check(layout.getItemText(6)=="Bass 6x10","6x10 replacement changed its existing layout ordinal");
    // An explicit cabinet choice loads a fitting unit and notifies the host.
    // Restored mismatches render the safe effective choice without rewriting
    // project state or acquiring automation gestures during a refresh.
    {
        cabSceneUITests::HostEvents events(*p);
        for(int selected=1;selected<=9;++selected) {
            const auto& cabinet=cabLayout::layouts[size_t(selected-1)];
            const int incompatible=cabinet.bass ? 1 : 13;
            set(*p,"lcab1_layout",selected==1 ? 2.f : 1.f);set(*p,"xcab1_driver",float(incompatible));controls.refreshState();events.reset();
            layout.setSelectedId(selected+1,juce::sendNotificationSync);
            events.expect({"lcab1_layout","xcab1_driver"});
            cabLayout::Settings actual{};actual.layout=selected;actual.voice.driver=juce::roundToInt(raw("xcab1_driver"));
            actual.voice.base.cabinet=juce::roundToInt(raw("ocab1_design"));
            check(cabLayout::isDriverCompatible(actual),"cabinet selection loads an incompatible speaker");
            check(driverChoice.getSelectedId()==actual.voice.driver+1 && layout.getText()==cabinet.name,"cabinet selection label or speaker mismatch");
            for(int candidate=0;candidate<=14;++candidate) {
                auto request=actual;request.voice.driver=candidate;
                check(driverChoice.isItemEnabled(candidate+1)==cabLayout::isDriverCompatible(request),"incompatible speaker menu item enabled");
            }
            events.reset();driverChoice.setSelectedId(incompatible+1,juce::sendNotificationSync);
            check(raw("xcab1_driver")==actual.voice.driver && driverChoice.getSelectedId()==actual.voice.driver+1,"disabled speaker selection entered cabinet");events.expect();
            set(*p,"xcab1_driver",float(incompatible));const auto before=cabSceneUITests::parameterValues(*p);events.reset();controls.refreshState();
            check(driverChoice.getSelectedId()==actual.voice.driver+1 && raw("xcab1_driver")==incompatible,"restored mismatch not displayed safely");
            cabSceneUITests::unchangedExcept(*p,before,{});events.expect();
        }
        set(*p,"lcab1_layout",1);set(*p,"xcab1_driver",6);controls.refreshState();events.reset();
        layout.setSelectedId(4,juce::sendNotificationSync);events.expect({"lcab1_layout"});
        check(raw("xcab1_driver")==6 && driverChoice.getText()=="Crimson 12","compatible speaker lost on cabinet change");
        set(*p,"lcab1_layout",0);set(*p,"ocab1_design",1);set(*p,"xcab1_driver",13);controls.refreshState();events.reset();
        check(layout.getText()=="Bass 4x10" && driverChoice.getSelectedId()==10 && raw("xcab1_driver")==13,"four-unit cabinet did not resolve bass diameter");
        for(int candidate=1;candidate<=14;++candidate)
            check(driverChoice.isItemEnabled(candidate+1)==(candidate==9 || candidate==10 || candidate==12),"four-unit bass cabinet admits another diameter or family");
        driverChoice.setSelectedId(2,juce::sendNotificationSync);events.expect();
        check(raw("xcab1_driver")==13 && driverChoice.getSelectedId()==10,"speaker choice silently changed cabinet family");
        // Family is a separate explicit choice on the four-unit cabinet.
        family.setSelectedId(1,juce::sendNotificationSync);events.expect({"ocab1_design","xcab1_driver"});
        check(raw("xcab1_driver")==0 && layout.getSelectedId()==4 && raw("lcab1_layout")==0
            && layout.getText()=="Guitar 4x12","explicit guitar family did not preserve the four-unit state under its unique label");
        events.reset();family.setSelectedId(2,juce::sendNotificationSync);events.expect({"ocab1_design"});
        check(raw("ocab1_design")==1 && layout.getSelectedId()==7 && raw("lcab1_layout")==0
            && layout.getText()=="Bass 4x10" && driverChoice.getText()=="Chimera Bass 10","explicit bass family did not preserve the four-unit state under its unique label");
        for(int bass=0;bass<2;++bass) {
            set(*p,"lcab1_layout",0);set(*p,"ocab1_design",float(bass));set(*p,"xcab1_driver",0);controls.refreshState();
            const auto audioBefore=render(*p);events.reset();
            cabSceneUITests::navigationUnchanged(*p,[&] {
                controls.refreshState();
                check(layout.getSelectedId()==(bass ? 7 : 4) && raw("lcab1_layout")==0,
                    "four-unit display migration rewrites the saved layout ordinal");
                panel.setView(CabPanel::View::irLoader);panel.setView(CabPanel::View::cabinet);controls.refreshState();
            });
            events.expect();equal(audioBefore,render(*p));
            const auto before=cabSceneUITests::parameterValues(*p);events.reset();
            layout.setSelectedId(2,juce::sendNotificationSync);
            if(bass)events.expect({"lcab1_layout","xcab1_driver"});else events.expect({"lcab1_layout"});
            cabSceneUITests::unchangedExcept(*p,before,bass
                ? std::initializer_list<juce::String>{"lcab1_layout","xcab1_driver"}
                : std::initializer_list<juce::String>{"lcab1_layout"});
            check(raw("lcab1_layout")==1,"explicit visible cabinet choice does not reach its existing ordinal");
        }
        for(int explicitLayout:{3,6}) {
            set(*p,"lcab1_layout",float(explicitLayout));controls.refreshState();events.reset();
            const auto before=cabSceneUITests::parameterValues(*p);controls.refreshState();
            check(layout.getSelectedId()==explicitLayout+1 && raw("lcab1_layout")==explicitLayout,
                "explicit four-speaker template is confused with saved layout zero");
            cabSceneUITests::unchangedExcept(*p,before,{});events.expect();
        }
    }
    const std::array<int,9> driver{1,1,1,13,9,9,9,11,14};
    for(int selected=1;selected<=9;++selected) {
        set(*p,"xcab1_driver",float(driver[size_t(selected-1)]));layout.setSelectedId(selected+1,juce::sendNotificationSync);
        check(raw("lcab1_layout")==selected && scene.layoutSelection()==selected,"layout host binding");
        const auto model=scene.arrayGeometry();check(scene.speakerCount()==cabLayout::count(selected),"room array count diverges");
        for(int unit=0;unit<model.count;++unit)for(int mic=1;mic<=20;++mic) {
            unitA.setSelectedId(unit+1,juce::sendNotificationSync);set(*p,"xcab1_Amic",float(mic));scene.refresh();
            check(raw("lcab1_Aunit")==unit && scene.micGeometry(0).unit==unit,"unit UI mapping");
            const auto c=scene.speakerCentre(unit);const auto face=scene.baffleBounds();const auto physical=model.centres[size_t(unit)];
            check(std::abs((c.x-face.getCentreX())/face.getWidth()-physical.x/model.box.width)<1e-6
                && std::abs((face.getCentreY()-c.y)/face.getHeight()-physical.y/model.box.height)<1e-6,"UI and DSP geometry diverge");
            for(float pos:{0.f,1.f})for(float distance:{2.f,60.f}) {
                set(*p,"ocab1_Aposition",pos);set(*p,"ocab1_Adistance",distance);scene.refresh();const auto visual=scene.micVisualGeometry(0);
                check(scene.getLocalBounds().toFloat().contains(visual.bodyBounds),"array mic clipped");
                check(visual.capsule.getDistanceFrom(scene.microphoneAnchor(0))<.01f,"array capsule detached");
            }
        }
        set(*p,"ocab1_Aposition",.3f);set(*p,"ocab1_Adistance",10);set(*p,"lcab1_Aunit",0);set(*p,"lcab1_Bunit",float(model.count-1));scene.refresh();
        cabIntegratedUITests::layout(panel);cabMicrophoneUITests::snapshot(panel,screenshots,("cab-visual-layout-"+juce::String(selected)+".png").toRawUTF8());
    }
    set(*p,"mode",2);CabRoomOverview room(*p);room.setBounds(0,0,1000,600);
    for(int selected=1;selected<=9;++selected) {
        for(int lane=0;lane<3;++lane) {set(*p,cabLayoutID(lane,"layout"),float(selected));set(*p,cabExpansionID(lane,"driver"),float(driver[size_t(selected-1)]));}
        room.refreshState();scene.refresh();
        for(int lane=0;lane<3;++lane) {
            const auto g=room.displayedGeometry(lane),focused=scene.arrayGeometry();
            check(g.count==focused.count && g.box.volume==focused.box.volume && g.radius==focused.radius,"room enclosure differs from focused view");
            for(int unit=0;unit<g.count;++unit)check(g.centres[size_t(unit)].x==focused.centres[size_t(unit)].x && g.centres[size_t(unit)].y==focused.centres[size_t(unit)].y,"room coordinates differ from DSP");
        }
    }
    set(*p,"lcab1_layout",7);set(*p,"xcab1_driver",9);set(*p,"lcab2_layout",2);set(*p,"xcab2_driver",1);set(*p,"lcab3_layout",4);set(*p,"xcab3_driver",13);
    room.refreshState();cabMicrophoneUITests::snapshot(room,screenshots,"cab-visual-layout-matrix-room.png");set(*p,"mode",0);
    // The preview's removed fourth row resolves to the last row in the same
    // column. Its eight-value automation bank stays byte-for-byte recallable.
    set(*p,"lcab1_Aunit",6);set(*p,"lcab1_Bunit",7);controls.refreshState();
    check(scene.speakerCount()==6 && scene.micGeometry(0).unit==4 && scene.micGeometry(1).unit==5,
        "6x10 preview-state migration does not preserve each microphone's speaker column");
    check(!unitA.isItemEnabled(7) && !unitA.isItemEnabled(8)
        && unitA.getItemText(6)=="Unit 7 (uses 5)" && unitA.getItemText(7)=="Unit 8 (uses 6)",
        "6x10 retained unit labels disagree with their effective audible targets");
    for(int unit=0;unit<8;++unit) {
        const auto id="ocab1_speakerImage"+(unit ? juce::String(unit+1) : juce::String());
        check(cabMicrophoneUITests::component<cabArt::View>(scene,id.toRawUTF8()).isVisible()==(unit<6),
            "6x10 still renders a seventh or eighth speaker from the preview");
    }
    const auto migrated=render(*p);juce::MemoryBlock previewState;
    check(p->tryGetStateInformation(previewState),"6x10 preview-unit state serialization");
    {
        auto previewRecall=std::make_unique<ChimeraProcessor>();
        previewRecall->setStateInformation(previewState.getData(),int(previewState.getSize()));
        check(previewRecall->parameters().getRawParameterValue("lcab1_Aunit")->load()==6
            && previewRecall->parameters().getRawParameterValue("lcab1_Bunit")->load()==7,
            "6x10 project recall rewrites the preview's serialized unit values");
        equal(migrated,render(*previewRecall));
    }
    set(*p,"lcab1_Aunit",4);set(*p,"lcab1_Bunit",5);equal(migrated,render(*p));
    set(*p,"lcab1_Aunit",6);set(*p,"lcab1_Bunit",7);
    {
        cabSceneUITests::HostEvents migrationEvents(*p);
        cabSceneUITests::navigationUnchanged(*p,[&] {controls.refreshState();
            panel.setView(CabPanel::View::irLoader);panel.setView(CabPanel::View::cabinet);});
        migrationEvents.expect();
    }
    // Saved out-of-range target is clamped at use time; automation is not rewritten.
    set(*p,"lcab1_layout",1);set(*p,"lcab1_Aunit",7);scene.refresh();check(scene.micGeometry(0).unit==0 && raw("lcab1_Aunit")==7,"small-array fallback rewrites automation");
    set(*p,"lcab1_layout",7);scene.refresh();check(scene.micGeometry(0).unit==5 && raw("lcab1_Aunit")==7,"6x10 fallback target or saved unit lost");
    panel.setView(CabPanel::View::irLoader);panel.setView(CabPanel::View::cabinet);check(raw("lcab1_layout")==7 && raw("lcab1_Aunit")==7,"IR navigation loses layout");
    std::cout<<"PASS nine layouts: production, routes, LOW DI, project/comparison/legacy/captured-IR recall, host and UI geometry\n";
#if JUCE_LINUX
    const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if(!display || display->userArea.isEmpty()) {
        const bool ci=juce::SystemStats::getEnvironmentVariable("CI",{}).equalsIgnoreCase("true")
            || juce::SystemStats::getEnvironmentVariable("GITHUB_ACTIONS",{}).equalsIgnoreCase("true");
        check(!ci,"CI layout gesture contracts require a primary X11 display; use run_linux_ui_tests.py");
        std::cout<<"SKIP native array gestures: local Linux has no primary X11 display; DSP/state/UI geometry completed, native CI coverage still required\n";
        return;
    }
#endif
    // Gesture guards require a showing native component, not only its visible
    // flag. Use the same real-peer fixture as the existing scene gesture tests.
    cabSceneUITests::Showing showing(panel);scene.refresh();cabSceneUITests::HostEvents events(*p);
    check(scene.beginMicDrag(0,scene.microphoneHitPoint(0)),"array drag start");
    scene.dragMicTo(scene.microphoneHitPoint(0)+scene.speakerCentre(4)-scene.speakerCentre(5));scene.endMicDrag();
    events.expect({"lcab1_Aunit","ocab1_Aposition"});check(raw("lcab1_Aunit")==4,"drag cannot address fifth unit");
    events.reset();check(scene.beginMicDrag(0,scene.microphoneHitPoint(0)),"array cancel start");set(*p,"lcab1_layout",1);scene.dragMicTo({0,0});
    check(!scene.isDragging(),"layout automation does not cancel drag");events.expect({"lcab1_Aunit","ocab1_Aposition"},1,false,{"lcab1_layout"});
    std::cout<<"PASS native array gestures: fifth-unit drag and host layout-change cancellation\n";
}

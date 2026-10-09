#pragma once
void layoutStateContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    auto p=std::make_unique<ChimeraProcessor>();
    const auto raw=[&](const juce::String& id){return p->parameters().getRawParameterValue(id)->load();};
    const auto delta=[](const auto& a,const auto& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(double(a[n]-b[n])));return d;};
    check(p->getParameters().size()==4845,"v3 append-only parameter count");
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
    // Saved out-of-range target is clamped at use time; automation is not rewritten.
    set(*p,"lcab1_layout",1);set(*p,"lcab1_Aunit",7);scene.refresh();check(scene.micGeometry(0).unit==0 && raw("lcab1_Aunit")==7,"small-array fallback rewrites automation");
    set(*p,"lcab1_layout",7);scene.refresh();check(scene.micGeometry(0).unit==7,"large-array selection lost");
    panel.setView(CabPanel::View::irLoader);panel.setView(CabPanel::View::cabinet);check(raw("lcab1_layout")==7 && raw("lcab1_Aunit")==7,"IR navigation loses layout");
    // Native scene gestures are also exercised in CI; no native peer is needed here.
    panel.setVisible(true);scene.refresh();cabSceneUITests::HostEvents events(*p);
    check(scene.beginMicDrag(0,scene.microphoneHitPoint(0)),"array drag start");
    scene.dragMicTo(scene.microphoneHitPoint(0)+scene.speakerCentre(4)-scene.speakerCentre(7));scene.endMicDrag();
    events.expect({"lcab1_Aunit","ocab1_Aposition"});check(raw("lcab1_Aunit")==4,"drag cannot address fifth unit");
    events.reset();check(scene.beginMicDrag(0,scene.microphoneHitPoint(0)),"array cancel start");set(*p,"lcab1_layout",1);scene.dragMicTo({0,0});
    check(!scene.isDragging(),"layout automation does not cancel drag");events.expect({"lcab1_Aunit","ocab1_Aposition"},1,false,{"lcab1_layout"});
    std::cout<<"PASS nine layouts: production, routes, LOW DI, project/comparison/legacy/captured-IR recall, host and UI geometry\n";
}

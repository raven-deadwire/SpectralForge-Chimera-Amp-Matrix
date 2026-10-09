#pragma once
// Included after the production render/equality helpers in CabPanelStateTests.
void expansionStateContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    auto p=std::make_unique<ChimeraProcessor>();
    const auto selectDriver=[&](int lane,int driver) {
        const auto* selected=driver ? &cabExpansion::drivers[size_t(driver-1)] : nullptr;
        const bool bass=selected && selected->bass;const int inches=selected ? selected->inches : 12;
        const int layout=bass ? (inches==15 ? 4 : inches==12 ? 9 : 6) : 3;
        set(*p,originalCabID(lane,"design"),bass ? 1.f : 0.f);
        set(*p,cabLayoutID(lane,"layout"),float(layout));
        set(*p,cabExpansionID(lane,"driver"),float(driver));
    };
    set(*p,"gateon",0);set(*p,"oversampling",0);set(*p,"lowampmix",1);set(*p,"mode",0);
    for(int lane=0;lane<3;++lane) {
        set(*p,"ampon"+juce::String(lane+1),0);set(*p,originalCabID(lane,"Aon"),1);
        selectDriver(lane,lane+1);set(*p,cabExpansionID(lane,"Amic"),1);
    }
    const auto changed=[](const auto& a,const auto& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(double(a[n]-b[n])));return d;};
    const auto anchor=render(*p);std::vector<std::vector<float>> driverAudio{anchor};
    for(int d=2;d<=14;++d) {
        selectDriver(0,d);const auto audio=render(*p);
        for(const auto& previous:driverAudio)check(changed(previous,audio)>1e-5,"compatible expanded speakers produce duplicate production audio");
        driverAudio.push_back(audio);
    }
    selectDriver(0,1);
    for(int m=2;m<=20;++m) {
        set(*p,"xcab1_Amic",float(m));check(changed(anchor,render(*p))>1e-5,"expanded microphone selector has no production audio effect");
    }
    for(int lane=0;lane<3;++lane) {
        selectDriver(lane,14-lane);set(*p,cabExpansionID(lane,"Amic"),float(20-lane));
        set(*p,cabExpansionID(lane,"Bmic"),float(10+lane));set(*p,cabExpansionID(lane,"tweeter"),float(lane+1));
        set(*p,originalCabID(lane,"Bon"),1);set(*p,cabLayoutID(lane,"Bunit"),lane==1 ? 0.f : lane==0 ? 1.f : 3.f);set(*p,originalCabID(lane,"tweeter"),.5f);
        set(*p,"cabblend"+juce::String(lane+1),.4f);
    }
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<(mode==1 ? 2 : 1);++dual) {
        set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));const auto a=render(*p);
        juce::MemoryBlock saved;check(p->tryGetStateInformation(saved),"expanded project serialization");
        auto recalled=std::make_unique<ChimeraProcessor>();recalled->setStateInformation(saved.getData(),int(saved.getSize()));equal(a,render(*recalled));
        for(int lane=0;lane<3;++lane)for(const auto* suffix:{"driver","Amic","Bmic","tweeter"}) {
            const auto id=cabExpansionID(lane,suffix);
            check(p->parameters().getRawParameterValue(id)->load()==recalled->parameters().getRawParameterValue(id)->load(),"expanded parameter recall");
        }
    }
    set(*p,"mode",0);const int other=1-p->comparisonSlot();p->copyComparison();const auto before=render(*p);
    selectDriver(0,6);set(*p,"xcab1_Amic",8);p->selectComparison(other);equal(before,render(*p));
    // Remove only the appended bank from a real v1 project. Dirty v2 choices
    // must not survive a legacy load, including the active comparison state.
    for(int lane=0;lane<3;++lane) {
        set(*p,cabLayoutID(lane,"layout"),0);
        for(const auto* suffix:{"driver","Amic","Bmic","tweeter"})set(*p,cabExpansionID(lane,suffix),0);
    }
    const auto legacyAudio=render(*p);auto tree=p->parameters().copyState();
    for(int n=tree.getNumChildren();--n>=0;)if(tree.getChild(n).getProperty("id").toString().startsWith("xcab"))tree.removeChild(n,nullptr);
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*tree.createXml(),legacy);
    for(int lane=0;lane<3;++lane) {set(*p,cabExpansionID(lane,"driver"),14);set(*p,cabExpansionID(lane,"Amic"),20);set(*p,cabExpansionID(lane,"Bmic"),19);set(*p,cabExpansionID(lane,"tweeter"),3);}
    p->setStateInformation(legacy.getData(),int(legacy.getSize()));equal(legacyAudio,render(*p));
    for(int lane=0;lane<3;++lane)for(const auto* suffix:{"driver","Amic","Bmic","tweeter"})check(p->parameters().getRawParameterValue(cabExpansionID(lane,suffix))->load()==0,"missing expanded controls inherit dirty state");
    CabPanel panel(*p,0);panel.setView(CabPanel::View::cabinet);cabIntegratedUITests::layout(panel);
    auto& drivers=cabMicrophoneUITests::component<juce::ComboBox>(panel,"xcab1_driver");
    auto& micA=cabMicrophoneUITests::component<juce::ComboBox>(panel,"xcab1_Amic");
    auto& micB=cabMicrophoneUITests::component<juce::ComboBox>(panel,"xcab1_Bmic");
    check(drivers.getNumItems()==15 && micA.getNumItems()==21 && micB.getNumItems()==21,"expanded product UI inventory");
    auto& scene=cabMicrophoneUITests::component<CabScene>(panel,"cabScene1");
    for(int driver:{0,9,13}) {
        selectDriver(0,driver);scene.refresh();
        for(int model=1;model<=20;++model)for(int unit=0;unit<scene.speakerCount();++unit)for(float position:{0.f,1.f})for(float distance:{2.f,60.f}) {
            set(*p,"xcab1_Amic",float(model));set(*p,"lcab1_Aunit",float(unit));set(*p,"ocab1_Aposition",position);set(*p,"ocab1_Adistance",distance);scene.refresh();
            check(scene.arraySettings().voice.driver==driver && scene.micGeometry(0).unit==unit,"matching cabinet replaced expanded driver or pickup target");
            const auto visual=scene.micVisualGeometry(0);
            check(visual.capsule.getDistanceFrom(scene.microphoneAnchor(0))<.01f,"expanded artwork capsule detached from pickup point");
            check(scene.getLocalBounds().toFloat().contains(visual.bodyBounds),"expanded microphone body clipped at a control boundary");
            auto& image=cabMicrophoneUITests::component<cabArt::View>(scene,"ocab1_Aimage");
            check(image.asset()==cabArt::capturedMicrophone(micCatalog::byId(cabExpansion::microphones[size_t(model-1)].catalogId)),"expanded scene retains another mic's artwork");
            const auto capsuleLocal=visual.capsule-juce::Point<float>(float(image.getX()),float(image.getY()));
            check(image.isArtworkPoint(capsuleLocal),"rotated capsule no longer hits its visible artwork");
        }
    }
    auto& layout=cabMicrophoneUITests::component<juce::ComboBox>(panel,"lcab1_layout");
    layout.setSelectedId(2,juce::sendNotificationSync); // Guitar 1x12 accepts Nocturne 100.
    set(*p,"lcab1_Aunit",0);set(*p,"ocab1_Aposition",.25f);set(*p,"ocab1_Adistance",10);
    drivers.setSelectedId(8,juce::sendNotificationSync);micA.setSelectedId(21,juce::sendNotificationSync);micB.setSelectedId(11,juce::sendNotificationSync);
    check(p->parameters().getRawParameterValue("xcab1_driver")->load()==7 && p->parameters().getRawParameterValue("xcab1_Amic")->load()==20
        && p->parameters().getRawParameterValue("xcab1_Bmic")->load()==10,"expanded UI host bindings");
    const auto sound=render(*p);check(changed(legacyAudio,sound)>1e-5,"expanded UI selection fails to change production sound");
    check(!cabMicrophoneUITests::component<juce::ComboBox>(panel,"ocab1_Amic").isEnabled(),"legacy mic must not pretend to control selected v2 response");
    cabMicrophoneUITests::snapshot(panel,screenshots,"cab-visual-expanded-raven-strike.png");
    set(*p,"lcab1_layout",8);set(*p,"lcab1_Aunit",0);set(*p,"xcab1_driver",14);set(*p,"xcab1_Amic",5);
    check(cabMicrophoneUITests::dispatchUntil([&]{return drivers.getSelectedId()==15 && micA.getSelectedId()==6
        && scene.layoutSelection()==8 && scene.arraySettings().voice.driver==14 && scene.micGeometry(0).expansion==5;}),"expanded host automation does not refresh UI");
    cabMicrophoneUITests::snapshot(panel,screenshots,"cab-visual-expanded-depth-anchor.png");
    panel.setView(CabPanel::View::irLoader);panel.setView(CabPanel::View::cabinet);
    check(p->parameters().getRawParameterValue("xcab1_driver")->load()==14 && p->parameters().getRawParameterValue("xcab1_Amic")->load()==5,"IR navigation loses expansion selection");
    std::cout<<"PASS expanded 14 drivers/20 microphones: production audio, all routes, independent A/B, recall, legacy and UI\n";
}

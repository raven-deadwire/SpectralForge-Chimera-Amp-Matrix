#pragma once
#include <set>

namespace cabDriverVisualTests {
inline void writeImage(const juce::Image& image,const juce::File& folder,const char* name) {
    if(folder==juce::File{})return;
    check(folder.createDirectory().wasOk(),"driver visual screenshot directory");
    auto stream=folder.getChildFile(name).createOutputStream();
    check(stream!=nullptr && stream->setPosition(0) && stream->truncate().wasOk(),"driver visual screenshot stream");
    juce::PNGImageFormat png;check(png.writeImageToStream(image,*stream),"driver visual screenshot encoding");
}
inline double greyDifference(const juce::Image& a,const juce::Image& b) {
    check(a.getBounds()==b.getBounds(),"comparable driver image sizes");
    double sum=0;
    for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x) {
        const auto grey=[](juce::Colour c){return (.2126*c.getRed()+.7152*c.getGreen()+.0722*c.getBlue())*c.getFloatAlpha();};
        sum+=std::abs(grey(a.getPixelAt(x,y))-grey(b.getPixelAt(x,y)));
    }
    return sum/(255.*a.getWidth()*a.getHeight());
}
inline void frontSilhouette(const juce::Image& image) {
    double weight=0,xSum=0,ySum=0,xx=0,yy=0,xy=0;
    for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x) {
        const double a=image.getPixelAt(x,y).getFloatAlpha();
        weight+=a;xSum+=a*x;ySum+=a*y;xx+=a*x*x;yy+=a*y*y;xy+=a*x*y;
    }
    check(weight>image.getWidth()*image.getHeight()*.65,"front driver has no complete opaque assembly");
    const double cx=xSum/weight,cy=ySum/weight;
    xx=xx/weight-cx*cx;yy=yy/weight-cy*cy;xy=xy/weight-cx*cy;
    const double spread=std::sqrt((xx-yy)*(xx-yy)+4*xy*xy),trace=xx+yy;
    check(std::sqrt((trace+spread)/(trace-spread))<1.015,"driver silhouette is oblique instead of circular/front-facing");
    check(std::abs(cx-(image.getWidth()-1)*.5)<.75 && std::abs(cy-(image.getHeight()-1)*.5)<.75,
        "front driver axis is displaced from its circular assembly");
}
}

// Product regressions: compare rendered outputs and independent nominal sizes,
// not the particular camera/painter implementation that happens to produce them.
void driverVisualContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    using namespace cabMicrophoneUITests;
    using namespace cabDriverVisualTests;
    // A fresh Dual/Matrix room has no focused panel owning its image bank.
    // Observe without creating, and drop observation handles before repainting.
    {
        using SharedBank=juce::SharedResourcePointer<cabArt::Bank>;
        const auto previous=SharedBank::getSharedObjectWithoutCreating();
        {
            auto roomProcessor=std::make_unique<ChimeraProcessor>();set(*roomProcessor,"mode",2);
            for(int lane=0;lane<3;++lane)set(*roomProcessor,originalCabID(lane,"Aon"),1);
            CabRoomOverview roomOnly(*roomProcessor);roomOnly.setSize(1040,620);roomOnly.refreshState();
            const cabArt::Bank* retainedAddress=nullptr;
            {
                const auto retained=SharedBank::getSharedObjectWithoutCreating();
                check(retained.has_value(),"room-only entry has no equipment cache before its first paint");
                retainedAddress=&retained->get();
                if(previous)check(retainedAddress==&previous->get(),"room entry replaced an existing shared equipment cache");
            }
            const auto state=cabSceneUITests::parameterValues(*roomProcessor);
            for(int frame=0;frame<2;++frame) {
                check(roomOnly.createComponentSnapshot(roomOnly.getLocalBounds()).isValid(),"room-only snapshot");
                const auto retained=SharedBank::getSharedObjectWithoutCreating();
                check(retained && &retained->get()==retainedAddress,"room repaint releases or recreates its equipment cache");
            }
            check(cabSceneUITests::parameterValues(*roomProcessor)==state,"room-only painting changes audio parameters");
        }
        const auto after=SharedBank::getSharedObjectWithoutCreating();
        check(previous ? after && &after->get()==&previous->get() : !after,
            "room teardown changed prior equipment cache ownership or leaked its image bank");
    }
    auto p=std::make_unique<ChimeraProcessor>();
    set(*p,"ocab1_Aon",1);set(*p,"ocab1_Bon",1);
    set(*p,"xcab1_Amic",1);set(*p,"xcab1_Bmic",20);
    set(*p,"ocab1_Aposition",.25f);set(*p,"ocab1_Bposition",.75f);
    set(*p,"ocab1_Adistance",10);set(*p,"ocab1_Bdistance",10);
    CabPanel panel(*p,0);panel.setView(CabPanel::View::cabinet);
    auto& scene=component<CabScene>(panel,"cabScene1");
    const std::array<int,3> drivers{{9,11,13}},inches{{10,12,15}};
    int dimensionCases=0,placementCases=0;
    for(int layout=0;layout<=9;++layout) {
        set(*p,"lcab1_layout",float(layout));
        float baseDiameter=0,baseCamera=0,baseHeadWidth=0;
        std::array<juce::Point<float>,2> baseMicSize{};
        for(size_t d=0;d<drivers.size();++d) {
            set(*p,"xcab1_driver",float(drivers[d]));scene.refresh();
            const float diameter=scene.speakerOuterDiameter();
            if(d==0) {baseDiameter=diameter;baseCamera=scene.pixelsPerMetre();baseHeadWidth=scene.amplifierBounds().getWidth();}
            check(std::abs(diameter/baseDiameter-float(inches[d])/10.f)<.0001f,
                "10/12/15-inch choices do not change the visible driver diameter in physical proportion");
            check(std::abs(scene.pixelsPerMetre()-baseCamera)<.0001f,"driver selection silently reframes the camera");
            check(std::abs(scene.amplifierBounds().getWidth()-baseHeadWidth)<.0001f,"changing speaker diameter resizes the amplifier head");
            for(int slot=0;slot<2;++slot) {
                const auto mic=scene.micVisualGeometry(slot);
                const juce::Point<float> size{mic.bodyBounds.getWidth(),mic.bodyBounds.getHeight()};
                if(d==0)baseMicSize[size_t(slot)]=size;
                check(size.getDistanceFrom(baseMicSize[size_t(slot)])<.01f,"changing speaker diameter resizes the same microphone");
                check(mic.capsule.getDistanceFrom(scene.microphoneAnchor(slot))<.01f,"driver resize detaches the microphone capsule");
            }
            for(int unit=0;unit<scene.speakerCount();++unit) {
                const auto id="ocab1_speakerImage"+(unit ? juce::String(unit+1) : juce::String());
                auto& view=component<cabArt::View>(scene,id.toRawUTF8());
                const auto bounds=scene.getLocalArea(&view,view.artworkBounds());
                check(std::abs(bounds.getWidth()-bounds.getHeight())<.01f && std::abs(bounds.getWidth()-diameter)<.01f,
                    "rendered speaker is distorted or ignores its nominal outer diameter");
                check(bounds.getCentre().getDistanceFrom(scene.speakerCentre(unit))<.01f,"driver front is detached from its acoustic axis");
                check(scene.baffleBounds().expanded(.01f).contains(bounds),"physical driver extends outside the front baffle");
            }
            ++dimensionCases;
        }
    }

    // Exercise both microphones at opposite boundary units/positions. This also
    // covers the longest and widest catalog bodies at the smallest camera scale.
    for(int layout=0;layout<=9;++layout)for(const int driver:drivers) {
        set(*p,"lcab1_layout",float(layout));set(*p,"xcab1_driver",float(driver));scene.refresh();
        const int last=scene.speakerCount()-1;
        for(int mic=1;mic<=20;++mic)for(int unit:{0,last})for(float position:{0.f,1.f})for(float distance:{2.f,60.f}) {
            set(*p,"xcab1_Amic",float(mic));set(*p,"xcab1_Bmic",float(mic));
            set(*p,layout ? "lcab1_Aunit" : "ocab1_Aunit",float(unit));
            set(*p,layout ? "lcab1_Bunit" : "ocab1_Bunit",float(last-unit));
            set(*p,"ocab1_Aposition",position);set(*p,"ocab1_Bposition",1.f-position);
            set(*p,"ocab1_Adistance",distance);set(*p,"ocab1_Bdistance",distance);scene.refresh();
            for(int slot=0;slot<2;++slot) {
                const auto visual=scene.micVisualGeometry(slot);
                if(!scene.getLocalBounds().toFloat().contains(visual.bodyBounds))
                    std::cerr<<"Clipped driver visual layout="<<layout<<" driver="<<driver<<" mic="<<mic
                        <<" slot="<<slot<<" unit="<<unit<<" position="<<position<<" distance="<<distance
                        <<" body="<<visual.bodyBounds.toString()<<" scene="<<scene.getLocalBounds().toString()<<'\n';
                check(scene.getLocalBounds().toFloat().contains(visual.bodyBounds),"fixed-camera microphone body clipped at a boundary placement");
                check(visual.capsule.getDistanceFrom(scene.microphoneAnchor(slot))<.01f,"boundary placement detaches microphone capsule");
                auto& image=component<cabArt::View>(scene,slot ? "ocab1_Bimage" : "ocab1_Aimage");
                const auto local=visual.capsule-juce::Point<float>(float(image.getX()),float(image.getY()));
                check(image.isArtworkPoint(local),"boundary capsule is no longer on the visible microphone");
                ++placementCases;
            }
        }
    }

    // Capture actual production View rendering at a common diameter: a reused
    // bass/guitar sprite cannot pass the pairwise pixel or unique-style checks.
    std::vector<juce::Image> designs;
    std::set<juce::String> styles;
    juce::Image contact(juce::Image::RGB,7*256,2*294,true);juce::Graphics cg(contact);
    cg.fillAll(juce::Colour(0xff181d1d));
    set(*p,"lcab1_layout",0);
    for(int driver=1;driver<=14;++driver) {
        set(*p,"xcab1_driver",float(driver));scene.refresh();
        auto& actual=component<cabArt::View>(scene,"ocab1_speakerImage");
        const auto style=actual.getProperties()["cabSpeakerStyle"].toString();
        check(style.isNotEmpty() && styles.insert(style).second,"driver selection reuses another front-face design identity");
        for(int unit=1;unit<scene.speakerCount();++unit) {
            auto& sibling=component<cabArt::View>(scene,("ocab1_speakerImage"+juce::String(unit+1)).toRawUTF8());
            check(sibling.getProperties()["cabSpeakerStyle"].toString()==style,"one unit retains a stale front-face design");
        }
        cabArt::View canonical;canonical.setAsset(actual.asset(),actual.getTitle());canonical.setSpeakerDesign(driver);
        canonical.setSize(256,256);canonical.setArtworkArea({0.f,0.f,256.f,256.f});
        const auto image=canonical.createComponentSnapshot(canonical.getLocalBounds());frontSilhouette(image);
        for(const auto& previous:designs)check(greyDifference(previous,image)>.002,"distinct driver types reuse the same rendered front design");
        designs.push_back(image);
        const int x=((driver-1)%7)*256,y=((driver-1)/7)*294;
        cg.drawImageAt(image,x,y);cg.setColour(juce::Colour(0xffeee8dc));cg.setFont(juce::FontOptions(16.f));
        cg.drawText(cabExpansion::drivers[size_t(driver-1)].name,x,y+260,256,26,juce::Justification::centred);
    }
    writeImage(contact,screenshots,"cab-visual-driver-front-designs-14.png");

    // Same viewport, mic model and distance across all three diameters.
    set(*p,"lcab1_layout",6);set(*p,"xcab1_Amic",1);set(*p,"xcab1_Bmic",10);
    set(*p,"lcab1_Aunit",0);set(*p,"lcab1_Bunit",3);
    set(*p,"ocab1_Aposition",.25f);set(*p,"ocab1_Bposition",.25f);
    set(*p,"ocab1_Adistance",10);set(*p,"ocab1_Bdistance",10);
    juce::Image sizes(juce::Image::RGB,scene.getWidth()*3,scene.getHeight()+36,true);juce::Graphics sg(sizes);
    sg.fillAll(juce::Colour(0xff181d1d));
    for(size_t i=0;i<drivers.size();++i) {
        set(*p,"xcab1_driver",float(drivers[i]));scene.refresh();
        const auto state=cabSceneUITests::parameterValues(*p);
        const auto image=scene.createComponentSnapshot(scene.getLocalBounds());
        check(cabSceneUITests::parameterValues(*p)==state,"rendering the changed driver writes host parameters");
        const int x=int(i)*scene.getWidth();sg.drawImageAt(image,x,36);sg.setColour(juce::Colour(0xffeee8dc));sg.setFont(juce::FontOptions(17.f));
        sg.drawText(juce::String(inches[i])+" inch / "+cabExpansion::drivers[size_t(drivers[i]-1)].name,
            x,2,scene.getWidth(),30,juce::Justification::centred);
    }
    writeImage(sizes,screenshots,"cab-visual-driver-size-10-12-15.png");

    // Room overview must redraw for driver changes within the same family too.
    set(*p,"mode",2);for(int lane=0;lane<3;++lane) {
        set(*p,originalCabID(lane,"Aon"),1);set(*p,originalCabID(lane,"Bon"),1);
        set(*p,cabLayoutID(lane,"layout"),float(lane==2 ? 7 : 6));
        set(*p,cabExpansionID(lane,"driver"),float(drivers[size_t(lane)]));
    }
    CabRoomOverview room(*p);room.setSize(1040,620);room.refreshState();
    auto& card=component<juce::Button>(room,"cabRoomRig1");juce::Image prior;
    for(int driver=1;driver<=14;++driver) {
        set(*p,"xcab1_driver",float(driver));room.refreshState();
        check(int(card.getProperties()["cabRoomDriver"])==driver,"room retains a stale driver selection");
        const auto image=card.createComponentSnapshot(card.getLocalBounds());
        if(prior.isValid())check(greyDifference(prior,image)>.0001,"room does not redraw a changed driver front");
        prior=image;
    }
    for(int layout=0;layout<=9;++layout) {
        set(*p,"lcab1_layout",float(layout));room.refreshState();
        check(int(card.getProperties()["cabRoomLayout"])==layout
            && int(card.getProperties()["cabRoomUnitCount"])==cabLayout::count(layout),"room layout/unit count is stale");
    }
    set(*p,"lcab1_layout",6);set(*p,"xcab1_driver",9);room.refreshState();
    const auto before=cabSceneUITests::parameterValues(*p);
    snapshot(room,screenshots,"cab-visual-driver-room-matrix.png");
    check(cabSceneUITests::parameterValues(*p)==before,"room painting changes audio state");
    std::cout<<"PASS driver visuals: 14 distinct circular fronts, "<<dimensionCases
        <<" fixed-camera diameter/mic-size cases, "<<placementCases<<" microphone boundary placements and room refresh\n";
}

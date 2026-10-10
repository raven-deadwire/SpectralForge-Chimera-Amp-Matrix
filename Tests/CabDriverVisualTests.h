#pragma once
#include <set>
#include "CabEnclosureMaterialTests.h"

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
inline int matchingLayout(int driver) {
    return driver<=8 ? 1 : (driver==9 || driver==10 || driver==12) ? 6 : driver==13 ? 4 : 8;
}
inline void hornGrilleContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    cabLayout::Settings selected{};selected.layout=6;selected.voice.base.cabinet=1;
    const auto model=cabLayout::geometry(selected);
    for(bool native:{false,true})for(float scale:{200.f,400.f,700.f})for(int kind=0;kind<4;++kind) {
        const auto type=native ? std::unique_ptr<juce::ImageType>(new juce::NativeImageType)
                               : std::unique_ptr<juce::ImageType>(new juce::SoftwareImageType);
        const int width=int(std::ceil(float(model.box.width)*scale))+40;
        const int height=int(std::ceil(float(model.box.height+model.box.depth*.16)*scale))+40;
        const juce::Rectangle<float> box(20.f,20.f,float(model.box.width)*scale,
            float(model.box.height+model.box.depth*.16)*scale);
        juce::Image rear(juce::Image::ARGB,width,height,true,*type),mask(juce::Image::ARGB,width,height,true,*type);
        {juce::Graphics g(rear);g.fillAll(juce::Colour(0xff111719));cabLayoutView::tweeter(g,model,box,true,.65f,kind);}
        {juce::Graphics g(mask);cabLayoutView::grille(g,model,box,true);}
        juce::Image front(juce::Image::ARGB,width,height,true,*type);
        {juce::Graphics g(front);g.fillAll(juce::Colour(0xff111719));cabLayoutView::frontHardware(g,model,box,true,.65f,kind);}
        const auto centre=cabLayoutView::point(model,box,model.horn);
        const auto region=juce::Rectangle<float>(.118f*scale,.080f*scale).withCentre(centre).toNearestInt();
        const juce::Image::BitmapData rearPixels(rear,juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData maskPixels(mask,juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData frontPixels(front,juce::Image::BitmapData::readOnly);
        int wires=0,holes=0;
        for(int y=region.getY();y<region.getBottom();++y)for(int x=region.getX();x<region.getRight();++x) {
            const auto back=rearPixels.getPixelColour(x,y),wire=maskPixels.getPixelColour(x,y),actual=frontPixels.getPixelColour(x,y);
            const float a=wire.getFloatAlpha();
            const auto blend=[&](int b,int f){return juce::roundToInt((1-a)*float(b)+a*float(f));};
            check(std::abs(int(actual.getRed())-blend(back.getRed(),wire.getRed()))<=3
                && std::abs(int(actual.getGreen())-blend(back.getGreen(),wire.getGreen()))<=3
                && std::abs(int(actual.getBlue())-blend(back.getBlue(),wire.getBlue()))<=3,
                "HF hardware covers the foreground grille instead of sitting behind it");
            wires+=wire.getAlpha()>100;holes+=wire.getAlpha()<8;
        }
        if(!(wires>8 && holes>8))std::cerr<<"horn grille mismatch native="<<int(native)
            <<" scale="<<scale<<" kind="<<kind<<" wires="<<wires<<" holes="<<holes<<'\n';
        check(wires>8 && holes>8,"central horn lost continuous grille wires or open apertures");
        if(!native && scale==700.f && kind==0)writeImage(front,screenshots,"cab-visual-horn-behind-grille-detail.png");
    }
    std::cout<<"PASS HF depth: 24 native/software renders, four kinds and three scales, continuous wires and transmissive apertures\n";
}
inline void shellArtworkContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    const juce::SharedResourcePointer<cabArt::Bank> bank;
    cabIntegratedUITests::artworkResources();
    check(bank->enclosureSkins[0].isValid() && bank->enclosureSkins[1].isValid(),
        "original cabinet materials are absent from the shared artwork bank");
    check(!bank->enclosureSkins[0].bass && bank->enclosureSkins[1].bass
        && juce::String(bank->enclosureSkins[0].key())=="cab-guitar-412-shell"
        && juce::String(bank->enclosureSkins[1].key())=="cab-bass-410-shell",
        "guitar and bass shells lost their original material identities");
    cabLayout::Settings selected{};selected.layout=3;
    const auto model=cabLayout::geometry(selected);constexpr float scale=400.f;
    const juce::Rectangle<float> box(20.f,20.f,float(model.box.width)*scale,
        float(model.box.height+model.box.depth*.16)*scale);
    for(const auto& skin:bank->enclosureSkins) {
        const auto& tile=skin.grilleOverlay;
        int low=255,high=0;
        for(int y=0;y<tile.getHeight();++y)for(int x=0;x<tile.getWidth();++x) {
            const int alpha=tile.getPixelAt(x,y).getAlpha();low=juce::jmin(low,alpha);high=juce::jmax(high,alpha);
        }
        check(low==0 && high>100 && high<255,"original grille lost its open apertures or became an opaque cover");
        juce::Image before(juce::Image::ARGB,336,367,true);
        {juce::Graphics g(before);g.fillAll(juce::Colour(0xffb5a390));}
        auto after=before.createCopy();
        {juce::Graphics g(after);cabLayoutView::grille(g,model,box,skin.bass);}
        check(greyDifference(before,after)>.005,"continuous foreground grille does not cover the driver plane");
        check(after.getPixelAt(0,0)==before.getPixelAt(0,0),"grille escapes the physical front baffle");
    }
    // Use the real scene painter and identical bounds to isolate the two
    // original shells from speaker choice, lighting and physical camera size.
    std::array<juce::Image,2> shells;
    for(int family=0;family<2;++family) {
        shells[size_t(family)]=juce::Image(juce::Image::ARGB,336,367,true);
        juce::Graphics g(shells[size_t(family)]);cabLayoutView::enclosure(g,model,box,family==1);
    }
    const auto face=cabLayoutView::baffle(model,box);
    const auto rowEdges=[&](const juce::Image& image,float y) {
        int left=image.getWidth(),right=-1;
        for(int x=0;x<image.getWidth();++x)if(image.getPixelAt(x,juce::roundToInt(y)).getAlpha()>64) {
            left=juce::jmin(left,x);right=juce::jmax(right,x);
        }
        return juce::Point<int>(left,right);
    };
    const auto guitarTop=rowEdges(shells[0],face.getY()+face.getHeight()*.16f);
    const auto guitarLower=rowEdges(shells[0],face.getY()+face.getHeight()*.65f);
    const auto bassTop=rowEdges(shells[1],face.getY()+face.getHeight()*.16f);
    const auto bassLower=rowEdges(shells[1],face.getY()+face.getHeight()*.65f);
    check(guitarTop.x>guitarLower.x+3 && guitarTop.y<guitarLower.y-3,
        "guitar 4x12 upper baffle still has a flat rectangular silhouette");
    check(std::abs(bassTop.x-bassLower.x)<=1 && std::abs(bassTop.y-bassLower.y)<=1,
        "guitar slant was incorrectly applied to the straight bass enclosure");
    check(greyDifference(shells[0],shells[1])>.004,
        "production enclosure painter still replaces guitar and bass materials with one generic shell");
    juce::Image comparison(juce::Image::ARGB,672,367,true);juce::Graphics g(comparison);
    g.drawImageAt(shells[0],0,0);g.drawImageAt(shells[1],336,0);
    writeImage(comparison,screenshots,"cab-visual-original-shell-materials.png");
    // All nine cabinets use the same production shell painter at one metre
    // scale. This evidence makes material stretching on short/wide/tall boxes
    // visible without scaling each cabinet independently to its card.
    juce::Image catalog(juce::Image::RGB,960,1080,true);juce::Graphics cg(catalog);
    cg.fillAll(juce::Colour(0xff181b1b));
    for(size_t index=0;index<cabLayout::layouts.size();++index) {
        const auto& entry=cabLayout::layouts[index];
        cabLayout::Settings settings{};settings.layout=int(index)+1;settings.voice.base.cabinet=int(entry.bass);
        settings=cabLayout::effectiveSettings(settings);
        const auto geometry=cabLayout::geometry(settings);constexpr float metres=300.f;
        const int column=int(index)%3,row=int(index)/3;
        const float width=float(geometry.box.width)*metres;
        const float height=float(geometry.box.height+geometry.box.depth*.16)*metres;
        const juce::Rectangle<float> bounds(column*320.f+(320.f-width)*.5f,row*360.f+320.f-height,width,height);
        check(geometry.count==entry.columns*entry.rows,"cabinet style catalog has an incorrect driver count");
        cabLayoutView::enclosure(cg,geometry,bounds,entry.bass);
        cabLayoutView::speakers(cg,geometry,bounds,settings.voice.driver,int(entry.bass));
        cabLayoutView::grille(cg,geometry,bounds,entry.bass);
        cg.setColour(juce::Colour(0xffd9c6a1));cg.setFont(juce::FontOptions(16.f));
        cg.drawText(entry.name,column*320,row*360+330,320,24,juce::Justification::centred);
    }
    writeImage(catalog,screenshots,"cab-visual-full-style-catalog-9.png");
}
inline void roomCameraContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    using namespace cabMicrophoneUITests;
    auto p=std::make_unique<ChimeraProcessor>();set(*p,"mode",2);set(*p,"lowampmix",1);
    const std::array<int,3> layouts{{7,3,6}},drivers{{9,1,9}},amps{{5,2,25}};
    const std::array<float,3> widths{{.63f,.74f,.62f}},heights{{.94f,.76f,.64f}},depths{{.40f,.36f,.40f}};
    for(int lane=0;lane<3;++lane) {
        p->setAmpModel(lane,amps[size_t(lane)]);
        set(*p,"ampon"+juce::String(lane+1),1);set(*p,"cab"+juce::String(lane+1),1);
        set(*p,originalCabID(lane,"Aon"),1);set(*p,originalCabID(lane,"Bon"),1);
        set(*p,cabLayoutID(lane,"layout"),float(layouts[size_t(lane)]));
        set(*p,cabExpansionID(lane,"driver"),float(drivers[size_t(lane)]));
    }
    CabRoomOverview room(*p);
    for(const auto size:std::array<juce::Point<int>,3>{{{1140,412},{1040,620},{900,560}}}) {
        room.setSize(size.x,size.y);room.refreshState();
        const float scale=room.displayedScale(0);
        check(std::isfinite(scale) && scale>0.f,"active room camera is invalid at a supported viewport");
        const auto before=cabSceneUITests::parameterValues(*p);
        cabSceneUITests::HostEvents events(*p);
        for(int lane=0;lane<3;++lane) {
            const auto box=room.displayedCabinetBounds(lane),head=room.displayedHeadBounds(lane);
            const auto area=room.getRigBounds(lane).withPosition(0,0).toFloat();
            const auto model=room.displayedGeometry(lane);const auto physicalHead=cabPhysical::head(amps[size_t(lane)]);
            check(std::abs(room.displayedScale(lane)-scale)<.0001f,"room independently enlarges a rig beside the 6x10");
            check(std::abs(box.getWidth()/scale-widths[size_t(lane)])<.00001f
                && std::abs(box.getHeight()/scale-heights[size_t(lane)]-depths[size_t(lane)]*.16f)<.00001f,
                "6x10 / 4x12 / 4x10 room cabinet proportions differ from their physical geometry");
            check(std::abs(head.getWidth()/scale-physicalHead.width)<.00001f
                && std::abs(head.getHeight()/scale-physicalHead.height-physicalHead.depth*.16f)<.00001f,
                "room head does not share its cabinet's physical metre");
            check(area.contains(box) && area.contains(head),"active stack is clipped by a narrow room lane");
            check(std::abs(box.getBottom()-room.displayedCabinetBounds(0).getBottom())<.0001f,
                "room cabinets have different ground baselines");
            const auto face=cabLayoutView::baffle(model,box);
            check(std::abs(face.getHeight()/scale-heights[size_t(lane)])<.00001f,
                "room baffle front height includes an extra scale or roof projection");
        }
        check(std::abs(room.displayedCabinetBounds(1).getWidth()/room.displayedCabinetBounds(0).getWidth()-.74f/.63f)<.0001f,
            "guitar 4x12 is wrongly narrower than the 6x10");
        check(std::abs(room.displayedCabinetBounds(2).getWidth()/room.displayedCabinetBounds(0).getWidth()-.62f/.63f)<.0001f,
            "bass 4x10 is wrongly much narrower than the 6x10");
        snapshot(room,screenshots,("cab-visual-room-610-412-410-"+juce::String(size.x)+"x"+juce::String(size.y)+".png").toRawUTF8());
        if(size.x==1140)writeImage(room.createComponentSnapshot(room.getLocalBounds(),true,.75f),screenshots,
            "cab-visual-room-610-412-410-75-percent.png");
        cabSceneUITests::unchangedExcept(*p,before,{});events.expect();
    }
    room.setSize(1040,620);room.refreshState();
    const float withSix=room.displayedScale(0);
    check(std::abs(withSix-432.f/1.39784f)<.0001f,
        "6x10 room does not frame the actual .94 m cabinet plus selected head");
    check(room.displayedCabinetBounds(1).getWidth()>225.f,
        "whole-room 4x12 beside 6x10 still loses width to unused microphone space");
    set(*p,"lcab1_layout",6);room.refreshState();
    const float withoutSix=room.displayedScale(0);
    check(withoutSix>withSix*1.10f && room.displayedCabinetBounds(1).getWidth()>240.f,
        "room without an 6x10 still reserves the catalogue's unused tall-cabinet envelope");
    const auto cabinet=room.displayedCabinetBounds(1);const auto head=room.displayedHeadBounds(1);
    for(int mic:{1,3,12,20})for(float distance:{2.f,60.f}) {
        set(*p,"xcab2_Amic",float(mic));set(*p,"ocab2_Adistance",distance);
        set(*p,"ocab2_Aposition",distance==2.f ? 0.f : 1.f);set(*p,"lcab2_Aunit",distance==2.f ? 0.f : 3.f);
        const auto before=cabSceneUITests::parameterValues(*p);room.refreshState();
        check(room.displayedScale(1)==withoutSix && room.displayedCabinetBounds(1)==cabinet && room.displayedHeadBounds(1)==head,
            "room camera jitters with microphone identity or placement automation");
        cabSceneUITests::unchangedExcept(*p,before,{});
    }
    snapshot(room,screenshots,"cab-visual-room-410-412-410-active-fit.png");
    // A stored 6x10 in an inactive lane must not constrain a Dual room.
    set(*p,"mode",1);set(*p,"lcab3_layout",7);room.refreshState();
    const float inactiveSix=room.displayedScale(0);
    set(*p,"lcab3_layout",1);room.refreshState();
    check(room.displayedScale(0)==inactiveSix,"inactive Matrix HIGH cabinet reframes a Dual room");
    // Reproduce the reported room: one guitar driver beside a single six-driver
    // bass enclosure. Keep this rendered artifact in the native CAB evidence;
    // generic capture thumbnails cannot verify modeled enclosure composition.
    room.setSize(1040,780);
    set(*p,"lcab1_layout",1);set(*p,"xcab1_driver",1);
    set(*p,"lcab2_layout",7);set(*p,"xcab2_driver",10);
    for(int lane=0;lane<2;++lane)p->setAmpModel(lane,0);
    room.refreshState();
    check(room.displayedGeometry(0).count==1 && room.displayedGeometry(1).count==6,
        "reported Dual 1x12/6x10 fixture uses the wrong modeled speaker counts");
    snapshot(room,screenshots,"cab-visual-room-dual-112-610-1040x780.png");
    // The user's original full-cabinet style reference: compare the shells in
    // its 4x12 / 4x10 layout as well as the current 1x12 / 6x10 composition.
    set(*p,"lcab1_layout",0);set(*p,"xcab1_driver",0);set(*p,"ocab1_design",0);
    set(*p,"lcab2_layout",0);set(*p,"xcab2_driver",0);set(*p,"ocab2_design",1);
    // The supplied reference uses the 515-style tube-window head, not the
    // cloth-front British head used by the earlier comparison fixture.
    for(int lane=0;lane<2;++lane)p->setAmpModel(lane,2);
    room.refreshState();
    check(room.displayedGeometry(0).count==4 && room.displayedGeometry(1).count==4,
        "original-style reference room has the wrong driver layout");
    set(*p,"ocab2_tweeter",0);set(*p,"xcab2_tweeter",0);room.refreshState();
    const auto withoutHorn=room.createComponentSnapshot(room.getLocalBounds());
    set(*p,"ocab2_tweeter",.65f);room.refreshState();
    const auto beforeHornRender=cabSceneUITests::parameterValues(*p);
    const auto withHorn=room.createComponentSnapshot(room.getLocalBounds());
    check(greyDifference(withoutHorn,withHorn)>.00005,"room does not display the active original HF horn");
    check(cabSceneUITests::parameterValues(*p)==beforeHornRender,"horn rendering writes host parameters");
    snapshot(room,screenshots,"cab-visual-room-dual-original-style-412-410.png");
    // Exercise the current selectable layouts too: a legacy-only screenshot
    // could hide a misplaced authored 4x10 horn.
    set(*p,"lcab1_layout",3);set(*p,"xcab1_driver",1);
    set(*p,"lcab2_layout",6);set(*p,"xcab2_driver",9);room.refreshState();
    const auto currentBass=room.displayedGeometry(1);
    const auto currentBox=room.displayedCabinetBounds(1);
    const auto hornCentre=cabLayoutView::point(currentBass,currentBox,currentBass.horn);
    const auto face=cabLayoutView::baffle(currentBass,currentBox);
    check(hornCentre.getDistanceFrom(face.getCentre())<.01f,"current 4x10 horn is not centred between the cones");
    set(*p,"ocab2_tweeter",0);room.refreshState();
    const auto currentWithout=room.createComponentSnapshot(room.getLocalBounds());
    set(*p,"ocab2_tweeter",.65f);room.refreshState();
    const auto currentBefore=cabSceneUITests::parameterValues(*p);
    const auto currentWith=room.createComponentSnapshot(room.getLocalBounds());
    // The local centre must contain visible hardware, not only a change
    // elsewhere on the cabinet caused by the enabled control.
    const auto roomHornCentre=hornCentre+room.getRigBounds(1).getPosition().toFloat();
    const auto centreRegion=juce::Rectangle<int>(int(roomHornCentre.x)-12,int(roomHornCentre.y)-8,24,16);
    writeImage(currentWithout,screenshots,"cab-visual-current-horn-off.png");
    writeImage(currentWith,screenshots,"cab-visual-current-horn-on.png");
    std::cout<<"4x10 centre horn pixel difference="
        <<greyDifference(currentWithout.getClippedImage(centreRegion),currentWith.getClippedImage(centreRegion))
        <<" centre="<<roomHornCentre.x<<","<<roomHornCentre.y<<'\n';
    check(greyDifference(currentWithout.getClippedImage(centreRegion),currentWith.getClippedImage(centreRegion))>.02,
        "current 4x10 original horn plate is absent at the centre");
    check(cabSceneUITests::parameterValues(*p)==currentBefore,"current 4x10 horn rendering writes host parameters");
    snapshot(room,screenshots,"cab-visual-room-current-412-410-centre-horn.png");
    const auto detailBounds=juce::Rectangle<float>(.28f*room.displayedScale(1),.20f*room.displayedScale(1))
        .withCentre(roomHornCentre).toNearestInt();
    const auto detailRoom=room.createComponentSnapshot(room.getLocalBounds(),true,3.f);
    writeImage(detailRoom.getClippedImage({detailBounds.getX()*3,detailBounds.getY()*3,
        detailBounds.getWidth()*3,detailBounds.getHeight()*3}),screenshots,"cab-visual-room-410-horn-detail.png");
    std::cout<<"PASS room camera: active-only common scale, 6x10/4x12/4x10 physical proportions, normal/constrained/75% visuals, "
        <<"unused microphone space removed, inactive-rig isolation and stable microphone automation\n";
}
}

// Product regressions: compare rendered outputs and independent nominal sizes,
// not the particular camera/painter implementation that happens to produce them.
void driverVisualContracts(const juce::File& screenshots) {
    using namespace spectralforge;
    using namespace cabMicrophoneUITests;
    using namespace cabDriverVisualTests;
    cabEnclosureMaterialTests::run();
    shellArtworkContracts(screenshots);
    hornGrilleContracts(screenshots);
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
    const std::array<int,10> layoutInches{{10,12,12,12,15,10,10,10,12,12}};
    int dimensionCases=0,placementCases=0;
    std::array<float,10> layoutCameras{};
    std::array<juce::Point<float>,2> referenceMicMetres{};
    set(*p,"ocab1_design",1);
    for(int layout=0;layout<=9;++layout) {
        set(*p,"lcab1_layout",float(layout));
        juce::Rectangle<float> baseCabinet;
        for(size_t d=0;d<drivers.size();++d) {
            set(*p,"xcab1_driver",float(drivers[d]));
            const auto state=cabSceneUITests::parameterValues(*p);
            scene.refresh();
            check(cabSceneUITests::parameterValues(*p)==state,"resolving an incompatible stored driver rewrites host parameters");
            const float diameter=scene.speakerOuterDiameter();
            if(d==0) {baseCabinet=scene.cabinetBounds();layoutCameras[size_t(layout)]=scene.pixelsPerMetre();}
            check(std::abs(diameter/scene.pixelsPerMetre()-float(layoutInches[size_t(layout)])*.0254f)<.00001f,
                "an incompatible restored driver changes the cabinet's fixed nominal speaker diameter");
            check(scene.cabinetBounds()==baseCabinet,"an incompatible stored driver resizes the fixed cabinet box");
            check(std::abs(scene.pixelsPerMetre()-layoutCameras[size_t(layout)])<.0001f,
                "driver selection reframes an unchanged focused cabinet/head");
            check(std::abs(scene.amplifierBounds().getWidth()/scene.pixelsPerMetre()
                    -cabPhysical::head(p->selectedAmpModel(0)).width)<.00001f,
                "amplifier head width does not use the same physical scale as the cabinet");
            originalCab::Settings requestedBase{};requestedBase.cabinet=1;
            const auto effective=cabLayout::effectiveSettings({{requestedBase,drivers[d],0,0},layout,0});
            for(int slot=0;slot<2;++slot) {
                const auto mic=scene.micVisualGeometry(slot);
                const juce::Point<float> metres{mic.bodyBounds.getWidth()/scene.pixelsPerMetre(),
                    mic.bodyBounds.getHeight()/scene.pixelsPerMetre()};
                if(dimensionCases==0)referenceMicMetres[size_t(slot)]=metres;
                check(metres.getDistanceFrom(referenceMicMetres[size_t(slot)])<.00001f,
                    "focused framing changes a microphone's physical dimensions relative to its cabinet");
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
                check(view.getProperties()["cabSpeakerStyle"].toString()==cabSpeakerArt::styleKey(effective.voice.driver,int(cabLayout::isBass(effective))),
                    "incompatible raw driver artwork disagrees with the effective fixed-diameter model");
            }
            ++dimensionCases;
        }
    }
    check(layoutCameras[1]>layoutCameras[7]*1.25f,
        "focused 1x12 still reserves the unselected 6x10 envelope instead of fitting its own stack");

    // Independent real dimensions: SM57 body is 32 x 157 mm, and its catalog
    // image is rotated 65 degrees. This compares the actual rendered envelope
    // against a 12-inch speaker, not against the production dimension table.
    set(*p,"lcab1_layout",1);set(*p,"xcab1_driver",1);set(*p,"xcab1_Amic",1);
    juce::Point<float> closeMicSize;
    for(float distance:{2.f,60.f}) {
        set(*p,"ocab1_Adistance",distance);scene.refresh();
        const auto body=scene.micVisualGeometry(0).bodyBounds;
        const float angle=juce::degreesToRadians(65.f);
        const float width=.032f*std::cos(angle)+.157f*std::sin(angle);
        const float height=.032f*std::sin(angle)+.157f*std::cos(angle);
        check(std::abs(body.getWidth()/scene.speakerOuterDiameter()-width/.3048f)<.0001f
            && std::abs(body.getHeight()/scene.speakerOuterDiameter()-height/.3048f)<.0001f,
            "SM57 artwork does not preserve its real dimensions relative to a 12-inch driver");
        const juce::Point<float> size{body.getWidth(),body.getHeight()};
        if(distance==2.f)closeMicSize=size;
        else check(size.getDistanceFrom(closeMicSize)<.01f,"moving a microphone away invents body-size growth");
    }

    // Exercise both microphones at opposite boundary units/positions. This also
    // covers the longest and widest bodies at every selected-stack camera scale.
    for(int layout=0;layout<=9;++layout)for(const int driver:drivers) {
        set(*p,"lcab1_layout",float(layout));set(*p,"xcab1_driver",float(driver));scene.refresh();
        const int last=scene.speakerCount()-1;
        const float camera=scene.pixelsPerMetre();const auto cabinet=scene.cabinetBounds();
        for(int mic=1;mic<=20;++mic)for(int unit:{0,last})for(float position:{0.f,1.f})for(float distance:{2.f,60.f}) {
            set(*p,"xcab1_Amic",float(mic));set(*p,"xcab1_Bmic",float(mic));
            set(*p,layout ? "lcab1_Aunit" : "ocab1_Aunit",float(unit));
            set(*p,layout ? "lcab1_Bunit" : "ocab1_Bunit",float(last-unit));
            set(*p,"ocab1_Aposition",position);set(*p,"ocab1_Bposition",1.f-position);
            set(*p,"ocab1_Adistance",distance);set(*p,"ocab1_Bdistance",distance);scene.refresh();
            check(std::abs(scene.pixelsPerMetre()-camera)<.0001f && scene.cabinetBounds()==cabinet,
                "microphone identity, unit, position or distance automation reframes the focused camera");
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
    for(int driver=1;driver<=14;++driver) {
        set(*p,"lcab1_layout",float(matchingLayout(driver)));set(*p,"xcab1_driver",float(driver));
        set(*p,"xcab1_Amic",1);set(*p,"xcab1_Bmic",20);
        set(*p,"ocab1_Adistance",10);set(*p,"ocab1_Bdistance",10);
        const auto state=cabSceneUITests::parameterValues(*p);scene.refresh();
        check(cabSceneUITests::parameterValues(*p)==state,"compatible driver refresh rewrites host parameters");
        check(std::abs(scene.pixelsPerMetre()-layoutCameras[size_t(matchingLayout(driver))])<.0001f,
            "compatible driver selection changes its cabinet/head camera scale");
        check(std::abs(scene.amplifierBounds().getWidth()/scene.pixelsPerMetre()
            -cabPhysical::head(p->selectedAmpModel(0)).width)<.00001f,
            "compatible driver selection changes the head's physical width");
        for(int slot=0;slot<2;++slot) {
            const auto body=scene.micVisualGeometry(slot).bodyBounds;
            check(juce::Point<float>(body.getWidth()/scene.pixelsPerMetre(),body.getHeight()/scene.pixelsPerMetre())
                .getDistanceFrom(referenceMicMetres[size_t(slot)])<.00001f,
                "compatible model selection changes the microphone's physical dimensions");
        }
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
    set(*p,"xcab1_Amic",1);set(*p,"xcab1_Bmic",10);
    set(*p,"lcab1_Aunit",0);set(*p,"lcab1_Bunit",0);
    set(*p,"ocab1_Aposition",.25f);set(*p,"ocab1_Bposition",.25f);
    set(*p,"ocab1_Adistance",10);set(*p,"ocab1_Bdistance",10);
    juce::Image sizes(juce::Image::RGB,scene.getWidth()*3,scene.getHeight()+36,true);juce::Graphics sg(sizes);
    sg.fillAll(juce::Colour(0xff181d1d));
    for(size_t i=0;i<drivers.size();++i) {
        set(*p,"lcab1_layout",float(matchingLayout(drivers[i])));set(*p,"xcab1_driver",float(drivers[i]));scene.refresh();
        check(std::abs(scene.speakerOuterDiameter()/scene.pixelsPerMetre()-float(inches[i])*.0254f)<.00001f,
            "diameter comparison screenshot uses an incompatible cabinet/driver combination");
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
        set(*p,cabLayoutID(lane,"layout"),float(matchingLayout(drivers[size_t(lane)])));
        set(*p,cabExpansionID(lane,"driver"),float(drivers[size_t(lane)]));
    }
    CabRoomOverview room(*p);room.setSize(1040,620);room.refreshState();
    const std::array<float,3> roomCabinetWidths{{.62f,.49f,.56f}};
    const float roomScale=room.displayedScale(0);
    check(roomScale>0.f,"room has no physical camera scale");
    for(int lane=0;lane<3;++lane) {
        check(std::abs(room.displayedScale(lane)-roomScale)<.0001f,"Matrix rig cards use inconsistent pixels per metre");
        check(std::abs(room.displayedCabinetBounds(lane).getWidth()/roomScale-roomCabinetWidths[size_t(lane)])<.00001f,
            "room independently fits a cabinet instead of displaying its physical width");
    }
    auto& card=component<juce::Button>(room,"cabRoomRig1");juce::Image prior;
    for(int driver=1;driver<=14;++driver) {
        set(*p,"lcab1_layout",float(matchingLayout(driver)));set(*p,"xcab1_driver",float(driver));room.refreshState();
        check(int(card.getProperties()["cabRoomDriver"])==driver,"room retains a stale driver selection");
        const auto image=card.createComponentSnapshot(card.getLocalBounds());
        if(prior.isValid())check(greyDifference(prior,image)>.0001,"room does not redraw a changed driver front");
        prior=image;
    }
    for(int layout=0;layout<=9;++layout) {
        set(*p,"lcab1_layout",float(layout));set(*p,"xcab1_driver",13);
        const auto state=cabSceneUITests::parameterValues(*p);room.refreshState();
        check(cabSceneUITests::parameterValues(*p)==state,"room mismatch resolution changes stored host parameters");
        originalCab::Settings requestedBase{};requestedBase.cabinet=1;
        const auto effective=cabLayout::effectiveSettings({{requestedBase,13,0,0},layout,0});
        check(int(card.getProperties()["cabRoomLayout"])==layout
            && int(card.getProperties()["cabRoomUnitCount"])==cabLayout::count(layout),"room layout/unit count is stale");
        check(int(card.getProperties()["cabRoomDriver"])==effective.voice.driver,"room shows the raw incompatible driver instead of the effective model");
        for(int lane=0;lane<3;++lane) {
            const float scale=room.displayedScale(lane);const auto model=room.displayedGeometry(lane);
            check(std::abs(scale-room.displayedScale(0))<.0001f,"room gives a changed cabinet an independent camera scale");
            check(std::abs(room.displayedCabinetBounds(lane).getWidth()/scale-float(model.box.width))<.00001f,
                "room layout change distorts the relative physical width");
        }
    }
    set(*p,"lcab1_layout",6);set(*p,"xcab1_driver",9);room.refreshState();
    const auto before=cabSceneUITests::parameterValues(*p);
    snapshot(room,screenshots,"cab-visual-driver-room-matrix.png");
    check(cabSceneUITests::parameterValues(*p)==before,"room painting changes audio state");
    std::cout<<"PASS driver visuals: 14 distinct circular fronts, "<<dimensionCases
        <<" fixed-diameter restore cases, real microphone scale, "<<placementCases<<" microphone boundary placements and room refresh\n";
    roomCameraContracts(screenshots);
}

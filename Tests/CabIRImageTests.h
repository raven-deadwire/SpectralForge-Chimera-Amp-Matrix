#pragma once
#include "CabIntegratedUITests.h"

namespace cabIRImageTests {
using namespace cabMicrophoneUITests;
inline void run(const juce::File& folder,const juce::File& screenshots) {
    using Instrument=spectralforge::IRMetadata::Instrument;
    using Asset=spectralforge::cabArt::Asset;
    using Image=spectralforge::cabArt::View;
    using Cabinet=spectralforge::capturedCabArt::CabinetView;
    using spectralforge::capturedCabArt::configuration;
    spectralforge::IRMetadata metadata;
    metadata.values[1]="Traynor TC1510 (1x10 + 1x15)";
    require(configuration(metadata).diameters==std::vector<int>{10,15},"Mixed capture cabinet lost one of its documented driver sizes");
    metadata.values[1]=juce::String::fromUTF8("Custom 4 × 12");
    require(configuration(metadata).diameters==std::vector<int>{12,12,12,12},"Spaced Unicode cabinet configuration was not recognized");
    require(!configuration(spectralforge::IRMetadata::factory(0)).known()
        && !configuration(spectralforge::IRMetadata::factory(1)).known(),
        "Factory captures were assigned an undocumented cabinet configuration");

    auto processor=std::make_unique<ChimeraProcessor>();
    const auto automate=[&](const char* id,float value) {
        auto* parameter=processor->parameters().getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    automate("cabtype1",1);automate("cabBtype1",2);
    CabPanel panel(*processor,0,{folder},folder.getChildFile("library.json"));
    const auto before=processor->parameters().copyState();
    panel.setView(CabPanel::View::irLoader);
    require(before.isEquivalentTo(processor->parameters().copyState()),"Opening IR images changed audio parameters or capture state");
    auto& cabinetA=component<Cabinet>(panel,"cabAcabinetImage1");
    auto& cabinetB=component<Cabinet>(panel,"cabBcabinetImage1");
    auto& micA=component<Image>(panel,"cabAmicImage1");
    auto& micB=component<Image>(panel,"cabBmicImage1");
    require(micA.asset()==Asset::dynamic57 && micB.asset()==Asset::dynamic57 && micA.hasImage() && micB.hasImage(),
        "Factory IR microphone artwork is missing or incorrect");
    require(int(cabinetA.getProperties()["cabCaptureUnitCount"])==0 && int(cabinetB.getProperties()["cabCaptureUnitCount"])==0,
        "Factory IR artwork invented a speaker count");
    require(cabinetB.getTitle()=="Unspecified cabinet","Unknown factory cabinet is falsely identified");
    const auto validateLayout=[&] {
        cabIntegratedUITests::layout(panel);
        for(const char* slot:{"A","B"}) {
            const auto prefix="cab"+juce::String(slot);
            auto& cabinet=component<Cabinet>(panel,(prefix+"cabinetImage1").toRawUTF8());
            auto& mic=component<Image>(panel,(prefix+"micImage1").toRawUTF8());
            auto& menu=component<juce::ComboBox>(panel,(prefix+"cabinet1").toRawUTF8());
            auto& caption=component<juce::Label>(panel,(prefix+"micCaption1").toRawUTF8());
            require(cabinet.isVisible() && mic.isVisible() && caption.isVisible()
                && cabinet.getWidth()>=250 && cabinet.getHeight()>=190 && mic.getHeight()>=150,
                "IR loader reverted to missing or tiny equipment thumbnails");
            require(!cabinet.getBounds().intersects(menu.getBounds()) && !mic.getBounds().intersects(menu.getBounds())
                && !caption.getBounds().intersects(menu.getBounds()),"IR loader images or captions overlap the selection controls");
        }
    };
    validateLayout();snapshot(panel,screenshots,"cab-visual-ir-images-factory.png");

    const auto mixed=folder.getChildFile("Mixed.wav"),tagged=folder.getChildFile("Bass_8x10.wav"),unknown=folder.getChildFile("Personal.wav");
    writeFixture(mixed,"Shure SM57 + Royer R-121","Traynor TC1510 (1x10 + 1x15)",Instrument::bass,"",9);
    writeFixture(tagged,"Sennheiser MD 421","Ampeg 8x10",Instrument::bass,"10",19);
    writeFixture(unknown,"","",Instrument::unspecified,"",29);
    require(processor->loadMicIR(0,0,mixed).wasOk() && processor->loadMicIR(0,1,tagged).wasOk(),"Cannot load IR image metadata fixtures");
    require(dispatchUntil([&]{return int(cabinetA.getProperties()["cabCaptureUnitCount"])==2
        && int(cabinetB.getProperties()["cabCaptureUnitCount"])==8 && micB.asset()==Asset::dynamic421;}),
        "IR imagery did not follow the independent A/B capture metadata");
    require(micA.asset()==Asset::count && bool(micA.getProperties()["cabCaptureMixedMicrophones"])
        && component<juce::Label>(panel,"cabAmicCaption1").getText()=="Mixed microphones",
        "Mixed IR was falsely assigned a single microphone image");
    validateLayout();snapshot(panel,screenshots,"cab-visual-ir-images-tagged-mixed.png");
    require(processor->loadMicIR(0,0,unknown).wasOk(),"Cannot load unspecified IR artwork fixture");
    require(dispatchUntil([&]{return cabinetA.getTitle()=="Unspecified cabinet"
        && component<juce::Label>(panel,"cabAmicCaption1").getText()=="Unspecified microphone";}),
        "Unknown IR retained stale model identity or omitted its equipment captions");
    require(micA.asset()==Asset::count && !bool(micA.getProperties()["cabCaptureMixedMicrophones"])
        && int(cabinetA.getProperties()["cabCaptureUnitCount"])==0,"Unknown IR invented capture hardware");
    panel.setSize(900,748);validateLayout();snapshot(panel,screenshots,"cab-visual-ir-images-unknown-compact.png");
    panel.setView(CabPanel::View::cabinet);
    require(!cabinetA.isVisible() && !micA.isVisible() && !component<juce::Label>(panel,"cabAmicCaption1").isVisible(),
        "Capture images leaked into modeled cabinet controls");
    std::cout<<"PASS IR images: factory and tagged captures, mixed/unknown identity, expanded and compact layouts, and parameter-neutral navigation\n";
}
}

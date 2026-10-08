#pragma once
#include "NativeUITests.h"

namespace uiRefreshTests {
using namespace nativeUITests;
inline void run(const juce::File& directory) {
    std::cout<<"RUN UI refresh: construct processor and editor\n"<<std::flush;
    auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;
    // The editor owns a large control tree; keep its storage off the Windows
    // message-thread stack while preserving editor-before-processor teardown.
    auto editorStorage=std::make_unique<ChimeraEditor>(p);auto& editor=*editorStorage;
    auto& canvas=*editor.findChildWithID("surface");
    std::cout<<"RUN UI refresh: idle state and page transitions\n"<<std::flush;
    set(p,"mode",2);tab(canvas,"RIGS");settle(150);
    auto* amp=find<AmpNativePanel>(canvas,"ampNativePanel1");
    auto* post=find<PostNativePanel>(canvas,"postNativePanel0");
    require(amp && post,"Refresh test panels missing");
    const auto nativeCount=amp->stateRefreshCount(),postCount=post->stateRefreshCount(),meters=post->meterRefreshCount(),metadata=editor.metadataRefreshCount();
    settle(250);
    require(amp->stateRefreshCount()==nativeCount,"Unchanged amp state refreshed on idle ticks");
    require(post->stateRefreshCount()==postCount && post->meterRefreshCount()==meters,"Hidden POST panel refreshed");
    require(editor.metadataRefreshCount()==metadata,"Unchanged cabinet metadata fetched during idle ticks");

    tab(canvas,"PRE");settle(100);const auto hiddenAmp=amp->stateRefreshCount();
    p.setAmpModel(0,22);set(p,spectralforge::postNativeModelID(0),2);settle(140);
    require(amp->stateRefreshCount()==hiddenAmp && post->stateRefreshCount()==postCount,"Hidden model change refreshed offscreen panel");
    tab(canvas,"RIGS");settle(100);
    require(find<AmpSelector>(canvas,"ampSelect1")->getSelectedId()==23,"Hidden amp change lost on tab reveal");
    tab(canvas,"POST");settle(100);require(post->selectedModel()==2,"Hidden POST change lost on tab reveal");
    const auto visibleMeters=post->meterRefreshCount(),visibleState=post->stateRefreshCount();settle(160);
    require(post->meterRefreshCount()>visibleMeters && post->stateRefreshCount()==visibleState,"POST meters and model state are not separated");

    // A modal ALL window may outlive the owning page. Drive the page callback
    // directly to test that lifecycle without faking an allowed mouse click.
    std::cout<<"RUN UI refresh: detached ALL window lifetimes\n"<<std::flush;
    auto all=open(canvas,"postExpand0","postNativePanel0");auto* full=dynamic_cast<PostNativePanel*>(all->getContentComponent());
    for(auto* c:canvas.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(c);b && b->getButtonText()=="PRE")b->onClick();
    settle(100);const auto hiddenParent=post->stateRefreshCount();set(p,spectralforge::postNativeModelID(0),1);settle(120);
    require(full->selectedModel()==1 && post->stateRefreshCount()==hiddenParent,"Detached POST ALL stopped or refreshed hidden parent");close(all);
    tab(canvas,"RIGS");all=open(canvas,"ampExpand1","ampNativePanel1");auto* ampFull=dynamic_cast<AmpNativePanel*>(all->getContentComponent());
    for(auto* c:canvas.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(c);b && b->getButtonText()=="PRE")b->onClick();
    settle(100);const auto fullCount=ampFull->stateRefreshCount(),ownerCount=amp->stateRefreshCount();p.setAmpModel(0,15);settle(120);
    require(ampFull->stateRefreshCount()>fullCount && amp->stateRefreshCount()==ownerCount,"Detached amp ALL stopped or refreshed hidden parent");close(all);

    // IR metadata may be edited while its cabinet is hidden, without changing
    // its short label. The explicit library revision must still invalidate it.
    std::cout<<"RUN UI refresh: cabinet metadata invalidation\n"<<std::flush;
    const auto file=directory.getChildFile("ui-refresh-ir.wav");
    {juce::WavAudioFormat format;auto output=file.createOutputStream();
     auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(output.release(),48000,1,24,{},0));
     require(writer!=nullptr,"IR fixture writer failed");juce::AudioBuffer<float> impulse(1,32);impulse.clear();impulse.setSample(0,0,1);require(writer->writeFromAudioSampleBuffer(impulse,0,32),"IR fixture write failed");}
    require(p.loadIR(0,file).wasOk(),"IR fixture import failed");set(p,"cabtype1",3);
    auto m=p.cabMetadata(0);m.values[3]="TEST MICROPHONE";const auto revision=p.cabDisplayRevision(0);p.setCabMetadata(0,m);
    require(revision!=p.cabDisplayRevision(0),"Metadata-only change did not invalidate display");
    tab(canvas,"RIGS");settle(100);bool tooltipFound=false;
    for(auto* c:canvas.getChildren())if(auto* label=dynamic_cast<juce::Label*>(c))tooltipFound |= label->getTooltip().contains("TEST MICROPHONE");
    require(tooltipFound,"Metadata tooltip not refreshed on reveal");
    const auto reads=editor.metadataRefreshCount();settle(180);require(editor.metadataRefreshCount()==reads,"Stable IR metadata repeatedly fetched");

    // Scaling and tab switches must not write audio parameters. Existing native
    // suites cover gestures, host automation, state restore and A/B banks.
    std::cout<<"RUN UI refresh: scaling and parameter preservation\n"<<std::flush;
    std::vector<float> values;for(auto* parameter:p.getParameters())values.push_back(parameter->getValue());
    for(float scale:{.75f,1.f,1.25f,1.5f}) {
        editor.setSize(juce::roundToInt(1180*scale),juce::roundToInt(780*scale));
        for(auto* page:{"PRE","POST","RIGS"}){tab(canvas,page);settle(50);}
        snapshot(editor,directory,("UI-refresh-"+juce::String(scale*100,0)).toRawUTF8());
    }
    int n=0;for(auto* parameter:p.getParameters())require(parameter->getValue()==values[(size_t)n++],"Scaling/page transition wrote a parameter");
    std::cout<<"RUN UI refresh: native peer hide and reveal\n"<<std::flush;
    editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);editor.setVisible(true);settle(100);editor.setVisible(false);settle(80);
    const auto hiddenState=amp->stateRefreshCount(),hiddenMetadata=editor.metadataRefreshCount();settle(200);
    require(amp->stateRefreshCount()==hiddenState && editor.metadataRefreshCount()==hiddenMetadata,"Hidden editor performs panel/metadata work");
    editor.setVisible(true);settle(100);require(amp->stateRefreshCount()>hiddenState,"Revealed editor missed deferred refresh");editor.removeFromDesktop();
    std::cout<<"PASS UI refresh: stable state/metadata, hidden panels/editor, deferred model change, detached ALL windows, live meters, metadata-only invalidation, scaling 75/100/125/150 and parameter preservation\n";
}
}

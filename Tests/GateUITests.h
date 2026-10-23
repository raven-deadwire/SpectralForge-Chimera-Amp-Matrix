#include "EditorSurface.h"
#pragma once
#include "PluginEditor.h"
#include "NativeMessageLoop.h"
#include <stdexcept>
#include <vector>

// Run from the native Windows suite; also available to the offline probe under Xvfb.
inline void checkGateUI(ChimeraProcessor& processor,ChimeraEditor& editor,const juce::File& directory)
{
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const auto settle=[&](int ms=60){require(chimeraTest::dispatchFor(ms),"Gate UI loop exited");};
    const auto find=[](auto&& self,juce::Component& root,const juce::String& id)->juce::Component* {
        if(root.getComponentID()==id)return &root;
        for(auto* child:root.getChildren())if(auto* found=self(self,*child,id))return found;
        return nullptr;
    };
    const auto values=[&]{std::vector<float> result;for(auto* p:processor.getParameters())result.push_back(p->getValue());return result;};
    struct Writes final:juce::AudioProcessorListener {
        ChimeraProcessor& processor;int changes{},gestures{};
        explicit Writes(ChimeraProcessor& p):processor(p){processor.addListener(this);}
        ~Writes() override {processor.removeListener(this);}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++gestures;}
        void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++gestures;}
    };
    struct Peer {
        ChimeraEditor& editor;bool added{},visible{};
        explicit Peer(ChimeraEditor& e):editor(e),added(!e.isOnDesktop()),visible(e.isVisible()) {
            if(added)editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);editor.setVisible(true);
        }
        ~Peer(){editor.setVisible(visible);if(added)editor.removeFromDesktop();}
    } peer(editor);
    settle();
    auto* canvas=editorSurface(editor);require(canvas!=nullptr,"Missing editor canvas");
    // Gate / Expander lives in DYNAMICS; the top-bar slot opens DYNAMICS.
    auto* entry=dynamic_cast<juce::TextButton*>(find(find,*canvas,"commonDynamics.open"));
    require(entry && entry->isShowing() && entry->isEnabled(),"Dynamics entry missing from the top bar");
    require(find(find,*canvas,"gateSpectrum.open")==nullptr,"Old gate top-bar entry remains");
    const auto initial=values();
    {Writes writes(processor);entry->triggerClick();settle(90);
     auto* gateTab=dynamic_cast<juce::TextButton*>(find(find,*canvas,"commonDynamics.page.gate"));
     require(gateTab!=nullptr,"Gate / Expander tab missing");gateTab->triggerClick();settle(60);
     require(values()==initial && writes.changes==0 && writes.gestures==0,"Opening Gate spectrum changed a sound parameter or host gesture");}
    auto* panel=dynamic_cast<spectralforge::CommonDynamicsPanel*>(find(find,*canvas,"commonDynamics.panel"));
    require(panel && panel->isShowing() && panel->selectedPage()==spectralforge::CommonDynamicsPanel::Page::gate,
            "Gate spectrum entry opened the wrong workspace");
    auto* spectrum=dynamic_cast<spectralforge::DynamicsSpectrumPanel*>(find(find,*panel,"commonDynamics.spectrum"));
    require(spectrum && spectrum->isShowing() && spectrum->selectedStage()==spectralforge::DynamicsStage::gate,
            "Gate adjustments have no visible stage spectrum");
    auto* slider=dynamic_cast<juce::Slider*>(find(find,*panel,"commonDynamics.gateRangeDb"));
    require(slider!=nullptr,"Focused Gate Range control missing");
    auto* oldRange=canvas->findChildWithID("gateRangeDb");
    require(oldRange && !oldRange->isShowing(),"Global Gate Range still permits editing without a spectrum");
    const float previous=processor.parameters().getRawParameterValue("gateRangeDb")->load();
    for(int width:{885,1180,1475,1770}) {
        {const auto before=values();Writes writes(processor);editor.setSize(width,width*780/1180);settle();
         require(values()==before && writes.changes==0 && writes.gestures==0,"Gate editor scaling wrote audio parameters");}
        for(const char* id:{"commonDynamics.gatethreshold","gateRatio","gateAttack","commonDynamics.gatehold","commonDynamics.gaterelease","commonDynamics.gateRangeDb","gateMode","commonDynamics.gateon","commonDynamics.gateAfterRig"}) {
            auto* control=find(find,*panel,id);
            require(control && control->isShowing() && editorBoundsReachable(editor,*control),
                    "Gate control is missing or outside the editor at a supported scale");
        }
        require(spectrum->isShowing() && editorBoundsReachable(editor,*spectrum),
                "Gate spectrum is clipped during adjustment");
        slider->setValue(24,juce::sendNotificationSync);
        require(processor.parameters().getRawParameterValue("gateRangeDb")->load()==24,"UI Range edit did not reach DSP parameter");
        auto* parameter=processor.parameters().getParameter("gateRangeDb");parameter->setValueNotifyingHost(1.f);
        require(slider->getValue()==96 && slider->getTextFromValue(96)=="Full" && slider->getValueFromText("Full")==96,
                "Host Range automation or text entry lost the Full attenuation value");
        if(directory.isDirectory()) {
            auto output=directory.getChildFile("Gate-"+juce::String(width)+".png").createOutputStream();
            require(output!=nullptr,"Cannot write Gate screenshot");
            juce::PNGImageFormat png;require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*output),"Cannot encode Gate screenshot");
        }
    }
    slider->setValue(previous,juce::sendNotificationSync);settle();
    require(values()==initial,"Gate editing failed to preserve/restore the complete parameter state");
    auto* tone=dynamic_cast<juce::TextButton*>(find(find,*canvas,"path.tone.eq"));require(tone,"Gate spectrum has no return navigation");
    {Writes writes(processor);tone->triggerClick();editor.setSize(1180,780);settle();
     require(values()==initial && writes.changes==0 && writes.gestures==0 && !spectrum->isShowing(),
             "Leaving Gate spectrum changed audio or kept its analyzer visible");}
}

#pragma once
#include "PluginEditor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

// Exercise the shipped editor callbacks and AudioProcessor together. Catalog
// coverage alone cannot detect a selector that displays a new voice while its
// audio path still receives an old, clamped host parameter.
namespace correctionUITests {
inline void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
inline void settle(int milliseconds=70) {juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);}
inline juce::Component* find(juce::Component& root,const juce::String& id) {
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;
    return nullptr;
}
inline void set(ChimeraProcessor& processor,const juce::String& id,float value) {
    auto* parameter=processor.parameters().getParameter(id);
    require(parameter!=nullptr,"Correction UI test parameter missing");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
inline void snapshot(juce::Component& editor,const juce::File& directory,const char* name) {
    auto stream=directory.getChildFile(juce::String(name)+".png").createOutputStream();
    juce::PNGImageFormat png;
    require(stream && png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()),*stream),
            "Cannot save correction UI evidence");
}
inline void clickTab(juce::Component& canvas,const char* title) {
    for(auto* child:canvas.getChildren())
        if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()==title) {
            button->triggerClick();settle();return;
        }
    throw std::runtime_error("Correction UI test could not find a page tab");
}
inline AmpSelector& selector(juce::Component& canvas,int lane) {
    auto* control=dynamic_cast<AmpSelector*>(find(canvas,"ampSelect"+juce::String(lane+1)));
    require(control!=nullptr,"Actual amplifier selector not exposed in editor");return *control;
}
inline juce::ComboBox& channelSelector(juce::Component& canvas,int lane) {
    auto* control=dynamic_cast<juce::ComboBox*>(find(canvas,"ampChannel"+juce::String(lane+1)));
    require(control!=nullptr,"Actual amplifier channel selector not exposed in editor");return *control;
}
inline std::vector<float> render(ChimeraProcessor& processor) {
    // Reset DSP histories for every selection; otherwise residual filter/sag
    // state could make two identical routed voices appear different.
    processor.prepareToPlay(48000,256);
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    std::vector<float> result;result.reserve(24*256);
    for(int block=0;block<64;++block) {
        for(int n=0;n<256;++n) {
            const double t=(block*256+n)/48000.;
            const auto partial=[t](double hz){return std::sin(juce::MathConstants<double>::twoPi*hz*t);};
            const float sample=float(.17*(partial(61.7354)+.4*partial(197)+.3*partial(997)+.15*partial(3520)));
            audio.setSample(0,n,sample);audio.setSample(1,n,0);
        }
        processor.processBlock(audio,midi);
        for(int n=0;n<256;++n) {
            require(std::isfinite(audio.getSample(0,n)) && std::abs(audio.getSample(0,n))<64,
                    "Selected amplifier produced non-finite or excessive processor audio");
            require(std::abs(audio.getSample(1,n))<1.e-8f,"Amplifier selection leaked into the silent stereo input");
        }
        if(block>=40)result.insert(result.end(),audio.getReadPointer(0),audio.getReadPointer(0)+256);
    }
    return result;
}
inline double rms(const std::vector<float>& values) {
    double energy=0;for(float value:values)energy+=double(value)*value;
    return std::sqrt(energy/values.size());
}
inline double matchedDifference(const std::vector<float>& a,const std::vector<float>& b) {
    const double aRms=rms(a),bRms=rms(b);
    require(a.size()==b.size() && aRms>1.e-5 && bRms>1.e-5,"Selected amplifier is silent in the actual processor");
    double energy=0;for(size_t i=0;i<a.size();++i)energy+=std::pow(a[i]/aRms-b[i]/bRms,2);
    return std::sqrt(energy/a.size());
}
inline void run(const juce::File& directory) {
    const auto storage=std::make_unique<ChimeraProcessor>();auto& processor=*storage;
    require(processor.pedalBoardState().enabled,"A fresh instance still opens the old pedalboard by default");
    auto editorStorage=std::make_unique<ChimeraEditor>(processor);auto& editor=*editorStorage;
    auto* canvas=editor.findChildWithID("surface");
    require(canvas!=nullptr,"Correction UI editor surface missing");
    clickTab(*canvas,"PRE");
    auto* board=canvas->findChildWithID("universalPedalBoard");
    require(board && board->isVisible(),"Fresh PRE page does not expose the five-pedal board");
    for(int position=0;position<5;++position) {
        auto* model=dynamic_cast<juce::ComboBox*>(board->findChildWithID("boardModelAt"+juce::String(position)));
        require(model && model->isVisible() && board->getLocalBounds().contains(model->getBounds()),
                "Fresh PRE page does not display five accessible model selectors");
    }
    for(const auto* id:{"compmodel","filtermodel","fuzzmodel","boostmodel","drivemodel"}) {
        auto* legacy=canvas->findChildWithID(id);
        require(!legacy || !legacy->isVisible(),"Fresh PRE page still exposes the fixed legacy modules");
    }
    snapshot(editor,directory,"Correction-fresh-PRE");

    // A pre-board project opens the same current board and directly editable
    // slots. Compatibility audio handling is internal, without a Legacy page
    // or a second engine-selection step in the user flow.
    auto legacy=processor.parameters().copyState();
    for(int i=legacy.getNumChildren();--i>=0;)
        if(const auto id=legacy.getChild(i).getProperty("id").toString();id.startsWith("board") || id.startsWith("nativeAmp_") || id.startsWith("pn_"))legacy.removeChild(i,nullptr);
    legacy.removeProperty("schemaVersion",nullptr);
    juce::MemoryBlock bytes;auto xml=legacy.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
    processor.setStateInformation(bytes.getData(),int(bytes.getSize()));settle(120);
    for(const auto* id:{"legacyPreControls","boardEnabled"})
        if(auto* obsolete=find(*canvas,id))require(!obsolete->isVisible(),"Recalled PRE still exposes an obsolete engine-switch/Legacy UI");
    for(int position=0;position<5;++position) {
        auto* model=dynamic_cast<juce::ComboBox*>(board->findChildWithID("boardModelAt"+juce::String(position)));
        require(model && model->isVisible() && model->isEnabled(),
                "Recalled project does not expose directly editable five-slot pedal selectors");
    }
    snapshot(editor,directory,"Correction-recalled-PRE");

    // Use an empty new board and bypass optional global processing so these
    // renders measure the selected amplifier's actual processor routing.
    for(int owner=0;owner<5;++owner)processor.setPedalModel(owner,0);
    settle(120);
    require(processor.pedalBoardState().enabled,"Editing the visible board did not activate its audio path");
    for(int position=0;position<5;++position)
        require(board->findChildWithID("boardModelAt"+juce::String(position))->isEnabled(),
                "Pedal selectors became disabled after direct editing");
    set(processor,"gateon",0);set(processor,"mode",0);
    set(processor,"output",0);set(processor,"input",0);set(processor,"oversampling",2);
    for(int lane=0;lane<3;++lane) {
        set(processor,"cab"+juce::String(lane+1),0);set(processor,"drive"+juce::String(lane+1),.68f);
    }
    processor.setRateAndBufferSizeDetails(48000,256);
    clickTab(*canvas,"RIGS");auto& amp=selector(*canvas,0);
    require(amp.getNumItems()==spectralforge::ampModelCount,"Editor recall inventory incomplete");
    require(amp.selectMenuResult(3),"Cannot select legacy reference voice through the actual editor");settle();
    std::vector<std::vector<float>> voices;voices.push_back(render(processor));
    double smallestResidual=100.;
    for(int model=15;model<spectralforge::ampModelCount;++model) {
        if(!spectralforge::ampIsActive(model))continue;
        require(amp.selectMenuResult(model+1),"New amplifier cannot be selected in the product editor");settle();
        require(processor.selectedAmpModel(0)==model && amp.getSelectedId()==model+1,
                "New amplifier menu selection did not reach the actual AudioProcessor");
        require(amp.getText()==spectralforge::ampInfo(model).name,"Selected amplifier field uses a reference name");
        require(!amp.getTooltip().containsIgnoreCase(spectralforge::ampInfo(model).reference),
                "Selected amplifier tooltip reveals its original reference name");
        auto output=render(processor);
        for(const auto& previous:voices) {
            const double residual=matchedDifference(output,previous);smallestResidual=juce::jmin(smallestResidual,residual);
            require(residual>1.e-5,"New amplifier selector still routes to an already rendered voice");
        }
        voices.push_back(std::move(output));
        auto& channel=channelSelector(*canvas,0);
        const auto& panel=spectralforge::ampNativePanel(model);
        require(channel.getNumItems()==int(panel.channels.size()),"Native channel selector does not match the selected amplifier panel");
        require(channel.isVisible()==(panel.channels.size()>1),"Single-channel amplifier exposes an unnecessary channel selector");
        for(int index=0;index<channel.getNumItems();++index) {
            channel.setSelectedItemIndex(index,juce::sendNotificationSync);settle(20);
            require(processor.selectedAmpChannel(0)==index,"Channel selection did not reach actual amplifier DSP state");
        }
        auto* reference=dynamic_cast<juce::Label*>(find(*canvas,"ampreference1"));
        require(reference && reference->isVisible() && reference->getText()==juce::String::fromUTF8(spectralforge::ampInfo(model).reference),
                "Selected amplifier caption must retain its original model name without a prefix");
    }
    snapshot(editor,directory,"Correction-new-amp-channels");

    set(processor,"mode",2);settle(120);
    const std::array<int,3> models{15,18,22};
    for(int lane=0;lane<3;++lane) {
        auto& control=selector(*canvas,lane);
        require(control.isVisible() && control.selectMenuResult(models[(size_t)lane]+1),
                "Matrix lane cannot select an extended amplifier");settle();
        require(processor.selectedAmpModel(lane)==models[(size_t)lane],"Matrix lane selection reached the wrong processor lane");
    }
    for(int lane=0;lane<3;++lane) {
        require(processor.selectedAmpModel(lane)==models[(size_t)lane],"A later lane selection changed an earlier lane");
        require(selector(*canvas,lane).getSelectedId()==models[(size_t)lane]+1,"Matrix lane selector did not retain its model");
    }
    snapshot(editor,directory,"Correction-three-extended-lanes");
    std::cout<<"PASS correction UI: directly editable five-slot PRE after fresh/old recall, alias-only amp menus with visible reference captions, "
               "8 editor-to-audio DSP selections, native channel callbacks and isolated Matrix lane selectors; "
               "minimum RMS-matched processor residual "<<smallestResidual<<"\n";
}
} // namespace correctionUITests

inline void runCorrectionUITests(const juce::File& directory) {correctionUITests::run(directory);}

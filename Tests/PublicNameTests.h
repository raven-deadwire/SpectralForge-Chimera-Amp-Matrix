#pragma once
#include "AmpCatalog.h"
#include "AmpNativeCatalog.h"
#include "EffectSelectionCatalog.h"
#include "GuitarSignaturePresets.h"
#include "PresetSummaries.h"
#include "PostNativeCatalog.h"
#include <iostream>
#include <stdexcept>

namespace publicNameTests {
inline const juce::StringArray& forbiddenTerms() {
    static const auto terms=[] {
        // This is test-only policy input, never embedded in a product target.
        // __FILE__ follows the included header's source path on CMake builders.
        const auto file=juce::File(juce::String(__FILE__)).getParentDirectory().getParentDirectory()
            .getChildFile("Validation/public-presentation-policy.json");
        if(!file.existsAsFile())throw std::runtime_error("Public-name test policy is missing");
        const auto policy=juce::JSON::parse(file);
        if(policy["schema"].toString()!="spectralforge.public-presentation.v1")
            throw std::runtime_error("Unknown public-name test policy");
        juce::StringArray result;
        if(const auto* values=policy["forbidden_terms"].getArray())
            for(const auto& term:*values)result.add(term.toString().toLowerCase());
        if(result.size()<25)throw std::runtime_error("Public-name test inventory is incomplete");
        return result;
    }();
    return terms;
}
inline bool word(juce::juce_wchar ch) {return juce::CharacterFunctions::isLetterOrDigit(ch) || ch=='_';}
inline juce::String forbiddenMatch(const juce::String& text) {
    const auto normal=text.toLowerCase();
    for(const auto& term:forbiddenTerms()) {
        int from=0;
        while(from<normal.length()) {
            const int index=normal.indexOf(from,term);
            if(index<0)break;
            const int end=index+term.length();
            if((index==0 || !word(normal[index-1])) && (end==normal.length() || !word(normal[end])))
                return term;
            from=index+1;
        }
    }
    return {};
}
inline void safeText(const juce::String& text,const juce::String& context) {
    const auto name=forbiddenMatch(text);
    if(name.isNotEmpty())throw std::runtime_error(("Original amp/effect name in "+context+": "+name+" / "+text).toStdString());
}
inline void negativeControls() {
    // An independent injected brand catches regressions even when a caption
    // and its production helper are accidentally changed together.
    if(forbiddenMatch("Selected model: bOsS SD-1").isEmpty()
       || forbiddenMatch("Amp reference: Fender Twin Reverb").isEmpty()
       || forbiddenMatch("board0_m29_boss_drive").isNotEmpty()
       || forbiddenMatch("Mid-forward soft clipping").isNotEmpty())
        throw std::runtime_error("Public-name positive/negative controls failed");
}
inline void catalogPresentation() {
    using namespace spectralforge;
    negativeControls();
    for(int model=0;model<ampModelCount;++model) {
        const auto context="AMP "+juce::String(model);
        safeText(juce::String::fromUTF8(ampInfo(model).name),context+" alias");
        safeText(juce::String::fromUTF8(ampInfo(model).character),context+" caption");
        const auto& panel=ampNativePanel(model);
        for(const auto* channel:panel.channels)safeText(juce::String::fromUTF8(channel),context+" channel");
        for(const auto* route:panel.routes)safeText(juce::String::fromUTF8(route),context+" route");
        for(const auto& control:panel.controls) {
            safeText(juce::String::fromUTF8(control.label),context+" control");
            safeText(juce::String::fromUTF8(control.group),context+" group");
            for(const auto* option:control.options)safeText(juce::String::fromUTF8(option),context+" option");
        }
    }
    for(int model=0;model<pedalModelCount;++model) {
        const auto context="PRE "+juce::String(model);
        const auto& spec=pedalModel(model);
        safeText(juce::String::fromUTF8(spec.name),context+" host model name");
        safeText(juce::String::fromUTF8(pedalMenuName(model)),context+" alias");
        safeText(juce::String::fromUTF8(pedalDescription(model)),context+" caption");
        for(int control=0;control<spec.controlCount;++control) {
            safeText(juce::String::fromUTF8(spec.controls[(size_t)control].label),context+" control");
            safeText(juce::String::fromUTF8(spec.controls[(size_t)control].choices),context+" choices");
        }
    }
    safeText(juce::String::fromUTF8(pedalBoardDSPStatus),"PRE status");
    for(int family=0;family<int(modelFamilies.size());++family)
        for(int model=0;model<effectSelectableCount(family);++model) {
            safeText(juce::String::fromUTF8(modelInfo(family,model).name),"Effect host choice");
            safeText(juce::String::fromUTF8(modelInfo(family,model).character),"Effect caption");
            safeText(juce::String::fromUTF8(effectFamilyMenuName(family,model)),"Effect selector");
        }
    for(int section=0;section<3;++section)for(int model=0;model<postNativeModelsPerSection;++model) {
        const auto& spec=postNativeModel(section,model);
        safeText(juce::String::fromUTF8(spec.name),"POST alias");
        safeText(juce::String::fromUTF8(postNativeDescription(section,model)),"POST caption");
        for(int control=0;control<spec.controlCount;++control) {
            const auto& item=spec.controls[(size_t)control];
            safeText(juce::String::fromUTF8(item.label),"POST control");
            safeText(juce::String::fromUTF8(item.options),"POST choices");
            safeText(juce::String::fromUTF8(item.note),"POST control tooltip");
        }
    }
    for(int preset=0;preset<selectablePresetCount;++preset) {
        safeText(juce::String::fromUTF8(selectablePresetName(preset)),"Factory preset name");
        safeText(juce::String::fromUTF8(selectablePresetDescription(preset)),"Factory preset description");
        const auto summary=juce::String::fromUTF8(selectablePresetSummary(preset));
        safeText(summary,"Factory preset summary");
        if(summary.isEmpty() || summary.length()>=100)throw std::runtime_error("Preset summary must be one short Easy view line");
    }
}
inline void hostPresentation(juce::AudioProcessor& processor) {
    catalogPresentation();
    int parameters=0,choices=0;
    for(auto* parameter:processor.getParameters()) {
        safeText(parameter->getName(512),"actual host parameter name");
        if(const auto* choice=dynamic_cast<juce::AudioParameterChoice*>(parameter))
            for(const auto& value:choice->choices){safeText(value,"actual host parameter choice");++choices;}
        ++parameters;
    }
    std::cout<<"PASS public presentation: catalog captions, options and factory presets; "
             <<parameters<<" actual host parameter names and "<<choices<<" choices; injected-name controls\n";
}
inline void componentText(juce::Component& component,const juce::String& context) {
    safeText(component.getName(),context+" accessibility name");
    if(auto* tooltip=dynamic_cast<juce::TooltipClient*>(&component))safeText(tooltip->getTooltip(),context+" tooltip");
    if(auto* label=dynamic_cast<juce::Label*>(&component))safeText(label->getText(),context+" label");
    if(auto* button=dynamic_cast<juce::Button*>(&component))safeText(button->getButtonText(),context+" button");
    if(auto* combo=dynamic_cast<juce::ComboBox*>(&component)) {
        safeText(combo->getText(),context+" selection");
        for(int item=0;item<combo->getNumItems();++item)safeText(combo->getItemText(item),context+" menu item");
    }
    for(auto* child:component.getChildren())if(child->isVisible())componentText(*child,context);
}
} // namespace publicNameTests

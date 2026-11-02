#pragma once
#include "ModelCatalog.h"
#include "EffectSelectionCatalog.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectralforge {
// Keeps a model ComboBox (item id = model + 1) in step with a released choice
// parameter and its appended extension (docs/POST_MOTION_SPACE_1_8.md).
// JUCE's ComboBoxAttachment maps by item index over the item count, so it
// cannot drive a list longer than the released choice; this replaces it for
// the extended DELAY / REVERB / MODULATION selectors.
class ExtendedEffectModelSync final : private juce::ComboBox::Listener {
public:
    ExtendedEffectModelSync(juce::AudioProcessorValueTreeState& state,int familyIndex,juce::ComboBox& box)
        :family(familyIndex),combo(box) {
        const auto refresh=[this](float){update();};
        choice=std::make_unique<juce::ParameterAttachment>(*state.getParameter(modelFamilies[(size_t)family].parameter),refresh);
        extension=std::make_unique<juce::ParameterAttachment>(*state.getParameter(effectExtensionID(family)),refresh);
        choiceValue=state.getRawParameterValue(modelFamilies[(size_t)family].parameter);extensionValue=state.getRawParameterValue(effectExtensionID(family));
        combo.addListener(this);update();
    }
    ~ExtendedEffectModelSync() override {combo.removeListener(this);}
    int selected() const noexcept {return effectSelectedModel(family,choiceValue->load(),extensionValue->load());}
private:
    void update() {if(!writing)combo.setSelectedId(selected()+1,juce::dontSendNotification);}
    void comboBoxChanged(juce::ComboBox*) override {
        const int chosen=combo.getSelectedId()-1;if(chosen<0 || chosen==selected())return;
        const int count=modelFamilies[(size_t)family].count;
        const juce::ScopedValueSetter<bool> guard(writing,true);
        // Released models write the released choice and clear the extension;
        // appended models write only the extension.
        if(chosen<count){choice->setValueAsCompleteGesture(float(chosen));extension->setValueAsCompleteGesture(0.f);}
        else extension->setValueAsCompleteGesture(float(chosen-count+1));
    }
    int family;juce::ComboBox& combo;bool writing{};
    std::unique_ptr<juce::ParameterAttachment> choice,extension;
    const std::atomic<float>* choiceValue{};const std::atomic<float>* extensionValue{};
};
// Released groups keep their order; the 1.8 models add their own groups.
inline void buildExtendedEffectMenu(juce::PopupMenu& root,int family) {
    const auto names=effectFamilyMenuNames(family);
    if(family==10) {
        const std::array<const char*,8> groups{"Chorus","Phaser","Flanger","Vibrato","Tremolo","Rotary","Vibe","Ring"};
        const auto groupOf=[](int m){return m<2?0:m-1;};   // two chorus models, then one model per group
        for(int group=0;group<int(groups.size());++group){juce::PopupMenu items;for(int m=0;m<names.size();++m)if(groupOf(m)==group)items.addItem(m+1,names[m]);root.addSubMenu(groups[(size_t)group],items);}
        return;
    }
    const std::array<const char*,6> types=family==1?std::array<const char*,6>{"Digital","Tape","Analog","Multi-tap","Modulated","Dual"}
                                                   :std::array<const char*,6>{"Plate","Hall","Spring","Studio hall","Ambient","Gated"};
    for(int m=0;m<names.size() && m<int(types.size());++m){juce::PopupMenu items;items.addItem(m+1,names[m]);root.addSubMenu(types[(size_t)m],items);}
}
} // namespace spectralforge

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AmpCatalog.h"

// Browsing may repeat an amp under several tags, but the ComboBox must retain
// exactly 15 items in the original order: JUCE ComboBoxParameterAttachment
// maps selected ITEM INDEX / (NUM ITEMS - 1), not the displayed popup's item ID.
class AmpSelector final : public juce::ComboBox {
public:
    AmpSelector() {
        addItemList(spectralforge::ampNames(),1);
        setTooltip("Browse by role or instrument. Every lane can select any existing amp; tags do not add hardware channels.");
    }
    juce::PopupMenu browsingMenu() const {
        using namespace spectralforge;
        const auto group=[this](AmpRole role,AmpInstrument instrument) {
            juce::PopupMenu result;
            for(int i=0;i<ampModelCount;++i)if(ampMatches(i,role,instrument)) {
                const auto& info=ampCatalog[static_cast<size_t>(i)];
                result.addItem(i+1,juce::String(info.name)+" / "+info.reference,true,getSelectedId()==i+1);
            }
            return result;
        };
        juce::PopupMenu menu;
        menu.addSectionHeader("CURRENT VOICES / any lane");
        menu.addSubMenu("All 15 amps",group(AmpRole::any,AmpInstrument::any));
        juce::PopupMenu roles;
        for(const auto& item:ampRoleChoices)roles.addSubMenu(item.label,group(item.role,AmpInstrument::any));
        menu.addSubMenu("Role / overlapping tags",roles);
        juce::PopupMenu instruments;
        instruments.addSubMenu("Guitar",group(AmpRole::any,AmpInstrument::guitar));
        instruments.addSubMenu("Bass",group(AmpRole::any,AmpInstrument::bass));
        menu.addSubMenu("Instrument / cross-selection allowed",instruments);
        menu.addSeparator();
        juce::PopupMenu pending;
        for(size_t i=0;i<pendingAmpTargets.size();++i) {
            const auto& target=pendingAmpTargets[i];
            pending.addItem(1001+int(i),juce::String(target.required ? "Required: " : "Candidate: ")+target.name+" / "+target.scope,false,false);
        }
        menu.addSubMenu("Development targets / not selectable",pending);
        return menu;
    }
    bool selectMenuResult(int id) {
        if(spectralforge::ampIndexFromMenuId(id)<0)return false;
        setSelectedId(id,juce::sendNotificationAsync);
        return true;
    }
    void showPopup() override {
        // Delegate lifecycle, keyboard navigation, target deletion and safe
        // asynchronous callback handling to JUCE. It copies this menu before
        // returning; restore the flat host enumeration before any user choice.
        auto* menu=getRootMenu();
        const auto stableMenu=*menu;
        *menu=browsingMenu();
        juce::ComboBox::showPopup();
        *menu=stableMenu;
    }
};

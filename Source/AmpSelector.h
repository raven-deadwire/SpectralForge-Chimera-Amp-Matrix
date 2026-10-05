#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AmpCatalog.h"

// A ComboBox user change is normally delivered asynchronously. Preserve its
// current selection while that notification waits, instead of overwriting it
// with the previous processor value during a timer refresh.
class StableAmpComboBox : public juce::ComboBox {
    int lastSyncedId{};
    bool synced{};
public:
    void syncSelectedId(int id) {
        if(synced && getSelectedId()!=lastSyncedId)return;
        resetSyncExplicit(id);
    }
    void acceptSelection() { lastSyncedId=getSelectedId();synced=true; }
    // Model-bank replacement deliberately discards an older bank's pending UI
    // choice and installs the new bank's actual processor value.
    void resetSyncExplicit(int id) {
        setSelectedId(id,juce::dontSendNotification);
        acceptSelection();
    }
};

// UI model IDs are independent of the preserved 15-choice legacy host parameter.
// The processor explicitly routes a choice to its legacy or extension bank.
class AmpSelector final : public StableAmpComboBox {
public:
    AmpSelector() {
        addItemList(spectralforge::ampNames(),1);
        setItemEnabled(spectralforge::legacyHiddenAmpIndex+1,false);
        setTooltip("Select a Chimera amplifier. Every head is available on any lane.");
    }
    juce::PopupMenu browsingMenu() const {
        using namespace spectralforge;
        juce::PopupMenu menu;
        for(const auto& category:ampRoleChoices) {
            juce::PopupMenu group;
            for(int i=0;i<ampModelCount;++i)if(ampIsActive(i) && ampPrimaryRoles[(size_t)i]==category.role)
                group.addItem(i+1,juce::String::fromUTF8(ampInfo(i).name),true,getSelectedId()==i+1);
            menu.addSubMenu(category.label,group);
        }
        return menu;
    }
    bool selectMenuResult(int id) {
        if(!spectralforge::ampIsActive(spectralforge::ampIndexFromMenuId(id)))return false;
        setSelectedId(id,juce::sendNotificationSync);
        return true;
    }
    void showPopup() override {
        if(!isEnabled())return;
        const juce::Component::SafePointer<AmpSelector> safe(this);
        browsingMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int id) {
            if(!safe)return;
            // The base mouse/keyboard path marks its popup active before calling
            // this override. Clear that flag after both selection and cancel.
            safe->hidePopup();
            if(safe && id>0)safe->selectMenuResult(id);
        });
    }
};

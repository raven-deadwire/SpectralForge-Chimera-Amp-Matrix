#pragma once
#include "AmpSelector.h"
#include <array>
#include <stdexcept>
#include <iostream>
namespace ampSelectorTests {
inline void run() {
    using namespace spectralforge;
    const auto check=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    AmpSelector selector;check(selector.getNumItems()==ampModelCount,"Amp selector inventory incomplete");
    selector.setSelectedId(8,juce::dontSendNotification);
    auto menu=selector.browsingMenu();std::array<int,ampModelCount> found{};int ticked=0;
    for(juce::PopupMenu::MenuItemIterator it(menu,true);it.next();) {
        const auto& item=it.getItem();
        if(item.itemID==0)continue;
        check(item.itemID>=1&&item.itemID<=ampModelCount,"Non-product target in amp selector");
        check(item.isEnabled,"Selectable DSP unexpectedly disabled");
        check(item.text==juce::String::fromUTF8(ampInfo(item.itemID-1).name),"Selector exposes reference or extra name");
        for(const auto& info:ampCatalog)check(!item.text.containsIgnoreCase(info.reference),"Original amplifier name exposed");
        ++found[(size_t)item.itemID-1];
        if(item.isTicked){check(item.itemID==8,"Wrong amp ticked");++ticked;}
    }
    for(int model=0;model<ampModelCount;++model)check(found[(size_t)model]==(ampIsActive(model)?1:0),"Active amp duplicated/missing or legacy amp offered");
    check(!selector.selectMenuResult(17),"Legacy Ironball offered for new selection");
    check(ticked==1,"Selected amp duplicated across categories");
    check(!selector.selectMenuResult(1001)&&selector.getSelectedId()==8,"Invalid result changed selection");
    check(selector.selectMenuResult(24)&&selector.getSelectedId()==24,"E670FE not selectable");
    check(selector.selectMenuResult(niflheimrAmpModel+1)&&selector.getSelectedId()==niflheimrAmpModel+1,"Niflheimr not selectable");
    // Reproduce a timer refresh between the user changing the displayed value
    // and JUCE delivering its pending onChange callback.
    const auto pendingSelection=[&check](StableAmpComboBox& control,bool keyboard) {
        int delivered=0;
        control.resetSyncExplicit(1);
        control.onChange=[&]{control.acceptSelection();delivered=control.getSelectedId();};
        if(keyboard)control.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
        else control.setSelectedId(2,juce::sendNotificationAsync);
        control.syncSelectedId(1);
        check(control.getSelectedId()==2,"Timer refresh erased a pending user amp/channel selection");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        check(delivered==2,"Pending amp/channel callback received the old processor selection");
        control.syncSelectedId(1);
        check(control.getSelectedId()==1,"Accepted selection blocked a later host/state refresh");
        control.onChange=nullptr;
    };
    AmpSelector pendingAmp;pendingSelection(pendingAmp,true);
    StableAmpComboBox pendingChannel;pendingChannel.addItem("A",1);pendingChannel.addItem("B",2);
    pendingSelection(pendingChannel,false);
    pendingChannel.clear(juce::dontSendNotification);pendingChannel.addItem("New bank",7);
    pendingChannel.resetSyncExplicit(7);pendingChannel.syncSelectedId(7);
    check(pendingChannel.getSelectedId()==7,"Explicit model-bank replacement retained an obsolete UI selection");

    // Enter through the real base keyboard path, which sets ComboBox's private
    // menuActive flag. Cancellation must clear it so the next open can work.
    AmpSelector popup;
    popup.setBounds(40,40,300,30);
    popup.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    popup.setVisible(true);
    for(int attempt=0;attempt<2;++attempt) {
        popup.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        check(popup.isPopupActive(),"Amp popup did not open or reopen through the user input path");
        juce::PopupMenu::dismissAllActiveMenus();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        check(!popup.isPopupActive(),"Amp popup cancellation left the base menu-active flag stuck");
    }
    popup.removeFromDesktop();
    std::cout<<"PASS: 25 active amps exactly once, Chimera names only, new choices selectable; pending asynchronous amp/channel selection retained; popup cancellation and reopening\n";
}
}

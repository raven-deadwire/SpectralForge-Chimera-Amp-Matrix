#pragma once
#include "AmpSelector.h"
#include <array>
#include <stdexcept>
#include <iostream>
namespace ampSelectorTests {
inline void run() {
    const auto check=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    AmpSelector selector;
    check(selector.getNumItems()==15,"Amp browse tags changed host item count");
    for(int i=0;i<15;++i) {
        check(selector.getItemId(i)==i+1,"Amp browse changed host item IDs");
        check(selector.getItemText(i)==spectralforge::ampCatalog[size_t(i)].name,"Amp browse changed original item labels");
    }
    selector.setSelectedId(8,juce::dontSendNotification);
    auto menu=selector.browsingMenu();std::array<int,15> found{};int pending=0,ticked=0;
    for(juce::PopupMenu::MenuItemIterator it(menu,true);it.next();) {
        const auto& item=it.getItem();
        if(item.itemID>=1&&item.itemID<=15) {check(item.isEnabled,"Existing amp unexpectedly disabled");++found[size_t(item.itemID-1)];if(item.isTicked){check(item.itemID==8,"Wrong tagged amp marked selected");++ticked;}}
        if(item.itemID>=1001&&item.itemID<=1008) {check(!item.isEnabled,"Unimplemented amp target is selectable");++pending;}
    }
    for(int count:found)check(count>=3,"Amp missing from all/instrument/role menus");
    check(ticked>=3&&pending==8,"Tagged selected amp or pending count missing");
    check(selector.getNumItems()==15&&selector.getSelectedItemIndex()==7,"Building popup changed normalised host mapping");
    check(!selector.selectMenuResult(1001)&&selector.getSelectedId()==8,"Pending menu result changed audio model");
    check(selector.selectMenuResult(15)&&selector.getSelectedItemIndex()==14,"Known menu selection failed");
    std::cout<<"PASS: tagged popup retains flat 15-value automation, cross-selection, and eight disabled targets\n";
}
}

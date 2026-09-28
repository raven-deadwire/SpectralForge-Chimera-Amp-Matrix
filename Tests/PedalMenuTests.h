#pragma once
#include "PedalBoardPanel.h"
#include <array>
#include <iostream>
#include <set>
#include <stdexcept>

namespace pedalMenuTests {
inline void run() {
    using namespace spectralforge;
    const auto check=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    const auto settle=[] {juce::MessageManager::getInstance()->runDispatchLoopUntil(40);};
    PedalSelector selector;
    check(selector.getNumItems()==pedalModelCount,"Pedal selector inventory incomplete");
    selector.resetSyncExplicit(30);
    const auto menu=selector.browsingMenu();std::array<int,pedalModelCount> found{};
    int ticked=0,groups=0;
    for(juce::PopupMenu::MenuItemIterator it(menu,false);it.next();)
        if(it.getItem().subMenu!=nullptr)++groups;
    check(groups==10,"Pedals are not grouped into the ten exclusive effect types");
    for(juce::PopupMenu::MenuItemIterator it(menu,true);it.next();) {
        const auto& item=it.getItem();if(item.itemID==0)continue;
        check(item.itemID>=1 && item.itemID<=pedalModelCount,"Pedal menu has a non-product target");
        check(item.isEnabled,"Implemented pedal disabled in selector");
        check(item.text==pedalMenuName(item.itemID-1),"Pedal selector exposes a hardware name");
        for(const auto* brand:{"Ibanez","Klon","Pro Co","SansAmp","Darkglass","MXR","Origin Effects",
                               "Manley","EHX","Mu-Tron","Boss","Dunlop","Sola Sound","ZVEX","Xotic",
                               "Dallas","SD-1","OCD","M104","JB-2"})
            check(!item.text.containsIgnoreCase(brand),"Pedal popup contains original hardware branding");
        ++found[(size_t)item.itemID-1];if(item.isTicked){++ticked;check(item.itemID==30,"Wrong pedal menu item ticked");}
    }
    for(int count:found)check(count==1,"Pedal missing or duplicated across menu categories");
    check(ticked==1,"Selected pedal repeated across categories");
    check(!selector.selectMenuResult(0) && !selector.selectMenuResult(41),"Invalid pedal menu result accepted");
    for(int family=0;family<int(modelFamilies.size());++family) {
        const auto names=effectFamilyMenuNames(family);
        check(names.size()==modelFamilies[(size_t)family].count,"Rack effect alias inventory incomplete");
        for(int model=0;model<names.size();++model)
            check(!names[model].containsIgnoreCase(modelInfo(family,model).reference),"Rack selector exposes original reference");
    }

    // A timer refresh between keyboard input and JUCE's asynchronous callback
    // must not discard the selected pedal or restore an old model bank.
    selector.resetSyncExplicit(1);int delivered=0;
    selector.onChange=[&]{selector.acceptSelection();delivered=selector.getSelectedId();};
    selector.keyPressed(juce::KeyPress(juce::KeyPress::downKey));selector.syncSelectedId(1);
    check(selector.getSelectedId()==2,"Pedal timer refresh discarded pending keyboard selection");
    settle();check(delivered==2,"Pedal callback received stale model");
    selector.syncSelectedId(3);check(selector.getSelectedId()==3,"Later host selection blocked by menu sync");
    selector.onChange=nullptr;
    selector.setBounds(40,40,250,30);selector.addToDesktop(juce::ComponentPeer::windowIsTemporary);selector.setVisible(true);
    for(int attempt=0;attempt<2;++attempt) {
        selector.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));settle();
        check(selector.isPopupActive(),"Pedal popup cannot open or reopen");
        juce::PopupMenu::dismissAllActiveMenus();settle();
        check(!selector.isPopupActive(),"Pedal popup cancel retained menu-active state");
    }
    selector.removeFromDesktop();

    const auto storage=std::make_unique<ChimeraProcessor>();auto& processor=*storage;
    PedalBoardPanel board(processor);board.setBounds(0,0,1140,416);board.setVisible(true);
    auto* first=dynamic_cast<PedalSelector*>(board.findChildWithID("boardModelAt0"));
    check(first!=nullptr && first->isEnabled(),"Board model selector missing or disabled");
    check(first->selectMenuResult(30),"Cannot select Dual Circuit through real board control");
    check(processor.pedalBoardState().instances[0].model==29,"Pedal selection waits for approval before loading DSP");
    check(juce::Component::getNumCurrentlyModalComponents()==0,"Pedal selection opened a parameter-bank alert");
    auto* power=dynamic_cast<juce::TextButton*>(board.findChildWithID("boardBypassAt0"));
    check(power!=nullptr && power->getButtonText()=="ON","Active pedal does not expose explicit ON state");
    power->triggerClick();settle();
    check(power->getButtonText()=="OFF" && processor.pedalBoardState().instances[0].bypass,
          "Pedal OFF button does not bypass the actual selected instance");
    power->triggerClick();settle();
    check(power->getButtonText()=="ON" && !processor.pedalBoardState().instances[0].bypass,
          "Pedal ON button did not restore the selected instance");
    auto* copy=dynamic_cast<juce::Button*>(board.findChildWithID("boardCopyAt0"));
    check(copy!=nullptr && copy->isEnabled(),"Copy control unavailable with empty destination slot");
    copy->triggerClick();settle();
    const auto copied=processor.pedalBoardState();int duplicates=0;
    for(const auto& instance:copied.instances)if(instance.model==29)++duplicates;
    check(duplicates==2,"Pedal copy did not immediately create independent destination instance");
    check(juce::Component::getNumCurrentlyModalComponents()==0,"Pedal copy opened a parameter-bank alert");
    auto* detail=dynamic_cast<juce::Button*>(board.findChildWithID("boardDetailAt0"));
    check(detail!=nullptr,"Stable pedal detail button ID missing");detail->triggerClick();settle();
    auto* close=dynamic_cast<juce::Button*>(board.findChildWithID("boardDetailClose"));
    auto* title=dynamic_cast<juce::Label*>(board.findChildWithID("boardDetailTitle"));
    check(close!=nullptr && close->isVisible() && title!=nullptr && title->getText()=="Dual Circuit",
          "Pedal detail view did not open the selected product controls");
    for(auto* child:board.getChildren())if(child!=close && child->isVisible())
        check(!close->getBounds().intersects(child->getBounds()),"Return button overlaps a visible pedal-detail control");
    close->triggerClick();settle();
    check(first->isVisible() && !close->isVisible(),"Pedal detail view did not return to five slots");

    // Every four-control pedal uses two columns and two rows on its card and
    // detailed view, independently of which family supplies the DSP.
    for(int model=1;model<pedalModelCount;++model)if(pedalModel(model).controlCount==4) {
        first->selectMenuResult(model+1);settle();
        const auto layout=[&] {
            std::set<int> x,y;int count=0;
            for(auto* child:board.getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider && slider->isVisible())
                for(int control=0;control<4;++control)if(slider->getComponentID()==pedalControlID(0,model,control)) {
                    ++count;x.insert(slider->getX());y.insert(slider->getY());
                    check(board.getLocalBounds().contains(slider->getBounds()),"Four-control pedal clips outside its board");
                }
            check(count==4 && x.size()==2 && y.size()==2,"Four-control pedal is not laid out in a 2 by 2 grid");
        };
        layout();detail->triggerClick();settle();layout();
        auto* detailPower=dynamic_cast<juce::TextButton*>(board.findChildWithID("boardDetailBypass"));
        check(detailPower && detailPower->isVisible() && detailPower->getButtonText()=="ON","Detail view is missing ON/OFF control");
        detailPower->triggerClick();settle();
        check(detailPower->getButtonText()=="OFF" && processor.pedalBoardState().instances[0].bypass,"Detail OFF does not bypass pedal DSP");
        detailPower->triggerClick();settle();close->triggerClick();settle();
    }
    // State/automation updates must not activate the compatibility audio path;
    // pressing an actual control key does activate it.
    first->selectMenuResult(28);settle();processor.setPedalBoardEnabled(false);
    const auto id=pedalControlID(0,27,0);auto* parameter=processor.parameters().getParameter(id);
    check(parameter!=nullptr,"Pedal parameter missing from product state");parameter->setValueNotifyingHost(.25f);settle();
    check(!processor.pedalBoardState().enabled,"APVTS notification unexpectedly activated the new board");
    juce::Slider* activeControl=nullptr;
    for(auto* child:board.getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child);slider && slider->isVisible() && slider->getComponentID()==id)activeControl=slider;
    check(activeControl && activeControl->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),"Pedal keyboard edit was not accepted");
    check(processor.pedalBoardState().enabled,"A deliberate keyboard pedal edit did not activate its audio path");
    std::cout<<"PASS pedal menus: 39 DSPs in 10 exclusive categories, aliases only, async selection and popup reopen, immediate load/copy without dialogs, ON/OFF, four-control 2 by 2 layouts and user-only board activation\n";
}
}

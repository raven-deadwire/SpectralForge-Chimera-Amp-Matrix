#pragma once
#include "PluginEditor.h"
#include <stdexcept>

namespace rigViewTests {
// Use only for tests of the amplifier knobs/metadata. The product RIGS page
// deliberately opens its room in Dual/Matrix; performance/default-view tests
// should continue to exercise that room instead of calling this helper.
inline void showControls(juce::Component& canvas) {
    auto* panel=canvas.findChildWithID("ampNativePanel1");
    auto* toggle=dynamic_cast<juce::TextButton*>(canvas.findChildWithID("cabRigControls"));
    if(!panel || !toggle)throw std::runtime_error("Missing rig controls navigation");
    if(panel->isVisible())return;
    if(!toggle->isVisible() || !toggle->isEnabled() || toggle->getButtonText()!="RIG CONTROLS")
        throw std::runtime_error("RIG CONTROLS is unavailable from the cabinet room");
    toggle->triggerClick();
    const auto started=juce::Time::getMillisecondCounterHiRes();
    while(!panel->isVisible()) {
        if(juce::Time::getMillisecondCounterHiRes()-started>2000
            || !juce::MessageManager::getInstance()->runDispatchLoopUntil(10))
            throw std::runtime_error("RIG CONTROLS did not reveal the real amplifier controls");
    }
    auto* room=canvas.findChildWithID("cabRoomOverview");
    if(!room || room->isVisible() || toggle->getButtonText()!="CABINET ROOM")
        throw std::runtime_error("Rig controls and cabinet room navigation disagree");
}
}

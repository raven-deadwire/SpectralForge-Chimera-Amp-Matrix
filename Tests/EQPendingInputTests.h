#pragma once
#include "GraphicalEQPanel.h"
#include "PluginProcessor.h"
#include "NativeMessageLoop.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace eqPendingInputTests {
inline void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class T>
T* component(juce::Component& parent, const juce::String& id) {
    if (parent.getComponentID() == id) return dynamic_cast<T*>(&parent);
    for (auto* child : parent.getChildren())
        if (auto* found = component<T>(*child, id)) return found;
    return nullptr;
}

inline juce::TextEditor* textEditor(juce::Component& parent) {
    if (auto* input = dynamic_cast<juce::TextEditor*>(&parent)) return input;
    for (auto* child : parent.getChildren())
        if (auto* found = textEditor(*child)) return found;
    return nullptr;
}

inline void typeUncommitted(juce::Slider& slider, const juce::String& text) {
    slider.showTextBox();
    auto* input = textEditor(slider);
    require(input != nullptr, "Pending-input regression did not open a native numeric TextEditor");
    input->setText(text, false);
}

inline void writeHost(ChimeraProcessor& processor, int bank, int band, float value) {
    auto* parameter = processor.parameters().getParameter(spectralforge::eqID(bank, band, "frequency"));
    require(parameter != nullptr, "Pending-input fixture parameter missing");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

inline float readHost(ChimeraProcessor& processor, int bank, int band) {
    return processor.parameters().getRawParameterValue(spectralforge::eqID(bank, band, "frequency"))->load();
}

// Call immediately AFTER editingWorkflow() for this bank, while the native
// panel is visible and both Band 1 and Band 2 are occupied. No event dispatch
// is permitted between the invalidating operation and the stale commit: that
// boundary is the regression this test exercises.
inline void run(ChimeraProcessor& processor, spectralforge::GraphicalEQPanel& panel, int bank) {
    using spectralforge::eqID;
    require(panel.isShowing() && panel.isBandVisible(0) && panel.isBandVisible(1),
            "Pending-input regression requires two visible persistent slots");
    auto* selector = component<juce::ComboBox>(panel, eqID(bank, -1, "band"));
    require(selector != nullptr, "Pending-input band selector missing");
    const int previousSelection = selector->getSelectedId();
    const float firstValue = readHost(processor, bank, 0), secondValue = readHost(processor, bank, 1);
    writeHost(processor, bank, 0, 1000.f); writeHost(processor, bank, 1, 1000.f);
    require(chimeraTest::dispatchFor(60), "Pending-input fixture message loop exited");
    selector->setSelectedId(1, juce::sendNotificationSync);
    auto* frequency = component<juce::Slider>(panel, eqID(bank, 0, "frequency"));
    require(frequency != nullptr, "Pending-input numeric control missing");
    typeUncommitted(*frequency, "3210 Hz");
    selector->setSelectedId(2, juce::sendNotificationSync);
    // Same underlying Slider, new target. Its own value is still 1000 Hz, so
    // JUCE's ordinary setValue() would not automatically hide the old editor.
    require(textEditor(*frequency) == nullptr, "Changing equal-valued slots retained the old slot's pending input");
    frequency->hideTextBox(false);
    require(readHost(processor, bank, 0) == 1000.f && readHost(processor, bank, 1) == 1000.f,
            "Pending text was committed to the newly selected equal-valued slot");
    require(panel.selectedBand() == 1, "Pending-input slot switch selected the wrong persistent band");

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    typeUncommitted(*frequency, "4567 Hz");
    processor.setStateInformation(saved.getData(), int(saved.getSize()));
    // Simulate Return/focus loss before the panel's 25 Hz timer can observe the
    // recall. Equal parameter values must still invalidate this old text edit.
    frequency->hideTextBox(false);
    require(readHost(processor, bank, 1) == 1000.f && !processor.eqEditing().canUndo(),
            "Same-values project recall accepted a stale numeric commit before the UI timer");
    require(chimeraTest::dispatchFor(60), "Pending-input recall message loop exited");
    require(std::abs(frequency->getValue() - 1000.) < .02, "Rejected stale input left a false displayed frequency");

    typeUncommitted(*frequency, "5678 Hz");
    writeHost(processor, bank, 1, 1000.f); // A same-value host notification is an ownership boundary.
    frequency->hideTextBox(false);
    require(readHost(processor, bank, 1) == 1000.f && !processor.eqEditing().canUndo(),
            "Same-value host automation accepted a stale pending numeric commit");
    require(chimeraTest::dispatchFor(60), "Pending-input automation message loop exited");
    typeUncommitted(*frequency, "1530 Hz");
    frequency->hideTextBox(false);
    require(std::abs(readHost(processor, bank, 1) - 1530.f) < .02f && processor.eqEditing().canUndo(),
            "A fresh numeric entry remained blocked after stale input cancellation");
    require(processor.eqEditing().undo() && std::abs(readHost(processor, bank, 1) - 1000.f) < .02f,
            "Fresh post-recall numeric input was not one reversible edit");

    writeHost(processor, bank, 0, firstValue); writeHost(processor, bank, 1, secondValue);
    selector->setSelectedId(previousSelection, juce::sendNotificationSync);
    require(chimeraTest::dispatchFor(60), "Pending-input cleanup message loop exited");
    std::cout << "PASS native pending numeric input / equal-valued slot switch / same-values project+host ownership / immediate stale commit rejected bank=" << bank << '\n';
}
}

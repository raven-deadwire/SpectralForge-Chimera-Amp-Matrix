#pragma once
#include "PluginProcessor.h"
#include "EffectSelectionCatalog.h"
#include "EffectModelSync.h"
#include "HardwareArtwork.h"
#include "KnobInteraction.h"
#include "PostSlots.h"
#include <functional>

// POST second slots on the POST page (docs/POST_SECOND_SLOTS.md).
//  - SlotSelector: the "1 2 3 4" switch on every rack row and on the CREATIVE
//    strip (four slots since docs/POST_FOUR_SLOTS.md). It only chooses which
//    slot the row shows; it writes nothing. A number lights green while that
//    slot is on, so an active slot is visible while another is shown.
//  - EffectSlotRow: the MODULATION / DELAY / REVERB row of slot 2-4. It draws
//    and behaves like the released row (model, ON, ALL, three knobs, meter;
//    DELAY adds SYNC) on the slot's prefixed parameters.
namespace spectralforge::postSlotUI {
inline const juce::Colour ink(0xffe1dbce), muted(0xffa4a79f), line(0xff3a3a34), panel(0xff171b1b), background(0xff0d1010), lit(0xffa5c1a0);

class SlotSelector final : public juce::Component {
public:
    SlotSelector(const juce::String& id, const juce::String& section) {
        setComponentID(id);setTitle(section + " slot");
        for (int i = 0; i < postSlotCount; ++i) {
            auto& b = buttons[size_t(i)];addAndMakeVisible(b);
            b.setButtonText(juce::String(i + 1));b.setComponentID(id + "." + juce::String(i + 1));
            b.setClickingTogglesState(false);b.setRadioGroupId(1);
            b.setTooltip(i == 0 ? "Show " + section + " slot 1" : "Show " + section + " slot " + juce::String(i + 1) + " (another independent " + section + "). A green light: that slot is on.");
            b.setConnectedEdges((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < postSlotCount - 1 ? juce::Button::ConnectedOnRight : 0));
            b.onClick = [this, i] { select(i, true); };
        }
        select(0, false);
    }
    std::function<void(int)> onChange;
    int shown() const noexcept { return current; }
    void select(int slot, bool notify) {
        current = juce::jlimit(0, postSlotCount - 1, slot);
        for (int i = 0; i < postSlotCount; ++i) buttons[size_t(i)].setToggleState(i == current, juce::dontSendNotification);
        if (notify && onChange) onChange(current);
    }
    // Lights a slot's number while that slot (2-4) is switched on.
    void setSlotActive(int slot, bool active) {
        if (slot < 1 || slot >= postSlotCount || active == lights[size_t(slot)]) return;
        lights[size_t(slot)] = active;repaint();
    }
    void setSecondActive(bool active) { setSlotActive(1, active); }
    void paintOverChildren(juce::Graphics& g) override {
        for (int i = 1; i < postSlotCount; ++i) if (lights[size_t(i)]) {   // a small green light in the corner of the number
            const auto r = buttons[size_t(i)].getBounds().toFloat();
            g.setColour(juce::Colour(0xff0d1010));g.fillEllipse(r.getRight() - 7.f, r.getY() + 1.5f, 5.5f, 5.5f);
            g.setColour(lit);g.fillEllipse(r.getRight() - 6.f, r.getY() + 2.5f, 3.5f, 3.5f);
        }
    }
    bool slotShownActive(int slot) const noexcept { return slot >= 1 && slot < postSlotCount && lights[size_t(slot)]; }
    bool secondShownActive() const noexcept { return lights[1]; }
    juce::TextButton& button(int i) noexcept { return buttons[size_t(i)]; }
    void resized() override { auto r = getLocalBounds();const int w = r.getWidth() / postSlotCount;for (int i = 0; i < postSlotCount; ++i) buttons[size_t(i)].setBounds(i == postSlotCount - 1 ? r : r.removeFromLeft(w)); }
private:
    std::array<juce::TextButton, postSlotCount> buttons;int current{};std::array<bool, postSlotCount> lights{};
};

// The MODULATION (family 10), DELAY (1) or REVERB (2) row of slot 2, 3 or 4, 1140 x 64.
class EffectSlotRow final : public juce::Component, private juce::Timer {
public:
    EffectSlotRow(ChimeraProcessor& owner, int familyIndex, int slotIndex = 1) : processor(owner), family(familyIndex), slot(slotIndex) {
        setComponentID("postSlotRow" + juce::String(family) + postSlotSuffix(slot));
        const auto id = [this](const juce::String& x) { return postSlotID(slot, x); };
        header.setText(juce::String(family == 10 ? "MODULATION" : family == 1 ? "DELAY" : "REVERB") + postSlotNumber(slot), juce::dontSendNotification);
        header.setFont(juce::FontOptions(15.f));header.setColour(juce::Label::textColourId, ink);
        scope.setText("SLOT " + juce::String(slot + 1) + " / AFTER RIG MERGE", juce::dontSendNotification);scope.setFont(juce::FontOptions(10.f));scope.setColour(juce::Label::textColourId, muted);
        for (auto* label : {&header, &scope}) addAndMakeVisible(*label);
        model.setName(juce::String(modelFamilies[size_t(family)].category) + postSlotNumber(slot) + " model");model.setComponentID(id(modelFamilies[size_t(family)].parameter));
        buildExtendedEffectMenu(*model.getRootMenu(), family);addAndMakeVisible(model);
        modelSync = std::make_unique<ExtendedEffectModelSync>(processor.parameters(), family, model, slot);
        model.onChange = [this] { refreshStyle(); };
        const juce::String on = family == 10 ? "choruson" : family == 1 ? "delayon" : "reverbon";
        enabled.setComponentID(id(on));enabled.setClickingTogglesState(true);addAndMakeVisible(enabled);
        enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters(), id(on), enabled);
        expand.setButtonText("ALL");expand.setComponentID("postExpand" + juce::String(family == 10 ? 3 : family == 1 ? 4 : 5) + postSlotSuffix(slot));expand.setTooltip("Open all rack controls");addAndMakeVisible(expand);
        expand.onClick = [this] { if (onOpen) onOpen(); };
        const std::array<const char*, 3> ids = family == 10 ? std::array<const char*, 3>{"chorusrate", "chorusdepth", "chorusmix"} : family == 1 ? std::array<const char*, 3>{"delaytime", "delayfeedback", "delaymix"} : std::array<const char*, 3>{"reverbsize", "reverbdamping", "reverbmix"};
        const std::array<const char*, 3> names = family == 10 ? std::array<const char*, 3>{"RATE", "DEPTH", "MIX"} : family == 1 ? std::array<const char*, 3>{"TIME", "FEEDBACK", "MIX"} : std::array<const char*, 3>{"SIZE", "DAMPING", "MIX"};
        const std::array<const char*, 3> suffix = family == 10 ? std::array<const char*, 3>{" Hz", "", ""} : family == 1 ? std::array<const char*, 3>{" ms", "", ""} : std::array<const char*, 3>{"", "", ""};
        for (size_t k = 0; k < 3; ++k) {
            auto& label = labels[k];auto& slider = controls[k];
            label.setText(names[k], juce::dontSendNotification);label.setFont(juce::FontOptions(10.f));label.setJustificationType(juce::Justification::centred);label.setColour(juce::Label::textColourId, ink);
            slider.setName(names[k]);slider.setTooltip(juce::String(names[k]) + " (slot " + juce::String(slot + 1) + ")");slider.setScrollWheelEnabled(false);slider.setTextValueSuffix(suffix[k]);slider.setNumDecimalPlacesToDisplay(family == 10 && k == 0 ? 2 : 1);
            slider.setComponentID(id(ids[k]));ui::configureRotaryKnob(slider);slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 22);
            slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible(label);addAndMakeVisible(slider);
            attachments[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters(), id(ids[k]), slider);
        }
        if (family == 1) {
            sync.setButtonText("SYNC");sync.setComponentID(id("delaysync"));sync.setClickingTogglesState(true);sync.setTooltip("Quarter-note delay from TAP/manual tempo or HOST BPM.");addAndMakeVisible(sync);
            syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters(), id("delaysync"), sync);
            controls[0].textFromValueFunction = [this](double value) { return juce::String(sync.getToggleState() ? 60000.0 / processor.currentTempo() : value, 1); };
            controls[0].updateText();
        }
        enabled.onStateChange = [this] { const auto text = enabled.getToggleState() ? "ON" : "OFF";if (enabled.getButtonText() != text) { enabled.setButtonText(text);repaint(0, 0, 60, 64); } };
        enabled.setButtonText(enabled.getToggleState() ? "ON" : "OFF");
        refreshStyle();timerCallback();startTimerHz(15);
    }
    ~EffectSlotRow() override { stopTimer(); }
    std::function<void()> onOpen;
    int selectedModel() const { return juce::jmax(0, model.getSelectedId() - 1); }
    void paint(juce::Graphics& g) override {
        const float w = float(getWidth());const int m = selectedModel();
        g.setColour(panel.withAlpha(.9f));g.fillRoundedRectangle(0, 0, w, 64, 4);
        art::rack(g, {276, 0, w - 276, 64}, family, m);
        g.setColour(juce::Colour(modelInfo(family, m).colour).withAlpha(.85f));g.fillRect(8.f, 8.f, 2.f, 46.f);
        g.setColour(enabled.getToggleState() ? lit : line);g.fillEllipse(16, 26, 5, 5);
        const float level = processor.postModuleLevel(family == 10 ? 3 : family == 1 ? 4 : 5, slot);
        g.setColour(background.withAlpha(.88f));g.fillRoundedRectangle(298, 16, 112, 32, 3);
        g.setColour(ink);g.setFont(juce::FontOptions(8.5f));
        g.drawText("OUT  " + juce::String(juce::Decibels::gainToDecibels(level, -60.f), 1) + " dBFS", 302, 17, 104, 14, juce::Justification::centred);
        const float amount = juce::jlimit(0.f, 1.f, (juce::Decibels::gainToDecibels(level, -60.f) + 60.f) / 60.f);
        for (int segment = 0; segment < 16; ++segment) { g.setColour(segment < float(amount * 16) ? (level >= 1 ? juce::Colour(0xffe49b73) : lit) : line);g.fillRect(306.f + segment * 6, 35.f, 4.f, 5.f); }
    }
    void resized() override {
        header.setBounds(28, 2, 116, 19);model.setBounds(28, 23, 182, 24);scope.setBounds(28, 47, 182, 15);
        enabled.setBounds(221, 13, 43, 25);expand.setBounds(221, 42, 43, 19);sync.setBounds(277, 1, 47, 15);
        const int cell = 692 / 3;
        for (int k = 0; k < 3; ++k) {
            const int controlWidth = juce::jmin(144, cell - 7), x = 438 + k * cell + (cell - controlWidth) / 2;
            labels[size_t(k)].setBounds(x, 2, controlWidth, 16);controls[size_t(k)].setBounds(x, 17, controlWidth, 45);
        }
    }
private:
    ChimeraProcessor& processor;int family{}, slot{1};int shownModel{-1};
    juce::Label header, scope;juce::ComboBox model;juce::TextButton enabled{"OFF"}, expand, sync;
    std::array<juce::Slider, 3> controls;std::array<juce::Label, 3> labels;
    std::unique_ptr<ExtendedEffectModelSync> modelSync;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment, syncAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> attachments;
    float shownLevel{-1};bool shownOn{};
    void refreshStyle() {
        const int m = selectedModel();if (m == shownModel) return;shownModel = m;
        const auto hardware = art::rackStyle(family, m);
        model.setTooltip(juce::String(effectFamilyMenuName(family, m)) + "\n" + juce::String(modelInfo(family, m).character) + "\nSlot " + juce::String(slot + 1) + ", applied after rig merge.");
        for (size_t k = 0; k < controls.size(); ++k) {
            int knob = hardware.knobStyle;
            if (knob == 8) knob = k == 0 ? 6 : 5;
            else if (knob == 5) knob = k % 3 == 0 ? 5 : k % 3 == 1 ? 7 : 0;
            controls[k].getProperties().set("knobStyle", knob);controls[k].getProperties().set("brightFace", hardware.brightFace);
            controls[k].setColour(juce::Slider::textBoxTextColourId, hardware.brightFace ? juce::Colour(0xff202923) : ink);
            controls[k].setColour(juce::Slider::textBoxBackgroundColourId, hardware.brightFace ? juce::Colour(0xffeae5d9).withAlpha(.67f) : background.withAlpha(.8f));
            labels[k].setColour(juce::Label::textColourId, hardware.brightFace ? juce::Colour(0xff202923) : ink);
            labels[k].setColour(juce::Label::backgroundColourId, hardware.brightFace ? juce::Colour(0xffeae5d9).withAlpha(.90f) : background.withAlpha(.88f));
        }
        repaint();
    }
    void timerCallback() override {
        if (!isShowing()) return;
        refreshStyle();
        if (family == 1) { controls[0].setEnabled(!sync.getToggleState());controls[0].updateText(); }
        const float level = processor.postModuleLevel(family == 10 ? 3 : family == 1 ? 4 : 5, slot);
        const float shown = std::round(juce::Decibels::gainToDecibels(level, -60.f) * 10.f);
        if (shown != shownLevel || enabled.getToggleState() != shownOn) { shownLevel = shown;shownOn = enabled.getToggleState();repaint(0, 0, 420, 64); }
    }
};
} // namespace spectralforge::postSlotUI

#pragma once

#include "PluginProcessor.h"
#include <array>
#include <memory>

namespace spectralforge::cabRoom {

// Room Core controls on the cabinet room surface (docs/ROOM_CORE_1_3_3.md):
// one shared room (on/off, size, absorption, return) and a send per rig that
// exists in the current routing mode. Each control is attached to its host
// parameter; showing or hiding the strip changes no audio setting.
class RoomControls : public juce::Component {
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    struct Knob {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SA> attachment;
    };
    ChimeraProcessor& processor;
    juce::TextButton power;
    juce::ComboBox size;
    std::unique_ptr<BA> powerAttachment;
    std::unique_ptr<CA> sizeAttachment;
    Knob absorption, level;
    std::array<Knob, roomLaneCount> sends;
    int visibleSends{1};

    void setUp(Knob& knob, const juce::String& id, const juce::String& text, const juce::String& parameter,
               const juce::String& tooltip) {
        knob.label.setText(text, juce::dontSendNotification);
        knob.label.setFont(juce::FontOptions(10.f, juce::Font::bold));
        knob.label.setJustificationType(juce::Justification::centredLeft);
        knob.label.setColour(juce::Label::textColourId, juce::Colour(0xffc6bba8));
        knob.label.setInterceptsMouseClicks(false, false);
        knob.slider.setComponentID("cabRoom.room." + id);
        knob.slider.setName(text);
        knob.slider.setTooltip(tooltip);
        knob.slider.setSliderStyle(juce::Slider::LinearHorizontal);
        knob.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
        knob.slider.setScrollWheelEnabled(false);
        knob.slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff3f4a4c));
        knob.slider.setColour(juce::Slider::trackColourId, juce::Colour(0xffb29362));
        knob.slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe5d0ac));
        addAndMakeVisible(knob.label);
        addAndMakeVisible(knob.slider);
        knob.attachment = std::make_unique<SA>(processor.parameters(), parameter, knob.slider);
    }

public:
    explicit RoomControls(ChimeraProcessor& p) : processor(p) {
        setComponentID("cabRoom.room");
        setTitle("Room");
        setDescription("One shared room for all rigs: size, absorption and return level, plus a send per rig.");
        power.setComponentID("cabRoom.room.on");
        power.setButtonText("ROOM");
        power.setClickingTogglesState(true);
        power.setTooltip("Send the rigs into one shared room. Off lets the room's tail ring out.");
        power.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb29362));
        addAndMakeVisible(power);
        powerAttachment = std::make_unique<BA>(processor.parameters(), "roomOn", power);
        size.setComponentID("cabRoom.room.size");
        size.setTooltip("Room size: Small 3.5 x 3 x 2.5 m, Medium 7 x 5 x 3 m, Large 14 x 10 x 6 m.");
        size.addItemList({"Small", "Medium", "Large"}, 1);
        addAndMakeVisible(size);
        sizeAttachment = std::make_unique<CA>(processor.parameters(), "roomSize", size);
        setUp(absorption, "absorption", "ABSORPTION", "roomAbsorption",
              "More absorption shortens the decay and darkens the tail.");
        setUp(level, "return", "RETURN", "roomReturn", "Level of the room added back to the rigs, in dB.");
        for (int lane = 0; lane < roomLaneCount; ++lane)
            setUp(sends[size_t(lane)], "send" + juce::String(lane + 1), "RIG " + juce::String(lane + 1) + " SEND",
                  "roomSend" + juce::String(lane + 1),
                  "How much of rig " + juce::String(lane + 1) + "'s amp and cabinet goes into the room. "
                      "Matrix LOW sends only its AMP + CAB share; the DI stays dry.");
    }
    ~RoomControls() override {
        powerAttachment.reset();
        sizeAttachment.reset();
        absorption.attachment.reset();
        level.attachment.reset();
        for (auto& send : sends) send.attachment.reset();
    }
    // Rigs in the current routing mode (Classic 1, Dual 2, Matrix 3).
    void setRigCount(int count) {
        count = juce::jlimit(1, roomLaneCount, count);
        if (count == visibleSends) return;
        visibleSends = count;
        resized();
    }
    int rigCount() const noexcept { return visibleSends; }

    void resized() override {
        auto area = getLocalBounds().reduced(8, 4);
        const int rowHeight = juce::jmin(22, juce::jmax(16, area.getHeight() / 2 - 2));
        auto top = area.removeFromTop(rowHeight);
        area.removeFromTop(2);
        auto bottom = area.removeFromTop(rowHeight);
        power.setBounds(top.removeFromLeft(64));
        top.removeFromLeft(8);
        size.setBounds(top.removeFromLeft(96));
        top.removeFromLeft(12);
        const auto place = [](Knob& knob, juce::Rectangle<int> cell) {
            knob.label.setBounds(cell.removeFromLeft(juce::jmin(84, cell.getWidth() / 3)));
            knob.slider.setBounds(cell);
        };
        const int half = top.getWidth() / 2;
        place(absorption, top.removeFromLeft(half - 6));
        top.removeFromLeft(12);
        place(level, top);
        const int sendWidth = bottom.getWidth() / visibleSends;
        for (int lane = 0; lane < roomLaneCount; ++lane) {
            auto& send = sends[size_t(lane)];
            const bool shown = lane < visibleSends;
            send.label.setVisible(shown);
            send.slider.setVisible(shown);
            if (shown) place(send, bottom.removeFromLeft(sendWidth).withTrimmedRight(lane + 1 < visibleSends ? 12 : 0));
        }
    }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xff151714).withAlpha(.82f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);
        g.setColour(juce::Colour(0xffb99b6c).withAlpha(.45f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 4.f, 1.f);
    }
};

} // namespace spectralforge::cabRoom

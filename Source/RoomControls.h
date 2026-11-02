#pragma once

#include "PluginProcessor.h"
#include <array>
#include <memory>

namespace spectralforge::cabRoom {

// Room Core controls on the cabinet room surface (docs/ROOM_CORE_1_3_3.md):
// one shared room (on/off, size, absorption, return) and a send per rig that
// exists in the current routing mode. Each control is attached to its host
// parameter; showing or hiding the strip changes no audio setting.
class RoomControls : public juce::Component, private juce::Timer {
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
    // Room Advanced 1.8: balance, decay, diffusion, wet cuts and width.
    std::array<Knob, 6> advanced;
    // Room Advanced 1.8 spatial stage: declared space switch, dimensions,
    // surface materials, cabinet placement and the room-mic pair.
    juce::TextButton space;
    std::unique_ptr<BA> spaceAttachment;
    std::array<Knob, 3> dimensions;
    struct Surface { juce::Label label; juce::ComboBox box; std::unique_ptr<CA> attachment; };
    std::array<Surface, 3> surfaces;
    std::array<Knob, 3> cabinet;
    std::array<Knob, 4> microphone;
    bool spaceShown{};
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
        struct Spec { const char* id; const char* text; const char* parameter; const char* tooltip; };
        static constexpr std::array<Spec, 6> specs{{
            {"earlyLate", "EARLY / LATE", "roomEarlyLate", "Balance of early reflections and late tail. -100 % early only, 0 % the room as calibrated, +100 % tail only."},
            {"decay", "DECAY", "roomDecay", "Late decay time relative to the size and absorption: 25 % to 400 %."},
            {"diffusion", "DIFFUSION", "roomDiffusion", "Smears the sound entering the tail into a denser, smoother decay."},
            {"lowCut", "LOW CUT", "roomLowCut", "Removes low frequencies from the room return only (12 dB/oct). 20 Hz is off."},
            {"highCut", "HIGH CUT", "roomHighCut", "Removes high frequencies from the room return only (12 dB/oct). 20 kHz is off."},
            {"width", "WIDTH", "roomWidth", "Stereo width of the room return: 0 % mono, 100 % as calibrated, 200 % wider."}}};
        for (size_t i = 0; i < specs.size(); ++i)
            setUp(advanced[i], specs[i].id, specs[i].text, specs[i].parameter, specs[i].tooltip);
        space.setComponentID("cabRoom.room.space");
        space.setButtonText("SPACE");
        space.setClickingTogglesState(true);
        space.setTooltip("Use the declared room below (dimensions, surfaces, cabinet and mic positions) instead of Size and Absorption.");
        space.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb29362));
        addAndMakeVisible(space);
        spaceAttachment = std::make_unique<BA>(processor.parameters(), "roomSpace", space);
        static constexpr std::array<Spec, 3> dimensionSpecs{{
            {"length", "LENGTH", "roomSpaceLength", "Room length, back wall to front wall: 2 to 30 m."},
            {"roomWidth", "WIDTH (M)", "roomSpaceWidth", "Room width, wall to wall: 2 to 20 m."},
            {"height", "HEIGHT", "roomSpaceHeight", "Room height, floor to ceiling: 2 to 10 m."}}};
        for (size_t i = 0; i < dimensions.size(); ++i)
            setUp(dimensions[i], dimensionSpecs[i].id, dimensionSpecs[i].text, dimensionSpecs[i].parameter, dimensionSpecs[i].tooltip);
        static constexpr std::array<Spec, 3> surfaceSpecs{{
            {"floor", "FLOOR", "roomFloor", "Floor material. Its absorption sets the floor bounce and, with the other surfaces, the decay."},
            {"walls", "WALLS", "roomWalls", "Material of all four walls."},
            {"ceiling", "CEILING", "roomCeiling", "Ceiling material."}}};
        juce::StringArray names;
        for (const auto& material : roomMaterials) names.add(material.name);
        for (size_t i = 0; i < surfaces.size(); ++i) {
            auto& surface = surfaces[i];
            surface.label.setText(surfaceSpecs[i].text, juce::dontSendNotification);
            surface.label.setFont(juce::FontOptions(10.f, juce::Font::bold));
            surface.label.setColour(juce::Label::textColourId, juce::Colour(0xffc6bba8));
            surface.label.setInterceptsMouseClicks(false, false);
            surface.box.setComponentID(juce::String("cabRoom.room.") + surfaceSpecs[i].id);
            surface.box.setTooltip(surfaceSpecs[i].tooltip);
            surface.box.addItemList(names, 1);
            addAndMakeVisible(surface.label);
            addAndMakeVisible(surface.box);
            surface.attachment = std::make_unique<CA>(processor.parameters(), surfaceSpecs[i].parameter, surface.box);
        }
        static constexpr std::array<Spec, 3> cabinetSpecs{{
            {"cabDistance", "CAB TO WALL", "roomCabDistance", "Cabinet distance from the back wall, in metres."},
            {"cabAcross", "CAB ACROSS", "roomCabAcross", "Cabinet position across the room: 5 % next to the left wall, 95 % next to the right."},
            {"cabHeight", "CAB HEIGHT", "roomCabHeight", "Height of the speaker centre above the floor, in metres."}}};
        for (size_t i = 0; i < cabinet.size(); ++i)
            setUp(cabinet[i], cabinetSpecs[i].id, cabinetSpecs[i].text, cabinetSpecs[i].parameter, cabinetSpecs[i].tooltip);
        static constexpr std::array<Spec, 4> micSpecs{{
            {"micDistance", "MIC DIST", "roomMicDistance", "Room mic distance from the cabinet, towards the front wall, in metres. Independent of the close mics A and B."},
            {"micAcross", "MIC ACROSS", "roomMicAcross", "Room mic position across the room."},
            {"micHeight", "MIC HEIGHT", "roomMicHeight", "Room mic height above the floor, in metres."},
            {"micSpacing", "SPACING", "roomMicSpacing", "Spacing of the room-mic pair, in metres. 0 gives identical early reflections in both channels."}}};
        for (size_t i = 0; i < microphone.size(); ++i)
            setUp(microphone[i], micSpecs[i].id, micSpecs[i].text, micSpecs[i].parameter, micSpecs[i].tooltip);
        refreshSpace();
        startTimerHz(10);
    }
    // Size and Absorption have no effect while the declared space is on, and
    // the space controls none while it is off.
    void refreshSpace() {
        const auto* raw = processor.parameters().getRawParameterValue("roomSpace");
        const bool declared = raw != nullptr && raw->load() > .5f;
        if (declared == spaceShown && isShowing()) return;
        spaceShown = declared;
        size.setEnabled(!declared);
        absorption.slider.setEnabled(!declared);
        for (auto* knob : {&dimensions[0], &dimensions[1], &dimensions[2], &cabinet[0], &cabinet[1], &cabinet[2],
                           &microphone[0], &microphone[1], &microphone[2], &microphone[3]})
            knob->slider.setEnabled(declared);
        for (auto& surface : surfaces) surface.box.setEnabled(declared);
    }
    void timerCallback() override { refreshSpace(); }
    bool spaceControlsEnabled() const noexcept { return spaceShown; }
    ~RoomControls() override {
        powerAttachment.reset();
        sizeAttachment.reset();
        absorption.attachment.reset();
        level.attachment.reset();
        for (auto& send : sends) send.attachment.reset();
        for (auto& knob : advanced) knob.attachment.reset();
        stopTimer();
        spaceAttachment.reset();
        for (auto* group : {dimensions.data(), cabinet.data()})
            for (size_t i = 0; i < 3; ++i) group[i].attachment.reset();
        for (auto& knob : microphone) knob.attachment.reset();
        for (auto& surface : surfaces) surface.attachment.reset();
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
        const int rowHeight = juce::jmin(22, juce::jmax(16, area.getHeight() / 8 - 3));
        auto top = area.removeFromTop(rowHeight);
        area.removeFromTop(2);
        auto bottom = area.removeFromTop(rowHeight);
        area.removeFromTop(4);
        std::array<juce::Rectangle<int>, 2> extra{area.removeFromTop(rowHeight), {}};
        area.removeFromTop(2);
        extra[1] = area.removeFromTop(rowHeight);
        area.removeFromTop(4);
        std::array<juce::Rectangle<int>, 4> spatial{};
        for (auto& row : spatial) { row = area.removeFromTop(rowHeight); area.removeFromTop(2); }
        power.setBounds(top.removeFromLeft(64));
        top.removeFromLeft(6);
        space.setBounds(top.removeFromLeft(58));
        top.removeFromLeft(6);
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
        for (size_t i = 0; i < advanced.size(); ++i) {
            auto& row = extra[i / 3];
            const int cell = row.getWidth() / int(3 - i % 3);
            place(advanced[i], row.removeFromLeft(cell).withTrimmedRight(i % 3 < 2 ? 12 : 0));
        }
        const auto placeRow = [&](juce::Rectangle<int> row, Knob* knobs, int count) {
            for (int i = 0; i < count; ++i) {
                const int cell = row.getWidth() / (count - i);
                place(knobs[i], row.removeFromLeft(cell).withTrimmedRight(i + 1 < count ? 12 : 0));
            }
        };
        placeRow(spatial[0], dimensions.data(), 3);
        auto materialsRow = spatial[1];
        for (size_t i = 0; i < surfaces.size(); ++i) {
            auto cell = materialsRow.removeFromLeft(materialsRow.getWidth() / int(surfaces.size() - i)).withTrimmedRight(i + 1 < surfaces.size() ? 12 : 0);
            surfaces[i].label.setBounds(cell.removeFromLeft(juce::jmin(60, cell.getWidth() / 3)));
            surfaces[i].box.setBounds(cell);
        }
        placeRow(spatial[2], cabinet.data(), 3);
        placeRow(spatial[3], microphone.data(), 4);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xff151714).withAlpha(.82f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);
        g.setColour(juce::Colour(0xffb99b6c).withAlpha(.45f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 4.f, 1.f);
    }
};

} // namespace spectralforge::cabRoom

#include "PluginEditor.h"

namespace {
void styleLabel(juce::Label& label, float size = 14.0f)
{
    label.setColour(juce::Label::textColourId, juce::Colour(0xffe2e7ef));
    label.setFont(juce::FontOptions(size));
    label.setJustificationType(juce::Justification::centred);
}
juce::String frequencyText(float hz)
{
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, 2) + " kHz"
                        : juce::String(juce::roundToInt(hz)) + " Hz";
}
}

ChimeraEditor::ChimeraEditor(ChimeraProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Keep UI separators ASCII so they are legible on every supported font/code page.
    styleLabel(title, 22.0f);
    title.setText("SpectralForge  |  CHIMERA AMP MATRIX  |  1.0 TEST - BAND TONE",
                  juce::dontSendNotification);
    addAndMakeVisible(title);
    mode.setName("Routing Mode");
    mode.addItemList({"CLASSIC", "DUAL", "MATRIX"}, 1);
    mode.setTooltip("Classic: one rig. Dual: two parallel rigs. Matrix: three frequency bands.");
    addAndMakeVisible(mode);
    styleLabel(routingHelp);
    addAndMakeVisible(routingHelp);
    styleLabel(x1Label, 13.0f); styleLabel(x2Label, 13.0f);
    x1Label.setText("LOW / MID CROSSOVER", juce::dontSendNotification);
    x2Label.setText("MID / HIGH CROSSOVER", juce::dontSendNotification);
    addAndMakeVisible(x1Label); addAndMakeVisible(x2Label);
    setupKnob(x1, "LOW / MID", " Hz");
    setupKnob(x2, "MID / HIGH", " Hz");
    addAndMakeVisible(x1); addAndMakeVisible(x2);
    auto& state = p.parameters();
    ma = std::make_unique<CA>(state, "mode", mode);
    a1 = std::make_unique<SA>(state, "x1", x1);
    a2 = std::make_unique<SA>(state, "x2", x2);

    const std::array<juce::String, 8> names{
        "DRIVE", "LEVEL", "BASS", "LOW MID", "HIGH MID", "TREBLE", "PRESENCE", "RESONANCE"};
    for (int i = 0; i < 3; ++i)
    {
        auto& lane = lanes[i];
        const auto n = juce::String(i + 1);
        styleLabel(lane.header, 17.0f); styleLabel(lane.range);
        addAndMakeVisible(lane.header); addAndMakeVisible(lane.range);
        lane.amp.addItemList({"Glass", "Brit Edge", "Tight 515", "Wide Rect", "Liquid Lead",
                              "Iron Tube", "Solid Punch", "Modern Bass"}, 1);
        lane.amp.setTooltip("Amplifier model for this rig");
        addAndMakeVisible(lane.amp);
        lane.aa = std::make_unique<CA>(state, "amp" + n, lane.amp);
        const auto sliders = lane.fullRangeControls();
        const std::array<juce::String, 8> ids{"drive", "level", "bass", "lowmid", "highmid",
                                             "treble", "presence", "resonance"};
        for (size_t k = 0; k < sliders.size(); ++k)
        {
            setupKnob(*sliders[k], names[k], k == 0 ? "" : " dB");
            styleLabel(lane.knobLabels[k], 13.0f);
            lane.knobLabels[k].setText(names[k], juce::dontSendNotification);
            addAndMakeVisible(lane.knobLabels[k]);
            addAndMakeVisible(*sliders[k]);
            lane.sa[k] = std::make_unique<SA>(state, ids[k] + n, *sliders[k]);
        }
        setupKnob(lane.bandTone, "BAND TONE", " dB");
        lane.bandTone.setTooltip("Pre-amp tilt within this input band. Negative: darker. Positive: brighter. The pivot follows the crossovers.");
        styleLabel(lane.toneLabel, 13.0f);
        styleLabel(lane.tonePivot, 14.0f);
        styleLabel(lane.toneHelp, 14.0f);
        lane.toneLabel.setText("BAND TONE", juce::dontSendNotification);
        lane.toneHelp.setText("Darker (-)  |  Brighter (+)\nTone shapes the band before the amp.",
                              juce::dontSendNotification);
        addAndMakeVisible(lane.toneLabel); addAndMakeVisible(lane.tonePivot);
        addAndMakeVisible(lane.toneHelp); addAndMakeVisible(lane.bandTone);
        lane.toneAttachment = std::make_unique<SA>(state, "bandtone" + n, lane.bandTone);
        for (auto* button : {&lane.mute, &lane.solo})
        {
            button->setClickingTogglesState(true);
            button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff267c98));
            addAndMakeVisible(*button);
        }
        lane.mute.setTooltip("Mute this active rig");
        lane.solo.setTooltip("Listen to this active rig on its own");
        lane.muteAttachment = std::make_unique<BA>(state, "mute" + n, lane.mute);
        lane.soloAttachment = std::make_unique<BA>(state, "solo" + n, lane.solo);
    }
    mode.onChange = [this] { updateModeUI(); };
    x1.onValueChange = [this] { updateBandLabels(); };
    x2.onValueChange = [this] { updateBandLabels(); };
    setSize(1180, 690);
    updateModeUI();
    startTimerHz(20);
}

void ChimeraEditor::setupKnob(juce::Slider& slider, const juce::String& name,
                              const juce::String& suffix)
{
    slider.setName(name); slider.setTooltip(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 22);
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(name.contains("/") ? 0 : name == "DRIVE" ? 2 : 1);
}

void ChimeraEditor::timerCallback()
{
    const int current = static_cast<int>(processor.parameters().getRawParameterValue("mode")->load());
    if (current != lastMode) updateModeUI();
    if (current == 2) updateBandLabels();
}

void ChimeraEditor::updateBandLabels()
{
    if (lastMode != 2) return;
    const auto low = processor.parameters().getRawParameterValue("x1")->load();
    const auto high = processor.parameters().getRawParameterValue("x2")->load();
    lanes[0].range.setText("INPUT: below " + frequencyText(low), juce::dontSendNotification);
    lanes[1].range.setText("INPUT: " + frequencyText(low) + " - " + frequencyText(high), juce::dontSendNotification);
    lanes[2].range.setText("INPUT: above " + frequencyText(high), juce::dontSendNotification);
    const auto sr = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.0;
    for (int i = 0; i < 3; ++i)
        lanes[i].tonePivot.setText("TONE PIVOT: " + frequencyText(spectralforge::matrixTonePivot(i, low, high, sr)),
                                   juce::dontSendNotification);
}

void ChimeraEditor::updateModeUI()
{
    lastMode = static_cast<int>(processor.parameters().getRawParameterValue("mode")->load());
    const bool matrix = lastMode == 2;
    const int count = lastMode == 0 ? 1 : lastMode == 1 ? 2 : 3;
    x1.setVisible(matrix); x2.setVisible(matrix);
    x1Label.setVisible(matrix); x2Label.setVisible(matrix);
    for (int i = 0; i < 3; ++i)
    {
        auto& lane = lanes[i];
        const bool show = i < count;
        lane.header.setVisible(show); lane.range.setVisible(show); lane.amp.setVisible(show);
        lane.mute.setVisible(show); lane.solo.setVisible(show);
        const auto sliders = lane.fullRangeControls();
        for (size_t k = 0; k < sliders.size(); ++k)
        {
            const bool visible = show && (!matrix || k < 2);
            sliders[k]->setVisible(visible); lane.knobLabels[k].setVisible(visible);
        }
        lane.bandTone.setVisible(show && matrix); lane.toneLabel.setVisible(show && matrix);
        lane.tonePivot.setVisible(show && matrix); lane.toneHelp.setVisible(show && matrix);
    }
    if (lastMode == 0)
    {
        routingHelp.setText("CLASSIC | Single full-range amp/cab rig | No crossover", juce::dontSendNotification);
        lanes[0].header.setText("FULL-RANGE RIG", juce::dontSendNotification);
        lanes[0].range.setText("Full-range input", juce::dontSendNotification);
    }
    else if (lastMode == 1)
    {
        routingHelp.setText("DUAL | Two full-range rigs in parallel | No crossover", juce::dontSendNotification);
        lanes[0].header.setText("FULL-RANGE RIG A", juce::dontSendNotification);
        lanes[1].header.setText("FULL-RANGE RIG B", juce::dontSendNotification);
        for (int i = 0; i < 2; ++i) lanes[i].range.setText("Full-range input", juce::dontSendNotification);
    }
    else
    {
        routingHelp.setText("MATRIX | Split input bands | Drive, level and tone per band", juce::dontSendNotification);
        lanes[0].header.setText("LOW BAND", juce::dontSendNotification);
        lanes[1].header.setText("MID BAND", juce::dontSendNotification);
        lanes[2].header.setText("HIGH BAND", juce::dontSendNotification);
        updateBandLabels();
    }
    resized(); repaint();
}

void ChimeraEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101217));
    g.setColour(juce::Colour(0xffe2e5ea)); g.setFont(juce::FontOptions(16.0f));
    g.drawText("ROUTING MODE", 25, 65, 170, 24, juce::Justification::centredLeft);
    if (lastMode == 2)
    {
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("LR4 crossover | 24 dB/oct", 660, 145, 475, 25, juce::Justification::centredLeft);
        g.drawText("Band tone follows the crossover points.", 660, 175, 475, 25, juce::Justification::centredLeft);
    }
    const int count = lastMode == 0 ? 1 : lastMode == 1 ? 2 : 3;
    const float gap = 14.0f, width = (getWidth() - 40.0f - gap * (count - 1)) / count;
    for (int i = 0; i < count; ++i)
    {
        const float x = 20.0f + i * (width + gap);
        g.setColour(juce::Colour(0xff292d36)); g.fillRoundedRectangle(x, 220.0f, width, 450.0f, 9.0f);
        g.setColour(juce::Colour(0xff555d6c)); g.drawRoundedRectangle(x, 220.0f, width, 450.0f, 9.0f, 1.2f);
    }
}

void ChimeraEditor::resized()
{
    title.setBounds(20, 12, getWidth() - 40, 38);
    mode.setBounds(25, 95, 170, 32);
    routingHelp.setBounds(215, 94, getWidth() - 235, 36);
    x1Label.setBounds(230, 135, 190, 22); x1.setBounds(270, 158, 110, 58);
    x2Label.setBounds(425, 135, 190, 22); x2.setBounds(465, 158, 110, 58);
    const bool matrix = lastMode == 2;
    const int count = lastMode == 0 ? 1 : lastMode == 1 ? 2 : 3;
    const int gap = 14, width = (getWidth() - 40 - gap * (count - 1)) / count;
    for (int i = 0; i < count; ++i)
    {
        auto& lane = lanes[i];
        const int x = 20 + i * (width + gap);
        lane.header.setBounds(x + 10, 234, width - 20, 26);
        lane.range.setBounds(x + 10, 262, width - 20, 22);
        lane.amp.setBounds(x + 20, 299, width - 40, 32);
        if (matrix)
        {
            const int cell = (width - 24) / 3;
            std::array<juce::Slider*, 3> sliders{&lane.drive, &lane.level, &lane.bandTone};
            std::array<juce::Label*, 3> labels{&lane.knobLabels[0], &lane.knobLabels[1], &lane.toneLabel};
            for (int k = 0; k < 3; ++k)
            {
                const int left = x + 12 + k * cell;
                labels[k]->setBounds(left, 354, cell, 22);
                sliders[k]->setBounds(left + 4, 380, cell - 8, 106);
            }
            lane.tonePivot.setBounds(x + 12, 513, width - 24, 24);
            lane.toneHelp.setBounds(x + 12, 544, width - 24, 48);
        }
        else
        {
            const auto sliders = lane.fullRangeControls();
            const int cell = (width - 32) / 4;
            const int knobWidth = juce::jmin(110, cell);
            for (int k = 0; k < 8; ++k)
            {
                const int left = x + 16 + (k % 4) * cell + (cell - knobWidth) / 2;
                const int top = 346 + (k / 4) * 130;
                lane.knobLabels[k].setBounds(left, top, knobWidth, 22);
                sliders[k]->setBounds(left, top + 24, knobWidth, 94);
            }
        }
        lane.mute.setBounds(x + width / 2 - 91, 626, 82, 28);
        lane.solo.setBounds(x + width / 2 + 9, 626, 82, 28);
    }
}

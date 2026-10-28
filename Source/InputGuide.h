#pragma once
#include "StandaloneInput.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Tells a new Standalone player why the guitar is silent and how to fix it.
// Hidden in plug-ins and as soon as the input is on, however it was turned on
// (here or with "Mute audio input" in the audio settings).
class InputGuide final : public juce::Component, private juce::Timer {
public:
    InputGuide() {
        setComponentID("inputGuide");setName("Guitar input guide");
        message.setComponentID("inputGuide.text");
        message.setText("No guitar sound? Input starts off to prevent speaker howl. Use headphones, then press INPUT ON.",
                        juce::dontSendNotification);
        message.setFont(juce::FontOptions(12.5f));message.setColour(juce::Label::textColourId,ink);
        message.setJustificationType(juce::Justification::centredLeft);message.setMinimumHorizontalScale(.85f);
        settings.setComponentID("inputGuide.settings");settings.getProperties().set("utilityDock",true);
        settings.setTooltip("Choose the audio interface your guitar is plugged into and the output you listen on.");
        settings.onClick=[]{if(auto* input=spectralforge::standalone::inputMute)if(input->openAudioSettings)input->openAudioSettings();};
        unmute.setComponentID("inputGuide.unmute");unmute.getProperties().set("utilityDock",true);
        unmute.setTooltip("Turn the guitar input on. Use headphones: with speakers and a nearby microphone this can howl. Mute it again in AUDIO SETTINGS.");
        unmute.onClick=[this]{if(auto* input=spectralforge::standalone::inputMute)if(input->unmute)input->unmute();refresh();};
        for(auto* c:std::initializer_list<juce::Component*>{&message,&settings,&unmute})addAndMakeVisible(*c);
        setVisible(false);refresh();startTimerHz(4);
    }
    ~InputGuide() override {stopTimer();}

    static bool needed() {
        const auto* input=spectralforge::standalone::inputMute;
        return input!=nullptr && input->muted && input->muted();
    }
    void refresh() {
        const bool show=needed();
        if(show!=isVisible()){setVisible(show);if(show)toFront(false);}
    }
    void resized() override {
        auto r=getLocalBounds().reduced(10,8);
        const auto buttons=r.withSizeKeepingCentre(r.getWidth(),juce::jmin(r.getHeight(),32));
        unmute.setBounds(r.removeFromRight(86).withY(buttons.getY()).withHeight(buttons.getHeight()));r.removeFromRight(8);
        settings.setBounds(r.removeFromRight(120).withY(buttons.getY()).withHeight(buttons.getHeight()));r.removeFromRight(10);
        message.setBounds(r);
    }
    void paint(juce::Graphics& g) override {
        const auto area=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(fill);g.fillRoundedRectangle(area,6);
        g.setColour(edge);g.drawRoundedRectangle(area,6,1.f);
    }

private:
    void timerCallback() override {refresh();}
    static inline const juce::Colour fill{0xff2a2318},edge{0xffc4a678},ink{0xffe1dbce};
    juce::Label message;
    juce::TextButton settings{"AUDIO SETTINGS"},unmute{"INPUT ON"};
};

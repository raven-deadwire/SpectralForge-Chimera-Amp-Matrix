#pragma once
#include "PluginProcessor.h"
#include "PrecisionTuner.h"

class PrecisionTunerPanel final : public juce::Component,private juce::Timer {
public:
    explicit PrecisionTunerPanel(ChimeraProcessor& p):processor(p){
        setComponentID("precisionTuner");setName("Precision tuner");setOpaque(true);
        for(const auto* name:spectralforge::tuning::profileNames)profile.addItem(name,profile.getNumItems()+1);
        display.addItemList({"STROBE","NEEDLE"},1);strings.addItem("AUTO STRING",1);
        for(int i=1;i<=8;++i)strings.addItem("STRING "+juce::String(i),i+1);
        for(auto* c:{&profile,&display,&strings})addAndMakeVisible(*c);
        profile.setName("Tuning temperament");display.setName("Tuning display");strings.setName("Target string, high to low");
        profile.setComponentID("tunerProfile");display.setComponentID("tunerDisplay");strings.setComponentID("tunerString");
        auto& state=p.parameters();
        profileAttachment=std::make_unique<CA>(state,"tunerProfile",profile);displayAttachment=std::make_unique<CA>(state,"tunerDisplay",display);
        stringAttachment=std::make_unique<juce::ParameterAttachment>(*state.getParameter("tunerString"),[this](float v){strings.setSelectedId(int(v)+1,juce::dontSendNotification);},nullptr);
        stringAttachment->sendInitialUpdate();strings.onChange=[this]{stringAttachment->setValueAsCompleteGesture(float(strings.getSelectedId()-1));};
        mute.setClickingTogglesState(true);addAndMakeVisible(mute);mute.setComponentID("tunerMute");
        mute.setTooltip("Silence the output while tuning; your processing settings are preserved.");
        muteAttachment=std::make_unique<BA>(state,"tunermute",mute);
        reference.setSliderStyle(juce::Slider::LinearHorizontal);reference.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,26);reference.setTextValueSuffix(" Hz");reference.setName("A4 reference");addAndMakeVisible(reference);
        refAttachment=std::make_unique<SA>(state,"tunerref",reference);
        for(int i=0;i<8;++i){
            const auto n=juce::String(i+1);auto& note=customNotes[size_t(i)];auto& offset=customOffsets[size_t(i)];
            for(auto* slider:{&note,&offset}){slider->setSliderStyle(juce::Slider::IncDecButtons);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,73,24);addAndMakeVisible(*slider);}
            note.setName("String "+n+" MIDI note");offset.setName("String "+n+" cent offset");offset.setTextValueSuffix(" ct");
            note.setTooltip("MIDI note number; use +/- or enter 12..96. Display includes octave.");
            noteAttachments[size_t(i)]=std::make_unique<SA>(state,"tunerNote"+n,note);offsetAttachments[size_t(i)]=std::make_unique<SA>(state,"tunerOffset"+n,offset);
            // SliderAttachment installs its own formatter; apply musical names afterwards.
            note.textFromValueFunction=[](double v){return juce::String(int(v))+" / "+noteName(int(v));};note.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue();};note.updateText();
        }
        status.setName("Tuner reading");status.setComponentID("tunerReading");status.setFont(juce::FontOptions(17.f));status.setJustificationType(juce::Justification::centred);addAndMakeVisible(status);
        startTimerHz(30);timerCallback();
    }
    ~PrecisionTunerPanel() override{stopTimer();}
    static juce::String noteName(int midi){const std::array<const char*,12> names{{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}};return juce::String(names[size_t((midi%12+12)%12)])+juce::String(midi/12-1);}
    void resized() override{
        profile.setBounds(18,36,450,30);display.setBounds(480,36,120,30);strings.setBounds(612,36,190,30);
        mute.setBounds(974,36,148,30);
        reference.setBounds(66,371,300,28);status.setBounds(20,323,752,35);
        for(int i=0;i<8;++i){customNotes[size_t(i)].setBounds(862,108+i*31,116,26);customOffsets[size_t(i)].setBounds(988,108+i*31,132,26);}
    }
    void paint(juce::Graphics& g) override{
        const auto ink=juce::Colour(0xfff0f4eb),accent=juce::Colour(0xffa8dfbc);
        g.fillAll(juce::Colour(0xff172321));g.setColour(ink);g.setFont(juce::FontOptions(15.f,juce::Font::bold));g.drawText("PRECISION TUNER",18,5,380,26,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(12.f));g.drawText("A4",20,371,40,28,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff0b1211));g.fillRoundedRectangle(20,82,752,235,8);
        g.setColour(valid?accent:ink);g.setFont(juce::FontOptions(62.f,juce::Font::bold));g.drawText(valid?noteName(reading.midi):"--",35,91,710,77,juce::Justification::centred);
        g.setFont(juce::FontOptions(19.f));g.drawText(valid?(reading.cents>0?"+":"")+juce::String(reading.cents,2)+" ct   /   "+juce::String(measured,3)+" Hz":"PLAY ONE NOTE",35,171,710,28,juce::Justification::centred);
        if(display.getSelectedId()==1){
            for(int band=0;band<3;++band){juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(42,216+band*28,708,21);g.setColour(valid?accent.withAlpha(1.f-band*.22f):ink.withAlpha(.12f));
                for(int stripe=-1;stripe<32;++stripe)g.fillRect(float(42+(stripe+phases[size_t(band)])*32),float(216+band*28),12.f,21.f);
            }
        }else{
            g.setColour(ink.withAlpha(.6f));g.drawLine(70,260,722,260,2);for(int i=-5;i<=5;++i)g.drawVerticalLine(396+i*62,247,274);
            if(valid){g.setColour(accent);g.fillRoundedRectangle(float(393+std::clamp(reading.cents,-50.0,50.0)*6.2),230,6,59,2);}
        }
        g.setFont(juce::FontOptions(12.f));g.setColour(ink);g.drawText(display.getSelectedId()==1?"FLAT  <     Stop the bands to tune     >  SHARP":"FLAT  <     Center the needle to tune     >  SHARP",36,297,720,18,juce::Justification::centred);
        const bool custom=settings.profile==spectralforge::tuning::Profile::custom;
        g.setFont(juce::FontOptions(13.f,juce::Font::bold));g.drawText(custom?"STRING       NOTE             OFFSET":"TUNING GUIDE",810,80,312,25,juce::Justification::centredLeft);
        if(custom){for(int i=0;i<8;++i)g.drawText(juce::String(i+1),820,108+i*31,32,26,juce::Justification::centred);}
        else{
            juce::String guide="Use a clean, single note.\n\nAUTO detects the closest target. Select a string to lock its target.\n\nStrobe motion follows the measured beat frequency. Right = sharp, left = flat.\n\nLow bass notes need a longer settling time.";
            using P=spectralforge::tuning::Profile;
            if(settings.profile==P::feitenElectric||settings.profile==P::feitenElectric12)guide="BFTS electric / Anderson targets\n\nPublished electric-guitar offsets. For a correctly fitted system.\n\nOpen: pluck the open string.\n12th fret: fret the note; do not use a harmonic.\n\nOpen and fretted offsets differ. Acoustic and bass tables are not included.";
            if(settings.profile==P::bass5 || settings.profile==P::bass6)guide="Equal temperament / bass\n\nStrings are numbered high to low.\n5 strings: G2 D2 A1 E1 B0\n6 strings: C3 G2 D2 A1 E1 B0\n\nUse Custom for BEAD, drop A or other tunings.\n\nThese are zero-offset targets, not compensated bass profiles.";
            if(settings.profile==P::tt6||settings.profile==P::tt7||settings.profile==P::tt8||settings.profile==P::ttChromatic)guide="True Temperament / Thidell Formula 1\n\nFor a TT-equipped instrument.\n\nOpen-string mode distinguishes high E, low E, low B and F#.\n\nChromatic uses the published 12-note table. Select the profile for the task.\n\nCustom supports alternate note targets.";
            g.setFont(juce::FontOptions(14.f));g.drawFittedText(guide,810,115,306,274,juce::Justification::topLeft,15);
        }
    }
private:
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override{
        if(!isShowing()) {lastTime=0;return;}
        const auto previousSettings=settings;const auto oldMeasured=measured;const bool oldValid=valid;
        const auto oldPhases=phases;const bool opening=lastTime==0;
        const auto get=[this](const juce::String& id){return processor.parameters().getRawParameterValue(id)->load();};
        settings.profile=static_cast<spectralforge::tuning::Profile>(int(get("tunerProfile")));settings.reference=get("tunerref");settings.stringIndex=int(get("tunerString"))-1;
        for(int i=0;i<8;++i){settings.customMidi[size_t(i)]=int(get("tunerNote"+juce::String(i+1)));settings.customCents[size_t(i)]=get("tunerOffset"+juce::String(i+1));const bool show=settings.profile==spectralforge::tuning::Profile::custom;customNotes[size_t(i)].setVisible(show);customOffsets[size_t(i)].setVisible(show);}
        const int count=spectralforge::tuning::strings(settings.profile);strings.setEnabled(count>0);for(int i=1;i<=8;++i)strings.setItemEnabled(i+1,i<=count);
        measured=processor.tuningFrequency();reading=spectralforge::tuning::target(measured,settings);valid=get("tuneron")>.5f&&processor.tuningConfidence()>.9f&&reading.valid;
        const double now=juce::Time::getMillisecondCounterHiRes()*.001,dt=lastTime>0?std::min(.1,now-lastTime):0;lastTime=now;
        if(valid){if(previousTarget!=reading.hz)phases.fill(0);for(int i=0;i<3;++i)phases[size_t(i)]=spectralforge::tuning::advanceDisplayStrobe(phases[size_t(i)],measured,reading.hz,dt,1<<i);previousTarget=reading.hz;}else {phases.fill(0);previousTarget=0;}
        juce::String message=get("tuneron")<.5f?"TUNER OFF":!valid?(count>0 && settings.stringIndex>=count?"Select AUTO or a string in this profile":"LISTENING  /  Play one steady note"):(reading.stringIndex>=0?"STRING "+juce::String(reading.stringIndex+1)+"  /  ":"")+juce::String(reading.cents,2)+" ct  /  target "+juce::String(reading.hz,3)+" Hz  /  offset "+juce::String(reading.offset,1)+" ct";
        if(get("tuneron")>.5f)message+=get("tunermute")>.5f?"  /  MUTED":"  /  MONITOR";
        status.setText(message,juce::dontSendNotification);
        const int selectedDisplay=display.getSelectedId();
        const bool guideChanged=settings.profile!=previousSettings.profile
            || settings.reference!=previousSettings.reference || settings.stringIndex!=previousSettings.stringIndex
            || settings.customMidi!=previousSettings.customMidi || settings.customCents!=previousSettings.customCents;
        // The panel paints an opaque background. Only the moving reading needs
        // a 30 Hz repaint; idle frames must not redraw the rig underneath it.
        if(opening || guideChanged || selectedDisplay!=lastDisplay)repaint();
        else if(valid!=oldValid || measured!=oldMeasured || phases!=oldPhases)repaint(20,82,752,235);
        lastDisplay=selectedDisplay;
    }
    ChimeraProcessor& processor;juce::ComboBox profile,display,strings;juce::TextButton mute{"MUTE OUTPUT"};juce::Slider reference;juce::Label status;
    std::array<juce::Slider,8> customNotes,customOffsets;std::array<std::unique_ptr<SA>,8> noteAttachments,offsetAttachments;
    std::unique_ptr<CA> profileAttachment,displayAttachment;std::unique_ptr<juce::ParameterAttachment> stringAttachment;std::unique_ptr<SA> refAttachment;std::unique_ptr<BA> muteAttachment;
    spectralforge::tuning::Settings settings;spectralforge::tuning::Target reading;bool valid{};int lastDisplay{};double measured{},lastTime{},previousTarget{};std::array<double,3> phases{};
};

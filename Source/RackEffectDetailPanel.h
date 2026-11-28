#pragma once
#include "PluginProcessor.h"
#include "NativeControlView.h"
#include "EffectSelectionCatalog.h"
#include "HardwareArtwork.h"
#include "EffectModelSync.h"

#include "PostReverbControls.h"

// The reverb's tempo shape, drawn (docs/POST_REVERB_DECAY.md): the level the reverb follows through one pass of
// the shape at the set depth, a line for every quarter note, the tempo, and where the pass is now.
class ReverbShapeDisplay final : public juce::Component,private juce::Timer {
public:
    ReverbShapeDisplay(ChimeraProcessor& owner,int slotIndex):processor(owner),slot(slotIndex) {
        setComponentID(spectralforge::postSlotID(slot,"reverbShapeDisplay"));setInterceptsMouseClicks(false,false);
        setTitle("Reverb shape");setDescription("The level the reverb follows through one pass of the shape");
        for(int i=0;i<spectralforge::postReverbShapePerSlot;++i)values[(size_t)i]=processor.parameters().getRawParameterValue(spectralforge::reverbShapeID(slot,i));
        for(int i=0;i<spectralforge::postReverbShapeMorePerSlot;++i)more[(size_t)i]=processor.parameters().getRawParameterValue(spectralforge::reverbShapeMoreID(slot,i));
        read();startTimerHz(30);
    }
    ~ReverbShapeDisplay() override {stopTimer();}
    // What is drawn, for the tests.
    int shownShape() const noexcept {return shape;}
    float shownDepth() const noexcept {return depth;}
    int shownLength() const noexcept {return length;}
    float shownPosition() const noexcept {return position;}
    float shownLevel(double phase) const noexcept {return shape==spectralforge::reverbShapeOff?1.f:1.f-depth*(1.f-spectralforge::reverbShapeCurved(spectralforge::reverbShapeLevel(shape,phase),curve));}
    bool shownInSeconds() const noexcept {return inSeconds;}
    float shownSeconds() const noexcept {return seconds;}
    float shownCurve() const noexcept {return curve;}
    // The lines drawn across the pass: one for every quarter note in bars, four parts in seconds.
    int shownDivisions() const noexcept {return inSeconds?4:juce::roundToInt(spectralforge::reverbShapeBeats[length]);}
    juce::Rectangle<float> plot() const noexcept {return getLocalBounds().toFloat().reduced(10.f,6.f).withTrimmedTop(18.f).withTrimmedBottom(4.f);}
    void refresh() {timerCallback();}
    void paint(juce::Graphics& g) override {
        const auto all=getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0d1010));g.fillRoundedRectangle(all,4.f);g.setColour(juce::Colour(0xff2b302e));g.drawRoundedRectangle(all.reduced(.5f),4.f,1.f);
        const auto area=plot();const bool off=shape==spectralforge::reverbShapeOff;
        const juce::Colour ink=off?juce::Colour(0xff6c706a):juce::Colour(0xffd0b386);
        g.setFont(juce::FontOptions(10.5f));g.setColour(juce::Colour(0xffa4a79f));
        g.drawText(inSeconds?juce::String("FREE"):juce::String(juce::roundToInt(tempo))+" BPM",juce::Rectangle<float>(area.getX(),4.f,90.f,16.f),juce::Justification::centredLeft);
        g.drawText(off?juce::String("SHAPE OFF"):spectralforge::reverbShapeChoices()[shape]+" / "+(inSeconds?juce::String(seconds,2)+" s":spectralforge::reverbShapeLengthChoices()[length]),juce::Rectangle<float>(area.getRight()-200.f,4.f,200.f,16.f),juce::Justification::centredRight);
        const int beats=shownDivisions();
        g.setColour(juce::Colour(0xff2b302e));for(int beat=0;beat<=beats;++beat){const float x=area.getX()+area.getWidth()*float(beat)/float(beats);g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());}
        g.drawHorizontalLine(juce::roundToInt(area.getBottom()),area.getX(),area.getRight());
        juce::Path line;const int steps=juce::jmax(2,juce::roundToInt(area.getWidth()));
        for(int i=0;i<=steps;++i){const double phase=juce::jmin(.999999,double(i)/double(steps));const float x=area.getX()+area.getWidth()*float(i)/float(steps),y=area.getBottom()-area.getHeight()*shownLevel(phase);if(i==0)line.startNewSubPath(x,y);else line.lineTo(x,y);}
        juce::Path fill=line;fill.lineTo(area.getRight(),area.getBottom());fill.lineTo(area.getX(),area.getBottom());fill.closeSubPath();
        g.setColour(ink.withAlpha(off?.08f:.18f));g.fillPath(fill);g.setColour(ink);g.strokePath(line,juce::PathStrokeType(1.6f));
        if(!off && position>=0.f){const float x=area.getX()+area.getWidth()*juce::jlimit(0.f,1.f,position);g.setColour(juce::Colour(0xffe1dbce));g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
            g.fillEllipse(x-3.f,area.getBottom()-area.getHeight()*shownLevel(juce::jmin(.999999,double(position)))-3.f,6.f,6.f);}
    }
private:
    ChimeraProcessor& processor;int slot{};std::array<std::atomic<float>*,spectralforge::postReverbShapePerSlot> values{};std::array<std::atomic<float>*,spectralforge::postReverbShapeMorePerSlot> more{};
    int shape{},length{};float depth{1.f},position{-1.f},tempo{120.f},seconds{1.f},curve{0.f};bool inSeconds{};
    bool read() {
        const int s=juce::jlimit(0,spectralforge::reverbShapeCount-1,juce::roundToInt(values[0]->load())),l=juce::jlimit(0,spectralforge::reverbShapeLengthCount-1,juce::roundToInt(values[2]->load()));
        const float d=juce::jlimit(0.f,1.f,values[1]->load()),where=processor.reverbShapePosition(slot),bpm=processor.currentTempo();
        const bool unit=more[0]->load()>.5f;const float time=more[1]->load(),bend=juce::jlimit(-1.f,1.f,more[2]->load());
        const bool changed=s!=shape || l!=length || d!=depth || where!=position || juce::roundToInt(bpm)!=juce::roundToInt(tempo) || unit!=inSeconds || time!=seconds || bend!=curve;
        shape=s;length=l;depth=d;position=where;tempo=bpm;inSeconds=unit;seconds=time;curve=bend;return changed;
    }
    void timerCallback() override {if(read())repaint();}
};

// The three time/modulation processors retain their released parameter IDs.
// A second set of attachments lets the rack overview and ALL panel stay in sync.
class RackEffectDetailPanel final : public juce::Component,private juce::Timer {
public:
    // `slotIndex` 1: the POST second slot (docs/POST_SECOND_SLOTS.md).
    RackEffectDetailPanel(ChimeraProcessor& owner,int familyIndex,int slotIndex=0):processor(owner),family(familyIndex),slot(slotIndex) {
        setComponentID("postEffectDetail"+juce::String(family)+spectralforge::postSlotSuffix(slot));
        for(auto* label:{&title,&reference}){addAndMakeVisible(*label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe1dbce));}
        title.setFont(juce::FontOptions(13.f,juce::Font::bold));reference.setFont(juce::FontOptions(11.f));
        title.setText(juce::String(spectralforge::modelFamilies[(size_t)family].category)+spectralforge::postSlotNumber(slot),juce::dontSendNotification);
        addAndMakeVisible(model);model.setComponentID(id(spectralforge::modelFamilies[(size_t)family].parameter));
        spectralforge::buildExtendedEffectMenu(*model.getRootMenu(),family);
        modelSync=std::make_unique<spectralforge::ExtendedEffectModelSync>(processor.parameters(),family,model,slot);
        model.onChange=[this]{refreshModel();};
        addAndMakeVisible(enabled);enabled.setClickingTogglesState(true);enabled.setComponentID(id(family==10?"choruson":family==1?"delayon":"reverbon"));
        enabledAttachment=std::make_unique<juce::ParameterAttachment>(*processor.parameters().getParameter(enabled.getComponentID()),[this](float value){enabled.setToggleState(value>.5f,juce::dontSendNotification);enabled.setButtonText(value>.5f?"ON":"OFF");});enabledAttachment->sendInitialUpdate();
        enabled.onClick=[this]{enabledAttachment->setValueAsCompleteGesture(enabled.getToggleState()?1.f:0.f);};
        const std::array<const char*,3> ids=family==10?std::array<const char*,3>{"chorusrate","chorusdepth","chorusmix"}:family==1?std::array<const char*,3>{"delaytime","delayfeedback","delaymix"}:std::array<const char*,3>{"reverbsize","reverbdamping","reverbmix"};
        const std::array<const char*,3> labels=family==10?std::array<const char*,3>{"RATE","DEPTH","MIX"}:family==1?std::array<const char*,3>{"TIME","FEEDBACK","MIX"}:std::array<const char*,3>{"SIZE","DAMPING","MIX"};
        for(int i=0;i<3;++i){auto control=std::make_unique<NativeControlView>();const auto& range=processor.parameters().getParameter(id(ids[(size_t)i]))->getNormalisableRange();control->bind(processor.parameters(),id(ids[(size_t)i]),labels[(size_t)i],title.getText(),0,{},range.start,range.end,range.interval,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
        if(family==1){auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),id("delaysync"),"TEMPO SYNC","DELAY",2,{"OFF","ON"},0,1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));controls.front()->slider.textFromValueFunction=[this](double value){const bool sync=processor.parameters().getRawParameterValue(id("delaysync"))->load()>.5f;return juce::String(sync?60000.0/processor.currentTempo():value,1)+" ms";};timerCallback();startTimerHz(15);}
        if(family==10)controls.front()->slider.setTextValueSuffix(" Hz");
        if(family==2) {
            // DECAY, first in the first row (docs/POST_REVERB_DECAY.md): at its lowest SIZE sets the decay, as it always did.
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbDecayID(slot),"DECAY",title.getText(),0,{},0,spectralforge::reverbDecayMaxSeconds,0,3);
             control->slider.setSkewFactorFromMidPoint(2.0);control->slider.textFromValueFunction=[](double value){return spectralforge::reverbDecayText(float(value));};
             control->slider.valueFromTextFunction=[](const juce::String& text){return text.trim().equalsIgnoreCase("SIZE")?0.0:text.getDoubleValue();};control->slider.updateText();
             control->slider.setDoubleClickReturnValue(true,0.0);addAndMakeVisible(*control);controls.insert(controls.begin(),std::move(control));}
            // Character, the rest of the first row (docs/POST_REVERB_CHARACTER.md): an older converter, fewer bits, a slow drift.
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbCharacterID(slot,0),"CONVERTER",title.getText(),1,spectralforge::reverbConverterChoices(),0,spectralforge::reverbConverterCount-1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            for(int i=1;i<spectralforge::postReverbCharacterPerSlot;++i){auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbCharacterID(slot,i),i==1?"DIRT":"DRIFT",title.getText(),0,{},0,1,0,3);
                control->slider.setNumDecimalPlacesToDisplay(2);control->slider.setDoubleClickReturnValue(true,0.0);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            shapeDisplay=std::make_unique<ReverbShapeDisplay>(processor,slot);addAndMakeVisible(*shapeDisplay);
            // The reverb's second row (docs/POST_REVERB_CONTROLS.md): four knobs and the freeze switch.
            for(int i=0;i<spectralforge::postReverbControlsPerSlot;++i) {
                static constexpr std::array<const char*,spectralforge::postReverbControlsPerSlot> names{"PRE-DELAY","LOW CUT","TONE","WIDTH","FREEZE"};
                static constexpr std::array<const char*,spectralforge::postReverbControlsPerSlot> units{" ms"," Hz","","",""};
                static constexpr std::array<int,spectralforge::postReverbControlsPerSlot> decimals{0,0,2,2,0};
                const auto& spec=spectralforge::reverbControlSpecs[(size_t)i];const auto parameter=spectralforge::reverbControlID(slot,i);
                auto control=std::make_unique<NativeControlView>();
                control->bind(processor.parameters(),parameter,names[(size_t)i],title.getText(),spec.toggle?2:0,spec.toggle?juce::StringArray{"OFF","ON"}:juce::StringArray{},spec.minimum,spec.maximum,spec.toggle?1:0,3);
                if(!spec.toggle){control->slider.setTextValueSuffix(units[(size_t)i]);control->slider.setNumDecimalPlacesToDisplay(decimals[(size_t)i]);control->slider.setDoubleClickReturnValue(true,spec.initial);}
                addAndMakeVisible(*control);controls.push_back(std::move(control));
            }
            // The third row (docs/POST_REVERB_SHAPE.md, docs/POST_REVERB_SHAPE_MORE.md): the shape, bars or seconds, and its length in each.
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeID(slot,0),"SHAPE",title.getText(),1,spectralforge::reverbShapeChoices(),0,spectralforge::reverbShapeCount-1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeMoreID(slot,0),"UNIT",title.getText(),1,spectralforge::reverbShapeUnitChoices(),0,1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeID(slot,2),"LENGTH",title.getText(),1,spectralforge::reverbShapeLengthChoices(),0,spectralforge::reverbShapeLengthCount-1,1,3);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeMoreID(slot,1),"SECONDS",title.getText(),0,{},spectralforge::reverbShapeMinSeconds,spectralforge::reverbShapeMaxSeconds,0,3);control->slider.setSkewFactorFromMidPoint(1.0);control->slider.setTextValueSuffix(" s");control->slider.setNumDecimalPlacesToDisplay(2);control->slider.setDoubleClickReturnValue(true,1.0);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            // The fourth row: how deep the shape goes, how its level is bent, and how far the tone follows it.
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeID(slot,1),"DEPTH",title.getText(),0,{},0,1,0,3);control->slider.setDoubleClickReturnValue(true,1.0);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeMoreID(slot,2),"CURVE",title.getText(),0,{},-1,1,0,3);control->slider.setNumDecimalPlacesToDisplay(2);control->slider.setDoubleClickReturnValue(true,0.0);addAndMakeVisible(*control);controls.push_back(std::move(control));}
            {auto control=std::make_unique<NativeControlView>();control->bind(processor.parameters(),spectralforge::reverbShapeMoreID(slot,3),"MORPH",title.getText(),0,{},-1,1,0,3);control->slider.setNumDecimalPlacesToDisplay(2);control->slider.setDoubleClickReturnValue(true,0.0);addAndMakeVisible(*control);controls.push_back(std::move(control));}
        }
        refreshModel();setSize(740,family==2?762:270);if(family==2){timerCallback();startTimerHz(15);}
    }
    ~RackEffectDetailPanel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171b1b));spectralforge::art::rack(g,{0,0,(float)getWidth(),78},family,juce::jmax(0,model.getSelectedId()-1));
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.9f));g.fillRoundedRectangle(8,6,(float)getWidth()-16,64,4);
    }
    void resized() override {
        title.setBounds(18,8,260,18);model.setBounds(18,29,330,25);reference.setBounds(18,54,getWidth()-36,18);enabled.setBounds(getWidth()-104,24,85,28);
        // The reverb has four rows: DECAY, SIZE, DAMPING and MIX with the character's three, its added controls under them, the tempo shape in the last two.
        const size_t first=family==2?4+size_t(spectralforge::postReverbCharacterPerSlot):controls.size(),second=family==2?first+size_t(spectralforge::postReverbControlsPerSlot):first;
        const auto row=[this](size_t from,size_t to,int y){if(to<=from)return;const int cell=(getWidth()-32)/int(to-from);for(size_t i=from;i<to;++i)controls[i]->setBounds(16+int(i-from)*cell,y,cell-8,154);};
        row(0,first,92);row(first,second,256);
        // The third and fourth rows: the shape's controls on the left, four and then three, and its display beside both.
        if(family==2){const int left=getWidth()*3/5-16,cell=left/4;const size_t fourth=second+4;
            for(size_t i=second;i<controls.size();++i){const bool lower=i>=fourth;const int column=int(i-(lower?fourth:second));controls[i]->setBounds(16+column*cell,lower?584:420,cell-8,154);}
            if(shapeDisplay)shapeDisplay->setBounds(16+left+8,428,getWidth()-left-40,302);}
    }
private:
    ChimeraProcessor& processor;int family{},slot{};
    juce::String id(const juce::String& x) const {return spectralforge::postSlotID(slot,x);}juce::Label title,reference;juce::ComboBox model;juce::TextButton enabled;
    std::unique_ptr<spectralforge::ExtendedEffectModelSync> modelSync;
    std::unique_ptr<juce::ParameterAttachment> enabledAttachment;std::vector<std::unique_ptr<NativeControlView>> controls;std::unique_ptr<ReverbShapeDisplay> shapeDisplay;
    void timerCallback() override {
        // The reverb shape's length is set in bars or in seconds: the one not in use is greyed.
        if(family==2 && controls.size()>=16){const bool seconds=processor.parameters().getRawParameterValue(spectralforge::reverbShapeMoreID(slot,0))->load()>.5f;controls[14]->choice.setEnabled(!seconds);controls[15]->slider.setEnabled(seconds);}
        if(family==1 && !controls.empty()){controls.front()->slider.setEnabled(processor.parameters().getRawParameterValue(id("delaysync"))->load()<.5f);controls.front()->slider.updateText();}}
    void refreshModel(){const int selected=juce::jmax(0,model.getSelectedId()-1);reference.setText(juce::String::fromUTF8(spectralforge::modelInfo(family,selected).character),juce::dontSendNotification);reference.setTooltip(reference.getText());for(auto& control:controls)control->slider.getProperties().set("knobStyle",spectralforge::art::rackStyle(family,selected).knobStyle);repaint();}
};

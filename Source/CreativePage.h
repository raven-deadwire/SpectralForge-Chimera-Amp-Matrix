#pragma once
#include "PluginProcessor.h"
#include "CreativeFX.h"
#include "SynthPage.h"
#include "RasterArtwork.h"

// The CREATIVE page (docs/CREATIVE_PAGE.md): both CREATIVE slots on one page
// of their own. Each slot has its model tiles, the model's three controls
// under their real names, the slicer's step grid, the Laser's sweep graph,
// and the placement of the slot in the signal path. Every control is attached
// to its host parameter; opening the page or switching the shown slot changes
// nothing.
namespace spectralforge::creativeUI {
using synthUI::panelColour;using synthUI::edgeColour;using synthUI::inkColour;using synthUI::mutedColour;using synthUI::accentColour;using synthUI::traceColour;

// The slicer's sixteen steps as bars. A built-in pattern shows its levels;
// the first drag copies it into the Custom steps and switches to Custom, so
// any pattern is a starting point. Steps past the length are dimmed.
class StepGrid final : public juce::Component,public juce::SettableTooltipClient {
public:
    StepGrid(juce::AudioProcessorValueTreeState& s,int slotIndex):state(s),slot(slotIndex),pattern(s,postSlotID(slotIndex,"creativePattern")),length(s,postSlotID(slotIndex,"creativeLength")) {
        setComponentID(postSlotID(slot,"creativeStepGrid"));
        for(int i=0;i<creativeSlicerSteps;++i)steps.push_back(std::make_unique<synthUI::GraphParameter>(s,postSlotID(slot,creativeStepKey(i))));
        setTooltip("Slicer steps: drag a bar to set how far that step opens. Editing a built-in pattern copies it to Custom.");
    }
    bool custom() const {return juce::roundToInt(pattern.value())>=creativeCustomPattern;}
    int shownLength() const {return custom()?juce::jlimit(2,creativeSlicerSteps,juce::roundToInt(length.value())):creativeSlicerSteps;}
    float level(int step) const {
        if(custom())return steps[size_t(step)]->value()*.01f;
        return creativePatterns[size_t(juce::jlimit(0,creativeCustomPattern-1,juce::roundToInt(pattern.value())))].levels[size_t(step)];
    }
    juce::Rectangle<float> cell(int step) const {
        const auto r=getLocalBounds().toFloat().reduced(6,6);const float w=r.getWidth()/float(creativeSlicerSteps);
        return {r.getX()+w*float(step)+2,r.getY(),w-4,r.getHeight()};
    }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(getLocalBounds().toFloat(),4);
        g.setColour(edgeColour);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),4,1);
        const int count=shownLength();
        for(int i=0;i<creativeSlicerSteps;++i){
            const auto c=cell(i);const bool used=i<count;const float v=level(i);
            g.setColour(juce::Colour(0xff182022));g.fillRoundedRectangle(c,2);
            if(i%4==0){g.setColour(edgeColour);g.fillRect(c.getX()-2.5f,c.getY(),1.f,c.getHeight());}
            g.setColour((custom()?accentColour:traceColour).withAlpha(used?.9f:.25f));
            g.fillRoundedRectangle(c.withTop(c.getBottom()-juce::jmax(2.f,c.getHeight()*v)),2);
            g.setColour(mutedColour.withAlpha(used?1.f:.4f));g.setFont(juce::FontOptions(8.5f));
            g.drawText(juce::String(i+1),c.withHeight(11).toNearestInt(),juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(!custom()){   // start from the pattern that is showing
            const int from=juce::jlimit(0,creativeCustomPattern-1,juce::roundToInt(pattern.value()));
            for(int i=0;i<creativeSlicerSteps;++i){auto& p=*steps[size_t(i)];p.begin();p.set(creativePatterns[size_t(from)].levels[size_t(i)]*100.f);p.end();}
            length.begin();length.set(float(creativeSlicerSteps));length.end();
            pattern.begin();pattern.set(float(creativeCustomPattern));pattern.end();
        }
        touched=-1;mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const auto r=getLocalBounds().toFloat().reduced(6,6);
        const int step=juce::jlimit(0,creativeSlicerSteps-1,int((float(e.x)-r.getX())/(r.getWidth()/float(creativeSlicerSteps))));
        if(step!=touched){if(touched>=0)steps[size_t(touched)]->end();touched=step;steps[size_t(step)]->begin();}
        float v=juce::jlimit(0.f,1.f,(r.getBottom()-float(e.y))/r.getHeight());
        if(v<.04f)v=0;if(v>.96f)v=1;
        steps[size_t(step)]->set(v*100.f);repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {if(touched>=0)steps[size_t(touched)]->end();touched=-1;}
private:
    juce::AudioProcessorValueTreeState& state;int slot{};synthUI::GraphParameter pattern,length;
    std::vector<std::unique_ptr<synthUI::GraphParameter>> steps;int touched{-1};
};

// The Laser's sweep over time: semitones from the note's own pitch in the two
// pitch modes, the band-pass centre in the filter mode.
class LaserGraph final : public juce::Component {
public:
    LaserGraph(juce::AudioProcessorValueTreeState& s,int slot):mode(s,postSlotID(slot,"creativeLaserMode")),a(s,postSlotID(slot,"creativeA")),c(s,postSlotID(slot,"creativeC")) {setComponentID(postSlotID(slot,"creativeLaserGraph"));}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);g.setColour(edgeColour);g.drawRoundedRectangle(r.reduced(.5f),4,1);
        const auto plot=r.reduced(34,18);const int m=juce::roundToInt(mode.value());
        const float time=.02f+.58f*a.value()*.01f,range=m==laserFilterSweep?1.f:c.value()*.01f;
        // x: 0..0.7 s. y: +24..-24 st (pitch) or 8 kHz..150 Hz, log (filter).
        const auto x=[&](float seconds){return plot.getX()+plot.getWidth()*seconds/.7f;};
        const float middle=plot.getCentreY();
        g.setColour(edgeColour);g.drawHorizontalLine(int(m==laserFilterSweep?plot.getBottom():middle),plot.getX(),plot.getRight());
        g.drawVerticalLine(int(x(time)),plot.getY(),plot.getBottom());
        juce::Path path;
        for(int i=0;i<=140;++i){
            const float seconds=.7f*float(i)/140.f,left=seconds<time?1.f-seconds/time:0.f;float y;
            if(m==laserFilterSweep)y=plot.getY()+plot.getHeight()*(seconds<time?seconds/time:1.f);
            else y=middle-(m==laserPitchRise?-1.f:1.f)*range*left*left*plot.getHeight()*.5f;
            if(i==0)path.startNewSubPath(x(seconds),y);else path.lineTo(x(seconds),y);
        }
        g.setColour(traceColour);g.strokePath(path,juce::PathStrokeType(2.f));
        g.setColour(mutedColour);g.setFont(juce::FontOptions(9.f));
        const auto note=[&](const juce::String& text,float y){g.drawText(text,juce::Rectangle<float>(r.getX()+2,y-6,30,12),juce::Justification::centredRight);};
        if(m==laserFilterSweep){note("8 kHz",plot.getY());note("150 Hz",plot.getBottom());}
        else {note("+24",plot.getY());note("0 st",middle);note("-24",plot.getBottom());}
        g.drawText(juce::String(juce::roundToInt(time*1000.f))+" ms",juce::Rectangle<float>(x(time)-30,plot.getBottom()+2,60,12),juce::Justification::centred);
        g.drawText(m==laserFilterSweep?"Each picked note restarts the band-pass sweep":m==laserPitchRise?"Each picked note starts below its pitch and rises onto it":"Each picked note starts above its pitch and dives onto it",
                   juce::Rectangle<float>(plot.getX(),r.getY()+3,plot.getWidth(),12),juce::Justification::centredRight);
    }
private:
    synthUI::GraphParameter mode,a,c;
};

// One slot's controls. The page holds two and shows one.
class CreativeSlotView final : public juce::Component,private juce::Timer {
public:
    CreativeSlotView(ChimeraProcessor& owner,int slotIndex):processor(owner),slot(slotIndex),state(owner.parameters()),
        on(state,id("creativeOn"),"ON"),insert(state,id("creativeInsert"),{"AMP INPUT","BEFORE TONE","POST RACK","AFTER POST"},"Where this slot runs"),
        sync(state,id("creativeSync"),creativeSyncNames(),"Sync: Free, or a note value at the tempo"),
        a(state,id("creativeA"),"A"),b(state,id("creativeB"),"B"),c(state,id("creativeC"),"C"),mix(state,id("creativeMix"),"MIX"),output(state,id("creativeOutput"),"OUTPUT"),
        pattern(state,id("creativePattern"),creativePatternNames(),"Slicer pattern"),length(state,id("creativeLength"),"STEPS"),attack(state,id("creativeAttack"),"ATTACK"),
        stereo(state,id("creativeStereo"),{"MONO","ALTERNATE L / R"},"Slicer stereo: both channels together, or steps alternating left and right"),
        laserMode(state,id("creativeLaserMode"),{"PITCH DIVE","PITCH RISE","FILTER SWEEP"},"Laser mode"),grid(state,slot),laser(state,slot) {
        setComponentID(slot?"creativeSlot_2":"creativeSlot");
        for(int i=0;i<creativeModelCount;++i){
            auto tile=std::make_unique<juce::TextButton>(juce::String(creativeModels[size_t(i)].name).toUpperCase());
            tile->setComponentID(id("creativeTile")+juce::String(i));tile->setTooltip(juce::String(creativeModels[size_t(i)].name)+": "+creativeModels[size_t(i)].character);
            tile->setColour(juce::TextButton::buttonColourId,juce::Colour(creativeModels[size_t(i)].colour).darker(.55f));
            tile->setColour(juce::TextButton::buttonOnColourId,juce::Colour(creativeModels[size_t(i)].colour).brighter(.15f));
            tile->setColour(juce::TextButton::textColourOffId,inkColour);tile->setColour(juce::TextButton::textColourOnId,juce::Colours::white);
            tile->onClick=[this,i]{if(auto* p=state.getParameter(id("creativeModel"))){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(i)));p->endChangeGesture();}refresh();};
            addAndMakeVisible(*tile);tiles.push_back(std::move(tile));
        }
        for(auto* k:{&a,&b,&c}){k->setCompact();addAndMakeVisible(*k);}
        for(auto* l:{&aName,&bName,&cName}){l->setFont(juce::FontOptions(11.f,juce::Font::bold));l->setJustificationType(juce::Justification::centred);l->setColour(juce::Label::textColourId,inkColour);addAndMakeVisible(*l);}
        for(auto* l:{&name,&character,&placementNote,&syncName,&patternName}){l->setColour(juce::Label::textColourId,mutedColour);l->setFont(juce::FontOptions(10.f));addAndMakeVisible(*l);}
        name.setFont(juce::FontOptions(17.f,juce::Font::bold));name.setColour(juce::Label::textColourId,inkColour);
        syncName.setText("SYNC",juce::dontSendNotification);patternName.setText("PATTERN",juce::dontSendNotification);
        length.setIncDec();
        for(juce::Component* x:std::initializer_list<juce::Component*>{&on,&insert,&sync,&mix,&output,&pattern,&length,&attack,&stereo,&laserMode,&grid,&laser})addAndMakeVisible(*x);
        refresh();startTimerHz(15);
    }
    ~CreativeSlotView() override {stopTimer();}
    int model() const {return juce::jlimit(0,creativeModelCount-1,juce::roundToInt(state.getRawParameterValue(id("creativeModel"))->load()));}
    // The name the page shows for control 0-2 of the selected model.
    juce::String controlName(int index) const {
        const int m=model();
        if(m==3 && index==2 && juce::roundToInt(state.getRawParameterValue(id("creativeLaserMode"))->load())==laserFilterSweep)return "RESONANCE";
        return creativeModels[size_t(m)].controls[size_t(index)];
    }
    void refresh() {
        const int m=model();shown=m;
        for(int i=0;i<creativeModelCount;++i)tiles[size_t(i)]->setToggleState(i==m,juce::dontSendNotification);
        name.setText(juce::String(creativeModels[size_t(m)].name).toUpperCase(),juce::dontSendNotification);
        character.setText(creativeModels[size_t(m)].character,juce::dontSendNotification);
        aName.setText(controlName(0),juce::dontSendNotification);bName.setText(controlName(1),juce::dontSendNotification);cName.setText(controlName(2),juce::dontSendNotification);
        shownLaser=juce::roundToInt(state.getRawParameterValue(id("creativeLaserMode"))->load());
        const bool slicer=m==0,zap=m==3;
        for(juce::Component* x:std::initializer_list<juce::Component*>{&pattern,&patternName,&length,&attack,&stereo,&grid})x->setVisible(slicer);
        laserMode.setVisible(zap);laser.setVisible(zap);
        shownCustom=grid.custom();length.setEnabled(shownCustom);length.setAlpha(shownCustom?1.f:.45f);
        const int place=juce::jlimit(0,int(creativeInsertCount)-1,juce::roundToInt(state.getRawParameterValue(id("creativeInsert"))->load()));shownPlace=place;
        static const std::array<const char*,creativeInsertCount> notes{"Before the amps, like a pedal: the amp distorts the effect.","After the amps and cabinets, before TONE EQ and dynamics.","In the POST rack, at the CREATIVE block's position (drag it on the POST page).","After the whole POST rack."};
        placementNote.setText(notes[size_t(place)],juce::dontSendNotification);
        resized();repaint();
    }
    void paint(juce::Graphics& g) override {
        const auto stage=display.toFloat();
        g.setColour(panelColour);g.fillRoundedRectangle(stage,5);g.setColour(edgeColour);g.drawRoundedRectangle(stage.reduced(.5f),5,1);
        const int m=model();
        if(m!=0 && m!=3){   // the model's procedural face, with what it does in words
            art::rackSurface(g,stage.reduced(8,34),art::creativeSurface(m));
            g.setColour(juce::Colour(0xff0d1010).withAlpha(.55f));g.fillRoundedRectangle(stage.reduced(8,34),4);
        }
        g.setColour(mutedColour);g.setFont(juce::FontOptions(9.f,juce::Font::bold));
        g.drawText("PLACEMENT",placementLabel,juce::Justification::centredLeft);
    }
    void resized() override {
        auto r=getLocalBounds();
        // Left: the nine models.
        auto left=r.removeFromLeft(236);r.removeFromLeft(10);
        const int tileH=(left.getHeight()-8*5)/creativeModelCount;
        for(auto& t:tiles){t->setBounds(left.removeFromTop(tileH));left.removeFromTop(5);}
        // Bottom: placement.
        auto bottom=r.removeFromBottom(46);r.removeFromBottom(8);
        placementLabel=bottom.removeFromLeft(74);insert.setBounds(bottom.removeFromLeft(430).reduced(0,9));bottom.removeFromLeft(12);placementNote.setBounds(bottom);
        // Right: the model's three controls, then sync, mix and output.
        auto right=r.removeFromRight(300);r.removeFromRight(10);
        auto head=right.removeFromTop(34);on.setBounds(head.removeFromRight(70).reduced(0,3));
        syncName.setBounds(head.removeFromLeft(40));sync.setBounds(head.removeFromLeft(110).reduced(0,4));
        right.removeFromTop(8);
        auto big=right.removeFromTop(150);const int cell=big.getWidth()/3;
        const auto place=[&](synthUI::Knob& k,juce::Label& l){auto c=big.removeFromLeft(cell);l.setBounds(c.removeFromTop(16));k.setBounds(c.reduced(6,2));};
        place(a,aName);place(b,bName);place(c,cName);
        right.removeFromTop(10);
        auto small=right.removeFromTop(96);mix.setBounds(small.removeFromLeft(small.getWidth()/2).reduced(34,0));output.setBounds(small.reduced(34,0));
        // Centre: the model's display.
        display=r;
        auto inner=r.reduced(12,8);
        auto title=inner.removeFromTop(24);name.setBounds(title.removeFromLeft(220));character.setBounds(title);
        inner.removeFromTop(6);
        if(shown==0){
            auto row=inner.removeFromTop(28);
            patternName.setBounds(row.removeFromLeft(58));pattern.setBounds(row.removeFromLeft(170).reduced(0,2));row.removeFromLeft(10);
            length.setBounds(row.removeFromLeft(104));row.removeFromLeft(10);stereo.setBounds(row.removeFromLeft(210).reduced(0,2));
            inner.removeFromTop(8);
            attack.setBounds(inner.removeFromRight(70).withSizeKeepingCentre(64,96));inner.removeFromRight(6);
            grid.setBounds(inner);
        } else if(shown==3){
            laserMode.setBounds(inner.removeFromTop(28).removeFromLeft(360).reduced(0,2));inner.removeFromTop(8);
            laser.setBounds(inner);
        }
    }
    juce::Component* control(const juce::String& componentID) {return findChildWithID(componentID);}
    StepGrid& stepGrid() noexcept {return grid;}
private:
    juce::String id(const juce::String& x) const {return postSlotID(slot,x);}
    void timerCallback() override {
        if(!isShowing())return;
        on.setButtonText(on.getToggleState()?"ON":"OFF");
        const int place=juce::roundToInt(state.getRawParameterValue(id("creativeInsert"))->load());
        if(model()!=shown || grid.custom()!=shownCustom || place!=shownPlace || juce::roundToInt(state.getRawParameterValue(id("creativeLaserMode"))->load())!=shownLaser)refresh();
        if(grid.isVisible())grid.repaint();
        if(laser.isVisible())laser.repaint();
    }
    ChimeraProcessor& processor;int slot{};juce::AudioProcessorValueTreeState& state;
    synthUI::Toggle on;synthUI::Segmented insert;synthUI::Choice sync;synthUI::Knob a,b,c,mix,output;
    synthUI::Choice pattern;synthUI::Knob length,attack;synthUI::Segmented stereo,laserMode;StepGrid grid;LaserGraph laser;
    std::vector<std::unique_ptr<juce::TextButton>> tiles;
    juce::Label name,character,aName,bName,cName,placementNote,syncName,patternName;
    juce::Rectangle<int> display,placementLabel;int shown{-1},shownPlace{-1},shownLaser{-1};bool shownCustom{};
};

class CreativePage final : public juce::Component {
public:
    explicit CreativePage(ChimeraProcessor& owner) {
        setComponentID("creativePage");setTitle("Creative");setLookAndFeel(&look);
        title.setText("CREATIVE",juce::dontSendNotification);title.setFont(juce::FontOptions(15.f,juce::Font::bold));title.setColour(juce::Label::textColourId,inkColour);addAndMakeVisible(title);
        hint.setText("Two slots. Each one picks a model and where it runs: before the amps, after them, in the POST rack or after it.",juce::dontSendNotification);
        hint.setFont(juce::FontOptions(10.f));hint.setColour(juce::Label::textColourId,mutedColour);addAndMakeVisible(hint);
        for(int i=0;i<2;++i){
            views[size_t(i)]=std::make_unique<CreativeSlotView>(owner,i);addChildComponent(*views[size_t(i)]);
            auto& tab=tabs[size_t(i)];tab.setButtonText(i?"SLOT 2":"SLOT 1");tab.setComponentID(i?"creativePage.slot2":"creativePage.slot1");
            tab.setTooltip(i?"Show the second CREATIVE slot":"Show the first CREATIVE slot");tab.setClickingTogglesState(false);
            tab.setColour(juce::TextButton::buttonOnColourId,accentColour);tab.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff111414));
            tab.setConnectedEdges(i?juce::Button::ConnectedOnLeft:juce::Button::ConnectedOnRight);
            tab.onClick=[this,i]{showSlot(i);};addAndMakeVisible(tab);
        }
        showSlot(0);
    }
    ~CreativePage() override {setLookAndFeel(nullptr);}
    // Display only: which slot's controls are on screen.
    void showSlot(int slot) {
        shown=juce::jlimit(0,1,slot);
        for(int i=0;i<2;++i){views[size_t(i)]->setVisible(i==shown);tabs[size_t(i)].setToggleState(i==shown,juce::dontSendNotification);}
        if(views[size_t(shown)])views[size_t(shown)]->refresh();
    }
    int shownSlot() const noexcept {return shown;}
    CreativeSlotView& view(int slot) {return *views[size_t(slot)];}
    juce::Component* control(const juce::String& componentID) {
        for(auto& v:views)if(auto* c=v->control(componentID))return c;
        return findChildWithID(componentID);
    }
    void paint(juce::Graphics& g) override {g.setColour(juce::Colour(0xff141a1b));g.fillRoundedRectangle(getLocalBounds().toFloat(),6);}
    void resized() override {
        auto r=getLocalBounds().reduced(12,8);
        auto top=r.removeFromTop(28);title.setBounds(top.removeFromLeft(110));
        tabs[0].setBounds(top.removeFromLeft(74).reduced(0,2));tabs[1].setBounds(top.removeFromLeft(74).reduced(0,2));top.removeFromLeft(14);hint.setBounds(top);
        r.removeFromTop(8);
        for(auto& v:views)v->setBounds(r);
    }
private:
    synthUI::SynthLook look;   // declared first: destroyed after every child that uses it
    juce::Label title,hint;std::array<juce::TextButton,2> tabs;std::array<std::unique_ptr<CreativeSlotView>,2> views;int shown{};
};
} // namespace spectralforge::creativeUI

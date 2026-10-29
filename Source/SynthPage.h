#pragma once
#include "PluginProcessor.h"
#include "SynthLayer.h"

// The SYNTH page (docs/SYNTH_PAGE.md): oscillators with live wave displays, a
// draggable filter response, draggable ADSR graphs, LFO shapes, the
// modulation matrix and a tracking / output scope. Every control is attached
// to its host parameter; graph drags are complete host gestures. Opening the
// page changes nothing.
namespace spectralforge::synthUI {
inline const juce::Colour panelColour{0xff1b2224}, edgeColour{0xff3a4a4c}, inkColour{0xffe1dbce}, mutedColour{0xff9caaa5}, accentColour{0xffd7a35a}, traceColour{0xff8fd0c0};

inline juce::String noteName(double hz) {
    if(hz<=0)return "--";
    const double midi=69+12*std::log2(hz/440.0);const int note=juce::roundToInt(midi);
    static const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    const int cents=juce::roundToInt(100*(midi-note));
    return juce::String(names[(note%12+12)%12])+juce::String(note/12-1)+(cents>=0?" +":" ")+juce::String(cents)+" ct";
}

// A parameter written from a graph drag: one host gesture per drag.
class GraphParameter {
public:
    GraphParameter(juce::AudioProcessorValueTreeState& state,const juce::String& id):parameter(state.getParameter(id)) {}
    float value() const {return parameter?parameter->convertFrom0to1(parameter->getValue()):0.f;}
    void begin(){if(parameter&&!dragging){parameter->beginChangeGesture();dragging=true;}}
    void set(float v){if(parameter)parameter->setValueNotifyingHost(parameter->convertTo0to1(parameter->getNormalisableRange().snapToLegalValue(v)));}
    void end(){if(parameter&&dragging){parameter->endChangeGesture();dragging=false;}}
    juce::NormalisableRange<float> range() const {return parameter?parameter->getNormalisableRange():juce::NormalisableRange<float>{};}
private:
    juce::RangedAudioParameter* parameter{};bool dragging{};
};

class Knob final : public juce::Component {
public:
    Knob(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::String& text) {
        slider.setComponentID(id);slider.setName(text);slider.setTooltip(text);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,58,14);
        slider.setColour(juce::Slider::rotarySliderFillColourId,accentColour);slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,inkColour);slider.setScrollWheelEnabled(false);
        label.setText(text,juce::dontSendNotification);label.setFont(juce::FontOptions(9.f,juce::Font::bold));label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,mutedColour);label.setInterceptsMouseClicks(false,false);
        addAndMakeVisible(label);addAndMakeVisible(slider);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,id,slider);
    }
    void resized() override {auto r=getLocalBounds();label.setBounds(r.removeFromTop(11));slider.setBounds(r);}
    juce::Slider slider;
private:
    juce::Label label;std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
class Choice final : public juce::ComboBox {
public:
    Choice(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::StringArray& items,const juce::String& tip) {
        setComponentID(id);setTooltip(tip);setName(tip);addItemList(items,1);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,id,*this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
class Toggle final : public juce::TextButton {
public:
    Toggle(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::String& text):juce::TextButton(text) {
        setComponentID(id);setClickingTogglesState(true);setTooltip(text);
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffa5c1a0));setColour(juce::TextButton::textColourOnId,juce::Colour(0xff111414));
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,id,*this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// One or two cycles of the oscillator's current wave position.
class WaveDisplay final : public juce::Component {
public:
    WaveDisplay(juce::AudioProcessorValueTreeState& s,int oscillator):state(s),osc(oscillator) {setComponentID(synthOscID(osc,"Display"));}
    float wave() const {return state.getRawParameterValue(synthOscID(osc,"Wave"))->load();}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        g.setColour(edgeColour);g.drawHorizontalLine(int(r.getCentreY()),r.getX()+4,r.getRight()-4);
        const bool on=state.getRawParameterValue(synthOscID(osc,"On"))->load()>.5f;
        juce::Path path;const float w=r.getWidth()-8,h=r.getHeight()*.38f;
        for(int i=0;i<=200;++i){const double phase=2.0*i/200.0;const float x=r.getX()+4+w*float(i)/200.f,y=r.getCentreY()-h*float(synthWaveDisplay(wave(),phase));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        g.setColour(on?traceColour:mutedColour.withAlpha(.4f));g.strokePath(path,juce::PathStrokeType(1.8f));
        g.setColour(mutedColour);g.setFont(juce::FontOptions(9.f));
        static const char* names[]{"SINE","TRI","SAW","SQUARE","PULSE"};
        g.drawText(names[juce::jlimit(0,4,juce::roundToInt(wave()))],getLocalBounds().reduced(6,2),juce::Justification::topRight);
    }
private:
    juce::AudioProcessorValueTreeState& state;int osc;
};

// Filter response on a log axis; drag horizontally for cutoff, vertically for resonance.
class FilterCurve final : public juce::Component,public juce::SettableTooltipClient {
public:
    FilterCurve(juce::AudioProcessorValueTreeState& s,double sampleRate):state(s),cutoff(s,"synthCutoff"),resonance(s,"synthResonance"),rate(sampleRate) {
        setComponentID("synthFilterCurve");setTitle("Filter response");setTooltip("Drag left / right for cutoff, up / down for resonance.");
    }
    static float xForHz(float hz,float width){return width*float(std::log(hz/20.f)/std::log(1000.f));}
    static float hzForX(float x,float width){return 20.f*std::pow(1000.f,juce::jlimit(0.f,1.f,x/width));}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        g.setColour(edgeColour.withAlpha(.6f));for(const float hz:{100.f,1000.f,10000.f})g.drawVerticalLine(int(xForHz(hz,r.getWidth())),r.getY(),r.getBottom());
        const int type=juce::roundToInt(state.getRawParameterValue("synthFilterType")->load());
        const bool on=state.getRawParameterValue("synthFilterOn")->load()>.5f;
        juce::Path path;
        for(int x=0;x<=int(r.getWidth());x+=2){
            const double db=juce::Decibels::gainToDecibels(synthFilterMagnitude(type,cutoff.value(),resonance.value()*.01,hzForX(float(x),r.getWidth()),rate),-60.0);
            const float y=juce::jmap(float(juce::jlimit(-36.0,24.0,db)),-36.f,24.f,r.getBottom()-2,r.getY()+2);
            if(x==0)path.startNewSubPath(float(x),y);else path.lineTo(float(x),y);
        }
        g.setColour(on?accentColour:mutedColour.withAlpha(.4f));g.strokePath(path,juce::PathStrokeType(2.f));
        const float handle=xForHz(cutoff.value(),r.getWidth());g.setColour(inkColour);g.fillEllipse(handle-4,r.getY()+r.getHeight()*(1.f-resonance.value()*.01f)*.8f+2,8,8);
        g.setColour(mutedColour);g.setFont(juce::FontOptions(9.f));g.drawText(juce::String(juce::roundToInt(cutoff.value()))+" Hz / "+juce::String(juce::roundToInt(resonance.value()))+" %",getLocalBounds().reduced(6,2),juce::Justification::topRight);
    }
    void mouseDown(const juce::MouseEvent& e) override {cutoff.begin();resonance.begin();apply(e);}
    void mouseDrag(const juce::MouseEvent& e) override {apply(e);}
    void mouseUp(const juce::MouseEvent&) override {cutoff.end();resonance.end();}
    void refresh(){repaint();}
private:
    void apply(const juce::MouseEvent& e){
        cutoff.set(hzForX(float(e.x),float(getWidth())));
        resonance.set(100.f*juce::jlimit(0.f,1.f,1.f-(float(e.y)-2.f)/(float(getHeight())*.8f)));
        repaint();
    }
    juce::AudioProcessorValueTreeState& state;GraphParameter cutoff,resonance;double rate;
};

// ADSR graph with three handles: attack peak (time), decay / sustain point
// (time and level) and release end (time).
class EnvelopeGraph final : public juce::Component,public juce::SettableTooltipClient {
public:
    EnvelopeGraph(juce::AudioProcessorValueTreeState& s,const juce::String& base):state(s),attack(s,base+"Attack"),decay(s,base+"Decay"),sustain(s,base+"Sustain"),release(s,base+"Release"),prefix(base) {
        setComponentID(base+"Graph");setTitle("Envelope");setTooltip("Drag the points: attack time, decay time and sustain level, release time.");
    }
    // Time axis: each stage takes a quarter of the width at its maximum (log scale).
    static float span(float ms,float lo,float hi,float quarter){return quarter*float(std::log(ms/lo)/std::log(hi/lo));}
    static float msFor(float x,float lo,float hi,float quarter){return lo*std::pow(hi/lo,juce::jlimit(0.f,1.f,x/quarter));}
    juce::Point<float> attackPoint() const {return {2+span(attack.value(),.5f,5000.f,q()),2.f};}
    juce::Point<float> sustainPoint() const {const auto a=attackPoint();return {a.x+span(decay.value(),5.f,10000.f,q()),level(sustain.value()*.01f)};}
    juce::Point<float> releasePoint() const {const auto d=sustainPoint();return {d.x+q()*.6f+span(release.value(),5.f,10000.f,q()),float(getHeight()-2)};}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        const auto a=attackPoint(),d=sustainPoint(),e=releasePoint();const juce::Point<float> start{2.f,float(getHeight()-2)},hold{d.x+q()*.6f,d.y};
        juce::Path path;path.startNewSubPath(start);path.lineTo(a);
        for(int i=1;i<=24;++i){const float t=float(i)/24.f;path.lineTo(a.x+(d.x-a.x)*t,level(sustain.value()*.01f+(1.f-sustain.value()*.01f)*std::exp(-6.9f*t)));}
        path.lineTo(hold);
        for(int i=1;i<=24;++i){const float t=float(i)/24.f;path.lineTo(hold.x+(e.x-hold.x)*t,level(sustain.value()*.01f*std::exp(-6.9f*t)));}
        g.setColour(accentColour);g.strokePath(path,juce::PathStrokeType(1.8f));
        g.setColour(inkColour);for(const auto p:{a,d,e})g.fillEllipse(p.x-4,p.y-4,8,8);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const auto p=e.position;float best=1e9f;which=-1;
        const std::array<juce::Point<float>,3> points{attackPoint(),sustainPoint(),releasePoint()};
        for(int i=0;i<3;++i){const float d=points[size_t(i)].getDistanceFrom(p);if(d<best){best=d;which=i;}}
        (which==0?attack:which==1?decay:release).begin();if(which==1)sustain.begin();
        mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const float x=e.position.x;
        if(which==0)attack.set(msFor(x-2,.5f,5000.f,q()));
        else if(which==1){decay.set(msFor(x-attackPoint().x,5.f,10000.f,q()));sustain.set(100.f*juce::jlimit(0.f,1.f,(float(getHeight()-2)-e.position.y)/float(getHeight()-4)));}
        else if(which==2)release.set(msFor(x-sustainPoint().x-q()*.6f,5.f,10000.f,q()));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {attack.end();decay.end();sustain.end();release.end();which=-1;}
private:
    float q() const {return float(getWidth()-4)/3.6f;}
    float level(float v) const {return float(getHeight()-2)-v*float(getHeight()-4);}
    juce::AudioProcessorValueTreeState& state;GraphParameter attack,decay,sustain,release;juce::String prefix;int which{-1};
};

class LfoGraph final : public juce::Component {
public:
    LfoGraph(juce::AudioProcessorValueTreeState& s,ChimeraProcessor& p,int index):state(s),processor(p),lfo(index) {setComponentID("synthLfo"+juce::String(lfo+1)+"Graph");}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        const int shape=juce::roundToInt(state.getRawParameterValue("synthLfo"+juce::String(lfo+1)+"Shape")->load());
        juce::Path path;juce::Random steps(7);double held=0;
        for(int i=0;i<=120;++i){const double phase=2.0*i/120.0;if(shape==4&&i%60==0)held=steps.nextDouble()*2-1;const float x=r.getX()+3+(r.getWidth()-6)*float(i)/120.f,y=r.getCentreY()-r.getHeight()*.38f*float(synthLfoShape(shape,phase,held));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        g.setColour(traceColour);g.strokePath(path,juce::PathStrokeType(1.5f));
        const float now=float(processor.synthEngine().lfoOutput(lfo));g.setColour(accentColour);g.fillEllipse(r.getRight()-10,r.getCentreY()-r.getHeight()*.38f*now-3,6,6);
    }
private:
    juce::AudioProcessorValueTreeState& state;ChimeraProcessor& processor;int lfo;
};

// Tracking readout, level and the synth's output waveform.
class Scope final : public juce::Component {
public:
    explicit Scope(ChimeraProcessor& p):processor(p) {setComponentID("synthScope");}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        const auto& engine=processor.synthEngine();const auto& tracker=engine.pitchTracker();
        g.setColour(inkColour);g.setFont(juce::FontOptions(10.f,juce::Font::bold));
        g.drawText("TRACK  "+noteName(processor.synthTrackedFrequency()),getLocalBounds().reduced(6,2).removeFromTop(14),juce::Justification::centredLeft);
        g.setColour(mutedColour);g.setFont(juce::FontOptions(9.f));
        g.drawText("CLARITY "+juce::String(tracker.clarity(),2)+"  VOICES "+juce::String(engine.soundingVoices()),getLocalBounds().reduced(6,2).withTrimmedTop(14).removeFromTop(12),juce::Justification::centredLeft);
        const float level=juce::jlimit(0.f,1.f,float(juce::Decibels::gainToDecibels(tracker.rms(),-60.0)+60.0)/60.f);
        g.setColour(edgeColour);g.fillRect(6.f,30.f,r.getWidth()-12,4.f);g.setColour(traceColour);g.fillRect(6.f,30.f,(r.getWidth()-12)*level,4.f);
        std::array<float,512> samples{};engine.readScope(samples.data(),int(samples.size()));
        juce::Path path;const float top=40,h=r.getHeight()-top-4;
        for(size_t i=0;i<samples.size();++i){const float x=4+(r.getWidth()-8)*float(i)/float(samples.size()-1),y=top+h*.5f-h*.45f*juce::jlimit(-1.f,1.f,samples[i]);if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        g.setColour(accentColour);g.strokePath(path,juce::PathStrokeType(1.2f));
    }
private:
    ChimeraProcessor& processor;
};

class SynthPage final : public juce::Component,private juce::Timer {
public:
    explicit SynthPage(ChimeraProcessor& owner):processor(owner),state(owner.parameters()),filter(state,owner.getSampleRate()>0?owner.getSampleRate():48000.0),
        ampEnvelope(state,"synthAmp"),modEnvelope(state,"synthModEnv"),lfoGraphs{LfoGraph(state,owner,0),LfoGraph(state,owner,1)},scope(owner) {
        setComponentID("synthPage");setTitle("Synth");
        // Header
        add(std::make_unique<Toggle>(state,"synthOn","SYNTH ON"));
        add(std::make_unique<Choice>(state,"synthSource",synthSourceNames(),"Played by the guitar, MIDI or both (a held MIDI note wins)"));
        add(std::make_unique<Choice>(state,"synthInsert",synthInsertNames(),"Where the synth joins the guitar signal"));
        patch.setComponentID("synthPatch");patch.setTooltip("Load a patch: every synth parameter is set, one host gesture each");
        {int id=1;for(const auto& p:synthPatches())patch.addItem(p.name,id++);}
        patch.setTextWhenNothingSelected("PATCH");patch.onChange=[this]{applyPatch(patch.getSelectedId()-1);};addAndMakeVisible(patch);
        for(const auto* id:{"synthVoices","synthOctave","synthSensitivity"}){auto s=std::make_unique<Knob>(state,id,juce::String(id)=="synthVoices"?"VOICES":juce::String(id)=="synthOctave"?"OCTAVE":"THRESHOLD");header.push_back(s.get());add(std::move(s));}
        // Oscillators
        for(int o=0;o<2;++o){
            auto& p=oscPanels[size_t(o)];
            p.on=add(std::make_unique<Toggle>(state,synthOscID(o,"On"),o==0?"OSC A":"OSC B"));
            p.display=std::make_unique<WaveDisplay>(state,o);addAndMakeVisible(*p.display);
            for(const auto& [field,text]:std::initializer_list<std::pair<const char*,const char*>>{{"Wave","WAVE"},{"Octave","OCT"},{"Semi","SEMI"},{"Fine","FINE"},{"Unison","UNISON"},{"Detune","DETUNE"},{"Level","LEVEL"}})
                p.knobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,synthOscID(o,field),text))));
        }
        subOn=add(std::make_unique<Toggle>(state,"synthSubOn","SUB"));
        subOctave=add(std::make_unique<Choice>(state,"synthSubOctave",juce::StringArray{"-1 oct","-2 oct"},"Sub octave"));
        subShape=add(std::make_unique<Choice>(state,"synthSubShape",juce::StringArray{"Sine","Square"},"Sub shape"));
        subLevel=static_cast<Knob*>(add(std::make_unique<Knob>(state,"synthSubLevel","LEVEL")));
        noiseOn=add(std::make_unique<Toggle>(state,"synthNoiseOn","NOISE"));
        noiseColor=static_cast<Knob*>(add(std::make_unique<Knob>(state,"synthNoiseColor","COLOUR")));
        noiseLevel=static_cast<Knob*>(add(std::make_unique<Knob>(state,"synthNoiseLevel","LEVEL")));
        // Filter
        filterOn=add(std::make_unique<Toggle>(state,"synthFilterOn","FILTER"));
        filterType=add(std::make_unique<Choice>(state,"synthFilterType",synthFilterTypeNames(),"Filter type"));
        addAndMakeVisible(filter);
        for(const auto& [id,text]:std::initializer_list<std::pair<const char*,const char*>>{{"synthCutoff","CUTOFF"},{"synthResonance","RES"},{"synthDrive","DRIVE"},{"synthKeyTrack","KEY"},{"synthFilterEnv","ENV 2"}})
            filterKnobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,id,text))));
        // Envelopes
        addAndMakeVisible(ampEnvelope);addAndMakeVisible(modEnvelope);
        for(int e=0;e<2;++e){const juce::String base=e==0?"synthAmp":"synthModEnv";
            for(const auto& [field,text]:std::initializer_list<std::pair<const char*,const char*>>{{"Attack","A"},{"Decay","D"},{"Sustain","S"},{"Release","R"}})
                envKnobs[size_t(e)].push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,base+field,text))));}
        // LFOs
        for(int l=0;l<2;++l){const juce::String base="synthLfo"+juce::String(l+1);
            lfoShape[size_t(l)]=add(std::make_unique<Choice>(state,base+"Shape",synthLfoShapeNames(),"LFO "+juce::String(l+1)+" shape"));
            lfoSync[size_t(l)]=add(std::make_unique<Choice>(state,base+"Sync",synthLfoSyncNames(),"LFO "+juce::String(l+1)+" tempo sync"));
            lfoRate[size_t(l)]=static_cast<Knob*>(add(std::make_unique<Knob>(state,base+"Rate","RATE")));
            addAndMakeVisible(lfoGraphs[size_t(l)]);}
        // Modulation matrix
        for(int m=0;m<synthModSlots;++m){
            auto& row=modRows[size_t(m)];
            row.source=add(std::make_unique<Choice>(state,synthModID(m,"Source"),synthModSourceNames(),"Mod "+juce::String(m+1)+" source"));
            row.target=add(std::make_unique<Choice>(state,synthModID(m,"Target"),synthModTargetNames(),"Mod "+juce::String(m+1)+" target"));
            auto amount=std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,juce::Slider::TextBoxRight);
            amount->setComponentID(synthModID(m,"Amount"));amount->setTextBoxStyle(juce::Slider::TextBoxRight,false,34,16);amount->setTooltip("Mod "+juce::String(m+1)+" amount");
            amount->setColour(juce::Slider::trackColourId,accentColour);amount->setScrollWheelEnabled(false);
            row.amountAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,synthModID(m,"Amount"),*amount);
            row.amount=static_cast<juce::Slider*>(add(std::move(amount)));
        }
        // Scope and master
        addAndMakeVisible(scope);
        for(const auto& [id,text]:std::initializer_list<std::pair<const char*,const char*>>{{"synthLevel","LEVEL"},{"synthGlide","GLIDE"},{"synthWidth","WIDTH"},{"synthFM","FM B>A"}})
            masterKnobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,id,text))));
        startTimerHz(20);
    }
    ~SynthPage() override {stopTimer();}
    // Writes a patch: every synth parameter except on / source / insert / level, each as one gesture.
    void applyPatch(int index) {
        const auto patches=synthPatches();if(index<0||index>=int(patches.size()))return;
        static const juce::StringArray kept{"synthOn","synthSource","synthInsert","synthLevel","synthSensitivity"};
        for(auto* raw:processor.getParameters()){
            auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw);if(parameter==nullptr||!parameter->getParameterID().startsWith("synth")||kept.contains(parameter->getParameterID()))continue;
            float value=parameter->convertFrom0to1(parameter->getDefaultValue());
            for(const auto& [id,v]:patches[size_t(index)].values)if(id==parameter->getParameterID())value=v;
            const float normal=parameter->convertTo0to1(value);if(std::abs(normal-parameter->getValue())<1e-6f)continue;
            parameter->beginChangeGesture();parameter->setValueNotifyingHost(normal);parameter->endChangeGesture();
        }
        repaint();
    }
    void paint(juce::Graphics& g) override {
        for(const auto& [area,title]:panels()){
            g.setColour(panelColour);g.fillRoundedRectangle(area.toFloat(),5);g.setColour(edgeColour);g.drawRoundedRectangle(area.toFloat().reduced(.5f),5,1);
            g.setColour(mutedColour);g.setFont(juce::FontOptions(10.f,juce::Font::bold));g.drawText(title,area.reduced(8,4).removeFromTop(14),juce::Justification::centredRight);
        }
    }
    void resized() override {
        auto top=getLocalBounds().removeFromTop(26);
        top.removeFromLeft(300);
        children[0]->setBounds(top.removeFromLeft(86).reduced(0,1));top.removeFromLeft(6);
        children[1]->setBounds(top.removeFromLeft(130).reduced(0,1));top.removeFromLeft(6);
        children[2]->setBounds(top.removeFromLeft(130).reduced(0,1));top.removeFromLeft(6);
        patch.setBounds(top.removeFromLeft(140).reduced(0,1));top.removeFromLeft(6);
        for(auto* k:header){k->slider.setSliderStyle(juce::Slider::IncDecButtons);k->slider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,52,22);k->setBounds(top.removeFromLeft(110));top.removeFromLeft(4);}
        const auto p=panels();
        for(int o=0;o<2;++o){
            auto r=p[size_t(o)].first.reduced(8,6);auto& panel=oscPanels[size_t(o)];
            panel.on->setBounds(r.removeFromTop(18).removeFromLeft(70));r.removeFromTop(4);
            panel.display->setBounds(r.removeFromTop(56));r.removeFromTop(4);
            auto row1=r.removeFromTop(56),row2=r;
            for(int k=0;k<4;++k)panel.knobs[size_t(k)]->setBounds(row1.removeFromLeft(row1.getWidth()/(4-k)));
            for(int k=4;k<7;++k)panel.knobs[size_t(k)]->setBounds(row2.removeFromLeft(row2.getWidth()/(7-k)));
        }
        {auto r=p[2].first.reduced(8,6);auto sub=r.removeFromTop(r.getHeight()/2);
         subOn->setBounds(sub.removeFromTop(18).removeFromLeft(50));sub.removeFromTop(3);
         auto menus=sub.removeFromTop(20);subOctave->setBounds(menus.removeFromLeft(menus.getWidth()/2-2));menus.removeFromLeft(4);subShape->setBounds(menus);
         subLevel->setBounds(sub.withSizeKeepingCentre(60,sub.getHeight()));
         auto headNoise=r.removeFromTop(18);noiseOn->setBounds(headNoise.removeFromLeft(60));
         noiseColor->setBounds(r.removeFromLeft(r.getWidth()/2));noiseLevel->setBounds(r);}
        {auto r=p[3].first.reduced(8,6);auto head=r.removeFromTop(18);filterOn->setBounds(head.removeFromLeft(70));head.removeFromLeft(6);filterType->setBounds(head.removeFromLeft(120));
         r.removeFromTop(4);filter.setBounds(r.removeFromTop(r.getHeight()-60));r.removeFromTop(4);
         for(size_t k=0;k<filterKnobs.size();++k)filterKnobs[k]->setBounds(r.removeFromLeft(r.getWidth()/int(filterKnobs.size()-k)));}
        for(int e=0;e<2;++e){auto r=p[size_t(4+e)].first.reduced(8,6);r.removeFromTop(16);
            (e==0?ampEnvelope:modEnvelope).setBounds(r.removeFromTop(r.getHeight()-60));r.removeFromTop(4);
            for(size_t k=0;k<4;++k)envKnobs[size_t(e)][k]->setBounds(r.removeFromLeft(r.getWidth()/int(4-k)));}
        {auto r=p[6].first.reduced(8,6);
         for(int l=0;l<2;++l){auto half=r.removeFromTop(r.getHeight()/(2-l));auto head=half.removeFromTop(18);head.removeFromRight(42);
             lfoShape[size_t(l)]->setBounds(head.removeFromLeft(head.getWidth()/2-2));head.removeFromLeft(4);lfoSync[size_t(l)]->setBounds(head);
             lfoRate[size_t(l)]->setBounds(half.removeFromRight(60));lfoGraphs[size_t(l)].setBounds(half.reduced(0,3));}}
        {auto r=p[7].first.reduced(8,6);r.removeFromTop(16);const int h=r.getHeight()/synthModSlots;
         for(int m=0;m<synthModSlots;++m){auto row=r.removeFromTop(h).reduced(0,1);auto& mr=modRows[size_t(m)];
             mr.source->setBounds(row.removeFromLeft(row.getWidth()*3/10));row.removeFromLeft(3);mr.target->setBounds(row.removeFromLeft(row.getWidth()*45/100));row.removeFromLeft(3);mr.amount->setBounds(row);}}
        {auto r=p[8].first.reduced(8,6);r.removeFromTop(16);scope.setBounds(r.removeFromTop(r.getHeight()-60));r.removeFromTop(4);
         for(size_t k=0;k<masterKnobs.size();++k)masterKnobs[k]->setBounds(r.removeFromLeft(r.getWidth()/int(masterKnobs.size()-k)));}
    }
    juce::Component* control(const juce::String& id) {for(auto* c:getChildren())if(c->getComponentID()==id)return c;for(auto& k:owned)if(auto* knob=dynamic_cast<Knob*>(k.get()))if(knob->slider.getComponentID()==id)return &knob->slider;return nullptr;}
    FilterCurve& filterCurve() noexcept {return filter;}
    EnvelopeGraph& envelopeGraph(int index) noexcept {return index==0?ampEnvelope:modEnvelope;}
    WaveDisplay& waveDisplay(int osc) noexcept {return *oscPanels[size_t(juce::jlimit(0,1,osc))].display;}
private:
    // Panels: four across the top, five across the bottom.
    std::array<std::pair<juce::Rectangle<int>,const char*>,9> panels() const {
        const int top=30,w=getWidth(),h=getHeight()-top,rowA=h*49/100,gap=6;
        const int y2=top+rowA+gap,h2=h-rowA-gap;
        const auto col=[&](int x,int width,int y,int height){return juce::Rectangle<int>(x,y,width-gap,height);};
        const int a0=w*24/100,a1=w*24/100,a2=w*14/100,a3=w-a0-a1-a2;
        const int b0=w*19/100,b1=w*19/100,b2=w*17/100,b3=w*25/100,b4=w-b0-b1-b2-b3;
        return {{{col(0,a0,top,rowA),"OSC A"},{col(a0,a1,top,rowA),"OSC B"},{col(a0+a1,a2,top,rowA),"SUB / NOISE"},{col(a0+a1+a2,a3+gap,top,rowA),"FILTER"},
                 {col(0,b0,y2,h2),"ENV 1  AMP"},{col(b0,b1,y2,h2),"ENV 2  MOD"},{col(b0+b1,b2,y2,h2),"LFO 1 / 2"},{col(b0+b1+b2,b3,y2,h2),"MOD MATRIX"},{col(b0+b1+b2+b3,b4+gap,y2,h2),"SCOPE / MASTER"}}};
    }
    template<class C> C* add(std::unique_ptr<C> c){auto* raw=c.get();addAndMakeVisible(*raw);children.push_back(raw);owned.push_back(std::move(c));return raw;}
    void timerCallback() override {
        if(!isShowing())return;
        for(auto& p:oscPanels)p.display->repaint();
        filter.repaint();ampEnvelope.repaint();modEnvelope.repaint();for(auto& l:lfoGraphs)l.repaint();scope.repaint();
    }
    struct OscPanel {juce::Component* on{};std::unique_ptr<WaveDisplay> display;std::vector<Knob*> knobs;};
    struct ModRow {juce::Component* source{};juce::Component* target{};juce::Slider* amount{};std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;};
    ChimeraProcessor& processor;juce::AudioProcessorValueTreeState& state;
    std::vector<std::unique_ptr<juce::Component>> owned;std::vector<juce::Component*> children;std::vector<Knob*> header;
    juce::ComboBox patch;
    std::array<OscPanel,2> oscPanels;
    juce::Component* subOn{};juce::Component* subOctave{};juce::Component* subShape{};Knob* subLevel{};
    juce::Component* noiseOn{};Knob* noiseColor{};Knob* noiseLevel{};
    juce::Component* filterOn{};juce::Component* filterType{};FilterCurve filter;std::vector<Knob*> filterKnobs;
    EnvelopeGraph ampEnvelope,modEnvelope;std::array<std::vector<Knob*>,2> envKnobs;
    std::array<juce::Component*,2> lfoShape{},lfoSync{};std::array<Knob*,2> lfoRate{};std::array<LfoGraph,2> lfoGraphs;
    std::array<ModRow,synthModSlots> modRows;
    Scope scope;std::vector<Knob*> masterKnobs;
};
} // namespace spectralforge::synthUI

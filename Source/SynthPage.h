#pragma once
#include "PluginProcessor.h"
#include "SynthLayer.h"
#include "SynthSampleIO.h"

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

// The page's look: ring knobs that fill their space, value boxes in ink,
// a dark bar for horizontal bar sliders.
class SynthLook final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,float start,float end,juce::Slider& slider) override {
        const auto r=juce::Rectangle<int>(x,y,width,height).toFloat().reduced(2);
        const float radius=juce::jmin(r.getWidth(),r.getHeight())*.5f-1.5f,thick=juce::jlimit(2.f,4.f,radius*.22f);
        if(radius<3)return;
        const auto c=r.getCentre();const float angle=start+position*(end-start);
        juce::Path back,value;back.addCentredArc(c.x,c.y,radius,radius,0,start,end,true);value.addCentredArc(c.x,c.y,radius,radius,0,start,angle,true);
        g.setColour(edgeColour);g.strokePath(back,juce::PathStrokeType(thick,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(slider.isEnabled()?accentColour:mutedColour);g.strokePath(value,juce::PathStrokeType(thick,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const auto tip=c.getPointOnCircumference(radius-thick*1.6f,angle);
        g.setColour(inkColour);g.drawLine({c.getPointOnCircumference(radius*.25f,angle),tip},1.6f);
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float minimum,float maximum,juce::Slider::SliderStyle style,juce::Slider& slider) override {
        if(style!=juce::Slider::LinearBar){juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,position,minimum,maximum,style,slider);return;}
        const auto r=juce::Rectangle<int>(x,y,width,height).toFloat();
        g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,3);
        g.setColour(accentColour.withAlpha(.4f));g.fillRoundedRectangle(r.withRight(position),3);
        g.setColour(edgeColour);g.drawRoundedRectangle(r.reduced(.5f),3,1);
    }
    juce::Label* createSliderTextBox(juce::Slider& slider) override {
        auto* label=juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setColour(juce::Label::textColourId,inkColour);label->setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
        label->setColour(juce::Label::outlineColourId,juce::Colours::transparentBlack);label->setFont(juce::FontOptions(11.f,juce::Font::bold));
        return label;
    }
};

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

// A rotary control with its name above and its value below (double-click
// the value to type one). setIncDec() turns it into - / + buttons beside the
// value, setCompact() drops the name.
class Knob final : public juce::Component {
public:
    Knob(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::String& text) {
        slider.setComponentID(id);slider.setName(text);slider.setTooltip(text);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        slider.setColour(juce::Slider::rotarySliderFillColourId,accentColour);slider.setColour(juce::Slider::rotarySliderOutlineColourId,edgeColour);
        slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId,inkColour);slider.setScrollWheelEnabled(false);
        label.setText(text,juce::dontSendNotification);label.setFont(juce::FontOptions(9.f,juce::Font::bold));label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId,mutedColour);label.setInterceptsMouseClicks(false,false);label.setBorderSize({});
        value.setFont(juce::FontOptions(9.5f));value.setJustificationType(juce::Justification::centred);value.setColour(juce::Label::textColourId,inkColour);
        value.setBorderSize({});value.setEditable(false,true,false);value.setTooltip(text+": double-click to type a value");
        value.onTextChange=[this]{slider.setValue(slider.getValueFromText(value.getText()),juce::sendNotificationSync);showValue();};
        addAndMakeVisible(label);addAndMakeVisible(slider);addAndMakeVisible(value);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,id,slider);
        slider.onValueChange=[this]{showValue();};showValue();
    }
    void resized() override {
        auto r=getLocalBounds();
        if(label.isVisible())label.setBounds(r.removeFromTop(10));
        if(incDec){value.setFont(juce::FontOptions(11.f,juce::Font::bold));slider.setBounds(r.removeFromRight(34));value.setBounds(r);return;}
        value.setBounds(r.removeFromBottom(11));
        slider.setBounds(r);
    }
    void setIncDec(const juce::String& suffix={}){
        if(incDec)return;
        incDec=true;slider.setTextValueSuffix(suffix);slider.setSliderStyle(juce::Slider::IncDecButtons);slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        showValue();resized();
    }
    void setCompact(){label.setVisible(false);resized();}
    juce::Slider slider;
private:
    void showValue(){value.setText(slider.getTextFromValue(slider.getValue()),juce::dontSendNotification);}
    juce::Label label,value;std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;bool incDec{};
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

// A choice parameter as a row of buttons (engine type, oscillator mode).
// Each click is one host gesture.
class Segmented final : public juce::Component {
public:
    Segmented(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::StringArray& labels,const juce::String& tip)
        :attachment(*state.getParameter(id),[this](float v){select(juce::roundToInt(v));}) {
        setComponentID(id);setTitle(tip);
        for(int i=0;i<labels.size();++i){
            auto b=std::make_unique<juce::TextButton>(labels[i]);b->setComponentID(id+juce::String(i));b->setTooltip(tip+": "+labels[i]);
            b->setClickingTogglesState(false);b->setColour(juce::TextButton::buttonOnColourId,accentColour);b->setColour(juce::TextButton::textColourOnId,juce::Colour(0xff111414));
            b->setConnectedEdges((i>0?juce::Button::ConnectedOnLeft:0)|(i+1<labels.size()?juce::Button::ConnectedOnRight:0));
            b->onClick=[this,i]{attachment.setValueAsCompleteGesture(float(i));};
            addAndMakeVisible(*b);buttons.push_back(std::move(b));
        }
        attachment.sendInitialUpdate();
    }
    void resized() override {auto r=getLocalBounds();for(size_t i=0;i<buttons.size();++i)buttons[i]->setBounds(r.removeFromLeft(r.getWidth()/int(buttons.size()-i)));}
    juce::TextButton& button(int i){return *buttons[size_t(i)];}
    int count() const {return int(buttons.size());}
    int selected() const {return current;}
private:
    void select(int i){current=i;for(size_t k=0;k<buttons.size();++k)buttons[k]->setToggleState(int(k)==i,juce::dontSendNotification);}
    juce::ParameterAttachment attachment;std::vector<std::unique_ptr<juce::TextButton>> buttons;int current{};
};

// The oscillator display. Wave mode: the current wave; click or drag across
// it to morph (each shape sits above its tile on the strip below), scroll to fine-tune, double-click to snap to the nearest shape.
// Sample mode: the sample's overview with its start marker (drag it) and the
// loop region. Dropping an audio file on it loads the file into this
// oscillator in either mode.
class WaveDisplay final : public juce::Component,public juce::SettableTooltipClient,public juce::FileDragAndDropTarget {
public:
    WaveDisplay(juce::AudioProcessorValueTreeState& s,ChimeraProcessor& owner,int oscillator)
        :state(s),processor(owner),osc(oscillator),wave(s,synthOscID(oscillator,"Wave")),start(s,synthOscID(oscillator,"Start")) {
        setComponentID(synthOscID(osc,"Display"));setTitle(juce::String("Oscillator ")+"ABC"[osc]+" display");
        setTooltip("Click or drag across the wave to morph it; scroll for small steps; double-click snaps to a shape. In Sample mode, drag the start marker. Drop an audio file here to load it.");
    }
    bool sampleMode() const {return state.getRawParameterValue(synthOscID(osc,"Mode"))->load()>.5f;}
    float position() const {return state.getRawParameterValue(synthOscID(osc,"Wave"))->load();}
    // Wave position for a horizontal position: each shape sits above its tile
    // on the strip below (tile centres at 10 %, 30 % ... 90 % of the width).
    static float waveForX(float x,float width){return 4.f*juce::jlimit(0.f,1.f,(x/std::max(1.f,width)-.1f)/.8f);}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        if(dropHover){g.setColour(accentColour);g.drawRoundedRectangle(r.reduced(1),4,2);}
        const bool on=state.getRawParameterValue(synthOscID(osc,"On"))->load()>.5f;
        const auto trace=on?traceColour:mutedColour.withAlpha(.4f);
        g.setColour(edgeColour);g.drawHorizontalLine(int(r.getCentreY()),r.getX()+4,r.getRight()-4);
        g.setFont(juce::FontOptions(9.f));
        if(!sampleMode()){
            juce::Path path;const float w=r.getWidth()-8,h=r.getHeight()*.36f;
            for(int i=0;i<=200;++i){const double phase=2.0*i/200.0;const float x=r.getX()+4+w*float(i)/200.f,y=r.getCentreY()-h*float(synthWaveDisplay(position(),phase));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
            g.setColour(trace);g.strokePath(path,juce::PathStrokeType(1.8f));
            g.setColour(mutedColour);g.drawText(synthWaveLabel(position()).toUpperCase(),getLocalBounds().reduced(6,2),juce::Justification::topRight);
            return;
        }
        const auto* sample=processor.synthEngine().sample(osc);
        if(sample==nullptr){g.setColour(mutedColour);g.drawText("DROP AN AUDIO FILE OR PRESS LOAD",getLocalBounds(),juce::Justification::centred);return;}
        const float h=r.getHeight()*.42f,cx=r.getCentreY(),w=r.getWidth()-8;
        const float startX=r.getX()+4+w*start.value()*.01f;
        if(state.getRawParameterValue(synthOscID(osc,"Loop"))->load()>.5f){g.setColour(accentColour.withAlpha(.12f));g.fillRect(startX,r.getY()+2,r.getRight()-4-startX,r.getHeight()-4);}
        g.setColour(trace);
        for(int k=0;k<SynthSample::overviewColumns;++k){const auto [lo,hi]=sample->overview[size_t(k)];const float x=r.getX()+4+w*float(k)/float(SynthSample::overviewColumns);
            g.drawVerticalLine(int(x),cx-h*juce::jlimit(0.f,1.f,hi),cx-h*juce::jlimit(-1.f,0.f,lo)+1);}
        g.setColour(accentColour);g.fillRect(startX-1,r.getY()+2,2.f,r.getHeight()-4);
        g.setColour(inkColour);g.drawText(sample->name,getLocalBounds().reduced(6,2),juce::Justification::topLeft);
        g.setColour(mutedColour);g.drawText(juce::String(sample->length/sample->sampleRate,2)+" s"+(sample->stereo()?" ST":""),getLocalBounds().reduced(6,2),juce::Justification::topRight);
    }
    void mouseDown(const juce::MouseEvent& e) override {active().begin();mouseDrag(e);}
    void mouseDrag(const juce::MouseEvent& e) override {
        if(sampleMode())start.set(100.f*juce::jlimit(0.f,1.f,(e.position.x-4)/float(getWidth()-8)));
        else wave.set(waveForX(e.position.x,float(getWidth())));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {wave.end();start.end();}
    void mouseDoubleClick(const juce::MouseEvent&) override {if(!sampleMode()){wave.begin();wave.set(float(juce::roundToInt(position())));wave.end();repaint();}}
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& d) override {
        if(sampleMode()||d.deltaY==0)return;
        wave.begin();wave.set(juce::jlimit(0.f,4.f,position()+(d.deltaY>0?.05f:-.05f)));wave.end();repaint();
    }
    bool isInterestedInFileDrag(const juce::StringArray& files) override {return files.size()==1 && synthSample::formats().findFormatForFileExtension(juce::File(files[0]).getFileExtension())!=nullptr;}
    void fileDragEnter(const juce::StringArray&,int,int) override {dropHover=true;repaint();}
    void fileDragExit(const juce::StringArray&) override {dropHover=false;repaint();}
    void filesDropped(const juce::StringArray& files,int,int) override {dropHover=false;load(juce::File(files[0]));}
    void load(const juce::File& file){
        const auto result=processor.loadSynthSample(osc,file);
        if(result.failed())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Sample not loaded",result.getErrorMessage());
        repaint();
    }
private:
    GraphParameter& active(){return sampleMode()?start:wave;}
    juce::AudioProcessorValueTreeState& state;ChimeraProcessor& processor;int osc;GraphParameter wave,start;bool dropHover{};
};

// The five shapes as a strip of tiles, evenly across the width like the
// display above. A click selects the shape; the marker shows the current
// morph position.
class WaveShapes final : public juce::Component,public juce::SettableTooltipClient {
public:
    WaveShapes(juce::AudioProcessorValueTreeState& s,int oscillator):state(s),osc(oscillator),wave(s,synthOscID(oscillator,"Wave")) {
        setComponentID(synthOscID(osc,"Shapes"));setTitle("Wave shapes");setTooltip("Sine, triangle, saw, square, pulse: click a shape.");
    }
    juce::Rectangle<int> tile(int i) const {auto r=getLocalBounds();const int w=r.getWidth()/5;return r.withX(i*w).withWidth(i==4?r.getWidth()-4*w:w).reduced(1,0);}
    void paint(juce::Graphics& g) override {
        const float position=state.getRawParameterValue(synthOscID(osc,"Wave"))->load();
        for(int i=0;i<5;++i){
            const auto r=tile(i).toFloat();const bool near=std::abs(position-float(i))<.5f;
            g.setColour(near?accentColour.withAlpha(.25f):juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,3);
            juce::Path p;for(int k=0;k<=24;++k){const float x=r.getX()+3+(r.getWidth()-6)*float(k)/24.f,y=r.getCentreY()-r.getHeight()*.3f*float(synthWaveShape(i,k/24.0));if(k==0)p.startNewSubPath(x,y);else p.lineTo(x,y);}
            g.setColour(near?inkColour:mutedColour);g.strokePath(p,juce::PathStrokeType(1.2f));
        }
        const float x=float(getWidth())*(.1f+.2f*juce::jlimit(0.f,4.f,position));
        juce::Path marker;marker.addTriangle(x-3,0,x+3,0,x,4);g.setColour(accentColour);g.fillPath(marker);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        for(int i=0;i<5;++i)if(tile(i).expanded(1,0).contains(e.getPosition())){wave.begin();wave.set(float(i));wave.end();repaint();return;}
    }
private:
    juce::AudioProcessorValueTreeState& state;int osc;GraphParameter wave;
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
        setComponentID("synthPage");setTitle("Synth");setLookAndFeel(&look);
        // Header: on, engine type and character, routing, patch, voices
        on=add(std::make_unique<Toggle>(state,"synthOn","SYNTH ON"));
        type=add(std::make_unique<Segmented>(state,"synthType",juce::StringArray{"DVERGR ANALOG","SEIDR DYNAMIC","MIMIR IMPACT"},"Engine type"));
        {auto c=std::make_unique<juce::Slider>(juce::Slider::LinearBar,juce::Slider::TextBoxLeft);c->setComponentID("synthCharacter");c->setTextValueSuffix("");
         c->setColour(juce::Slider::trackColourId,accentColour.withAlpha(.35f));c->setColour(juce::Slider::textBoxTextColourId,inkColour);c->setColour(juce::Slider::backgroundColourId,juce::Colour(0xff0e1414));c->setScrollWheelEnabled(false);
         characterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,"synthCharacter",*c);
         c->textFromValueFunction=[](double v){return "CHARACTER "+juce::String(juce::roundToInt(v))+" %";};c->updateText();
         character=static_cast<juce::Slider*>(add(std::move(c)));}
        source=add(std::make_unique<Choice>(state,"synthSource",synthSourceNames(),"Played by the guitar, MIDI or both (a held MIDI note wins)"));
        insert=add(std::make_unique<Choice>(state,"synthInsert",synthInsertNames(),"Where the synth joins the guitar signal"));
        patch.setComponentID("synthPatch");patch.setTooltip("Load a patch: every synth parameter is set, one host gesture each");
        {int id=1;for(const auto& p:synthPatches())patch.addItem(p.name,id++);}
        patch.setTextWhenNothingSelected("PATCH");patch.onChange=[this]{applyPatch(patch.getSelectedId()-1);};addAndMakeVisible(patch);
        for(const auto* id:{"synthVoices","synthOctave"}){auto s=std::make_unique<Knob>(state,id,juce::String(id)=="synthVoices"?"VOICES":"OCTAVE");header.push_back(s.get());add(std::move(s));}
        // Oscillators A, B and C
        for(int o=0;o<synthOscCount;++o){
            auto& p=oscPanels[size_t(o)];
            p.on=add(std::make_unique<Toggle>(state,synthOscID(o,"On"),juce::String("OSC ")+"ABC"[o]));
            p.mode=static_cast<Segmented*>(add(std::make_unique<Segmented>(state,synthOscID(o,"Mode"),juce::StringArray{"WAVE","SAMPLE"},juce::String("Oscillator ")+"ABC"[o]+" mode")));
            p.display=static_cast<WaveDisplay*>(add(std::make_unique<WaveDisplay>(state,owner,o)));
            p.shapes=static_cast<WaveShapes*>(add(std::make_unique<WaveShapes>(state,o)));
            auto load=std::make_unique<juce::TextButton>("LOAD");load->setComponentID(synthOscID(o,"Load"));load->setTooltip("Load an audio file (WAV, AIFF, FLAC, OGG; up to 20 s) into this oscillator");
            load->onClick=[this,o]{chooseSample(o);};p.load=static_cast<juce::TextButton*>(add(std::move(load)));
            auto clear=std::make_unique<juce::TextButton>("X");clear->setComponentID(synthOscID(o,"Clear"));clear->setTooltip("Remove the sample and return to Wave mode");
            clear->onClick=[this,o]{processor.clearSynthSample(o);};p.clear=static_cast<juce::TextButton*>(add(std::move(clear)));
            p.loop=add(std::make_unique<Toggle>(state,synthOscID(o,"Loop"),"LOOP"));
            {auto root=std::make_unique<Knob>(state,synthOscID(o,"Root"),"ROOT");root->slider.setTooltip("The note the sample plays at its own speed (set from the detected pitch on load)");p.root=root.get();add(std::move(root));}
            for(const auto& [field,text]:std::initializer_list<std::pair<const char*,const char*>>{{"Octave","OCT"},{"Semi","SEMI"},{"Fine","FINE"},{"Level","LEVEL"},{"Unison","UNISON"},{"Detune","DETUNE"},{"Wave","MORPH"},{"Start","START"}})
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
        for(const auto& [id,text]:std::initializer_list<std::pair<const char*,const char*>>{{"synthLevel","LEVEL"},{"synthGlide","GLIDE"},{"synthWidth","WIDTH"},{"synthFM","FM B>A"},{"synthSensitivity","THRESH"}})
            masterKnobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,id,text))));
        updateModes();
        startTimerHz(20);
    }
    ~SynthPage() override {stopTimer();setLookAndFeel(nullptr);}
    // Writes a patch: every synth parameter except those synthPatchKeeps() names, each as one gesture.
    void applyPatch(int index) {
        const auto patches=synthPatches();if(index<0||index>=int(patches.size()))return;
        for(auto* raw:processor.getParameters()){
            auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw);if(parameter==nullptr||!parameter->getParameterID().startsWith("synth")||synthPatchKeeps(parameter->getParameterID()))continue;
            float value=parameter->convertFrom0to1(parameter->getDefaultValue());
            for(const auto& [id,v]:patches[size_t(index)].values)if(id==parameter->getParameterID())value=v;
            const float normal=parameter->convertTo0to1(value);if(std::abs(normal-parameter->getValue())<1e-6f)continue;
            parameter->beginChangeGesture();parameter->setValueNotifyingHost(normal);parameter->endChangeGesture();
        }
        updateModes();repaint();
    }
    void paint(juce::Graphics& g) override {
        for(const auto& [area,title]:panels()){
            g.setColour(panelColour);g.fillRoundedRectangle(area.toFloat(),5);g.setColour(edgeColour);g.drawRoundedRectangle(area.toFloat().reduced(.5f),5,1);
            g.setColour(mutedColour);g.setFont(juce::FontOptions(10.f,juce::Font::bold));g.drawText(title,area.reduced(8,4).removeFromTop(14),juce::Justification::centredRight);
        }
        // Sample mode: the root note and the pitch found in the sample.
        g.setFont(juce::FontOptions(9.f));
        for(int o=0;o<synthOscCount;++o){
            const auto& p=oscPanels[size_t(o)];if(!p.loop->isVisible())continue;
            const auto* sample=processor.synthEngine().sample(o);
            const auto area=p.shapes->getBounds().withTrimmedLeft(p.loop->getWidth()+p.root->getWidth()+8);
            g.setColour(mutedColour);
            g.drawText(sample==nullptr?juce::String("NO SAMPLE"):sample->detectedHz>0?"FOUND "+juce::String(sample->detectedHz,1)+" Hz":juce::String("NO PITCH FOUND"),area,juce::Justification::centredLeft);
        }
    }
    void resized() override {
        auto top=getLocalBounds().removeFromTop(26);
        on->setBounds(top.removeFromLeft(80).reduced(0,1));top.removeFromLeft(6);
        type->setBounds(top.removeFromLeft(318).reduced(0,1));top.removeFromLeft(6);
        character->setBounds(top.removeFromLeft(140).reduced(0,2));top.removeFromLeft(6);
        source->setBounds(top.removeFromLeft(118).reduced(0,1));top.removeFromLeft(6);
        insert->setBounds(top.removeFromLeft(118).reduced(0,1));top.removeFromLeft(6);
        patch.setBounds(top.removeFromLeft(126).reduced(0,1));top.removeFromLeft(6);
        for(size_t i=0;i<header.size();++i){auto* k=header[i];k->setCompact();k->setBounds(top.removeFromLeft(std::min(100,top.getWidth()/int(header.size()-i))).reduced(0,1));k->setIncDec(i==0?" VOICES":" OCT");top.removeFromLeft(4);}
        const auto p=panels();
        for(int o=0;o<synthOscCount;++o){
            auto r=p[size_t(o)].first.reduced(8,6);auto& panel=oscPanels[size_t(o)];
            auto head=r.removeFromTop(18);panel.on->setBounds(head.removeFromLeft(50));head.removeFromLeft(4);panel.mode->setBounds(head.removeFromLeft(98));
            head.removeFromLeft(4);panel.clear->setBounds(head.removeFromRight(18));head.removeFromRight(2);panel.load->setBounds(head.removeFromRight(juce::jmin(44,head.getWidth())));
            r.removeFromTop(4);panel.display->setBounds(r.removeFromTop(48));r.removeFromTop(3);
            auto strip=r.removeFromTop(18);panel.shapes->setBounds(strip);
            panel.loop->setBounds(strip.removeFromLeft(44));strip.removeFromLeft(4);
            panel.root->setCompact();panel.root->setBounds(strip.removeFromLeft(80));panel.root->setIncDec();
            r.removeFromTop(3);
            auto row1=r.removeFromTop(r.getHeight()/2),row2=r;
            for(int k=0;k<4;++k)panel.knobs[size_t(k)]->setBounds(row1.removeFromLeft(row1.getWidth()/(4-k)));
            for(int k=4;k<7;++k)panel.knobs[size_t(k)]->setBounds(row2.removeFromLeft(row2.getWidth()/(7-k)));
            panel.knobs[7]->setBounds(panel.knobs[6]->getBounds());   // START takes MORPH's place in Sample mode
        }
        {auto r=p[3].first.reduced(8,6);auto head=r.removeFromTop(18);filterOn->setBounds(head.removeFromLeft(70));head.removeFromLeft(6);filterType->setBounds(head.removeFromLeft(120));
         r.removeFromTop(4);filter.setBounds(r.removeFromTop(r.getHeight()-60));r.removeFromTop(4);
         for(size_t k=0;k<filterKnobs.size();++k)filterKnobs[k]->setBounds(r.removeFromLeft(r.getWidth()/int(filterKnobs.size()-k)));}
        for(int e=0;e<2;++e){auto r=p[size_t(4+e)].first.reduced(8,6);r.removeFromTop(16);
            (e==0?ampEnvelope:modEnvelope).setBounds(r.removeFromTop(r.getHeight()-56));r.removeFromTop(4);
            for(size_t k=0;k<4;++k)envKnobs[size_t(e)][k]->setBounds(r.removeFromLeft(r.getWidth()/int(4-k)));}
        {auto r=p[6].first.reduced(8,6);r.removeFromTop(14);
         for(int l=0;l<2;++l){auto half=r.removeFromTop(r.getHeight()/(2-l));auto head=half.removeFromTop(18);
             lfoShape[size_t(l)]->setBounds(head.removeFromLeft(head.getWidth()/2-2));head.removeFromLeft(4);lfoSync[size_t(l)]->setBounds(head);
             lfoRate[size_t(l)]->setBounds(half.removeFromRight(48));lfoGraphs[size_t(l)].setBounds(half.reduced(0,3));}}
        {auto r=p[7].first.reduced(8,6);r.removeFromTop(16);const int h=r.getHeight()/synthModSlots;
         for(int m=0;m<synthModSlots;++m){auto row=r.removeFromTop(h).reduced(0,1);auto& mr=modRows[size_t(m)];
             mr.source->setBounds(row.removeFromLeft(row.getWidth()*3/10));row.removeFromLeft(3);mr.target->setBounds(row.removeFromLeft(row.getWidth()*45/100));row.removeFromLeft(3);mr.amount->setBounds(row);}}
        {auto r=p[8].first.reduced(8,6);r.removeFromTop(16);auto sub=r.removeFromTop(r.getHeight()/2);
         auto head=sub.removeFromTop(18);subOn->setBounds(head.removeFromLeft(50));sub.removeFromTop(3);
         auto menus=sub.removeFromLeft(sub.getWidth()*3/5);subOctave->setBounds(menus.removeFromTop(20));menus.removeFromTop(3);subShape->setBounds(menus.removeFromTop(20));
         subLevel->setBounds(sub.withTrimmedLeft(4));
         auto headNoise=r.removeFromTop(18);noiseOn->setBounds(headNoise.removeFromLeft(60));
         noiseColor->setBounds(r.removeFromLeft(r.getWidth()/2));noiseLevel->setBounds(r);}
        {auto r=p[9].first.reduced(8,6);r.removeFromTop(16);scope.setBounds(r.removeFromTop(r.getHeight()-56));r.removeFromTop(4);
         for(size_t k=0;k<masterKnobs.size();++k)masterKnobs[k]->setBounds(r.removeFromLeft(r.getWidth()/int(masterKnobs.size()-k)));}
        updateModes();
    }
    juce::Component* control(const juce::String& id) {
        for(auto& k:owned){
            if(k->getComponentID()==id)return k.get();
            if(auto* knob=dynamic_cast<Knob*>(k.get()))if(knob->slider.getComponentID()==id)return &knob->slider;
            if(auto* seg=dynamic_cast<Segmented*>(k.get()))for(int i=0;i<seg->count();++i)if(id==seg->getComponentID()+juce::String(i))return &seg->button(i);
        }
        return getComponentID()==id?this:nullptr;
    }
    FilterCurve& filterCurve() noexcept {return filter;}
    EnvelopeGraph& envelopeGraph(int index) noexcept {return index==0?ampEnvelope:modEnvelope;}
    WaveDisplay& waveDisplay(int osc) noexcept {return *oscPanels[size_t(juce::jlimit(0,synthOscCount-1,osc))].display;}
    WaveShapes& waveShapes(int osc) noexcept {return *oscPanels[size_t(juce::jlimit(0,synthOscCount-1,osc))].shapes;}
    // Shows each oscillator's Wave or Sample controls, and the character
    // tooltip of the selected type. Called on layout, patch loads and by the timer.
    void updateModes() {
        for(int o=0;o<synthOscCount;++o){
            auto& p=oscPanels[size_t(o)];const bool sample=state.getRawParameterValue(synthOscID(o,"Mode"))->load()>.5f;
            p.shapes->setVisible(!sample);p.knobs[6]->setVisible(!sample);
            for(juce::Component* c:{static_cast<juce::Component*>(p.load),static_cast<juce::Component*>(p.clear),p.loop,static_cast<juce::Component*>(p.root),static_cast<juce::Component*>(p.knobs[7])})c->setVisible(sample);
        }
        static const char* tips[]{"Dvergr Analog: slow pitch drift per oscillator (about 5 cents RMS at 100 %) and soft saturation into the filter",
                                  "Seidr Dynamic: the playing level (guitar level or velocity) moves the filter over 3 octaves, brightens the waves and sets the level over 12 dB",
                                  "Mimir Impact: each attack strikes an FM index (60 ms) and a noise click (4 ms)"};
        character->setTooltip(juce::String(tips[juce::jlimit(0,2,juce::roundToInt(state.getRawParameterValue("synthType")->load()))])+". At 0 % every type is the plain voice.");
    }
private:
    // Panels: three oscillators and the filter across the top; envelopes,
    // LFOs, the matrix, sub / noise and scope / master across the bottom.
    std::array<std::pair<juce::Rectangle<int>,const char*>,10> panels() const {
        const int top=30,w=getWidth(),h=getHeight()-top,rowA=h*52/100,gap=6;
        const int y2=top+rowA+gap,h2=h-rowA-gap;
        const auto col=[&](int x,int width,int y,int height){return juce::Rectangle<int>(x,y,width-gap,height);};
        const int a0=w*22/100,a3=w-3*a0;
        const int b0=w*14/100,b1=w*14/100,b2=w*13/100,b3=w*21/100,b4=w*14/100,b5=w-b0-b1-b2-b3-b4;
        return {{{col(0,a0,top,rowA),"OSC A"},{col(a0,a0,top,rowA),"OSC B"},{col(2*a0,a0,top,rowA),"OSC C"},{col(3*a0,a3+gap,top,rowA),"FILTER"},
                 {col(0,b0,y2,h2),"ENV 1  AMP"},{col(b0,b1,y2,h2),"ENV 2  MOD"},{col(b0+b1,b2,y2,h2),"LFO 1 / 2"},{col(b0+b1+b2,b3,y2,h2),"MOD MATRIX"},
                 {col(b0+b1+b2+b3,b4,y2,h2),"SUB / NOISE"},{col(b0+b1+b2+b3+b4,b5+gap,y2,h2),"SCOPE / MASTER"}}};
    }
    template<class C> C* add(std::unique_ptr<C> c){auto* raw=c.get();addAndMakeVisible(*raw);children.push_back(raw);owned.push_back(std::move(c));return raw;}
    void chooseSample(int osc){
        chooser=std::make_unique<juce::FileChooser>("Load a sample into oscillator "+juce::String::charToString("ABC"[osc]),juce::File(),synthSample::formats().getWildcardForAllFormats());
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,osc](const juce::FileChooser& c){
            if(c.getResult().existsAsFile())oscPanels[size_t(osc)].display->load(c.getResult());});
    }
    void timerCallback() override {
        if(!isShowing())return;
        updateModes();
        for(auto& p:oscPanels){p.display->repaint();p.shapes->repaint();}
        filter.repaint();ampEnvelope.repaint();modEnvelope.repaint();for(auto& l:lfoGraphs)l.repaint();scope.repaint();
        repaint(oscPanels[0].shapes->getBounds().getUnion(oscPanels[2].shapes->getBounds()));
    }
    struct OscPanel {juce::Component* on{};Segmented* mode{};WaveDisplay* display{};WaveShapes* shapes{};juce::TextButton* load{};juce::TextButton* clear{};juce::Component* loop{};Knob* root{};std::vector<Knob*> knobs;};
    struct ModRow {juce::Component* source{};juce::Component* target{};juce::Slider* amount{};std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;};
    SynthLook look;   // declared first: destroyed after every child that uses it
    ChimeraProcessor& processor;juce::AudioProcessorValueTreeState& state;
    std::vector<std::unique_ptr<juce::Component>> owned;std::vector<juce::Component*> children;std::vector<Knob*> header;
    juce::Component* on{};juce::Component* type{};juce::Slider* character{};juce::Component* source{};juce::Component* insert{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> characterAttachment;
    juce::ComboBox patch;std::unique_ptr<juce::FileChooser> chooser;
    std::array<OscPanel,synthOscCount> oscPanels;
    juce::Component* subOn{};juce::Component* subOctave{};juce::Component* subShape{};Knob* subLevel{};
    juce::Component* noiseOn{};Knob* noiseColor{};Knob* noiseLevel{};
    juce::Component* filterOn{};juce::Component* filterType{};FilterCurve filter;std::vector<Knob*> filterKnobs;
    EnvelopeGraph ampEnvelope,modEnvelope;std::array<std::vector<Knob*>,2> envKnobs;
    std::array<juce::Component*,2> lfoShape{},lfoSync{};std::array<Knob*,2> lfoRate{};std::array<LfoGraph,2> lfoGraphs;
    std::array<ModRow,synthModSlots> modRows;
    Scope scope;std::vector<Knob*> masterKnobs;
};
} // namespace spectralforge::synthUI

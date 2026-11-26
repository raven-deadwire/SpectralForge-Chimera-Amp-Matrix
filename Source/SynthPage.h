#pragma once
#include "PluginProcessor.h"
#include "SynthLayer.h"
#include "SynthSampleIO.h"
#include "RasterArtwork.h"

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
        // A knob the matrix modulates: an outer arc from the set value, towards
        // the side the modulation pushes it (its length follows the amount; it
        // is an indicator, not the exact range).
        if(const double mod=double(slider.getProperties().getWithDefault("mod",0.0));std::abs(mod)>.005){
            const float span=float(juce::jlimit(-1.0,1.0,mod))*.5f*(end-start),to=juce::jlimit(start,end,angle+span);
            juce::Path arc;arc.addCentredArc(c.x,c.y,radius+thick*.9f,radius+thick*.9f,0,juce::jmin(angle,to),juce::jmax(angle,to),true);
            g.setColour(traceColour);g.strokePath(arc,juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            g.fillEllipse(r.getRight()-6,r.getY()+1,4,4);
        }
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float minimum,float maximum,juce::Slider::SliderStyle style,juce::Slider& slider) override {
        if(style!=juce::Slider::LinearBar){juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,position,minimum,maximum,style,slider);return;}
        const auto r=juce::Rectangle<int>(x,y,width,height).toFloat();
        g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,3);
        if(slider.getMinimum()<0 && slider.getMaximum()>0){   // a two-way amount: the bar grows from the centre
            const float centre=r.getX()+r.getWidth()*float(-slider.getMinimum()/(slider.getMaximum()-slider.getMinimum()));
            g.setColour((slider.getValue()<0?traceColour:accentColour).withAlpha(.45f));g.fillRect(juce::Rectangle<float>::leftTopRightBottom(juce::jmin(centre,position),r.getY()+1,juce::jmax(centre,position),r.getBottom()-1));
            g.setColour(mutedColour);g.fillRect(centre-.5f,r.getY()+2,1.f,r.getHeight()-4);
        } else {g.setColour(accentColour.withAlpha(.4f));g.fillRoundedRectangle(r.withRight(position),3);}
        g.setColour(edgeColour);g.drawRoundedRectangle(r.reduced(.5f),3,1);
    }
    // Narrow menus (the matrix rows, the sequencer's step length): smaller text and less room kept for the arrow, so names are not cut.
    juce::Font getComboBoxFont(juce::ComboBox& box) override {return box.getWidth()<90?juce::Font(juce::FontOptions(juce::jmin(11.5f,float(box.getHeight())*.8f))):juce::LookAndFeel_V4::getComboBoxFont(box);}
    void drawComboBox(juce::Graphics& g,int width,int height,bool down,int bx,int by,int bw,int bh,juce::ComboBox& box) override {
        if(width>=90){juce::LookAndFeel_V4::drawComboBox(g,width,height,down,bx,by,bw,bh,box);return;}
        const auto r=juce::Rectangle<int>(0,0,width,height).toFloat();
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));g.fillRoundedRectangle(r,3);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));g.drawRoundedRectangle(r.reduced(.5f),3,1);
        const float cx=float(width)-10,cy=float(height)*.5f;juce::Path arrow;arrow.startNewSubPath(cx-4,cy-2);arrow.lineTo(cx,cy+2);arrow.lineTo(cx+4,cy-2);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled()?.9f:.2f));g.strokePath(arrow,juce::PathStrokeType(1.5f));
    }
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override {
        if(box.getWidth()>=90){juce::LookAndFeel_V4::positionComboBoxText(box,label);return;}
        label.setBounds(1,1,box.getWidth()-19,box.getHeight()-2);label.setFont(getComboBoxFont(box));label.setBorderSize({1,4,1,0});
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
    void setLabel(const juce::String& text){if(label.getText()!=text){label.setText(text,juce::dontSendNotification);slider.setName(text);slider.setTooltip(text);}}
    juce::Slider slider;
private:
    void showValue(){value.setText(slider.getTextFromValue(slider.getValue()),juce::dontSendNotification);}
    juce::Label label,value;std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;bool incDec{};
};
class Choice final : public juce::ComboBox {
public:
    // `first`: the text shown for the first choice in place of its parameter name (for example "Source" for "Off").
    Choice(juce::AudioProcessorValueTreeState& state,const juce::String& id,juce::StringArray items,const juce::String& tip,const juce::String& first={}) {
        if(first.isNotEmpty() && !items.isEmpty())items.set(0,first);
        setComponentID(id);setTooltip(tip);setName(tip);addItemList(items,1);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,id,*this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};
// A horizontal bar on a 0-100 % parameter (the look's LinearBar).
class Bar final : public juce::Slider {
public:
    Bar(juce::AudioProcessorValueTreeState& state,const juce::String& id,const juce::String& tip) {
        setComponentID(id);setName(tip);setTooltip(tip);setSliderStyle(juce::Slider::LinearBar);setScrollWheelEnabled(false);
        setColour(juce::Slider::textBoxTextColourId,inkColour);setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,id,*this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
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
    // The oscillator's wavetable (docs/SYNTH_WAVETABLE.md); 0 is the classic shapes.
    int table() const {return juce::jlimit(0,synthTableCount-1,juce::roundToInt(state.getRawParameterValue(synthWavetableID(osc,"Table"))->load()));}
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
        if(!sampleMode() && table()>0){
            // A wavetable: six of its frames stacked back to front, and the
            // frame at the wave position, through the warp, in front of them.
            const auto& tables=SynthWavetables::get();const int t=table();const float at=position()*.25f;
            const int warp=juce::roundToInt(state.getRawParameterValue(synthWavetableID(osc,"Warp"))->load());
            const double amount=state.getRawParameterValue(synthWavetableID(osc,"WarpAmount"))->load()*.01;
            const float depthX=r.getWidth()*.14f,depthY=r.getHeight()*.44f,w=r.getWidth()-10-depthX,h=r.getHeight()*.17f,baseY=r.getBottom()-5-h;
            const auto curve=[&](float where,bool warped){juce::Path path;
                for(int i=0;i<=96;++i){double phase=double(i)/96.0;if(phase>=1.0)phase=.999999;if(warped)phase=synthWarpPhase(warp,amount,phase);
                    const float x=r.getX()+5+depthX*where+w*float(i)/96.f,y=baseY-depthY*where-h*float(tables.read(t,where,phase,1.0/4096.0));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
                return path;};
            for(int k=5;k>=0;--k){g.setColour(mutedColour.withAlpha(on?.34f:.18f));g.strokePath(curve(float(k)/5.f,false),juce::PathStrokeType(1.f));}
            g.setColour(trace);g.strokePath(curve(at,true),juce::PathStrokeType(1.9f));
            g.setColour(mutedColour);g.drawText(juce::String(synthTables[size_t(t)].name).toUpperCase()+"  "+juce::String(juce::roundToInt(at*100.f))+" %",getLocalBounds().reduced(6,2),juce::Justification::topRight);
            return;
        }
        if(!sampleMode()){
            juce::Path path;const float w=r.getWidth()-8,h=r.getHeight()*.36f;
            for(int i=0;i<=200;++i){const double phase=2.0*i/200.0;const float x=r.getX()+4+w*float(i)/200.f,y=r.getCentreY()-h*float(synthWaveDisplay(position(),phase));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
            g.setColour(trace);g.strokePath(path,juce::PathStrokeType(1.8f));
            g.setColour(mutedColour);g.drawText(synthWaveLabel(position()).toUpperCase(),getLocalBounds().reduced(6,2),juce::Justification::topRight);
            return;
        }
        const auto sample=processor.synthEngine().sharedSample(osc);   // kept alive while drawn
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
        else if(table()>0)wave.set(4.f*juce::jlimit(0.f,1.f,(e.position.x-5)/float(juce::jmax(1,getWidth()-10))));   // a wavetable: left to right is first to last frame
        else wave.set(waveForX(e.position.x,float(getWidth())));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {wave.end();start.end();}
    void mouseDoubleClick(const juce::MouseEvent&) override {if(!sampleMode() && table()==0){wave.begin();wave.set(float(juce::roundToInt(position())));wave.end();repaint();}}
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

// Both filters' responses on a log axis (filter 2 only while it is on). Drag
// a handle: left / right for cutoff, up / down for resonance. A press takes
// the nearer handle.
class FilterCurve final : public juce::Component,public juce::SettableTooltipClient {
public:
    FilterCurve(juce::AudioProcessorValueTreeState& s,double sampleRate):state(s),cutoff{GraphParameter(s,"synthCutoff"),GraphParameter(s,"synthFilter2Cutoff")},
        resonance{GraphParameter(s,"synthResonance"),GraphParameter(s,"synthFilter2Resonance")},rate(sampleRate) {
        setComponentID("synthFilterCurve");setTitle("Filter response");setTooltip("Drag a handle left / right for cutoff, up / down for resonance. Filter 2 is drawn while it is on.");
    }
    static float xForHz(float hz,float width){return width*float(std::log(hz/20.f)/std::log(1000.f));}
    static float hzForX(float x,float width){return 20.f*std::pow(1000.f,juce::jlimit(0.f,1.f,x/width));}
    bool on(int f) const {return state.getRawParameterValue(f==0?"synthFilterOn":"synthFilter2On")->load()>.5f;}
    juce::Point<float> handle(int f) const {
        const auto r=getLocalBounds().toFloat();
        return {xForHz(cutoff[size_t(f)].value(),r.getWidth()),r.getY()+r.getHeight()*(1.f-resonance[size_t(f)].value()*.01f)*.8f+6};
    }
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        g.setColour(edgeColour.withAlpha(.6f));for(const float hz:{100.f,1000.f,10000.f})g.drawVerticalLine(int(xForHz(hz,r.getWidth())),r.getY(),r.getBottom());
        juce::String text;
        for(int f=1;f>=0;--f){   // filter 1 on top
            if(f==1 && !on(1))continue;
            const int type=juce::roundToInt(state.getRawParameterValue(f==0?"synthFilterType":"synthFilter2Type")->load());
            juce::Path path;
            for(int x=0;x<=int(r.getWidth());x+=2){
                const double hz=hzForX(float(x),r.getWidth()),c=cutoff[size_t(f)].value(),q=resonance[size_t(f)].value()*.01;
                const double db=juce::Decibels::gainToDecibels(f==0?synthFilterMagnitude(type,c,q,hz,rate):synthFilter2Magnitude(type,c,q,hz,rate),-60.0);
                const float y=juce::jmap(float(juce::jlimit(-36.0,24.0,db)),-36.f,24.f,r.getBottom()-2,r.getY()+2);
                if(x==0)path.startNewSubPath(float(x),y);else path.lineTo(float(x),y);
            }
            const auto colour=f==0?accentColour:traceColour;
            g.setColour(on(f)?colour:mutedColour.withAlpha(.4f));g.strokePath(path,juce::PathStrokeType(f==0?2.f:1.6f));
            const auto h=handle(f);g.setColour(f==0?inkColour:traceColour);g.fillEllipse(h.x-4,h.y-4,8,8);
            if(on(1)){g.setColour(juce::Colour(0xff0e1414));g.setFont(juce::FontOptions(7.f,juce::Font::bold));g.drawText(juce::String(f+1),juce::Rectangle<float>(h.x-4,h.y-4,8,8),juce::Justification::centred);}
            text=juce::String(juce::roundToInt(cutoff[size_t(f)].value()))+" Hz / "+juce::String(juce::roundToInt(resonance[size_t(f)].value()))+" %"+(text.isEmpty()?juce::String():"    2: "+text);
        }
        g.setColour(mutedColour);g.setFont(juce::FontOptions(9.f));g.drawText(text,getLocalBounds().reduced(6,2),juce::Justification::topRight);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        active=on(1) && e.position.getDistanceFrom(handle(1))<e.position.getDistanceFrom(handle(0))?1:0;
        cutoff[size_t(active)].begin();resonance[size_t(active)].begin();apply(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {apply(e);}
    void mouseUp(const juce::MouseEvent&) override {cutoff[size_t(active)].end();resonance[size_t(active)].end();}
    void refresh(){repaint();}
private:
    void apply(const juce::MouseEvent& e){
        cutoff[size_t(active)].set(hzForX(float(e.x),float(getWidth())));
        resonance[size_t(active)].set(100.f*juce::jlimit(0.f,1.f,1.f-(float(e.y)-2.f)/(float(getHeight())*.8f)));
        repaint();
    }
    juce::AudioProcessorValueTreeState& state;std::array<GraphParameter,2> cutoff,resonance;double rate;int active{};
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

// The step sequencer's sixteen steps as bars around a centre line. Drag
// across them to draw values (one host gesture per step touched);
// double-click a step for 0. Steps past the length are dimmed.
class StepGraph final : public juce::Component,public juce::SettableTooltipClient {
public:
    StepGraph(juce::AudioProcessorValueTreeState& s,ChimeraProcessor& p):state(s),processor(p) {
        setComponentID("synthSeqGraph");setTitle("Step sequencer");setTooltip("Drag across the steps to draw them: up is positive, down is negative. Double-click a step for 0.");
        for(int k=0;k<synthSeqMaxSteps;++k)steps[size_t(k)]=s.getParameter(synthSeqStepID(k));
    }
    int length() const {return juce::jlimit(1,synthSeqMaxSteps,juce::roundToInt(state.getRawParameterValue("synthSeqSteps")->load()));}
    float value(int k) const {return state.getRawParameterValue(synthSeqStepID(k))->load();}
    juce::Rectangle<float> column(int k) const {const float w=float(getWidth())/float(synthSeqMaxSteps);return {w*float(k),0.f,w,float(getHeight())};}
    void paint(juce::Graphics& g) override {
        const auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff0e1414));g.fillRoundedRectangle(r,4);
        const int n=length(),now=processor.synthEngine().sounding()?processor.synthEngine().sequencerStep():-1;const float mid=r.getCentreY(),half=r.getHeight()*.5f-2;
        for(int k=0;k<synthSeqMaxSteps;++k){
            const auto c=column(k).reduced(1,0);const float v=value(k)*.01f,y=mid-half*v;
            if(k==now){g.setColour(inkColour.withAlpha(.10f));g.fillRect(c);}
            g.setColour((k<n?(v<0?traceColour:accentColour):mutedColour).withAlpha(k<n?.85f:.25f));
            g.fillRect(juce::Rectangle<float>::leftTopRightBottom(c.getX(),juce::jmin(mid,y),c.getRight(),juce::jmax(mid,y)).expanded(0,.75f));
        }
        g.setColour(edgeColour);g.drawHorizontalLine(int(mid),r.getX(),r.getRight());
    }
    void mouseDown(const juce::MouseEvent& e) override {last=-1;apply(e);}
    void mouseDrag(const juce::MouseEvent& e) override {apply(e);}
    void mouseUp(const juce::MouseEvent&) override {last=-1;for(int k=0;k<synthSeqMaxSteps;++k)if(touched[size_t(k)]){steps[size_t(k)]->endChangeGesture();touched[size_t(k)]=false;}}
    void mouseDoubleClick(const juce::MouseEvent& e) override {const int k=stepAt(e);auto* q=steps[size_t(k)];q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(0.f));q->endChangeGesture();repaint();}
private:
    int stepAt(const juce::MouseEvent& e) const {return juce::jlimit(0,synthSeqMaxSteps-1,int(float(e.x)*float(synthSeqMaxSteps)/float(juce::jmax(1,getWidth()))));}
    // Writes the step under the pointer and, on a fast drag, the steps it passed over (a straight line between the two points).
    void apply(const juce::MouseEvent& e){
        const int k=stepAt(e);const float v=juce::jlimit(-100.f,100.f,std::round(100.f*(1.f-2.f*float(e.y)/float(juce::jmax(1,getHeight())))));
        const int from=last<0?k:last;const float fromValue=last<0?v:lastValue;
        for(int step=juce::jmin(from,k);step<=juce::jmax(from,k);++step){
            if(step==from && last>=0 && from!=k)continue;   // written by the previous event
            auto* q=steps[size_t(step)];if(!touched[size_t(step)]){q->beginChangeGesture();touched[size_t(step)]=true;}
            q->setValueNotifyingHost(q->convertTo0to1(std::round(from==k?v:fromValue+(v-fromValue)*float(step-from)/float(k-from))));
        }
        last=k;lastValue=v;repaint();
    }
    juce::AudioProcessorValueTreeState& state;ChimeraProcessor& processor;std::array<juce::RangedAudioParameter*,synthSeqMaxSteps> steps{};std::array<bool,synthSeqMaxSteps> touched{};int last{-1};float lastValue{};
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
        g.drawText("CLARITY "+juce::String(tracker.clarity(),2)+"  VOICES "+juce::String(engine.soundingVoices())+(processor.synthBusEnabled()?"  SYNTH BUS ON":"  SYNTH BUS OFF IN HOST"),getLocalBounds().reduced(6,2).withTrimmedTop(14).removeFromTop(12),juce::Justification::centredLeft);
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
        ampEnvelope(state,"synthAmp"),modEnvelope(state,"synthModEnv"),lfoGraphs{LfoGraph(state,owner,0),LfoGraph(state,owner,1)},seqGraph(state,owner),scope(owner) {
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
            // Wavetable (docs/SYNTH_WAVETABLE.md): the table, and its warp under the display.
            p.table=add(std::make_unique<Choice>(state,synthWavetableID(o,"Table"),synthTableNames(),juce::String("Oscillator ")+"ABC"[o]+" wavetable. Classic is the five-shape oscillator; a table plays sixteen frames that POSITION moves through."));
            p.warp=add(std::make_unique<Choice>(state,synthWavetableID(o,"Warp"),synthWarpNames(),"Warp: reshapes how the table is read. Bend pushes the wave to one side, Squeeze narrows its first half, Sync restarts it faster, Mirror folds it, Quantize steps it."));
            p.warpAmount=add(std::make_unique<Bar>(state,synthWavetableID(o,"WarpAmount"),"Warp amount"));
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
        filterOn=add(std::make_unique<Toggle>(state,"synthFilterOn","FILTER 1"));
        filterType=add(std::make_unique<Choice>(state,"synthFilterType",synthFilterTypeNames(),"Filter 1 type"));
        addAndMakeVisible(filter);
        // Filter 2, routing and distortion (docs/SYNTH_FILTERS.md).
        filter2On=add(std::make_unique<Toggle>(state,"synthFilter2On","FILTER 2"));
        filter2Type=add(std::make_unique<Choice>(state,"synthFilter2Type",synthFilter2TypeNames(),"Filter 2 type. Comb + and Comb - ring at the cutoff's pitch; KEY at 100 % makes them follow the note."));
        for(const auto& [id,text]:std::initializer_list<std::pair<const char*,const char*>>{{"synthFilter2Cutoff","CUTOFF"},{"synthFilter2Resonance","RES"},{"synthFilter2KeyTrack","KEY"},{"synthFilter2Env","ENV 2"}})
            filter2Knobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,id,text))));
        filterRouting=add(std::make_unique<Choice>(state,"synthFilterRouting",synthFilterRoutingNames(),"How the two filters are connected: one after the other, or side by side and mixed"));
        {auto bar=std::make_unique<Bar>(state,"synthFilterBalance","Parallel balance: filter 1 alone on the left, both in the centre, filter 2 alone on the right");
         bar->textFromValueFunction=[](double v){const int i=juce::roundToInt(v);return i==50?juce::String("1 = 2"):i<50?"1  "+juce::String(100-i)+" %":"2  "+juce::String(i)+" %";};bar->updateText();
         filterBalance=static_cast<juce::Slider*>(add(std::move(bar)));}
        distOn=add(std::make_unique<Toggle>(state,"synthDistOn","DIST"));
        distType=add(std::make_unique<Choice>(state,"synthDistType",synthDistTypeNames(),"Distortion type. It runs in every voice, so chords stay clear."));
        distPlace=add(std::make_unique<Choice>(state,"synthDistPlace",synthDistPlaceNames(),"Where the distortion sits. In Parallel, After filter 1 distorts filter 1's branch only."));
        for(const auto& [id,text,tip,slot]:std::initializer_list<std::tuple<const char*,const char*,const char*,juce::Slider**>>{
                {"synthDistDrive","DRIVE","Distortion drive: 0 to +36 dB into the shaper (Bit crush: 16 bits down to 3)",&distDrive},{"synthDistMix","MIX","Distortion mix: clean to distorted",&distMix}}){
            auto bar=std::make_unique<Bar>(state,id,tip);const juce::String name(text);
            bar->textFromValueFunction=[name](double v){return name+" "+juce::String(juce::roundToInt(v))+" %";};bar->updateText();
            *slot=static_cast<juce::Slider*>(add(std::move(bar)));}
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
        // Modulation matrix (docs/SYNTH_WAVETABLE.md, "Modulation matrix"): SOURCE
        // moves TARGET by AMOUNT. Only the rows in use are shown; + adds one.
        for(auto [label,text]:{std::pair<juce::Label*,const char*>{&modSourceTitle,"SOURCE"},{&modTargetTitle,"MOVES  TARGET"},{&modAmountTitle,"AMOUNT"}}){
            label->setText(text,juce::dontSendNotification);label->setFont(juce::FontOptions(9.f,juce::Font::bold));label->setColour(juce::Label::textColourId,mutedColour);label->setBorderSize({});addAndMakeVisible(*label);}
        // Rows 1-8 are the released ones; rows 9-16 (docs/SYNTH_SEQUENCER.md) also know the sequencer, the macros and the newer targets.
        for(int m=0;m<modRowCount;++m){
            auto& row=modRows[size_t(m)];const auto n=juce::String(m+1);const bool wide=m>=synthModSlots;
            row.source=add(std::make_unique<Choice>(state,modRowID(m,"Source"),wide?synthModSourceNamesB():synthModSourceNames(),"Modulation "+n+": what moves the target (an envelope, an LFO, how hard you play ...)","Source ..."));
            row.target=add(std::make_unique<Choice>(state,modRowID(m,"Target"),wide?synthModTargetNamesB():synthModTargetNames(),"Modulation "+n+": what is moved","Target ..."));
            auto amount=std::make_unique<juce::Slider>();amount->setSliderStyle(juce::Slider::LinearBar);   // the bar shows its value as text
            amount->setComponentID(modRowID(m,"Amount"));amount->setTooltip("Modulation "+n+" amount: right of the centre moves the target up, left moves it down. Double-click for 0.");
            amount->setColour(juce::Slider::textBoxTextColourId,inkColour);amount->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);amount->setScrollWheelEnabled(false);
            row.amountAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,modRowID(m,"Amount"),*amount);
            // After the attachment, which installs the parameter's own text.
            amount->setDoubleClickReturnValue(true,0.0);amount->textFromValueFunction=[](double v){const int i=juce::roundToInt(v);return juce::String(i>0?"+":"")+juce::String(i)+" %";};amount->updateText();
            row.amount=static_cast<juce::Slider*>(add(std::move(amount)));
            auto clear=std::make_unique<juce::TextButton>("x");clear->setComponentID(modRowID(m,"Clear"));clear->setTooltip("Remove modulation "+n);
            clear->onClick=[this,m]{clearModulation(m);};row.clear=static_cast<juce::TextButton*>(add(std::move(clear)));
            for(juce::Component* c:{row.source,row.target,static_cast<juce::Component*>(row.amount),static_cast<juce::Component*>(row.clear)})matrixHolder.addChildComponent(*c);   // the rows scroll
        }
        modAdd.setButtonText("+ ADD MODULATION");modAdd.setComponentID("synthModAdd");modAdd.setTooltip("Add a modulation row. You can also right-click a knob and choose what moves it.");
        modAdd.onClick=[this]{for(int m=0;m<modRowCount;++m)if(modRowShown(m))revealed[size_t(m)]=true;for(int k=0;k<modRowCount;++k)if(const int row=modRowByPreference(k);!modRowShown(row)){revealed[size_t(row)]=true;break;}layoutMatrix();};matrixHolder.addAndMakeVisible(modAdd);
        matrixView.setViewedComponent(&matrixHolder,false);matrixView.setScrollBarsShown(true,false);matrixView.setScrollBarThickness(8);addAndMakeVisible(matrixView);
        modHint.setText("Right-click a knob to modulate it.",juce::dontSendNotification);modHint.setFont(juce::FontOptions(9.f));modHint.setColour(juce::Label::textColourId,mutedColour);modHint.setBorderSize({});addAndMakeVisible(modHint);
        // Step sequencer and macros (docs/SYNTH_SEQUENCER.md): they share the LFO panel, one view at a time.
        {static const char* names[]{"LFO","SEQ","MACRO"};static const char* tips[]{"LFO 1 and 2","Step sequencer: a pattern of up to sixteen values in time with the host tempo","Macros: four controls of your own; connect each to what it should move"};
         for(int t=0;t<3;++t){auto& b=modTabs[size_t(t)];b.setButtonText(names[t]);b.setComponentID(juce::String("synthModTab")+names[t]);b.setTooltip(tips[t]);b.setClickingTogglesState(true);b.setRadioGroupId(7301);
            b.setColour(juce::TextButton::buttonOnColourId,accentColour);b.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff111414));
            b.setConnectedEdges((t>0?juce::Button::ConnectedOnLeft:0)|(t<2?juce::Button::ConnectedOnRight:0));b.onClick=[this,t]{if(modTabs[size_t(t)].getToggleState())showModTab(t);};addAndMakeVisible(b);}}
        addAndMakeVisible(seqGraph);
        {auto steps=std::make_unique<Knob>(state,"synthSeqSteps","STEPS");seqSteps=steps.get();add(std::move(steps));}
        seqRate=add(std::make_unique<Choice>(state,"synthSeqRate",synthSeqRateNames(),"Length of one step, in time with the host tempo (120 BPM without one)"));
        {auto bar=std::make_unique<Bar>(state,"synthSeqSmooth","Smoothing: 0 % jumps from step to step, 100 % glides for the whole step");
         bar->textFromValueFunction=[](double v){return "SMOOTH "+juce::String(juce::roundToInt(v))+" %";};bar->updateText();seqSmooth=static_cast<juce::Slider*>(add(std::move(bar)));}
        seqRetrigger=add(std::make_unique<Toggle>(state,"synthSeqRetrigger","RETRIG"));
        static_cast<juce::TextButton*>(seqRetrigger)->setTooltip("On: a note played while nothing is held starts the pattern at step 1. Off: the pattern keeps running.");
        for(int k=0;k<synthMacroCount;++k){
            auto bar=std::make_unique<Bar>(state,synthMacroID(k),"Macro "+juce::String(k+1)+": choose it as a SOURCE in the matrix, or right-click a knob. It can be automated.");
            bar->textFromValueFunction=[k](double v){return "MACRO "+juce::String(k+1)+"   "+juce::String(juce::roundToInt(v))+" %";};bar->updateText();macros[size_t(k)]=static_cast<juce::Slider*>(add(std::move(bar)));}
        modTabs[0].setToggleState(true,juce::dontSendNotification);showModTab(0);
        // Knobs the matrix can move: right-click opens "Modulate with".
        for(auto& k:owned)if(auto* knob=dynamic_cast<Knob*>(k.get()))if(modTargetFor(knob->slider.getComponentID())>0)knob->slider.addMouseListener(this,false);
        // Scope and master
        output=add(std::make_unique<Choice>(state,"synthOutput",synthOutputNames(),"Synth output: into the guitar mix at its insert point, to the plugin's SYNTH output bus, or both. Without a SYNTH bus (the host enables it) the synth stays in the mix."));
        addAndMakeVisible(scope);
        for(const auto& [id,text]:std::initializer_list<std::pair<const char*,const char*>>{{"synthLevel","LEVEL"},{"synthGlide","GLIDE"},{"synthWidth","WIDTH"},{"synthFM","FM B>A"},{"synthSensitivity","THRESH"}})
            masterKnobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,id,text))));
        // Effect rack (docs/SYNTH_EFFECTS.md): FX in the top bar shows it in place of the envelopes, LFOs, matrix and sub / noise.
        fxButton.setButtonText("FX");fxButton.setComponentID("synthFxView");fxButton.setClickingTogglesState(true);
        fxButton.setTooltip("Show the effect rack (chorus, phaser, delay, reverb, EQ) in place of the envelopes, LFOs and matrix. The effects run whichever view is shown.");
        fxButton.setColour(juce::TextButton::buttonOnColourId,accentColour);fxButton.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff111414));
        fxButton.onClick=[this]{showEffects(fxButton.getToggleState());};addAndMakeVisible(fxButton);
        {struct Spec{const char* on;const char* name;std::vector<std::pair<const char*,const char*>> knobs;};
         const std::array<Spec,synthEffectCount> specs{{
            {"synthFxChorusOn","CHORUS",{{"synthFxChorusRate","RATE"},{"synthFxChorusDepth","DEPTH"},{"synthFxChorusMix","MIX"}}},
            {"synthFxPhaserOn","PHASER",{{"synthFxPhaserRate","RATE"},{"synthFxPhaserDepth","DEPTH"},{"synthFxPhaserFeedback","FEEDBACK"},{"synthFxPhaserMix","MIX"}}},
            {"synthFxDelayOn","DELAY",{{"synthFxDelayTime","TIME"},{"synthFxDelayFeedback","FEEDBACK"},{"synthFxDelayMix","MIX"}}},
            {"synthFxReverbOn","REVERB",{{"synthFxReverbDecay","DECAY"},{"synthFxReverbDamp","DAMP"},{"synthFxReverbMix","MIX"}}},
            {"synthFxEqOn","EQ",{{"synthFxEqLow","LOW 200 Hz"},{"synthFxEqHigh","HIGH 4 kHz"}}}}};
         for(int e=0;e<synthEffectCount;++e){auto& panel=fxPanels[size_t(e)];
            panel.on=add(std::make_unique<Toggle>(state,specs[size_t(e)].on,specs[size_t(e)].name));
            for(const auto& [id,text]:specs[size_t(e)].knobs)panel.knobs.push_back(static_cast<Knob*>(add(std::make_unique<Knob>(state,id,text))));}
         fxDelaySync=add(std::make_unique<Choice>(state,"synthFxDelaySync",synthFxDelaySyncNames(),"Delay time: Free uses TIME; the others follow the host tempo (120 BPM without one). D is dotted, T a triplet. At most 2 s."));
         fxDelayPingPong=add(std::make_unique<Toggle>(state,"synthFxDelayPingPong","PING-PONG"));
         static_cast<juce::TextButton*>(fxDelayPingPong)->setTooltip("On: the repeats alternate between left and right. Off: each side repeats on its own.");}
        voiceView={&ampEnvelope,&modEnvelope,&modSourceTitle,&modTargetTitle,&modAmountTitle,&matrixView,&modHint,&seqGraph,seqSteps,seqRate,seqSmooth,seqRetrigger,
                   subOn,subOctave,subShape,subLevel,noiseOn,noiseColor,noiseLevel};
        for(auto& knobs:envKnobs)for(auto* k:knobs)voiceView.push_back(k);
        for(auto& b:modTabs)voiceView.push_back(&b);
        for(int l=0;l<2;++l)for(juce::Component* c:{lfoShape[size_t(l)],lfoSync[size_t(l)],static_cast<juce::Component*>(lfoRate[size_t(l)]),static_cast<juce::Component*>(&lfoGraphs[size_t(l)])})voiceView.push_back(c);
        for(auto* m:macros)voiceView.push_back(m);
        showEffects(false);
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
        // The engine's strip behind the top bar (Tools/generate_post_rack_faces.py),
        // dimmed under the controls, which name the engine.
        shownType=juce::jlimit(0,2,juce::roundToInt(state.getRawParameterValue("synthType")->load()));
        art::synthStrip(g,{0.f,0.f,float(getWidth()),26.f},shownType,false);
        g.setColour(juce::Colours::black.withAlpha(.35f));g.fillRect(0,0,getWidth(),26);
        std::vector<std::pair<juce::Rectangle<int>,const char*>> drawn;
        {const auto all=panels();for(size_t i=0;i<all.size();++i)if(!(fxView && i>=4 && i<=8))drawn.push_back(all[i]);}
        if(fxView)for(const auto& area:effectPanels())drawn.push_back({area,""});
        for(const auto& [area,title]:drawn){
            g.setColour(panelColour);g.fillRoundedRectangle(area.toFloat(),5);g.setColour(edgeColour);g.drawRoundedRectangle(area.toFloat().reduced(.5f),5,1);
            g.setColour(mutedColour);g.setFont(juce::FontOptions(10.f,juce::Font::bold));g.drawText(title,area.reduced(8,4).removeFromTop(14),juce::Justification::centredRight);
        }
        // Sample mode: the root note and the pitch found in the sample.
        g.setFont(juce::FontOptions(9.f));
        for(int o=0;o<synthOscCount;++o){
            const auto& p=oscPanels[size_t(o)];if(!p.loop->isVisible())continue;
            const auto sample=processor.synthEngine().sharedSample(o);
            const auto area=p.shapes->getBounds().withTrimmedLeft(p.loop->getWidth()+p.root->getWidth()+8);
            g.setColour(mutedColour);
            g.drawText(sample==nullptr?juce::String("NO SAMPLE"):sample->detectedHz>0?"FOUND "+juce::String(sample->detectedHz,1)+" Hz":juce::String("NO PITCH FOUND"),area,juce::Justification::centredLeft);
        }
    }
    void resized() override {
        auto top=getLocalBounds().removeFromTop(26);
        on->setBounds(top.removeFromLeft(80).reduced(0,1));top.removeFromLeft(6);
        type->setBounds(top.removeFromLeft(298).reduced(0,1));top.removeFromLeft(6);
        character->setBounds(top.removeFromLeft(140).reduced(0,2));top.removeFromLeft(6);
        source->setBounds(top.removeFromLeft(118).reduced(0,1));top.removeFromLeft(6);
        insert->setBounds(top.removeFromLeft(118).reduced(0,1));top.removeFromLeft(6);
        patch.setBounds(top.removeFromLeft(102).reduced(0,1));top.removeFromLeft(6);
        fxButton.setBounds(top.removeFromLeft(38).reduced(0,1));top.removeFromLeft(6);
        for(size_t i=0;i<header.size();++i){auto* k=header[i];k->setCompact();k->setBounds(top.removeFromLeft(std::min(100,top.getWidth()/int(header.size()-i))).reduced(0,1));k->setIncDec(i==0?" VOICES":" OCT");top.removeFromLeft(4);}
        const auto p=panels();
        for(int o=0;o<synthOscCount;++o){
            auto r=p[size_t(o)].first.reduced(8,6);auto& panel=oscPanels[size_t(o)];
            auto head=r.removeFromTop(18);panel.on->setBounds(head.removeFromLeft(50));head.removeFromLeft(4);panel.mode->setBounds(head.removeFromLeft(98));
            head.removeFromLeft(4);panel.table->setBounds(head);   // Wave mode; LOAD and X take this space in Sample mode
            panel.clear->setBounds(head.removeFromRight(18));head.removeFromRight(2);panel.load->setBounds(head.removeFromRight(juce::jmin(44,head.getWidth())));
            r.removeFromTop(4);panel.display->setBounds(r.removeFromTop(48));r.removeFromTop(3);
            auto strip=r.removeFromTop(18);panel.shapes->setBounds(strip);
            {auto warpArea=strip;panel.warp->setBounds(warpArea.removeFromLeft(96));warpArea.removeFromLeft(4);panel.warpAmount->setBounds(warpArea);}
            panel.loop->setBounds(strip.removeFromLeft(44));strip.removeFromLeft(4);
            panel.root->setCompact();panel.root->setBounds(strip.removeFromLeft(80));panel.root->setIncDec();
            r.removeFromTop(3);
            auto row1=r.removeFromTop(r.getHeight()/2),row2=r;
            for(int k=0;k<4;++k)panel.knobs[size_t(k)]->setBounds(row1.removeFromLeft(row1.getWidth()/(4-k)));
            for(int k=4;k<7;++k)panel.knobs[size_t(k)]->setBounds(row2.removeFromLeft(row2.getWidth()/(7-k)));
            panel.knobs[7]->setBounds(panel.knobs[6]->getBounds());   // START takes MORPH's place in Sample mode
        }
        {auto r=p[3].first.reduced(8,6);
         // Routing on top, the curve, one row per filter (switch and type on the left, knobs beside), the distortion along the bottom.
         auto head=r.removeFromTop(18);filterRouting->setBounds(head.removeFromLeft(124));head.removeFromLeft(6);filterBalance->setBounds(head.removeFromLeft(juce::jmin(110,head.getWidth()-96)));
         auto dist=r.removeFromBottom(20);r.removeFromBottom(4);
         distOn->setBounds(dist.removeFromLeft(42));dist.removeFromLeft(4);distType->setBounds(dist.removeFromLeft(86));dist.removeFromLeft(4);distPlace->setBounds(dist.removeFromLeft(104));dist.removeFromLeft(4);
         distDrive->setBounds(dist.removeFromLeft(dist.getWidth()/2-2));dist.removeFromLeft(4);distMix->setBounds(dist);
         const auto row=[](juce::Rectangle<int> area,juce::Component* on,juce::Component* type,const std::vector<Knob*>& knobs){
             auto left=area.removeFromLeft(98);on->setBounds(left.removeFromTop(20).withTrimmedRight(26));left.removeFromTop(3);type->setBounds(left.removeFromTop(20));area.removeFromLeft(4);
             const int w=area.getWidth()/5;for(auto* k:knobs)k->setBounds(area.removeFromLeft(w));};
         auto second=r.removeFromBottom(42);r.removeFromBottom(3);auto first=r.removeFromBottom(42);r.removeFromBottom(3);
         row(first,filterOn,filterType,filterKnobs);row(second,filter2On,filter2Type,filter2Knobs);
         r.removeFromTop(4);filter.setBounds(r);}
        for(int e=0;e<2;++e){auto r=p[size_t(4+e)].first.reduced(8,6);r.removeFromTop(16);
            (e==0?ampEnvelope:modEnvelope).setBounds(r.removeFromTop(r.getHeight()-56));r.removeFromTop(4);
            for(size_t k=0;k<4;++k)envKnobs[size_t(e)][k]->setBounds(r.removeFromLeft(r.getWidth()/int(4-k)));}
        {auto r=p[6].first.reduced(8,6);
         {auto tabs=r.removeFromTop(16);for(int t=0;t<3;++t)modTabs[size_t(t)].setBounds(tabs.removeFromLeft(tabs.getWidth()/(3-t)));}
         r.removeFromTop(4);
         {auto q=r;auto bottom=q.removeFromBottom(18);q.removeFromBottom(3);auto middle=q.removeFromBottom(20);q.removeFromBottom(3);auto upper=q.removeFromBottom(18);q.removeFromBottom(3);seqGraph.setBounds(q);
          seqSteps->setCompact();seqSteps->setBounds(upper);seqSteps->setIncDec(" STEPS");
          seqRetrigger->setBounds(middle.removeFromRight(48));middle.removeFromRight(4);seqRate->setBounds(middle);seqSmooth->setBounds(bottom);}
         {auto q=r;const int h=juce::jmin(26,q.getHeight()/synthMacroCount);for(auto* m:macros)m->setBounds(q.removeFromTop(h).reduced(0,2));}
         for(int l=0;l<2;++l){auto half=r.removeFromTop(r.getHeight()/(2-l));auto head=half.removeFromTop(18);
             lfoShape[size_t(l)]->setBounds(head.removeFromLeft(head.getWidth()/2-2));head.removeFromLeft(4);lfoSync[size_t(l)]->setBounds(head);
             lfoRate[size_t(l)]->setBounds(half.removeFromRight(48));lfoGraphs[size_t(l)].setBounds(half.reduced(0,3));}}
        layoutMatrix();
        {const auto areas=effectPanels();
         for(int e=0;e<synthEffectCount;++e){auto r=areas[size_t(e)].reduced(8,6);auto& panel=fxPanels[size_t(e)];
            auto head=r.removeFromTop(20);panel.on->setBounds(head.removeFromLeft(74));r.removeFromTop(6);
            if(e==synthFxDelay){auto row=r.removeFromTop(20);fxDelayPingPong->setBounds(row.removeFromRight(78));row.removeFromRight(4);fxDelaySync->setBounds(row);r.removeFromTop(6);}
            const int count=int(panel.knobs.size()),perRow=count==4?2:count,rows=(count+perRow-1)/perRow,rowHeight=juce::jmin(76,r.getHeight()/rows);
            r=r.withSizeKeepingCentre(r.getWidth(),rowHeight*rows);
            for(int k=0;k<count;++k){if(k%perRow==0)head=r.removeFromTop(rowHeight);panel.knobs[size_t(k)]->setBounds(head.removeFromLeft(head.getWidth()/(perRow-k%perRow)));}}}
        {auto r=p[8].first.reduced(8,6);r.removeFromTop(16);auto sub=r.removeFromTop(r.getHeight()/2);
         auto head=sub.removeFromTop(18);subOn->setBounds(head.removeFromLeft(50));sub.removeFromTop(3);
         auto menus=sub.removeFromLeft(sub.getWidth()*3/5);subOctave->setBounds(menus.removeFromTop(20));menus.removeFromTop(3);subShape->setBounds(menus.removeFromTop(20));
         subLevel->setBounds(sub.withTrimmedLeft(4));
         auto headNoise=r.removeFromTop(18);noiseOn->setBounds(headNoise.removeFromLeft(60));
         noiseColor->setBounds(r.removeFromLeft(r.getWidth()/2));noiseLevel->setBounds(r);}
        {auto r=p[9].first.reduced(8,6);auto title=r.removeFromTop(16);output->setBounds(title.removeFromLeft(title.getWidth()-110).withHeight(18));r.removeFromTop(3);scope.setBounds(r.removeFromTop(r.getHeight()-56));r.removeFromTop(4);
         for(size_t k=0;k<masterKnobs.size();++k)masterKnobs[k]->setBounds(r.removeFromLeft(r.getWidth()/int(masterKnobs.size()-k)));}
        updateModes();
    }
    juce::Component* control(const juce::String& id) {
        for(auto& k:owned){
            if(k->getComponentID()==id)return k.get();
            if(auto* knob=dynamic_cast<Knob*>(k.get()))if(knob->slider.getComponentID()==id)return &knob->slider;
            if(auto* seg=dynamic_cast<Segmented*>(k.get()))for(int i=0;i<seg->count();++i)if(id==seg->getComponentID()+juce::String(i))return &seg->button(i);
        }
        const std::function<juce::Component*(juce::Component&)> find=[&](juce::Component& parent)->juce::Component* {
            for(auto* child:parent.getChildren()){if(child->getComponentID()==id)return child;if(auto* deeper=find(*child))return deeper;}return nullptr;};
        if(auto* direct=find(*this))return direct;   // members such as the matrix's + button, inside its scrolling list
        return getComponentID()==id?this:nullptr;
    }
    FilterCurve& filterCurve() noexcept {return filter;}
    EnvelopeGraph& envelopeGraph(int index) noexcept {return index==0?ampEnvelope:modEnvelope;}
    // The matrix target a knob stands for (0: none). Octave, semitones and fine all stand for the oscillator's pitch.
    static int modTargetFor(const juce::String& id) {
        for(int o=0;o<synthOscCount;++o){
            if(id==synthOscID(o,"Octave")||id==synthOscID(o,"Semi")||id==synthOscID(o,"Fine"))return targetPitchA+o;
            if(id==synthOscID(o,"Wave"))return targetWaveA+o;
            if(id==synthOscID(o,"Level"))return targetLevelA+o;
            if(id==synthOscID(o,"Detune"))return targetDetune;
        }
static const std::array<std::pair<const char*,int>,8> direct{{{"synthFM",targetFM},{"synthCutoff",targetCutoff},{"synthResonance",targetResonance},{"synthSubLevel",targetSubLevel},{"synthNoiseLevel",targetNoiseLevel},{"synthLevel",targetAmp},
            {"synthFilter2Cutoff",targetCutoff2},{"synthFilter2Resonance",targetResonance2}}};
        for(const auto& [name,target]:direct)if(id==name)return target;
        return 0;
    }
    // Rows on screen and the marks on modulated knobs follow the parameters (a patch or the host may change them).
    void refreshModulation() {if(modShownMask()!=shownModRows)layoutMatrix();updateModulatedKnobs();}
    // Sixteen rows: 0-7 are the released parameters, 8-15 the second bank.
    static constexpr int modRowCount=synthModSlots+synthModSlotsB;
    static juce::String modRowID(int row,const char* field) {return row<synthModSlots?synthModID(row,field):synthModBID(row-synthModSlots,field);}
    // New connections take the second bank first (it knows every source and target), then the released rows.
    static int modRowByPreference(int k) {return k<synthModSlotsB?synthModSlots+k:k-synthModSlotsB;}
    int modValue(int row,const char* field) const {return juce::roundToInt(state.getRawParameterValue(modRowID(row,field))->load());}
    bool modRowUsed(int row) const {return modValue(row,"Source")>0 || modValue(row,"Target")>0;}
    // On screen: the rows in use and those added with +; with neither, one empty row.
    bool modRowShown(int row) const {
        if(modRowUsed(row) || revealed[size_t(row)])return true;
        if(row!=synthModSlots)return false;
        for(int m=0;m<modRowCount;++m)if(modRowUsed(m) || revealed[size_t(m)])return false;
        return true;
    }
    int modRowsShown() const {int n=0;for(int m=0;m<modRowCount;++m)n+=modRowShown(m);return n;}
    int modShownMask() const {int mask=0;for(int m=0;m<modRowCount;++m)mask|=modRowShown(m)?1<<m:0;return mask;}
    // Connects `source` to `target`: the row that already does, else the first free row. Returns the row or -1.
    int assignModulation(int source,int target) {
        int row=-1;const bool wide=source>=synthModSourceCount || target>=synthModTargetCount;   // only rows 9-16 can hold these
        for(int m=0;m<modRowCount && row<0;++m)if(modValue(m,"Source")==source && modValue(m,"Target")==target)row=m;
        for(int k=0;k<modRowCount && row<0;++k){const int m=modRowByPreference(k);if(!(wide && m<synthModSlots) && !modRowUsed(m))row=m;}
        if(row<0)return -1;
        setParameter(modRowID(row,"Source"),float(source));setParameter(modRowID(row,"Target"),float(target));
        if(modValue(row,"Amount")==0)setParameter(modRowID(row,"Amount"),50.f);
        revealed[size_t(row)]=true;layoutMatrix();updateModulatedKnobs();
        return row;
    }
    void clearModulation(int row) {
        for(const char* field:{"Source","Target","Amount"})setParameter(modRowID(row,field),0.f);
        revealed[size_t(row)]=false;
        layoutMatrix();updateModulatedKnobs();
    }
    // LFO, SEQ or MACRO in the modulation-source panel.
    void showModTab(int tab) {
        modTab=juce::jlimit(0,2,tab);modTabs[size_t(modTab)].setToggleState(true,juce::dontSendNotification);
        for(int l=0;l<2;++l)for(juce::Component* c:{lfoShape[size_t(l)],lfoSync[size_t(l)],static_cast<juce::Component*>(lfoRate[size_t(l)]),static_cast<juce::Component*>(&lfoGraphs[size_t(l)])})c->setVisible(modTab==0);
        for(juce::Component* c:{static_cast<juce::Component*>(&seqGraph),static_cast<juce::Component*>(seqSteps),seqRate,static_cast<juce::Component*>(seqSmooth),seqRetrigger})c->setVisible(modTab==1);
        for(auto* m:macros)m->setVisible(modTab==2);
    }
    StepGraph& stepGraph() noexcept {return seqGraph;}
    // The effect rack, or the envelopes, LFOs, matrix and sub / noise, in the lower row (a view of the page, not a parameter).
    void showEffects(bool show) {
        fxView=show;fxButton.setToggleState(show,juce::dontSendNotification);
        for(auto* c:voiceView)c->setVisible(!show);
        for(auto& panel:fxPanels){panel.on->setVisible(show);for(auto* k:panel.knobs)k->setVisible(show);}
        fxDelaySync->setVisible(show);fxDelayPingPong->setVisible(show);
        if(!show){showModTab(modTab);layoutMatrix();}
        updateModes();repaint();
    }
    bool effectsShown() const noexcept {return fxView;}
    void mouseDown(const juce::MouseEvent& e) override {
        if(!e.mods.isPopupMenu() || e.eventComponent==nullptr || e.eventComponent==this)return;
        const int target=modTargetFor(e.eventComponent->getComponentID());if(target<=0)return;
        juce::PopupMenu menu;const auto sources=synthModSourceNamesB();const auto name=synthModTargetNamesB()[target];
        menu.addSectionHeader("MODULATE "+name.toUpperCase()+" WITH");
        bool any=false;
        for(int source=1;source<sources.size();++source){bool on=false;for(int m=0;m<modRowCount;++m)on|=modValue(m,"Source")==source && modValue(m,"Target")==target;any|=on;menu.addItem(source,sources[source],true,on);}
        menu.addSeparator();menu.addItem(100,"Remove every modulation of "+name,any);
        juce::Component::SafePointer<SynthPage> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.eventComponent),[safe,target](int result){
            if(!safe || result<=0)return;
            if(result==100){for(int m=0;m<modRowCount;++m)if(safe->modValue(m,"Target")==target)safe->clearModulation(m);return;}
            if(safe->assignModulation(result,target)<0)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Modulation matrix full","Every modulation row that can hold this connection is in use. Remove one first.");
        });
    }
    WaveDisplay& waveDisplay(int osc) noexcept {return *oscPanels[size_t(juce::jlimit(0,synthOscCount-1,osc))].display;}
    WaveShapes& waveShapes(int osc) noexcept {return *oscPanels[size_t(juce::jlimit(0,synthOscCount-1,osc))].shapes;}
    // Shows each oscillator's Wave or Sample controls, and the character
    // tooltip of the selected type. Called on layout, patch loads and by the timer.
    void updateModes() {
        for(int o=0;o<synthOscCount;++o){
            auto& p=oscPanels[size_t(o)];const bool sample=state.getRawParameterValue(synthOscID(o,"Mode"))->load()>.5f;
            const bool table=!sample && state.getRawParameterValue(synthWavetableID(o,"Table"))->load()>.5f;
            p.shapes->setVisible(!sample && !table);p.knobs[6]->setVisible(!sample);p.knobs[6]->setLabel(table?"POSITION":"MORPH");
            p.table->setVisible(!sample);p.warp->setVisible(table);p.warpAmount->setVisible(table);
            for(juce::Component* c:{static_cast<juce::Component*>(p.load),static_cast<juce::Component*>(p.clear),p.loop,static_cast<juce::Component*>(p.root),static_cast<juce::Component*>(p.knobs[7])})c->setVisible(sample);
        }
        const bool parallel=state.getRawParameterValue("synthFilterRouting")->load()>.5f;
        filterBalance->setVisible(parallel);
        // The delay's TIME is used in Free only.
        if(fxDelaySync!=nullptr){const bool free=state.getRawParameterValue("synthFxDelaySync")->load()<.5f;auto* time=fxPanels[synthFxDelay].knobs[0];time->setEnabled(free);time->setAlpha(free?1.f:.35f);}
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
        return {{{col(0,a0,top,rowA),"OSC A"},{col(a0,a0,top,rowA),"OSC B"},{col(2*a0,a0,top,rowA),"OSC C"},{col(3*a0,a3+gap,top,rowA),"FILTERS / DIST"},
                 {col(0,b0,y2,h2),"ENV 1  AMP"},{col(b0,b1,y2,h2),"ENV 2  MOD"},{col(b0+b1,b2,y2,h2),""},{col(b0+b1+b2,b3,y2,h2),"MOD MATRIX"},
                 {col(b0+b1+b2+b3,b4,y2,h2),"SUB / NOISE"},{col(b0+b1+b2+b3+b4,b5+gap,y2,h2),"SCOPE / MASTER"}}};
    }
    // Five effect panels across the envelopes, LFOs, matrix and sub / noise.
    std::array<juce::Rectangle<int>,synthEffectCount> effectPanels() const {
        const auto p=panels();const int gap=6,x=p[4].first.getX(),right=p[8].first.getRight(),y=p[4].first.getY(),h=p[4].first.getHeight(),w=(right-x+gap)/synthEffectCount;
        std::array<juce::Rectangle<int>,synthEffectCount> areas;
        for(int e=0;e<synthEffectCount;++e)areas[size_t(e)]={x+e*w,y,(e==synthEffectCount-1?right-(x+e*w):w-gap),h};
        return areas;
    }
    template<class C> C* add(std::unique_ptr<C> c){auto* raw=c.get();addAndMakeVisible(*raw);children.push_back(raw);owned.push_back(std::move(c));return raw;}
    void chooseSample(int osc){
        chooser=std::make_unique<juce::FileChooser>("Load a sample into oscillator "+juce::String::charToString("ABC"[osc]),juce::File(),synthSample::formats().getWildcardForAllFormats());
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,osc](const juce::FileChooser& c){
            if(c.getResult().existsAsFile())oscPanels[size_t(osc)].display->load(c.getResult());});
    }
    void timerCallback() override {
        if(!isShowing())return;
        if(const int t=juce::jlimit(0,2,juce::roundToInt(state.getRawParameterValue("synthType")->load()));t!=shownType){shownType=t;repaint(0,0,getWidth(),28);}
        updateModes();
        refreshModulation();
        for(auto& p:oscPanels){p.display->repaint();p.shapes->repaint();}
        filter.repaint();ampEnvelope.repaint();modEnvelope.repaint();for(auto& l:lfoGraphs)l.repaint();if(seqGraph.isVisible())seqGraph.repaint();scope.repaint();
        repaint(oscPanels[0].shapes->getBounds().getUnion(oscPanels[2].shapes->getBounds()));
    }
    struct OscPanel {juce::Component* on{};Segmented* mode{};WaveDisplay* display{};WaveShapes* shapes{};juce::Component* table{};juce::Component* warp{};juce::Component* warpAmount{};juce::TextButton* load{};juce::TextButton* clear{};juce::Component* loop{};Knob* root{};std::vector<Knob*> knobs;};
    void setParameter(const juce::String& id,float value) {
        if(auto* q=state.getParameter(id)){q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(value));q->endChangeGesture();}
    }
    // Lays out the rows on screen in a scrolling list, the + button under them, and hides the rest.
    void layoutMatrix() {
        auto r=panels()[7].first.reduced(8,6);r.removeFromTop(16);
        const int shown=modRowsShown(),h=17;const bool add=shown<modRowCount;
        auto titles=r.removeFromTop(11);
        modHint.setVisible(!fxView && r.getHeight()>=60);if(modHint.isVisible())modHint.setBounds(r.removeFromBottom(12));
        const int content=shown*h+(add?20:0);const bool scroll=content>r.getHeight();const int width=r.getWidth()-(scroll?10:0);
        const auto columns=[](juce::Rectangle<int> row){std::array<juce::Rectangle<int>,4> c;c[3]=row.removeFromRight(16);row.removeFromRight(2);
            c[0]=row.removeFromLeft(row.getWidth()*35/100);row.removeFromLeft(3);c[1]=row.removeFromLeft(row.getWidth()*60/100);row.removeFromLeft(3);c[2]=row;return c;};
        {const auto c=columns(titles.withWidth(width));modSourceTitle.setBounds(c[0]);modTargetTitle.setBounds(c[1]);modAmountTitle.setBounds(c[2]);}
        matrixView.setBounds(r);matrixHolder.setSize(width,juce::jmax(content,1));
        int y=0;
        for(int m=0;m<modRowCount;++m){
            auto& mr=modRows[size_t(m)];const bool visible=modRowShown(m);
            for(juce::Component* c:{mr.source,mr.target,static_cast<juce::Component*>(mr.amount),static_cast<juce::Component*>(mr.clear)})c->setVisible(visible);
            if(!visible)continue;
            const auto c=columns(juce::Rectangle<int>(0,y,width,h).reduced(0,1));mr.source->setBounds(c[0]);mr.target->setBounds(c[1]);mr.amount->setBounds(c[2]);mr.clear->setBounds(c[3]);y+=h;
        }
        modAdd.setVisible(add);if(add)modAdd.setBounds(0,y+2,juce::jmin(130,width),16);
        shownModRows=modShownMask();
    }
    // Marks every knob the matrix moves (the look draws the arc).
    void updateModulatedKnobs() {
        std::array<double,synthModTargetCountB> sum{};
        for(int m=0;m<modRowCount;++m){const int target=modValue(m,"Target");if(modValue(m,"Source")>0 && target>0 && target<synthModTargetCountB)sum[size_t(target)]+=state.getRawParameterValue(modRowID(m,"Amount"))->load()*.01;}
        for(auto& k:owned)if(auto* knob=dynamic_cast<Knob*>(k.get())){const int target=modTargetFor(knob->slider.getComponentID());if(target<=0)continue;
            const double mod=juce::jlimit(-1.0,1.0,sum[size_t(target)]);
            if(std::abs(double(knob->slider.getProperties().getWithDefault("mod",0.0))-mod)>1e-6){knob->slider.getProperties().set("mod",mod);knob->slider.repaint();}}
    }
    struct ModRow {juce::Component* source{};juce::Component* target{};juce::Slider* amount{};juce::TextButton* clear{};std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;};
    SynthLook look;   // declared first: destroyed after every child that uses it
    ChimeraProcessor& processor;juce::AudioProcessorValueTreeState& state;
    std::vector<std::unique_ptr<juce::Component>> owned;std::vector<juce::Component*> children;std::vector<Knob*> header;
    int shownType{};   // the engine whose strip is drawn
    juce::Component* on{};juce::Component* type{};juce::Component* output{};juce::Slider* character{};juce::Component* source{};juce::Component* insert{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> characterAttachment;
    juce::ComboBox patch;std::unique_ptr<juce::FileChooser> chooser;
    std::array<OscPanel,synthOscCount> oscPanels;
    juce::Component* subOn{};juce::Component* subOctave{};juce::Component* subShape{};Knob* subLevel{};
    juce::Component* noiseOn{};Knob* noiseColor{};Knob* noiseLevel{};
    juce::Component* filterOn{};juce::Component* filterType{};FilterCurve filter;std::vector<Knob*> filterKnobs;
    juce::Component* filter2On{};juce::Component* filter2Type{};std::vector<Knob*> filter2Knobs;juce::Component* filterRouting{};juce::Slider* filterBalance{};
    juce::Component* distOn{};juce::Component* distType{};juce::Component* distPlace{};juce::Slider* distDrive{};juce::Slider* distMix{};
    EnvelopeGraph ampEnvelope,modEnvelope;std::array<std::vector<Knob*>,2> envKnobs;
    std::array<juce::Component*,2> lfoShape{},lfoSync{};std::array<Knob*,2> lfoRate{};std::array<LfoGraph,2> lfoGraphs;
    std::array<ModRow,modRowCount> modRows;juce::Label modSourceTitle,modTargetTitle,modAmountTitle,modHint;juce::TextButton modAdd;std::array<bool,modRowCount> revealed{};int shownModRows{-1};
    juce::Component matrixHolder;juce::Viewport matrixView;   // after the rows it shows; the page's `owned` controls outlive both
    std::array<juce::TextButton,3> modTabs;int modTab{};StepGraph seqGraph;Knob* seqSteps{};juce::Component* seqRate{};juce::Slider* seqSmooth{};juce::Component* seqRetrigger{};std::array<juce::Slider*,synthMacroCount> macros{};
    Scope scope;std::vector<Knob*> masterKnobs;
    struct FxPanel {juce::Component* on{};std::vector<Knob*> knobs;};
    juce::TextButton fxButton;bool fxView{};std::array<FxPanel,synthEffectCount> fxPanels;juce::Component* fxDelaySync{};juce::Component* fxDelayPingPong{};std::vector<juce::Component*> voiceView;
};
} // namespace spectralforge::synthUI

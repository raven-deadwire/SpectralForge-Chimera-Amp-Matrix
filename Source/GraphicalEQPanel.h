#pragma once
#include "GraphicalEQ.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

namespace spectralforge {
class GraphicalEQPanel : public juce::Component, private juce::Timer {
public:
    GraphicalEQPanel(juce::AudioProcessorValueTreeState& s,GraphicalEQ& d,int e):state(s),dsp(d),instance(e) {
        setOpaque(true);setComponentID(e==0?"toneEQ_panel":"finalEQ_panel");
        bypass.setButtonText("BYPASS");bypass.setClickingTogglesState(true);bypass.setComponentID(eqID(e,-1,"bypass"));
        addAndMakeVisible(bypass);bypassAttachment=std::make_unique<BA>(state,eqID(e,-1,"bypass"),bypass);
        analyzerToggle.setButtonText("FFT IN / OUT");analyzerToggle.setToggleState(true,juce::dontSendNotification);analyzerToggle.setClickingTogglesState(true);
        analyzerToggle.onClick=[this]{updateAnalyzer();repaint();};addAndMakeVisible(analyzerToggle);
        for(int b=0;b<graphicalEQBands;++b)selector.addItem("Band "+juce::String(b+1),b+1);
        selector.setComponentID(eqID(e,-1,"band"));selector.setSelectedId(1);selector.onChange=[this]{selectBand(selector.getSelectedId()-1);};addAndMakeVisible(selector);
        enabled.setButtonText("BAND ON");enabled.setClickingTogglesState(true);addAndMakeVisible(enabled);
        type.addItemList(eqFilterNames(),1);type.onChange=[this]{sliders[1].setEnabled(gainApplicable());rebuildResponse();repaint();};addAndMakeVisible(type);
        for(size_t i=0;i<sliders.size();++i) {
            auto& slider=sliders[i];slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,145,24);
            slider.setName(i==0?"Frequency Hz":i==1?"Gain dB":"Q factor");
            slider.textFromValueFunction=[i](double v){return juce::String(v,i==0?1:2)+(i==0?" Hz":i==1?" dB":" Q");};
            slider.valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue();};addAndMakeVisible(slider);
        }
        for(auto& s:spectra)s.fill(-120);
        selectBand(0);startTimerHz(25);
    }
    ~GraphicalEQPanel() override {stopTimer();endDrag();dsp.analyzer.enabled.store(false);}
    void visibilityChanged() override {updateAnalyzer();if(!isShowing())fftPosition=0;}
    void parentHierarchyChanged() override {updateAnalyzer();}
    int selectedBand() const noexcept {return selected;}
    void setNavigationSelected(bool selectedPanel) {if(navigationSelected!=selectedPanel){navigationSelected=selectedPanel;repaint();}}
    bool isNavigationSelected() const noexcept {return navigationSelected;}
    void focusForNavigation() {if(isShowing())sliders[0].grabKeyboardFocus();}
    uint64_t fftFrames() const noexcept {return analyzedFrames;}
    double lastFFTTimeMicros() const noexcept {return fftMicros;}
    float spectrumDB(bool output,int bin) const noexcept {return spectra[output?1:0][(size_t)juce::jlimit(0,1024,bin)];}
    juce::Point<float> nodePosition(int band) const {const auto v=dsp.values(band);return {frequencyX(v.frequency),gainY(v.type<=2?v.gain:0.f)};}
    void resized() override {
        bypass.setBounds(getWidth()-104,10,94,26);analyzerToggle.setBounds(10,39,142,26);
        selector.setBounds(10,getHeight()-106,108,26);enabled.setBounds(124,getHeight()-106,104,26);type.setBounds(234,getHeight()-106,getWidth()-244,26);
        const int w=(getWidth()-30)/3;for(int i=0;i<3;++i)sliders[(size_t)i].setBounds(10+i*(w+5),getHeight()-63,w,45);
        rebuildResponse();repaint();
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff151b1c));g.setColour(juce::Colour(0xffb9c8c5));g.setFont(15);
        if(navigationSelected){g.setColour(juce::Colour(0xff94d2ae));g.drawRect(getLocalBounds(),2);g.setColour(juce::Colour(0xffb9c8c5));}
        g.drawText(instance==0?"TONE EQ / BEFORE POST":"FINAL EQ / AFTER WIDTH",10,8,getWidth()-120,28,juce::Justification::centredLeft);
        const auto area=plot();g.setColour(juce::Colour(0xff0b1012));g.fillRect(area);
        for(float f:{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f}) {
            const auto x=frequencyX(f);g.setColour(juce::Colour(0xff30393a));g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
            g.setColour(juce::Colour(0xff9baaa8));g.setFont(9);g.drawText(f<1000?juce::String(int(f)):juce::String(f/1000,0)+"k",int(x)-17,int(area.getBottom())+3,34,14,juce::Justification::centred);
        }
        for(int db=-24;db<=24;db+=12) {const auto y=gainY(float(db));g.setColour(juce::Colour(db==0?0xff576768:0xff30393a));g.drawHorizontalLine(int(y),area.getX(),area.getRight());g.setColour(juce::Colour(0xff9baaa8));g.drawText(juce::String(db),2,int(y)-7,32,14,juce::Justification::centredRight);}
        for(int db:{0,-45,-90})g.drawText(juce::String(db),int(area.getRight())+2,int(area.getBottom()-(db+90)/90.f*area.getHeight())-7,28,14,juce::Justification::centredLeft);
        if(analyzerToggle.getToggleState()) {
            g.saveState();g.reduceClipRegion(area.toNearestInt());
            for(int stream=0;stream<2;++stream) {juce::Path p;bool first=true;for(int x=0;x<int(area.getWidth());++x) {
                const float f=xFrequency(area.getX()+float(x));const int bin=juce::jlimit(0,1024,int(std::round(f*2048/dsp.sampleRate())));
                const float db=spectra[(size_t)stream][(size_t)bin];const float y=area.getBottom()-(db+90)/90*area.getHeight();
                if(first){p.startNewSubPath(area.getX()+float(x),y);first=false;}else p.lineTo(area.getX()+float(x),y);
            }g.setColour(juce::Colour(stream==0?0xff6b91b2:0xffbaaa68).withAlpha(.65f));g.strokePath(p,juce::PathStrokeType(1));}
            g.restoreState();
        }
        g.saveState();g.reduceClipRegion(area.toNearestInt());g.setColour(juce::Colour(0xff94d2ae).withAlpha(dsp.isBypassed()?.35f:1.f));g.strokePath(response,juce::PathStrokeType(2));g.restoreState();
        for(int b=0;b<graphicalEQBands;++b) {
            const auto v=dsp.values(b);const auto p=nodePosition(b);g.setColour(juce::Colour(b==selected?0xfff1d490:v.enabled?0xff91cda9:0xff566868));
            if(v.enabled)g.fillEllipse(p.x-8,p.y-8,16,16);else g.drawEllipse(p.x-8,p.y-8,16,16,1.5f);
            g.setFont(9);g.setColour(v.enabled?juce::Colours::black:juce::Colour(0xffd6ddda));g.drawText(juce::String(b+1),int(p.x)-9,int(p.y)-7,18,14,juce::Justification::centred);
        }
        g.setFont(9);g.setColour(juce::Colour(0xff9baaa8));g.drawText("Drag: Hz / dB   Wheel: Q   Blue IN / Gold OUT dBFS",160,40,getWidth()-170,24,juce::Justification::centredLeft);
        const int w=(getWidth()-30)/3;for(int i=0;i<3;++i)g.drawText(i==0?"FREQUENCY":i==1?"GAIN dB":"Q",10+i*(w+5),getHeight()-78,w,12,juce::Justification::centred);
        g.drawText("EQ CPU "+juce::String(dsp.average.load(),1)+"% / peak "+juce::String(dsp.peak.load(),1)+"%",10,getHeight()-18,getWidth()-20,16,juce::Justification::centredLeft);
    }
    void mouseDown(const juce::MouseEvent& event) override {
        if(!plot().contains(event.position))return;
        float distance=18;int closest=-1;
        for(int b=0;b<graphicalEQBands;++b) {const float d=nodePosition(b).getDistanceFrom(event.position);if(d<distance){distance=d;closest=b;}}
        if(closest<0)return;selector.setSelectedId(closest+1,juce::dontSendNotification);selectBand(closest);dragging=true;
        parameter("frequency")->beginChangeGesture();parameter("gain")->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        if(!dragging)return;write("frequency",xFrequency(event.position.x));if(gainApplicable())write("gain",yGain(event.position.y));rebuildResponse();repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {endDrag();}
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override {
        if(!plot().contains(event.position))return;auto* p=parameter("q");p->beginChangeGesture();write("q",dsp.values(selected).q*std::exp(wheel.deltaY*1.5f));p->endChangeGesture();rebuildResponse();repaint();
    }
private:
    bool navigationSelected{};
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    juce::Rectangle<float> plot() const {return {40,72,float(getWidth()-76),float(getHeight()-200)};}
    float frequencyX(float hz) const {return plot().getX()+plot().getWidth()*std::log(std::clamp(hz,20.f,20000.f)/20)/std::log(1000.f);}
    float xFrequency(float x) const {return 20*std::pow(1000.f,std::clamp((x-plot().getX())/std::max(1.f,plot().getWidth()),0.f,1.f));}
    float gainY(float db) const {return plot().getCentreY()-db/48*plot().getHeight();}
    float yGain(float y) const {return std::clamp((plot().getCentreY()-y)/std::max(1.f,plot().getHeight())*48,-24.f,24.f);}
    juce::RangedAudioParameter* parameter(const char* field) {return state.getParameter(eqID(instance,selected,field));}
    void write(const char* field,float value) {auto* p=parameter(field);p->setValueNotifyingHost(juce::jlimit(0.f,1.f,p->convertTo0to1(value)));}
    bool gainApplicable() const {return dsp.values(selected).type<=2;}
    void endDrag() {if(dragging){parameter("frequency")->endChangeGesture();parameter("gain")->endChangeGesture();dragging=false;}}
    void selectBand(int band) {
        endDrag();selected=juce::jlimit(0,graphicalEQBands-1,band);
        enabledAttachment.reset();typeAttachment.reset();for(auto& a:sliderAttachments)a.reset();
        enabled.setComponentID(eqID(instance,selected,"enabled"));type.setComponentID(eqID(instance,selected,"type"));
        enabledAttachment=std::make_unique<BA>(state,eqID(instance,selected,"enabled"),enabled);
        typeAttachment=std::make_unique<CA>(state,eqID(instance,selected,"type"),type);
        constexpr std::array<const char*,3> fields{"frequency","gain","q"};
        for(size_t i=0;i<3;++i){sliders[i].setComponentID(eqID(instance,selected,fields[i]));sliderAttachments[i]=std::make_unique<SA>(state,eqID(instance,selected,fields[i]),sliders[i]);
            sliders[i].textFromValueFunction=[i](double v){return juce::String(v,i==0?1:i==1?2:3)+(i==0?" Hz":i==1?" dB":" Q");};
            sliders[i].valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue();};sliders[i].updateText();}
        sliders[1].setEnabled(gainApplicable());repaint();
    }
    void updateAnalyzer() {dsp.analyzer.enabled.store(isShowing() && analyzerToggle.getToggleState());}
    void rebuildResponse() {
        response.clear();const auto area=plot();if(area.getWidth()<=0 || area.getHeight()<=0)return;
        std::array<EQCoefficients,graphicalEQBands> coeffs{};std::array<bool,graphicalEQBands> active{};
        for(int b=0;b<graphicalEQBands;++b){const auto v=dsp.values(b);active[(size_t)b]=v.enabled;coeffs[(size_t)b]=eqCoefficients(v.type,v.frequency,v.gain,v.q,dsp.sampleRate());}
        for(int x=0;x<int(area.getWidth());++x) {const float f=xFrequency(area.getX()+float(x));double magnitude=1;
            for(int b=0;b<graphicalEQBands;++b)if(active[(size_t)b])magnitude*=coeffs[(size_t)b].magnitude(f,dsp.sampleRate());
            const float y=gainY(float(20*std::log10(std::max(1.e-12,magnitude))));
            if(x==0)response.startNewSubPath(area.getX(),y);else response.lineTo(area.getX()+float(x),y);
        }
    }
    void analyzeFrame() {
        const auto start=std::chrono::steady_clock::now();
        std::array<std::array<float,1025>,2> power{};
        for(int stream=0;stream<4;++stream) {
            work.fill(0);for(int n=0;n<2048;++n)work[(size_t)n]=fftInput[(size_t)stream][(size_t)n]*(.5f-.5f*std::cos(2*juce::MathConstants<float>::pi*float(n)/2048));
            fft.performFrequencyOnlyForwardTransform(work.data());
            for(int k=0;k<=1024;++k)power[(size_t)(stream%2)][(size_t)k]+=.5f*work[(size_t)k]*work[(size_t)k]/(512.f*512.f);
        }
        for(int s=0;s<2;++s)for(int k=0;k<=1024;++k)spectra[(size_t)s][(size_t)k]=10*std::log10(std::max(1.e-12f,power[(size_t)s][(size_t)k]));
        fftMicros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();++analyzedFrames;
    }
    void timerCallback() override {
        updateAnalyzer();if(!isShowing())return;bool changed=false;
        if(dsp.analyzer.enabled.load()) {
            EQAnalyzerFrame frame;bool ready=false;
            // Drain at most the FIFO capacity so UI work is bounded under live audio.
            for(uint32_t n=0;n<EQAnalyzerFIFO::capacity && dsp.analyzer.pop(frame);++n) {
                fftInput[0][(size_t)fftPosition]=frame.inputL;fftInput[1][(size_t)fftPosition]=frame.outputL;
                fftInput[2][(size_t)fftPosition]=frame.inputR;fftInput[3][(size_t)fftPosition]=frame.outputR;
                if(++fftPosition==2048){fftPosition=0;latestFrame=fftInput;ready=true;}
            }
            if(ready){const auto partial=fftInput;fftInput=latestFrame;analyzeFrame();fftInput=partial;changed=true;}
        }
        std::array<float,64> key{};int n=0;key[(size_t)n++]=dsp.isBypassed()?1.f:0.f;
        for(int b=0;b<graphicalEQBands;++b){const auto v=dsp.values(b);for(float f:{v.enabled?1.f:0.f,v.frequency,v.gain,v.q,float(v.type)})key[(size_t)n++]=f;}
        key[61]=float(dsp.sampleRate());key[62]=std::round(dsp.average.load()*10);key[63]=std::round(dsp.peak.load()*10);
        if(key!=lastKey){lastKey=key;rebuildResponse();sliders[1].setEnabled(gainApplicable());changed=true;}
        if(changed)repaint();
    }
    juce::AudioProcessorValueTreeState& state;GraphicalEQ& dsp;int instance,selected{};bool dragging{};
    juce::TextButton bypass,enabled,analyzerToggle;juce::ComboBox selector,type;std::array<juce::Slider,3> sliders;
    std::unique_ptr<BA> bypassAttachment,enabledAttachment;std::unique_ptr<CA> typeAttachment;std::array<std::unique_ptr<SA>,3> sliderAttachments;
    juce::Path response;std::array<float,64> lastKey{};
    juce::dsp::FFT fft{11};std::array<std::array<float,2048>,4> fftInput{},latestFrame{};std::array<float,4096> work{};
    std::array<std::array<float,1025>,2> spectra{};int fftPosition{};uint64_t analyzedFrames{};double fftMicros{};
};
}

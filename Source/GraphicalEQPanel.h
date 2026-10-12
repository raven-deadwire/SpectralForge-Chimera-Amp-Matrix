#pragma once
#include "GraphicalEQ.h"
#include "EQSpectrumAnalyzer.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace spectralforge {
// Analyzer preferences are presentation only. Sound controls retain the
// released APVTS attachments/IDs, gestures and parameter meanings.
class GraphicalEQPanel : public juce::Component, private juce::Timer {
public:
    GraphicalEQPanel(juce::AudioProcessorValueTreeState& s,GraphicalEQ& d,int e)
        :state(s),dsp(d),instance(e) {
        setOpaque(true);setComponentID(e==0?"toneEQ_panel":"finalEQ_panel");
        bypass.setButtonText("BYPASS EQ");bypass.setClickingTogglesState(true);
        bypass.setComponentID(eqID(e,-1,"bypass"));addAndMakeVisible(bypass);
        bypassAttachment=std::make_unique<BA>(state,eqID(e,-1,"bypass"),bypass);
        const auto displayButton=[this,e](juce::TextButton& button,const char* text,const char* id) {
            button.setButtonText(text);button.setToggleState(true,juce::dontSendNotification);
            button.setClickingTogglesState(true);button.setComponentID(eqID(e,-1,id));addAndMakeVisible(button);
        };
        displayButton(analyzerToggle,"LIVE","analyzer");
        displayButton(inputToggle,"IN","inputSpectrum");displayButton(outputToggle,"OUT","outputSpectrum");
        analyzerToggle.setTooltip("Run the selected EQ's live spectrum analyzer. Analysis does not change the sound.");
        inputToggle.setTooltip("Show the dashed input spectrum of this EQ, averaged by stereo power.");
        outputToggle.setTooltip(e==0?"Show the solid output spectrum of Tone EQ, before POST."
                                    :"Show the solid output spectrum of Final EQ, before output trim and tuner mute.");
        analyzerToggle.onClick=[this]{updateAnalyzer();repaint();};
        inputToggle.onClick=[this]{repaint(plot().toNearestInt());};
        outputToggle.onClick=[this]{repaint(plot().toNearestInt());};
        resolution.setComponentID(eqID(e,-1,"resolution"));
        resolution.addItem("Fast / 2048",1);resolution.addItem("Normal / 4096",2);resolution.addItem("Detailed / 8192",3);
        resolution.setSelectedId(2,juce::dontSendNotification);addAndMakeVisible(resolution);
        resolution.onChange=[this] {
            spectrum.setFFTSize(1 << (resolution.getSelectedId()+10));spectrum.reset(dsp.analyzer);
            clearSpectrumPaths();updateResolutionTooltip();repaint();
        };
        smoothing.setComponentID(eqID(e,-1,"smoothing"));
        smoothing.addItem("Fast",1);smoothing.addItem("Balanced",2);smoothing.addItem("Smooth",3);
        smoothing.setSelectedId(2,juce::dontSendNotification);addAndMakeVisible(smoothing);
        smoothing.setTooltip("Analyzer attack/release only. Time-based smoothing leaves the EQ response and audio unchanged.");
        smoothing.onChange=[this]{spectrum.setSmoothing(smoothing.getSelectedId()-1);};
        for(int b=0;b<graphicalEQBands;++b)selector.addItem("Band "+juce::String(b+1),b+1);
        selector.setComponentID(eqID(e,-1,"band"));selector.setSelectedId(1);
        selector.onChange=[this]{selectBand(selector.getSelectedId()-1);};addAndMakeVisible(selector);
        enabled.setButtonText("BAND ON");enabled.setClickingTogglesState(true);addAndMakeVisible(enabled);
        type.addItemList(eqFilterNames(),1);
        type.onChange=[this]{sliders[1].setEnabled(gainApplicable());rebuildResponse();repaint();};addAndMakeVisible(type);
        for(size_t i=0;i<sliders.size();++i) {
            auto& slider=sliders[i];slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,i==0?108:94,26);
            slider.setName(i==0?"Frequency Hz":i==1?"Gain dB":"Q factor");slider.setWantsKeyboardFocus(true);
            slider.textFromValueFunction=[i](double v){return juce::String(v,i==0?1:2)+(i==0?" Hz":i==1?" dB":" Q");};
            slider.valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue();};addAndMakeVisible(slider);
        }
        selectBand(0);updateResolutionTooltip();startTimerHz(25);
    }
    ~GraphicalEQPanel() override {stopTimer();endDrag();dsp.analyzer.enabled.store(false);}
    void visibilityChanged() override {if(!isShowing())endDrag();updateAnalyzer();}
    void parentHierarchyChanged() override {updateAnalyzer();}
    int selectedBand() const noexcept {return selected;}
    void setNavigationSelected(bool chosen) {if(navigationSelected!=chosen){navigationSelected=chosen;repaint();}}
    bool isNavigationSelected() const noexcept {return navigationSelected;}
    void focusForNavigation() {if(isShowing())sliders[0].grabKeyboardFocus();}
    uint64_t fftFrames() const noexcept {return spectrum.frames();}
    double lastFFTTimeMicros() const noexcept {return spectrum.fftMicros();}
    float spectrumDB(bool output,int bin) const noexcept {return spectrum.rawDB(output,bin);}
    int analysisFFTSize() const noexcept {return spectrum.fftSize();}
    double analysisUpdateMicros() const noexcept {return spectrum.updateMicros();}
    double analysisSampleRate() const noexcept {return spectrum.lastSampleRate();}
    uint64_t analysisTotalConsumedSamples() const noexcept {return spectrum.totalConsumedSamples();}
    uint64_t analysisDiscardedSamples() const noexcept {return spectrum.discardedSamples();}
    uint32_t analysisLastPendingSamples() const noexcept {return spectrum.lastPendingSamples();}
    uint32_t analysisLastConsumedSamples() const noexcept {return spectrum.lastConsumedSamples();}
    uint32_t analysisLastFFTWindows() const noexcept {return spectrum.lastFFTWindows();}
    uint64_t analysisUpdates() const noexcept {return spectrum.updates();}
    double analysisLastConsumerIntervalMs() const noexcept {return spectrum.lastConsumerIntervalMs();}
    double spectrumPathMicros() const noexcept {return pathMicros;}
    double lastPaintMicros() const noexcept {return paintMicros;}
    float displaySpectrumDB(bool output,int bin) const noexcept {return spectrum.smoothedDB(output,bin);}
    bool analyzerHasData() const noexcept {return spectrum.hasData();}
    // Opt-in native-test diagnostics. Kept identical in every translation unit;
    // production editors never install an observer or change the 40 ms cadence.
    // The observer runs only on the message thread, never on the audio callback.
    using DiagnosticObserver=void (*)(void*,const GraphicalEQPanel&,bool,double);
    void configureDiagnostics(int intervalMs,bool renderPanel,DiagnosticObserver observer=nullptr,void* context=nullptr) {
        diagnosticObserver=observer;diagnosticContext=context;diagnosticRenderPanel=renderPanel;
        startTimer(std::max(1,intervalMs));
    }
    void clearDiagnostics() {
        diagnosticObserver=nullptr;diagnosticContext=nullptr;diagnosticRenderPanel=true;startTimerHz(25);
    }
    juce::Rectangle<float> graphBounds() const noexcept {return plot();}
    juce::Point<float> nodePosition(int band) const {
        const auto v=dsp.values(band);return {frequencyX(v.frequency),gainY(v.type<=2?v.gain:0.f)};
    }
    void resized() override {
        // Logical workspace scales with the existing 75-150% editor canvas.
        analyzerToggle.setBounds(350,22,76,28);inputToggle.setBounds(440,22,60,28);outputToggle.setBounds(510,22,64,28);
        resolution.setBounds(592,22,172,28);smoothing.setBounds(780,22,156,28);
        bypass.setBounds(getWidth()-174,22,156,28);
        const int y=getHeight()-45;
        selector.setBounds(16,y,106,28);enabled.setBounds(132,y,96,28);type.setBounds(240,y,174,28);
        sliders[0].setBounds(434,y-3,240,34);sliders[1].setBounds(694,y-3,198,34);sliders[2].setBounds(912,y-3,getWidth()-930,34);
        rebuildResponse();rebuildSpectrumPaths();repaint();
    }
    void paint(juce::Graphics& g) override {
        if(!diagnosticRenderPanel) {
            if(diagnosticObserver)diagnosticObserver(diagnosticContext,*this,true,0.);
            return;
        }
        const auto start=std::chrono::steady_clock::now();
        const juce::Colour text(0xffe0e8e5),muted(0xff9aadaa),grid(0xff283638);
        const juce::Colour in(0xff7ab6cf),out(0xffd3b478),eq(0xffc4ead5);
        g.fillAll(juce::Colour(0xff121a1d));
        g.setGradientFill({juce::Colour(0xff202d30),0.f,0.f,juce::Colour(0xff141e21),0.f,62.f,false});
        g.fillRect(0,0,getWidth(),62);
        g.setColour(navigationSelected?eq.withAlpha(.65f):grid);g.fillRect(0,0,getWidth(),2);
        g.setColour(text);g.setFont(juce::FontOptions(19.f,juce::Font::bold));
        g.drawText(instance==0?"TONE EQ":"FINAL EQ",18,9,310,26,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(juce::FontOptions(12.5f));
        g.drawText(instance==0?"Before POST":"After width / before output trim",18,36,312,17,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(12.f));
        g.drawText("ANALYZER",350,5,80,15,juce::Justification::centredLeft);
        g.drawText("RESOLUTION",592,5,172,15,juce::Justification::centredLeft);
        g.drawText("SMOOTHING",780,5,156,15,juce::Justification::centredLeft);
        g.setColour(in);g.drawLine(441.f,12.f,463.f,12.f,1.4f);g.drawLine(469.f,12.f,491.f,12.f,1.4f);
        g.setColour(out);g.drawLine(512.f,12.f,570.f,12.f,2.f);
        const auto area=plot();
        g.setGradientFill({juce::Colour(0xff0b1216),area.getX(),area.getY(),juce::Colour(0xff142125),area.getX(),area.getBottom(),false});
        g.fillRect(area);
        g.setFont(juce::FontOptions(12.5f));
        for(float f:{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f}) {
            const auto x=frequencyX(f);g.setColour(grid);g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
            g.setColour(muted);g.drawText(f<1000?juce::String(int(f)):juce::String(f/1000,0)+"k",int(x)-20,int(area.getBottom())+5,40,16,juce::Justification::centred);
        }
        for(int db=-24;db<=24;db+=6) {
            const auto y=gainY(float(db));g.setColour(juce::Colour(db==0?0xff617773:0xff283638));
            g.drawHorizontalLine(juce::roundToInt(y),area.getX(),area.getRight());
            if(db%12==0){g.setColour(muted);g.drawText((db>0?"+":"")+juce::String(db),5,int(y)-8,40,16,juce::Justification::centredRight);}
        }
        g.setColour(muted);g.setFont(juce::FontOptions(12.f));
        g.drawText("EQ dB",7,62,40,15,juce::Justification::centredRight);
        g.drawText("dBFS",int(area.getRight())+9,62,42,15,juce::Justification::centredLeft);
        for(int db:{0,-24,-48,-72,-96})g.drawText(juce::String(db),int(area.getRight())+9,int(spectrumY(float(db)))-8,40,16,juce::Justification::centredLeft);
        g.saveState();g.reduceClipRegion(area.toNearestInt());
        if(analyzerToggle.getToggleState() && spectrum.hasData()) {
            for(int stream=0;stream<2;++stream) {
                if(!(stream==0?inputToggle:outputToggle).getToggleState())continue;
                const auto colour=stream==0?in:out;
                juce::ColourGradient fill(colour.withAlpha(stream==0?.23f:.27f),0.f,area.getY(),colour.withAlpha(.015f),0.f,area.getBottom(),false);
                fill.addColour(.64,colour.withAlpha(.08f));g.setGradientFill(fill);g.fillPath(spectrumFills[(size_t)stream]);
                g.setColour(colour.withAlpha(.055f));g.strokePath(spectrumPaths[(size_t)stream],juce::PathStrokeType(5.f));
                g.setColour(colour.withAlpha(stream==0?.74f:.94f));
                if(stream==0)g.fillPath(dashedInput);else g.strokePath(spectrumPaths[1],juce::PathStrokeType(1.7f));
            }
        }
        // Separate the actual summed EQ response from both live spectra.
        g.setColour(juce::Colours::black.withAlpha(.42f));g.strokePath(response,juce::PathStrokeType(5.f));
        g.setColour(eq.withAlpha(dsp.isBypassed()?.38f:1.f));g.strokePath(response,juce::PathStrokeType(2.4f));
        g.restoreState();
        for(int b=0;b<graphicalEQBands;++b)if(b!=selected)drawNode(g,b,false);
        drawNode(g,selected,true);
        if(!spectrum.hasData() || !analyzerToggle.getToggleState()) {
            g.setColour(muted.withAlpha(.85f));g.setFont(juce::FontOptions(12.5f));
            g.drawText(analyzerToggle.getToggleState()?"Waiting for audio":"Analyzer off",int(area.getX())+10,int(area.getBottom())-24,230,18,juce::Justification::centredLeft);
        }
        g.setColour(grid);g.drawRect(area,1.f);g.drawHorizontalLine(getHeight()-65,16.f,float(getWidth()-16));
        g.setColour(muted);g.setFont(juce::FontOptions(12.f));const int labelY=getHeight()-62;
        g.drawText("BAND",16,labelY,106,15,juce::Justification::centredLeft);
        g.drawText("FILTER",240,labelY,174,15,juce::Justification::centredLeft);
        g.drawText("FREQUENCY",434,labelY,240,15,juce::Justification::centredLeft);
        g.drawText("GAIN",694,labelY,198,15,juce::Justification::centredLeft);
        g.drawText("Q",912,labelY,100,15,juce::Justification::centredLeft);
        paintMicros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        if(diagnosticObserver)diagnosticObserver(diagnosticContext,*this,true,paintMicros);
    }
    void mouseDown(const juce::MouseEvent& event) override {
        if(!plot().contains(event.position))return;
        // Hit-test in the same front-to-back order as painting. The selected
        // node is drawn last; coincident nodes must not silently redirect its
        // drag/automation to the first persistent slot.
        float distance=18;int closest=-1;
        const auto consider=[&](int band) {
            const float d=nodePosition(band).getDistanceFrom(event.position);
            if(d<distance){distance=d;closest=band;}
        };
        consider(selected);
        for(int b=graphicalEQBands-1;b>=0;--b)if(b!=selected)consider(b);
        if(closest<0)return;selector.setSelectedId(closest+1,juce::dontSendNotification);selectBand(closest);dragging=true;
        parameter("frequency")->beginChangeGesture();parameter("gain")->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        if(!dragging)return;write("frequency",xFrequency(event.position.x));if(gainApplicable())write("gain",yGain(event.position.y));rebuildResponse();repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {endDrag();}
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override {
        if(!plot().contains(event.position))return;auto* p=parameter("q");p->beginChangeGesture();
        write("q",dsp.values(selected).q*std::exp(wheel.deltaY*1.5f));p->endChangeGesture();rebuildResponse();repaint();
    }
private:
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    juce::Rectangle<float> plot() const noexcept {return {56,80,float(std::max(1,getWidth()-112)),float(std::max(1,getHeight()-170))};}
    float frequencyX(float hz) const {return plot().getX()+plot().getWidth()*std::log(std::clamp(hz,20.f,20000.f)/20)/std::log(1000.f);}
    float xFrequency(float x) const {return 20*std::pow(1000.f,std::clamp((x-plot().getX())/std::max(1.f,plot().getWidth()),0.f,1.f));}
    float gainY(float db) const {return plot().getCentreY()-db/48*plot().getHeight();}
    float yGain(float y) const {return std::clamp((plot().getCentreY()-y)/std::max(1.f,plot().getHeight())*48,-24.f,24.f);}
    float spectrumY(float db) const {return plot().getBottom()-(std::clamp(db,-96.f,0.f)+96)/96*plot().getHeight();}
    juce::RangedAudioParameter* parameter(const char* field){return state.getParameter(eqID(instance,selected,field));}
    void write(const char* field,float value){auto* p=parameter(field);p->setValueNotifyingHost(juce::jlimit(0.f,1.f,p->convertTo0to1(value)));}
    bool gainApplicable() const {return dsp.values(selected).type<=2;}
    void endDrag(){if(dragging){parameter("frequency")->endChangeGesture();parameter("gain")->endChangeGesture();dragging=false;}}
    void drawNode(juce::Graphics& g,int band,bool chosen) const {
        const auto v=dsp.values(band);const auto p=nodePosition(band);const float r=chosen?9.f:v.enabled?8.f:7.5f;
        const juce::Colour colour(chosen?0xfff0d39b:v.enabled?0xffb5d6c6:0xff718d8a);
        if(chosen){g.setColour(colour.withAlpha(.12f));g.fillEllipse(p.x-13,p.y-13,26,26);}
        g.setColour(juce::Colour(0xff101b1e));g.fillEllipse(p.x-r,p.y-r,2*r,2*r);
        g.setColour(colour);if(v.enabled)g.fillEllipse(p.x-r,p.y-r,2*r,2*r);else g.drawEllipse(p.x-r,p.y-r,2*r,2*r,chosen?2.f:1.f);
        g.setFont(juce::FontOptions(chosen?12.f:11.f,juce::Font::bold));g.setColour(v.enabled?juce::Colour(0xff172421):colour);
        g.drawText(juce::String(band+1),int(p.x)-10,int(p.y)-8,20,16,juce::Justification::centred);
    }
    void selectBand(int band) {
        endDrag();selected=juce::jlimit(0,graphicalEQBands-1,band);
        enabledAttachment.reset();typeAttachment.reset();for(auto& a:sliderAttachments)a.reset();
        enabled.setComponentID(eqID(instance,selected,"enabled"));type.setComponentID(eqID(instance,selected,"type"));
        enabledAttachment=std::make_unique<BA>(state,eqID(instance,selected,"enabled"),enabled);
        typeAttachment=std::make_unique<CA>(state,eqID(instance,selected,"type"),type);
        constexpr std::array<const char*,3> fields{"frequency","gain","q"};
        for(size_t i=0;i<3;++i){
            sliders[i].setComponentID(eqID(instance,selected,fields[i]));
            sliderAttachments[i]=std::make_unique<SA>(state,eqID(instance,selected,fields[i]),sliders[i]);
            sliders[i].textFromValueFunction=[i](double v){return juce::String(v,i==0?1:i==1?2:3)+(i==0?" Hz":i==1?" dB":" Q");};
            sliders[i].valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue();};sliders[i].updateText();
        }
        sliders[1].setEnabled(gainApplicable());repaint();
    }
    void updateAnalyzer() {
        if(!isShowing())endDrag();
        const bool active=isShowing() && analyzerToggle.getToggleState();if(active==analyzerActive)return;
        dsp.analyzer.enabled.store(false);spectrum.reset(dsp.analyzer);clearSpectrumPaths();
        analyzerActive=active;dsp.analyzer.enabled.store(active);repaint();
    }
    void updateResolutionTooltip() {
        const double rate=dsp.sampleRate(),size=spectrum.fftSize();
        resolution.setTooltip(juce::String(int(size))+" samples / "+juce::String(rate/size,2)+" Hz bin spacing / "+juce::String(1000*size/rate,1)+" ms analysis window at "+juce::String(rate/1000,1)+" kHz. Higher resolution responds more slowly; audio latency is unchanged.");
    }
    void rebuildResponse() {
        response.clear();const auto area=plot();if(area.getWidth()<=0 || area.getHeight()<=0)return;
        std::array<EQCoefficients,graphicalEQBands> coeffs{};std::array<bool,graphicalEQBands> active{};
        for(int b=0;b<graphicalEQBands;++b){const auto v=dsp.values(b);active[(size_t)b]=v.enabled;coeffs[(size_t)b]=eqCoefficients(v.type,v.frequency,v.gain,v.q,dsp.sampleRate());}
        for(int x=0;x<=int(area.getWidth());++x){
            const float f=xFrequency(area.getX()+float(x));if(f>dsp.sampleRate()*.5)break;double magnitude=1;
            for(int b=0;b<graphicalEQBands;++b)if(active[(size_t)b])magnitude*=coeffs[(size_t)b].magnitude(f,dsp.sampleRate());
            const float y=gainY(float(20*std::log10(std::max(1.e-12,magnitude))));
            if(x==0)response.startNewSubPath(area.getX(),y);else response.lineTo(area.getX()+float(x),y);
        }
    }
    void clearSpectrumPaths(){for(auto& p:spectrumPaths)p.clear();for(auto& p:spectrumFills)p.clear();dashedInput.clear();}
    void rebuildSpectrumPaths() {
        const auto start=std::chrono::steady_clock::now();clearSpectrumPaths();if(!spectrum.hasData())return;
        const auto area=plot();const int width=int(area.getWidth());
        for(int stream=0;stream<2;++stream){
            auto& path=spectrumPaths[(size_t)stream];float lastX=area.getX();
            for(int x=0;x<=width;++x){
                const float px=area.getX()+float(x),centre=xFrequency(px);if(centre>dsp.sampleRate()*.5)break;
                const auto db=spectrum.displayDB(stream==1,xFrequency(px-.5f),centre,xFrequency(px+.5f));const float y=spectrumY(db);
                if(x==0)path.startNewSubPath(px,y);else path.lineTo(px,y);lastX=px;
            }
            auto& fill=spectrumFills[(size_t)stream];fill=path;fill.lineTo(lastX,area.getBottom());fill.lineTo(area.getX(),area.getBottom());fill.closeSubPath();
        }
        const float dashes[]{4.f,3.f};juce::PathStrokeType(1.25f).createDashedStroke(dashedInput,spectrumPaths[0],dashes,2);
        pathMicros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    }
    void timerCallback() override {
        const auto diagnosticStart=diagnosticObserver?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        updateAnalyzer();if(!isShowing())return;bool changed=false;
        if(diagnosticObserver)pathMicros=0.; // A no-path tick must not reuse a previous measurement.
        if(analyzerActive && spectrum.update(dsp.analyzer,dsp.sampleRate())){rebuildSpectrumPaths();changed=true;}
        // CPU meter fluctuations must not cause a transfer-curve rebuild.
        std::array<float,62> key{};int n=0;key[(size_t)n++]=dsp.isBypassed()?1.f:0.f;
        for(int b=0;b<graphicalEQBands;++b){const auto v=dsp.values(b);for(float f:{v.enabled?1.f:0.f,v.frequency,v.gain,v.q,float(v.type)})key[(size_t)n++]=f;}
        key[61]=float(dsp.sampleRate());
        if(key!=lastKey){const bool rateChanged=key[61]!=lastKey[61];lastKey=key;rebuildResponse();sliders[1].setEnabled(gainApplicable());if(rateChanged)updateResolutionTooltip();changed=true;}
        if(changed && diagnosticRenderPanel)repaint(); // Only this workspace, never the whole editor.
        if(diagnosticObserver)diagnosticObserver(diagnosticContext,*this,false,
            std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-diagnosticStart).count());
    }
    juce::AudioProcessorValueTreeState& state;GraphicalEQ& dsp;int instance,selected{};
    bool dragging{},navigationSelected{},analyzerActive{};
    juce::TextButton bypass,enabled,analyzerToggle,inputToggle,outputToggle;
    juce::ComboBox selector,type,resolution,smoothing;std::array<juce::Slider,3> sliders;
    std::unique_ptr<BA> bypassAttachment,enabledAttachment;std::unique_ptr<CA> typeAttachment;std::array<std::unique_ptr<SA>,3> sliderAttachments;
    juce::Path response,dashedInput;std::array<juce::Path,2> spectrumPaths,spectrumFills;
    std::array<float,62> lastKey{};EQSpectrumAnalyzer spectrum;
    double pathMicros{},paintMicros{};
    DiagnosticObserver diagnosticObserver{};void* diagnosticContext{};bool diagnosticRenderPanel{true};
};
}

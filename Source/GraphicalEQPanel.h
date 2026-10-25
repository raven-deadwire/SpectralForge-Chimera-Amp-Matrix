#pragma once
#include "GraphicalEQ.h"
#include "EQSpectrumAnalyzer.h"
#include "EQEditState.h"
#include "EQCaptureBoundary.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace spectralforge {
// Analyzer preferences are presentation only. Sound controls retain the
// released APVTS attachments/IDs, gestures and parameter meanings.
class GraphicalEQPanel : public juce::Component, private juce::Timer {
public:
    GraphicalEQPanel(juce::AudioProcessorValueTreeState& s,GraphicalEQ& d,int e,EQEditState& edits,EQCaptureBoundary& boundary)
        :state(s),dsp(d),editing(edits),captureBoundary(boundary),instance(e) {
        setOpaque(true);setWantsKeyboardFocus(true);setComponentID(e==0?"toneEQ_panel":"finalEQ_panel");
        lastExternalRevision=editing.externalRevision();
        pendingCaptureRequest=captureBoundary.latestRequest();
        bypass.setButtonText("BYPASS EQ");bypass.setClickingTogglesState(true);
        bypass.setComponentID(eqID(e,-1,"bypass"));addAndMakeVisible(bypass);
        bypassAttachment=std::make_unique<PA>(*state.getParameter(eqID(e,-1,"bypass")),[this](float v){bypass.setToggleState(v>.5f,juce::dontSendNotification);});
        bypassAttachment->sendInitialUpdate();
        bypass.onClick=[this]{discreteEdit(-1,"bypass",bypass.getToggleState()?1.f:0.f,"EQ bypass");};
        const auto displayButton=[this,e](juce::TextButton& button,const char* text,const char* id) {
            button.setButtonText(text);button.setToggleState(true,juce::dontSendNotification);
            button.setClickingTogglesState(true);button.setComponentID(eqID(e,-1,id));addAndMakeVisible(button);
        };
        displayButton(analyzerToggle,"LIVE","analyzer");
        displayButton(inputToggle,"IN","inputSpectrum");displayButton(outputToggle,"OUT","outputSpectrum");
        freezeToggle.setButtonText("FREEZE");freezeToggle.setClickingTogglesState(true);
        freezeToggle.setComponentID(eqID(e,-1,"freeze"));freezeToggle.setEnabled(false);addAndMakeVisible(freezeToggle);
        freezeToggle.setTooltip("Keep the last complete spectrum. Release to wait for a new full analysis window. Audio continues unchanged.");
        freezeToggle.onClick=[this]{setSnapshotFrozen(freezeToggle.getToggleState());};
        const auto editButton=[this,e](juce::TextButton& button,const char* title,const char* id) {
            button.setButtonText(title);button.setComponentID(eqID(e,-1,id));addAndMakeVisible(button);
        };
        editButton(addButton,"+ BAND","addBand");editButton(deleteButton,"DELETE","deleteBand");
        editButton(undoButton,"UNDO","undo");editButton(redoButton,"REDO","redo");
        addButton.setTooltip("Add a Bell band at 1 kHz, or double-click the graph at the frequency and gain you need.");
        deleteButton.setTooltip("Remove this band. BAND ON keeps a bypassed band available for editing.");
        addButton.onClick=[this]{createBand(1000.f,0.f);};deleteButton.onClick=[this]{deleteSelectedBand();};
        undoButton.onClick=[this]{applyHistory(false);};redoButton.onClick=[this]{applyHistory(true);};
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
            if(spectrum.isFrozen()){resolution.setSelectedId(spectrum.fftSize()==2048?1:spectrum.fftSize()==8192?3:2,juce::dontSendNotification);return;}
            spectrum.setFFTSize(1 << (resolution.getSelectedId()+10));spectrum.reset(dsp.analyzer);
            clearSpectrumPaths();updateResolutionTooltip();repaint();
        };
        smoothing.setComponentID(eqID(e,-1,"smoothing"));
        smoothing.addItem("Fast",1);smoothing.addItem("Balanced",2);smoothing.addItem("Smooth",3);
        smoothing.setSelectedId(2,juce::dontSendNotification);addAndMakeVisible(smoothing);
        smoothing.setTooltip("Analyzer attack/release only. Time-based smoothing leaves the EQ response and audio unchanged.");
        smoothing.onChange=[this]{
            if(spectrum.isFrozen()){smoothing.setSelectedId(spectrum.smoothingMode()+1,juce::dontSendNotification);return;}
            spectrum.setSmoothing(smoothing.getSelectedId()-1);
        };
        selector.setComponentID(eqID(e,-1,"band"));selector.setTextWhenNothingSelected("No bands");
        selector.onChange=[this]{selectBand(selector.getSelectedId()-1);};addAndMakeVisible(selector);
        enabled.setButtonText("BAND ON");enabled.setClickingTogglesState(true);addAndMakeVisible(enabled);
        enabled.onClick=[this]{if(selected>=0)discreteEdit(selected,"enabled",enabled.getToggleState()?1.f:0.f,"Band enable");};
        type.addItemList(eqFilterNames(),1);
        type.onChange=[this]{if(selected>=0)discreteEdit(selected,"type",float(type.getSelectedId()-1),"Filter type");updateBandEnablement();};addAndMakeVisible(type);
        for(size_t i=0;i<sliders.size();++i) {
            auto& slider=sliders[i];slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,i==0?108:94,26);
            slider.setName(i==0?"Frequency Hz":i==1?"Gain dB":"Q factor");slider.setWantsKeyboardFocus(true);
            slider.textFromValueFunction=[i](double v){return juce::String(v,i==0?1:2)+(i==0?" Hz":i==1?" dB":" Q");};
            slider.valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue();};addAndMakeVisible(slider);
            slider.onDragStart=[this,i]{
                endDrag();sliderGestureRejected=cancelExternalInput();
                if(sliderGestureRejected || selected<0)return;
                sliderEditing=editing.beginEdit(instance,juce::String("Band ")+juce::String(selected+1)+" "+editFields[i]);
                if(sliderEditing)editing.touchField(instance,selected,editFields[i]);
            };
            slider.onValueChange=[this,i]{
                if(refreshingControls || selected<0)return;
                if(sliderGestureRejected){syncDisplayedValues();return;}
                if(sliderEditing) {
                    if(!editing.setValue(instance,selected,editFields[i],float(sliders[i].getValue())))syncDisplayedValues();
                }
                else discreteEdit(selected,editFields[i],float(sliders[i].getValue()),"Band value");
                rebuildResponse();updateHistoryControls();repaint();
            };
            slider.onDragEnd=[this]{if(sliderEditing){editing.endEdit();sliderEditing=false;refreshEditingState();}sliderGestureRejected=false;};
        }
        // Dynamic EQ lives on the band it acts on. The four shared slots keep
        // their released parameter IDs; DYNAMIC claims or frees a slot.
        dynamicToggle.setButtonText("DYNAMIC");dynamicToggle.setClickingTogglesState(true);dynamicToggle.setComponentID(eqID(e,-1,"dynamic"));
        dynamicToggle.setTooltip("Cut this band only when its frequency range gets louder than Threshold. Tone EQ and Final EQ share four dynamic bands.");
        dynamicToggle.onClick=[this]{setSelectedBandDynamic(dynamicToggle.getToggleState());};addAndMakeVisible(dynamicToggle);
        dynamicStatus.setFont(juce::FontOptions(12.f));dynamicStatus.setComponentID(eqID(e,-1,"dynamicStatus"));
        dynamicStatus.setColour(juce::Label::textColourId,juce::Colour(0xffd3b478));addAndMakeVisible(dynamicStatus);
        for(size_t i=0;i<dynamicSliders.size();++i) {
            auto& slider=dynamicSliders[i];slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,92,26);slider.setWantsKeyboardFocus(true);
            slider.setName(juce::String("Dynamic ")+dynamicFieldNames[i]);slider.setComponentID(eqID(e,-1,(juce::String("dynamic_")+dynamicFields[i]).toRawUTF8()));
            addAndMakeVisible(slider);
        }
        refreshEditingState();updateResolutionTooltip();startTimerHz(25);
    }
    ~GraphicalEQPanel() override {stopTimer();endDrag();dsp.analyzer.enabled.store(false);}
    void visibilityChanged() override {if(!isShowing()){discardPendingNumericInput();endDrag();}updateAnalyzer();}
    void parentHierarchyChanged() override {updateAnalyzer();}
    int selectedBand() const noexcept {return selected;}
    // Presentation only: reuse this editor inside the focused Dynamic EQ page.
    void setCompactWorkspace(bool compact) {if(compactWorkspace==compact)return;compactWorkspace=compact;endDrag();resized();}
    void selectBandForEditing(int band) {refreshEditingState();if(band>=0 && band<graphicalEQBands && isBandVisible(band))selectBand(band);}
    void setNavigationSelected(bool chosen) {if(navigationSelected!=chosen){navigationSelected=chosen;repaint();}}
    bool isNavigationSelected() const noexcept {return navigationSelected;}
    void focusForNavigation() {if(isShowing()){if(selected>=0)sliders[0].grabKeyboardFocus();else grabKeyboardFocus();}}
    std::function<void(int)> onHistoryBankSelected;
    bool isBandVisible(int band) const noexcept {return editing.isOccupied(instance,band);}
    // Shared dynamic slot (0-3) acting on this band, or -1.
    int dynamicSlotForBand(int band) const {
        for(int slot=0;slot<4;++slot)if(auto* target=state.getRawParameterValue(dynamicSlotID(slot,"target"));target && int(target->load())==instance*12+band+1)return slot;
        return -1;
    }
    bool isSnapshotFrozen() const noexcept {return spectrum.isFrozen();}
    bool isWaitingForFreshCapture() const noexcept {return pendingCaptureRequest!=0;}
    void setSnapshotFrozen(bool frozen) {
        if(frozen==spectrum.isFrozen())return;
        if(frozen){liveResolutionChoice=resolution.getSelectedId();liveSmoothingChoice=smoothing.getSelectedId();}
        dsp.analyzer.enabled.store(false);analyzerActive=false;
        // A callback that was already running can still publish after a FIFO
        // flush. Keep capture off until a later callback has observed this
        // request and completed, then flush again before accepting fresh input.
        pendingCaptureRequest=frozen?0:captureBoundary.requestDrain();
        spectrum.setFrozen(frozen,dsp.analyzer);
        freezeToggle.setEnabled(spectrum.isFrozen() || spectrum.canFreezeSnapshot());
        freezeToggle.setToggleState(spectrum.isFrozen(),juce::dontSendNotification);
        freezeToggle.setButtonText(spectrum.isFrozen()?"FROZEN":"FREEZE");
        resolution.setSelectedId(frozen?(spectrum.fftSize()==2048?1:spectrum.fftSize()==8192?3:2):liveResolutionChoice,juce::dontSendNotification);
        smoothing.setSelectedId(frozen?spectrum.smoothingMode()+1:liveSmoothingChoice,juce::dontSendNotification);
        resolution.setEnabled(!frozen);smoothing.setEnabled(!frozen);analyzerToggle.setEnabled(!frozen);
        rebuildSpectrumPaths();updateAnalyzer();updateResolutionTooltip();repaint();
    }
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
        if(compactWorkspace) {
            analyzerToggle.setBounds(198,4,54,26);inputToggle.setBounds(258,4,44,26);outputToggle.setBounds(308,4,44,26);
            freezeToggle.setBounds(360,4,96,26);resolution.setBounds(468,4,160,26);smoothing.setBounds(640,4,148,26);
            bypass.setBounds(getWidth()-166,4,148,26);
            addButton.setBounds(56,34,84,25);deleteButton.setBounds(148,34,82,25);undoButton.setBounds(244,34,76,25);redoButton.setBounds(328,34,76,25);
            const int y=getHeight()-33;layoutDynamicRow(y-37);
            selector.setBounds(16,y,106,28);enabled.setBounds(132,y,96,28);type.setBounds(240,y,174,28);
            sliders[0].setBounds(434,y-3,240,34);sliders[1].setBounds(694,y-3,198,34);sliders[2].setBounds(912,y-3,getWidth()-930,34);
            rebuildResponse();rebuildSpectrumPaths();repaint();return;
        }
        // Logical workspace scales with the existing 75-150% editor canvas.
        analyzerToggle.setBounds(286,22,60,28);inputToggle.setBounds(354,22,46,28);outputToggle.setBounds(408,22,48,28);
        freezeToggle.setBounds(465,22,109,28);
        resolution.setBounds(592,22,172,28);smoothing.setBounds(780,22,156,28);
        bypass.setBounds(getWidth()-174,22,156,28);
        addButton.setBounds(56,68,88,27);deleteButton.setBounds(154,68,88,27);
        undoButton.setBounds(264,68,76,27);redoButton.setBounds(350,68,76,27);
        const int y=getHeight()-45;layoutDynamicRow(y-38);
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
        if(!compactWorkspace) {
        g.setGradientFill({juce::Colour(0xff202d30),0.f,0.f,juce::Colour(0xff141e21),0.f,62.f,false});
        g.fillRect(0,0,getWidth(),62);
        g.setColour(navigationSelected?eq.withAlpha(.65f):grid);g.fillRect(0,0,getWidth(),2);
        g.setColour(text);g.setFont(juce::FontOptions(19.f,juce::Font::bold));
        g.drawText(instance==0?"TONE EQ":"FINAL EQ",18,9,252,26,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(juce::FontOptions(12.5f));
        g.drawText(instance==0?"Before POST":"After width / before output trim",18,36,252,17,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(12.f));
        g.drawText("ANALYZER",286,5,80,15,juce::Justification::centredLeft);
        g.drawText("RESOLUTION",592,5,172,15,juce::Justification::centredLeft);
        g.drawText("SMOOTHING",780,5,156,15,juce::Justification::centredLeft);
        g.setColour(in);g.drawLine(355.f,12.f,372.f,12.f,1.4f);g.drawLine(378.f,12.f,397.f,12.f,1.4f);
        g.setColour(out);g.drawLine(410.f,12.f,453.f,12.f,2.f);
        g.setColour(spectrum.isFrozen()?out:muted);g.setFont(juce::FontOptions(12.5f));
        const auto hint=spectrum.isFrozen()
            ? (spectrum.hasData()?"Snapshot  /  "+juce::String(spectrum.lastSampleRate()/1000.,1)+" kHz  /  "+juce::String(spectrum.fftSize()):juce::String("Snapshot: no complete frame"))
            : (occupiedCount()==graphicalEQBands?juce::String("12 bands in use  /  Delete a band to add another"):juce::String("Double-click to add  /  Delete to remove"));
        g.drawText(hint,446,72,getWidth()-502,20,juce::Justification::centredRight);
        } else {
            g.setColour(text);g.setFont(juce::FontOptions(16.f,juce::Font::bold));
            g.drawText(instance==0?"TONE EQ":"FINAL EQ",18,5,170,25,juce::Justification::centredLeft);
            g.setColour(muted);g.setFont(juce::FontOptions(11.5f));
            g.drawText("Dashed: static  /  shaded: allowed cut  /  solid: applied",420,35,getWidth()-476,22,juce::Justification::centredRight);
        }
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
        g.drawText("EQ dB",7,compactWorkspace?51:87,40,15,juce::Justification::centredRight);
        g.drawText("dBFS",int(area.getRight())+9,compactWorkspace?51:87,42,15,juce::Justification::centredLeft);
        for(int db:{0,-24,-48,-72,-96})g.drawText(juce::String(db),int(area.getRight())+9,int(spectrumY(float(db)))-8,40,16,juce::Justification::centredLeft);
        g.saveState();g.reduceClipRegion(area.toNearestInt());
        if((analyzerToggle.getToggleState() || spectrum.isFrozen()) && spectrum.hasData()) {
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
        // Static settings, configured dynamic range and applied coefficients
        // remain distinct from both measured audio spectra.
        if(hasDynamicRange) {
            g.setColour(juce::Colour(0xff9b82cb).withAlpha(.16f));g.fillPath(dynamicRangeFill);
            g.setColour(juce::Colour(0xffbca6e3).withAlpha(.55f));g.strokePath(rangeResponse,juce::PathStrokeType(1.f));
            const float dashes[]{5.f,4.f};juce::Path dashed;
            juce::PathStrokeType(1.2f).createDashedStroke(dashed,staticResponse,dashes,2);
            g.setColour(eq.withAlpha(.48f));g.fillPath(dashed);
        }
        // Separate the actual summed EQ response from both live spectra.
        g.setColour(juce::Colours::black.withAlpha(.42f));g.strokePath(response,juce::PathStrokeType(5.f));
        g.setColour(eq.withAlpha(dsp.isBypassed()?.38f:1.f));g.strokePath(response,juce::PathStrokeType(2.4f));
        g.restoreState();
        for(int b=0;b<graphicalEQBands;++b)if(b!=selected && isBandVisible(b))drawNode(g,b,false);
        if(selected>=0 && isBandVisible(selected))drawNode(g,selected,true);
        if(!spectrum.hasData() || (!analyzerToggle.getToggleState() && !spectrum.isFrozen())) {
            g.setColour(muted.withAlpha(.85f));g.setFont(juce::FontOptions(12.5f));
            g.drawText(spectrum.isFrozen()?"Snapshot: no complete frame":analyzerToggle.getToggleState()?"Waiting for fresh audio":"Analyzer off",int(area.getX())+10,int(area.getBottom())-24,260,18,juce::Justification::centredLeft);
        }
        g.setColour(grid);g.drawRect(area,1.f);g.drawHorizontalLine(getHeight()-(compactWorkspace?53:65),16.f,float(getWidth()-16));
        g.setColour(muted);g.setFont(juce::FontOptions(12.f));const int labelY=getHeight()-(compactWorkspace?49:62);
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
        refreshEditingState();grabKeyboardFocus();
        // Hit-test in the same front-to-back order as painting. The selected
        // node is drawn last; coincident nodes must not silently redirect its
        // drag/automation to the first persistent slot.
        float distance=18;int closest=-1;
        const auto consider=[&](int band) {
            if(band<0 || !isBandVisible(band))return;
            const float d=nodePosition(band).getDistanceFrom(event.position);
            if(d<distance){distance=d;closest=band;}
        };
        consider(selected);
        for(int b=graphicalEQBands-1;b>=0;--b)if(b!=selected)consider(b);
        if(closest<0)return;selector.setSelectedId(closest+1,juce::dontSendNotification);selectBand(closest);
        if(event.mods.isPopupMenu()) {
            juce::PopupMenu menu;menu.addItem(1,"Delete Band "+juce::String(closest+1));
            const auto revision=editing.revision();juce::Component::SafePointer<GraphicalEQPanel> safe(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,closest,revision](int result){
                if(safe && result==1 && safe->editing.revision()==revision && safe->selected==closest)safe->deleteSelectedBand();
            });return;
        }
        if(!event.mods.isLeftButtonDown())return;
        dragging=editing.beginEdit(instance,"Drag Band "+juce::String(selected+1));
        if(dragging){editing.touchField(instance,selected,"frequency");if(gainApplicable())editing.touchField(instance,selected,"gain");}
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        if(!dragging)return;write("frequency",xFrequency(event.position.x));if(gainApplicable())write("gain",yGain(event.position.y));rebuildResponse();repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {endDrag();}
    void mouseDoubleClick(const juce::MouseEvent& event) override {
        if(!event.mods.isLeftButtonDown() || !plot().contains(event.position))return;
        for(int b=0;b<graphicalEQBands;++b)if(isBandVisible(b) && nodePosition(b).getDistanceFrom(event.position)<18.f)return;
        createBand(xFrequency(event.position.x),yGain(event.position.y));
    }
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override {
        if(!plot().contains(event.position) || selected<0)return;
        discreteEdit(selected,"q",dsp.values(selected).q*std::exp(wheel.deltaY*1.5f),"Band Q");
    }
    bool keyPressed(const juce::KeyPress& key) override {
        // Let a numeric TextEditor keep its own text-editing shortcuts.
        if(dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))return false;
        if(key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) {
            if(key.getKeyCode()=='Z' || key.getKeyCode()=='z'){applyHistory(key.getModifiers().isShiftDown());return true;}
            if(key.getKeyCode()=='Y' || key.getKeyCode()=='y'){applyHistory(true);return true;}
        }
        if(key.getKeyCode()==juce::KeyPress::deleteKey || key.getKeyCode()==juce::KeyPress::backspaceKey){deleteSelectedBand();return true;}
        return false;
    }
private:
    using PA=juce::ParameterAttachment;
    static constexpr std::array<const char*,3> editFields{"frequency","gain","q"};
    juce::Rectangle<float> plot() const noexcept {return {56,compactWorkspace?66.f:105.f,float(std::max(1,getWidth()-112)),float(std::max(1,getHeight()-(compactWorkspace?177:233)))};}
    float frequencyX(float hz) const {return plot().getX()+plot().getWidth()*std::log(std::clamp(hz,20.f,20000.f)/20)/std::log(1000.f);}
    float xFrequency(float x) const {return 20*std::pow(1000.f,std::clamp((x-plot().getX())/std::max(1.f,plot().getWidth()),0.f,1.f));}
    float gainY(float db) const {return plot().getCentreY()-db/48*plot().getHeight();}
    float yGain(float y) const {return std::clamp((plot().getCentreY()-y)/std::max(1.f,plot().getHeight())*48,-24.f,24.f);}
    float spectrumY(float db) const {return plot().getBottom()-(std::clamp(db,-96.f,0.f)+96)/96*plot().getHeight();}
    void write(const char* field,float value){if(selected>=0)editing.setValue(instance,selected,field,value);}
    bool gainApplicable() const {return selected>=0 && isBandVisible(selected) && dsp.values(selected).type<=2;}
    void endDrag(){if(dragging || sliderEditing){editing.endEdit();dragging=sliderEditing=false;updateHistoryControls();}}
    void drawNode(juce::Graphics& g,int band,bool chosen) const {
        const auto v=dsp.values(band);const auto p=nodePosition(band);const float r=chosen?9.f:v.enabled?8.f:7.5f;
        const juce::Colour colour(chosen?0xfff0d39b:v.enabled?0xffb5d6c6:0xff718d8a);
        if(chosen){g.setColour(colour.withAlpha(.12f));g.fillEllipse(p.x-13,p.y-13,26,26);}
        g.setColour(juce::Colour(0xff101b1e));g.fillEllipse(p.x-r,p.y-r,2*r,2*r);
        g.setColour(colour);if(v.enabled)g.fillEllipse(p.x-r,p.y-r,2*r,2*r);else g.drawEllipse(p.x-r,p.y-r,2*r,2*r,chosen?2.f:1.f);
        g.setFont(juce::FontOptions(chosen?12.f:11.f,juce::Font::bold));g.setColour(v.enabled?juce::Colour(0xff172421):colour);
        g.drawText(juce::String(band+1),int(p.x)-10,int(p.y)-8,20,16,juce::Justification::centred);
        for(int slot=0;slot<4;++slot)if(auto* target=state.getRawParameterValue(dynamicSlotID(slot,"target"));target && int(target->load())==instance*12+band+1){
            g.setColour(juce::Colour(0xffd3b478));g.setFont(juce::FontOptions(11.f));
            g.drawText("D"+juce::String(slot+1)+"  -"+juce::String(dsp.dynamicReduction(band),1),juce::roundToInt(p.x)-34,juce::roundToInt(p.y)+13,80,17,juce::Justification::centred);
            break;
        }
    }
    void selectBand(int band) {
        cancelExternalInput();discardPendingNumericInput();
        endDrag();selected=(band>=0 && band<graphicalEQBands && isBandVisible(band))?band:-1;
        enabledAttachment.reset();typeAttachment.reset();for(auto& a:sliderAttachments)a.reset();
        selector.setSelectedId(selected+1,juce::dontSendNotification);
        if(selected<0) {
            enabled.setComponentID({});type.setComponentID({});enabled.setToggleState(false,juce::dontSendNotification);
            for(auto& slider:sliders)slider.setComponentID({});bindDynamic();updateBandEnablement();repaint();return;
        }
        enabled.setComponentID(eqID(instance,selected,"enabled"));type.setComponentID(eqID(instance,selected,"type"));
        enabledAttachment=std::make_unique<PA>(*state.getParameter(eqID(instance,selected,"enabled")),[this](float v){enabled.setToggleState(v>.5f,juce::dontSendNotification);});
        typeAttachment=std::make_unique<PA>(*state.getParameter(eqID(instance,selected,"type")),[this](float v){type.setSelectedId(juce::roundToInt(v)+1,juce::dontSendNotification);updateBandEnablement();});
        enabledAttachment->sendInitialUpdate();typeAttachment->sendInitialUpdate();
        const juce::ScopedValueSetter<bool> refreshing(refreshingControls,true);
        for(size_t i=0;i<3;++i){
            sliders[i].setComponentID(eqID(instance,selected,editFields[i]));
            auto& parameter=*state.getParameter(eqID(instance,selected,editFields[i]));
            const auto range=parameter.getNormalisableRange();
            juce::NormalisableRange<double> sliderRange(range.start,range.end,
                [range](double,double,double value){return double(range.convertFrom0to1(float(value)));},
                [range](double,double,double value){return double(range.convertTo0to1(float(value)));},
                [range](double,double,double value){return double(range.snapToLegalValue(float(value)));});
            sliderRange.interval=range.interval;sliderRange.skew=range.skew;sliderRange.symmetricSkew=range.symmetricSkew;
            sliders[i].setNormalisableRange(sliderRange);
            sliders[i].setDoubleClickReturnValue(true,parameter.convertFrom0to1(parameter.getDefaultValue()));
            sliderAttachments[i]=std::make_unique<PA>(parameter,[this,i](float v){sliders[i].setValue(v,juce::dontSendNotification);});
            sliderAttachments[i]->sendInitialUpdate();
            sliders[i].textFromValueFunction=[i](double v){return juce::String(v,i==0?1:i==1?2:3)+(i==0?" Hz":i==1?" dB":" Q");};
            sliders[i].valueFromTextFunction=[](const juce::String& t){return t.getDoubleValue();};sliders[i].updateText();
        }
        bindDynamic();updateBandEnablement();repaint();
    }
    int occupiedCount() const noexcept {
        int count=0;for(int b=0;b<graphicalEQBands;++b)if(isBandVisible(b))++count;return count;
    }
    void updateBandEnablement() {
        const bool chosen=selected>=0 && isBandVisible(selected);
        selector.setEnabled(occupiedCount()>0);enabled.setEnabled(chosen);type.setEnabled(chosen);
        sliders[0].setEnabled(chosen);sliders[1].setEnabled(chosen && gainApplicable());sliders[2].setEnabled(chosen);
        deleteButton.setEnabled(chosen);addButton.setEnabled(occupiedCount()<graphicalEQBands);
        updateDynamicControls();
    }
    void layoutDynamicRow(int y) {
        dynamicToggle.setBounds(16,y,106,28);dynamicStatus.setBounds(130,y,104,28);
        const int x=240,width=std::max(5*60,getWidth()-x-18)/5;
        for(size_t i=0;i<dynamicSliders.size();++i)dynamicSliders[i].setBounds(x+int(i)*width,y-3,width-8,34);
    }
    void bindDynamic() {
        const int slot=selected>=0?dynamicSlotForBand(selected):-1;
        if(slot!=attachedDynamicSlot) {
            attachedDynamicSlot=slot;
            for(size_t i=0;i<dynamicSliders.size();++i) {
                dynamicAttachments[i].reset();
                // Show the fields the next claimed slot would use until a slot is attached.
                const int shown=slot>=0?slot:std::max(0,nextFreeDynamicSlot());
                auto* parameter=state.getParameter(dynamicSlotID(shown,dynamicFields[i]));if(!parameter)continue;
                dynamicAttachments[i]=std::make_unique<juce::SliderParameterAttachment>(*parameter,dynamicSliders[i]);
                // The attachment installs the host text; keep a short labelled readout.
                dynamicSliders[i].textFromValueFunction=[i](double v){return juce::String(dynamicFieldShort[i])+" "+juce::String(v,i==4?0:1)+dynamicFieldUnits[i];};
                dynamicSliders[i].valueFromTextFunction=[](const juce::String& t){return t.retainCharacters("-.0123456789").getDoubleValue();};
                dynamicSliders[i].updateText();
            }
        }
        updateDynamicControls();
    }
    int nextFreeDynamicSlot() const {
        for(int slot=0;slot<4;++slot)if(auto* target=state.getRawParameterValue(dynamicSlotID(slot,"target"));target && int(target->load())==0)return slot;
        return -1;
    }
    bool dynamicApplicable() const {return selected>=0 && isBandVisible(selected) && dsp.values(selected).type<=2;}
    void setSelectedBandDynamic(bool on) {
        if(selected<0)return;
        const int current=dynamicSlotForBand(selected);
        const auto assign=[this](int slot,int value){
            if(auto* parameter=state.getParameter(dynamicSlotID(slot,"target"))){parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));parameter->endChangeGesture();}
        };
        if(on && current<0) {
            const int slot=nextFreeDynamicSlot();
            if(slot<0 || !dynamicApplicable()){dynamicNotice=slot<0?"4 of 4 in use":"Bell / shelf only";updateDynamicControls();return;}
            assign(slot,instance*12+selected+1);
        } else if(!on)for(int slot=0;slot<4;++slot)if(dynamicSlotForBand(selected)==slot)assign(slot,0);
        dynamicNotice.clear();bindDynamic();rebuildResponse();repaint();
    }
    void updateDynamicControls() {
        const int slot=selected>=0?dynamicSlotForBand(selected):-1;
        if(slot!=attachedDynamicSlot){bindDynamic();return;}
        const bool active=slot>=0,possible=dynamicApplicable();
        dynamicToggle.setToggleState(active,juce::dontSendNotification);
        dynamicToggle.setEnabled(selected>=0 && (active || (possible && nextFreeDynamicSlot()>=0)));
        for(auto& slider:dynamicSliders)slider.setEnabled(active);
        juce::String status;
        if(dynamicNotice.isNotEmpty())status=dynamicNotice;
        else if(active)status=(possible && dsp.values(selected).enabled && !dsp.isBypassed())?"GR -"+juce::String(dsp.dynamicReduction(selected),1)+" dB":"Idle";
        else if(selected>=0 && !possible)status="Bell / shelf only";
        else if(selected>=0 && nextFreeDynamicSlot()<0)status="4 of 4 in use";
        else if(selected>=0)status=juce::String(4-usedDynamicSlots())+" of 4 free";
        if(dynamicStatus.getText()!=status)dynamicStatus.setText(status,juce::dontSendNotification);
    }
    int usedDynamicSlots() const {int used=0;for(int slot=0;slot<4;++slot)if(auto* t=state.getRawParameterValue(dynamicSlotID(slot,"target"));t && int(t->load())!=0)++used;return used;}
    void updateHistoryControls() {
        const int undoBank=editing.nextUndoBank(),redoBank=editing.nextRedoBank();
        undoButton.setEnabled(undoBank>=0);redoButton.setEnabled(redoBank>=0);
        undoButton.setTooltip(undoBank>=0?"Undo "+juce::String(undoBank==0?"Tone EQ: ":"Final EQ: ")+editing.undoDescription():"No EQ edit to undo");
        redoButton.setTooltip(redoBank>=0?"Redo "+juce::String(redoBank==0?"Tone EQ: ":"Final EQ: ")+editing.redoDescription():"No EQ edit to redo");
    }
    void refreshEditingState() {
        cancelExternalInput();
        const auto mask=editing.occupiedMask(instance);
        if(mask!=lastOccupiedMask) {
            lastOccupiedMask=mask;selector.clear(juce::dontSendNotification);
            for(int b=0;b<graphicalEQBands;++b)if(isBandVisible(b))selector.addItem("Band "+juce::String(b+1),b+1);
            int next=selected;
            if(next<0 || !isBandVisible(next)){next=-1;for(int b=0;b<graphicalEQBands;++b)if(isBandVisible(b)){next=b;break;}}
            selectBand(next);repaint();
        }
        updateBandEnablement();updateHistoryControls();
    }
    void discreteEdit(int band,const char* field,float value,const juce::String& title) {
        if(cancelExternalInput()){syncDisplayedValues();return;}
        endDrag();if(!editing.beginEdit(instance,title)){syncDisplayedValues();return;}
        editing.setValue(instance,band,field,value);editing.endEdit();syncDisplayedValues();rebuildResponse();repaint();
    }
    void syncDisplayedValues() {
        refreshEditingState();
        if(bypassAttachment)bypassAttachment->sendInitialUpdate();
        if(enabledAttachment)enabledAttachment->sendInitialUpdate();
        if(typeAttachment)typeAttachment->sendInitialUpdate();
        for(auto& attachment:sliderAttachments)if(attachment)attachment->sendInitialUpdate();
    }
    void discardPendingNumericInput() {
        for(auto& slider:sliders)slider.hideTextBox(true);
    }
    bool cancelExternalInput() {
        const auto revision=editing.externalRevision();if(revision==lastExternalRevision)return false;
        lastExternalRevision=revision;discardPendingNumericInput();
        if(sliderEditing)sliderGestureRejected=true;
        endDrag();return true;
    }
    void createBand(float hz,float gain) {
        endDrag();const int added=editing.addBand(instance,hz,gain);refreshEditingState();
        if(added>=0){selectBand(added);grabKeyboardFocus();}rebuildResponse();repaint();
    }
    void deleteSelectedBand() {
        endDrag();if(selected<0)return;
        editing.removeBand(instance,selected);refreshEditingState();rebuildResponse();repaint();grabKeyboardFocus();
    }
    void applyHistory(bool redo) {
        endDrag();const int bank=redo?editing.nextRedoBank():editing.nextUndoBank();
        const bool changed=redo?editing.redo():editing.undo();refreshEditingState();rebuildResponse();repaint();
        if(changed && bank>=0 && onHistoryBankSelected)onHistoryBankSelected(bank);
    }
    void updateAnalyzer() {
        if(!isShowing())endDrag();
        if(pendingCaptureRequest!=0) {
            dsp.analyzer.enabled.store(false);analyzerActive=false;
            if(!captureBoundary.isDrained(pendingCaptureRequest))return;
            pendingCaptureRequest=0;spectrum.reset(dsp.analyzer);clearSpectrumPaths();
        }
        const bool active=isShowing() && analyzerToggle.getToggleState() && !spectrum.isFrozen();if(active==analyzerActive)return;
        dsp.analyzer.enabled.store(false);
        if(!spectrum.isFrozen()){spectrum.reset(dsp.analyzer);clearSpectrumPaths();}
        analyzerActive=active;dsp.analyzer.enabled.store(active);repaint();
    }
    void updateResolutionTooltip() {
        const double rate=spectrum.isFrozen() && spectrum.hasData()?spectrum.lastSampleRate():dsp.sampleRate(),size=spectrum.fftSize();
        resolution.setTooltip(juce::String(int(size))+" samples / "+juce::String(rate/size,2)+" Hz bin spacing / "+juce::String(1000*size/rate,1)+" ms analysis window at "+juce::String(rate/1000,1)+" kHz. Higher resolution responds more slowly; audio latency is unchanged.");
    }
    void rebuildResponse() {
        response.clear();staticResponse.clear();rangeResponse.clear();dynamicRangeFill.clear();hasDynamicRange=false;
        const auto area=plot();if(area.getWidth()<=0 || area.getHeight()<=0)return;
        std::array<EQCoefficients,graphicalEQBands> coeffs{},base{},maximum{};std::array<bool,graphicalEQBands> active{};
        for(int b=0;b<graphicalEQBands;++b) {
            const auto v=dsp.values(b);active[size_t(b)]=v.enabled;float range=0;
            for(int slot=0;slot<4;++slot)if(auto* target=state.getRawParameterValue(dynamicSlotID(slot,"target"));target && int(target->load())==instance*12+b+1) {
                if(v.enabled && v.type<=2)if(auto* p=state.getRawParameterValue(dynamicSlotID(slot,"range")))range=DynamicEQBindings::read(p,0,24,6);
                break;
            }
            hasDynamicRange|=range>0;
            coeffs[size_t(b)]=eqCoefficients(v.type,v.frequency,v.gain-dsp.dynamicReduction(b),v.q,dsp.sampleRate());
            base[size_t(b)]=eqCoefficients(v.type,v.frequency,v.gain,v.q,dsp.sampleRate());
            maximum[size_t(b)]=eqCoefficients(v.type,v.frequency,v.gain-range,v.q,dsp.sampleRate());
        }
        // One frequency phasor per graph pixel, shared by all three curves.
        // Coefficients/paths are message-thread-only and rebuilt on changes.
        std::vector<juce::Point<float>> lowEdge;lowEdge.reserve(size_t(area.getWidth())+1);
        for(int x=0;x<=int(area.getWidth());++x) {
            const float px=area.getX()+float(x),f=xFrequency(px);if(f>dsp.sampleRate()*.5)break;
            const auto z=std::polar(1.,-2*juce::MathConstants<double>::pi*f/dsp.sampleRate()),z2=z*z;
            const auto magnitude=[&](const EQCoefficients& c){return std::abs((c.b0+c.b1*z+c.b2*z2)/(1.+c.a1*z+c.a2*z2));};
            double actual=1,configured=1,minimum=1;
            for(int b=0;b<graphicalEQBands;++b)if(active[size_t(b)]) {
                actual*=magnitude(coeffs[size_t(b)]);configured*=magnitude(base[size_t(b)]);minimum*=magnitude(maximum[size_t(b)]);
            }
            const auto y=[&](double v){return gainY(float(20*std::log10(std::max(1.e-12,v))));};
            const float appliedY=dsp.isBypassed()?gainY(0):y(actual),staticY=y(configured),rangeY=y(minimum);
            if(x==0){response.startNewSubPath(px,appliedY);staticResponse.startNewSubPath(px,staticY);rangeResponse.startNewSubPath(px,rangeY);dynamicRangeFill.startNewSubPath(px,staticY);}
            else {response.lineTo(px,appliedY);staticResponse.lineTo(px,staticY);rangeResponse.lineTo(px,rangeY);dynamicRangeFill.lineTo(px,staticY);}
            lowEdge.emplace_back(px,rangeY);
        }
        for(auto it=lowEdge.rbegin();it!=lowEdge.rend();++it)dynamicRangeFill.lineTo(*it);
        dynamicRangeFill.closeSubPath();
    }
    void clearSpectrumPaths(){for(auto& p:spectrumPaths)p.clear();for(auto& p:spectrumFills)p.clear();dashedInput.clear();}
    void rebuildSpectrumPaths() {
        const auto start=std::chrono::steady_clock::now();clearSpectrumPaths();if(!spectrum.hasData())return;
        const auto area=plot();const int width=int(area.getWidth());
        for(int stream=0;stream<2;++stream){
            auto& path=spectrumPaths[(size_t)stream];float lastX=area.getX();
            for(int x=0;x<=width;++x){
                const float px=area.getX()+float(x),centre=xFrequency(px);if(centre>spectrum.lastSampleRate()*.5)break;
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
        updateAnalyzer();refreshEditingState();if(!isShowing())return;bool changed=false;
        freezeToggle.setEnabled(spectrum.isFrozen() || spectrum.canFreezeSnapshot());
        if(diagnosticObserver)pathMicros=0.; // A no-path tick must not reuse a previous measurement.
        if(analyzerActive && spectrum.update(dsp.analyzer,dsp.sampleRate())){rebuildSpectrumPaths();changed=true;}
        // CPU meter fluctuations must not cause a transfer-curve rebuild.
        std::array<float,62> key{};int n=0;key[(size_t)n++]=dsp.isBypassed()?1.f:0.f;
        for(int b=0;b<graphicalEQBands;++b){const auto v=dsp.values(b);for(float f:{v.enabled?1.f:0.f,v.frequency,v.gain,v.q,float(v.type)})key[(size_t)n++]=f;}
        key[61]=float(dsp.sampleRate());
        if(key!=lastKey){const bool rateChanged=key[61]!=lastKey[61];lastKey=key;rebuildResponse();updateBandEnablement();if(rateChanged)updateResolutionTooltip();changed=true;}
        std::array<float,12> reductions{};for(int b=0;b<12;++b)reductions[size_t(b)]=std::round(dsp.dynamicReduction(b)*20.f)/20.f;
        std::array<int,4> targets{};for(int slot=0;slot<4;++slot)if(auto* p=state.getRawParameterValue(dynamicSlotID(slot,"target")))targets[size_t(slot)]=int(p->load());
        std::array<float,4> ranges{};for(int slot=0;slot<4;++slot)if(auto* p=state.getRawParameterValue(dynamicSlotID(slot,"range")))ranges[size_t(slot)]=p->load();
        if(targets!=lastDynamicTargets)dynamicNotice.clear();
        if(reductions!=lastReductions || targets!=lastDynamicTargets || ranges!=lastDynamicRanges){lastReductions=reductions;lastDynamicTargets=targets;lastDynamicRanges=ranges;rebuildResponse();changed=true;}
        updateDynamicControls();
        if(changed && diagnosticRenderPanel)repaint(); // Only this workspace, never the whole editor.
        if(diagnosticObserver)diagnosticObserver(diagnosticContext,*this,false,
            std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-diagnosticStart).count());
    }
    juce::AudioProcessorValueTreeState& state;GraphicalEQ& dsp;EQEditState& editing;EQCaptureBoundary& captureBoundary;int instance,selected{-1};
    bool compactWorkspace{},hasDynamicRange{};
    bool dragging{},sliderEditing{},sliderGestureRejected{},refreshingControls{},navigationSelected{},analyzerActive{};
    uint16_t lastOccupiedMask{0xffff};int liveResolutionChoice{2},liveSmoothingChoice{2};
    uint64_t lastExternalRevision{},pendingCaptureRequest{};
    juce::TextButton bypass,enabled,analyzerToggle,inputToggle,outputToggle,freezeToggle,addButton,deleteButton,undoButton,redoButton;
    juce::ComboBox selector,type,resolution,smoothing;std::array<juce::Slider,3> sliders;
    std::unique_ptr<PA> bypassAttachment,enabledAttachment,typeAttachment;std::array<std::unique_ptr<PA>,3> sliderAttachments;
    juce::Path response,staticResponse,rangeResponse,dynamicRangeFill,dashedInput;std::array<juce::Path,2> spectrumPaths,spectrumFills;
    std::array<float,62> lastKey{};EQSpectrumAnalyzer spectrum;
    std::array<float,12> lastReductions{};std::array<int,4> lastDynamicTargets{};std::array<float,4> lastDynamicRanges{};
    static constexpr std::array<const char*,5> dynamicFields{"threshold","range","ratio","attack","release"};
    static constexpr std::array<const char*,5> dynamicFieldNames{"threshold dB","maximum cut dB","ratio","attack ms","release ms"};
    static constexpr std::array<const char*,5> dynamicFieldShort{"THR","CUT","RATIO","ATK","REL"};
    static constexpr std::array<const char*,5> dynamicFieldUnits{" dB"," dB",":1"," ms"," ms"};
    juce::TextButton dynamicToggle;juce::Label dynamicStatus;std::array<juce::Slider,5> dynamicSliders;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>,5> dynamicAttachments;int attachedDynamicSlot{-2};juce::String dynamicNotice;
    double pathMicros{},paintMicros{};
    DiagnosticObserver diagnosticObserver{};void* diagnosticContext{};bool diagnosticRenderPanel{true};
};
}

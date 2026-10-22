#pragma once
#include "DynamicsAnalysis.h"
#include "EQCaptureBoundary.h"
#include "EQSpectrumAnalyzer.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>

namespace spectralforge {

// One message-thread consumer for the selected dynamics stage. IN and OUT are
// measured audio, not a synthetic EQ response. Frequency markers identify real
// detector filters/crossovers; broadband gain reduction is metered separately.
class DynamicsSpectrumPanel final : public juce::Component, private juce::Timer {
public:
    struct FrequencyMarker {float frequency{};juce::String label;};

    DynamicsSpectrumPanel(DynamicsAnalysis& source,EQCaptureBoundary& boundary)
        :analysis(source),captureBoundary(boundary) {
        setComponentID("commonDynamics.spectrum");setOpaque(true);
        displayButton(live,"LIVE","live",true);
        displayButton(input,"IN","input",true);
        displayButton(output,"OUT","output",true);
        displayButton(freeze,"FREEZE","freeze",false);freeze.setEnabled(false);
        live.setTooltip("Display the selected processor's actual input and output spectra. This control does not change audio.");
        input.setTooltip("Input of the selected dynamics stage; dashed blue line, stereo power average.");
        output.setTooltip("Output of the same dynamics stage; solid gold line, stereo power average.");
        freeze.setTooltip("Hold the last complete measured IN/OUT frame. Audio and dynamics controls continue normally.");
        live.onClick=[this]{restartCapture();};
        input.onClick=[this]{repaint();};output.onClick=[this]{repaint();};
        freeze.onClick=[this]{setSnapshotFrozen(freeze.getToggleState());};
        resolution.setComponentID("commonDynamics.spectrum.resolution");
        resolution.addItem("Fast / 2048",1);resolution.addItem("Normal / 4096",2);resolution.addItem("Detail / 8192",3);
        resolution.setSelectedId(2,juce::dontSendNotification);addAndMakeVisible(resolution);
        resolution.onChange=[this]{
            if(spectrum.isFrozen())return;
            if(analysis.uiConsumer==this)analysis.disable();captureActive=false;
            spectrum.setFFTSize(1 << (resolution.getSelectedId()+10));restartCapture();
        };
        smoothing.setComponentID("commonDynamics.spectrum.smoothing");
        smoothing.addItem("Fast",1);smoothing.addItem("Balanced",2);smoothing.addItem("Smooth",3);
        smoothing.setSelectedId(2,juce::dontSendNotification);addAndMakeVisible(smoothing);
        smoothing.setTooltip("Display ballistics only. Changing smoothing does not change dynamics or latency.");
        smoothing.onChange=[this]{if(!spectrum.isFrozen())spectrum.setSmoothing(smoothing.getSelectedId()-1);};
        startTimerHz(25);
    }
    ~DynamicsSpectrumPanel() override {stopTimer();endMarkerDrag();releaseCapture();}

    void setStage(DynamicsStage next) {
        if(next==stage)return;
        endMarkerDrag();stage=next;
        // A frozen compressor frame must never be relabelled as a gate frame.
        sourceInvalidated=true;
        freeze.setToggleState(false,juce::dontSendNotification);freeze.setButtonText("FREEZE");
        resolution.setEnabled(true);smoothing.setEnabled(true);live.setEnabled(true);
        markerCount=0;highlightLow=highlightHigh=0;restartCapture();
    }
    DynamicsStage selectedStage() const noexcept {return stage;}
    // Source identity belongs to this view until it acquires the shared lease.
    // A hidden native editor must not overwrite another visible editor's tap.
    void setPreOwner(int owner) {
        if(requestedPreOwner==owner)return;requestedPreOwner=owner;
        if(stage==DynamicsStage::preCompressor)sourceChanged();
    }
    void setPostNativeSource(int source) {
        source=std::clamp(source,-1,1);if(requestedPostSource==source)return;requestedPostSource=source;
        if(stage==DynamicsStage::postCompressor)sourceChanged();
    }
    int preOwner() const noexcept {return requestedPreOwner;}
    int postNativeSource() const noexcept {return requestedPostSource;}
    bool isSnapshotFrozen() const noexcept {return !sourceInvalidated && spectrum.isFrozen();}
    bool isWaitingForFreshCapture() const noexcept {return pendingDrain!=0;}
    bool analyzerHasData() const noexcept {return !sourceInvalidated && spectrum.hasData();}
    uint64_t fftFrames() const noexcept {return spectrum.frames();}
    double analysisSampleRate() const noexcept {return sourceInvalidated?0.:spectrum.lastSampleRate();}
    float spectrumDB(bool isOutput,int bin) const noexcept {return sourceInvalidated?EQSpectrumAnalyzer::floorDB:spectrum.rawDB(isOutput,bin);}
    juce::Rectangle<float> graphBounds() const noexcept {return plot();}
    // Call after changing a native pedal owner or another source identity even
    // when the stage enum stays the same. A frozen old-owner frame is invalid.
    void sourceChanged() {
        endMarkerDrag();sourceInvalidated=true;
        freeze.setToggleState(false,juce::dontSendNotification);freeze.setButtonText("FREEZE");
        resolution.setEnabled(true);smoothing.setEnabled(true);live.setEnabled(true);
        // Source observers may run while this visible panel is behind another
        // dynamics dialog. Retire this view's frame without stealing its lease.
        if(analysis.uiConsumer==this || analysis.uiConsumer==nullptr)restartCapture();else repaint();
    }

    void setSnapshotFrozen(bool frozen) {
        if(frozen && sourceInvalidated)return;
        if(frozen==spectrum.isFrozen())return;
        acquireCapture();if(analysis.uiConsumer!=this)return;analysis.disable();captureActive=false;
        pendingDrain=captureBoundary.requestDrain();
        spectrum.setFrozen(frozen,analysis.fifo);
        freeze.setToggleState(frozen,juce::dontSendNotification);freeze.setButtonText(frozen?"FROZEN":"FREEZE");
        resolution.setEnabled(!frozen);smoothing.setEnabled(!frozen);live.setEnabled(!frozen);
        freeze.setEnabled(frozen || spectrum.canFreezeSnapshot());
        rebuildPaths();updateCapture();repaint();
    }
    void setFrequencyMarkers(const std::array<FrequencyMarker,3>& next,int count) {
        count=std::clamp(count,0,3);bool changed=count!=markerCount;
        for(int i=0;i<count;++i)changed|=markers[size_t(i)].frequency!=next[size_t(i)].frequency || markers[size_t(i)].label!=next[size_t(i)].label;
        markers=next;markerCount=count;if(changed)repaint();
    }
    void setBandHighlight(float low,float high) {
        if(low==highlightLow && high==highlightHigh)return;
        highlightLow=low;highlightHigh=high;repaint();
    }
    std::function<void(int)> onMarkerDragStart;
    std::function<void(int,float)> onMarkerDrag;
    std::function<void(int)> onMarkerDragEnd;

    void visibilityChanged() override {endMarkerDrag();if(isShowing())restartCapture();else releaseCapture();}
    void parentHierarchyChanged() override {if(isShowing())restartCapture();else releaseCapture();}
    void resized() override {
        const int right=getWidth()-12;
        smoothing.setBounds(right-116,5,116,27);resolution.setBounds(right-263,5,139,27);
        freeze.setBounds(right-354,5,83,27);output.setBounds(right-406,5,44,27);
        input.setBounds(right-454,5,40,27);live.setBounds(right-514,5,52,27);
        rebuildPaths();repaint();
    }
    void paint(juce::Graphics& g) override {
        const juce::Colour muted(0xff9aadaa),grid(0xff283638),in(0xff7ab6cf),out(0xffd3b478);
        g.fillAll(juce::Colour(0xff121a1d));g.setFont(juce::FontOptions(12.f));g.setColour(muted);
        g.drawText("SPECTRUM / dBFS",12,8,std::max(20,getWidth()-550),20,juce::Justification::centredLeft);
        const auto area=plot();
        g.setGradientFill({juce::Colour(0xff0b1216),area.getTopLeft(),juce::Colour(0xff142125),area.getBottomLeft(),false});g.fillRect(area);
        if(highlightHigh>highlightLow && highlightHigh>20.f) {
            const auto left=frequencyX(std::max(20.f,highlightLow)),right=frequencyX(std::min(20000.f,highlightHigh));
            g.setColour(juce::Colour(0xff81b8a0).withAlpha(.085f));g.fillRect(left,area.getY(),std::max(0.f,right-left),area.getHeight());
        }
        for(float hz:{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f}) {
            const auto x=frequencyX(hz);g.setColour(grid);g.drawVerticalLine(juce::roundToInt(x),area.getY(),area.getBottom());
            g.setColour(muted);g.drawText(hz<1000.f?juce::String(int(hz)):juce::String(hz/1000.f,0)+"k",int(x)-20,int(area.getBottom())+5,40,16,juce::Justification::centred);
        }
        for(int db:{0,-24,-48,-72,-96}) {
            const auto y=levelY(float(db));g.setColour(grid);g.drawHorizontalLine(juce::roundToInt(y),area.getX(),area.getRight());
            g.setColour(muted);g.drawText(juce::String(db),4,int(y)-8,40,16,juce::Justification::centredRight);
        }
        g.saveState();g.reduceClipRegion(area.toNearestInt());
        if(!sourceInvalidated && (live.getToggleState() || spectrum.isFrozen()) && spectrum.hasData()) {
            for(int stream=0;stream<2;++stream) {
                if(!(stream==0?input:output).getToggleState())continue;
                const auto colour=stream==0?in:out;
                juce::ColourGradient fill(colour.withAlpha(.22f),area.getTopLeft(),colour.withAlpha(.012f),area.getBottomLeft(),false);
                g.setGradientFill(fill);g.fillPath(fills[size_t(stream)]);g.setColour(colour.withAlpha(.9f));
                if(stream==0)g.fillPath(dashedInput);else g.strokePath(paths[1],juce::PathStrokeType(1.8f));
            }
        }
        for(int i=0;i<markerCount;++i) {
            const auto x=frequencyX(markers[size_t(i)].frequency);g.setColour(juce::Colour(0xffa3d2bb));
            const float dashes[]{3.f,4.f};g.drawDashedLine({x,area.getY(),x,area.getBottom()},dashes,2,1.2f);
        }
        g.restoreState();
        for(int i=0;i<markerCount;++i) {
            const auto x=frequencyX(markers[size_t(i)].frequency);g.setColour(juce::Colour(0xffa3d2bb));
            g.fillRoundedRectangle(x-5.f,area.getY()-3.f,10.f,9.f,2.f);g.setFont(juce::FontOptions(11.f));
            g.drawText(markers[size_t(i)].label,int(x)-42,int(area.getY())+8,84,18,juce::Justification::centred);
        }
        g.setColour(grid);g.drawRect(area,1.f);
        if(sourceInvalidated || !spectrum.hasData() || (!live.getToggleState() && !spectrum.isFrozen())) {
            g.setColour(muted);g.setFont(juce::FontOptions(12.f));
            const auto text=sourceInvalidated?"Waiting for fresh stage audio":spectrum.isFrozen()?"Snapshot: no complete frame":!live.getToggleState()?"Analyzer off":pendingDrain!=0?"Waiting for fresh stage audio":"Waiting for audio";
            g.drawText(text,int(area.getX())+12,int(area.getBottom())-26,int(area.getWidth())-24,18,juce::Justification::centredLeft);
        }
        g.setColour(muted);g.setFont(juce::FontOptions(11.f));
        const auto caption=isSnapshotFrozen()?"Measured snapshot / "+juce::String(spectrum.lastSampleRate()/1000.,1)+" kHz":analysis.uiConsumer!=this?juce::String("Analyzer paused by another dynamics view"):juce::String("Measured IN / OUT of this stage");
        g.drawText(caption,52,getHeight()-18,getWidth()-64,16,juce::Justification::centredRight);
    }
    void mouseDown(const juce::MouseEvent& event) override {
        if(!event.mods.isLeftButtonDown() || !plot().expanded(0.f,6.f).contains(event.position))return;
        float nearest=12.f;int selected=-1;
        for(int i=0;i<markerCount;++i) {
            const float distance=std::abs(event.position.x-frequencyX(markers[size_t(i)].frequency));
            if(distance<nearest){nearest=distance;selected=i;}
        }
        if(selected<0 || !onMarkerDrag)return;endMarkerDrag();draggedMarker=selected;
        if(onMarkerDragStart)onMarkerDragStart(selected);
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        if(draggedMarker>=0 && onMarkerDrag)onMarkerDrag(draggedMarker,xFrequency(event.position.x));
    }
    void mouseUp(const juce::MouseEvent&) override {endMarkerDrag();}

private:
    void displayButton(juce::TextButton& button,const char* label,const char* id,bool initiallyOn) {
        button.setButtonText(label);button.setComponentID("commonDynamics.spectrum."+juce::String(id));
        button.setClickingTogglesState(true);button.setToggleState(initiallyOn,juce::dontSendNotification);addAndMakeVisible(button);
    }
    juce::Rectangle<float> plot() const noexcept {return {52.f,49.f,float(std::max(1,getWidth()-76)),float(std::max(1,getHeight()-91))};}
    float frequencyX(float hz) const {return plot().getX()+plot().getWidth()*std::log(std::clamp(hz,20.f,20000.f)/20.f)/std::log(1000.f);}
    float xFrequency(float x) const {return 20.f*std::pow(1000.f,std::clamp((x-plot().getX())/plot().getWidth(),0.f,1.f));}
    float levelY(float db) const {return plot().getBottom()-(std::clamp(db,-96.f,6.f)+96.f)/102.f*plot().getHeight();}
    void endMarkerDrag() {if(draggedMarker>=0){const int marker=draggedMarker;draggedMarker=-1;if(onMarkerDragEnd)onMarkerDragEnd(marker);}}
    void clearPaths() {for(auto& path:paths)path.clear();for(auto& fill:fills)fill.clear();dashedInput.clear();}
    void acquireCapture() {
        if(!isShowing() || stage==DynamicsStage::none || analysis.uiConsumer==this)return;
        previousConsumer=static_cast<DynamicsSpectrumPanel*>(analysis.uiConsumer);
        analysis.disable();analysis.uiConsumer=this;captureActive=false;
    }
    void releaseCapture() {
        captureActive=false;pendingDrain=0;
        if(analysis.uiConsumer!=this)return;
        analysis.disable();analysis.uiConsumer=nullptr;
        if(!spectrum.isFrozen()){spectrum.reset(analysis.fifo);clearPaths();}
        auto previous=previousConsumer;previousConsumer=nullptr;
        if(previous && previous->isShowing())previous->restartCapture();
    }
    void restartCapture() {
        acquireCapture();captureActive=false;
        if(analysis.uiConsumer!=this){repaint();return;}
        analysis.disable();
        if(stage==DynamicsStage::preCompressor)analysis.preOwner.store(requestedPreOwner,std::memory_order_release);
        if(stage==DynamicsStage::postCompressor)analysis.postNativeSource.store(requestedPostSource,std::memory_order_release);
        pendingDrain=captureBoundary.requestDrain();
        if(sourceInvalidated) {
            // reset() intentionally retains the last completed freeze frame.
            // A new processor/owner must retire that frame as well. Thaw is
            // the analyzer's explicit completed-snapshot invalidation path.
            if(!spectrum.isFrozen())spectrum.setFrozen(true,analysis.fifo);
            spectrum.setFrozen(false,analysis.fifo);
        }
        sourceInvalidated=false;
        spectrum.reset(analysis.fifo);if(!spectrum.isFrozen())clearPaths();
        freeze.setEnabled(spectrum.isFrozen() || spectrum.canFreezeSnapshot());updateCapture();repaint();
    }
    void updateCapture() {
        if(!isShowing()){releaseCapture();return;}
        // JUCE does not notify a child when only an ancestor's visibility
        // changes. The spectrum may already be locally visible inside a hidden
        // workspace, so opening that workspace must acquire the idle stream
        // here. Never steal the lease from another visible dynamics dialog.
        // restartCapture keeps the normal callback acknowledgement/flush gate.
        if(analysis.uiConsumer==nullptr && stage!=DynamicsStage::none){restartCapture();return;}
        if(analysis.uiConsumer!=this){captureActive=false;return;}
        if(pendingDrain!=0) {
            analysis.disable();captureActive=false;
            if(!captureBoundary.isDrained(pendingDrain))return;
            pendingDrain=0;spectrum.reset(analysis.fifo);if(!spectrum.isFrozen())clearPaths();
        }
        const bool desired=isShowing() && stage!=DynamicsStage::none && live.getToggleState() && !spectrum.isFrozen();
        if(desired==captureActive)return;
        if(!desired) {
            analysis.disable();captureActive=false;pendingDrain=captureBoundary.requestDrain();return;
        }
        // select() is presentation state. Publish capture only after the old
        // callback has completed and the consumer has discarded its last data.
        analysis.select(stage);analysis.fifo.enabled.store(true);captureActive=true;
    }
    void rebuildPaths() {
        clearPaths();if(!spectrum.hasData())return;const auto area=plot();
        for(int stream=0;stream<2;++stream) {
            auto& path=paths[size_t(stream)];float lastX=area.getX();bool started=false;
            for(int x=0;x<=int(area.getWidth());++x) {
                const float px=area.getX()+float(x),hz=xFrequency(px);if(hz>spectrum.lastSampleRate()*.5)break;
                const float y=levelY(spectrum.displayDB(stream==1,xFrequency(px-.5f),hz,xFrequency(px+.5f)));
                if(!started){path.startNewSubPath(px,y);started=true;}else path.lineTo(px,y);lastX=px;
            }
            if(started){auto& fill=fills[size_t(stream)];fill=path;fill.lineTo(lastX,area.getBottom());fill.lineTo(area.getX(),area.getBottom());fill.closeSubPath();}
        }
        const float dashes[]{4.f,3.f};juce::PathStrokeType(1.25f).createDashedStroke(dashedInput,paths[0],dashes,2);
    }
    void timerCallback() override {
        updateCapture();if(!isShowing())return;
        freeze.setEnabled(spectrum.isFrozen() || spectrum.canFreezeSnapshot());
        if(captureActive && spectrum.update(analysis.fifo,analysis.sampleRate())){rebuildPaths();repaint();}
    }
    DynamicsAnalysis& analysis;EQCaptureBoundary& captureBoundary;EQSpectrumAnalyzer spectrum;
    juce::Component::SafePointer<DynamicsSpectrumPanel> previousConsumer;
    DynamicsStage stage{DynamicsStage::none};bool captureActive{},sourceInvalidated{true};uint64_t pendingDrain{};
    int requestedPreOwner{-1},requestedPostSource{-1};
    juce::TextButton live,input,output,freeze;juce::ComboBox resolution,smoothing;
    std::array<juce::Path,2> paths,fills;juce::Path dashedInput;
    std::array<FrequencyMarker,3> markers{};int markerCount{},draggedMarker{-1};float highlightLow{},highlightHigh{};
};
}

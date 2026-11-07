#include "PluginEditor.h"
#include "EditorSurface.h"
#include "NativeMessageLoop.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <fstream>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

using namespace spectralforge;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
double processCPUSeconds(){
#if JUCE_WINDOWS
    FILETIME created{},exited{},kernel{},user{};check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)!=0,"Process CPU query");
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return double(k.QuadPart+u.QuadPart)*1.e-7;
#else
    timespec t{};check(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t)==0,"Process CPU query");return double(t.tv_sec)+double(t.tv_nsec)*1.e-9;
#endif
}
void settle(int ms){check(chimeraTest::dispatchFor(ms),"UI loop exited");}
template<class Predicate> bool waitFor(Predicate predicate){const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000;while(!predicate() && juce::Time::getMillisecondCounterHiRes()<deadline)settle(20);return predicate();}
template<class T> T* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return dynamic_cast<T*>(&c);for(auto* child:c.getChildren())if(auto* p=find<T>(*child,id))return p;return nullptr;}
juce::TextButton* tab(juce::Component& c,const juce::String& name){
    const auto id=name=="RIGS"?"cab":name=="PRE"?"pre":name=="POST"?"post":"tone.eq";
    auto* button=find<juce::TextButton>(c,"path."+juce::String(id));check(button!=nullptr,"Signal stage missing");return button;
}
void set(ChimeraProcessor& p,const juce::String& id,float v){auto* q=p.parameters().getParameter(id);check(q!=nullptr,"Parameter missing");q->setValueNotifyingHost(q->convertTo0to1(v));}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
struct HostEvents : juce::AudioProcessorParameter::Listener {
    int changes{},starts{},ends{};
    void parameterValueChanged(int,float) override {++changes;}
    void parameterGestureChanged(int,bool start) override {if(start)++starts;else ++ends;}
};
void numeric(juce::Slider& slider,const juce::String& text){slider.showTextBox();juce::TextEditor* input=nullptr;
    const auto scan=[&](auto&& self,juce::Component& c)->void{if(auto* t=dynamic_cast<juce::TextEditor*>(&c))input=t;for(auto* child:c.getChildren())self(self,*child);};scan(scan,slider);
    check(input!=nullptr,"Numeric editor did not open");input->setText(text,false);slider.hideTextBox(false);}
juce::MouseEvent mouse(juce::Component& target,juce::Point<float> position,juce::Point<float> down,bool dragging){return {juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1.f,0.f,0.f,0.f,0.f,&target,&target,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,dragging};}
void saveImage(const juce::Image& image,const juce::File& file) {
    juce::MemoryOutputStream encoded;juce::PNGImageFormat png;
    check(png.writeImageToStream(image,encoded),"Screenshot encoding");
    check(file.replaceWithData(encoded.getData(),encoded.getDataSize()),"Screenshot replacement failed");
    juce::MemoryBlock saved;
    if(!file.loadFileAsData(saved) || saved.getSize()!=encoded.getDataSize()
        || std::memcmp(saved.getData(),encoded.getData(),saved.getSize())!=0)
        throw std::runtime_error("Screenshot read-back did not match the completed PNG: "+file.getFileName().toStdString());
}
void snapshot(juce::Component& component,const juce::File& file) {
    saveImage(component.createComponentSnapshot(component.getLocalBounds()),file);
}
double percentile(std::vector<double> values,double fraction) {
    check(!values.empty(),"No native timing samples");std::sort(values.begin(),values.end());
    return values[(std::min)(values.size()-1,size_t(fraction*double(values.size()-1)))];
}
// A paced synthetic audio callback runs the production EQ processing on a
// separate thread while the native desktop dispatches real timer/paint events.
// It does not inject completed spectra or bypass the analyzer FIFO/FFT.
class SyntheticAudio {
public:
    explicit SyntheticAudio(ChimeraProcessor& p,bool wholeProcessor=false,double rate=48000,bool antiPhase=true):processor(p),fullRoute(wholeProcessor),sampleRate(rate),rightGain(antiPhase?-1.f:1.f){timings.reserve(10000);thread=std::thread([this]{run();});}
    ~SyntheticAudio(){running.store(false);if(thread.joinable())thread.join();}
    void stop(){running.store(false);if(thread.joinable())thread.join();}
    std::atomic<double> frequency{1007.8125},amplitude{.1};
    std::atomic<bool> pluck{false},noise{false}; // noise: deterministic white noise in place of the tone
    std::atomic<uint64_t> callbacks{0};
    const std::vector<double>& callbackMicrosAfterStop() const {return timings;}
private:
    void run() {
        juce::AudioBuffer<float> first(2,128),second(2,128);juce::MidiBuffer midi;double phase=0;uint64_t total=0;
        auto deadline=std::chrono::steady_clock::now();
        const auto interval=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(128./sampleRate));
        while(running.load()) {
            const double hz=frequency.load(),level=amplitude.load();const bool harmonic=pluck.load();
            for(int n=0;n<128;++n) {
                double value=std::sin(phase);
                if(noise.load()){seed=seed*1664525u+1013904223u;value=double(seed>>8)/double(1u<<24)*2.-1.;}
                if(harmonic) {
                    value+=.45*std::sin(phase*2)+.27*std::sin(phase*3)+.16*std::sin(phase*5)+.08*std::sin(phase*7);
                    const double age=std::fmod(double(total)/sampleRate,.65);value*=std::exp(-age/.18);
                }
                const float sample=float(level*value);first.setSample(0,n,sample);first.setSample(1,n,sample*rightGain);
                second.setSample(0,n,sample);second.setSample(1,n,sample*rightGain);
                phase=std::fmod(phase+2*juce::MathConstants<double>::pi*hz/sampleRate,2*juce::MathConstants<double>::pi);++total;
            }
            const auto callbackStart=std::chrono::steady_clock::now();
            if(fullRoute)processor.processBlock(first,midi);
            else {
                EQCaptureBoundary::CallbackScope captureScope(processor.eqCaptureBoundary());
                processor.graphicalEQ(0).process(first);processor.graphicalEQ(1).process(second);
            }
            const double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-callbackStart).count();
            if(timings.size()<10000)timings.push_back(elapsed);callbacks.fetch_add(1);
            deadline+=interval;const auto now=std::chrono::steady_clock::now();
            if(now>deadline+interval*2)deadline=now; // No unbounded catch-up burst after a scheduler pause.
            std::this_thread::sleep_until(deadline);
        }
    }
    ChimeraProcessor& processor;bool fullRoute{};double sampleRate{48000};float rightGain{-1};std::atomic<bool> running{true};uint32_t seed{12345u};std::vector<double> timings;std::thread thread;
};

void pendingThawEditorRecreation(ChimeraProcessor& processor,std::unique_ptr<ChimeraEditor>& editor) {
    auto* panel=find<GraphicalEQPanel>(*editor,"finalEQ_panel");check(panel,"Pending-thaw fixture has no final EQ");
    auto& fifo=processor.graphicalEQ(1).analyzer;
    // Model a callback whose capture decision predates thaw. The dedicated
    // Freeze suite additionally schedules this case on a paused producer thread.
    auto oldCallback=std::make_unique<EQCaptureBoundary::CallbackScope>(processor.eqCaptureBoundary());
    panel->setSnapshotFrozen(true);panel->setSnapshotFrozen(false);
    check(panel->isWaitingForFreshCapture() && !fifo.enabled.load(),"Thaw did not wait for the old callback");
    editor.reset();
    editor=std::make_unique<ChimeraEditor>(processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);
    tab(*editor,"EQ")->triggerClick();settle(60);
    find<juce::TextButton>(*editor,"path.final.eq")->triggerClick();
    panel=find<GraphicalEQPanel>(*editor,"finalEQ_panel");
    check(waitFor([&]{return panel->isShowing();}),"Recreated editor could not show pending EQ");
    check(panel->isWaitingForFreshCapture() && !fifo.enabled.load(),"Editor recreation forgot the processor's pending thaw request");
    for(int sample=0;sample<8192;++sample)fifo.push({.5f,.5f,.5f,.5f});
    oldCallback.reset();settle(60);
    check(panel->isWaitingForFreshCapture() && !fifo.enabled.load() && !panel->analyzerHasData(),
          "Recreated editor consumed late pre-thaw samples or accepted an old acknowledgement");
    juce::AudioBuffer<float> silence(2,128);silence.clear();juce::MidiBuffer midi;
    processor.processBlock(silence,midi); // Actual callback entry/exit acknowledges the pending request.
    check(waitFor([&]{return !panel->isWaitingForFreshCapture() && fifo.enabled.load();}),"Recreated editor never accepted the completed callback boundary");
    check(fifo.pendingSamples()==0 && !panel->analyzerHasData(),"Recreated editor retained late samples after acknowledged purge");
    std::cout<<"PASS pending thaw survives native editor recreation / late old window rejected / actual processBlock acknowledgement\n";
}
int spectrumPixels(const juce::Image& image,juce::Rectangle<int> bounds,bool output) {
    int count=0;bounds=bounds.getIntersection(image.getBounds());
    for(int y=bounds.getY();y<bounds.getBottom();++y)for(int x=bounds.getX();x<bounds.getRight();++x) {
        const auto colour=image.getPixelAt(x,y);const int r=colour.getRed(),g=colour.getGreen(),b=colour.getBlue();
        if(output ? (r>g+8 && g>b+8) : (b>r+14 && g>r+10))++count;
    }
    return count;
}
void overlappingBandDrag(ChimeraProcessor& processor,GraphicalEQPanel& panel,juce::ComboBox& selector,int bank) {
    constexpr std::array<const char*,4> fields{"frequency","gain","type","enabled"};
    std::array<std::array<float,4>,graphicalEQBands> previousValues{};
    const int previousSelection=selector.getSelectedId();
    for(int band=0;band<graphicalEQBands;++band)
        for(size_t field=0;field<fields.size();++field)
            previousValues[(size_t)band][field]=get(processor,eqID(bank,band,fields[field]));
    // The chosen node is painted last. Re-clicking an exact overlap must keep
    // that visible selection instead of redirecting automation to Band 1.
    for(int band=0;band<graphicalEQBands;++band) {
        set(processor,eqID(bank,band,"frequency"),1000);
        set(processor,eqID(bank,band,"gain"),0);
        set(processor,eqID(bank,band,"type"),0);
        set(processor,eqID(bank,band,"enabled"),1);
    }
    selector.setSelectedId(graphicalEQBands,juce::sendNotificationSync);settle(50);
    check(panel.selectedBand()==graphicalEQBands-1,"Overlap fixture did not select Band 12");
    std::array<HostEvents,graphicalEQBands> events{};
    std::array<float,graphicalEQBands> frequencies{},gains{};
    for(int band=0;band<graphicalEQBands;++band) {
        frequencies[(size_t)band]=get(processor,eqID(bank,band,"frequency"));
        gains[(size_t)band]=get(processor,eqID(bank,band,"gain"));
        for(const char* field:{"frequency","gain"})
            processor.parameters().getParameter(eqID(bank,band,field))->addListener(&events[(size_t)band]);
    }
    const auto start=panel.nodePosition(graphicalEQBands-1);
    const auto end=start+juce::Point<float>(panel.graphBounds().getWidth()*.02f,-panel.graphBounds().getHeight()*.04f);
    panel.mouseDown(mouse(panel,start,start,false));panel.mouseDrag(mouse(panel,end,start,true));panel.mouseUp(mouse(panel,end,start,true));
    for(int band=0;band<graphicalEQBands;++band)
        for(const char* field:{"frequency","gain"})
            processor.parameters().getParameter(eqID(bank,band,field))->removeListener(&events[(size_t)band]);
    check(panel.selectedBand()==graphicalEQBands-1 && selector.getSelectedId()==graphicalEQBands,
          "Dragging overlapping nodes changed the selected Band 12");
    for(int band=0;band<graphicalEQBands;++band) {
        const auto& host=events[(size_t)band];
        if(band==graphicalEQBands-1) {
            check(get(processor,eqID(bank,band,"frequency"))>frequencies[(size_t)band]
                  && get(processor,eqID(bank,band,"gain"))>gains[(size_t)band],"Overlap drag did not edit Band 12");
            check(host.changes>0 && host.starts==2 && host.ends==2,"Overlap drag did not balance Band 12 host gestures");
        } else {
            check(get(processor,eqID(bank,band,"frequency"))==frequencies[(size_t)band]
                  && get(processor,eqID(bank,band,"gain"))==gains[(size_t)band],"Overlap drag edited an unselected band");
            check(host.changes==0 && host.starts==0 && host.ends==0,"Overlap drag notified the host for an unselected band");
        }
    }
    for(int band=0;band<graphicalEQBands;++band)
        for(size_t field=0;field<fields.size();++field)
            set(processor,eqID(bank,band,fields[field]),previousValues[(size_t)band][field]);
    selector.setSelectedId(previousSelection,juce::sendNotificationSync);
    std::cout<<"PASS overlapping nodes retain selected Band 12 / only selected-band host changes / balanced gestures bank="<<bank<<'\n';
}
juce::MouseEvent mouseWith(juce::Component& target,juce::Point<float> position,juce::Point<float> down,bool dragging,int modifiers) {
    return {juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|modifiers),1.f,0.f,0.f,0.f,0.f,
            &target,&target,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,dragging};
}
// Shift drags a node at a tenth of the pointer's travel; Alt / Option keeps a
// drag to the axis it first moves along; pressing off centre does not move the
// node; Shift on the wheel is a fine Q step. Each drag stays one gesture per field.
void modifierDrag(ChimeraProcessor& processor,GraphicalEQPanel& panel,juce::ComboBox& selector,int bank) {
    selector.setSelectedId(1,juce::sendNotificationSync);
    const auto reset=[&]{set(processor,eqID(bank,0,"type"),0);set(processor,eqID(bank,0,"enabled"),1);
                         set(processor,eqID(bank,0,"frequency"),1000);set(processor,eqID(bank,0,"gain"),0);set(processor,eqID(bank,0,"q"),1);settle(60);};
    const auto plot=panel.graphBounds();
    const auto frequencyAt=[&](float x){return 20.f*std::pow(1000.f,std::clamp((x-plot.getX())/plot.getWidth(),0.f,1.f));};
    const auto gainAt=[&](float y){return std::clamp((plot.getCentreY()-y)/plot.getHeight()*48.f,-24.f,24.f);};
    const auto frequency=[&]{return get(processor,eqID(bank,0,"frequency"));};
    const auto gain=[&]{return get(processor,eqID(bank,0,"gain"));};
    const auto within=[](float a,float b,float tolerance){return std::abs(a-b)<=tolerance;};
    // Host quantisation: 0.01 Hz-ish in this range and 0.01 dB-ish; allow both.
    const auto sameFrequency=[&](float expected){return within(frequency(),expected,std::max(.05f,expected*2e-4f));};
    const auto sameGain=[&](float expected){return within(gain(),expected,.02f);};
    using MK=juce::ModifierKeys;
    const auto drag=[&](juce::Point<float> press,std::initializer_list<std::pair<juce::Point<float>,int>> moves){
        HostEvents events;auto* f=processor.parameters().getParameter(eqID(bank,0,"frequency"));auto* g=processor.parameters().getParameter(eqID(bank,0,"gain"));
        f->addListener(&events);g->addListener(&events);
        panel.mouseDown(mouse(panel,press,press,false));
        juce::Point<float> last=press;
        for(const auto& [point,mods]:moves){panel.mouseDrag(mouseWith(panel,point,press,true,mods));last=point;}
        panel.mouseUp(mouse(panel,last,press,true));
        f->removeListener(&events);g->removeListener(&events);
        check(events.starts==2 && events.ends==2,"Modifier drag did not keep one balanced gesture per field");
    };

    reset();auto start=panel.nodePosition(0);
    drag(start,{{start+juce::Point<float>(50,-20),MK::shiftModifier},{start+juce::Point<float>(100,-40),MK::shiftModifier}});
    check(sameFrequency(frequencyAt(start.x+10)) && sameGain(gainAt(start.y-4)),"Shift drag did not move a tenth of the pointer travel");
    check(frequency()<frequencyAt(start.x+20),"Shift drag moved at full speed");

    reset();start=panel.nodePosition(0);
    drag(start,{{start+juce::Point<float>(20,0),0},{start+juce::Point<float>(120,0),MK::shiftModifier}});
    check(sameFrequency(frequencyAt(start.x+30)),"Pressing Shift mid-drag made the node jump");

    reset();start=panel.nodePosition(0);
    drag(start+juce::Point<float>(6,5),{{start+juce::Point<float>(6,5),0}});
    check(sameFrequency(1000) && sameGain(0),"Pressing off a node's centre made it jump to the pointer");

    reset();start=panel.nodePosition(0);
    drag(start,{{start+juce::Point<float>(30,-5),MK::altModifier},{start+juce::Point<float>(60,-30),MK::altModifier}});
    check(sameFrequency(frequencyAt(start.x+60)) && sameGain(0),"Alt drag along frequency changed gain");

    reset();start=panel.nodePosition(0);
    drag(start,{{start+juce::Point<float>(5,-30),MK::altModifier},{start+juce::Point<float>(40,-50),MK::altModifier}});
    check(sameFrequency(1000) && sameGain(gainAt(start.y-50)),"Alt drag along gain changed frequency");

    reset();start=panel.nodePosition(0);
    drag(start,{{start+juce::Point<float>(2,-1),MK::altModifier}});
    check(sameFrequency(1000) && sameGain(0),"Alt drag moved before choosing an axis");

    reset();start=panel.nodePosition(0);
    drag(start,{{start+juce::Point<float>(100,-40),MK::shiftModifier|MK::altModifier}});
    check(sameFrequency(frequencyAt(start.x+10)) && sameGain(0),"Shift+Alt drag was not fine and frequency-only");

    // The panel drops the first edit after a host change it has not yet seen
    // (its 25 Hz timer may not have run on a slow runner); let it see reset()
    // first, as a press does before a drag.
    reset();panel.selectBandForEditing(0);
    juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;
    panel.mouseWheelMove(mouseWith(panel,plot.getCentre(),plot.getCentre(),false,MK::shiftModifier),wheel);
    const float fineQ=get(processor,eqID(bank,0,"q"));
    reset();panel.selectBandForEditing(0);panel.mouseWheelMove(mouse(panel,plot.getCentre(),plot.getCentre(),false),wheel);
    const float coarseQ=get(processor,eqID(bank,0,"q"));
    check(within(fineQ,std::exp(.2f*.15f),.002f) && within(coarseQ,std::exp(.2f*1.5f),.005f),"Shift wheel was not a fine Q step");
    reset();panel.selectBandForEditing(0);   // leave no unseen host change for the next test
    std::cout<<"PASS EQ modifier drag: Shift fine (x0.1, no jump mid-drag), Alt one axis, off-centre press, Shift wheel Q bank="<<bank<<'\n';
}
// Exercise actual panel callbacks and APVTS values at every input boundary.
// Offsets are independent expected node travel in pixels, not the drag algorithm.
int modifierTransitions(ChimeraProcessor& processor,GraphicalEQPanel& panel,int bank) {
    using MK=juce::ModifierKeys;
    constexpr int alt=MK::altModifier,shift=MK::shiftModifier;
    struct Step {juce::Point<float> pointer;int mods;juce::Point<float> node;bool keysOnly=false;};
    struct Case {const char* name;int downMods;std::vector<Step> steps;};
    const std::vector<Case> cases{
        {"release frequency lock",alt,{{{30,-5},alt,{30,0}},{{60,-30},alt,{60,0}},
            {{60,-30},0,{60,0}},{{70,-40},0,{70,-10}}}},
        {"release gain lock",alt,{{{5,-30},alt,{0,-30}},{{40,-50},alt,{0,-50}},
            {{40,-50},0,{0,-50}},{{50,-60},0,{10,-60}}}},
        {"free then lock from current node",0,{{{20,-20},0,{20,-20}},{{20,-20},alt,{20,-20}},
            {{23,-21},alt,{20,-20}},{{26,-21},alt,{26,-20}}}},
        // No suppressed travel at release: isolate the stale relock threshold
        // and axis decision from the independent unlock-position jump.
        {"clean release then relock gain",alt,{{{30,0},alt,{30,0}},{{40,-10},0,{40,-10}},
            {{40,-10},alt,{40,-10}},{{41,-12},alt,{40,-10}},{{41,-16},alt,{40,-16}}}},
        {"clean release then relock frequency",alt,{{{0,-30},alt,{0,-30}},{{10,-40},0,{10,-40}},
            {{10,-40},alt,{10,-40}},{{16,-41},alt,{16,-40}}}},
        {"relock frequency then gain",alt,{{{30,-5},alt,{30,0}},{{40,-15},0,{40,-10}},
            {{42,-16},alt,{40,-10}},{{46,-16},alt,{46,-10}},{{46,-16},0,{46,-10}},
            {{46,-16},alt,{46,-10}},{{47,-22},alt,{46,-16}}}},
        {"relock gain then frequency",alt,{{{5,-30},alt,{0,-30}},{{15,-40},0,{10,-40}},
            {{16,-42},alt,{10,-40}},{{16,-46},alt,{10,-46}},{{16,-46},0,{10,-46}},
            {{16,-46},alt,{10,-46}},{{22,-47},alt,{16,-46}}}},
        {"release below lock threshold",alt,{{{2,-1},alt,{0,0}},{{2,-1},0,{0,0}},{{3,-2},0,{1,-1}}}},
        {"Shift and Alt transitions",shift|alt,{{{30,-10},shift|alt,{0,0}},{{50,-20},shift|alt,{5,0}},
            {{50,-20},alt,{5,0}},{{60,-60},alt,{15,0}},{{60,-60},shift|alt,{15,0}},
            {{80,-80},shift|alt,{17,0}},{{80,-80},shift,{17,0}},{{90,-90},shift,{18,-1}},
            {{90,-90},0,{18,-1}},{{100,-100},0,{28,-11}}}},
        {"simultaneous moving transitions",0,{{{20,-10},0,{20,-10}},{{70,-30},shift|alt,{25,-10}},
            {{80,-40},0,{35,-20}},{{81,-46},alt,{35,-26}}}},
        {"stationary key callbacks rearm axis",alt,{{{30,-5},alt,{30,0}},{{60,-30},alt,{60,0}},
            {{60,-30},0,{60,0},true},{{60,-30},alt,{60,0},true},
            {{61,-32},alt,{60,0}},{{61,-36},alt,{60,-6}}}},
        {"stationary Shift retains locked axis",alt,{{{30,-5},alt,{30,0}},
            {{30,-5},shift|alt,{30,0},true},{{50,-105},shift|alt,{32,0}},
            {{50,-105},alt,{32,0},true},{{55,-110},alt,{37,0}}}},
        {"key release then moving relock",alt,{{{5,-30},alt,{0,-30}},{{40,-50},alt,{0,-50}},
            {{40,-50},0,{0,-50},true},{{50,-60},0,{10,-60}},
            {{50,-60},alt,{10,-60},true},{{56,-61},alt,{16,-60}}}}
    };
    struct Listen {
        juce::RangedAudioParameter* parameter;HostEvents events;
        explicit Listen(juce::RangedAudioParameter* p):parameter(p){parameter->addListener(&events);}
        ~Listen(){parameter->removeListener(&events);}
    };
    int failures=0;
    for(const auto& test:cases) {
        // A real external update gives each case an empty history baseline.
        set(processor,eqID(bank,0,"frequency"),900);
        set(processor,eqID(bank,0,"type"),0);set(processor,eqID(bank,0,"enabled"),1);
        set(processor,eqID(bank,0,"frequency"),1000);set(processor,eqID(bank,0,"gain"),0);
        settle(60);panel.selectBandForEditing(0);
        auto& history=processor.eqEditing();
        check(!history.canUndo() && !history.canRedo(),"Modifier fixture did not clear external history");
        Listen frequency(processor.parameters().getParameter(eqID(bank,0,"frequency")));
        Listen gain(processor.parameters().getParameter(eqID(bank,0,"gain")));
        Listen q(processor.parameters().getParameter(eqID(bank,0,"q")));
        const float initialFrequency=frequency.parameter->getValue(),initialGain=gain.parameter->getValue();
        const auto start=panel.nodePosition(0),press=start+juce::Point<float>(6,5);
        auto last=press;
        try {
            panel.mouseDown(mouseWith(panel,press,press,false,test.downMods));
            check(panel.nodePosition(0).getDistanceFrom(start)<.08f,"Off-centre modifier press moved the node");
            for(size_t step=0;step<test.steps.size();++step) {
                const auto& input=test.steps[step];last=press+input.pointer;
                const int changes=frequency.events.changes+gain.events.changes;
                if(input.keysOnly)panel.modifierKeysChanged(MK(MK::leftButtonModifier|input.mods));
                else panel.mouseDrag(mouseWith(panel,last,press,true,input.mods));
                check(!input.keysOnly || changes==frequency.events.changes+gain.events.changes,
                      "Modifier-only callback notified host parameter values");
                const auto actual=panel.nodePosition(0)-start;
                if(actual.getDistanceFrom(input.node)>.08f) {
                    std::cerr<<"MEASURE bank="<<bank<<" case="<<test.name<<" step="<<step
                        <<" expected="<<input.node.x<<','<<input.node.y<<" actual="<<actual.x<<','<<actual.y<<'\n';
                    throw std::runtime_error("Modifier transition broke node continuity or axis lock");
                }
                check(frequency.events.starts==1 && gain.events.starts==1
                      && frequency.events.ends==0 && gain.events.ends==0,"Modifier transition split a host gesture");
            }
            panel.mouseUp(mouseWith(panel,last,press,true,0));
            check(frequency.events.starts==1 && frequency.events.ends==1
                  && gain.events.starts==1 && gain.events.ends==1,"Drag did not close each host gesture exactly once");
            check(q.events.starts==0 && q.events.ends==0 && q.events.changes==0,"Node drag touched Q");
            const float finalFrequency=frequency.parameter->getValue(),finalGain=gain.parameter->getValue();
            check(history.undo(),"Modifier drag did not create an undo step");
            check(frequency.parameter->getValue()==initialFrequency && gain.parameter->getValue()==initialGain,
                  "One undo did not restore the complete modifier drag");
            check(!history.canUndo() && history.canRedo(),"Modifier drag created more than one undo step");
            check(history.redo(),"Modifier drag redo missing");
            check(frequency.parameter->getValue()==finalFrequency && gain.parameter->getValue()==finalGain,
                  "Redo did not restore both final parameter values");
            check(!history.canRedo(),"Modifier drag created more than one redo step");
            std::cout<<"PASS modifier transitions bank="<<bank<<" case="<<test.name<<'\n';
        } catch(const std::exception& error) {
            panel.mouseUp(mouseWith(panel,last,press,true,0));
            ++failures;std::cerr<<"FAIL modifier transitions bank="<<bank<<" case="<<test.name<<": "<<error.what()<<'\n';
        }
    }
    return failures;
}

// PEAK toggle: editor-local display state; the toolbar keeps every control
// visible and separate in both the full and the compact workspace layouts.
void peakToggle(ChimeraProcessor& processor,GraphicalEQPanel& panel,int bank) {
    auto* peak=find<juce::TextButton>(panel,eqID(bank,-1,"peakHold"));
    check(peak!=nullptr && !peak->getToggleState() && !panel.isPeakHold(),"PEAK toggle missing or on by default");
    juce::MemoryBlock before;processor.getStateInformation(before);
    peak->triggerClick();check(waitFor([&]{return panel.isPeakHold() && peak->getToggleState();}),"PEAK toggle did not turn peak hold on");
    juce::MemoryBlock during;processor.getStateInformation(during);
    check(before==during,"PEAK changed saved plug-in state");
    peak->triggerClick();check(waitFor([&]{return !panel.isPeakHold() && !peak->getToggleState();}),"PEAK toggle did not turn peak hold off");
    for(bool compact:{false,true}) {
        panel.setCompactWorkspace(compact);settle(30);
        std::vector<juce::Component*> toolbar;
        for(const char* id:{"analyzer","inputSpectrum","outputSpectrum","freeze","peakHold","resolution","smoothing","bypass"}) {
            auto* c=find<juce::Component>(panel,eqID(bank,-1,id));check(c!=nullptr && c->isVisible(),"EQ toolbar control missing");
            check(panel.getLocalBounds().contains(c->getBounds()),"EQ toolbar control is clipped");toolbar.push_back(c);
        }
        for(size_t a=0;a<toolbar.size();++a)for(size_t b=a+1;b<toolbar.size();++b)
            check(!toolbar[a]->getBounds().intersects(toolbar[b]->getBounds()),"EQ toolbar controls overlap");
    }
    panel.setCompactWorkspace(false);settle(30);
    std::cout<<"PASS EQ PEAK toggle: off by default, on/off, not saved state; toolbar unclipped and non-overlapping (full and compact) bank="<<bank<<'\n';
}
void closeTo(float actual,float expected,float tolerance,const char* message) {
    if(!(std::abs(actual-expected)<=tolerance)){std::cerr<<message<<": actual="<<actual<<" expected="<<expected<<'\n';check(false,message);}
}
// Display range (+/-6/12/24 dB view) and the cursor readout. View only:
// band values, drags and saved state keep the parameter range.
void displayRangeAndReadout(ChimeraProcessor& processor,GraphicalEQPanel& panel,juce::ComboBox& selector,int bank) {
    selector.setSelectedId(1,juce::sendNotificationSync);
    set(processor,eqID(bank,0,"type"),0);set(processor,eqID(bank,0,"enabled"),1);
    set(processor,eqID(bank,0,"frequency"),1000);set(processor,eqID(bank,0,"gain"),6);settle(60);panel.selectBandForEditing(0);
    const auto plot=panel.graphBounds();
    check(panel.displayRange()==24.f,"EQ display range default changed");
    juce::MemoryBlock before;processor.getStateInformation(before);
    // Every host parameter, the Undo/Redo stacks, the A/B slot and the edit
    // revision must be untouched by switching the view range.
    auto& history=processor.eqEditing();
    std::vector<float> parametersBefore;for(auto* p:processor.getParameters())parametersBefore.push_back(p->getValue());
    const bool undoBefore=history.canUndo(),redoBefore=history.canRedo();
    const auto undoTextBefore=history.undoDescription(),redoTextBefore=history.redoDescription();
    const int slotBefore=processor.comparisonSlot();const auto revisionBefore=history.revision();
    closeTo(panel.nodePosition(0).y,plot.getCentreY()-plot.getHeight()/8,.6f,"+6 dB node misplaced at +/-24");
    panel.setDisplayRange(12);check(panel.displayRange()==12.f,"Display range +/-12 not applied");
    closeTo(panel.nodePosition(0).y,plot.getCentreY()-plot.getHeight()/4,.6f,"+6 dB node misplaced at +/-12");
    panel.setDisplayRange(6);closeTo(panel.nodePosition(0).y,plot.getY(),.6f,"+6 dB node misplaced at +/-6");
    panel.setDisplayRange(7);check(panel.displayRange()==24.f,"Unsupported display range was accepted");
    for(float range:{12.f,6.f,24.f,6.f,24.f}){panel.setDisplayRange(range);settle(30);}
    juce::MemoryBlock after;processor.getStateInformation(after);check(before==after,"Display range changed saved state");
    {
        size_t i=0;for(auto* p:processor.getParameters())check(p->getValue()==parametersBefore[i++],"Display range changed a host parameter");
        check(history.canUndo()==undoBefore && history.canRedo()==redoBefore && history.undoDescription()==undoTextBefore
              && history.redoDescription()==redoTextBefore,"Display range changed the Undo/Redo history");
        check(processor.comparisonSlot()==slotBefore,"Display range changed the A/B slot");
        check(history.revision()==revisionBefore,"Display range advanced the EQ edit revision");
    }
    // A band beyond the view is drawn and hit at the graph edge; dragging it
    // moves from its true value (no jump to the edge value).
    set(processor,eqID(bank,0,"gain"),18);settle(60);panel.selectBandForEditing(0);panel.setDisplayRange(12);
    const auto edge=panel.visibleNodePosition(0);closeTo(edge.y,plot.getY(),.6f,"Out-of-view band not pinned to the edge");
    check(panel.nodePosition(0).y<plot.getY(),"Fixture band is not beyond the view");
    panel.mouseDown(mouse(panel,edge,edge,false));
    panel.mouseDrag(mouseWith(panel,edge+juce::Point<float>(30,0),edge,true,juce::ModifierKeys::altModifier));
    panel.mouseDrag(mouseWith(panel,edge+juce::Point<float>(60,0),edge,true,juce::ModifierKeys::altModifier));
    panel.mouseUp(mouse(panel,edge+juce::Point<float>(60,0),edge,true));
    check(std::abs(get(processor,eqID(bank,0,"gain"))-18)<.02f && get(processor,eqID(bank,0,"frequency"))>1000,
          "Dragging an out-of-view band jumped to the edge value");
    // A drag in a narrow view is one ordinary Undo step on the true values;
    // Undo/Redo do not change the view range.
    const float dragged=get(processor,eqID(bank,0,"frequency"));
    check(history.undo() && std::abs(get(processor,eqID(bank,0,"frequency"))-1000)<.02f && std::abs(get(processor,eqID(bank,0,"gain"))-18)<.02f,
          "Undo of a narrow-view drag did not restore the true values");
    check(history.redo() && std::abs(get(processor,eqID(bank,0,"frequency"))-dragged)<.02f,"Redo of a narrow-view drag failed");
    check(panel.displayRange()==12.f,"Undo/Redo changed the display range");
    // Readout: frequency and EQ dB at the cursor, empty outside the graph.
    const juce::Point<float> at{plot.getX()+plot.getWidth()*(std::log(50.f)/std::log(1000.f)),plot.getCentreY()-plot.getHeight()/4};
    const auto text=panel.readoutAt(at);
    check(text.contains("1.00 kHz") && text.contains("+6.0 dB"),("Readout wrong: "+text).toRawUTF8());
    check(panel.readoutAt({plot.getX()-5,plot.getCentreY()}).isEmpty(),"Readout outside the graph");
    panel.mouseMove(mouse(panel,at,at,false));check(panel.hoverPosition()==at,"Hover position not tracked");
    panel.mouseExit(mouse(panel,at,at,false));check(!plot.contains(panel.hoverPosition()),"Hover kept after leaving");
    panel.setDisplayRange(24);set(processor,eqID(bank,0,"gain"),0);settle(60);panel.selectBandForEditing(0);
    std::cout<<"PASS EQ display range +/-6/12/24 (view only, edge-pinned out-of-view node, true-value drag) and cursor readout bank="<<bank<<'\n';
}
// PEAK hold after the audio stops: the live spectrum has decayed away and
// only the held peak is drawn. The rebuild must keep both peak lines and
// build no live line, fill or dashed IN; Freeze, IN/OUT, resuming audio and
// an FFT size change keep their behaviour. Times rebuildSpectrumPaths().
void peakOnlyPaths(ChimeraProcessor& processor,GraphicalEQPanel& panel,int bank) {
    auto* input=find<juce::TextButton>(panel,eqID(bank,-1,"inputSpectrum"));
    auto* output=find<juce::TextButton>(panel,eqID(bank,-1,"outputSpectrum"));
    auto* resolution=find<juce::ComboBox>(panel,eqID(bank,-1,"resolution"));
    check(input && output && resolution,"Analyzer controls missing");
    const auto decayed=[&]{
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+20000;
        while((panel.analyzerHasData() || !panel.hasPeakHold()) && juce::Time::getMillisecondCounterHiRes()<deadline)settle(50);
        check(!panel.analyzerHasData() && panel.hasPeakHold(),"Live spectrum did not decay away while PEAK stayed held");
    };
    panel.setPeakHold(true);
    {
        SyntheticAudio audio(processor);
        check(waitFor([&]{return panel.hasPeakHold() && panel.analyzerHasData();}),"PEAK fixture did not record live audio");
        settle(300);check(panel.liveSpectrumPathsBuilt() && panel.peakPathsBuilt(),"Live audio did not build live and peak paths");
    }
    decayed();
    panel.resized();check(!panel.liveSpectrumPathsBuilt() && panel.peakPathsBuilt(),"Peak-only rebuild built live paths or lost the peak");
    std::vector<double> micros;
    for(int n=0;n<200;++n){panel.resized();micros.push_back(panel.spectrumPathMicros());}
    std::cout<<"MEASURE peak_only_path bank="<<bank<<" width="<<int(panel.graphBounds().getWidth())
             <<" p50_us="<<percentile(micros,.5)<<" p99_us="<<percentile(micros,.99)<<'\n';
    // IN/OUT only choose what is drawn: both peak lines stay built.
    output->triggerClick();input->triggerClick();panel.resized();
    check(!panel.liveSpectrumPathsBuilt() && panel.peakPathsBuilt(),"IN/OUT toggles changed the peak-only paths");
    output->triggerClick();input->triggerClick();
    // Freeze shows the last complete frame: live paths again, peak kept.
    panel.setSnapshotFrozen(true);check(panel.isSnapshotFrozen(),"Peak-only state could not freeze its last frame");
    check(panel.liveSpectrumPathsBuilt() && panel.peakPathsBuilt(),"Freeze did not draw its frame with the held peak");
    panel.setSnapshotFrozen(false);
    check(waitFor([&]{panel.resized();return panel.hasPeakHold() && !panel.analyzerHasData() && !panel.liveSpectrumPathsBuilt() && panel.peakPathsBuilt();}),
          "Thaw without audio did not return to peak-only paths");
    // Audio resumes: the live line is built again next to the peak.
    {
        SyntheticAudio audio(processor);
        check(waitFor([&]{return panel.analyzerHasData() && panel.liveSpectrumPathsBuilt() && panel.peakPathsBuilt();}),"Resumed audio did not rebuild the live line");
    }
    decayed();
    // A new FFT size clears the hold: nothing is left to build.
    const int previous=resolution->getSelectedId();resolution->setSelectedId(previous==3?2:3,juce::sendNotificationSync);
    panel.resized();check(!panel.hasPeakHold() && !panel.peakPathsBuilt() && !panel.liveSpectrumPathsBuilt(),"FFT size change kept peak or live paths");
    resolution->setSelectedId(previous,juce::sendNotificationSync);
    panel.setPeakHold(false);
    std::cout<<"PASS EQ peak-only paths: no live line/fill/dashed IN without live data, both peak lines kept through IN/OUT and Freeze/thaw, live rebuilt on resumed audio, FFT size clears bank="<<bank<<'\n';
}
// Frequency view (full / 20 Hz-2 kHz / 20-500 Hz): view only. A band
// outside a zoomed view is drawn and hit at the edge and drags from its
// true frequency; saved state is unchanged.
void frequencyViewZoom(ChimeraProcessor& processor,GraphicalEQPanel& panel,juce::ComboBox& selector,int bank) {
    selector.setSelectedId(1,juce::sendNotificationSync);
    set(processor,eqID(bank,0,"type"),0);set(processor,eqID(bank,0,"enabled"),1);
    set(processor,eqID(bank,0,"frequency"),100);set(processor,eqID(bank,0,"gain"),6);settle(60);panel.selectBandForEditing(0);
    const auto plot=panel.graphBounds();
    check(panel.frequencyViewIndex()==0,"Frequency view default changed");
    juce::MemoryBlock before;processor.getStateInformation(before);
    closeTo(panel.nodePosition(0).x,plot.getX()+plot.getWidth()*std::log(5.f)/std::log(1000.f),.6f,"100 Hz misplaced in the full view");
    panel.setFrequencyView(2);check(panel.frequencyViewIndex()==2,"Bass view not applied");
    closeTo(panel.nodePosition(0).x,plot.getX()+plot.getWidth()*std::log(5.f)/std::log(25.f),.6f,"100 Hz misplaced in the 20-500 Hz view");
    panel.setFrequencyView(1);closeTo(panel.nodePosition(0).x,plot.getX()+plot.getWidth()*std::log(5.f)/std::log(100.f),.6f,"100 Hz misplaced in the 20 Hz-2 kHz view");
    panel.setFrequencyView(7);check(panel.frequencyViewIndex()==0,"Unsupported frequency view was accepted");
    for(int view:{2,1,0,2,0}){panel.setFrequencyView(view);settle(30);}
    juce::MemoryBlock after;processor.getStateInformation(after);check(before==after,"Frequency view changed saved state");
    // A 5 kHz band in the 20-500 Hz view: pinned to the right edge, and a
    // gain-only (Alt) drag from there keeps its true frequency.
    set(processor,eqID(bank,0,"frequency"),5000);settle(60);panel.selectBandForEditing(0);panel.setFrequencyView(2);
    const auto edge=panel.visibleNodePosition(0);closeTo(edge.x,plot.getRight(),.6f,"Out-of-view band not pinned to the right edge");
    check(panel.nodePosition(0).x>plot.getRight(),"Fixture band is not beyond the view");
    const float gainBefore=get(processor,eqID(bank,0,"gain"));
    panel.mouseDown(mouse(panel,edge,edge,false));
    panel.mouseDrag(mouseWith(panel,edge+juce::Point<float>(0,-30),edge,true,juce::ModifierKeys::altModifier));
    panel.mouseDrag(mouseWith(panel,edge+juce::Point<float>(0,-60),edge,true,juce::ModifierKeys::altModifier));
    panel.mouseUp(mouse(panel,edge+juce::Point<float>(0,-60),edge,true));
    check(std::abs(get(processor,eqID(bank,0,"frequency"))-5000)<1 && get(processor,eqID(bank,0,"gain"))>gainBefore,
          "Dragging an out-of-view band jumped to the edge frequency");
    // A -18 dB band in a +/-12 dB view is pinned to the bottom edge, which is
    // also outside plot(); a frequency-only drag from there keeps its gain.
    panel.setFrequencyView(0);set(processor,eqID(bank,0,"frequency"),1000);set(processor,eqID(bank,0,"gain"),-18);settle(60);
    panel.selectBandForEditing(0);panel.setDisplayRange(12);
    const auto bottom=panel.visibleNodePosition(0);closeTo(bottom.y,plot.getBottom(),.6f,"Out-of-view band not pinned to the bottom edge");
    panel.mouseDown(mouse(panel,bottom,bottom,false));
    panel.mouseDrag(mouseWith(panel,bottom+juce::Point<float>(30,0),bottom,true,juce::ModifierKeys::altModifier));
    panel.mouseDrag(mouseWith(panel,bottom+juce::Point<float>(60,0),bottom,true,juce::ModifierKeys::altModifier));
    panel.mouseUp(mouse(panel,bottom+juce::Point<float>(60,0),bottom,true));
    check(std::abs(get(processor,eqID(bank,0,"gain"))+18)<.02f && get(processor,eqID(bank,0,"frequency"))>1000,
          "A band pinned to the bottom edge could not be dragged from its true gain");
    panel.setDisplayRange(24);panel.setFrequencyView(2);
    // Readout follows the zoomed mapping.
    const juce::Point<float> at{plot.getX()+plot.getWidth()*std::log(5.f)/std::log(25.f),plot.getCentreY()};
    check(panel.readoutAt(at).contains("100 Hz"),("Zoomed readout wrong: "+panel.readoutAt(at)).toRawUTF8());
    panel.setFrequencyView(0);set(processor,eqID(bank,0,"frequency"),1000);set(processor,eqID(bank,0,"gain"),0);settle(60);panel.selectBandForEditing(0);
    std::cout<<"PASS EQ frequency view full/20 Hz-2 kHz/20-500 Hz (view only, edge-pinned out-of-view node, true-frequency drag, readout) bank="<<bank<<'\n';
}
// Analyzer tilt (flat / 3 / 4.5 / 6 dB per octave around 1 kHz): the live
// and PEAK lines move by exactly the tilt at each frequency; the readout
// keeps the measured dBFS; saved state is unchanged. Uses a frozen frame.
void analyzerTilt(ChimeraProcessor& processor,GraphicalEQPanel& panel,int bank) {
    const auto plot=panel.graphBounds();
    check(panel.analyzerTilt()==0.f,"Analyzer tilt default is not flat");
    panel.setPeakHold(true);
    {
        SyntheticAudio audio(processor);audio.noise.store(true);
        check(waitFor([&]{return panel.analyzerHasData() && panel.hasPeakHold();}),"Tilt fixture did not record live audio");
        settle(300);panel.setSnapshotFrozen(true);
    }
    check(panel.isSnapshotFrozen(),"Tilt fixture could not freeze a frame");panel.resized();
    juce::MemoryBlock before;processor.getStateInformation(before);
    const auto xAt=[&](float hz){return plot.getX()+plot.getWidth()*std::log(hz/20.f)/std::log(1000.f);};
    const std::array<float,4> probes{100.f,400.f,1000.f,4000.f};
    struct Sample{float live,peak;};std::array<Sample,4> flat{};std::array<juce::String,4> readouts;
    for(size_t i=0;i<probes.size();++i){flat[i]={panel.drawnLineY(false,1,xAt(probes[i])),panel.drawnLineY(true,1,xAt(probes[i]))};readouts[i]=panel.readoutAt({xAt(probes[i]),plot.getCentreY()});}
    const auto dbOf=[&](float y){return (plot.getBottom()-y)/plot.getHeight()*96.f-96.f;};
    const auto unclamped=[&](float db){return db>-95.5f && db<-.5f;};
    for(size_t i=0;i<probes.size();++i)std::cout<<"TILT_FIXTURE bank="<<bank<<" hz="<<probes[i]<<" live_db="<<dbOf(flat[i].live)<<" peak_db="<<dbOf(flat[i].peak)<<'\n';
    int compared=0;
    for(float tilt:{3.f,4.5f,6.f}) {
        panel.setAnalyzerTilt(tilt);check(panel.analyzerTilt()==tilt,"Analyzer tilt not applied");
        for(size_t i=0;i<probes.size();++i) {
            const float shift=tilt*std::log2(probes[i]/1000.f);closeTo(panel.analyzerTiltDB(probes[i]),shift,1.e-4f,"Tilt dB wrong");
            for(bool peak:{false,true}) {
                const float base=peak?flat[i].peak:flat[i].live,now=panel.drawnLineY(peak,1,xAt(probes[i]));
                check(std::isfinite(base) && std::isfinite(now),"Tilt fixture lines not built");
                if(!unclamped(dbOf(base)) || !unclamped(dbOf(base)+shift))continue;
                closeTo(dbOf(now)-dbOf(base),shift,.05f,"Tilted line moved by the wrong amount");++compared;
            }
            check(panel.readoutAt({xAt(probes[i]),plot.getCentreY()})==readouts[i],"Tilt changed the measured readout");
        }
    }
    check(compared>=12,("Too few unclamped tilt comparisons: "+juce::String(compared)).toRawUTF8());
    panel.setAnalyzerTilt(5.f);check(panel.analyzerTilt()==0.f,"Unsupported tilt was accepted");
    panel.applyViewMenuResult(202);check(panel.analyzerTilt()==4.5f,"Tilt menu result not applied");
    panel.applyViewMenuResult(299);panel.applyViewMenuResult(150);panel.applyViewMenuResult(0);
    check(panel.analyzerTilt()==4.5f && panel.frequencyViewIndex()==0 && panel.displayRange()==24.f,"Unknown menu result changed the view");
    juce::MemoryBlock after;processor.getStateInformation(after);check(before==after,"Analyzer tilt changed saved state");
    panel.setAnalyzerTilt(0);
    for(size_t i=0;i<probes.size();++i)closeTo(panel.drawnLineY(false,1,xAt(probes[i])),flat[i].live,.01f,"Flat tilt did not restore the line");
    panel.setSnapshotFrozen(false);panel.setPeakHold(false);
    std::cout<<"PASS EQ analyzer tilt flat/3/4.5/6 dB/oct (live and peak lines shift exactly, readout measured, menu guard, saved state unchanged) comparisons="<<compared<<" bank="<<bank<<'\n';
}
// Group selection and group editing (Issue #53): Ctrl/Cmd+click, box
// selection, Ctrl/Cmd+A and Escape; a group drag moves every member by the
// same frequency ratio and gain change as one undo step with one host gesture
// per field; wheel Q and Delete apply to the group as one undo step.
// Selecting alone never changes saved state.
void groupEditing(ChimeraProcessor& processor,GraphicalEQPanel& panel,int bank) {
    const std::array<float,4> hz{100.f,400.f,1600.f,6400.f},db{3.f,-3.f,6.f,0.f},types{0.f,0.f,0.f,3.f};
    for(int b=0;b<12;++b)set(processor,eqID(bank,b,"enabled"),0);
    for(int b=0;b<4;++b){set(processor,eqID(bank,b,"frequency"),hz[size_t(b)]);set(processor,eqID(bank,b,"gain"),db[size_t(b)]);
        set(processor,eqID(bank,b,"q"),1);set(processor,eqID(bank,b,"type"),types[size_t(b)]);set(processor,eqID(bank,b,"enabled"),1);}
    check(waitFor([&]{panel.selectBandForEditing(0);return panel.isBandVisible(3) && !panel.isBandVisible(4) && panel.selectedBand()==0;}),"Group fixture bands not visible");
    panel.collapseSelection();
    const auto plot=panel.graphBounds();const int command=juce::ModifierKeys::commandModifier;
    const auto click=[&](int band,int modifiers){const auto at=panel.visibleNodePosition(band);
        panel.mouseDown(mouseWith(panel,at,at,false,modifiers));panel.mouseUp(mouseWith(panel,at,at,false,modifiers));};
    const auto box=[&](juce::Point<float> from,juce::Point<float> to,int modifiers){
        panel.mouseDown(mouseWith(panel,from,from,false,modifiers));
        panel.mouseDrag(mouseWith(panel,(from+to)*.5f,from,true,modifiers));panel.mouseDrag(mouseWith(panel,to,from,true,modifiers));
        check(!panel.marqueeArea().isEmpty(),"Box selection was not drawn");
        panel.mouseUp(mouseWith(panel,to,from,true,modifiers));check(panel.marqueeArea().isEmpty(),"Box selection stayed after release");};
    const auto key=[&](int code,int modifiers){return panel.keyPressed(juce::KeyPress(code,juce::ModifierKeys(modifiers),0));};
    const auto values=[&](int b){return std::array<float,4>{get(processor,eqID(bank,b,"frequency")),get(processor,eqID(bank,b,"gain")),get(processor,eqID(bank,b,"q")),get(processor,eqID(bank,b,"enabled"))};};
    std::array<std::array<float,4>,4> original{};for(int b=0;b<4;++b)original[size_t(b)]=values(b);
    const auto unchanged=[&](int b,const char* message){const auto now=values(b);
        for(size_t i=0;i<4;++i)check(std::abs(now[i]-original[size_t(b)][i])<=std::max(.02f,std::abs(original[size_t(b)][i])*1.e-4f),message);};
    const auto undoOnce=[&](const char* message){check(key('z',command),"Undo shortcut not handled");settle(40);for(int b=0;b<4;++b)unchanged(b,message);};
    juce::MemoryBlock before;processor.getStateInformation(before);

    // Selection only: Ctrl/Cmd+click toggles, the last added band is the selected one.
    click(0,0);check(panel.selectionMask()==1 && panel.selectedBand()==0,"Plain click did not select one band");
    click(1,command);click(2,command);check(panel.selectionMask()==0b0111 && panel.selectedBand()==2,"Ctrl/Cmd+click did not add bands");
    click(2,command);check(panel.selectionMask()==0b0011 && panel.selectedBand()==0,"Removing the selected band did not pass selection on");
    click(2,command);check(panel.selectionMask()==0b0111 && panel.selectedBand()==2,"Ctrl/Cmd+click did not re-add a band");
    check(key('a',command) && panel.selectionMask()==0b1111,"Ctrl/Cmd+A did not select every band");
    check(key(juce::KeyPress::escapeKey,0) && panel.selectionCount()==1,"Escape did not collapse the group");
    const auto xAt=[&](float f){return plot.getX()+plot.getWidth()*std::log(f/20.f)/std::log(1000.f);};
    box({xAt(70),plot.getY()+5},{xAt(600),plot.getBottom()-5},0);check(panel.selectionMask()==0b0011,"Box did not select the bands inside it");
    box({xAt(1100),plot.getY()+5},{xAt(2500),plot.getBottom()-5},command);check(panel.selectionMask()==0b0111,"Ctrl/Cmd+box did not add to the group");
    const auto empty=juce::Point<float>{xAt(11000),plot.getY()+8};
    panel.mouseDown(mouse(panel,empty,empty,false));panel.mouseUp(mouse(panel,empty,empty,false));
    check(panel.selectionMask()==0b0111,"A click on empty graph changed the group");
    {   // Evidence image: three members ringed, band 3 plain, a box being drawn.
        const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile("eq-group-edit");directory.createDirectory();
        const juce::Point<float> a{xAt(3000),plot.getY()+10},b{xAt(15000),plot.getCentreY()+40};
        panel.mouseDown(mouse(panel,a,a,false));panel.mouseDrag(mouse(panel,b,a,true));
        saveImage(panel.createComponentSnapshot(panel.getLocalBounds()),directory.getChildFile("group_bank"+juce::String(bank)+".png"));
        panel.mouseUp(mouse(panel,b,a,true));check(panel.selectionMask()==0b1000,"Evidence box did not select band 4 alone");
        click(0,0);click(1,command);click(2,command);
        check(panel.selectionMask()==0b0111,"Evidence box did not restore the group");
    }
    juce::MemoryBlock selected;processor.getStateInformation(selected);check(before==selected,"Selecting bands changed saved state");
    for(int b=0;b<4;++b)unchanged(b,"Selecting bands changed a value");

    // Group drag from a member: same ratio (x2) and gain change (+4 dB) for all.
    HostEvents events;std::vector<juce::AudioProcessorParameter*> listened;
    for(int b=0;b<4;++b)for(const char* field:{"frequency","gain"}){auto* parameter=processor.parameters().getParameter(eqID(bank,b,field));parameter->addListener(&events);listened.push_back(parameter);}
    const auto start=panel.visibleNodePosition(1);const juce::Point<float> travel{plot.getWidth()*std::log(2.f)/std::log(1000.f),-plot.getHeight()*4.f/48.f};
    panel.mouseDown(mouse(panel,start,start,false));check(panel.selectionMask()==0b0111 && panel.selectedBand()==1,"Pressing a member dropped the group");
    for(int step=1;step<=4;++step)panel.mouseDrag(mouse(panel,start+travel*(float(step)/4.f),start,true));
    panel.mouseUp(mouse(panel,start+travel,start,true));
    for(auto* parameter:listened)parameter->removeListener(&events);
    for(int b=0;b<3;++b){
        closeTo(get(processor,eqID(bank,b,"frequency")),hz[size_t(b)]*2,hz[size_t(b)]*.004f,"Group drag ratio wrong");
        closeTo(get(processor,eqID(bank,b,"gain")),db[size_t(b)]+4,.05f,"Group drag gain change wrong");
    }
    unchanged(3,"Group drag moved a band outside the group");
    check(events.starts==6 && events.ends==6,("Group drag was not one gesture per field: starts="+juce::String(events.starts)+" ends="+juce::String(events.ends)).toRawUTF8());
    undoOnce("Group drag was not undone in one step");
    check(panel.selectionMask()==0b0111,"Undo dropped the group");

    // A member at the range edge stops there; the others keep moving. HP has no gain.
    check(key('a',command),"Ctrl/Cmd+A failed");const auto from=panel.visibleNodePosition(0);
    const juce::Point<float> reach{plot.getWidth()*std::log(4.f)/std::log(1000.f),-plot.getHeight()*2.f/48.f};
    panel.mouseDown(mouse(panel,from,from,false));panel.mouseDrag(mouse(panel,from+reach*.5f,from,true));panel.mouseDrag(mouse(panel,from+reach,from,true));panel.mouseUp(mouse(panel,from+reach,from,true));
    closeTo(get(processor,eqID(bank,3,"frequency")),20000,1,"Group drag did not stop a member at 20 kHz");
    closeTo(get(processor,eqID(bank,0,"frequency")),400,1.6f,"Group drag did not keep moving the others");
    closeTo(get(processor,eqID(bank,3,"gain")),0,.01f,"Group drag changed the gain of a filter without gain");
    undoOnce("Edge-limited group drag was not undone in one step");
    // Alt keeps the group to the axis it first moves along.
    const auto alt=panel.visibleNodePosition(0);
    panel.mouseDown(mouseWith(panel,alt,alt,false,juce::ModifierKeys::altModifier));
    panel.mouseDrag(mouseWith(panel,alt+juce::Point<float>(30,-4),alt,true,juce::ModifierKeys::altModifier));
    panel.mouseDrag(mouseWith(panel,alt+juce::Point<float>(60,-8),alt,true,juce::ModifierKeys::altModifier));
    panel.mouseUp(mouseWith(panel,alt+juce::Point<float>(60,-8),alt,true,juce::ModifierKeys::altModifier));
    for(int b=0;b<4;++b)closeTo(get(processor,eqID(bank,b,"gain")),db[size_t(b)],.01f,"Alt group drag changed gain");
    check(get(processor,eqID(bank,1,"frequency"))>hz[1]*1.05f,"Alt group drag did not move frequency");
    undoOnce("Alt group drag was not undone in one step");

    // Wheel Q on a group: the same factor for every member, one undo step.
    check(key(juce::KeyPress::escapeKey,0),"Escape failed");click(0,0);click(1,command);click(2,command);
    juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;const auto over=panel.visibleNodePosition(2)+juce::Point<float>(0,30);
    panel.mouseWheelMove(mouse(panel,over,over,false),wheel);
    for(int b=0;b<3;++b)closeTo(get(processor,eqID(bank,b,"q")),std::exp(.3f),.01f,"Group wheel Q wrong");
    closeTo(get(processor,eqID(bank,3,"q")),1,.001f,"Group wheel Q changed a band outside the group");
    undoOnce("Group wheel Q was not undone in one step");

    // Delete a group as one step; undo brings every band back.
    check(key(juce::KeyPress::escapeKey,0),"Escape failed");click(1,0);click(2,command);check(panel.selectionMask()==0b0110,"Delete fixture selection wrong");
    check(key(juce::KeyPress::deleteKey,0),"Delete not handled");
    check(waitFor([&]{return panel.isBandVisible(0) && !panel.isBandVisible(1) && !panel.isBandVisible(2) && panel.isBandVisible(3);}),"Group delete did not remove exactly the group");
    check(get(processor,eqID(bank,1,"enabled"))==0 && get(processor,eqID(bank,2,"enabled"))==0,"Group delete left a band audible");
    check(panel.selectionCount()==1,"Group delete left a stale group");
    check(key('z',command) && waitFor([&]{return panel.isBandVisible(1) && panel.isBandVisible(2);}),"Group delete was not undone in one step");
    for(int b=0;b<4;++b)unchanged(b,"Group delete undo did not restore values");
    check(key('z',command|juce::ModifierKeys::shiftModifier) && waitFor([&]{return !panel.isBandVisible(1) && !panel.isBandVisible(2) && panel.isBandVisible(0);}),"Group delete redo failed");
    check(key('z',command) && waitFor([&]{return panel.isBandVisible(1) && panel.isBandVisible(2);}),"Second group delete undo failed");

    // The top DELETE button removes the group, like the Delete key (one undo step).
    key(juce::KeyPress::escapeKey,0);click(1,0);click(2,command);
    auto* deleteButton=find<juce::TextButton>(panel,eqID(bank,-1,"deleteBand"));check(deleteButton!=nullptr,"Delete button missing");
    deleteButton->triggerClick();
    check(waitFor([&]{return panel.isBandVisible(0) && !panel.isBandVisible(1) && !panel.isBandVisible(2) && panel.isBandVisible(3);}),"The DELETE button did not remove exactly the group");
    check(key('z',command) && waitFor([&]{return panel.isBandVisible(1) && panel.isBandVisible(2);}),"The DELETE button's group removal was not one undo step");
    // The selected band disappears (deleted elsewhere): selection passes to a
    // remaining group member, never to a band outside the group.
    click(1,0);click(2,command);check(panel.selectionMask()==0b0110 && panel.selectedBand()==2,"Vanishing fixture selection wrong");
    check(processor.eqEditing().removeBand(bank,2),"Fixture removal failed");
    check(waitFor([&]{return !panel.isBandVisible(2) && panel.selectedBand()==1;}),"Selection did not pass to the remaining group member");
    check(panel.selectionMask()==0b0010,("A band outside the group joined it: mask "+juce::String(panel.selectionMask())).toRawUTF8());
    check(key('z',command) && waitFor([&]{return panel.isBandVisible(2);}),"Fixture removal undo failed");
    for(int b=0;b<4;++b)unchanged(b,"Vanishing-selection fixture changed a value");
    // Esc during a group drag ends the drag: later pointer moves write nothing.
    click(0,0);click(1,command);
    const auto grab=panel.visibleNodePosition(1);
    panel.mouseDown(mouse(panel,grab,grab,false));panel.mouseDrag(mouse(panel,grab+juce::Point<float>(20,-10),grab,true));
    const auto atEsc=std::array<float,2>{get(processor,eqID(bank,0,"frequency")),get(processor,eqID(bank,1,"frequency"))};
    check(key(juce::KeyPress::escapeKey,0),"Escape during a group drag not handled");
    panel.mouseDrag(mouse(panel,grab+juce::Point<float>(80,-40),grab,true));panel.mouseUp(mouse(panel,grab+juce::Point<float>(80,-40),grab,true));
    check(get(processor,eqID(bank,0,"frequency"))==atEsc[0] && get(processor,eqID(bank,1,"frequency"))==atEsc[1],"A group drag kept writing after Escape");
    undoOnce("The drag up to Escape was not one undo step");
    // A plain click outside the group, or the band chooser, starts over.
    check(panel.selectionCount()==1 && !key(juce::KeyPress::escapeKey,0),"Escape with one band selected was consumed");
    click(0,0);click(1,command);check(panel.selectionMask()==0b0011,"Two-band fixture wrong");
    click(3,0);check(panel.selectionMask()==0b1000,"Plain click on a non-member kept the group");
    check(key('a',command),"Ctrl/Cmd+A failed");click(3,0);check(panel.selectionMask()==0b1111,"Plain click on a member dropped the group");
    auto* chooser=find<juce::ComboBox>(panel,eqID(bank,-1,"band"));check(chooser!=nullptr,"Band chooser missing");
    chooser->setSelectedId(2,juce::sendNotificationSync);check(panel.selectionMask()==0b0010,"Band chooser kept the group");
    std::cout<<"PASS EQ group selection (Ctrl/Cmd+click, box, Ctrl/Cmd+A, Escape) and group drag/edge/Alt, wheel Q and delete as single undo steps bank="<<bank<<'\n';
}
void editingWorkflow(ChimeraProcessor& processor,GraphicalEQPanel& panel,int bank) {
    auto* selector=find<juce::ComboBox>(panel,eqID(bank,-1,"band"));
    auto* add=find<juce::TextButton>(panel,eqID(bank,-1,"addBand"));
    auto* remove=find<juce::TextButton>(panel,eqID(bank,-1,"deleteBand"));
    auto* undo=find<juce::TextButton>(panel,eqID(bank,-1,"undo"));
    auto* redo=find<juce::TextButton>(panel,eqID(bank,-1,"redo"));
    check(selector && add && remove && undo && redo,"October 23 editing controls missing");
    check(selector->getNumItems()==0 && panel.selectedBand()==-1 && !remove->isEnabled(),"New EQ has phantom bands");
    const auto creation=panel.graphBounds().getCentre()+juce::Point<float>(30,-25);
    panel.mouseDoubleClick(mouse(panel,creation,creation,false));
    check(panel.isBandVisible(0) && panel.selectedBand()==0 && get(processor,eqID(bank,0,"enabled"))==1,"Graph creation failed");
    check(selector->getNumItems()==1 && get(processor,eqID(bank,0,"gain"))>0,"Graph creation lost gain or selector occupancy");
    auto* on=find<juce::TextButton>(panel,eqID(bank,0,"enabled"));on->triggerClick();
    check(waitFor([&]{return get(processor,eqID(bank,0,"enabled"))==0;}),"Bypass band failed");
    check(panel.isBandVisible(0) && selector->getNumItems()==1,"Bypass incorrectly deleted band");
    undo->triggerClick();check(waitFor([&]{return get(processor,eqID(bank,0,"enabled"))==1;}),"Undo band bypass failed");

    auto* frequency=find<juce::Slider>(panel,eqID(bank,0,"frequency"));
    const float previous=get(processor,eqID(bank,0,"frequency"));
    HostEvents numberEvents;auto* parameter=processor.parameters().getParameter(eqID(bank,0,"frequency"));parameter->addListener(&numberEvents);
    numeric(*frequency,"1735.25 Hz");parameter->removeListener(&numberEvents);
    check(std::abs(get(processor,eqID(bank,0,"frequency"))-1735.25f)<.02f,"Numeric commit failed");
    check(numberEvents.starts==1 && numberEvents.ends==1,"Numeric input was not one balanced host gesture");
    undo->triggerClick();check(waitFor([&]{return std::abs(get(processor,eqID(bank,0,"frequency"))-previous)<.02f;}),"Numeric undo failed");
    redo->triggerClick();check(waitFor([&]{return std::abs(get(processor,eqID(bank,0,"frequency"))-1735.25f)<.02f;}),"Numeric redo failed");

    const auto start=panel.nodePosition(0);const float gain=get(processor,eqID(bank,0,"gain"));
    HostEvents dragEvents;parameter->addListener(&dragEvents);
    auto* gainParameter=processor.parameters().getParameter(eqID(bank,0,"gain"));gainParameter->addListener(&dragEvents);
    panel.mouseDown(mouse(panel,start,start,false));
    for(int step=1;step<=4;++step)panel.mouseDrag(mouse(panel,start+juce::Point<float>(float(step*10),float(-step*3)),start,true));
    panel.mouseUp(mouse(panel,start+juce::Point<float>(40,-12),start,true));
    parameter->removeListener(&dragEvents);gainParameter->removeListener(&dragEvents);
    check(dragEvents.starts==2 && dragEvents.ends==2,"Graph drag did not keep one gesture per field");
    undo->triggerClick();check(waitFor([&]{return std::abs(get(processor,eqID(bank,0,"frequency"))-1735.25f)<.02f && std::abs(get(processor,eqID(bank,0,"gain"))-gain)<.02f;}),"Graph drag undo did not restore both axes in one step");
    check(panel.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)),"Delete shortcut not handled");
    check(!panel.isBandVisible(0) && selector->getNumItems()==0 && get(processor,eqID(bank,0,"enabled"))==0,"Delete left a phantom/audible band");
    undo->triggerClick();check(waitFor([&]{return panel.isBandVisible(0) && get(processor,eqID(bank,0,"enabled"))==1;}),"Undo deletion failed");
    redo->triggerClick();check(waitFor([&]{return !panel.isBandVisible(0) && selector->getNumItems()==0;}),"Redo deletion failed");
    add->triggerClick();check(waitFor([&]{return panel.selectedBand()==0 && selector->getNumItems()==1;}),"Deleted first slot was not reused");
    const auto reusedFrequency=get(processor,eqID(bank,0,"frequency"));
    std::cout<<"MEASURE reused EQ node bank="<<bank<<" frequency="<<juce::String(reusedFrequency,6)<<" gain="<<juce::String(get(processor,eqID(bank,0,"gain")),6)<<'\n';
    // Normalized float round-trips can represent 0 dB as about -1e-6 dB
    // on ARM. Require the exact canonical host value as well as physical
    // neutrality, rather than an unattainable literal zero on that platform.
    auto* reusedGainParameter=processor.parameters().getParameter(eqID(bank,0,"gain"));
    check(reusedGainParameter!=nullptr,"Reused gain parameter missing");
    const auto neutralGain=reusedGainParameter->convertFrom0to1(reusedGainParameter->convertTo0to1(0.f));
    check(std::abs(reusedFrequency-1000.f)<.02f
          && get(processor,eqID(bank,0,"gain"))==neutralGain && std::abs(neutralGain)<1.e-5f,
          "Reused slot did not start neutral");
    for(int band=1;band<12;++band){add->triggerClick();check(waitFor([&]{return selector->getNumItems()==band+1;}),"Add did not use the next persistent slot");}
    check(!add->isEnabled(),"Full EQ did not disable Add");
    const auto mask=processor.eqEditing().occupiedMask(bank);
    panel.mouseDoubleClick(mouse(panel,creation,creation,false));
    check(processor.eqEditing().occupiedMask(bank)==mask && selector->getNumItems()==12,"Full graph creation changed existing slots");
    // The original 24-control regression starts with occupied but bypassed
    // bands. Preserve its activation, filter, automation and gesture assertions.
    for(int band=0;band<12;++band)set(processor,eqID(bank,band,"enabled"),0);
    settle(50);
    std::cout<<"PASS native graph add/delete / disabled node visibility / numeric and drag undo-redo / persistent reuse / capacity bank="<<bank<<'\n';
}
void liveAnalyzer(ChimeraProcessor& processor,ChimeraEditor& editor,const juce::File& directory) {
    std::ofstream timing(directory.getChildFile("eq-analyzer-native-ui.csv").getFullPathName().toStdString());
    timing<<"bank,fft_size,rate,bin_spacing_hz,window_ms,first_complete_data_observed_ms,first_component_render_complete_ms,updates,fft_p50_us,fft_p99_us,update_p50_us,update_p99_us,path_p50_us,path_p99_us,panel_paint_p50_us,panel_paint_p99_us,snapshot_p50_us,snapshot_p99_us,native_interval_p50_ms,native_interval_p99_ms,process_cpu_with_snapshot_percent\n";
    SyntheticAudio audio(processor);
    for(int e=0;e<2;++e) {
        auto* bank=find<juce::TextButton>(editor,e==0?"path.tone.eq":"path.final.eq");check(bank,"Selected-bank controls missing");bank->triggerClick();
        auto* panel=find<GraphicalEQPanel>(editor,e==0?"toneEQ_panel":"finalEQ_panel");
        auto* other=find<GraphicalEQPanel>(editor,e==0?"finalEQ_panel":"toneEQ_panel");
        check(panel && other && waitFor([&]{return panel->isShowing() && !other->isShowing();}),"Exactly one selected EQ bank must be visible");
        check(panel->getWidth()>=1100 && panel->graphBounds().getWidth()>1000 && panel->graphBounds().getHeight()>200,
              "Selected EQ did not receive the large editing surface");
        check(waitFor([&]{return processor.graphicalEQ(e).analyzer.enabled.load() && !processor.graphicalEQ(1-e).analyzer.enabled.load();}),
              "Hidden bank still captures audio");
        const auto hiddenFrames=other->fftFrames();
        auto* resolution=find<juce::ComboBox>(*panel,eqID(e,-1,"resolution"));
        auto* smoothing=find<juce::ComboBox>(*panel,eqID(e,-1,"smoothing"));
        check(resolution && smoothing,"Analyzer quality controls missing");
        for(int choice=1;choice<=3;++choice) {
            const int size=1<<(choice+10),bin=43*(size/2048);
            const double started=juce::Time::getMillisecondCounterHiRes();const auto before=panel->fftFrames();
            resolution->setSelectedId(choice,juce::sendNotificationSync);
            check(panel->analysisFFTSize()==size,"Resolution selector did not reach the actual FFT");
            check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>before;}),"Selected FFT did not react to the synthetic audio callback");
            const double reaction=juce::Time::getMillisecondCounterHiRes()-started;
            const auto initialRender=panel->createComponentSnapshot(panel->getLocalBounds());check(initialRender.isValid(),"First native component render failed");
            const double rendered=juce::Time::getMillisecondCounterHiRes()-started;
            settle(200);
            check(std::abs(panel->spectrumDB(false,bin)+20)<.1,"Native input FFT canceled stereo anti-phase or is not calibrated");
            check(std::abs(panel->spectrumDB(true,bin)+14)<.15,"Native FFT did not measure the actual6dB EQ output");
            std::vector<double> fft,update,path,paint,capture,interval;
            uint64_t previousFrames=panel->fftFrames();double lastFrame=juce::Time::getMillisecondCounterHiRes();
            const auto cpu=processCPUSeconds(),wall=juce::Time::getMillisecondCounterHiRes();
            while(fft.size()<100) {
                check(waitFor([&]{return panel->fftFrames()>previousFrames;}),"Live native FFT stalled");
                const auto now=juce::Time::getMillisecondCounterHiRes();interval.push_back(now-lastFrame);lastFrame=now;previousFrames=panel->fftFrames();
                fft.push_back(panel->lastFFTTimeMicros());update.push_back(panel->analysisUpdateMicros());path.push_back(panel->spectrumPathMicros());
                const auto captureStart=juce::Time::getMillisecondCounterHiRes();
                const auto image=panel->createComponentSnapshot(panel->getLocalBounds());check(image.isValid(),"Native rendered surface is invalid");
                capture.push_back(1000*(juce::Time::getMillisecondCounterHiRes()-captureStart));paint.push_back(panel->lastPaintMicros());
            }
            const double processCPU=100000*(processCPUSeconds()-cpu)/(juce::Time::getMillisecondCounterHiRes()-wall);
            timing<<e<<','<<size<<",48000,"<<48000./size<<','<<1000.*size/48000<<','<<reaction<<','<<rendered<<','<<fft.size()<<','
                  <<percentile(fft,.5)<<','<<percentile(fft,.99)<<','<<percentile(update,.5)<<','<<percentile(update,.99)<<','
                  <<percentile(path,.5)<<','<<percentile(path,.99)<<','<<percentile(paint,.5)<<','<<percentile(paint,.99)<<','
                  <<percentile(capture,.5)<<','<<percentile(capture,.99)<<','<<percentile(interval,.5)<<','<<percentile(interval,.99)<<','<<processCPU<<'\n';
            snapshot(editor,directory.getChildFile("eq-ui-bank"+juce::String(e)+"-fft"+juce::String(size)+".png"));
            std::cout<<"NATIVE synthetic audio bank="<<e<<" FFT="<<size<<" input="<<panel->spectrumDB(false,bin)
                     <<" output="<<panel->spectrumDB(true,bin)<<" first_data_observed="<<reaction<<" ms paint_p50="<<percentile(paint,.5)<<" us\n";
        }
        check(other->fftFrames()==hiddenFrames,"Hidden EQ performed spectrum transforms");
        for(int choice=1;choice<=3;++choice) {
            const auto frames=panel->fftFrames();smoothing->setSelectedId(choice,juce::sendNotificationSync);
            check(panel->analyzerHasData() && panel->fftFrames()==frames,"Smoothing change reset a live observation");
        }
        smoothing->setSelectedId(2,juce::sendNotificationSync);
        auto* input=find<juce::TextButton>(*panel,eqID(e,-1,"inputSpectrum"));
        auto* output=find<juce::TextButton>(*panel,eqID(e,-1,"outputSpectrum"));
        auto* master=find<juce::TextButton>(*panel,eqID(e,-1,"analyzer"));check(input && output && master,"Independent IN/OUT/LIVE controls missing");
        auto* freeze=find<juce::TextButton>(*panel,eqID(e,-1,"freeze"));
        // Cursor readout at the 1007.8 Hz test tone: it may report only a
        // spectrum layer the user has shown (OUT first, else IN, else none).
        const auto readout=[&]{const auto p=panel->graphBounds();
            return panel->readoutAt({p.getX()+p.getWidth()*float(std::log(1007.8125/20)/std::log(1000.)),p.getCentreY()});};
        const auto readoutLevel=[](const juce::String& text,const char* label){return text.fromFirstOccurrenceOf(label,false,false).getFloatValue();};
        const auto expectReadout=[&](const char* label,const char* stage){
            const auto text=readout();const bool none=label==nullptr;
            const bool ok=text.contains("kHz") && text.contains(" dB") && (none
                ? !text.contains("dBFS") && !text.contains("OUT ") && !text.contains("IN ")
                : text.contains(juce::String(label)+" ") && text.contains("dBFS") && (juce::String(label)=="OUT"?!text.contains("IN "):!text.contains("OUT ")));
            if(!ok){std::cerr<<"READOUT bank="<<e<<" stage="<<stage<<" text="<<text<<'\n';check(false,"Cursor readout does not match the visible IN/OUT/LIVE/Freeze state");}
            return text;
        };
        // triggerClick() is asynchronous: wait for the toggle to change.
        const auto toggle=[&](juce::TextButton* button){const bool was=button->getToggleState();button->triggerClick();
            check(waitFor([&]{return button->getToggleState()!=was;}),"Spectrum layer toggle did not change");};
        check(input->getToggleState() && output->getToggleState() && master->getToggleState(),"Readout fixture does not start with LIVE, IN and OUT on");
        check(freeze && waitFor([&]{return freeze->isEnabled() && panel->analyzerHasData();}),"Snapshot Freeze did not become available");
        expectReadout("OUT","live both");
        freeze->triggerClick();check(waitFor([&]{return panel->isSnapshotFrozen();}),"Freeze button did not hold a frame");
        // Frozen: the readout reads the held frame, so it stays fixed, and it
        // still follows the IN/OUT visibility of that frame.
        const auto frozenReadout=expectReadout("OUT","frozen both");
        toggle(output);expectReadout("IN","frozen in only");
        toggle(input);expectReadout(nullptr,"frozen neither");
        toggle(output);expectReadout("OUT","frozen out only");
        toggle(input);
        const auto frozenFrames=panel->fftFrames();
        std::vector<float> frozen;
        for(int stream=0;stream<2;++stream)for(int bin=0;bin<=panel->analysisFFTSize()/2;++bin)frozen.push_back(panel->displaySpectrumDB(stream==1,bin));
        check(!processor.graphicalEQ(e).analyzer.enabled.load() && !resolution->isEnabled() && !smoothing->isEnabled() && !master->isEnabled(),"Frozen analyzer still captures or exposes changing frame settings");
        settle(180);
        find<juce::TextButton>(editor,e==0?"path.final.eq":"path.tone.eq")->triggerClick();settle(60);
        bank->triggerClick();check(waitFor([&]{return panel->isShowing();}),"Frozen bank could not reopen");
        check(panel->fftFrames()==frozenFrames && panel->isSnapshotFrozen(),"Freeze transformed data or was lost across bank navigation");
        size_t frozenIndex=0;for(int stream=0;stream<2;++stream)for(int bin=0;bin<=panel->analysisFFTSize()/2;++bin)
            check(panel->displaySpectrumDB(stream==1,bin)==frozen[frozenIndex++],"Frozen visible spectrum decayed or changed");
        check(expectReadout("OUT","frozen after navigation")==frozenReadout,"Frozen cursor readout changed while the frame was held");
        snapshot(editor,directory.getChildFile("eq-ui-freeze-bank"+juce::String(e)+".png"));
        // Trigger directly on the message thread to inspect the immediate thaw
        // boundary before the synthetic producer can complete a fresh window.
        panel->setSnapshotFrozen(false);
        check(!panel->analyzerHasData() && !panel->isSnapshotFrozen(),"Thaw displayed an old snapshot before fresh audio");
        check(!panel->isWaitingForFreshCapture() || !processor.graphicalEQ(e).analyzer.enabled.load(),"Thaw captured before its callback boundary was acknowledged");
        check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>frozenFrames;}),"Thaw did not resume from new audio");
        std::cout<<"PASS native Snapshot Freeze / exact bins / capture off / hidden bank / fresh thaw bank="<<e<<'\n';
        // PNG file encoding above is outside the timing interval and may block
        // the consumer long enough to trigger its correct overflow recovery.
        // Compare visible layers only after a fresh complete observation.
        const auto beforeLayers=panel->fftFrames();
        check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>beforeLayers;}),"Layer comparison did not recover a fresh audio observation after PNG encoding");
        const auto layerState=[&](const char* name){std::cout<<"NATIVE layer state bank="<<e<<' '<<name<<" hasData="<<panel->analyzerHasData()<<" frames="<<panel->fftFrames()<<" dropped="<<processor.graphicalEQ(e).analyzer.dropped.load()<<'\n';};
        layerState("both");
        const auto both=panel->createComponentSnapshot(panel->getLocalBounds());
        const float outLevel=readoutLevel(expectReadout("OUT","live both"),"OUT ");
        output->triggerClick();check(waitFor([&]{return !output->getToggleState() && panel->analyzerHasData();}),"OUT toggle did not retain a valid audio observation");
        const float inLevel=readoutLevel(expectReadout("IN","live in only"),"IN ");
        // The band at the tone is +6 dB, so OUT must read clearly above IN:
        // the readout is not swapping or mislabelling the two spectra.
        if(!(outLevel-inLevel>3 && outLevel-inLevel<9)){std::cerr<<"READOUT bank="<<e<<" OUT="<<outLevel<<" IN="<<inLevel<<'\n';check(false,"Readout OUT/IN levels do not reflect the +6 dB EQ");}
        layerState("in_only");
        const auto inOnly=panel->createComponentSnapshot(panel->getLocalBounds());
        check(spectrumPixels(both,panel->graphBounds().toNearestInt(),true)>spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),true)+30,
              "OUT visibility control did not remove the real output spectrum layer");
        input->triggerClick();check(waitFor([&]{return !input->getToggleState() && panel->analyzerHasData();}),"IN toggle did not retain a valid audio observation");
        layerState("neither");
        check(panel->analyzerHasData(),"Readout fixture lost analyzer data with both layers hidden");
        expectReadout(nullptr,"live neither");
        const auto neither=panel->createComponentSnapshot(panel->getLocalBounds());
        std::cout<<"NATIVE layer pixels bank="<<e<<" OUT both="<<spectrumPixels(both,panel->graphBounds().toNearestInt(),true)
                 <<" in_only="<<spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),true)
                 <<" IN in_only="<<spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),false)
                 <<" neither="<<spectrumPixels(neither,panel->graphBounds().toNearestInt(),false)<<'\n';
        check(spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),false)>spectrumPixels(neither,panel->graphBounds().toNearestInt(),false)+30,
              "IN visibility control did not remove the real input spectrum layer");
        saveImage(both,directory.getChildFile("eq-ui-toggle-bank"+juce::String(e)+"-both.png"));
        saveImage(inOnly,directory.getChildFile("eq-ui-toggle-bank"+juce::String(e)+"-in-only.png"));
        saveImage(neither,directory.getChildFile("eq-ui-toggle-bank"+juce::String(e)+"-neither.png"));
        toggle(output);expectReadout("OUT","live out only");
        toggle(input);expectReadout("OUT","live both again");
        master->triggerClick();check(waitFor([&]{return !processor.graphicalEQ(e).analyzer.enabled.load();}),"LIVE off still captures audio");
        expectReadout(nullptr,"analyzer off");
        const auto stoppedFrames=panel->fftFrames();settle(180);check(panel->fftFrames()==stoppedFrames,"LIVE off still transforms samples");
        master->triggerClick();check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>stoppedFrames;}),"LIVE re-enable did not acquire fresh input");
    }
    check(timing.good(),"Native timing evidence could not be written");

    auto* panel=find<GraphicalEQPanel>(editor,"finalEQ_panel");
    auto* resolution=find<juce::ComboBox>(*panel,eqID(1,-1,"resolution"));resolution->setSelectedId(2,juce::sendNotificationSync);
    const auto before=panel->fftFrames();audio.frequency.store(3000);
    check(waitFor([&]{return panel->fftFrames()>before && panel->spectrumDB(false,256)>-20.2f && panel->spectrumDB(false,86)<-70;}),"Native analyzer did not follow a new real input frequency");
    check(panel->spectrumDB(false,86)<-70,"Native raw spectrum retained the old frequency");
    check(audio.callbacks.load()>100,"Native proof did not run real paced audio callbacks");
    // Capture the visible peak before stopping. A loaded native dispatcher
    // may satisfy both requested waits in one long dispatch (observed on macOS),
    // so sampling the comparison point after the drain can already see -120 dB.
    // The proof is decay from the last live observation, with no fabricated FFT.
    const int peakBin=256;
    const float displayAtStop=panel->displaySpectrumDB(false,peakBin);
    audio.stop();const auto atStop=panel->fftFrames();
    const uint64_t pendingAtStop=processor.graphicalEQ(1).analyzer.pendingSamples();
    const uint64_t hop=uint64_t(panel->analysisFFTSize()/4);
    const uint64_t maximumDrainWindows=(pendingAtStop+hop-1)/hop+1;
    // Wait for conditions, not fixed durations: a loaded runner can stall the
    // UI timer for over a second, leaving the drain or idle decay unfinished.
    auto& fifo=processor.graphicalEQ(1).analyzer;
    check(waitFor([&]{return fifo.pendingSamples()==0;}),"Stopped producer backlog was not consumed");
    settle(20);
    const auto drained=panel->fftFrames();
    check(drained<=atStop+maximumDrainWindows,"Stopped producer triggered unbounded catch-up FFT work");
    // Frames never decrease, so a fabricated frame keeps this false until the deadline.
    check(waitFor([&]{return panel->fftFrames()==drained && panel->spectrumDB(false,peakBin)<=-119.9f
                             && panel->displaySpectrumDB(false,peakBin)<displayAtStop;}),
          "Stopped audio did not decay on the native screen without fabricated FFT frames");
    settle(100);
    check(panel->fftFrames()==drained,"Stopped audio produced FFT frames after decaying");
    snapshot(editor,directory.getChildFile("eq-ui-stopped-source.png"));
    std::cout<<"PASS native synthetic anti-phase sine / independent IN-OUT layers / master-off / stopped-source decay\n";

    // The motion proof now drives the entire production processBlock route, so
    // the product's global audio and CPU meters react alongside the real FFT.
    // Keep identical-polarity stereo here; anti-phase calibration was tested
    // independently above and must not rely on other amp/cab routing choices.
    processor.prepareToPlay(48000,128);
    auto* master=find<juce::TextButton>(*panel,eqID(1,-1,"analyzer"));
    master->triggerClick();check(waitFor([&]{return !processor.graphicalEQ(1).analyzer.enabled.load();}),"Motion capture reset did not stop the analyzer");
    master->triggerClick();check(waitFor([&]{return processor.graphicalEQ(1).analyzer.enabled.load();}),"Motion capture reset did not enable the analyzer");
    SyntheticAudio fullRoute(processor,true,48000,false);fullRoute.pluck.store(true);fullRoute.frequency.store(62.5);
    check(waitFor([&]{return panel->analyzerHasData() && processor.inputMeter()>.005f && processor.outputMeter()>1.e-5f;}),"Full-processor motion proof has no actual input/output audio");
    std::ofstream frames(directory.getChildFile("eq-analyzer-live-frames.csv").getFullPathName().toStdString());
    frames<<"frame,capture_start_ms,requested_frequency_hz,fft_size,fft_batches,rate,callback_samples,pluck_retrigger_s,pluck_decay_s,fixture,amplitude,right_gain,input_peak,output_peak\n";
    std::vector<juce::Image> liveFrames;liveFrames.reserve(32);
    const double sequenceStart=juce::Time::getMillisecondCounterHiRes();
    for(int frame=0;frame<32;++frame) {
        const double hz=frame<11?62.5:frame<22?187.5:750.;fullRoute.frequency.store(hz);settle(40);
        const double captured=juce::Time::getMillisecondCounterHiRes()-sequenceStart;
        check(panel->analyzerHasData(),"Full-processor live capture lost its audio observation");
        liveFrames.push_back(editor.createComponentSnapshot(editor.getLocalBounds()));check(liveFrames.back().isValid(),"Full-processor native capture failed");
        frames<<frame<<','<<captured<<','<<hz<<','<<panel->analysisFFTSize()<<','<<panel->fftFrames()
              <<",48000,128,0.65,0.18,synthetic_full_processBlock_harmonic_pluck,0.1,1,"<<processor.inputMeter()<<','<<processor.outputMeter()<<'\n';
    }
    fullRoute.stop();check(fullRoute.callbacks.load()>100,"Full-route proof did not produce real paced audio");
    check(frames.good(),"Live capture timestamps were not written");
    // Encoding is deliberately after the live observation and stop checks:
    // evidence generation must not manufacture UI starvation or FIFO drops.
    for(int frame=0;frame<int(liveFrames.size());++frame)
        saveImage(liveFrames[(size_t)frame],directory.getChildFile("eq-live-"+juce::String(frame).paddedLeft('0',3)+".png"));
    std::cout<<"PASS32 changing harmonic-pluck native frames through the full processBlock route / live input-output meters\n";

}
void fullProcessorNativeTiming(ChimeraProcessor& processor,ChimeraEditor& editor,const juce::File& directory) {
    processor.prepareToPlay(48000,128);
    auto* panel=find<GraphicalEQPanel>(editor,"finalEQ_panel");
    auto* resolution=find<juce::ComboBox>(*panel,eqID(1,-1,"resolution"));
    auto* master=find<juce::TextButton>(*panel,eqID(1,-1,"analyzer"));
    std::ofstream output(directory.getChildFile("eq-analyzer-full-callback.csv").getFullPathName().toStdString());
    output<<"capture,fft_size,rate,block,channels,mode,latency_samples,callbacks,whole_processor_p50_us,whole_processor_p99_us,whole_processor_max_us,budget_us,overruns,total_callbacks,minimum_capture_ms,capture_timeout_ms,observed_elapsed_ms,callbacks_at_minimum_capture,effective_samples_per_second,production_ratio,sample_target,target_reached,minimum_sample_coverage,within_5pct_nominal_rate,performance_acceptance\n";
    for(int size:{0,2048,4096,8192}) {
        const bool enabled=size!=0;
        if(master->getToggleState()!=enabled){master->triggerClick();settle(50);}
        if(enabled)resolution->setSelectedId(size==2048?1:size==4096?2:3,juce::sendNotificationSync);
        // A normal std::thread on a hosted runner has no audio scheduling
        // guarantee. Collect the required sample count within a hard timeout;
        // retain raw wall-clock throughput so a slow producer cannot masquerade
        // as successful real-time audio. Sample coverage is not a CPU verdict.
        constexpr uint64_t sampleTarget=600;
        constexpr double minimumCaptureMs=1800,maximumCaptureMs=8000;
        const auto observedStart=std::chrono::steady_clock::now();
        const auto elapsedMs=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-observedStart).count();};
        SyntheticAudio audio(processor,true);uint64_t callbacksAtMinimum=0;bool minimumObserved=false;
        while(elapsedMs()<maximumCaptureMs) {
            if(elapsedMs()>=minimumCaptureMs) {
                if(!minimumObserved){callbacksAtMinimum=audio.callbacks.load();minimumObserved=true;}
                if(audio.callbacks.load()>=sampleTarget)break;
            }
            settle(20);
        }
        audio.stop();const double observedElapsed=elapsedMs();
        const auto totalCallbacks=audio.callbacks.load();
        if(!minimumObserved)callbacksAtMinimum=totalCallbacks;
        const auto& all=audio.callbackMicrosAfterStop();
        std::vector<double> samples;if(all.size()>100)samples.assign(all.begin()+100,all.end());
        const double budget=128./48000*1.e6;
        const auto overruns=std::count_if(samples.begin(),samples.end(),[budget](double value){return value>budget;});
        const double effectiveRate=1000*double(totalCallbacks)*128/observedElapsed,productionRatio=effectiveRate/48000;
        const bool targetReached=totalCallbacks>=sampleTarget,minimumCoverage=all.size()>400 && samples.size()>300;
        const bool withinNominalRate=productionRatio>=.95 && productionRatio<=1.05;
        const double missing=std::numeric_limits<double>::quiet_NaN();
        const double p50=samples.empty()?missing:percentile(samples,.5),p99=samples.empty()?missing:percentile(samples,.99);
        const double maximum=samples.empty()?missing:*std::max_element(samples.begin(),samples.end());
        output<<enabled<<','<<size<<",48000,128,2,"<<get(processor,"mode")<<','<<processor.getLatencySamples()<<','<<samples.size()<<','
              <<p50<<','<<p99<<','<<maximum<<','<<budget<<','<<overruns<<','<<totalCallbacks<<','<<minimumCaptureMs<<','<<maximumCaptureMs<<','
              <<observedElapsed<<','<<callbacksAtMinimum<<','<<effectiveRate<<','<<productionRatio<<','<<sampleTarget<<','<<targetReached<<','
              <<minimumCoverage<<','<<withinNominalRate<<",NOT_ASSESSED\n";
        output.flush();
        std::cout<<"OBSERVATION whole processor FFT="<<size<<" p50/p99="<<p50<<'/'<<p99<<" us; callbacks="<<samples.size()
                 <<" total_callbacks="<<totalCallbacks<<" callbacks_at_minimum_capture="<<callbacksAtMinimum<<" observed_ms="<<observedElapsed
                 <<" production_ratio="<<productionRatio<<" target_reached="<<targetReached<<" minimum_coverage="<<minimumCoverage
                 <<" within_5pct_nominal_rate="<<withinNominalRate<<" overruns="<<overruns<<" performance_acceptance=NOT_ASSESSED"<<std::endl;
        check(output.good(),"Whole-processor timing evidence could not be written");
        check(minimumCoverage,"Insufficient whole-processor callbacks: diagnostic row preserved (requires >400 total and >300 after warmup)");
        check(targetReached,"Whole-processor callback sample target timed out: diagnostic row preserved");
    }
    check(output.good(),"Whole-processor timing evidence could not be written");
    // These observations run the normal native event pump without snapshots or
    // PNG encoding. Sample conservation is checked separately from FFT coverage
    // and from wall-clock source scheduling. A slow producer cannot qualify the
    // nominal sample rate merely because its smaller queue did not overflow.
    std::ofstream high(directory.getChildFile("eq-analyzer-high-rate.csv").getFullPathName().toStdString());
    std::ofstream updateRows(directory.getChildFile("eq-analyzer-high-rate-updates.csv").getFullPathName().toStdString());
    high<<"rate,block,fft_size,fifo_capacity,requested_capture_ms,observed_elapsed_ms,callbacks,produced_samples,initial_pending_samples,effective_samples_per_second,production_ratio,consumed_samples,discarded_samples,dropped_samples,pending_samples,sample_accounting_error,update_calls,observed_updates,unobserved_updates,observed_queue_high_water,fft_windows,first_complete_data_observed_ms,consumer_gap_p50_ms,consumer_gap_p99_ms,consumer_gap_max_ms,fft_windows_per_update_max,fft_p50_us,fft_p99_us,update_p50_us,update_p99_us,update_max_us,process_cpu_percent,within_5pct_nominal_rate,sample_flow_status,performance_acceptance\n";
    updateRows<<"rate,fft_size,elapsed_ms,update_sequence,unobserved_updates_since_previous,consumer_gap_ms,pending_snapshot_samples,consumed_samples,fft_windows,fft_us,update_us,total_consumed_samples,total_discarded_samples,total_dropped_samples,pending_samples\n";
    struct NativeUpdateObservation {
        double elapsed{},gap{},fftMicros{},updateMicros{};
        uint64_t sequence{},unobserved{},pendingSnapshot{},consumed{},windows{},totalConsumed{},discarded{},dropped{},pending{};
    };
    constexpr int highRateObservationMs=10000;
    for(double rate:{96000.,192000.})for(int size:{2048,4096,8192}) {
        processor.prepareToPlay(rate,128);
        resolution->setSelectedId(size==2048?1:size==4096?2:3,juce::sendNotificationSync);settle(60);
        check(waitFor([&]{return panel->analysisSampleRate()==rate;}),"High-rate analyzer did not finish its sample-rate transition before capture");
        check(processor.graphicalEQ(1).analyzer.enabled.load(),"High-rate observation did not enable analyzer capture");
        auto& fifo=processor.graphicalEQ(1).analyzer;
        const uint64_t initialPending=fifo.pendingSamples(),beforeDropped=fifo.dropped.load();
        const auto beforeConsumed=panel->analysisTotalConsumedSamples(),beforeDiscarded=panel->analysisDiscardedSamples();
        const auto beforeUpdates=panel->analysisUpdates(),beforeWindows=panel->fftFrames();
        auto previousUpdate=beforeUpdates;
        uint64_t unobserved=0,queueHighWater=initialPending;
        std::vector<NativeUpdateObservation> observations;observations.reserve(512);
        std::vector<double> gaps,fftTimes,updateTimes;gaps.reserve(512);fftTimes.reserve(512);updateTimes.reserve(512);
        const auto started=std::chrono::steady_clock::now();const double cpuStart=processCPUSeconds();
        const auto elapsedMs=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
        double firstData=std::numeric_limits<double>::quiet_NaN();
        const auto observe=[&] {
            const auto pending=uint64_t(fifo.pendingSamples());queueHighWater=std::max(queueHighWater,pending);
            const auto sequence=panel->analysisUpdates();if(sequence==previousUpdate)return;
            const auto skipped=sequence-previousUpdate-1;unobserved+=skipped;previousUpdate=sequence;
            const auto snapshot=uint64_t(panel->analysisLastPendingSamples());queueHighWater=std::max(queueHighWater,snapshot);
            const double now=elapsedMs(),gap=panel->analysisLastConsumerIntervalMs();
            const double fft=panel->lastFFTTimeMicros(),update=panel->analysisUpdateMicros();
            observations.push_back({now,gap,fft,update,sequence-beforeUpdates,skipped,snapshot,
                uint64_t(panel->analysisLastConsumedSamples()),uint64_t(panel->analysisLastFFTWindows()),
                panel->analysisTotalConsumedSamples()-beforeConsumed,panel->analysisDiscardedSamples()-beforeDiscarded,
                fifo.dropped.load()-beforeDropped,pending});
            gaps.push_back(gap);fftTimes.push_back(fft);updateTimes.push_back(update);
            if(!std::isfinite(firstData) && panel->analyzerHasData() && panel->fftFrames()>beforeWindows)firstData=now;
        };
        SyntheticAudio audio(processor,true,rate);
        while(elapsedMs()<highRateObservationMs) {settle(1);observe();}
        audio.stop();observe();
        const double observedElapsed=elapsedMs(),processCPU=100000*(processCPUSeconds()-cpuStart)/observedElapsed;
        const uint64_t produced=audio.callbacks.load()*128,consumed=panel->analysisTotalConsumedSamples()-beforeConsumed;
        const uint64_t discarded=panel->analysisDiscardedSamples()-beforeDiscarded,lost=fifo.dropped.load()-beforeDropped,pending=fifo.pendingSamples();
        const uint64_t updates=panel->analysisUpdates()-beforeUpdates,windows=panel->fftFrames()-beforeWindows;
        const int64_t accountingError=int64_t(initialPending+produced)-int64_t(consumed+discarded+lost+pending);
        const double effectiveRate=1000*double(produced)/observedElapsed,productionRatio=effectiveRate/rate;
        const bool withinNominalRate=productionRatio>=.95 && productionRatio<=1.05;
        const double missing=std::numeric_limits<double>::quiet_NaN();
        const auto quantile=[&](const std::vector<double>& values,double fraction){return values.empty()?missing:percentile(values,fraction);};
        const auto maximum=[&](const std::vector<double>& values){return values.empty()?missing:*std::max_element(values.begin(),values.end());};
        uint64_t maximumWindows=0;for(const auto& observation:observations)maximumWindows=std::max(maximumWindows,observation.windows);
        const char* flow=accountingError!=0?"ACCOUNTING_ERROR":lost>0?"FIFO_DROPS":discarded>0?"DISCONTINUITY":windows==0?"NO_ANALYSIS":"NO_LOSS";
        high<<rate<<",128,"<<size<<','<<EQAnalyzerFIFO::capacity<<','<<highRateObservationMs<<','<<observedElapsed<<','<<audio.callbacks.load()<<','
            <<produced<<','<<initialPending<<','<<effectiveRate<<','<<productionRatio<<','<<consumed<<','<<discarded<<','<<lost<<','<<pending<<','
            <<accountingError<<','<<updates<<','<<observations.size()<<','<<unobserved<<','<<queueHighWater<<','<<windows<<','<<firstData<<','
            <<quantile(gaps,.5)<<','<<quantile(gaps,.99)<<','<<maximum(gaps)<<','<<maximumWindows<<','
            <<quantile(fftTimes,.5)<<','<<quantile(fftTimes,.99)<<','<<quantile(updateTimes,.5)<<','<<quantile(updateTimes,.99)<<','<<maximum(updateTimes)<<','
            <<processCPU<<','<<withinNominalRate<<','<<flow<<",NOT_ASSESSED\n";
        // Write after joining the producer. Polling can miss a timer callback;
        // retain the missing count instead of claiming complete percentiles.
        for(const auto& observation:observations)
            updateRows<<rate<<','<<size<<','<<observation.elapsed<<','<<observation.sequence<<','<<observation.unobserved<<','<<observation.gap<<','
                <<observation.pendingSnapshot<<','<<observation.consumed<<','<<observation.windows<<','<<observation.fftMicros<<','<<observation.updateMicros<<','
                <<observation.totalConsumed<<','<<observation.discarded<<','<<observation.dropped<<','<<observation.pending<<'\n';
        high.flush();updateRows.flush();
        std::cout<<"OBSERVATION high-rate="<<rate<<" FFT="<<size<<" produced/consumed/discarded/dropped/pending="
            <<produced<<'/'<<consumed<<'/'<<discarded<<'/'<<lost<<'/'<<pending<<" accounting_error="<<accountingError
            <<" observed/missed_updates="<<observations.size()<<'/'<<unobserved<<" consumer_gap_p99_ms="<<quantile(gaps,.99)
            <<" production_ratio="<<productionRatio<<" within_5pct_nominal_rate="<<withinNominalRate
            <<" sample_flow_status="<<flow<<" performance_acceptance=NOT_ASSESSED"<<std::endl;
        check(high.good() && updateRows.good(),"High-rate FIFO evidence could not be written");
        check(accountingError==0,"High-rate FIFO sample accounting does not conserve actual produced samples");
        check(produced>0 && updates>0 && !observations.empty(),"High-rate native observation did not exercise producer and consumer");
    }
    processor.prepareToPlay(48000,128);
}
}
#include "EQAttributionDiagnostics.h"
#include "EQPendingInputTests.h"
#include "EQBassZoomTests.h"
int main(int argc,char** argv){try {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc>1 && (std::string(argv[1])=="--attribution" || std::string(argv[1])=="--attribution-headless"))return eqAttribution::run(argc,argv);
    auto processor=std::make_unique<ChimeraProcessor>();auto editor=std::make_unique<ChimeraEditor>(*processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);check(editor->getPeer()!=nullptr,"No native UI peer");
    auto* eqTab=tab(*editor,"EQ");check(eqTab!=nullptr,"EQ navigation missing");eqTab->triggerClick();settle(80);
    if(argc>1 && std::string(argv[1])=="--bass-zoom") {
        const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>2?argv[2]:"eq-bass-zoom");
        eqBassZoomTests::run(*processor,editor,directory);
        return 0;
    }
    if(argc>1 && std::string(argv[1])=="--group-edit") {
        for(int bank=0;bank<2;++bank) {
            auto* path=find<juce::TextButton>(*editor,bank==0?"path.tone.eq":"path.final.eq");check(path,"EQ path target missing");path->triggerClick();
            auto* panel=find<GraphicalEQPanel>(*editor,bank==0?"toneEQ_panel":"finalEQ_panel");
            check(panel && waitFor([&]{return panel->isShowing();}),"Group fixture panel not visible");
            groupEditing(*processor,*panel,bank);
        }
        return 0;
    }
    if(argc>1 && std::string(argv[1])=="--modifier-drag") {
        int failures=0;
        for(int bank=0;bank<2;++bank) {
            auto* path=find<juce::TextButton>(*editor,bank==0?"path.tone.eq":"path.final.eq");path->triggerClick();
            auto* panel=find<GraphicalEQPanel>(*editor,bank==0?"toneEQ_panel":"finalEQ_panel");
            check(panel && waitFor([&]{return panel->isShowing();}),"Modifier fixture panel not visible");
            set(*processor,eqID(bank,0,"enabled"),1);settle(80);panel->selectBandForEditing(0);
            auto* selector=find<juce::ComboBox>(*panel,eqID(bank,-1,"band"));check(selector,"Modifier fixture selector missing");
            modifierDrag(*processor,*panel,*selector,bank);
            failures+=modifierTransitions(*processor,*panel,bank);
        }
        return failures==0?0:1;
    }
    const juce::File directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>1?argv[1]:"eq-ui");directory.createDirectory();
    for(int e=0;e<2;++e){
        tab(*editor,"RIGS")->triggerClick();settle(80);
        auto* path=find<juce::TextButton>(*editor,e==0?"path.tone.eq":"path.final.eq");check(path,"EQ path target missing");path->triggerClick();
        auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(panel && waitFor([&]{return panel->isShowing() && panel->isNavigationSelected();}),"EQ path did not open the requested bank");
        editingWorkflow(*processor,*panel,e);
        eqPendingInputTests::run(*processor,*panel,e);
        auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));check(selector,"Band chooser missing");
        HostEvents host;auto* frequency=processor->parameters().getParameter(eqID(e,0,"frequency"));auto* gain=processor->parameters().getParameter(eqID(e,0,"gain"));auto* q=processor->parameters().getParameter(eqID(e,0,"q"));frequency->addListener(&host);gain->addListener(&host);q->addListener(&host);
        for(int b=0;b<12;++b){selector->setSelectedId(b+1,juce::sendNotificationSync);
            auto* f=find<juce::Slider>(*panel,eqID(e,b,"frequency"));auto* g=find<juce::Slider>(*panel,eqID(e,b,"gain"));auto* qs=find<juce::Slider>(*panel,eqID(e,b,"q"));check(f && g && qs,"Node controls missing");
            numeric(*f,juce::String(200+b*317)+" Hz");numeric(*g,"3.5 dB");numeric(*qs,"1.25 Q");
            std::cout<<"Numeric "<<e<<":"<<b<<" Hz="<<get(*processor,eqID(e,b,"frequency"))<<" dB="<<get(*processor,eqID(e,b,"gain"))<<" Q="<<get(*processor,eqID(e,b,"q"))<<'\n';
            check(std::abs(get(*processor,eqID(e,b,"frequency"))-(200+b*317))<.1 && std::abs(get(*processor,eqID(e,b,"gain"))-3.5f)<.00001f && std::abs(get(*processor,eqID(e,b,"q"))-1.25f)<.001,"Numeric node edit did not reach host");
            auto* on=find<juce::TextButton>(*panel,eqID(e,b,"enabled"));on->triggerClick();check(waitFor([&]{return get(*processor,eqID(e,b,"enabled"))==1;}),"Band activation not connected");
            auto* filter=find<juce::ComboBox>(*panel,eqID(e,b,"type"));filter->setSelectedId(4,juce::sendNotificationSync);check(get(*processor,eqID(e,b,"type"))==3 && !g->isEnabled(),"HP choice / gain applicability");
            filter->setSelectedId(1,juce::sendNotificationSync);
            if(b==0)for(int t=1;t<=7;++t){filter->setSelectedId(t,juce::sendNotificationSync);check(get(*processor,eqID(e,b,"type"))==float(t-1),"Filter menu did not reach host");}
            filter->setSelectedId(1,juce::sendNotificationSync);
        }
        selector->setSelectedId(1,juce::sendNotificationSync);const auto start=panel->nodePosition(0),end=start+juce::Point<float>(40,-18);
        panel->mouseDown(mouse(*panel,start,start,false));panel->mouseDrag(mouse(*panel,end,start,true));panel->mouseUp(mouse(*panel,end,start,true));
        check(get(*processor,eqID(e,0,"frequency"))>200 && get(*processor,eqID(e,0,"gain"))>3.5,"Graph drag not connected");
        juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;panel->mouseWheelMove(mouse(*panel,end,end,false),wheel);check(get(*processor,eqID(e,0,"q"))>1.25,"Wheel Q not connected");
        check(host.starts>=3 && host.starts==host.ends && host.changes>0,"Unbalanced/missing host gestures");frequency->removeListener(&host);gain->removeListener(&host);q->removeListener(&host);
        overlappingBandDrag(*processor,*panel,*selector,e);
        modifierDrag(*processor,*panel,*selector,e);
        peakToggle(*processor,*panel,e);
        peakOnlyPaths(*processor,*panel,e);
        displayRangeAndReadout(*processor,*panel,*selector,e);
        frequencyViewZoom(*processor,*panel,*selector,e);
        analyzerTilt(*processor,*panel,e);
        check(modifierTransitions(*processor,*panel,e)==0,"Modifier transition regression failed");
        auto* bypass=find<juce::TextButton>(*panel,eqID(e,-1,"bypass"));bypass->triggerClick();check(waitFor([&]{return !processor->graphicalEQ(e).isBypassed();}),"Bypass not connected");
        for(int b=0;b<12;++b)set(*processor,eqID(e,b,"enabled"),b==0?1.f:0.f);set(*processor,eqID(e,0,"frequency"),1007.8125f);set(*processor,eqID(e,0,"gain"),6);processor->graphicalEQ(e).prepare(48000);
    }
    liveAnalyzer(*processor,*editor,directory);
    juce::MemoryBlock observationState;processor->getStateInformation(observationState);
    // The same visible editor must track automation, A/B and project recall for
    // every band in both banks, including values changed while another tab is open.
    for(int e=0;e<2;++e)for(int b=0;b<12;++b){set(*processor,eqID(e,b,"frequency"),float(180+e*43+b*319));set(*processor,eqID(e,b,"gain"),float(b-6+e));set(*processor,eqID(e,b,"q"),.5f+float(b)*.1f);}
    const auto verifyBanks=[&](const char* stage){
        for(int e=0;e<2;++e){auto* path=find<juce::TextButton>(*editor,e==0?"path.tone.eq":"path.final.eq");path->triggerClick();
            auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(waitFor([&]{return panel->isShowing()&&panel->isNavigationSelected();}),"Restored EQ navigation stale");
            auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));
            // The panel rebuilds its band list on its next timer tick; a restore
            // that adds bands is not selectable until then.
            check(waitFor([&]{for(int b=0;b<12;++b)if(panel->isBandVisible(b)!=(selector->indexOfItemId(b+1)>=0))return false;return true;}),"Restored EQ band list stale");
            for(int b=0;b<12;++b){selector->setSelectedId(b+1,juce::sendNotificationSync);for(const char* field:{"frequency","gain","q"}){
                auto* control=find<juce::Slider>(*panel,eqID(e,b,field));const float expected=get(*processor,eqID(e,b,field));
                if(!control || std::abs(control->getValue()-expected)>=.02) {
                    // Name the stage and value so a platform-only failure can be traced from the CI log.
                    const auto detail=juce::String("Restored or automated numeric EQ control is stale: stage=")+stage+" bank="+juce::String(e)+" band="+juce::String(b)
                        +" field="+field+" slider="+(control?juce::String(control->getValue(),4)+" range=["+juce::String(control->getMinimum(),4)+","+juce::String(control->getMaximum(),4)+"]":juce::String("missing"))
                        +" parameter="+juce::String(expected,4);
                    check(false,detail.toRawUTF8());
                }
            }}selector->setSelectedId(1,juce::sendNotificationSync);
        }
    };
    // A narrow view range is editor state: A/B recall and factory presets
    // neither reset it nor are changed by it.
    const auto rangePanel=[&](int e){return find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");};
    const auto expectRange=[&](float range,const char* message){for(int e=0;e<2;++e)check(rangePanel(e)->displayRange()==range,message);};
    rangePanel(0)->setDisplayRange(12);rangePanel(1)->setDisplayRange(6);
    verifyBanks("automation");processor->copyComparison();juce::MemoryBlock saved;processor->getStateInformation(saved);
    processor->selectComparison(1);for(int e=0;e<2;++e)for(int b=0;b<12;++b)set(*processor,eqID(e,b,"gain"),12);verifyBanks("A/B select B");
    processor->selectComparison(0);verifyBanks("A/B select A");
    check(rangePanel(0)->displayRange()==12.f && rangePanel(1)->displayRange()==6.f,"A/B recall changed the EQ display range");
    for(int e=0;e<2;++e)for(int b=0;b<12;++b)check(std::abs(get(*processor,eqID(e,b,"gain"))-float(b-6+e))<.001,"A/B bank recall changed a node");
    // A factory recipe now supplies active EQ bands. Select an unrelated old
    // Band 12 so the same editor must remove it and display the new recipe.
    for(int e=0;e<2;++e){
        set(*processor,eqID(e,11,"enabled"),1);set(*processor,eqID(e,11,"type"),0);set(*processor,eqID(e,11,"gain"),7);
        auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");
        auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));
        check(waitFor([&]{return selector->getNumItems()==graphicalEQBands;}),"Factory recall fixture did not expose old Band 12");
        selector->setSelectedId(graphicalEQBands,juce::sendNotificationSync);
        check(panel->selectedBand()==11,"Factory recall fixture did not select old Band 12");
    }
    for(int slot=0;slot<4;++slot)set(*processor,dynamicSlotID(slot,"target"),float((slot%2+1)*graphicalEQBands));
    auto& factoryEdits=processor->eqEditing();
    for(float gain:{8.f,9.f}){
        check(factoryEdits.beginEdit(0,"Old Band 12 gain"),"Factory UI history fixture could not begin an edit");
        check(factoryEdits.setValue(0,11,"gain",gain),"Factory UI history fixture could not edit Band 12");
        factoryEdits.endEdit();
    }
    check(factoryEdits.undo() && factoryEdits.canUndo() && factoryEdits.canRedo(),"Factory UI Undo/Redo fixture missing");
    processor->loadFactoryPreset(0);
    check(!factoryEdits.canUndo() && !factoryEdits.canRedo(),"Factory UI recall retained the previous EQ history");
    check(rangePanel(0)->displayRange()==12.f && rangePanel(1)->displayRange()==6.f,"Factory preset recall changed the EQ display range");
    for(int e=0;e<2;++e)rangePanel(e)->setDisplayRange(24);expectRange(24.f,"EQ display range did not return to +/-24");
    for(int e=0;e<2;++e){
        auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");
        auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));
        auto* undo=find<juce::TextButton>(*panel,eqID(e,-1,"undo"));
        auto* redo=find<juce::TextButton>(*panel,eqID(e,-1,"redo"));
        uint16_t enabledMask=0;int enabledCount=0,firstEnabled=-1;
        for(int b=0;b<graphicalEQBands;++b)if(get(*processor,eqID(e,b,"enabled"))>.5f){
            enabledMask|=uint16_t(1u<<b);++enabledCount;if(firstEnabled<0)firstEnabled=b;
        }
        check(enabledMask!=0 && factoryEdits.occupiedMask(e)==enabledMask,"Factory UI occupancy differs from active EQ recipe");
        check(get(*processor,eqID(e,-1,"bypass"))==0,"Factory UI finishing EQ is bypassed");
        check(get(*processor,eqID(e,11,"enabled"))==0 && !panel->isBandVisible(11),"Factory UI retained stale Band 12");
        check(waitFor([&]{return selector->getNumItems()==enabledCount && panel->selectedBand()==firstEnabled
            && selector->getSelectedId()==firstEnabled+1 && !undo->isEnabled() && !redo->isEnabled();}),
            "Factory UI did not display authored bands, replace the stale selection or clear Undo/Redo");
        for(int b=0;b<graphicalEQBands;++b)
            check(panel->isBandVisible(b)==((enabledMask&(1u<<b))!=0),"Factory UI has hidden active or stale visible EQ bands");
        for(const char* field:{"frequency","gain","q"}){
            auto* control=find<juce::Slider>(*panel,eqID(e,firstEnabled,field));
            check(control && std::abs(control->getValue()-get(*processor,eqID(e,firstEnabled,field)))<.02,
                  "Factory UI numeric controls retained the previous band");
        }
    }
    for(int slot=0;slot<4;++slot){
        const int target=int(get(*processor,dynamicSlotID(slot,"target")));
        check(slot>=2?target==0:(target>0 && target!=graphicalEQBands && target!=2*graphicalEQBands),
              "Factory UI recall retained a stale dynamic assignment");
    }
    processor->setStateInformation(saved.getData(),int(saved.getSize()));verifyBanks("project restore");
    for(int e=0;e<2;++e)for(int b=0;b<12;++b)check(std::abs(get(*processor,eqID(e,b,"gain"))-float(b-6+e))<.001,"Project recall changed a node");
    for(int mode:{1,2,0}){set(*processor,"mode",float(mode));settle(100);check(find<GraphicalEQPanel>(*editor,"finalEQ_panel")->isNavigationSelected(),"Mode switch lost the global EQ selection");}
    // The compact signal strip has explicit Tone/Final targets, replacing the
    // old generic EQ tab. Returning through Final must retain its selected node.
    auto* finalPanel=find<GraphicalEQPanel>(*editor,"finalEQ_panel");
    auto* finalPath=find<juce::TextButton>(*editor,"path.final.eq");
    auto* finalSelector=find<juce::ComboBox>(*finalPanel,eqID(1,-1,"band"));
    check(finalPath && finalSelector,"Final EQ navigation or node selector missing");
    finalSelector->setSelectedId(7,juce::sendNotificationSync);
    tab(*editor,"RIGS")->triggerClick();check(waitFor([&]{return !finalPanel->isShowing();}),"CAB navigation did not leave EQ");
    finalPath->triggerClick();check(waitFor([&]{return finalPanel->isShowing() && finalPanel->selectedBand()==6;}),"Final EQ node selection did not survive CAB navigation");
    auto* tunerPath=find<juce::TextButton>(*editor,"tunerOpen");check(tunerPath,"Tuner utility target missing");
    tunerPath->triggerClick();check(waitFor([&]{return !finalPanel->isShowing();}),"Tuner navigation did not leave EQ");
    finalPath->triggerClick();check(waitFor([&]{return finalPanel->isShowing() && finalPanel->selectedBand()==6;}),"Utility navigation overwrote the selected Final EQ node");
    finalSelector->setSelectedId(1,juce::sendNotificationSync);
    for(int percent:{75,100,125,150}){editor->setSize(1180*percent/100,780*percent/100);settle(70);
        for(int e=0;e<2;++e){find<juce::TextButton>(*editor,e==0?"path.tone.eq":"path.final.eq")->triggerClick();settle(40);auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(panel->isShowing(),"Scaled bank is hidden");const auto local=panel->nodePosition(0);
            const auto global=editor->getLocalPoint(panel,local);const auto finish=global+juce::Point<float>(float(percent)/10,0);
            const auto start=panel->getLocalPoint(editor.get(),global),end=panel->getLocalPoint(editor.get(),finish);
            const float previous=get(*processor,eqID(e,0,"frequency"));panel->mouseDown(mouse(*panel,start,start,false));panel->mouseDrag(mouse(*panel,end,start,true));panel->mouseUp(mouse(*panel,end,start,true));
            check(get(*processor,eqID(e,0,"frequency"))>previous,"Graph transform drag failed at UI scale");set(*processor,eqID(e,0,"frequency"),previous);}
        for(int e=0;e<2;++e){auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(editorBoundsReachable(*editor,*panel),"EQ clipped at scale");for(const char* field:{"frequency","gain","q"}){auto* s=find<juce::Slider>(*panel,eqID(e,0,field));check(editorBoundsReachable(*editor,*s),"Numeric controls clipped");}}
        snapshot(*editor,directory.getChildFile("eq-ui-"+juce::String(percent)+".png"));
        const auto t=juce::Time::getMillisecondCounterHiRes();const auto cpu=processCPUSeconds();settle(1000);const double elapsed=juce::Time::getMillisecondCounterHiRes()-t;std::cout<<"UI idle "<<percent<<"% = "<<100000.*(processCPUSeconds()-cpu)/elapsed<<"% one core (1 second no audio, process CPU)\n";
    }
    tab(*editor,"RIGS")->triggerClick();for(int e=0;e<2;++e)check(waitFor([&]{return !processor->graphicalEQ(e).analyzer.enabled.load();}),"Hidden EQ analyzer still enabled");
    std::cout<<"PASS native functional coverage / automation / A-B and project recall / mode navigation / 75-150% / hidden analyzer"<<std::endl;
    // Timing is observational and must not prevent the functional recall,
    // navigation or scaling checks above from running. Restore the original
    // audio fixture and editor size for comparable whole-processor observations.
    processor->setStateInformation(observationState.getData(),int(observationState.getSize()));
    editor->setSize(1180,780);eqTab->triggerClick();settle(50);
    find<juce::TextButton>(*editor,"path.final.eq")->triggerClick();
    check(waitFor([&]{return find<GraphicalEQPanel>(*editor,"finalEQ_panel")->isShowing();}),"Timing observation did not restore the visible EQ bank");
    std::exception_ptr observationFailure;
    try{fullProcessorNativeTiming(*processor,*editor,directory);}catch(...){observationFailure=std::current_exception();}
    pendingThawEditorRecreation(*processor,editor);
    // Even an exhausted timing sample budget must leave close/teardown coverage
    // visible, and its original failure must still fail the overall test.
    editor.reset();for(int e=0;e<2;++e)check(!processor->graphicalEQ(e).analyzer.enabled.load(),"Closed editor analyzer still enabled");processor->releaseResources();
    std::cout<<"PASS real native peer / 24 numeric nodes / drag / overlapping selection / Q / host gestures / 7 choices / stereo FFT / 75-150% / hidden and close\n";
    if(observationFailure)std::rethrow_exception(observationFailure);
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

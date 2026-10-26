#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ABLevelMatch.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

// A/B level match: a monitoring cut that turns the louder A/B sound down to the
// quieter one. These tests cover the meter, the match, what it must never do
// (boost, touch the output while off, survive in a project) and the processor
// wiring.
namespace {
using spectralforge::ABLevelMatch;
void require(bool ok,const juce::String& message) {if(!ok)throw std::runtime_error(message.toStdString());}
constexpr double rate=48000;constexpr int block=256;

double rmsDb(const juce::AudioBuffer<float>& b) {
    double e=0;for(int c=0;c<b.getNumChannels();++c)for(int i=0;i<b.getNumSamples();++i)e+=double(b.getSample(c,i))*b.getSample(c,i);
    return 10*std::log10(e/(b.getNumChannels()*b.getNumSamples())+1e-30);
}
struct Sine {
    double frequency,phase{};
    void fill(juce::AudioBuffer<float>& b,float amplitude) {
        for(int i=0;i<b.getNumSamples();++i) {
            const float x=amplitude*float(std::sin(phase));phase+=juce::MathConstants<double>::twoPi*frequency/rate;
            for(int c=0;c<b.getNumChannels();++c)b.setSample(c,i,x);
        }
    }
};
// Plays one slot for some seconds; returns the RMS of the last half second.
double play(ABLevelMatch& m,int slot,float amplitude,double seconds,Sine& sine,int channels=2) {
    juce::AudioBuffer<float> b(channels,block);double tail=0;int tailBlocks=0;const int blocks=int(seconds*rate/block);
    for(int k=0;k<blocks;++k) {
        sine.fill(b,amplitude);m.process(b,slot);
        if(k>=blocks-int(.5*rate/block)){tail+=std::pow(10.,rmsDb(b)/10);++tailBlocks;}
    }
    return 10*std::log10(tail/std::max(1,tailBlocks));
}

void meter() {
    // BS.1770: a 997 Hz sine at full scale on one channel reads -3.01 LUFS.
    ABLevelMatch m;m.prepare(rate,1);Sine sine{997};
    play(m,0,1.f,4,sine,1);
    require(std::abs(m.loudness(0)-(-3.01f))<.1f,"K-weighted loudness is off: "+juce::String(m.loudness(0)));
    require(std::isnan(m.loudness(1)),"An unheard slot reported a loudness");
    // Low frequencies are weighted down (RLB high-pass), the presence region up.
    ABLevelMatch low;low.prepare(rate,1);Sine lowSine{30};play(low,0,1.f,4,lowSine,1);
    ABLevelMatch high;high.prepare(rate,1);Sine highSine{4000};play(high,0,1.f,4,highSine,1);
    require(low.loudness(0)<m.loudness(0)-1.f && high.loudness(0)>m.loudness(0)+2.f,"K-weighting shape is wrong");
    std::cout<<"MEASURE lufs 997Hz="<<m.loudness(0)<<" 30Hz="<<low.loudness(0)<<" 4kHz="<<high.loudness(0)<<'\n';
    std::cout<<"PASS meter: BS.1770 level, unheard slot unknown, K-weighting shape\n";
}

void match() {
    ABLevelMatch m;m.prepare(rate,2);m.setEnabled(true);Sine sine{220};
    const float quiet=.05f,loud=.2f; // 12 dB apart
    const double a=play(m,0,quiet,3,sine);
    require(m.appliedDb()==0.f,"Matched before the other sound was heard");
    play(m,1,loud,3,sine);
    const double b=play(m,1,loud,1,sine);
    require(std::abs(b-a)<.2,"The louder sound was not matched: "+juce::String(b-a,2)+" dB");
    require(std::abs(m.appliedDb()+12.f)<.2f,"Applied cut does not match the level difference");
    // The quieter sound plays untouched (never boosted).
    juce::AudioBuffer<float> x(2,block),y(2,block);
    play(m,0,quiet,1,sine);Sine copy=sine;sine.fill(x,quiet);copy.fill(y,quiet);m.process(y,0);
    for(int i=0;i<block;++i)require(x.getSample(0,i)==y.getSample(0,i),"The quieter sound was changed");
    require(m.appliedDb()==0.f,"The quieter sound was boosted or cut");
    std::cout<<"MEASURE match a="<<a<<" b="<<b<<" dB\n";

    // Off: bit-identical output, measurement continues.
    m.setEnabled(false);play(m,1,loud,.2,sine);
    Sine c1=sine,c2=sine;c1.fill(x,loud);c2.fill(y,loud);m.process(y,1);
    for(int i=0;i<block;++i)require(x.getSample(1,i)==y.getSample(1,i),"Output changed while level match was off");
    m.setEnabled(true);play(m,1,loud,.2,sine);
    require(std::abs(m.appliedDb()+12.f)<.3f,"Re-enabling did not use the kept measurement");

    // Silence does not move the estimates (beyond the filter's few-block decay tail).
    const float before=m.loudness(1);play(m,1,0.f,3,sine);
    require(std::abs(m.loudness(1)-before)<.05f,"Silence changed a slot's loudness: "+juce::String(m.loudness(1)-before,3)+" dB");

    // The cut is bounded.
    ABLevelMatch far;far.prepare(rate,2);far.setEnabled(true);Sine s2{220};
    play(far,0,.01f,3,s2);play(far,1,1.f,3,s2); // 40 dB apart, both above the gate
    require(std::abs(far.appliedDb()+float(ABLevelMatch::maximumCutDb))<.1f,"The cut exceeded its bound");

    // Copying A to B: they sound the same, no cut. Forgetting: no cut until re-heard.
    m.copied(0);play(m,1,quiet,.2,sine); // B now holds A's (quiet) sound
    require(std::abs(m.loudness(1)-m.loudness(0))<.5f && m.appliedDb()>-1.f,"Copy did not equalise the slots");
    m.forget();play(m,1,loud,.1,sine);
    require(std::isnan(m.loudness(0)) && m.appliedDb()>-.5f,"Forget kept a measurement or a cut");
    std::cout<<"PASS match: louder sound cut to the quieter one, never boosted, off is bit-identical, silence gated, bounded cut, copy and forget\n";
}

void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* q=p.parameters().getParameter(id);require(q!=nullptr,"Missing parameter "+id);q->setValueNotifyingHost(q->convertTo0to1(value));
}
double render(ChimeraProcessor& p,double seconds,Sine& sine,std::vector<float>* keep=nullptr) {
    juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;double tail=0;int tailBlocks=0;const int blocks=int(seconds*rate/block);
    for(int k=0;k<blocks;++k) {
        sine.fill(b,.05f);p.processBlock(b,midi);
        if(keep)for(int c=0;c<2;++c)for(int i=0;i<block;++i)keep->push_back(b.getSample(c,i));
        if(k>=blocks-int(.5*rate/block)){tail+=std::pow(10.,rmsDb(b)/10);++tailBlocks;}
    }
    return 10*std::log10(tail/std::max(1,tailBlocks));
}
void processor() {
    auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(0);
    p->prepareToPlay(rate,block);Sine sine{110};
    // B is the same sound 6 dB louder at the output trim.
    p->selectComparison(1);set(*p,"output",-1.f);p->selectComparison(0);set(*p,"output",-7.f);
    p->abLevelMatch().setEnabled(true);
    const double a=render(*p,3,sine);
    p->selectComparison(1);render(*p,3,sine);const double b=render(*p,1,sine);
    require(std::abs(b-a)<.5,"Processor A/B level match failed: "+juce::String(b-a,2)+" dB");
    require(p->abLevelMatch().appliedDb()<-4.f,"Processor did not report the applied cut");
    std::cout<<"MEASURE processor a="<<a<<" b="<<b<<" cut="<<p->abLevelMatch().appliedDb()<<" dB\n";

    // Never saved: the project carries neither the switch nor the cut.
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    require(!restored->abLevelMatch().enabled(),"A restored project switched level match on");
    // Loading a project forgets the old measurements.
    p->setStateInformation(saved.getData(),int(saved.getSize()));
    p->prepareToPlay(rate,block);render(*p,.1,sine);
    require(std::isnan(p->abLevelMatch().loudness(0)) && std::isnan(p->abLevelMatch().loudness(1)),"Loading a project kept old A/B measurements");
    p->releaseResources();

    // Off (the default): bit-identical to an instance that never had it.
    auto one=std::make_unique<ChimeraProcessor>(),two=std::make_unique<ChimeraProcessor>();
    for(auto* q:{one.get(),two.get()}){q->loadFactoryPreset(0);q->prepareToPlay(rate,block);}
    two->abLevelMatch().setEnabled(true);two->abLevelMatch().setEnabled(false);
    std::vector<float> x,y;Sine s1{110},s2{110};render(*one,1,s1,&x);render(*two,1,s2,&y);
    require(x==y,"Level match changed the output while off");
    std::cout<<"PASS processor: A/B level match on the real output, not saved, forgotten on project load, off is bit-identical\n";
}

void reprepare() {
    for(const double sr:{44100.,48000.,96000.}) {
        ABLevelMatch m;m.prepare(rate,2);Sine sine{997};
        play(m,0,.05f,2,sine);play(m,1,.2f,2,sine);m.setEnabled(true);
        m.copied(1); // queued work must not resurrect the previous session
        m.prepare(sr,1);
        require(std::isnan(m.loudness(0)) && std::isnan(m.loudness(1)),"prepare retained old measurements at "+juce::String(sr));
        juce::AudioBuffer<float> b(1,128);b.clear();m.process(b,1);
        require(std::isnan(m.loudness(0)) && std::isnan(m.loudness(1)) && m.appliedDb()==0,"prepare retained pending COPY or attenuation");
        play(m,0,.05f,4,sine,1);
        require(std::isfinite(m.loudness(0)),"Reprepare test did not establish a new measurement");
        m.setEnabled(true);
        m.prepare(sr,1);
        require(std::isnan(m.loudness(0)) && !m.enabled(),"same-rate prepare did not reset comparison session");
    }
    std::cout<<"PASS prepare: same-rate/device/channel/rate changes clear readings, pending COPY and MATCH\n";
}

void edgeCases() {
    for(const double sr:{44100.,48000.,96000.})for(const int channels:{1,2}) {
        ABLevelMatch m;m.prepare(sr,channels);m.setEnabled(true);
        juce::AudioBuffer<float> b(channels,257);
        for(int i=0;i<int(4*sr/257);++i){b.clear();m.process(b,i&1);}
        require(std::isnan(m.loudness(0)) && std::isnan(m.loudness(1)) && m.appliedDb()==0,"Silence established a match");
        m.setEnabled(false);
        const float values[]={0.f,-0.f,1.f,-1.f,1e-30f,-1e-30f,.1234567f};
        for(int c=0;c<channels;++c)for(int i=0;i<b.getNumSamples();++i)b.setSample(c,i,values[i%7]);
        juce::AudioBuffer<float> original;original.makeCopyOf(b);m.process(b,0);
        for(int c=0;c<channels;++c)require(std::memcmp(b.getReadPointer(c),original.getReadPointer(c),257*sizeof(float))==0,"OFF changed output bits, including signed zero");
    }
    ABLevelMatch m;m.prepare(rate,2);Sine s{997};
    play(m,0,.05f,2,s);play(m,1,.2f,2,s);
    m.copied(0);m.invalidate(1);play(m,1,.2f,.1,s);
    require(std::isnan(m.loudness(1)) && std::isfinite(m.loudness(0)),"Pending old COPY resurrected a replaced preset's reading");
    m.invalidate(0);m.copied(0);play(m,0,.05f,.1,s);
    require(std::isnan(m.loudness(0)) && std::isnan(m.loudness(1)),"COPY after preset invalidation copied old statistics");
    play(m,0,.05f,2,s);play(m,1,.2f,2,s);
    const auto before=m.loudness(1);m.switched();play(m,1,1.f,.2,s);
    require(m.loudness(1)==before,"Rapid A/B/A did not restart measurement holdoff");
    m.setEnabled(true);play(m,1,0.f,.1,s);
    const auto a=m.loudness(0),b=m.loudness(1);
    m.protectOutput(true);play(m,1,1.f,2,s);
    require(m.loudness(0)==a && m.loudness(1)==b,"Printing contaminated comparison statistics");
    std::cout<<"PASS edge cases: silence-only, OFF byte identity/signed zero, pending COPY ordering, rapid switch holdoff, print measurement freeze\n";
}

// A tiny signal is below the measurement gate, so the gain target is fixed.
// Check every sample, not just a final block's RMS, including stereo linking.
void ramps() {
    for(const double sr:{44100.,48000.,96000.})for(const int size:{1,64,257,1024})for(const int channels:{1,2}) {
        ABLevelMatch m;m.prepare(sr,channels);
        double phase=0;
        const auto feed=[&](int slot,float amplitude,int samples) {
            for(int done=0;done<samples;) {
                const int n=std::min(size,samples-done);juce::AudioBuffer<float> b(channels,n);
                for(int i=0;i<n;++i){const float x=amplitude*float(std::sin(phase));phase+=juce::MathConstants<double>::twoPi*997/sr;for(int c=0;c<channels;++c)b.setSample(c,i,x);}
                m.process(b,slot);done+=n;
            }
        };
        feed(0,.05f,int(2*sr));feed(1,.2f,int(2*sr));feed(1,0,int(sr));
        const float expected=juce::Decibels::decibelsToGain(m.loudness(0)-m.loudness(1));
        const int rampSamples=int(.05*sr);
        const auto check=[&](int slot,float start,float end) {
            for(int done=0;done<rampSamples+size;) {
                const int n=std::min(size,rampSamples+size-done);juce::AudioBuffer<float> b(channels,n);
                for(int c=0;c<channels;++c)for(int i=0;i<n;++i)b.setSample(c,i,1e-8f);
                m.process(b,slot);
                for(int i=0;i<n;++i)for(int c=0;c<channels;++c) {
                    const float expectedGain=start+(end-start)*std::min(1.f,float(done+i+1)/float(rampSamples));
                    require(std::abs(b.getSample(c,i)/1e-8f-expectedGain)<2e-4f,"50 ms ramp changed with rate, block size or channel");
                    require(b.getSample(c,i)<=1e-8f && b.getSample(c,i)>=0,"Ramp boosted or inverted output");
                }
                done+=n;
            }
        };
        m.setEnabled(true);check(1,1,expected);
        m.setEnabled(false);check(1,expected,1);
        m.setEnabled(true);check(1,1,expected);
        const float a=m.loudness(0),b=m.loudness(1);
        check(0,expected,1);check(1,1,expected);
        require(m.loudness(0)==a && m.loudness(1)==b,"Silent A/B switching changed measurements");
        m.prepare(sr,channels);m.setEnabled(true);
        feed(0,.01f,int(2*sr));feed(1,1.f,int(2*sr));
        require(std::abs(m.appliedDb()+24.f)<.001f,"24 dB cap changed with rate, block size or channel");
    }
    std::cout<<"PASS ramps: ON/OFF/A/B sample ramps at 44.1/48/96 kHz, 1/64/257/1024 samples, mono/stereo\n";
}

struct RecordingPlayHead : juce::AudioPlayHead {
    bool recording=false,available=true;
    juce::Optional<PositionInfo> getPosition() const override {
        if(!available)return {};
        PositionInfo p;p.setIsPlaying(recording);p.setIsRecording(recording);return p;
    }
};

void recallSafety() {
    auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(0);p->prepareToPlay(rate,block);Sine sine{110};
    render(*p,2,sine);p->selectComparison(1);set(*p,"output",-1);render(*p,2,sine);
    p->selectComparison(0);render(*p,.1,sine);
    const float other=p->abLevelMatch().loudness(1);
    require(std::isfinite(other) && std::isfinite(p->abLevelMatch().loudness(0)),"A/B switch lost measurements");
    p->loadFactoryPreset(1);render(*p,.1,sine);
    require(std::isnan(p->abLevelMatch().loudness(0)),"Factory preset retained the replaced slot's measurement");
    require(p->abLevelMatch().loudness(1)==other,"Factory preset discarded the unchanged comparison slot");
    render(*p,2,sine);p->copyComparison();p->selectComparison(1);render(*p,.1,sine);
    require(std::abs(p->abLevelMatch().loudness(0)-p->abLevelMatch().loudness(1))<.01f,"COPY did not preserve equal measurements");
    juce::MemoryBlock saved;p->getStateInformation(saved);p->abLevelMatch().setEnabled(true);
    p->setStateInformation(saved.getData(),int(saved.getSize()));
    require(!p->abLevelMatch().enabled(),"Restoring into a used instance left MATCH enabled");
    render(*p,.1,sine); // deliberately no prepareToPlay
    require(std::isnan(p->abLevelMatch().loudness(0)) && std::isnan(p->abLevelMatch().loudness(1)) && p->abLevelMatch().appliedDb()==0,"Restore without prepare retained readings or ramp");
    render(*p,2,sine);p->prepareToPlay(rate,block);
    require(std::isnan(p->abLevelMatch().loudness(1)),"Processor reprepare kept measurements");
    for(const double sr:{44100.,96000.}) {
        render(*p,3,sine);require(std::isfinite(p->abLevelMatch().loudness(1)),"Processor rate-change test had no reading");
        p->prepareToPlay(sr,257);
        require(std::isnan(p->abLevelMatch().loudness(0)) && std::isnan(p->abLevelMatch().loudness(1)),"Processor sample-rate change kept measurements");
    }
    std::cout<<"PASS recall: A/B retained, preset invalidates active only, COPY, existing-instance restore without prepare, repeated prepare\n";
}

void printSafety(bool offline) {
    auto matched=std::make_unique<ChimeraProcessor>(),plain=std::make_unique<ChimeraProcessor>();
    RecordingPlayHead host;matched->setPlayHead(&host);plain->setPlayHead(&host);
    for(auto* p:{matched.get(),plain.get()}) {
        p->loadFactoryPreset(0);p->prepareToPlay(rate,block);
        set(*p,"output",-7);Sine s{110};render(*p,2,s);
        p->selectComparison(1);set(*p,"output",-1);render(*p,2,s);
    }
    Sine left{110},right{110};
    {
        matched->abLevelMatch().setEnabled(true);
        render(*matched,.2,left);render(*plain,.2,right);
        require(matched->abLevelMatch().appliedDb()<-4,"Print test never entered an attenuated state");
        matched->setNonRealtime(offline);plain->setNonRealtime(offline);host.recording=!offline;
        std::vector<float> x,y;render(*matched,.1,left,&x);render(*plain,.1,right,&y);
        require(x.size()==y.size() && std::memcmp(x.data(),y.data(),x.size()*sizeof(float))==0,offline?"Offline bounce printed MATCH attenuation (including first samples)":"Recording printed MATCH attenuation (including first samples)");
        require(!matched->abLevelMatch().enabled() && matched->abLevelMatch().appliedDb()==0,"Print protection did not disarm MATCH");
        matched->abLevelMatch().setEnabled(true);x.clear();y.clear();render(*matched,.1,left,&x);render(*plain,.1,right,&y);
        require(x==y && !matched->abLevelMatch().enabled(),"MATCH could be rearmed while printing");
        matched->setNonRealtime(false);plain->setNonRealtime(false);host.recording=false;
        x.clear();y.clear();render(*matched,.1,left,&x);render(*plain,.1,right,&y);
        require(x==y && !matched->abLevelMatch().enabled(),"MATCH automatically resumed after printing");
    }
    matched->setPlayHead(nullptr);plain->setPlayHead(nullptr);
    std::cout<<"PASS "<<(offline?"offline":"recording")<<": real processor bit-exact from first sample, rearm blocked, no automatic resume\n";
}
void offlineSafety(){printSafety(true);}
void recordingSafety(){printSafety(false);}

void restoreSafety() {
    auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(0);p->prepareToPlay(rate,block);Sine s{110};
    set(*p,"output",-7);render(*p,2,s);p->selectComparison(1);set(*p,"output",-1);render(*p,2,s);
    p->abLevelMatch().setEnabled(true);render(*p,.2,s);
    require(p->abLevelMatch().appliedDb()<-4,"Restore test did not establish an active cut");
    juce::MemoryBlock saved;p->getStateInformation(saved);p->copyComparison();
    p->setStateInformation(saved.getData(),int(saved.getSize()));
    require(!p->abLevelMatch().enabled(),"Existing-instance project/reference restore retained MATCH ON");
    render(*p,.1,s);
    require(p->abLevelMatch().appliedDb()==0 && std::isnan(p->abLevelMatch().loudness(0)) && std::isnan(p->abLevelMatch().loudness(1)),"Project restore retained gain/statistics or pending COPY");
    std::cout<<"PASS restore: used instance with active attenuation/pending COPY resets without prepare\n";
}

void bypassSafety() {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(rate,block);
    RecordingPlayHead host;p->setPlayHead(&host);host.recording=true;
    p->abLevelMatch().setEnabled(true);juce::AudioBuffer<float> b(2,block);b.clear();juce::MidiBuffer midi;
    p->processBlockBypassed(b,midi);
    require(!p->abLevelMatch().enabled(),"Host bypass failed to disarm MATCH during recording");
    host.recording=false;host.available=false;p->processBlock(b,midi);
    require(!p->abLevelMatch().outputProtected(),"Unavailable playhead left print protection stuck");
    p->setPlayHead(nullptr);p->setNonRealtime(true);p->abLevelMatch().setEnabled(true);p->processBlock(b,midi);
    require(!p->abLevelMatch().enabled() && p->abLevelMatch().outputProtected(),"Offline safety required a playhead");
    std::cout<<"PASS host paths: bypass recording, unavailable transport, offline without playhead\n";
}

void uiSafety() {
    require(!juce::Desktop::getInstance().getDisplays().displays.isEmpty(),"MATCH UI test requires a working native display (use Tools/run_linux_ui_tests.py on Linux)");
    // Exercise the production enable warning, Cancel path and protected state.
    // No simulated mouse coordinates or mock editor: use actual button actions.
    struct Probe : juce::Timer {
        std::unique_ptr<ChimeraProcessor> p=std::make_unique<ChimeraProcessor>();
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        juce::TextButton* button{};int step{},ticks{};juce::String failure;
        static juce::Component* find(juce::Component& c) {
            if(c.getComponentID()=="ab.match")return &c;
            for(auto* child:c.getChildren())if(auto* found=find(*child))return found;
            return nullptr;
        }
        Probe() {
            p->prepareToPlay(rate,block);editor.reset(p->createEditor());
            button=dynamic_cast<juce::TextButton*>(find(*editor));
            require(button!=nullptr,"MATCH button missing");
            startTimer(20);
        }
        void timerCallback() override {
            try {
                require(++ticks<250,"MATCH warning/protection UI timed out");
                auto* alert=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                if(step==0){button->onClick();require(!p->abLevelMatch().enabled(),"MATCH enabled before warning acknowledgement");++step;}
                else if(step==1 && alert){require(alert->getName().contains("MATCH") && button->getTooltip().contains("OBS"),"Output warning missing");alert->triggerButtonClick("Cancel");++step;}
                else if(step==2 && !alert){require(!p->abLevelMatch().enabled(),"Cancel enabled MATCH");button->onClick();++step;}
                else if(step==3 && alert){alert->triggerButtonClick("Enable MATCH");++step;}
                else if(step==4 && !alert && p->abLevelMatch().enabled()) {
                    button->onClick();require(!p->abLevelMatch().enabled(),"MATCH button failed to switch off");
                    button->onClick();require(p->abLevelMatch().enabled(),"Acknowledged comparison did not re-enable");
                    p->setNonRealtime(true);Sine sine{110};render(*p,.02,sine);++step;
                }
                else if(step==5 && button->getButtonText()=="SAFE") {
                    require(!button->isEnabled() && !p->abLevelMatch().enabled(),"SAFE UI allowed MATCH activation");
                    p->setNonRealtime(false);Sine sine{110};render(*p,.02,sine);
                    p->abLevelMatch().setEnabled(true);static_cast<ChimeraEditor*>(editor.get())->setEasyMode(true);
                    require(!p->abLevelMatch().enabled(),"Easy view hid an active comparison cut");
                    p->abLevelMatch().setEnabled(true);editor.reset();
                    require(!p->abLevelMatch().enabled(),"Closing the editor left MATCH enabled");
                    stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();
                }
            } catch(const std::exception& e) {
                failure=e.what();juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
                stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();
            }
        }
    } probe;
    juce::MessageManager::getInstance()->runDispatchLoop();
    require(probe.failure.isEmpty(),probe.failure);
    std::cout<<"PASS UI: warning/Cancel/enable/OFF, SAFE during print, Easy view/editor close disarm\n";
}
}

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if(argc>1 && juce::String(argv[1])=="--ui") {
        try {uiSafety();return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    }
    int failures=0;
    for(const auto test:{meter,match,processor,reprepare,ramps,edgeCases,recallSafety,restoreSafety,offlineSafety,recordingSafety,bypassSafety})
        try {test();}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';++failures;}
    return failures==0?0:1;
}

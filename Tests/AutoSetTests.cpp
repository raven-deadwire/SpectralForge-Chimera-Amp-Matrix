#include "PluginProcessor.h"
#include "AutoSetController.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>

static thread_local bool counting=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n){if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}

using namespace spectralforge::autoset;
namespace {
void check(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
bool near(float a,float b,float tolerance){return std::abs(a-b)<=tolerance;}

std::vector<Window> windows(int count,float peakDb,float rmsDb){return std::vector<Window>(size_t(count),Window{peakDb,rmsDb});}
void append(std::vector<Window>& to,const std::vector<Window>& from){to.insert(to.end(),from.begin(),from.end());}

void analysisRules() {
    check(!analyseInputGate({},0).playingDetected,"Empty input reported playing");
    check(!analyseInputGate(windows(600,-100,-103),0).playingDetected,"Silence reported playing");
    check(!analyseCompressor(windows(600,-100,-103)).playingDetected,"Silent compressor input reported playing");

    // 1 s pauses with -72 dBFS noise, 5 s of playing peaking at -21 dBFS.
    std::vector<Window> pass;append(pass,windows(100,-72,-78));
    for(int i=0;i<500;++i)pass.push_back({-21.f-float(i%50)*.1f,-30.f-float(i%50)*.1f});
    const auto r=analyseInputGate(pass,0);
    check(r.playingDetected && r.pausesDetected,"Playing with pauses was not recognised");
    check(near(r.inputGainDb,12.f,.01f),"Input gain did not raise a quiet DI toward the target peak (clamped to +12)");
    check(near(r.noisePeakDb,-60.f,.01f),"Noise floor was not shifted by the new input gain");
    check(near(r.gateThresholdDb,-51.f,.01f),"Gate threshold is not noise + hysteresis + margin");
    check(r.gateRangeDb>=12.f,"Gate range below the minimum");

    // A healthy level is left alone; continuous playing does not move the gate.
    std::vector<Window> steady;for(int i=0;i<600;++i)steady.push_back({-9.5f-float(i%40)*.1f,-18.f-float(i%40)*.1f});
    const auto s=analyseInputGate(steady,3.f);
    check(s.inputGainDb==3.f,"Input gain chased a deviation below 2 dB");
    check(!s.pausesDetected,"Continuous playing reported pauses");

    // Noise close to the playing level: threshold sits between them.
    std::vector<Window> noisy;append(noisy,windows(100,-40,-46));append(noisy,windows(500,-12,-20));
    const auto n=analyseInputGate(noisy,0);
    check(n.pausesDetected && n.gateThresholdDb<n.playingPeakDb && n.gateThresholdDb>n.noisePeakDb,"Noisy-input gate threshold is not between noise and playing");

    // Compressor: picked dynamics (high crest, wide range) vs. steady sustain.
    std::vector<Window> picked;for(int i=0;i<600;++i){const float level=-40.f+float(i%30);picked.push_back({level+20.f,level});}
    const auto c=analyseCompressor(picked);
    check(c.playingDetected && c.ratio==4.f && c.attackMs==15.f,"Picked dynamics did not get a higher ratio and slower attack");
    check(c.thresholdDb<=-17.f && c.thresholdDb>=-26.f && c.makeupDb>0.f && c.makeupDb<=12.f,"Picked compressor threshold/makeup out of range");
    const auto sustained=analyseCompressor(windows(600,-14,-18));
    check(sustained.ratio==2.f && sustained.attackMs==3.f && sustained.makeupDb==0.f,"Steady sustain was compressed like picked playing");
    std::cout<<"PASS auto set rules: silence, quiet DI gain, gate threshold/range, unchanged steady input, noisy gate, compressor ratio/attack/makeup\n";
}

void captureWindows() {
    for(double rate:{44100.,48000.,96000.}) {
        Capture capture;capture.prepare(rate);
        std::vector<float> left(static_cast<size_t>(rate)),right(static_cast<size_t>(rate));
        for(size_t n=0;n<left.size();++n){left[n]=float(.5*std::sin(2*juce::MathConstants<double>::pi*220*double(n)/rate));right[n]=.25f*left[n];}
        const float* channels[2]{left.data(),right.data()};
        capture.push(Tap::input,channels,2,int(left.size()));
        std::vector<Window> out;capture.drain(Tap::input,out);
        check(out.empty(),"Capture recorded while not listening");
        capture.listen(Tap::input);
        counting=true;allocations=0;
        for(int offset=0;offset<int(left.size());offset+=256) {
            const float* block[2]{left.data()+offset,right.data()+offset};
            capture.push(Tap::input,block,2,std::min(256,int(left.size())-offset));
            capture.push(Tap::compressor,block,2,std::min(256,int(left.size())-offset));
        }
        counting=false;
        check(allocations==0,"Audio-thread capture allocated");
        capture.drain(Tap::input,out);
        check(out.size()>=99 && out.size()<=100,"One second did not produce 10 ms windows");
        for(const auto& w:out)check(near(w.peakDb,-6.02f,.3f) && near(w.rmsDb,-9.03f,.5f),"Window peak/RMS do not match the louder channel");
        std::vector<Window> other;capture.drain(Tap::compressor,other);
        check(other.empty(),"Inactive tap recorded windows");
    }
    std::cout<<"PASS auto set capture: 10 ms windows at 44.1/48/96 kHz, louder-channel peak/RMS, tap isolation, allocation-free audio path\n";
}

struct Gestures final:juce::AudioProcessorListener {
    int changes{},begins{},ends{};
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
    void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++begins;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++ends;}
};
std::vector<float> values(ChimeraProcessor& p){std::vector<float> v;for(auto* q:p.getParameters())v.push_back(q->getValue());return v;}
float get(ChimeraProcessor& p,const char* id){return p.parameters().getRawParameterValue(id)->load();}

// Plucked DI: decaying harmonic notes separated by noisy pauses.
struct DI {
    double rate;float notePeak,noisePeak;std::mt19937 random{7};double t{};
    float next() {
        const double cycle=std::fmod(t,2.0);t+=1.0/rate;
        std::uniform_real_distribution<float> noise(-noisePeak,noisePeak);
        float x=noise(random);
        if(cycle<1.5) {
            const double f=110*(1+int(t/2)%3*.25),env=std::exp(-cycle*2.5);
            x+=float(notePeak*.7*env*(std::sin(2*juce::MathConstants<double>::pi*f*t)+.4*std::sin(4*juce::MathConstants<double>::pi*f*t)));
        }
        return x;
    }
};
void run(ChimeraProcessor& p,Controller& c,DI* source,double seconds,double& now) {
    const int block=256;juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;
    for(int k=0;k<int(seconds*p.getSampleRate()/block);++k) {
        for(int n=0;n<block;++n){const float x=source?source->next():0.f;b.setSample(0,n,x);b.setSample(1,n,x);}
        p.processBlock(b,midi);now+=1000.0*block/p.getSampleRate();
        if(k%4==0)c.tick(now);
    }
    c.tick(now);
}

void controllerIntegration() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    double now=juce::Time::getMillisecondCounterHiRes();
    {   // Silence: fails without touching parameters.
        Controller c(*p);const auto before=values(*p);c.start();run(*p,c,nullptr,7,now);
        check(c.current().phase==Controller::Phase::failed && values(*p)==before,"Silence changed parameters or did not fail");
    }
    {   // No audio callbacks: times out.
        Controller c(*p);const auto before=values(*p);c.start();c.tick(now);check(c.busy(),"Auto Set gave up before the timeout");c.tick(now+Controller::noAudioTimeoutMs+500);
        check(c.current().phase==Controller::Phase::failed && values(*p)==before,"Missing audio did not time out cleanly");
    }
    Controller c(*p);Gestures g;p->addListener(&g);
    const auto before=values(*p);std::vector<Controller::Status> statuses;c.onStatus=[&](const auto& s){statuses.push_back(s);};
    DI di{48000,.126f,.00025f}; // notes peak ~ -18 dBFS, noise ~ -72 dBFS
    c.start();check(c.busy(),"Auto Set did not start listening");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::compressor,"First pass did not hand over to the compressor pass");
    check(get(*p,"input")>4.f && get(*p,"input")<=12.f,"Input gain was not raised toward the target");
    check(get(*p,"gateon")>.5f && get(*p,"gatethreshold")>-70.f && get(*p,"gatethreshold")<-35.f,"Gate threshold was not placed above the noise floor");
    run(*p,c,&di,7.0,now);
    p->removeListener(&g);
    check(c.current().phase==Controller::Phase::done && c.current().canUndo,"Compressor pass did not finish with undo available");
    check(get(*p,"cleanCompOn")>.5f && get(*p,"cleanCompDetector")>.5f && get(*p,"cleanCompMix")==1.f,"Clean compressor was not enabled in RMS mode");
    check(get(*p,"cleanCompRatio")>=2.f && get(*p,"cleanCompRatio")<=4.f && get(*p,"cleanCompThreshold")<0.f,"Compressor ratio/threshold out of range");
    check(g.begins==g.ends && g.begins>0 && g.changes>=g.begins,"Parameter writes were not balanced host gestures");
    bool progressed=false;for(const auto& s:statuses)progressed|=s.progress>0 && s.progress<1;
    check(progressed && c.current().text.contains("Comp"),"Status did not report progress and a summary");
    std::cout<<"AUTO SET synthetic DI (-18 dBFS notes, -72 dBFS noise): "<<c.current().text<<"\n";
    const auto applied=values(*p);
    int changed=0;for(size_t i=0;i<before.size();++i)if(applied[i]!=before[i])++changed;
    check(changed>=3 && changed<=13,"Auto Set touched parameters outside its scope");
    c.undo();
    check(values(*p)==before && !c.current().canUndo,"Undo did not restore every parameter exactly");
    std::cout<<"PASS auto set controller: silence/no-audio failures, two passes on the real processor, gate+input+compressor applied as host gestures, progress/summary, exact undo\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        analysisRules();captureWindows();controllerIntegration();
        std::cout<<"PASS Auto Set\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

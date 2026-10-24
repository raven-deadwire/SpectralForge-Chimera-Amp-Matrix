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

// Noise whose third-octave spectrum follows the target curve exactly, built
// by inverse FFT with random phases and looped.
std::vector<float> targetNoise(bool bass,double rate,double seconds,std::mt19937& random) {
    constexpr int order=15,size=1<<order;
    juce::dsp::FFT fft(order);std::vector<float> spectrum(size_t(2*size));
    std::uniform_real_distribution<double> phase(0,2*juce::MathConstants<double>::pi);
    for(int k=1;k<size/2;++k) {
        const double f=k*rate/size;
        // Band power is the sum of bins; third-octave width grows with f.
        const double magnitude=std::sqrt(std::pow(10.,toneTargetDb(bass,float(std::clamp(f,20.,20000.)))/10.)/f)*(f<35?0:1);
        const double ph=phase(random);spectrum[size_t(2*k)]=float(magnitude*std::cos(ph));spectrum[size_t(2*k+1)]=float(magnitude*std::sin(ph));
    }
    fft.performRealOnlyInverseTransform(spectrum.data());
    float peak=0;for(int n=0;n<size;++n)peak=std::max(peak,std::abs(spectrum[size_t(n)]));
    std::vector<float> out(size_t(seconds*rate));
    for(size_t n=0;n<out.size();++n)out[n]=.25f*spectrum[n%size_t(size)]/peak;
    return out;
}
void shelve(std::vector<float>& x,double frequency,double gain,double rate) {
    const auto c=spectralforge::eqCoefficients(2,frequency,gain,.707,rate);double z1=0,z2=0;
    for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}
// Plucked notes: fundamentals in the instrument's range, harmonics falling at 6 dB/octave.
std::vector<float> notes(double rate,double seconds,std::array<double,3> fundamentals,int harmonics) {
    std::vector<float> out(size_t(seconds*rate));
    for(size_t n=0;n<out.size();++n) {
        const double t=double(n)/rate,cycle=std::fmod(t,1.),f=fundamentals[size_t(int(t)%3)];double x=0;
        for(int h=1;h<=harmonics && f*h<rate*.45;++h)x+=std::sin(2*juce::MathConstants<double>::pi*f*h*t+h)/h;
        out[n]=float(.2*std::exp(-cycle*2)*x);
    }
    return out;
}

void toneRules() {
    std::mt19937 random{11};
    for(double rate:{44100.,48000.,96000.}) {
        check(!analyseTone(std::vector<float>(size_t(rate*6),0.f),rate).playingDetected,"Silence reported a tone");
        for(bool bass:{false,true}) {
            const auto flat=analyseTone(targetNoise(bass,rate,6,random),rate);
            check(flat.playingDetected && flat.bass==bass,"Instrument detection failed on a signal shaped like its own target");
            check(flat.activeMoves()<=1 && flat.errorBeforeDb<1.5f,"A signal already on target was re-equalised");
            // 8 dB too dark above 3 kHz: the fit must brighten, within limits.
            auto dark=targetNoise(bass,rate,6,random);shelve(dark,3000,-8,rate);
            const auto d=analyseTone(dark,rate);
            check(d.playingDetected && d.bass==bass,"Darkened signal changed the detected instrument");
            check(d.moves[6].gainDb>=2.f && d.moves[0].gainDb<=1.f && d.errorAfterDb<.6f*d.errorBeforeDb,"Darkened top end was not brightened");
            for(const auto& m:d.moves)check(std::abs(m.gainDb)<=toneMaxMoveDb && std::fmod(std::abs(m.gainDb),.5f)==0.f,"Tone move outside its limits or step");
            auto bright=targetNoise(bass,rate,6,random);shelve(bright,3000,8,rate);
            check(analyseTone(bright,rate).moves[6].gainDb<=-2.f,"Bright top end was not tamed");
        }
        check(!analyseTone(notes(rate,6,{110,147,196},40),rate).bass,"Guitar notes detected as bass");
        check(analyseTone(notes(rate,6,{41,55,73},6),rate).bass,"Bass notes detected as guitar");
        const auto forced=analyseTone(notes(rate,6,{110,147,196},40),rate,true);
        check(forced.bass && forced.targetDb[0]==toneTargetDb(true,toneBandCentres[0]),"A chosen instrument was overridden by detection");
    }
    std::cout<<"PASS auto set tone rules: silence, guitar/bass detection, on-target input left alone, dark/bright tilt corrected within +/-6 dB in 0.5 dB steps at 44.1/48/96 kHz\n";
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
        std::vector<float> tone;capture.drainTone(tone);check(tone.empty(),"Inactive tone tap recorded samples");
        capture.listen(Tap::tone);counting=true;allocations=0;
        capture.push(Tap::tone,channels,2,4096);capture.push(Tap::input,channels,2,4096);
        counting=false;check(allocations==0,"Audio-thread tone capture allocated");
        out.clear();capture.drainTone(tone);capture.drain(Tap::input,out);
        check(tone.size()==4096 && out.empty(),"Tone tap did not record exactly its own samples");
        for(size_t n=0;n<tone.size();++n)check(near(tone[n],.625f*left[n],1e-6f),"Tone tap is not the channel average");
    }
    std::cout<<"PASS auto set capture: 10 ms windows at 44.1/48/96 kHz, louder-channel peak/RMS, mono tone tap, tap isolation, allocation-free audio path\n";
}

struct Gestures final:juce::AudioProcessorListener {
    int changes{},begins{},ends{};
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
    void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++begins;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++ends;}
};
std::vector<float> values(ChimeraProcessor& p){std::vector<float> v;for(auto* q:p.getParameters())v.push_back(q->getValue());return v;}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}

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
    auto p=std::make_unique<ChimeraProcessor>();
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    setRaw(spectralforge::eqID(0,1,"enabled"),1);setRaw(spectralforge::eqID(0,1,"frequency"),700);setRaw(spectralforge::eqID(0,1,"gain"),-3);
    setRaw(spectralforge::dynamicSlotID(0,"target"),2);
    p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
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
    check(c.current().phase==Controller::Phase::tone && c.current().text.contains("2/3"),"First pass did not hand over to the Tone EQ pass");
    check(get(*p,"input")>4.f && get(*p,"input")<=12.f,"Input gain was not raised toward the target");
    check(get(*p,"gateon")>.5f && get(*p,"gatethreshold")>-70.f && get(*p,"gatethreshold")<-35.f,"Gate threshold was not placed above the noise floor");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::compressor,"Tone EQ pass did not hand over to the compressor pass");
    const auto& tone=c.lastToneAnalysis();
    check(tone.playingDetected && !tone.bass,"Tone pass did not hear the guitar DI");
    // The Dynamic EQ node on Tone band 2 is the user's: Auto Set must not move it.
    check(get(*p,spectralforge::eqID(0,1,"frequency"))==700.f && get(*p,spectralforge::eqID(0,1,"gain"))==-3.f,"Auto Set overwrote a Dynamic EQ node");
    int enabled=0;
    for(int b=0;b<spectralforge::graphicalEQBands;++b)if(b!=1 && get(*p,spectralforge::eqID(0,b,"enabled"))>.5f) {
        ++enabled;check(std::abs(get(*p,spectralforge::eqID(0,b,"gain")))<=toneMaxMoveDb,"Tone EQ move beyond the limit");
    }
    check(enabled==tone.activeMoves() && (enabled==0 || get(*p,spectralforge::eqID(0,-1,"bypass"))<.5f),"Applied Tone EQ does not match the analysis");
    run(*p,c,&di,7.0,now);
    p->removeListener(&g);
    check(c.current().phase==Controller::Phase::done && c.current().canUndo,"Compressor pass did not finish with undo available");
    check(get(*p,"cleanCompOn")>.5f && get(*p,"cleanCompDetector")>.5f && get(*p,"cleanCompMix")==1.f,"Clean compressor was not enabled in RMS mode");
    check(get(*p,"cleanCompRatio")>=2.f && get(*p,"cleanCompRatio")<=4.f && get(*p,"cleanCompThreshold")<0.f,"Compressor ratio/threshold out of range");
    check(g.begins==g.ends && g.begins>0 && g.changes>=g.begins,"Parameter writes were not balanced host gestures");
    bool progressed=false;for(const auto& s:statuses)progressed|=s.progress>0 && s.progress<1;
    check(progressed && c.current().text.contains("Comp"),"Status did not report progress and a summary");
    std::cout<<"AUTO SET synthetic DI (-18 dBFS notes, -72 dBFS noise): "<<c.current().text<<"\n";
    std::cout<<"AUTO SET tone moves:";for(const auto& m:tone.moves)std::cout<<' '<<m.frequency<<"Hz "<<m.gainDb<<"dB";
    std::cout<<" | fit error "<<tone.errorBeforeDb<<" -> "<<tone.errorAfterDb<<" dB\n";
    const auto applied=values(*p);
    int changed=0;for(size_t i=0;i<before.size();++i)if(applied[i]!=before[i])++changed;
    check(changed>=3 && changed<=14+5*spectralforge::graphicalEQBands,"Auto Set touched parameters outside its scope");
    c.undo();
    check(values(*p)==before && !c.current().canUndo,"Undo did not restore every parameter exactly");
    c.setToneMode(Controller::ToneMode::off);c.start();
    check(c.current().phase==Controller::Phase::input && c.current().text.contains("1/2"),"Excluding the Tone EQ did not shorten Auto Set to two passes");
    c.cancel();check(values(*p)==before,"Cancelling a run changed parameters");
    std::cout<<"PASS auto set controller: silence/no-audio failures, three passes on the real processor, input+gate+Tone EQ+compressor applied as host gestures, Dynamic EQ node kept, progress/summary, exact undo, tone pass optional\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        analysisRules();toneRules();captureWindows();controllerIntegration();
        std::cout<<"PASS Auto Set\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

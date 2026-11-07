// Auto Set instrument detection on a synthetic corpus: plucked guitar and bass
// (Karplus-Strong: single notes, chords, palm-muted low riffs, 5-string and
// slap bass), sung vowels (glottal pulses through formants, vibrato, glides,
// breath), keys (organ, synth pad, synth lead, piano), drums, and silence.
//   ChimeraAutoSetInstrumentTests                  run the corpus
//   ChimeraAutoSetInstrumentTests --classify a.wav  classify a recording (mono mix)
// The corpus is synthetic: it checks the rules and their safety margins, not
// accuracy on real recordings, which must be measured separately.
#include "../Source/AutoSetInstrument.h"
#include "../Source/AutoSetController.h"
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>

using namespace spectralforge::autoset;
using Controller=spectralforge::autoset::Controller;
namespace {
constexpr double rate=48000,seconds=8,twoPi=2*juce::MathConstants<double>::pi;
void check(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
// Portable random numbers: the standard distributions differ between
// libstdc++, libc++ and MSVC, so the corpus would differ per platform.
struct Uniform {double a,b;template<class R> double operator()(R& r) const {return a+(b-a)*(double(r())/4294967296.0);}};
struct Normal {double mean,sd;template<class R> double operator()(R& r) const {
    const double u1=std::max(1e-12,double(r())/4294967296.0),u2=double(r())/4294967296.0;
    return mean+sd*std::sqrt(-2*std::log(u1))*std::cos(2*juce::MathConstants<double>::pi*u2);}};
double midiHz(double note){return 440*std::pow(2.,(note-69)/12);}
std::vector<float> blank(){return std::vector<float>(size_t(rate*seconds),0.f);}
void addNoise(std::vector<float>& x,float level,std::mt19937& rng){Normal n{0.f,level};for(auto& v:x)v+=float(n(rng));}
void normalise(std::vector<float>& x,float peak){float m=0;for(float v:x)m=std::max(m,std::abs(v));if(m>0)for(auto& v:x)v*=peak/m;}

// Karplus-Strong string with a one-pole loss filter; `bright` shapes the pluck.
void pluck(std::vector<float>& x,size_t at,double hz,double length,double decaySeconds,float gain,float bright,std::mt19937& rng) {
    const double period=rate/hz;const int delay=std::max(2,int(period));const double frac=period-delay;
    std::vector<double> line(size_t(delay)+2,0.);Uniform u{-1,1};
    double lp=0;for(auto& v:line){const double n=u(rng);lp+=bright*(n-lp);v=lp;}
    // Classic Karplus-Strong: each period averages neighbours, scaled so the
    // note falls 60 dB in decaySeconds.
    const double feedback=std::pow(10.,-3./(decaySeconds*hz));(void)frac;
    size_t index=0;
    const size_t count=std::min(x.size()-std::min(x.size(),at),size_t(length*rate));
    for(size_t n=0;n<count;++n) {
        const size_t next=(index+1)%size_t(delay);
        const double current=line[index];
        line[index]=feedback*.5*(current+line[next]);index=next;
        const double release=n+size_t(.02*rate)>count?double(count-n)/(.02*rate):1.;
        x[at+n]+=float(gain*current*release);
    }
}
std::vector<float> guitar(int variant,unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Uniform u{0,1};
    const std::array<int,6> open{40,45,50,55,59,64}; // E2 A2 D3 G3 B3 E4
    double t=.1;
    while(t<seconds-.6) {
        if(variant==0) {          // single-note lines across the neck
            const double note=48+std::floor(u(rng)*30),length=.25+u(rng)*.6;
            pluck(x,size_t(t*rate),midiHz(note),length,1.2+u(rng),.6f,.5f,rng);t+=length;
        } else if(variant==1) {   // strummed open chords
            const int shape=int(u(rng)*3);const std::array<std::array<int,6>,3> chords{{{0,2,2,1,0,0},{-1,0,2,2,2,0},{3,2,0,0,0,3}}};
            for(int s=0;s<6;++s)if(chords[size_t(shape)][size_t(s)]>=0)
                pluck(x,size_t((t+s*.012)*rate),midiHz(open[size_t(s)]+chords[size_t(shape)][size_t(s)]),1.4,2.5,.35f,.6f,rng);
            t+=.7+u(rng)*.5;
        } else {                  // palm-muted riffs on drop D / 7-string B
            const double root=seed%2?35:38,length=.12+u(rng)*.15;   // B1 (62 Hz) or D2 (73 Hz)
            pluck(x,size_t(t*rate),midiHz(root+(u(rng)<.25?7:0)),length,.35,.7f,.35f,rng);
            if(u(rng)<.3)pluck(x,size_t(t*rate),midiHz(root+7),length,.35,.5f,.35f,rng);
            t+=length+.03;
            if(u(rng)<.15){pluck(x,size_t(t*rate),midiHz(64+std::floor(u(rng)*12)),.5,1.5,.5f,.5f,rng);t+=.5;}
        }
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
// A guitar through a drive pedal (Tap::amp hears pedals): clipped, so notes
// hold instead of decaying; some held notes get finger vibrato.
std::vector<float> drivenGuitar(unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Uniform u{0,1};
    double t=.1;
    while(t<seconds-.8) {
        const double note=40+std::floor(u(rng)*36),length=.3+u(rng)*1.2;
        pluck(x,size_t(t*rate),midiHz(note),length,4.,.6f,.5f,rng);t+=length;
    }
    // Finger vibrato: re-read with a slowly varying delay on the second half.
    std::vector<float> y(x);
    for(size_t n=x.size()/2;n<x.size();++n){const double d=12+10*std::sin(twoPi*5.5*double(n)/rate);const auto i=size_t(double(n)-d);y[n]=x[std::min(i,x.size()-1)];}
    for(auto& v:y)v=float(std::tanh(40*v)); // heavy clipping
    addNoise(y,.0003f,rng);normalise(y,.5f);return y;
}
// DEADWIRE-style Drop A guitar: chugs on A1 (55 Hz), power chords, a few leads.
std::vector<float> dropAGuitar(unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Uniform u{0,1};
    double t=.1;
    while(t<seconds-.6) {
        const double length=.12+u(rng)*.2,root=33; // A1
        pluck(x,size_t(t*rate),midiHz(root),length,.4,.7f,.35f,rng);
        if(u(rng)<.4){pluck(x,size_t(t*rate),midiHz(root+7),length,.4,.5f,.35f,rng);pluck(x,size_t(t*rate),midiHz(root+12),length,.4,.4f,.35f,rng);}
        t+=length+.03;
        if(u(rng)<.08){pluck(x,size_t(t*rate),midiHz(57+std::floor(u(rng)*12)),.5,1.5,.5f,.5f,rng);t+=.5;}
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
// Drop A bass (A0 = 27.5 Hz) following the same riff an octave down.
std::vector<float> dropABass(unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Uniform u{0,1};
    double t=.1;
    while(t<seconds-.6) {
        const double length=.2+u(rng)*.4,note=21+(u(rng)<.3?std::floor(u(rng)*8):0); // A0 and a few notes above
        pluck(x,size_t(t*rate),midiHz(note),length,2.5,.8f,.25f,rng);t+=length+.02;
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
// Validation console items (chimera-autoset-validation): additive notes with
// 1/h harmonics. det01: 110/220 Hz, 150 ms swell, +/-50 cents 5.5 Hz vibrato,
// 1.5 s; det02: 60 ms plucks every 250 ms on A1 55 Hz / A0 27.5 Hz; det03:
// 150 ms fifths on A1. `drive`: tanh 40.
std::vector<float> validationInput(int item,double f0,bool drive) {
    auto x=blank();
    const auto note=[&](double hz,double start,double length,double attack,double decay,double vibrato){
        const size_t at=size_t(start*rate);double phase=0;
        for(size_t n=0;n<size_t(length*rate) && at+n<x.size();++n) {
            const double t=double(n)/rate,cents=vibrato*std::sin(twoPi*5.5*t)*std::clamp((t-.2)/.2,0.,1.);
            phase+=twoPi*hz*std::pow(2.,cents/1200)/rate;
            double v=0;for(int h=1;h<=10;++h)v+=std::sin(h*phase)/h;
            x[at+n]+=float(v*(attack>0?std::min(1.,t/attack):1.)*std::exp(-t/decay)*std::clamp((length-t)/.02,0.,1.));
        }
    };
    if(item==1){int k=0;for(double t=.1;t<seconds-1.6;t+=1.7,++k)note(k%2?2*f0:f0,t,1.5,.15,2.5,50);}
    else if(item==2){for(double t=.1;t<seconds-.1;t+=.25)note(f0,t,.06,.0005,.4,0);}
    else {for(double t=.1;t<seconds-.2;t+=.25){note(f0,t,.15,.0005,.4,0);note(f0*1.5,t,.15,.0005,.4,0);}}
    std::mt19937 rng(9);addNoise(x,.0003f,rng);normalise(x,.5f);
    if(drive){for(auto& v:x)v=float(std::tanh(40*v));normalise(x,.5f);}
    return x;
}
std::vector<float> bass(int variant,unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Uniform u{0,1};
    double t=.1;
    while(t<seconds-.6) {
        const double low=variant==1?23:28;                      // 5-string from B0, 4-string from E1
        const double note=low+std::floor(u(rng)*(variant==1?24:22)),length=.25+u(rng)*.5;
        pluck(x,size_t(t*rate),midiHz(note),length,variant==2?1.5:3.,.8f,variant==2?.9f:.25f,rng);
        t+=length+.02;
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
// Sung vowels: Rosenberg glottal pulses through four formant resonators,
// vibrato after the onset, jitter and shimmer, breath noise, legato glides.
std::vector<float> vocal(bool female,unsigned seed) {
    std::mt19937 rng(seed);auto source=blank();Uniform u{0,1};Normal g{0,1};
    const double scale=female?1.17:1.;
    const std::array<std::array<double,4>,4> formants{{{730,1090,2440,3400},{530,1840,2480,3400},{300,870,2240,3300},{570,840,2410,3300}}};
    std::vector<int> vowelAt(source.size(),0);
    double t=.15,phase=0,hz=0;const double low=female?55:43,high=female?76:64; // F3-E5 / G2-E4
    std::vector<double> amplitude(source.size(),0.),pitch(source.size(),0.);
    while(t<seconds-.8) {
        const double phrase=1.5+u(rng)*1.5;int vowel=int(u(rng)*4);
        double note=low+std::floor(u(rng)*(high-low));
        for(double p=0;p<phrase;) {
            const double length=.4+u(rng)*.8,target=midiHz(note);
            for(size_t n=size_t((t+p)*rate);n<std::min(source.size(),size_t((t+p+length)*rate));++n) {
                const double local=double(n)/rate-(t+p),fromPhrase=double(n)/rate-t;
                const double glide=p>0?std::min(1.,local/.08):1.;
                const double startHz=hz>0?hz:target,base=startHz+(target-startHz)*glide;
                const double depth=std::min(1.,std::max(0.,(local-.15)/.2))*(35+15*std::sin(seed+0.))/1200.;
                pitch[n]=base*std::pow(2.,depth*std::sin(twoPi*(5.3+.6*std::sin(double(seed)))*local));
                amplitude[n]=std::min(1.,fromPhrase/.09)*std::min(1.,(t+phrase-double(n)/rate)/.12)*(.8+.2*std::sin(twoPi*.7*local));
                vowelAt[n]=vowel;
            }
            hz=target;p+=length;note+=std::round(g(rng)*2);note=std::clamp(note,low,high);if(u(rng)<.3)vowel=int(u(rng)*4);
        }
        t+=phrase+.3+u(rng)*.4;hz=0;
    }
    // Glottal source.
    double jitter=1;
    for(size_t n=0;n<source.size();++n) {
        if(amplitude[n]<=0){phase=0;continue;}
        phase+=pitch[n]*jitter/rate;if(phase>=1){phase-=1;jitter=1+.006*g(rng);}
        const double open=.6,pulse=phase<open?.5*(1-std::cos(juce::MathConstants<double>::pi*phase/open*1.0)):(phase<open+.12?std::cos(juce::MathConstants<double>::pi*(phase-open)/.24):0);
        source[n]=float(amplitude[n]*(pulse-.3)+amplitude[n]*.04*g(rng));
    }
    // Formants (two-pole resonators in parallel, vowel per sample).
    std::vector<float> x(source.size(),0.f);
    std::array<std::array<double,2>,4> state{};
    for(size_t n=0;n<source.size();++n) {
        double y=0;
        for(int k=0;k<4;++k) {
            const double fc=formants[size_t(vowelAt[n])][size_t(k)]*scale,bw=60+fc*.06,r=std::exp(-juce::MathConstants<double>::pi*bw/rate);
            const double a1=2*r*std::cos(twoPi*fc/rate),a2=-r*r;auto& s=state[size_t(k)];
            const double out=(1-r)*source[n]+a1*s[0]+a2*s[1];s[1]=s[0];s[0]=out;y+=out*(k==0?1.:k==1?.7:.35);
        }
        x[n]=float(y);
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
std::vector<float> keys(int variant,unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Uniform u{0,1};
    double t=.1;
    while(t<seconds-.8) {
        const double length=.6+u(rng)*1.4,root=48+std::floor(u(rng)*15);
        std::vector<double> notes{root};if(variant!=2){notes.push_back(root+4);notes.push_back(root+7);}
        const size_t start=size_t(t*rate),count=std::min(x.size()-start,size_t(length*rate));
        for(double note:notes) {
            const double hz=midiHz(note);double ph=u(rng);
            for(size_t n=0;n<count;++n) {
                const double time=double(n)/rate,attack=variant==1?std::min(1.,time/.25):std::min(1.,time/.004);
                const double release=std::min(1.,(length-time)/.03);double v=0;
                if(variant==3) {          // piano: fast attack, decaying partials
                    for(int h=1;h<=8;++h)v+=std::sin(twoPi*hz*h*time*(1+.0004*h*h))*std::exp(-time*(1.2+.6*h))/h;
                } else if(variant==0) {   // organ drawbars
                    for(int h:{1,2,3,4,6,8})v+=std::sin(twoPi*hz*h*time+h)/h;
                } else {                  // saw pad / lead, gently low-passed
                    ph+=hz/rate;ph-=std::floor(ph);v=0;for(int h=1;h<=12;++h)v+=std::sin(twoPi*h*ph)/h*std::exp(-h*.15);
                }
                x[start+n]+=float(.3*attack*release*v);
            }
        }
        t+=length+(variant==3?.05:.15+u(rng)*.3);
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
std::vector<float> drums(int variant,unsigned seed) {
    std::mt19937 rng(seed);auto x=blank();Normal g{0,1};Uniform u{0,1};
    const double step=60./(variant?140:100)/2; // eighth notes
    for(int k=0;k*step<seconds-.3;++k) {
        const size_t at=size_t((.1+k*step)*rate);
        const auto hit=[&](int type,float gain) {
            double lp=0,hp=0,ph=0;
            for(size_t n=0;n<size_t(.4*rate) && at+n<x.size();++n) {
                const double time=double(n)/rate,noise=g(rng);double v=0;
                if(type==0){ph+=(50+90*std::exp(-time*35))/rate;v=std::sin(twoPi*ph)*std::exp(-time*14)+.3*noise*std::exp(-time*120);}
                else if(type==1){lp+=.35*(noise-lp);ph+=185/rate;v=(.8*lp+.4*std::sin(twoPi*ph))*std::exp(-time*22);}
                else{hp=noise-hp*.0;v=(noise-lp)*std::exp(-time*70);lp+=.6*(noise-lp);}
                x[at+n]+=float(gain*v);
            }
        };
        if(k%4==0 || (variant && u(rng)<.2))hit(0,.9f);
        if(k%4==2)hit(1,.7f);
        hit(2,.25f);
    }
    addNoise(x,.0003f,rng);normalise(x,.5f);return x;
}
// Auto Set's own test DI (AutoSetTests.cpp): decaying two-harmonic notes.
std::vector<float> autoSetDI() {
    std::vector<float> x(size_t(rate*seconds));std::mt19937 random{7};Uniform noise{-.00025f,.00025f};
    for(size_t n=0;n<x.size();++n) {
        const double t=double(n)/rate,cycle=std::fmod(t,2.0);float v=noise(random);
        if(cycle<1.5){const double f=110*(1+int(t/2)%3*.25),env=std::exp(-cycle*2.5);v+=float(.126*.7*env*(std::sin(twoPi*f*t)+.4*std::sin(2*twoPi*f*t)));}
        x[n]=v;
    }
    return x;
}

struct Case {std::string name;Source expected;std::vector<Source> accepted;std::vector<float> audio;};
void print(const std::string& name,const InstrumentResult& r) {
    const auto& f=r.features;
    std::cout<<std::left<<std::setw(22)<<name<<" -> "<<std::setw(12)<<sourceName(r.source)<<std::right<<std::fixed<<std::setprecision(2)
             <<" conf="<<r.confidence<<" voiced="<<f.voicedFraction<<" f0="<<std::setprecision(0)<<f.f0Low<<"/"<<f.f0Median<<"/"<<f.f0High
             <<std::setprecision(1)<<" attack="<<f.attackMs<<"ms decay="<<f.decayDbPerSecond<<"dB/s short="<<std::setprecision(2)<<f.shortDecayFraction
             <<std::setprecision(1)<<" vib="<<f.vibratoCents<<"c glide="<<std::setprecision(2)<<f.glideFraction<<" held="<<f.heldSegments<<" notes="<<f.notes
             <<std::setprecision(1)<<" low="<<f.lowRatioDb<<"dB fund="<<f.fundamentalDb<<"dB pitchedHits="<<std::setprecision(2)<<f.pitchedOnsetFraction<<std::setprecision(1)<<" noise="<<f.noiseRatioDb<<"dB flat="<<std::setprecision(3)<<f.flatness<<std::setprecision(1)<<" bass="<<r.bass<<(r.bassDecisive?"(pitch)":"(energy)")<<'\n';
}
int classifyFile(const juce::File& file) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    check(reader!=nullptr,"Cannot read "+file.getFullPathName().toStdString());
    juce::AudioBuffer<float> buffer(int(reader->numChannels),int(std::min<juce::int64>(reader->lengthInSamples,juce::int64(reader->sampleRate*60))));
    reader->read(&buffer,0,buffer.getNumSamples(),0,true,true);
    std::vector<float> mono(size_t(buffer.getNumSamples()));
    for(int c=0;c<buffer.getNumChannels();++c)for(int n=0;n<buffer.getNumSamples();++n)mono[size_t(n)]+=buffer.getSample(c,n)/float(buffer.getNumChannels());
    // Auto Set hears 6 s per pass; classify consecutive 6 s pieces and the whole file.
    const size_t piece=size_t(reader->sampleRate*6);
    for(size_t start=0;start+piece<=mono.size();start+=piece)
        print(file.getFileName().toStdString()+" @"+std::to_string(int(double(start)/reader->sampleRate))+"s",
              classifyInstrument({mono.begin()+std::ptrdiff_t(start),mono.begin()+std::ptrdiff_t(start+piece)},reader->sampleRate));
    print(file.getFileName().toStdString()+" (all)",classifyInstrument(mono,reader->sampleRate));
    return 0;
}
// Auto Set on the real processor, instrument "detected" (the default).
// A voice: input, gates and the dynamics run; every amp, boost, cabinet and
// routing step and both EQ target curves are kept, and their parameters do
// not move. The same voice with Guitar chosen, and a guitar, run those steps.
struct ControllerRun {Controller::Status status;std::vector<juce::String> changed;InstrumentResult heard;};
ControllerRun runAutoSet(const std::vector<float>& audio,int instrument,bool skip=true) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,256);p->prepareToPlay(rate,256);
    std::map<juce::String,float> before;for(auto* q:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(q))before[r->paramID]=r->getValue();
    Controller c(*p);Controller::Options o;o.instrument=instrument;o.skipForVoiceOrDrums=skip;c.setOptions(o);c.start();check(c.busy(),"Auto Set did not start");
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;double now=juce::Time::getMillisecondCounterHiRes();size_t at=0;
    for(int k=0;c.busy() && k<int(240*rate/256);++k) {
        for(int n=0;n<256;++n){const float v=audio[at++%audio.size()];block.setSample(0,n,v);block.setSample(1,n,v);}
        p->processBlock(block,midi);now+=1000.*256/rate;if(k%4==0)c.tick(now);
    }
    ControllerRun r{c.current(),{},c.lastInstrumentDetection()};
    for(auto* q:p->getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(q))if(ranged->getValue()!=before[ranged->paramID])r.changed.push_back(ranged->paramID);
    return r;
}
bool ampSide(const juce::String& id) {
    const auto low=id.toLowerCase();
    if(low.startsWith("dynamiceq"))return false; // the Dynamic EQ step runs ("dynamic" also contains "mic")
    // Tone EQ bands are left out: the Dynamic EQ step (which runs) places its
    // nodes on them; the Tone EQ target step is checked as kept above.
    for(const char* part:{"ampnative","amp1","amp2","amp3","ampmodel","channel","cab","mic","pedal","boost","dual","matrix","lane"})if(low.contains(part))return true;
    return false;
}
void controllerSources() {
    std::vector<float> voice;for(unsigned seed:{34u,35u})for(float v:vocal(seed==35,seed))voice.push_back(v*.25f);   // peaks near -18 dBFS
    const auto sung=runAutoSet(voice,0);
    for(const auto& step:sung.status.steps)std::cout<<"  vocal step "<<int(step.phase)<<(step.changed?" changed: ":" kept: ")<<step.result<<'\n';
    print("controller heard",sung.heard);
    check(sung.status.phase==Controller::Phase::done,"Auto Set on a voice did not finish: "+sung.status.text.toStdString());
    check(sung.heard.source==Source::vocal,std::string("The voice was detected as ")+sourceName(sung.heard.source));
    check(!sung.status.steps.empty() && sung.status.steps[0].result.contains("Vocal (detected"),"The input step does not say what was heard");
    int keptForVoice=0;
    for(const auto& step:sung.status.steps) {
        using P=Controller::Phase;
        const bool guitarOnly=step.phase==P::boost || step.phase==P::channel || step.phase==P::amp || step.phase==P::power || step.phase==P::cab || step.phase==P::cuts
                           || step.phase==P::mics || step.phase==P::rigs || step.phase==P::dual || step.phase==P::matrix || step.phase==P::polarity || step.phase==P::tone;
        if(guitarOnly){check(!step.changed,"A guitar-only step ran on a voice: "+step.result.toStdString());keptForVoice+=step.result.contains("Vocal heard");}
        if(step.phase==P::level)check(step.result.contains("Final EQ kept (Vocal heard"),"The Final EQ target ran on a voice: "+step.result.toStdString());
    }
    check(keptForVoice>=6,"Too few guitar-only steps reported for the voice: "+std::to_string(keptForVoice));
    for(const auto& id:sung.changed)check(!ampSide(id),"Auto Set changed "+id.toStdString()+" on a voice");
    check(std::find(sung.changed.begin(),sung.changed.end(),juce::String("input"))!=sung.changed.end(),"Input gain was not set for the voice");
    // Default options: skipping is off, so a detected voice keeps every step and says so.
    check(!Controller::Options{}.skipForVoiceOrDrums,"Skipping for a voice or drums is on by default");
    const auto shown=runAutoSet(voice,0,false);
    check(shown.status.phase==Controller::Phase::done && shown.heard.source==Source::vocal
          && shown.status.steps[0].result.contains("amp and cabinet steps still run")
          && std::none_of(shown.status.steps.begin(),shown.status.steps.end(),[](const auto& s){return s.result.contains("Vocal heard");}),
          "With skipping off, a detected voice lost a step or was not reported");
    // Guitar chosen: the options win over the detection.
    const auto forced=runAutoSet(voice,1);
    check(forced.status.phase==Controller::Phase::done && std::none_of(forced.status.steps.begin(),forced.status.steps.end(),[](const auto& s){return s.result.contains("Vocal heard");}),
          "Choosing Guitar did not run the guitar steps");
    // A guitar keeps every step.
    std::vector<float> played;for(unsigned seed:{3u,4u})for(float v:guitar(0,seed))played.push_back(v*.25f);
    const auto strummed=runAutoSet(played,0);
    check(strummed.status.phase==Controller::Phase::done && strummed.heard.source==Source::guitar,"The guitar run did not finish as guitar");
    check(std::none_of(strummed.status.steps.begin(),strummed.status.steps.end(),[](const auto& s){return s.result.contains(" heard: this step is for guitar");}),"A guitar lost a step");
    std::cout<<"PASS Auto Set with instrument detection: off by default a voice keeps every step (reported); with skipping on it keeps "<<keptForVoice<<" guitar/bass steps and every amp/cab/routing/Tone EQ parameter; Guitar chosen overrides; a guitar keeps every step\n";
}
} // namespace

int main(int argc,char** argv) {
    try {
        if(argc>1 && std::string(argv[1])=="--controller"){juce::ScopedJuceInitialiser_GUI init;controllerSources();return 0;}
        if(argc>2 && std::string(argv[1])=="--classify"){for(int i=2;i<argc;++i)classifyFile(juce::File::getCurrentWorkingDirectory().getChildFile(argv[i]));return 0;}
        std::vector<Case> corpus;
        for(unsigned seed=1;seed<=4;++seed) {
            for(int v=0;v<3;++v)corpus.push_back({"guitar"+std::string(v==0?"-lines":v==1?"-chords":"-lowriff")+std::to_string(seed),Source::guitar,{Source::guitar},guitar(v,seed*11+unsigned(v))});
            corpus.push_back({"guitar-driven"+std::to_string(seed),Source::guitar,{Source::guitar,Source::keys},drivenGuitar(seed*31)});
            // Bass lines between 40 and 220 Hz are also a down-tuned guitar's range: the
            // low-energy cue decides and may say guitar (it keeps every step either way).
            for(int v=0;v<3;++v)corpus.push_back({"bass"+std::string(v==0?"-4string":v==1?"-5string":"-slap")+std::to_string(seed),Source::bass,
                                                  {Source::bass,Source::guitar},bass(v,seed*13+unsigned(v))});
            corpus.push_back({"guitar-dropA"+std::to_string(seed),Source::guitar,{Source::guitar},dropAGuitar(seed*37)});
            corpus.push_back({"bass-dropA"+std::to_string(seed),Source::bass,{Source::bass},dropABass(seed*41)});
            for(int v=0;v<2;++v)corpus.push_back({"vocal"+std::string(v?"-female":"-male")+std::to_string(seed),Source::vocal,{Source::vocal},vocal(v==1,seed*17+unsigned(v))});
            for(int v=0;v<3;++v)corpus.push_back({"keys"+std::string(v==0?"-organ":v==1?"-pad":"-lead")+std::to_string(seed),Source::keys,{Source::keys},keys(v,seed*19+unsigned(v))});
            corpus.push_back({"piano"+std::to_string(seed),Source::keys,{Source::keys,Source::guitar},keys(3,seed*23)});
            for(int v=0;v<2;++v)corpus.push_back({"drums"+std::string(v?"-fast":"-slow")+std::to_string(seed),Source::percussion,{Source::percussion},drums(v,seed*29+unsigned(v))});
        }
        corpus.push_back({"autoset-test-DI",Source::guitar,{Source::guitar},autoSetDI()});
        // Validation console: a swelled guitar with wide vibrato may read as a
        // voice (Auto Set keeps every step unless skipping is turned on); short
        // Drop A mutes must not read as drums; fifths must not be bass by pitch.
        for(bool drive:{false,true}) {
            const std::string tag=drive?"-drive":"-clean";
            corpus.push_back({"val-det01"+tag,Source::guitar,{Source::guitar,Source::vocal},validationInput(1,110,drive)});
            corpus.push_back({"val-det02-A1"+tag,Source::guitar,{Source::guitar,Source::bass},validationInput(2,55,drive)});
            corpus.push_back({"val-det02-A0"+tag,Source::bass,{Source::bass,Source::guitar},validationInput(2,27.5,drive)});
            corpus.push_back({"val-det03"+tag,Source::guitar,{Source::guitar,Source::bass},validationInput(3,55,drive)});
        }
        std::map<Source,std::map<Source,int>> confusion;int wrong=0;float worstStringRisk=0;
        for(const auto& c:corpus) {
            const auto r=classifyInstrument(c.audio,rate);print(c.name,r);
            ++confusion[c.expected][r.source];
            const bool accepted=std::find(c.accepted.begin(),c.accepted.end(),r.source)!=c.accepted.end();
            if(!accepted){++wrong;std::cout<<"  MISS "<<c.name<<" expected "<<sourceName(c.expected)<<'\n';}
            // Safety: a guitar or bass must never lose its amp steps.
            const bool vocalAccepted=std::find(c.accepted.begin(),c.accepted.end(),Source::vocal)!=c.accepted.end();
            if(stringInstrument(c.expected) && !vocalAccepted)check(keepsAmpSteps(r.source),"SAFETY: "+c.name+" classified as "+sourceName(r.source));
            if(stringInstrument(c.expected))check(r.source!=Source::percussion,"SAFETY: "+c.name+" classified as percussion");
            if(c.expected==Source::guitar)check(!(r.bass && r.bassDecisive),"A guitar was called bass by its pitch range: "+c.name);
            if(stringInstrument(c.expected) && !vocalAccepted)worstStringRisk=std::max({worstStringRisk,r.scores[size_t(Source::vocal)],r.scores[size_t(Source::percussion)]});
        }
        // Auto Set hears 6 s: the first 6 s of every case must hold the same rules.
        int wrongShort=0;
        for(const auto& c:corpus) {
            const auto r=classifyInstrument({c.audio.begin(),c.audio.begin()+std::ptrdiff_t(6*rate)},rate);
            if(std::find(c.accepted.begin(),c.accepted.end(),r.source)==c.accepted.end()){++wrongShort;print("  6s MISS "+c.name,r);}
            if(stringInstrument(c.expected))check(r.source!=Source::percussion && (r.source!=Source::vocal || std::find(c.accepted.begin(),c.accepted.end(),Source::vocal)!=c.accepted.end()),"SAFETY (6 s): "+c.name+" classified as "+sourceName(r.source));
        }
        std::cout<<"MEASURE misses on the first 6 s of each case: "<<wrongShort<<" of "<<corpus.size()<<'\n';
        check(wrongShort<=3,"Too many misses on 6 s windows: "+std::to_string(wrongShort));
        // Silence and a noise floor are not playing.
        {auto quiet=blank();std::mt19937 rng(3);addNoise(quiet,.00005f,rng);const auto r=classifyInstrument(quiet,rate);
         check(!r.features.playing && r.source==Source::unknown,"Noise floor was heard as playing");}
        check(classifyInstrument(std::vector<float>(1000,0.f),rate).source==Source::unknown,"Too-short audio was classified");
        // Resampled rates give the same verdicts.
        for(double other:{44100.,96000.}) {
            const auto& c=corpus[0];std::vector<float> resampled;const double step=rate/other;
            for(double p=0;p+1<double(c.audio.size());p+=step){const auto i=size_t(p);const double f=p-double(i);resampled.push_back(float(c.audio[i]*(1-f)+c.audio[i+1]*f));}
            check(classifyInstrument(resampled,other).source==Source::guitar,"Guitar not recognised at "+std::to_string(int(other))+" Hz");
        }
        std::cout<<"CONFUSION (rows expected, columns detected: unknown guitar bass vocal keys percussion)\n";
        for(auto e:{Source::guitar,Source::bass,Source::vocal,Source::keys,Source::percussion}) {
            std::cout<<std::left<<std::setw(14)<<sourceName(e)<<std::right;
            for(auto d:{Source::unknown,Source::guitar,Source::bass,Source::vocal,Source::keys,Source::percussion})std::cout<<std::setw(5)<<confusion[e][d];
            std::cout<<'\n';
        }
        // Targets on this synthetic corpus: every string case safe (checked
        // above), and at most two misses in total.
        check(wrong<=2,"Too many misclassified corpus cases: "+std::to_string(wrong));
        // Margin: no guitar or bass case comes near a vocal or drum verdict (which needs 0.6).
        std::cout<<"MEASURE highest vocal/percussion score on guitar and bass cases: "<<worstStringRisk<<'\n';
        check(worstStringRisk<.3f,"A guitar or bass case came close to losing its amp steps");
        std::cout<<"PASS instrument detection: "<<corpus.size()<<" synthetic cases, "<<wrong<<" misses, no guitar/bass lost its amp steps\n";
        return 0;
    } catch(const std::exception& e){std::cout<<"FAIL "<<e.what()<<'\n';return 1;}
}

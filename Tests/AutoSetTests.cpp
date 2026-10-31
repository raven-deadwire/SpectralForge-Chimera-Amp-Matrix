#include "PluginProcessor.h"
#include "AutoSetController.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

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
std::vector<float> targetNoise(bool bass,double rate,double seconds,std::mt19937& random,Style style=Style::rock) {
    constexpr int order=15,size=1<<order;
    juce::dsp::FFT fft(order);std::vector<float> spectrum(size_t(2*size));
    std::uniform_real_distribution<double> phase(0,2*juce::MathConstants<double>::pi);
    for(int k=1;k<size/2;++k) {
        const double f=k*rate/size;
        // Band power is the sum of bins; third-octave width grows with f.
        const double magnitude=std::sqrt(std::pow(10.,toneTargetDb(bass,float(std::clamp(f,20.,20000.)),style)/10.)/f)*(f<35?0:1);
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
        check(forced.bass && near(forced.targetDb[1]-forced.targetDb[0],toneTargetDb(true,toneBandCentres[1])-toneTargetDb(true,toneBandCentres[0]),1e-4f),"A chosen instrument was overridden by detection");
    }
    std::cout<<"PASS auto set tone rules: silence, guitar/bass detection, on-target input left alone, dark/bright tilt corrected within +/-6 dB in 0.5 dB steps at 44.1/48/96 kHz\n";
}

void finishRules() {
    std::mt19937 random{23};
    check(toneTargetDb(false,50,Style::metal)==toneTargetDb(false,50)-3.f && toneTargetDb(false,8000,Style::clean)==toneTargetDb(false,8000)+2.f,"Style offsets not applied to the target");
    for(double rate:{44100.,48000.,96000.}) {
        const std::vector<Window> silent(600,Window{-100,-103});
        check(!analyseFinish(std::vector<float>(size_t(rate*6),0.f),silent,rate).playingDetected && !analyseFinish({},silent,rate).limiterPlaying,"Silence reported playing");
        // On-target, steady: leave the de-esser off.
        const auto flat=analyseFinish(targetNoise(false,rate,6,random),{},rate,false);
        check(flat.playingDetected && !flat.deEsser && std::abs(flat.spectralExcessDb)<1.5f && flat.burstExcessDb<2.f,"A steady on-target signal was de-essed");
        // 8 dB too bright above 4 kHz: de-ess the top, within limits.
        auto bright=targetNoise(false,rate,6,random);shelve(bright,4000,8,rate);
        const auto b=analyseFinish(bright,{},rate,false);
        check(b.deEsser && b.frequency>=2500 && b.frequency<=9000 && b.spectralExcessDb>=4.f,"Bright top end was not de-essed");
        check(b.rangeDb>=3 && b.rangeDb<=9 && b.thresholdDb>=-60 && b.thresholdDb<=-6 && b.ratio==4 && b.releaseMs==80,"De-esser settings outside their limits");
        // Pick clicks: short high-frequency bursts on a steady, on-target signal.
        auto clicks=targetNoise(false,rate,6,random);std::vector<float> burst(clicks.size());
        std::uniform_real_distribution<float> noise(-1,1);
        for(size_t n=0;n<burst.size();++n)burst[n]=std::fmod(double(n)/rate,.125)<.004?.6f*noise(random):0.f;
        const auto hp=spectralforge::eqCoefficients(3,5000,0,.707,rate); // clicks live above 5 kHz
        for(int k=0;k<4;++k){double z1=0,z2=0;for(auto& v:burst){const double y=hp.b0*v+z1;z1=hp.b1*v-hp.a1*y+z2;z2=hp.b2*v-hp.a2*y;v=float(y);}}
        for(size_t n=0;n<clicks.size();++n)clicks[n]+=burst[n];
        const auto c=analyseFinish(clicks,{},rate,false,Style::metal);
        check(c.deEsser && c.burstExcessDb>=4.f && c.releaseMs==50,"Pick-click bursts were not de-essed");
    }
    // Limiter: peaks above -1 dBFS are counted; release follows the style.
    std::vector<Window> hot;for(int i=0;i<600;++i)hot.push_back({i%10==0?-.5f:-6.f,-12.f});
    const auto l=analyseFinish({},hot,48000,false,Style::clean);
    check(l.limiterPlaying && near(l.outputPeakDb,-.5f,.01f) && near(l.overCeilingPercent,10.f,.01f) && l.limiterReleaseMs==150 && l.ceilingDb==limiterCeilingDb,"Limiter analysis wrong");
    std::cout<<"PASS auto set finish rules: style targets, silence, steady on-target signal left alone, bright top and pick clicks de-essed within limits at 44.1/48/96 kHz, limiter peaks and style release\n";
}

float maximumReduction(const std::vector<float>& samples,double rate,const FinishResult& result) {
    spectralforge::DeEsserCore core;core.prepare(rate);
    core.setSettings({result.deEsser,result.frequency,result.thresholdDb,result.rangeDb,result.attackMs,result.releaseMs,result.ratio,result.kneeDb,false});
    core.reset();float maximum=0;
    for(float x:samples){float* channels[]{&x};core.process(channels,1,1);maximum=std::max(maximum,core.reduction());}
    return maximum;
}
void riskProbe() {
    for(double rate:{44100.,48000.,96000.}) {
        for(const auto& f:std::vector<std::pair<const char*,std::array<double,3>>>{{"dropA_guitar",{55,65.406,82.407}},{"bass5_lowB",{30.868,41.203,55}},{"bass6_highC",{130.813,164.814,196}}}) {
            const auto x=notes(rate,6,f.second,40);
            const auto t=analyseTone(x,rate);
            std::cout<<"CLASS "<<rate<<' '<<f.first<<" bass="<<t.bass<<"\n";
        }
        for(const bool muted:{false,true})for(const bool clipped:{false,true}) {
            std::mt19937 random{417};auto x=targetNoise(false,rate,6,random);
            for(size_t n=0;n<x.size();++n) {
                const double t=double(n)/rate,cycle=std::fmod(t,.25);
                const double env=std::exp(-cycle*(muted?45.:12.));
                const double attack=cycle<.008?.16*std::sin(2*juce::MathConstants<double>::pi*6000*t)*std::exp(-cycle*500):0;
                double y=x[n]*env+attack;
                x[n]=float(clipped?.25*std::tanh(y*12):y);
            }
            for(auto style:{Style::rock,Style::clean,Style::metal}){
                const auto f=analyseFinish(x,{},rate,false,style);
                std::cout<<"ATTACK "<<rate<<" mute="<<muted<<" clip="<<clipped<<' '<<styleName(style)<<" on="<<f.deEsser<<" spec="<<f.spectralExcessDb<<" burst="<<f.burstExcessDb<<" split="<<f.frequency<<" range="<<f.rangeDb<<" overall="<<f.overallSpectralExcessDb<<" sustained="<<f.sustainedEvidence<<" protected="<<f.protectedAttackWindows<<" maxGR="<<maximumReduction(x,rate,f)<<"\n";
            }
        }
    }
}

void attackSafetyRules() {
    int protectedCases=0,positiveCases=0,overrideCases=0;
    int ambiguityCases=0;
    for(double rate:{44100.,48000.,96000.}) {
        for(const auto fundamentals:std::vector<std::array<double,3>>{{55,65.406,82.407},{30.868,41.203,55},{130.813,164.814,196}}) {
            for(bool clipped:{false,true}) {
                auto x=notes(rate,6,fundamentals,40);
                if(clipped)for(auto& v:x)v=.25f*std::tanh(v*12.f);
                for(bool bass:{false,true}) {
                    check(analyseTone(x,rate,bass).bass==bass && analyseFinish(x,{},rate,bass).bass==bass,"Extended-range/distorted playing overrode the explicit instrument");
                    ++overrideCases;
                }
            }
        }
        for(unsigned seed:{417u,991u})for(bool muted:{false,true})for(bool clipped:{false,true})for(auto style:{Style::rock,Style::clean,Style::metal}) {
            std::mt19937 random{seed};auto x=targetNoise(false,rate,6,random,style);
            for(size_t n=0;n<x.size();++n) {
                const double t=double(n)/rate,cycle=std::fmod(t+(seed==991u?.0037:0.),.25);
                const double env=std::exp(-cycle*(muted?45.:12.));
                const double attack=cycle<.008?.16*std::sin(2*juce::MathConstants<double>::pi*6000*t)*std::exp(-cycle*500):0;
                const double y=x[n]*env+attack;x[n]=float(clipped?.25*std::tanh(y*12):y);
            }
            const auto f=analyseFinish(x,{},rate,false,style);
            check(f.playingDetected && f.protectedAttackWindows>0 && !f.deEsser,"A body-coincident pick/palm-mute attack was automatically de-essed");
            check(maximumReduction(x,rate,f)==0.f,"Protected attack received high-band gain reduction");++protectedCases;
        }
        // A low fundamental changes the heuristic label, not the on-target
        // guitar high end. Auto must not inherit an unconfirmed bass cut.
        for(auto style:{Style::rock,Style::clean,Style::metal}) {
            std::mt19937 random{1229};auto x=targetNoise(false,rate,6,random,style);
            for(size_t n=0;n<x.size();++n)x[n]+=float(.2*std::sin(2*juce::MathConstants<double>::pi*55*double(n)/rate));
            const auto automatic=analyseFinish(x,{},rate,{},style);
            check(automatic.bass && !automatic.deEsser && !analyseFinish(x,{},rate,false,style).deEsser,
                  "Ambiguous low register imposed the darker bass de-esser target");
            check(analyseFinish(x,{},rate,true,style).deEsser,"Explicit bass no longer uses its own style curve");++ambiguityCases;
        }
        // Positive controls for every style: steady excess must still be treated.
        for(bool bass:{false,true})for(auto style:{Style::rock,Style::clean,Style::metal}) {
            std::mt19937 random{1103};auto x=targetNoise(bass,rate,6,random,style);shelve(x,4000,8,rate);
            const auto f=analyseFinish(x,{},rate,bass,style);
            check(f.sustainedEvidence && f.deEsser && f.rangeDb>=3 && f.rangeDb<=9,"Sustained excessive brightness was lost by attack protection");
            check(f.releaseMs==(style==Style::clean?120.f:style==Style::metal?50.f:80.f),"Style de-esser release changed");++positiveCases;
        }
    }
    std::cout<<"PASS attack safety: "<<protectedCases<<" protected synthetic attack cases, "<<positiveCases<<" sustained-brightness controls, "<<overrideCases<<" extended-range/distorted explicit-instrument cases, "<<ambiguityCases<<" auto-target ambiguity cases\n";
}

void lowpass(std::vector<float>& x,double frequency,double rate) {
    for(int k=0;k<2;++k){const auto c=spectralforge::eqCoefficients(4,frequency,0,.707,rate);double z1=0,z2=0;for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}}
}
void multibandRules() {
    std::mt19937 random{31};
    check(multibandCrossovers(true,Style::rock)==std::array<float,3>{80,300,2000} && multibandCrossovers(false,Style::metal)==std::array<float,3>{120,800,4500},"Crossovers do not follow instrument and style");
    for(double rate:{44100.,48000.,96000.}) {
        check(!analyseMultiband(std::vector<float>(size_t(rate*6),0.f),rate).playingDetected,"Silence reported playing");
        // Steady and on target: every band stays bypassed.
        const auto flat=analyseMultiband(targetNoise(false,rate,6,random),rate,false);
        check(flat.playingDetected && flat.activeBands()==0,"A balanced steady signal was multiband-compressed");
        // Booming low notes: the band below 150 Hz jumps 12 dB every other quarter second.
        auto boom=targetNoise(false,rate,6,random);std::vector<float> low(boom.size());
        std::normal_distribution<float> noise(0,.3f);for(auto& v:low)v=noise(random);
        lowpass(low,100,rate);
        for(size_t n=0;n<boom.size();++n)boom[n]+=low[n]*(std::fmod(double(n)/rate,.5)<.25?4.f:.25f);
        const auto b=analyseMultiband(boom,rate,false,Style::metal);
        check(b.bands[0].active && !b.bands[2].active && !b.bands[3].active,"Booming lows did not engage only the low band");
        check(b.bands[0].attackMs==30 && b.bands[0].releaseMs==140 && b.bands[0].ratio>=3 && b.bands[0].rangeDb>=3 && b.bands[0].rangeDb<=9,"Low band settings outside their rules");
    }
    std::cout<<"PASS auto set multiband rules: crossovers per instrument/style, silence, balanced signal left alone, booming lows compress only the low band within limits at 44.1/48/96 kHz\n";
}

// Phase 6: every note of a two-octave pentatonic scale from lowestHz in turn,
// harmonics falling as 1/h; resonance adds a narrow +gainDb peak at
// resonanceHz that only some notes excite (from E2, the 3rd harmonic of E2,
// the 2nd of B2 and the fundamental of B3 fall on 247 Hz).
std::vector<float> playedNotes(double rate,double seconds,std::mt19937& random,float resonanceHz=0,float gainDb=0,float lowestHz=82.41f) {
    constexpr std::array<int,11> scale{0,3,5,7,10,12,15,17,19,22,24};
    std::vector<float> x(size_t(rate*seconds));std::normal_distribution<float> hiss(0,.0005f);
    const size_t noteLength=size_t(rate*.25);double f0=lowestHz;
    for(size_t n=0;n<x.size();++n) {
        if(n%noteLength==0)f0=lowestHz*std::pow(2.,scale[(n/noteLength*4)%scale.size()]/12.);
        const double t=double(n%noteLength)/rate,env=std::exp(-t*6)*std::min(1.,t/.003);double v=0;
        for(int h=1;h<=8;++h)if(f0*h<rate*.45)v+=std::sin(2*juce::MathConstants<double>::pi*f0*h*t)/h;
        x[n]=float(.12*env*v)+hiss(random);
    }
    if(gainDb>0) {
        auto c=spectralforge::eqCoefficients(0,resonanceHz,gainDb,8,rate);double z1=0,z2=0;
        for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
    }
    return x;
}
void dynamicEQRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 random{41};
        check(!analyseDynamicEQ(std::vector<float>(size_t(rate*6),0.f),rate).playingDetected,"Silence reported playing");
        const auto even=analyseDynamicEQ(playedNotes(rate,6,random),rate,false);
        check(even.playingDetected && even.nodes.empty(),"Evenly voiced notes got a Dynamic EQ node");
        // A cabinet-like resonance at 250 Hz that some notes ring through.
        const auto ringing=analyseDynamicEQ(playedNotes(rate,6,random,250,12),rate,false,Style::rock);
        check(ringing.nodes.size()>=1 && near(ringing.nodes[0].frequency,250,1),"A 250 Hz resonance did not get a node there");
        const auto& node=ringing.nodes[0];
        check(node.thresholdDb<0 && node.ratio>=2 && node.ratio<=4 && node.rangeDb>=3 && node.rangeDb<=9 && near(node.attackMs,10,.01f) && near(node.releaseMs,150,.01f)
              && near(node.q,dynamicEQQ,.001f),"Dynamic EQ node outside its rules");
        // Style scales the release; an existing node nearby and no free slot both stop it.
        const auto metal=analyseDynamicEQ(playedNotes(rate,6,random,250,12),rate,false,Style::metal);
        check(!metal.nodes.empty() && near(metal.nodes[0].frequency,250,1) && near(metal.nodes[0].releaseMs,105,.01f),"Metal did not shorten the Dynamic EQ release");
        const auto avoided=analyseDynamicEQ(playedNotes(rate,6,random,250,12),rate,false,Style::rock,{240.f});
        check(std::none_of(avoided.nodes.begin(),avoided.nodes.end(),[](const auto& n){return std::abs(std::log2(n.frequency/240.f))<2.f/3.f;}),"A node was placed next to an existing one");
        check(analyseDynamicEQ(playedNotes(rate,6,random,250,12),rate,false,Style::rock,{},0).nodes.empty(),"A node was placed without a free slot");
        if(rate==48000.){
            std::cout<<"MEASURE dynamic EQ excess (even):";for(int b=0;b<toneBandCount;++b)if(even.excessDb[size_t(b)]!=0)std::cout<<' '<<toneBandCentres[size_t(b)]<<':'<<even.excessDb[size_t(b)];
            std::cout<<"\nMEASURE dynamic EQ excess (250 Hz ring):";for(int b=0;b<toneBandCount;++b)if(ringing.excessDb[size_t(b)]!=0)std::cout<<' '<<toneBandCentres[size_t(b)]<<':'<<ringing.excessDb[size_t(b)];
            std::cout<<"\nMEASURE dynamic EQ node: "<<node.frequency<<" Hz, threshold "<<node.thresholdDb<<" dB, "<<node.ratio<<":1, up to "<<node.rangeDb<<" dB\n";
        }
    }
    std::cout<<"PASS auto set dynamic EQ rules: silence, evenly voiced notes left alone, a 250 Hz resonance gets a node there within limits, metal release, existing nodes and slot count respected at 44.1/48/96 kHz\n";
}

// Phase 5: Gate 2 / Expander from the rig noise in the pause.
void gateStageRules() {
    // 6 s: playing at -18 dBFS input with a 1.5 s pause at -72 dBFS; the rig
    // makes playing louder and the pause carries its own hiss.
    const auto fixture=[](float rigPlayingDb,float rigNoiseDb,float pauseSeconds) {
        std::vector<Window> input,rig;
        for(int i=0;i<600;++i) {
            const bool pause=i>=300 && i<300+int(pauseSeconds*100);
            input.push_back(pause?Window{-70.f,-72.f}:Window{-18.f,-24.f});
            rig.push_back(pause?Window{rigNoiseDb,rigNoiseDb-3.f}:Window{rigPlayingDb,rigPlayingDb-6.f});
        }
        return std::pair{input,rig};
    };
    using Plan=GateStagesResult::Plan;
    {const auto [in,rig]=fixture(-12,-64,1.5f);const auto r=analyseGateStages(in,rig,0,-60,false,Style::clean);
     check(r.plan==Plan::soft && near(r.expanderRangeDb,14,.01f),"Faint hiss should get a shallower Expander range");}
    {const auto [in,rig]=fixture(-12,-80,1.5f);const auto r=analyseGateStages(in,rig,0,-60,false,Style::rock);
     check(r.measured && r.plan==Plan::none && near(r.rigNoiseDb,-80,.01f),"A quiet amp still added Gate 2 or the Expander");}
    {const auto [in,rig]=fixture(-12,-50,1.5f);const auto r=analyseGateStages(in,rig,2,-60,false,Style::rock);
     check(r.plan==Plan::soft && r.expanderAfterRig && near(r.expanderThresholdDb,-60,.01f) && near(r.expanderRatio,2,.01f)
           && near(r.expanderRangeDb,15,.01f) && near(r.mainThresholdDb,-78,.01f) && near(r.expanderReleaseMs,120,.01f),
           "Rock with amp hiss did not get an after-rig Expander at the gate threshold and a deeper main gate");}
    {const auto [in,rig]=fixture(-12,-50,1.5f);const auto r=analyseGateStages(in,rig,0,-60,false,Style::clean);
     check(r.plan==Plan::soft && near(r.expanderRatio,1.5f,.01f) && near(r.mainThresholdDb,-84,.01f) && near(r.expanderReleaseMs,200,.01f),"Clean style did not get the gentler Expander");}
    {const auto [in,rig]=fixture(-12,-50,1.5f);const auto r=analyseGateStages(in,rig,0,-60,false,Style::metal);
     check(r.plan==Plan::hard && r.mainAfterRig && near(r.gate2ThresholdDb,-66,.01f) && near(r.gate2RangeDb,18,.01f),"Metal with a wide gap did not get a post-rig gate and a PRE Gate 2");}
    {const auto [in,rig]=fixture(-40,-50,1.5f);const auto r=analyseGateStages(in,rig,0,-60,false,Style::metal);
     check(r.plan==Plan::soft && near(r.expanderRatio,2.5f,.01f) && near(r.expanderRangeDb,15,.01f),"Metal with a narrow gap should fade tails with the Expander, not chop them");}
    {const auto [in,rig]=fixture(-12,-50,0.f);const auto r=analyseGateStages(in,rig,0,-60,false,Style::rock);
     check(!r.measured && r.plan==Plan::none,"Gate stages were set without a pause");}
    std::cout<<"PASS auto set gate stage rules: quiet amp leaves them off, amp hiss adds an Expander (clean/rock/metal ratios, deeper main gate), metal with headroom gets a post-rig gate and PRE Gate 2, no pause changes nothing\n";
}
void captureWindows() {
    for(double rate:{44100.,48000.,96000.}) {
        // Two 512 KB sample rings: on the heap, as in the processor (a 1 MB stack overflows).
        auto owned=std::make_unique<Capture>();auto& capture=*owned;capture.prepare(rate);
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
        std::vector<float> tone;capture.drainSamples(tone);check(tone.empty(),"Inactive tone tap recorded samples");
        capture.listen({Tap::input,Tap::rig});
        capture.push(Tap::rig,channels,2,int(left.size()));capture.push(Tap::input,channels,2,int(left.size()));
        std::vector<Window> rigOut,inOut;capture.drain(Tap::rig,rigOut);capture.drain(Tap::input,inOut);
        check(rigOut.size()==inOut.size() && rigOut.size()>=99 && near(rigOut[10].peakDb,-6.02f,.3f),"Rig tap does not record windows alongside the input");
        capture.listen(Tap::tone);counting=true;allocations=0;
        capture.push(Tap::tone,channels,2,4096);capture.push(Tap::input,channels,2,4096);capture.push(Tap::deesser,channels,2,4096);
        counting=false;check(allocations==0,"Audio-thread tone capture allocated");
        out.clear();capture.drainSamples(tone);capture.drain(Tap::input,out);
        check(tone.size()==4096 && out.empty(),"Tone tap did not record exactly its own samples");
        for(size_t n=0;n<tone.size();++n)check(near(tone[n],.625f*left[n],1e-6f),"Tone tap is not the channel average");
        // Finish pass: de-esser samples and output windows together.
        capture.listen({Tap::deesser,Tap::output});tone.clear();out.clear();
        for(int offset=0;offset<int(left.size());offset+=256){const float* block[2]{left.data()+offset,right.data()+offset};const int n=std::min(256,int(left.size())-offset);
            capture.push(Tap::tone,block,2,n);capture.push(Tap::deesser,block,2,std::min(n,int(Capture::sampleCapacity)));capture.push(Tap::output,block,2,n);}
        capture.drainSamples(tone);capture.drain(Tap::output,out);
        check(tone.size()==left.size() && out.size()>=99 && out.size()<=100,"Simultaneous de-esser and output taps did not both record");
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

// Values read back from the processor pass through normalised ranges, which
// need not reproduce a limit bit-exactly on every platform: bounds allow 0.01.
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

// Feed the production Capture/Controller seam to isolate parameter ownership
// from amp voicing. The separate integration run below uses processBlock.
void feedCapture(ChimeraProcessor& p,Controller& c,const std::vector<float>& x,double& now) {
    for(size_t offset=0;offset<x.size();offset+=256) {
        const int count=int(std::min(size_t(256),x.size()-offset));
        const float* channels[]{x.data()+offset};
        for(auto tap:{Tap::input,Tap::tone,Tap::compressor,Tap::deesser,Tap::output})
            p.autoSetCapture().push(tap,channels,1,count);
        now+=1000.*count/p.getSampleRate();c.tick(now);
    }
}
void controllerFinishPreservation() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    const auto deesserValues=[&]{std::vector<float> out;for(auto* id:{"deEsserOn","deEsserFrequency","deEsserThreshold","deEsserRange","deEsserAttack","deEsserRelease","deEsserRatio","deEsserKnee","deEsserListen"})out.push_back(get(*p,id));return out;};
    for(bool bass:{false,true})for(auto style:{Style::rock,Style::clean,Style::metal}) {
        setRaw("deEsserOn",1);setRaw("deEsserFrequency",4700);setRaw("deEsserThreshold",-31);setRaw("deEsserRange",7);setRaw("deEsserListen",1);
        const auto manual=deesserValues(),before=values(*p);std::mt19937 random{802};
        const auto signal=targetNoise(bass,48000,6.5,random,style);
        Controller c(*p);c.setOptions({bass?2:1,false,style,true,false,true,1.f,0u,false,false,false,false,false,false});Gestures g;p->addListener(&g);
        double now=juce::Time::getMillisecondCounterHiRes();c.start();
        for(int pass=0;pass<3;++pass)feedCapture(*p,c,signal,now);
        check(c.current().phase==Controller::Phase::done && c.lastFinishAnalysis().bass==bass,"Explicit instrument or three-pass controller failed");
        check(!c.lastFinishAnalysis().deEsser && deesserValues()==manual && c.current().text.contains("De-esser unchanged"),"No-excess analysis changed a manual de-esser or misreported it as off");
        const float expectedRelease=style==Style::clean?150.f:style==Style::metal?60.f:100.f;
        check(near(get(*p,"liveLimiterRelease"),expectedRelease,.01f),"Style limiter release changed");
        c.undo();const auto restored=values(*p);
        for(size_t i=0;i<before.size();++i)check(near(before[i],restored[i],1e-6f),"Undo failed to restore the explicit style/instrument run");
        check(g.begins==g.ends && g.begins>0,"Apply/undo host gestures unbalanced");p->removeListener(&g);
    }
    p->releaseResources();
    std::cout<<"PASS explicit guitar/bass x three styles: manual de-esser/listen preserved, truthful summary, limiter release, apply/undo host notifications\n";
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
    // The Dynamic EQ node on Tone band 2 is the user's. Compare with the stored
    // values: skewed ranges need not hold the literal set above bit-exactly.
    const auto dynamicNode=[&]{std::vector<float> v;for(const auto* field:{"enabled","frequency","gain","q","type"})v.push_back(get(*p,spectralforge::eqID(0,1,field)));return v;};
    const auto nodeBefore=dynamicNode();
    DI di{48000,.126f,.00025f}; // notes peak ~ -18 dBFS, noise ~ -72 dBFS
    c.start();check(c.busy(),"Auto Set did not start listening");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::boost && c.current().text.contains("2/9"),"First pass did not hand over to the boost pass");
    check(get(*p,"input")>4.f && get(*p,"input")<=12.01f,"Input gain was not raised toward the target");
    // With an Expander (phase 5) it takes the threshold above the noise and the main gate sits below it.
    const float placed=get(*p,"expOn")>.5f?get(*p,"expThreshold"):get(*p,"gatethreshold");
    check(get(*p,"gateon")>.5f && placed>-70.f && placed<-35.f && get(*p,"gatethreshold")<=placed,"Gate threshold was not placed above the noise floor");
    {   // Phase 10 on the real processor: the default board is empty, so a boost may take its last slot.
        run(*p,c,&di,6.6,now);
        check(c.current().phase==Controller::Phase::amp && c.current().text.contains("3/9"),"Boost pass did not hand over to the amp pass");
        const auto& boost=c.lastBoostAnalysis();
        check(boost.playingDetected && !boost.bass,"Boost pass did not hear the guitar DI");
        int placed=0;for(int owner=0;owner<spectralforge::pedalBoardCapacity;++owner)placed+=juce::roundToInt(get(*p,spectralforge::pedalModelID(owner)))==boostPedalModel;
        check(placed==(boost.engaged?1:0),"Applied boost does not match the analysis");
        std::cout<<"MEASURE auto set boost (processBlock): low end "<<boost.lowExcessBeforeDb<<" > "<<boost.lowExcessAfterDb<<" dB over the target, drive "<<boost.compressionBefore<<" > "<<boost.compressionAfter
                 <<", engaged "<<boost.engaged<<'\n';
    }
    {   // Phase 8 on the real processor: the default head, heard through processBlock.
        const int model=p->selectedAmpModel(0),channel=p->selectedAmpChannel(0),route=p->selectedAmpNativeRoute(0);
        const int gainControl=spectralforge::easy::ampControl(model,channel,route,spectralforge::easy::Role::gain);
        const auto gainID=spectralforge::ampNativeControlID(0,model,gainControl,channel);
        const float gainBefore=get(*p,gainID);
        run(*p,c,&di,6.6,now);
        check(c.current().phase==Controller::Phase::cab && c.current().text.contains("4/9"),"Amp pass did not hand over to the cabinet mic pass");
        const auto& amp=c.lastAmpAnalysis();
        check(amp.playingDetected && !amp.bass && amp.before[ampGain].present && near(amp.before[ampGain].value,gainBefore,1e-4f),"Amp pass did not hear the DI or the head's GAIN");
        check(near(get(*p,gainID),amp.proposed[ampGain].value,.011f*(amp.before[ampGain].maximum-amp.before[ampGain].minimum)),"Applied GAIN does not match the amp analysis");
        std::cout<<"MEASURE auto set amp (processBlock): "<<spectralforge::ampInfo(model).name<<" drive "<<amp.compressionBefore<<" > "<<amp.compressionAfter<<" (target "<<amp.compressionTarget
                 <<"), tone error "<<amp.toneErrorBeforeDb<<" > "<<amp.toneErrorAfterDb<<" dB, "<<amp.changedKnobs()<<" knobs\n";
    }
    // Phase 9: the default cabinet plays a captured IR, which the pass keeps with the reason.
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::tone && c.current().text.contains("5/9"),"Cabinet mic pass did not hand over to the Tone EQ pass");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::compressor,"Tone EQ pass did not hand over to the compressor pass");
    const auto& tone=c.lastToneAnalysis();
    check(tone.playingDetected && !tone.bass,"Tone pass did not hear the guitar DI");
    check(dynamicNode()==nodeBefore && near(nodeBefore[1],700.f,.01f) && near(nodeBefore[2],-3.f,.01f),"Auto Set overwrote a Dynamic EQ node");
    int enabled=0;
    for(int b=0;b<spectralforge::graphicalEQBands;++b)if(b!=1 && get(*p,spectralforge::eqID(0,b,"enabled"))>.5f) {
        ++enabled;check(std::abs(get(*p,spectralforge::eqID(0,b,"gain")))<=toneMaxMoveDb+.01f,"Tone EQ move beyond the limit");
    }
    // Phase 6 adds its 0 dB Dynamic EQ nodes on free bands, keeping the user's slot 1.
    const auto& dynamic=c.lastDynamicAnalysis();
    check(dynamic.playingDetected && dynamic.nodes.size()<=2,"Dynamic EQ step did not hear the guitar DI");
    check(enabled==tone.activeMoves()+int(dynamic.nodes.size()) && (enabled==0 || get(*p,spectralforge::eqID(0,-1,"bypass"))<.5f),"Applied Tone EQ does not match the analysis");
    check(near(get(*p,spectralforge::dynamicSlotID(0,"target")),2.f,.01f),"Auto Set reassigned the user's Dynamic EQ slot");
    for(size_t i=0;i<dynamic.nodes.size();++i) {
        const int target=juce::roundToInt(get(*p,spectralforge::dynamicSlotID(int(i)+1,"target")));
        check(target>=1 && target<=12 && target!=2 && near(get(*p,spectralforge::eqID(0,target-1,"frequency")),dynamic.nodes[i].frequency,.5f)
              && near(get(*p,spectralforge::eqID(0,target-1,"gain")),0.f,.01f) && near(get(*p,spectralforge::dynamicSlotID(int(i)+1,"threshold")),dynamic.nodes[i].thresholdDb,.01f),
              "Dynamic EQ node does not match the analysis");
    }
    run(*p,c,&di,7.0,now);
    check(c.current().phase==Controller::Phase::multiband && c.current().text.contains("7/9"),"Compressor pass did not hand over to the multiband pass");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::finish && c.current().text.contains("8/9"),"Multiband pass did not hand over to the de-esser and limiter pass");
    const auto& mb=c.lastMultibandAnalysis();
    check(mb.playingDetected && (get(*p,"mbOn")>.5f)==(mb.activeBands()>0),"Multiband pass did not hear the guitar or did not apply its analysis");
    for(int b=0;b<4 && mb.activeBands()>0;++b)
        check((get(*p,spectralforge::multibandID(b,"bypass"))<.5f)==mb.bands[size_t(b)].active && get(*p,spectralforge::multibandID(b,"solo"))<.5f,"Multiband band state does not match the analysis");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::level && c.current().text.contains("9/9"),"De-esser and limiter pass did not hand over to the Final EQ and output pass");
    const float trimBefore=get(*p,"output");
    run(*p,c,&di,6.6,now);
    p->removeListener(&g);
    check(c.current().phase==Controller::Phase::done && c.current().canUndo,"Auto Set did not finish with undo available");
    {   // Phase 7 applies its analysis: the trim moves by the proposed change, Final EQ moves land on free bands.
        const auto& level=c.lastLevelAnalysis();
        check(level.playingDetected && level.loudnessMeasured && level.loudnessLufs<0,"Final EQ and output pass did not hear the output");
        check(near(get(*p,"output"),trimBefore+level.trimChangeDb,.01f),"Output trim does not match the analysis");
        int finalBands=0;for(int b=0;b<spectralforge::graphicalEQBands;++b)finalBands+=get(*p,spectralforge::eqID(1,b,"enabled"))>.5f;
        check(finalBands==level.activeMoves() && (finalBands==0 || get(*p,spectralforge::eqID(1,-1,"bypass"))<.5f),"Applied Final EQ does not match the analysis");
        std::cout<<"MEASURE auto set level: "<<level.loudnessLufs<<" LUFS, peak "<<level.peakDb<<" dBFS, trim "<<level.trimChangeDb<<" dB, final moves "<<level.activeMoves()
                 <<" (rumble "<<level.rumbleExcessDb<<", mud "<<level.mudExcessDb<<", air "<<level.airExcessDb<<")\n";
    }
    const auto& finish=c.lastFinishAnalysis();
    check(finish.playingDetected && finish.limiterPlaying && !finish.bass,"Finish pass did not hear the de-esser and limiter inputs");
    // The live view says what each pass listens for, what it heard and how long it takes.
    {
        bool sawInput=false,sawPause=false,sawTone=false,monotonic=true;double last=0;
        for(const auto& s:statuses) {
            if(s.pass<0)continue;
            check(s.passes.size()==9 && s.phase==s.passes[size_t(s.pass)] && s.listeningFor.isNotEmpty() && s.heard.isNotEmpty(),"Listening status does not describe its pass");
            check(near(float(s.totalSeconds),9*6.f+8*.3f,.01f) && s.passSeconds<=s.passTotalSeconds+1e-6,"Listening status has the wrong duration");
            monotonic&=s.seconds+1e-6>=last;last=s.seconds;
            sawInput|=s.phase==Controller::Phase::input && s.levelDb>-30.f && s.heard.contains("Playing: heard");
            sawPause|=s.heard.contains("Pause: heard");
            sawTone|=s.phase==Controller::Phase::tone && s.heard.contains("Guitar");
        }
        check(monotonic && sawInput && sawPause,"Listening status did not report the playing level, playing and the pause");
        check(!sawTone,"Tone pass claimed an instrument before its analysis");
        const auto& steps=c.current().steps;
        check(steps.size()==12 && steps[0].phase==Controller::Phase::input && steps[0].result.contains("input") && steps[1].phase==Controller::Phase::gates
              && steps[2].phase==Controller::Phase::boost && steps[2].changed==c.lastBoostAnalysis().engaged
              && steps[3].phase==Controller::Phase::amp && steps[3].changed==(c.lastAmpAnalysis().changedKnobs()>0)
              && steps[4].phase==Controller::Phase::cab && !steps[4].changed && steps[4].result.contains("captured IR")
              && steps[5].phase==Controller::Phase::tone && steps[6].phase==Controller::Phase::dynamicEQ && steps[6].changed==!dynamic.nodes.empty()
              && steps[9].phase==Controller::Phase::finish && steps[10].phase==Controller::Phase::space && steps[11].phase==Controller::Phase::level && steps[7].changed && steps[7].result.contains(":1"),"Results do not list one row per pass with the applied values");
        // Gate 2 / Expander follow their analysis; the gates were held open only while the input pass listened.
        const auto& gates=c.lastGateAnalysis();
        check(gates.measured && (get(*p,"expOn")>.5f)==(gates.plan==GateStagesResult::Plan::soft) && (get(*p,"gate2On")>.5f)==(gates.plan==GateStagesResult::Plan::hard),
              "Applied Gate 2 / Expander do not match the analysis");
        if(gates.plan==GateStagesResult::Plan::soft)check(near(get(*p,"expThreshold"),gates.expanderThresholdDb,.01f) && near(get(*p,"gatethreshold"),gates.mainThresholdDb,.01f) && get(*p,"gateMode")<.5f,"Expander plan not applied");
        bool sawHiss=false;for(const auto& s:statuses)sawHiss|=s.phase==Controller::Phase::input && s.heard.contains("Amp hiss");
        check(sawHiss,"Input pass did not report the amp's hiss");
        std::cout<<"MEASURE auto set gates: rig noise "<<gates.rigNoiseDb<<" dBFS, gap "<<gates.rigGapDb<<" dB, plan "<<int(gates.plan)<<'\n';
        std::cout<<"MEASURE auto set results:";for(const auto& step:steps)std::cout<<" ["<<Controller::phaseName(step.phase)<<": "<<step.result<<"]";std::cout<<'\n';
    }
    check(get(*p,"liveLimiterOn")>.5f && near(get(*p,"liveLimiterCeiling"),limiterCeilingDb,.01f) && near(get(*p,"liveLimiterRelease"),100.f,.01f),"Limiter was not set for the rock style");
    check((get(*p,"deEsserOn")>.5f)==finish.deEsser && (!finish.deEsser || near(get(*p,"deEsserFrequency"),finish.frequency,1.f)),"Applied de-esser does not match the analysis");
    check(get(*p,"cleanCompOn")>.5f && get(*p,"cleanCompDetector")>.5f && near(get(*p,"cleanCompMix"),1.f,.001f),"Clean compressor was not enabled in RMS mode");
    check(get(*p,"cleanCompRatio")>=1.99f && get(*p,"cleanCompRatio")<=4.01f && get(*p,"cleanCompThreshold")<0.f,"Compressor ratio/threshold out of range");
    check(g.begins==g.ends && g.begins>0 && g.changes>=g.begins,"Parameter writes were not balanced host gestures");
    bool progressed=false;for(const auto& s:statuses)progressed|=s.progress>0 && s.progress<1;
    check(progressed && c.current().text.contains("Comp"),"Status did not report progress and a summary");
    std::cout<<"AUTO SET synthetic DI (-18 dBFS notes, -72 dBFS noise): "<<c.current().text<<"\n";
    std::cout<<"AUTO SET multiband:";for(int b=0;b<4;++b)std::cout<<" band"<<b+1<<(mb.bands[size_t(b)].active?" on":" off")<<" swing "<<mb.bands[size_t(b)].swingDb<<" excess "<<mb.bands[size_t(b)].excessDb;
    std::cout<<"\n";
    std::cout<<"AUTO SET finish: high-band excess "<<finish.spectralExcessDb<<" dB, burst excess "<<finish.burstExcessDb<<" dB, output peak "<<finish.outputPeakDb<<" dBFS\n";
    std::cout<<"AUTO SET tone moves:";for(const auto& m:tone.moves)std::cout<<' '<<m.frequency<<"Hz "<<m.gainDb<<"dB";
    std::cout<<" | fit error "<<tone.errorBeforeDb<<" -> "<<tone.errorAfterDb<<" dB\n";
    const auto applied=values(*p);
    int changed=0;for(size_t i=0;i<before.size();++i)if(applied[i]!=before[i])++changed;
    check(changed>=3 && changed<=35+5*spectralforge::graphicalEQBands+10*spectralforge::multibandBandCount+6*4,"Auto Set touched parameters outside its scope");
    c.undo();
    // Undo restores normalised values, which round-trip within float precision.
    const auto restored=values(*p);bool undone=restored.size()==before.size();
    for(size_t i=0;undone && i<before.size();++i)undone=near(restored[i],before[i],1e-6f);
    check(undone && !c.current().canUndo,"Undo did not restore every parameter");
    c.setOptions({0,false,Style::rock,false,false,true,1.f,0u,false,false,false,false,false,false});c.start();
    check(c.current().phase==Controller::Phase::input && c.current().text.contains("1/2"),"Excluding the optional steps did not shorten Auto Set to two passes");
    c.cancel();check(values(*p)==restored,"Cancelling a run changed parameters");
    // Edits made while listening and after applying are not owned by Auto Set.
    // Include an untouched optional stage: the old whole-snapshot undo lost it.
    c.setOptions({1,false,Style::rock,false,false,true,1.f,0u,false,false,false,false,false,false});c.start();
    setRaw("input",3.f);setRaw("deEsserRange",11.f);
    const float manualInput=get(*p,"input"),manualRange=get(*p,"deEsserRange");
    run(*p,c,&di,14,now);
    check(c.current().phase==Controller::Phase::done,"Ownership run did not complete");
    check(get(*p,"input")==manualInput,"Auto Set overwrote an input edit made while listening");
    setRaw("cleanCompThreshold",-17.f);
    const float automatedThreshold=get(*p,"cleanCompThreshold");
    p->addListener(&g);c.undo();p->removeListener(&g);
    check(get(*p,"input")==manualInput && get(*p,"deEsserRange")==manualRange && get(*p,"cleanCompThreshold")==automatedThreshold,
          "Undo overwrote a newer manual/host edit or an untouched optional stage");
    check(g.begins==g.ends,"Undo left host gestures unbalanced");
    std::cout<<"PASS auto set controller: silence/no-audio failures, five passes on the real processor, input+gate+Tone EQ+compressor+multiband+de-esser+limiter applied as host gestures, Dynamic EQ node kept, progress/summary, undo, optional passes, undo keeps newer edits\n";
}
}

// Locks and strength. Fixtures feed the capture taps directly, so each run
// analyses identical audio whatever the earlier passes applied: runs compare
// exactly, isolating how results are written from how the amp responds. The
// window taps hear the plucked DI; the multiband tap hears booming lows and the
// de-esser tap a bright top (the multibandRules/finishRules fixtures), so both
// stages engage.
struct LockRun {std::vector<float> values;Controller::Status status;std::vector<float> before;};
LockRun runLocked(const Controller::Options& options,const std::function<void(ChimeraProcessor&,Controller&)>& afterRun={},
                  const std::function<void(ChimeraProcessor&,Controller&)>& beforeRun={},
                  const std::function<void(ChimeraProcessor&,Controller&)>& duringRun={},
                  const std::vector<float>* finishFixture=nullptr) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    Controller c(*p);c.setOptions(options);
    if(beforeRun)beforeRun(*p,c);
    LockRun result;result.before=values(*p);
    DI di{48000,.126f,.00025f};double now=juce::Time::getMillisecondCounterHiRes();
    std::mt19937 random{41};
    auto bright=targetNoise(false,48000,6,random);shelve(bright,4000,8,48000);
    auto boom=targetNoise(false,48000,6,random);
    {std::vector<float> low(boom.size());std::normal_distribution<float> noise(0,.3f);for(auto& v:low)v=noise(random);
     lowpass(low,100,48000);for(size_t n=0;n<boom.size();++n)boom[n]+=low[n]*(std::fmod(double(n)/48000,.5)<.25?4.f:.25f);}
    if(finishFixture)bright=*finishFixture;
    std::vector<float> block(256);c.start();
    for(int k=0;c.busy() && k<int(60*48000/256);++k) {
        if(duringRun)duringRun(*p,c);
        for(auto& x:block)x=di.next();
        const size_t offset=size_t(k)*256%(bright.size()-256);
        const float* channels[]{block.data()};const float* top[]{bright.data()+offset};const float* low[]{boom.data()+offset};
        for(auto tap:{Tap::input,Tap::tone,Tap::compressor,Tap::output,Tap::master})p->autoSetCapture().push(tap,channels,1,256);
        p->autoSetCapture().push(Tap::multiband,low,1,256);p->autoSetCapture().push(Tap::deesser,top,1,256);
        now+=1000.*256/48000;c.tick(now);
    }
    check(!c.busy(),"Locked Auto Set run did not finish");
    result.values=values(*p);result.status=c.current();
    if(afterRun)afterRun(*p,c);
    p->releaseResources();
    return result;
}
float valueOf(const LockRun& run,const juce::String& id) {
    static auto reference=std::make_unique<ChimeraProcessor>();
    auto* parameter=reference->parameters().getParameter(id);check(parameter!=nullptr,"Unknown parameter "+id.toStdString());
    return parameter->convertFrom0to1(run.values[size_t(parameter->getParameterIndex())]);
}
void inputStrengthRounding() {
    // Fixed decimal expectations, independent of the production formula.
    // JUCE's 0.01 dB range may return a few ulps either side of a value
    // after normalisation (including a slightly negative default zero).
    struct Case {float before,proposed,strength,expected;};
    const Case cases[]{
        {0,10.5f,.25f,2.5f},{0,10.5f,.5f,5.5f},{0,10.5f,.75f,8},
        {0,-10.5f,.5f,-5.5f},{1.01f,10.5f,.5f,6},
        {-.01f,10.5f,.5f,5},{.01f,-10.5f,.5f,-5},
        {1.23f,10.5f,.25f,3.5f},{1.23f,10.5f,.75f,8},
        {-24,24,.25f,-12},{-24,24,.5f,0},{-24,24,.75f,12}
    };
    for(const auto& c:cases)for(float error:{-0.000003f,0.f,0.000003f})
        check(scaledInputGainDb(c.before+error,c.proposed,c.strength)==c.expected,
              "Input strength rounding changed at a parameter conversion boundary");
    check(scaledInputGainDb(-.000003f,10.5f,1.f)==10.5f,"Full strength changed the proposal");
    std::cout<<"PASS input strength rounding: decimal ties, negative gain, 0.01 dB settings and conversion error\n";
}
void controllerLocksAndStrength() {
    using P=Controller::Phase;
    const Controller::Options all{1,true,Style::rock,true,true,false,1.f,0u,false,false,false,false,false,false};
    const auto full=runLocked(all);
    check(full.status.phase==P::done && full.status.steps.size()==5,"Reference run did not complete five passes");
    check(valueOf(full,"gateon")>.5f,"Reference run did not place the gate");
    const LockRun defaults{full.before,{},{}};

    // Strength 50%: the same analysis, written at half the amount.
    auto halfOptions=all;halfOptions.strength=.5f;
    const auto half=runLocked(halfOptions);
    check(half.status.phase==P::done && half.status.text.contains("50% strength"),"Half-strength run did not report its strength");
    const float inputBefore=valueOf(defaults,"input");
    const float inputFull=valueOf(full,"input"),inputHalf=valueOf(half,"input");
    check(near(inputHalf,float(std::round((std::round(double(inputBefore)*100)+std::round(double(inputFull)*100))/100)*.5),.01f),"Half strength did not halve the input gain change");
    check(near(valueOf(half,"gatethreshold"),std::round(std::clamp(valueOf(full,"gatethreshold")+inputHalf-inputFull,-90.f,-15.f)),.51f),
          "Gate was not placed for the applied input gain");
    check(near(valueOf(half,"gateRangeDb"),valueOf(full,"gateRangeDb"),.01f),"Strength changed the gate range");
    int toneBands=0;
    for(int b=0;b<spectralforge::graphicalEQBands;++b) {
        const auto id=[&](const char* field){return spectralforge::eqID(0,b,field);};
        check((valueOf(full,id("enabled"))>.5f)==(valueOf(half,id("enabled"))>.5f),"Strength changed which Tone EQ bands are used");
        if(valueOf(full,id("enabled"))<.5f)continue;
        ++toneBands;
        check(near(valueOf(half,id("gain")),.5f*valueOf(full,id("gain")),.02f) && near(valueOf(half,id("frequency")),valueOf(full,id("frequency")),.5f),
              "Half strength did not halve a Tone EQ move");
    }
    check(near(valueOf(full,"cleanCompMix"),1.f,.001f) && near(valueOf(half,"cleanCompMix"),.5f,.01f)
          && near(valueOf(half,"cleanCompRatio"),valueOf(full,"cleanCompRatio"),.01f),"Half strength did not mix the compressor in parallel");
    // The fixture engages both stages, so their strength checks are not vacuous.
    check(valueOf(full,"mbOn")>.5f && valueOf(full,"deEsserOn")>.5f,"Reference run did not engage the multiband and de-esser");
    check(near(valueOf(half,"mbMix"),.5f,.01f),"Half strength did not mix the multiband in parallel");
    check(near(valueOf(half,"deEsserRange"),.5f*valueOf(full,"deEsserRange"),.05f),"Half strength did not halve the de-esser range");
    check(near(valueOf(half,"liveLimiterCeiling"),valueOf(full,"liveLimiterCeiling"),.01f),"Strength changed the output limiter");

    // Locks: locked settings keep their values, their passes are skipped and
    // reported in signal order, the unlocked rest matches the reference.
    auto lockOptions=all;lockOptions.locks=Controller::lockInput|Controller::lockTone|Controller::lockCompressor|Controller::lockLimiter;
    const auto locked=runLocked(lockOptions,[](ChimeraProcessor& p,Controller& c){
        // A later edit to a locked setting survives UNDO AUTO.
        auto* input=p.parameters().getParameter("input");input->setValueNotifyingHost(input->convertTo0to1(5.5f));
        c.undo();check(near(get(p,"input"),5.5f,.01f),"Undo overwrote an edit to a locked setting");
        auto* gate=p.parameters().getParameter("gateon");
        check(gate->getValue()==gate->getDefaultValue(),"Undo did not restore the unlocked gate");
    });
    check(locked.status.phase==P::done,"Locked run did not complete");
    // The gate lock also keeps the Gate 2 / Expander result step of the input pass.
    {
        auto gateOptions=all;gateOptions.gates=true;gateOptions.locks=Controller::lockGate;
        const auto gated=runLocked(gateOptions);
        const auto& gateSteps=gated.status.steps;
        check(gated.status.phase==P::done && std::any_of(gateSteps.begin(),gateSteps.end(),[](const auto& s){return s.phase==P::gates && s.result=="kept (locked)" && !s.changed;}),
              "Gate lock did not keep Gate 2 and the Expander");
        for(const auto* id:{"gateon","gatethreshold","gateAfterRig","gateMode","gate2On","gate2Threshold","gate2RangeDb","expOn","expAfterRig","expThreshold","expRatio","expRangeDb"})
            check(near(valueOf(gated,id),valueOf(defaults,id),1e-4f),std::string("Gate lock changed: ")+id);
    }
    check((locked.status.passes==std::vector<P>{P::input,P::multiband,P::finish}),"Locked passes were not skipped");
    const auto& steps=locked.status.steps;
    check(steps.size()==5 && steps[1].phase==P::tone && steps[2].phase==P::compressor && steps[1].result=="kept (locked)" && steps[2].result=="kept (locked)"
          && !steps[1].changed && steps[0].result.contains("input kept (locked)") && steps[4].result.contains("limiter kept (locked)"),
          "Locked passes are not reported in signal order");
    for(const auto* id:{"input","cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompMix","liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"})
        check(near(valueOf(locked,id),valueOf(defaults,id),1e-4f),std::string("A locked setting changed: ")+id);
    for(int b=0;b<spectralforge::graphicalEQBands;++b)for(const auto* field:{"enabled","frequency","gain","q","type"})
        check(near(valueOf(locked,spectralforge::eqID(0,b,field)),valueOf(defaults,spectralforge::eqID(0,b,field)),1e-4f),"A locked Tone EQ band changed");
    check(near(valueOf(locked,"gatethreshold"),std::round(std::clamp(valueOf(full,"gatethreshold")-inputFull,-90.f,-15.f)),.51f),
          "Gate was not placed for the locked input gain");
    check(near(valueOf(locked,"mbOn"),valueOf(full,"mbOn"),1e-4f) && near(valueOf(locked,"deEsserOn"),valueOf(full,"deEsserOn"),1e-4f)
          && near(valueOf(locked,"deEsserRange"),valueOf(full,"deEsserRange"),1e-3f),"Unlocked stages differ from the reference run");

    // Everything locked: nothing to run, nothing written.
    {
        auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);Controller c(*p);
        auto o=all;for(const auto lock:Controller::lockOrder)o.locks|=lock;c.setOptions(o);
        const auto before=values(*p);c.start();
        check(!c.busy() && c.current().phase==P::failed && c.current().text.contains("locked") && values(*p)==before,"An all-locked run did not refuse cleanly");
        o.strength=0;c.setOptions(o);check(near(c.currentOptions().strength,Controller::minimumStrength,1e-6f),"Strength below the minimum was accepted");
        o.strength=std::numeric_limits<float>::quiet_NaN();c.setOptions(o);check(c.currentOptions().strength==1.f,"A non-finite strength was accepted");
        p->releaseResources();
    }
    std::cout<<"PASS auto set locks and strength: "<<toneBands<<" Tone EQ moves halved, parallel comp/multiband mix, gate follows applied input, locked passes skipped and kept by undo\n";
}

// Phase 6 on the controller: the Tone EQ pass hears notes that ring at 250 Hz.
struct DynamicRun {std::vector<float> before,values;Controller::Status status;DynamicEQResult analysis;std::vector<float> undone;};
DynamicRun runDynamic(const Controller::Options& options,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    DynamicRun result;result.before=values(*p);
    DI di{48000,.126f,.00025f};std::mt19937 random{43};const auto ringing=playedNotes(48000,6,random,250,12);
    double now=juce::Time::getMillisecondCounterHiRes();std::vector<float> block(256);c.start();
    for(int k=0;c.busy() && k<int(60*48000/256);++k) {
        for(auto& x:block)x=di.next();
        const float* channels[]{block.data()};const float* tone[]{ringing.data()+size_t(k)*256%(ringing.size()-256)};
        for(auto tap:{Tap::input,Tap::compressor,Tap::output,Tap::multiband,Tap::deesser})p->autoSetCapture().push(tap,channels,1,256);
        p->autoSetCapture().push(Tap::tone,tone,1,256);
        now+=1000.*256/48000;c.tick(now);
    }
    check(!c.busy(),"Dynamic EQ run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastDynamicAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
const Controller::Step* stepOf(const Controller::Status& status,Controller::Phase phase) {
    for(const auto& step:status.steps)if(step.phase==phase)return &step;
    return nullptr;
}
void controllerDynamicEQ() {
    using P=Controller::Phase;
    const auto value=[](const DynamicRun& r,const juce::String& id){return valueOf(LockRun{r.values,{},{}},id);};
    // The Tone EQ and every Dynamic EQ slot, as stored before and after a run.
    const auto eqUnchanged=[](const DynamicRun& r) {
        static auto reference=std::make_unique<ChimeraProcessor>();
        for(auto* q:reference->getParameters())if(auto* named=dynamic_cast<juce::AudioProcessorParameterWithID*>(q))
            if((named->paramID.startsWith("toneEQ_") || named->paramID.startsWith("dynamicEQ")) && r.values[size_t(q->getParameterIndex())]!=r.before[size_t(q->getParameterIndex())])return false;
        return true;
    };
    const auto undoneAll=[](const DynamicRun& r){for(size_t i=0;i<r.before.size();++i)if(!near(r.undone[i],r.before[i],1e-6f))return false;return true;};
    // Tone EQ step off, defaults (bypassed Tone EQ, no bands): the pass still
    // runs for the Dynamic EQ, which places a 0 dB node at 250 Hz on slot 1.
    const Controller::Options dynamicOnly{1,false,Style::rock,false,false,false,1.f,0u,true,false,false,false,false,false};
    const auto full=runDynamic(dynamicOnly);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::input,P::tone,P::compressor},"Dynamic EQ did not run the Tone EQ pass on its own");
    const auto* step=stepOf(full.status,P::dynamicEQ);
    check(step && step->changed && step->result.contains("250 Hz") && !stepOf(full.status,P::tone),"Dynamic EQ result row missing or wrong");
    check(full.analysis.nodes.size()>=1 && near(full.analysis.nodes[0].frequency,250,1),"Controller analysis missed the 250 Hz ring");
    const int target=juce::roundToInt(value(full,spectralforge::dynamicSlotID(0,"target")));
    check(target>=1 && target<=12,"Slot 1 was not assigned to a Tone EQ band");
    const auto band=[&](const DynamicRun& r,const char* f){return value(r,spectralforge::eqID(0,target-1,f));};
    const auto& node=full.analysis.nodes[0];
    check(near(band(full,"frequency"),250,.5f) && near(band(full,"gain"),0,.01f) && band(full,"enabled")>.5f && near(band(full,"type"),0,.01f) && near(band(full,"q"),node.q,.01f),"Tone EQ node does not match the analysis");
    check(near(value(full,spectralforge::dynamicSlotID(0,"threshold")),node.thresholdDb,.01f) && near(value(full,spectralforge::dynamicSlotID(0,"range")),node.rangeDb,.01f)
          && near(value(full,spectralforge::dynamicSlotID(0,"ratio")),node.ratio,.01f) && near(value(full,spectralforge::dynamicSlotID(0,"release")),node.releaseMs,.5f),"Dynamic EQ slot does not match the analysis");
    check(value(full,spectralforge::eqID(0,-1,"bypass"))<.5f,"The Tone EQ was left bypassed");
    int enabled=0;for(int b=0;b<spectralforge::graphicalEQBands;++b)enabled+=value(full,spectralforge::eqID(0,b,"enabled"))>.5f;
    check(enabled==int(full.analysis.nodes.size()),"The Dynamic EQ step enabled bands beyond its nodes");
    check(undoneAll(full),"UNDO AUTO did not restore the Dynamic EQ and Tone EQ");
    // Strength scales the maximum cut only.
    auto halfOptions=dynamicOnly;halfOptions.strength=.5f;
    const auto half=runDynamic(halfOptions);
    check(near(value(half,spectralforge::dynamicSlotID(0,"range")),.5f*node.rangeDb,.01f) && near(value(half,spectralforge::dynamicSlotID(0,"threshold")),node.thresholdDb,.01f),"Strength did not scale only the Dynamic EQ range");
    // Every slot already assigned: nothing to place.
    const auto busySlots=runDynamic(dynamicOnly,[](ChimeraProcessor& p){
        for(int s=0;s<4;++s){auto* t=p.parameters().getParameter(spectralforge::dynamicSlotID(s,"target"));t->setValueNotifyingHost(t->convertTo0to1(float(13+s)));}});
    step=stepOf(busySlots.status,P::dynamicEQ);
    check(step && !step->changed && step->result.contains("all four") && eqUnchanged(busySlots),"Assigned slots were not kept");
    // A user's curve behind a bypassed Tone EQ stays inaudible.
    const auto bypassed=runDynamic(dynamicOnly,[](ChimeraProcessor& p){p.parameters().getParameter(spectralforge::eqID(0,3,"enabled"))->setValueNotifyingHost(1.f);});
    step=stepOf(bypassed.status,P::dynamicEQ);
    check(step && !step->changed && step->result.contains("bypassed") && eqUnchanged(bypassed),"A bypassed user curve was switched on");
    // A Tone EQ lock keeps the Dynamic EQ too; its own lock keeps only it.
    auto toneLocked=dynamicOnly;toneLocked.tone=true;toneLocked.locks=Controller::lockTone;
    const auto locked=runDynamic(toneLocked);
    check(locked.status.passes==std::vector<P>{P::input,P::compressor} && locked.status.steps.size()==4 && locked.status.steps[1].phase==P::tone
          && locked.status.steps[2].phase==P::dynamicEQ && locked.status.steps[2].result=="kept (locked)","Tone EQ lock did not skip and report both steps in order");
    auto dynamicLocked=dynamicOnly;dynamicLocked.tone=true;dynamicLocked.locks=Controller::lockDynamicEQ;
    const auto ownLock=runDynamic(dynamicLocked);
    check(ownLock.status.passes==std::vector<P>{P::input,P::tone,P::compressor} && ownLock.status.steps.size()==4 && ownLock.status.steps[1].phase==P::tone
          && ownLock.status.steps[2].phase==P::dynamicEQ && ownLock.status.steps[2].result=="kept (locked)","Dynamic EQ lock did not keep only its step");
    for(int s=0;s<4;++s)check(near(value(ownLock,spectralforge::dynamicSlotID(s,"target")),0,.01f),"A locked Dynamic EQ slot was assigned");
    std::cout<<"MEASURE auto set dynamic EQ: "<<step->result<<" | "<<stepOf(full.status,P::dynamicEQ)->result<<'\n';
    std::cout<<"PASS auto set dynamic EQ controller: runs the Tone EQ pass alone, 0 dB node at 250 Hz on a free band and slot, strength scales the cut, assigned slots and a bypassed curve kept, Tone EQ and Dynamic EQ locks, undo\n";
}
// Phase 7 rules: loudness, trim, peak headroom and Final EQ corrections at the limiter input.
void bell(std::vector<float>& x,double frequency,double gain,double q,double rate) {
    const auto c=spectralforge::eqCoefficients(0,frequency,gain,q,rate);double z1=0,z2=0;
    for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}
void scale(std::vector<float>& x,float gain){for(auto& v:x)v*=gain;}
void levelRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 random{61};
        check(!analyseOutput(std::vector<float>(size_t(rate*6),0.f),rate).playingDetected,"Silence reported playing");
        // A 997 Hz sine at -20 dBFS on both channels reads -20 LUFS (BS.1770).
        {std::vector<float> sine(size_t(rate*6));for(size_t n=0;n<sine.size();++n)sine[n]=float(.1*std::sin(2*juce::MathConstants<double>::pi*997*double(n)/rate));
         const auto loud=integratedLoudness(sine,rate);check(loud && near(*loud,-20.f,.2f),"A -20 dBFS sine did not read -20 LUFS: "+std::to_string(loud?*loud:-999.f));}
        // On target and quiet: no Final EQ move, the trim brings it to -18 LUFS.
        auto quiet=targetNoise(false,rate,6,random);scale(quiet,.25f);
        const auto q=analyseOutput(quiet,rate,false);
        check(q.playingDetected && q.loudnessMeasured && q.activeMoves()==0,"On-target output got Final EQ moves");
        check(!q.peakLimited && near(q.trimChangeDb,std::round(std::min(outputMaxTrimChangeDb,outputTargetLufs-q.loudnessLufs)*2.f)*.5f,.01f) && q.trimChangeDb>0,
              "Quiet output was not brought toward the target: "+std::to_string(q.loudnessLufs)+" LUFS, peak "+std::to_string(q.peakDb)+", trim "+std::to_string(q.trimChangeDb));
        // Too loud: the trim comes down.
        auto hot=targetNoise(false,rate,6,random);scale(hot,3.f);
        const auto h=analyseOutput(hot,rate,false);
        check(h.trimChangeDb<0 && near(h.trimChangeDb,std::round(std::max(-outputMaxTrimChangeDb,std::min(outputTargetLufs-h.loudnessLufs,limiterCeilingDb-h.peakDb))*2.f)*.5f,.01f),"Loud output was not brought down");
        // Quiet with sharp peaks: the trim stops where the peaks reach the ceiling.
        auto clicks=quiet;scale(clicks,.1f);for(size_t n=0;n<clicks.size();n+=size_t(rate*.5))clicks[n]=.5f;
        const auto c=analyseOutput(clicks,rate,false);
        check(c.peakLimited && near(c.peakDb+c.trimChangeDb,limiterCeilingDb,.6f),"Peaks were pushed above the limiter ceiling");
        // Bright, dark and muddy outputs get half the difference, up to 3 dB.
        auto bright=targetNoise(false,rate,6,random);shelve(bright,6000,8,rate);
        const auto b=analyseOutput(bright,rate,false);
        check(b.moves[2].active && b.moves[2].type==2 && near(b.moves[2].frequency,8000,1) && b.moves[2].gainDb<=-2.f && b.moves[2].gainDb>=-finalMaxMoveDb && b.eqLoudnessDeltaDb<0,"Bright output did not get an air cut");
        auto dark=targetNoise(false,rate,6,random);shelve(dark,6000,-8,rate);
        const auto d=analyseOutput(dark,rate,false);
        check(d.moves[2].active && d.moves[2].gainDb>=2.f && d.moves[2].gainDb<=finalMaxMoveDb,"Dark output did not get an air lift");
        auto muddy=targetNoise(false,rate,6,random);bell(muddy,315,8,1,rate);
        const auto m=analyseOutput(muddy,rate,false);
        check(m.moves[1].active && near(m.moves[1].frequency,315,1) && m.moves[1].gainDb<=-2.f && !m.moves[2].active,
              "Muddy output did not get a low-mid cut: mud "+std::to_string(m.mudExcessDb)+" gain "+std::to_string(m.moves[1].gainDb)+" air "+std::to_string(m.airExcessDb)+" active "+std::to_string(m.moves[2].active));
        // Rumble under a guitar gets a 70 Hz high pass; a bass never does.
        auto rumble=targetNoise(false,rate,6,random);{std::vector<float> low(rumble.size());std::normal_distribution<float> noise(0,.4f);for(auto& v:low)v=noise(random);lowpass(low,55,rate);for(size_t n=0;n<rumble.size();++n)rumble[n]+=low[n];}
        const auto u=analyseOutput(rumble,rate,false);
        check(u.moves[0].active && u.moves[0].type==3 && near(u.moves[0].frequency,70,.1f),"Rumble under a guitar did not get a high pass");
        check(!analyseOutput(rumble,rate,true).moves[0].active,"A bass got the guitar's rumble high pass");
        if(rate==48000.)std::cout<<"MEASURE level rules: quiet "<<q.loudnessLufs<<" LUFS -> "<<q.trimChangeDb<<" dB; hot "<<h.loudnessLufs<<" -> "<<h.trimChangeDb<<"; clicks peak "<<c.peakDb
                                  <<" -> "<<c.trimChangeDb<<"; bright air "<<b.moves[2].gainDb<<" (excess "<<b.airExcessDb<<"); dark "<<d.moves[2].gainDb<<"; muddy "<<m.moves[1].gainDb<<" (excess "<<m.mudExcessDb<<"); rumble "<<u.rumbleExcessDb<<" dB\n";
    }
    std::cout<<"PASS auto set level rules: BS.1770 sine, on-target left alone and trimmed to -18 LUFS, loud trimmed down, peak headroom, bright/dark air, low-mid cut, guitar rumble high pass at 44.1/48/96 kHz\n";
}
// Phase 7 on the controller: the last pass hears a bright, quiet output.
struct LevelRun {std::vector<float> before,values,undone;Controller::Status status;OutputResult analysis;};
LevelRun runLevel(const Controller::Options& options,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    LevelRun result;result.before=values(*p);
    DI di{48000,.126f,.00025f};std::mt19937 random{67};auto bright=targetNoise(false,48000,6,random);shelve(bright,6000,8,48000);scale(bright,.25f);
    double now=juce::Time::getMillisecondCounterHiRes();std::vector<float> block(256);c.start();
    for(int k=0;c.busy() && k<int(60*48000/256);++k) {
        for(auto& x:block)x=di.next();
        const float* channels[]{block.data()};const float* master[]{bright.data()+size_t(k)*256%(bright.size()-256)};
        for(auto tap:{Tap::input,Tap::compressor,Tap::output,Tap::multiband,Tap::deesser,Tap::tone})p->autoSetCapture().push(tap,channels,1,256);
        p->autoSetCapture().push(Tap::master,master,1,256);
        now+=1000.*256/48000;c.tick(now);
    }
    check(!c.busy(),"Final EQ and output run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastLevelAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerLevel() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const Controller::Options levelOnly{1,false,Style::rock,false,false,false,1.f,0u,false,true,false,false,false,false};
    const auto full=runLevel(levelOnly);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::input,P::compressor,P::level},"Final EQ and output did not run as the last pass");
    const auto* step=stepOf(full.status,P::level);
    const auto& a=full.analysis;
    check(step && step->changed && step->result.contains("Final EQ") && step->result.contains("output"),"Final EQ and output result row missing");
    check(a.moves[2].active && a.moves[2].gainDb<0 && a.trimChangeDb!=0,"The bright, quiet output did not need an air cut and a trim");
    // The air shelf lands on a free Final EQ band, the trim moves by the analysis.
    int shelf=-1,enabled=0;
    for(int b=0;b<spectralforge::graphicalEQBands;++b)if(value(full.values,spectralforge::eqID(1,b,"enabled"))>.5f){++enabled;if(near(value(full.values,spectralforge::eqID(1,b,"type")),2,.01f))shelf=b;}
    check(shelf>=0 && enabled==a.activeMoves() && near(value(full.values,spectralforge::eqID(1,shelf,"gain")),a.moves[2].gainDb,.01f)
          && near(value(full.values,spectralforge::eqID(1,shelf,"frequency")),8000,1) && value(full.values,spectralforge::eqID(1,-1,"bypass"))<.5f,"Final EQ does not match the analysis");
    check(near(value(full.values,"output"),value(full.before,"output")+a.trimChangeDb,.01f),"Output trim does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the Final EQ and the output trim");
    // Strength scales the Final EQ gains and the trim change.
    auto halfOptions=levelOnly;halfOptions.strength=.5f;
    const auto half=runLevel(halfOptions);
    float halfShelf=0;for(int b=0;b<spectralforge::graphicalEQBands;++b)if(value(half.values,spectralforge::eqID(1,b,"enabled"))>.5f && near(value(half.values,spectralforge::eqID(1,b,"type")),2,.01f))halfShelf=value(half.values,spectralforge::eqID(1,b,"gain"));
    check(near(halfShelf,.5f*half.analysis.moves[2].gainDb,.01f) && near(value(half.values,"output"),scaledInputGainDb(value(half.before,"output"),value(half.before,"output")+half.analysis.trimChangeDb,.5f),.01f),"Strength did not scale the Final EQ and the trim");
    // Each lock keeps its part; both skip the pass.
    auto outputLocked=levelOnly;outputLocked.locks=Controller::lockOutput;
    const auto keptTrim=runLevel(outputLocked);
    check(value(keptTrim.values,"output")==value(keptTrim.before,"output") && stepOf(keptTrim.status,P::level)->result.contains("output kept (locked)"),"Output lock did not keep the trim");
    auto eqLocked=levelOnly;eqLocked.locks=Controller::lockFinalEQ;
    const auto keptEQ=runLevel(eqLocked);
    bool sameEQ=true;for(int b=0;b<spectralforge::graphicalEQBands;++b)sameEQ&=value(keptEQ.values,spectralforge::eqID(1,b,"enabled"))==value(keptEQ.before,spectralforge::eqID(1,b,"enabled"));
    check(sameEQ && value(keptEQ.values,"output")!=value(keptEQ.before,"output") && stepOf(keptEQ.status,P::level)->result.contains("Final EQ kept (locked)"),"Final EQ lock did not keep only the Final EQ");
    auto bothLocked=levelOnly;bothLocked.locks=Controller::lockOutput|Controller::lockFinalEQ;
    const auto skipped=runLevel(bothLocked);
    check(skipped.status.passes==std::vector<P>{P::input,P::compressor} && stepOf(skipped.status,P::level) && stepOf(skipped.status,P::level)->result=="kept (locked)","Locking both did not skip the pass");
    // A user curve behind a bypassed Final EQ stays inaudible; the trim still moves.
    const auto bypassed=runLevel(levelOnly,[](ChimeraProcessor& p){p.parameters().getParameter(spectralforge::eqID(1,4,"enabled"))->setValueNotifyingHost(1.f);});
    check(stepOf(bypassed.status,P::level)->result.contains("your curve is bypassed") && value(bypassed.values,spectralforge::eqID(1,-1,"bypass"))>.5f,"A bypassed Final EQ curve was switched on");
    std::cout<<"MEASURE auto set level controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set level controller: last pass, air shelf on a free Final EQ band, trim by the analysis, strength, Final EQ and output locks, bypassed curve kept, undo\n";
}

// Phase 8. A stand-in amp: tanh drive (GAIN 0-10 -> x0.5 to x158) and three
// tone filters (BASS 150 Hz shelf, MIDDLE 600 Hz bell, TREBLE 3 kHz shelf,
// 1.5 dB per step from 5).
void filter(std::vector<float>& x,int type,double frequency,double gain,double q,double rate) {
    const auto c=spectralforge::eqCoefficients(type,frequency,gain,q,rate);double z1=0,z2=0;
    for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}
AmpRenderer syntheticAmp(std::vector<float> input,double rate) {
    return [input=std::move(input),rate](const std::array<float,4>& v) {
        std::vector<float> y(input.size());const double drive=.5*std::pow(10.,v[0]/10.*2.5);
        for(size_t n=0;n<y.size();++n)y[n]=float(.25*std::tanh(drive*input[n]));
        filter(y,1,150,(v[1]-5)*1.5,.707,rate);filter(y,0,600,(v[2]-5)*1.5,1,rate);filter(y,2,3000,(v[3]-5)*1.5,.707,rate);
        return y;
    };
}
std::array<AmpKnob,4> knobs(float gain,float bass,float middle,float treble) {return {{{true,gain,0,10},{true,bass,0,10},{true,middle,0,10},{true,treble,0,10}}};}
void ampRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{61};const auto di=playedNotes(rate,ampRenderSeconds,rng);
        const auto amp=syntheticAmp(di,rate);
        // The measure: an almost linear amp keeps the dynamics, a hard one flattens them.
        check(ampCompression(di,amp({0,5,5,5}),rate)<.05f && ampCompression(di,amp({10,5,5,5}),rate)>.6f,"Compression measure does not follow the drive");
        const std::vector<float> silence(di.size());
        check(!analyseAmp(silence,silence,rate,knobs(0,5,5,5),syntheticAmp(silence,rate)).playingDetected,"Silence reported as playing");
        // GAIN bisects toward the style's target; metal drives harder than rock, clean backs off.
        const auto run=[&](std::array<AmpKnob,4> k,Style style){return analyseAmp(di,amp({k[0].value,k[1].value,k[2].value,k[3].value}),rate,k,amp,false,style);};
        const auto rock=run(knobs(0,5,5,5),Style::rock),metal=run(knobs(0,5,5,5),Style::metal),clean=run(knobs(10,5,5,5),Style::clean);
        check(rock.playingDetected && !rock.bass && near(rock.compressionTarget,.45f,1e-6f),"Rock guitar target not used");
        check(rock.changed(ampGain) && std::abs(rock.compressionAfter-.45f)<=ampCompressionTolerance && rock.gainReachable,"Rock GAIN did not reach the target");
        check(metal.proposed[ampGain].value>rock.proposed[ampGain].value && std::abs(metal.compressionAfter-.65f)<=ampCompressionTolerance,"Metal did not drive harder than rock");
        check(clean.proposed[ampGain].value<10 && clean.compressionAfter<=.15f+ampCompressionTolerance,"Clean did not back the GAIN off");
        // Near the target the knob stays; knob values snap to 1% of the range.
        const auto settled=run(knobs(rock.proposed[ampGain].value,5,5,5),Style::rock);
        check(!settled.changed(ampGain),"GAIN moved although the drive was already on target");
        check(near(rock.proposed[ampGain].value*10,std::round(rock.proposed[ampGain].value*10),1e-3f),"GAIN was not snapped to 1% of its range");
        // Tone: through this stand-in the plucked notes are heavy below 400 Hz and
        // dark above 2 kHz against the guitar target, so BASS comes down and
        // TREBLE goes up, each within 30% of its range, and the fit improves.
        const auto toned=run(knobs(rock.proposed[ampGain].value,5,5,5),Style::rock);
        check(toned.proposed[ampBass].value<5 && toned.proposed[ampTreble].value>5 && toned.toneErrorAfterDb<toned.toneErrorBeforeDb-ampToneMinGainDb,"Tone knobs did not move toward the target");
        for(int k=ampBass;k<=ampTreble;++k)check(std::abs(toned.proposed[size_t(k)].value-5)<=ampToneMaxMove*10+1e-3f,"A tone knob moved further than the limit");
        // The drive is re-checked after the tone moves.
        check(std::abs(toned.compressionAfter-.45f)<=ampCompressionTolerance,"GAIN was not re-set after the tone moved");
        // A role missing from the panel is left alone.
        auto missing=knobs(0,5,5,5);missing[ampTreble].present=false;
        const auto noTreble=run(missing,Style::rock);
        check(!noTreble.proposed[ampTreble].present && !noTreble.changed(ampTreble),"A missing knob was set");
        if(rate==48000.)std::cout<<"MEASURE amp rules 48 kHz: rock GAIN "<<rock.proposed[ampGain].value<<" ("<<rock.compressionBefore<<" > "<<rock.compressionAfter<<"), metal GAIN "<<metal.proposed[ampGain].value
                                <<", clean GAIN "<<clean.proposed[ampGain].value<<"; tone BASS "<<toned.proposed[ampBass].value<<" MIDDLE "<<toned.proposed[ampMiddle].value<<" TREBLE "<<toned.proposed[ampTreble].value
                                <<" (error "<<toned.toneErrorBeforeDb<<" > "<<toned.toneErrorAfterDb<<" dB)\n";
    }
    std::cout<<"PASS auto set amp rules at 44.1/48/96 kHz: compression measure, silence, rock/metal/clean GAIN targets, settled GAIN, tone toward the target with GAIN re-set, missing knob\n";
}
std::array<int,4> roleControls(int model,int channel,int route) {
    std::array<int,4> c{};for(int r=0;r<4;++r)c[size_t(r)]=spectralforge::easy::ampControl(model,channel,route,spectralforge::easy::Role(r));
    return c;
}
// The real native heads: the drive measure rises with GAIN, and a full
// analysis renders fast enough for the message thread.
void ampNativeProbe() {
    std::mt19937 rng{62};const auto di=playedNotes(48000,ampRenderSeconds,rng);
    for(int model:{0,2,8,spectralforge::firstOriginalAmpModel}) {
        auto state=spectralforge::defaultAmpNativeState(model);
        const auto controls=roleControls(model,state.channel,state.inputRoute);
        check(controls[ampGain]>=0,"Probe head has no GAIN knob");
        const auto& control=spectralforge::ampNativePanel(model).controls[size_t(controls[ampGain])];
        const auto render=nativeAmpRenderer(state,controls,di,48000);
        std::array<float,4> v{};for(int r=0;r<4;++r)v[size_t(r)]=controls[size_t(r)]>=0?state.values[size_t(controls[size_t(r)])]:0.f;
        auto low=v,high=v;low[ampGain]=control.minimum+.1f*(control.maximum-control.minimum);high[ampGain]=control.maximum;
        const float cLow=ampCompression(di,render(low),48000),cHigh=ampCompression(di,render(high),48000);
        check(cHigh>cLow,"The drive measure does not rise with the head's GAIN");
        std::array<AmpKnob,4> k{};
        for(int r=0;r<4;++r)if(controls[size_t(r)]>=0){const auto& c=spectralforge::ampNativePanel(model).controls[size_t(controls[size_t(r)])];k[size_t(r)]={true,v[size_t(r)],c.minimum,c.maximum};}
        const auto started=juce::Time::getMillisecondCounterHiRes();
        const auto r=analyseAmp(di,render(v),48000,k,render,false,Style::rock);
        const double ms=juce::Time::getMillisecondCounterHiRes()-started;
        check(r.playingDetected && ms<15000,"Native amp analysis failed or took too long");
        std::cout<<"MEASURE amp native "<<spectralforge::ampInfo(model).name<<": drive at GAIN 10% "<<cLow<<", max "<<cHigh<<"; rock analysis "<<r.compressionBefore<<" > "<<r.compressionAfter
                 <<", "<<r.changedKnobs()<<" knobs, "<<juce::roundToInt(ms)<<" ms\n";
    }
    std::cout<<"PASS auto set amp native probe: drive rises with GAIN on four heads, analysis time\n";
}
struct AmpRun {std::vector<float> before,values,undone;Controller::Status status;AmpResult analysis;juce::String gainID;};
// The pass hears the DI going into the amp and the head itself (no cabinet)
// at the settings it starts from. Fixtures feed the taps directly.
AmpRun runAmp(const Controller::Options& options,float gainFraction,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    constexpr int model=2;
    setRaw("mode",0);setRaw("ampon1",1);setRaw(spectralforge::ampNativeEnabledID(0),1);setRaw(spectralforge::ampNativeModelID(0),float(model));
    const int channel=p->selectedAmpChannel(0),route=p->selectedAmpNativeRoute(0);
    const auto controls=roleControls(model,channel,route);
    AmpRun result;result.gainID=spectralforge::ampNativeControlID(0,model,controls[ampGain],channel);
    {auto* gain=p->parameters().getParameter(result.gainID);gain->setValueNotifyingHost(gainFraction);}
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    result.before=values(*p);
    std::mt19937 rng{63};const auto di=playedNotes(48000,8,rng);
    std::array<float,4> now{};for(int r=0;r<4;++r)if(controls[size_t(r)]>=0)now[size_t(r)]=get(*p,spectralforge::ampNativeControlID(0,model,controls[size_t(r)],channel));
    const auto post=nativeAmpRenderer(p->autoSetAmpState(0),controls,di,48000)(now);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=di.size();offset+=256) {
        const float* in[]{di.data()+offset};const float* out[]{post.data()+offset};
        p->autoSetCapture().push(Tap::amp,in,1,256);
        for(auto tap:{Tap::tone,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,tap==Tap::tone?out:in,1,256);
        ms+=1000.*256/48000;c.tick(ms);
    }
    check(!c.busy(),"Amp run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastAmpAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerAmp() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const Controller::Options ampOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,true,false,false,false};
    const auto full=runAmp(ampOnly,.05f);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::amp);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::amp} && step && step->changed && step->result.contains("GAIN"),"Amp pass did not run or report");
    check(a.playingDetected && a.changed(ampGain) && a.proposed[ampGain].value>a.before[ampGain].value
          && std::abs(a.compressionAfter-a.compressionTarget)<std::abs(a.compressionBefore-a.compressionTarget),"A low GAIN was not raised toward the rock target");
    const float range=a.before[ampGain].maximum-a.before[ampGain].minimum;
    check(near(value(full.values,full.gainID),a.proposed[ampGain].value,.011f*range),"Applied GAIN does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the amp knobs");
    // Strength moves each knob part of the way.
    auto halfOptions=ampOnly;halfOptions.strength=.5f;
    const auto half=runAmp(halfOptions,.05f);
    check(near(value(half.values,half.gainID),half.analysis.before[ampGain].value+.5f*(half.analysis.proposed[ampGain].value-half.analysis.before[ampGain].value),.011f*range),"Strength did not scale the GAIN change");
    // The lock keeps the knobs and skips the pass.
    auto lockedOptions=ampOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockAmp;
    const auto locked=runAmp(lockedOptions,.05f);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::amp) && stepOf(locked.status,P::amp)->result=="kept (locked)"
          && value(locked.values,locked.gainID)==value(locked.before,locked.gainID),"Amp lock did not keep the knobs");
    // Dual routing and the classic engine are left alone, with the reason.
    const auto dual=runAmp(ampOnly,.05f,[](ChimeraProcessor& p){p.parameters().getParameter("mode")->setValueNotifyingHost(p.parameters().getParameter("mode")->convertTo0to1(1));});
    check(stepOf(dual.status,P::amp)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing amp was changed");
    const auto classic=runAmp(ampOnly,.05f,[](ChimeraProcessor& p){
        p.parameters().getParameter(spectralforge::ampNativeEnabledID(0))->setValueNotifyingHost(0);
        p.parameters().getParameter(spectralforge::ampNativeModelID(0))->setValueNotifyingHost(0);});
    check(stepOf(classic.status,P::amp)->result.contains("classic amp engine") && classic.values==classic.before,"Classic engine amp was changed");
    std::cout<<"MEASURE auto set amp controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set amp controller: amp pass alone, GAIN toward the rock target on the native head, strength, lock, Dual and classic engine kept, undo\n";
}
// Phase 9. Overlap-add FFT convolution, for rendering through the model's IR.
std::vector<float> convolve(const std::vector<float>& x,const std::vector<float>& h) {
    int order=1;while((1<<order)<2*int(h.size()))++order;
    const int size=1<<order,block=size-int(h.size())+1;juce::dsp::FFT fft(order);
    std::vector<float> response(size_t(2*size)),buffer(size_t(2*size)),y(x.size()+h.size());
    std::copy(h.begin(),h.end(),response.begin());fft.performRealOnlyForwardTransform(response.data());
    for(size_t start=0;start<x.size();start+=size_t(block)) {
        std::fill(buffer.begin(),buffer.end(),0.f);
        for(size_t n=0;n<size_t(block) && start+n<x.size();++n)buffer[n]=x[start+n];
        fft.performRealOnlyForwardTransform(buffer.data());
        for(int k=0;k<=size/2;++k) {
            const std::complex<float> product=std::complex<float>(buffer[size_t(2*k)],buffer[size_t(2*k+1)])*std::complex<float>(response[size_t(2*k)],response[size_t(2*k+1)]);
            buffer[size_t(2*k)]=product.real();buffer[size_t(2*k+1)]=product.imag();
        }
        fft.performRealOnlyInverseTransform(buffer.data());
        for(size_t n=0;n<size_t(size) && start+n<y.size();++n)y[start+n]+=buffer[n];
    }
    y.resize(x.size());return y;
}
spectralforge::cabLayout::Settings modeledMic(double position,double distance) {
    spectralforge::cabLayout::Settings s{};s.voice.base.enabled=true;s.voice.base.position=position;s.voice.base.distanceCm=distance;return s;
}
float bandAverage(const std::array<float,toneBandCount>& bands,float from,float to) {
    float sum=0;int count=0;for(int b=0;b<toneBandCount;++b)if(toneBandCentres[size_t(b)]>=from && toneBandCentres[size_t(b)]<=to){sum+=bands[size_t(b)];++count;}
    return sum/float(count);
}
void cabRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{91};
        const auto s=modeledMic(.5,10);
        // The model's bands follow the IR the engine plays: render through two placements.
        const auto noise=targetNoise(false,rate,6,rng);
        const auto through=[&](CabPlacement p){return convolve(noise,spectralforge::cabLayout::generate(spectralforge::cabLayout::key(withPlacement(s,p)),rate));};
        for(const CabPlacement other:{CabPlacement{.15,5},CabPlacement{.85,20}}) {
            const auto heardNow=measureSpectrum(through({.5,10}),rate,false),heardOther=measureSpectrum(through(other),rate,false);
            const auto now=cabBandsDb(s),next=cabBandsDb(withPlacement(s,other));
            float worst=0;
            for(int b=0;b<toneBandCount;++b)if(cabFitBand(b))worst=std::max(worst,std::abs((heardOther.bandsDb[size_t(b)]-heardNow.bandsDb[size_t(b)])-(next[size_t(b)]-now[size_t(b)])));
            check(worst<1.5f,"The model's band response does not predict the rendered IR (off by "+std::to_string(worst)+" dB)");
        }
        const std::vector<float> silence(size_t(rate*6));
        check(!analyseCab(silence,rate,s).playingDetected,"Silence reported as playing");
        // On the target already: the mic stays.
        const auto settled=analyseCab(targetNoise(false,rate,6,rng),rate,s,false);
        check(settled.playingDetected && !settled.changed(),"The mic moved although the balance was on target");
        // Too bright: the mic moves toward the cone's edge or away, and the top end drops.
        auto bright=targetNoise(false,rate,6,rng);shelve(bright,3000,8,rate);
        const auto darker=analyseCab(bright,rate,s,false);
        const auto topChange=[&](const CabResult& r){
            const auto now=cabBandsDb(s),next=cabBandsDb(withPlacement(s,r.proposed));
            std::array<float,toneBandCount> d{};for(int b=0;b<toneBandCount;++b)d[size_t(b)]=next[size_t(b)]-now[size_t(b)];
            return bandAverage(d,3150,6300)-bandAverage(d,250,1000);
        };
        check(darker.changed() && topChange(darker)<-2.f && darker.toneErrorAfterDb<darker.toneErrorBeforeDb-cabToneMinGainDb,"A bright sound did not move the mic to a darker place");
        // Too dark: toward the cap, and the top end rises.
        auto dark=targetNoise(false,rate,6,rng);shelve(dark,3000,-8,rate);
        const auto brighter=analyseCab(dark,rate,s,false);
        check(brighter.changed() && brighter.proposed.position<s.voice.base.position && topChange(brighter)>2.f,"A dark sound did not move the mic toward the cap");
        for(const auto& r:{darker,brighter}) {
            check(std::abs(r.proposed.position-r.before.position)<=cabPositionMaxMove+1e-6 && r.proposed.distanceCm>=5-1e-6 && r.proposed.distanceCm<=20+1e-6,"The mic moved further than the limit");
            check(r.roughnessAfterDb<=r.roughnessBeforeDb+cabRoughnessAllowanceDb+1e-4f,"The new placement cuts deeper notches");
            check(std::abs(r.proposed.distanceCm*2-std::round(r.proposed.distanceCm*2))<1e-6,"Distance not on the 0.5 cm grid");
        }
        // Moving away lowers the level, moving closer raises it; the controller makes it up on the mic's level.
        const float farther=cabLevelChangeDb(settled.heard,s,{.5,20}),nearer=cabLevelChangeDb(settled.heard,s,{.5,5});
        check(farther<-1.f && nearer>1.f && std::abs(cabLevelChangeDb(settled.heard,s,{.5,10}))<1e-4f,"Level change does not follow the distance ("+std::to_string(nearer)+", "+std::to_string(farther)+" dB)");
        if(rate==48000.)std::cout<<"MEASURE cab rules 48 kHz: bright > position "<<darker.proposed.position<<" distance "<<darker.proposed.distanceCm<<" cm (error "<<darker.toneErrorBeforeDb<<" > "<<darker.toneErrorAfterDb
                                <<" dB, level "<<cabLevelChangeDb(darker.heard,s,darker.proposed)<<" dB); dark > position "<<brighter.proposed.position<<" distance "<<brighter.proposed.distanceCm
                                <<" cm (error "<<brighter.toneErrorBeforeDb<<" > "<<brighter.toneErrorAfterDb<<" dB)\n";
    }
    std::cout<<"PASS auto set cab rules at 44.1/48/96 kHz: model matches the rendered IR, silence, on-target kept, bright moves darker, dark toward the cap, move limits, roughness, level change\n";
}
struct CabRun {std::vector<float> before,values,undone;Controller::Status status;CabResult analysis;float gainBefore{};};
// The pass hears the amp/cab output (a dark fixture fed to Tap::tone) with
// the Classic routing's mic A modeled at position 0.5, 10 cm.
CabRun runCab(const Controller::Options& options,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    setRaw("mode",0);setRaw("cab1",1);setRaw("ocab1_Aon",1);setRaw("ocab1_Aposition",.5f);setRaw("ocab1_Adistance",10);setRaw("cabAgain1",-2);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    CabRun result;result.before=values(*p);result.gainBefore=get(*p,"cabAgain1");
    std::mt19937 rng{92};auto post=targetNoise(false,48000,8,rng);shelve(post,3000,-8,48000);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=post.size();offset+=256) {
        const float* out[]{post.data()+offset};
        for(auto tap:{Tap::tone,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,out,1,256);
        ms+=1000.*256/48000;c.tick(ms);
    }
    check(!c.busy(),"Cab run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastCabAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerCab() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const Controller::Options cabOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,true,false,false};
    const auto full=runCab(cabOnly);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::cab);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::cab} && step && step->changed && step->result.contains("mic A position"),"Cabinet mic pass did not run or report");
    check(a.playingDetected && a.changed() && a.proposed.position<.5,"A dark sound did not move the modeled mic toward the cap");
    check(near(value(full.values,"ocab1_Aposition"),float(a.proposed.position),.0011f) && near(value(full.values,"ocab1_Adistance"),float(a.proposed.distanceCm),.051f),"Applied placement does not match the analysis");
    const float level=cabLevelChangeDb(a.heard,modeledMic(.5,10),{value(full.values,"ocab1_Aposition"),value(full.values,"ocab1_Adistance")});
    check(near(value(full.values,"cabAgain1"),std::round((full.gainBefore-level)*10.f)/10.f,.011f),"The mic's level does not hold the loudness");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the mic placement and level");
    // Strength moves the mic part of the way.
    auto halfOptions=cabOnly;halfOptions.strength=.5f;
    const auto half=runCab(halfOptions);
    check(near(value(half.values,"ocab1_Aposition"),float(.5+.5*(half.analysis.proposed.position-.5)),.0011f)
          && near(value(half.values,"ocab1_Adistance"),float(10+.5*(half.analysis.proposed.distanceCm-10)),.051f),"Strength did not scale the mic move");
    // The lock keeps the mic and skips the pass.
    auto lockedOptions=cabOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockCab;
    const auto locked=runCab(lockedOptions);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::cab) && stepOf(locked.status,P::cab)->result=="kept (locked)"
          && value(locked.values,"ocab1_Aposition")==value(locked.before,"ocab1_Aposition"),"Cabinet mic lock did not keep the mic");
    // Dual routing, a captured IR and two blended mics are left alone, with the reason.
    const auto setter=[](const char* id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    const auto dual=runCab(cabOnly,setter("mode",1));
    check(stepOf(dual.status,P::cab)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing mic was moved");
    const auto captured=runCab(cabOnly,setter("ocab1_Aon",0));
    check(stepOf(captured.status,P::cab)->result.contains("captured IR") && captured.values==captured.before,"A captured IR's mic was moved");
    const auto blended=runCab(cabOnly,[](ChimeraProcessor& p){
        for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"ocab1_Bon",1},{"cabblend1",.5f}}){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}});
    check(stepOf(blended.status,P::cab)->result.contains("two mics are blended") && blended.values==blended.before,"Blended mics were moved");
    // With the blend fully on mic B, mic B is the one placed.
    const auto onB=runCab(cabOnly,[](ChimeraProcessor& p){
        for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"ocab1_Bon",1},{"cabblend1",1},{"ocab1_Bposition",.5f},{"ocab1_Bdistance",10}}){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}});
    check(stepOf(onB.status,P::cab)->result.contains("mic B position") && value(onB.values,"ocab1_Bposition")!=value(onB.before,"ocab1_Bposition")
          && value(onB.values,"ocab1_Aposition")==value(onB.before,"ocab1_Aposition"),"The heard mic B was not the one placed");
    std::cout<<"MEASURE auto set cab controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set cab controller: cabinet mic pass alone, dark sound moves mic A toward the cap, level held, strength, lock, Dual, captured IR and blended mics kept, mic B, undo\n";
}
// Phase 10. The Tight 515 head with its GAIN at 80%, the plucked notes from the low E.
spectralforge::AmpNativeState drivenHead(float gainFraction=.8f) {
    auto state=spectralforge::defaultAmpNativeState(2);
    const auto controls=roleControls(2,state.channel,state.inputRoute);
    const auto& g=spectralforge::ampNativePanel(2).controls[size_t(controls[ampGain])];
    state.values[size_t(controls[ampGain])]=g.minimum+gainFraction*(g.maximum-g.minimum);
    return state;
}
void boostRules() {
    std::mt19937 rng{93};const auto di=playedNotes(48000,ampRenderSeconds,rng);
    const auto render=nativeBoostRenderer(drivenHead(),di,48000);
    const std::vector<float> silence(di.size());
    check(!analyseBoost(silence,48000,nativeBoostRenderer(drivenHead(),silence,48000),false,Style::metal).playingDetected,"Silence reported as playing");
    // The pedal itself: a bass cut in front of the amp, unity at LEVEL 0.5.
    const auto dry=render(std::nullopt),boosted=render(BoostSetting{0,.5f,.5f});
    const auto dryLow=boostLowExcessDb(measureSpectrum(dry,48000,false),Style::rock),boostedLow=boostLowExcessDb(measureSpectrum(boosted,48000,false),Style::rock);
    check(boostedLow<dryLow,"The Green Drive does not tighten the amp's low end");
    // Metal on a loose low end: the boost goes in, tightens the low end and keeps the drive in reach.
    const auto metal=analyseBoost(di,48000,render,false,Style::metal);
    check(metal.playingDetected && !metal.bass && metal.lowExcessBeforeDb>boostLooseLowDb,"The driven head's low end is not loose with low notes");
    check(metal.engaged && metal.lowExcessAfterDb<metal.lowExcessBeforeDb-boostMinTightenDb
          && metal.compressionAfter<=std::max(metal.compressionBefore,metal.compressionTarget)+boostDriveAllowance+1e-6f,"Metal did not get a boost that tightens the low end");
    check(std::find(boostDrives.begin(),boostDrives.end(),metal.setting.drive)!=boostDrives.end()
          && std::find(boostLevels.begin(),boostLevels.end(),metal.setting.level)!=boostLevels.end() && metal.setting.tone==.5f,"Boost setting off the candidate grid");
    // Clean style and bass get no boost.
    check(!analyseBoost(di,48000,render,false,Style::clean).engaged,"A clean style got a boost");
    check(!analyseBoost(di,48000,render,true,Style::metal).engaged,"A bass got a boost");
    // Notes whose lows are already thin leave the amp tight: no boost.
    auto thin=di;filter(thin,1,250,-24,.707,48000);
    const auto tight=analyseBoost(thin,48000,nativeBoostRenderer(drivenHead(),thin,48000),false,Style::metal);
    check(tight.playingDetected && tight.lowExcessBeforeDb<metal.lowExcessBeforeDb && !tight.engaged,"A tight low end got a boost");
    std::cout<<"MEASURE boost rules: pedal low end "<<dryLow<<" > "<<boostedLow<<" dB; metal "<<metal.lowExcessBeforeDb<<" > "<<metal.lowExcessAfterDb<<" dB, drive "<<metal.compressionBefore<<" > "<<metal.compressionAfter
             <<" (OVER DRIVE "<<metal.setting.drive<<", LEVEL "<<metal.setting.level<<"); thin lows "<<tight.lowExcessBeforeDb<<" dB\n";
    std::cout<<"PASS auto set boost rules: silence, the pedal tightens the low end, metal engages within the drive limit, candidate grid, clean, bass and tight lows kept\n";
}
struct BoostRun {std::vector<float> before,values,undone;Controller::Status status;BoostResult analysis;};
// The pass hears the DI going into the amp. Fixtures feed the taps directly.
BoostRun runBoost(const Controller::Options& options,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    constexpr int model=2;
    setRaw("mode",0);setRaw("ampon1",1);setRaw(spectralforge::ampNativeEnabledID(0),1);setRaw(spectralforge::ampNativeModelID(0),float(model));
    const int channel=p->selectedAmpChannel(0),route=p->selectedAmpNativeRoute(0);
    const auto controls=roleControls(model,channel,route);
    {auto* gain=p->parameters().getParameter(spectralforge::ampNativeControlID(0,model,controls[ampGain],channel));gain->setValueNotifyingHost(.8f);}
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    BoostRun result;result.before=values(*p);
    std::mt19937 rng{94};const auto di=playedNotes(48000,8,rng);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=di.size();offset+=256) {
        const float* in[]{di.data()+offset};
        for(auto tap:{Tap::amp,Tap::tone,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,in,1,256);
        ms+=1000.*256/48000;c.tick(ms);
    }
    check(!c.busy(),"Boost run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastBoostAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerBoost() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const Controller::Options boostOnly{1,false,Style::metal,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,true,false};
    const auto full=runBoost(boostOnly);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::boost);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::boost} && step && step->changed && step->result.contains("Green Drive in slot 5"),"Boost pass did not run or report");
    // The default board order puts owner 5 last, right before the amp.
    const auto id=[](int c){return spectralforge::pedalControlID(4,boostPedalModel,c);};
    check(a.engaged && near(value(full.values,spectralforge::pedalModelID(4)),float(boostPedalModel),.01f) && value(full.values,spectralforge::pedalBypassID(4,boostPedalModel))<.5f
          && near(value(full.values,id(0)),a.setting.drive,.011f) && near(value(full.values,id(1)),a.setting.tone,.011f) && near(value(full.values,id(2)),a.setting.level,.011f),"Applied boost does not match the analysis");
    for(int owner=0;owner<4;++owner)check(value(full.values,spectralforge::pedalModelID(owner))==value(full.before,spectralforge::pedalModelID(owner)),"Another board slot changed");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not remove the boost");
    // Strength scales the drive and the level change in dB.
    auto halfOptions=boostOnly;halfOptions.strength=.5f;
    const auto half=runBoost(halfOptions);
    const float levelDb=juce::Decibels::gainToDecibels(4*half.analysis.setting.level*half.analysis.setting.level);
    check(near(value(half.values,id(0)),.5f*half.analysis.setting.drive,.011f) && near(value(half.values,id(2)),std::sqrt(juce::Decibels::decibelsToGain(.5f*levelDb)/4.f),.011f),"Strength did not scale the boost");
    // The lock keeps the board and skips the pass.
    auto lockedOptions=boostOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockBoost;
    const auto locked=runBoost(lockedOptions);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::boost) && stepOf(locked.status,P::boost)->result=="kept (locked)"
          && value(locked.values,spectralforge::pedalModelID(4))==value(locked.before,spectralforge::pedalModelID(4)),"Boost lock did not keep the board");
    // Kept, with the reason: clean style, a drive already on the board, no free last slot, Dual routing.
    auto cleanOptions=boostOnly;cleanOptions.style=Style::clean;
    const auto clean=runBoost(cleanOptions);
    check(stepOf(clean.status,P::boost)->result.contains("clean style") && clean.values==clean.before,"A clean style got a boost");
    const auto setter=[](juce::String id,float v){return [id,v](ChimeraProcessor& p){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};};
    const auto drive=runBoost(boostOnly,setter(spectralforge::pedalModelID(0),3));
    check(stepOf(drive.status,P::boost)->result.contains("already on the board") && drive.values==drive.before,"A boost was added next to a drive");
    const auto chorus=runBoost(boostOnly,setter(spectralforge::pedalModelID(4),32)); // Classic Chorus
    check(stepOf(chorus.status,P::boost)->result.contains("no free board slot") && chorus.values==chorus.before,"A boost went before a later pedal");
    const auto dual=runBoost(boostOnly,setter("mode",1));
    check(stepOf(dual.status,P::boost)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing got a boost");
    std::cout<<"MEASURE auto set boost controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set boost controller: boost pass alone, Green Drive on the last free slot as analysed, strength, lock, clean, existing drive, occupied last slot and Dual kept, undo\n";
}
// Phase 11. Plucked notes at a fixed interval, as the input pass's 10 ms windows.
std::vector<float> pluckedEvery(double intervalMs,double seconds,double rate=48000,double decayPerSecond=5) {
    std::vector<float> x(size_t(seconds*rate));std::mt19937 rng{95};std::normal_distribution<float> hiss(0,.0003f);
    const size_t every=size_t(intervalMs*.001*rate);
    for(size_t n=0;n<x.size();++n){const double t=double(n%every)/rate;x[n]=float(.3*std::exp(-t*decayPerSecond)*std::sin(2*juce::MathConstants<double>::pi*196*t))+hiss(rng);}
    return x;
}
std::vector<Window> windowsOf(const std::vector<float>& x,double rate=48000) {
    std::vector<Window> w;const size_t block=size_t(rate*.01);
    for(size_t b=0;b+block<=x.size();b+=block){double sum=0;float peak=0;for(size_t n=b;n<b+block;++n){sum+=double(x[n])*x[n];peak=std::max(peak,std::abs(x[n]));}w.push_back({toDb(peak),toDb(float(std::sqrt(sum/double(block))))});}
    return w;
}
void spaceRules() {
    check(!analyseSpace(windowsOf(std::vector<float>(48000*6)),false,Style::rock).feel.measured,"Silence reported as playing");
    // Sparse notes with an explicitly selected 60 BPM sync reference.
    const auto sparse=analyseSpace(windowsOf(pluckedEvery(1000,6)),false,Style::clean,60);
    check(sparse.feel.measured && sparse.sparse && !sparse.dense && near(sparse.feel.onsetsPerSecond,1.f,.25f) && near(sparse.feel.beatMs,1000.f,20.f),"Sparse notes not measured as one a second");
    check(sparse.reverb.on && near(sparse.reverb.mix,.25f,.011f) && near(sparse.reverb.size,.6f,1e-6f) && sparse.delay.on && near(sparse.delay.timeMs,1000.f,20.f)
          && near(sparse.delay.mix,.18f,.011f) && near(sparse.delay.feedback,.3f,1e-6f),"Sparse clean guitar did not get a wide reverb and a delay on the beat");
    check(sparse.feel.pauseFraction>.2f && sparse.feel.sustainMs>200.f && sparse.feel.sustainMs<1000.f,"Pauses or ring not measured");
    // A 300 ms diagnostic pulse plus explicit 200 BPM sync; a 200 ms pulse remains diagnostic only.
    const auto eighths=analyseSpace(windowsOf(pluckedEvery(300,6,48000,12)),false,Style::rock,200);
    check(near(eighths.feel.beatMs,300.f,15.f) && eighths.delay.on && near(eighths.delay.timeMs,300.f,15.f),"Diagnostic pulse or explicit sync reference was not measured");
    const auto quick=analyseSpace(windowsOf(pluckedEvery(200,6,48000,20)),false,Style::rock);
    check(near(quick.feel.beatMs,400.f,20.f) && !quick.dense && !quick.delay.on,"A 200 ms note gap was not folded to a 400 ms beat, or the busy part got a delay");
    // Dense riffing: the reverb tightens and no delay is added; metal is driest.
    const auto dense=analyseSpace(windowsOf(pluckedEvery(100,6,48000,40)),false,Style::metal);
    check(dense.dense && dense.reverb.mix<=.05f && dense.reverb.size<=.15f+1e-6f && !dense.delay.on,"Dense metal playing was not kept tight");
    check(!analyseSpace(windowsOf(pluckedEvery(1000,6)),false,Style::metal).delay.on,"Metal got a delay");
    // Bass: a little reverb, no delay.
    const auto bass=analyseSpace(windowsOf(pluckedEvery(500,6)),true,Style::rock);
    check(bass.reverb.on && near(bass.reverb.mix,.05f,.011f) && !bass.delay.on,"Bass was not kept nearly dry");
    std::cout<<"MEASURE space rules: sparse "<<sparse.feel.onsetsPerSecond<<" notes/s, beat "<<sparse.feel.beatMs<<" ms, ring "<<sparse.feel.sustainMs<<" ms, pauses "<<sparse.feel.pauseFraction
             <<"; dense "<<dense.feel.onsetsPerSecond<<" notes/s, reverb "<<dense.reverb.mix<<"\n";
    std::cout<<"PASS auto set space rules: silence, sparse clean reverb and delay on the beat, diagnostic beat folding, dense metal tight, metal no delay, bass nearly dry\n";
}
struct SpaceRun {std::vector<float> before,values,undone;Controller::Status status;SpaceResult analysis;};
SpaceRun runSpace(const Controller::Options& options,const std::function<void(ChimeraProcessor&)>& prepare={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    // This harness explicitly selects a known manual quarter-note tempo.
    for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"delaysync",1},{"tempo",60}}) {
        auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));
    }
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    SpaceRun result;result.before=values(*p);
    const auto notes=pluckedEvery(1000,8);double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=notes.size();offset+=256) {
        const float* in[]{notes.data()+offset};
        for(auto tap:{Tap::input,Tap::compressor})p->autoSetCapture().push(tap,in,1,256);
        ms+=1000.*256/48000;c.tick(ms);
    }
    check(!c.busy(),"Space run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastSpaceAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerSpace() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const Controller::Options spaceOnly{1,false,Style::clean,false,false,false,1.f,Controller::lockCompressor,false,false,false,false,false,true};
    const auto full=runSpace(spaceOnly);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::space);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::input} && step && step->changed && step->result.contains("reverb on") && step->result.contains("delay on"),"Delay + reverb step did not run or report");
    check(value(full.values,"reverbon")>.5f && near(value(full.values,"reverbmix"),a.reverb.mix,.011f) && near(value(full.values,"reverbsize"),a.reverb.size,.011f)
          && value(full.values,"delayon")>.5f && value(full.values,"delaytime")==value(full.before,"delaytime") && near(value(full.values,"delaymix"),a.delay.mix,.011f),"Applied delay and reverb do not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the delay and reverb");
    // Strength scales the mixes.
    auto halfOptions=spaceOnly;halfOptions.strength=.5f;
    const auto half=runSpace(halfOptions);
    check(near(value(half.values,"reverbmix"),std::round(.5f*half.analysis.reverb.mix*100.f)/100.f,.011f) && near(value(half.values,"delaymix"),std::round(.5f*half.analysis.delay.mix*100.f)/100.f,.011f),"Strength did not scale the mixes");
    // A reverb the player has on keeps its size and damping; only its mix moves. A synced delay keeps the saved free-time value.
    const auto setter=[](std::initializer_list<std::pair<const char*,float>> pairs){std::vector<std::pair<const char*,float>> list(pairs);return [list](ChimeraProcessor& p){for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}};};
    const auto own=runSpace(spaceOnly,setter({{"reverbon",1},{"reverbsize",.9f},{"reverbdamping",.1f},{"reverbmix",.5f},{"delaysync",1}}));
    check(near(value(own.values,"reverbsize"),.9f,.011f) && near(value(own.values,"reverbdamping"),.1f,.011f) && near(value(own.values,"reverbmix"),own.analysis.reverb.mix,.011f)
          && value(own.values,"delaytime")==value(own.before,"delaytime") && value(own.values,"delayon")>.5f,"The player's reverb settings or the synced delay time were changed");
    // The lock keeps both effects.
    auto lockedOptions=spaceOnly;lockedOptions.locks=Controller::lockCompressor|Controller::lockSpace;
    const auto locked=runSpace(lockedOptions);
    check(stepOf(locked.status,P::space) && stepOf(locked.status,P::space)->result=="kept (locked)" && value(locked.values,"reverbon")==value(locked.before,"reverbon")
          && value(locked.values,"delayon")==value(locked.before,"delayon"),"Delay + reverb lock did not keep the effects");
    // Without the input pass there is no playing to judge.
    auto noInput=spaceOnly;noInput.locks=Controller::lockInput|Controller::lockGate;
    const auto skipped=runSpace(noInput);
    check(stepOf(skipped.status,P::space) && stepOf(skipped.status,P::space)->result.contains("input pass did not run"),"Missing input pass not reported");
    std::cout<<"MEASURE auto set space controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set space controller: step after the input pass, reverb and delay on the beat as analysed, strength, the player's reverb and saved free-time value kept, lock, no input pass, undo\n";
}
void parameterEditNotifications() {
    auto p=std::make_unique<ChimeraProcessor>();
    auto* q=p->parameters().getParameter("input");
    ParameterEdits edits(*q);
    const auto original=q->getValue();
    auto revision=edits.revision();
    const float target=q->convertTo0to1(4.f);
    check(edits.write(target,original,revision) && edits.unchanged(revision),"Auto Set's own notifications revoked ownership");
    // Automation on another thread must not be hidden by a process-wide flag.
    std::thread host([&]{q->setValueNotifyingHost(q->convertTo0to1(7.f));q->setValueNotifyingHost(target);});host.join();
    check(!edits.unchanged(revision),"Host-thread A-B-A notifications were lost");
    revision=edits.revision();
    q->beginChangeGesture();
    const auto touchedRevision=edits.revision();
    check(!edits.write(original,target,touchedRevision),"An active user gesture was overwritten");
    q->endChangeGesture();
    check(!edits.unchanged(revision),"A same-value manual gesture was not an edit");
    struct HostCallback final:juce::AudioProcessorListener {
        std::function<void()> onBegin,onValue,onEnd;
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {if(onValue){auto f=std::move(onValue);onValue={};f();}}
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {if(onBegin){auto f=std::move(onBegin);onBegin={};f();}}
        void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {if(onEnd){auto f=std::move(onEnd);onEnd={};f();}}
    } callback;
    p->addListener(&callback);
    const float hostValue=q->convertTo0to1(-3.f);
    for(int when=0;when<3;++when) {
        q->setValueNotifyingHost(original);revision=edits.revision();
        auto action=[&]{q->setValueNotifyingHost(hostValue);};
        if(when==0)callback.onBegin=action;else if(when==1)callback.onValue=action;else callback.onEnd=action;
        check(!edits.write(target,original,revision) && q->getValue()==hostValue,"A reentrant host edit was overwritten or reclaimed");
    }
    p->removeListener(&callback);
    // The added observer itself must not allocate in the host notification path.
    allocations=0;counting=true;q->setValueNotifyingHost(original);counting=false;
    check(allocations==0,"Parameter edit observation allocated on the audio thread");
    std::cout<<"PASS parameter edit notifications: own gestures, host thread, same-value touch, begin/value/end reentrancy, zero allocations\n";
}

void controllerEditHistory() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    Controller c(*p);c.setOptions({1,false,Style::rock,false,false,true,1.f,0u,false,false,false,false,false,false});
    auto* input=p->parameters().getParameter("input");
    auto* threshold=p->parameters().getParameter("cleanCompThreshold");
    const float before=input->getValue();
    const float gateBefore=get(*p,"gatethreshold");
    double now=juce::Time::getMillisecondCounterHiRes();
    DI di{48000,.126f,.00025f};std::vector<float> signal(48000*14);
    for(auto& x:signal)x=di.next();
    c.start();
    // Manual A -> B -> A is still a user decision, even though its final
    // value equals the snapshot. Host automation has the same notification path.
    input->beginChangeGesture();
    input->setValueNotifyingHost(input->convertTo0to1(3.f));
    input->setValueNotifyingHost(before);input->endChangeGesture();
    feedCapture(*p,c,signal,now);
    check(c.current().phase==Controller::Phase::done,"Edit-history run did not finish");
    check(input->getValue()==before,"Auto Set overwrote a manual A-B-A edit during listening");
    check(get(*p,"gatethreshold")==gateBefore && c.current().text.contains("input edited while listening"),"Gate used a capture with changing input gain");
    const float automated=threshold->getValue();
    threshold->setValueNotifyingHost(threshold->convertTo0to1(-17.f));
    threshold->setValueNotifyingHost(automated);
    c.undo();
    check(threshold->getValue()==automated,"Undo overwrote host automation that returned to Auto Set's value");
    p->releaseResources();
    std::cout<<"PASS Auto Set edit history: manual and host A-B-A changes survive apply and undo\n";
}


// Keep these checks independent of the controller's touched-ID list: every
// parameter in the user's locked stage (including manual-only controls) counts.
// The head knobs phase 8 may write (Classic routing, every head/channel/input).
const juce::StringArray& ampKnobIDs() {
    static const auto ids=[]{
        juce::StringArray result;
        for(int model=0;model<spectralforge::ampModelCount;++model) {
            const auto& panel=spectralforge::ampNativePanel(model);
            for(int channel=0;channel<std::max(1,int(panel.channels.size()));++channel)for(int route=0;route<std::max(1,int(panel.routes.size()));++route)
                for(int control:roleControls(model,channel,route))if(control>=0)result.addIfNotAlreadyThere(spectralforge::ampNativeControlID(0,model,control,channel));
        }
        return result;
    }();
    return ids;
}
bool inLockedStage(const juce::String& id,Controller::Lock lock) {
    switch(lock) {
        case Controller::lockInput:return id=="input";
        case Controller::lockGate:return id.startsWith("gate") || id.startsWith("exp"); // the gate lock covers Gate 2 and the Expander
        case Controller::lockTone:return id.startsWith("toneEQ_");
        case Controller::lockCompressor:return id.startsWith("cleanComp");
        case Controller::lockMultiband:return id.startsWith("mb");
        case Controller::lockDeEsser:return id.startsWith("deEsser");
        case Controller::lockLimiter:return id.startsWith("liveLimiter");
        case Controller::lockDynamicEQ:return id.startsWith("dynamicEQ");
        case Controller::lockFinalEQ:return id.startsWith("finalEQ_");
        case Controller::lockOutput:return id=="output";
        case Controller::lockAmp:return ampKnobIDs().contains(id);
        case Controller::lockSpace:return id=="delayon" || id=="delaytime" || id=="delayfeedback" || id=="delaymix" || id=="reverbon" || id=="reverbsize" || id=="reverbdamping" || id=="reverbmix";
        case Controller::lockBoost:return id.startsWith("board") && (id.endsWith("Model") || id.contains("_m1_"));
        case Controller::lockCab:return id=="ocab1_Aposition" || id=="ocab1_Adistance" || id=="ocab1_Bposition" || id=="ocab1_Bdistance" || id=="cabAgain1" || id=="cabBgain1";
    }
    return false;
}
void controllerStrengthMatrix() {
    const Controller::Options all{1,true,Style::rock,true,true,false,1.f,0u,false,false,false,false,false,false};
    const auto full=runLocked(all);
    const LockRun defaults{full.before,{},{}};
    check(valueOf(full,"mbOn")>.5f && valueOf(full,"deEsserOn")>.5f,"Strength matrix needs active positive controls");
    for(float amount:{.25f,.5f,.75f,1.f}) {
        auto o=all;o.strength=amount;const auto r=runLocked(o);
        check(r.status.phase==Controller::Phase::done,"Strength matrix run incomplete");
        const float input=valueOf(r,"input"),base=valueOf(defaults,"input"),maximum=valueOf(full,"input");
        check(near(input,float(std::round((std::round(double(base)*100)+double(amount)*(std::round(double(maximum)*100)-std::round(double(base)*100)))/50)*.5),.01f),"Strength did not scale the input delta");
        check(near(valueOf(r,"gatethreshold"),std::round(std::clamp(valueOf(full,"gatethreshold")+input-maximum,-90.f,-15.f)),.51f),"Gate did not follow scaled input");
        for(const auto* id:{"gateon","gateRangeDb","cleanCompOn","cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompDetector","mbOn","mbDetector","mbCrossover1","mbCrossover2","mbCrossover3","deEsserOn","deEsserFrequency","deEsserThreshold","deEsserAttack","deEsserRelease","deEsserRatio","deEsserKnee","liveLimiterOn","liveLimiterCeiling","liveLimiterRelease"})
            check(near(valueOf(r,id),valueOf(full,id),.001f),std::string("Strength changed an unscaled control: ")+id);
        check(near(valueOf(r,"cleanCompMix"),amount,.001f) && near(valueOf(r,"mbMix"),amount,.001f),"Parallel mix did not follow strength");
        check(near(valueOf(r,"deEsserRange"),amount*valueOf(full,"deEsserRange"),.002f),"De-esser range did not follow strength");
        int moves=0;
        for(int b=0;b<spectralforge::graphicalEQBands;++b) {
            const auto id=[&](const char* f){return spectralforge::eqID(0,b,f);};
            for(const auto* f:{"enabled","type","frequency","q"})check(near(valueOf(r,id(f)),valueOf(full,id(f)),.001f),"Strength changed EQ shape or active nodes");
            if(valueOf(full,id("enabled"))>.5f){++moves;check(near(valueOf(r,id("gain")),amount*valueOf(full,id("gain")),.002f),"Tone gain did not follow strength");}
        }
        check(moves>0,"Strength EQ checks were vacuous");
        for(int b=0;b<4;++b)for(const auto* f:{"bypass","mode","threshold","range","ratio","attack","release","knee","makeup","solo"})
            check(near(valueOf(r,spectralforge::multibandID(b,f)),valueOf(full,spectralforge::multibandID(b,f)),.001f),"Strength altered multiband settings instead of mix");
    }
    std::cout<<"PASS strength matrix: 25/50/75/100%, input/gate/EQ/comp/multiband/de-esser/limiter\n";
}
void controllerEveryLock() {
    for(const auto lock:Controller::lockOrder)for(float amount:{.25f,.5f,.75f,1.f}) {
        // The amp step runs only for its own lock (the fixtures feed no amp input), so it is skipped.
        Controller::Options o{1,true,Style::rock,true,true,false,amount,unsigned(lock),true,true,lock==Controller::lockAmp,lock==Controller::lockCab,lock==Controller::lockBoost,lock==Controller::lockSpace};
        std::vector<int> lockedIndices;std::vector<float> seeded;bool editedDuringRun=false;
        const auto r=runLocked(o,[&](ChimeraProcessor& p,Controller& c){
            auto applied=values(p);
            for(int i:lockedIndices)check(applied[size_t(i)]==seeded[size_t(i)],"A locked stage changed during Auto Set");
            // A host edit to every locked parameter is also outside Undo's scope.
            for(int i:lockedIndices)p.getParameters()[i]->setValueNotifyingHost(.63f);
            const auto automated=values(p);c.undo();const auto undone=values(p);
            for(int i:lockedIndices)check(undone[size_t(i)]==automated[size_t(i)],"Undo changed a locked parameter's host edit");
        },[&](ChimeraProcessor& p,Controller&){
            for(auto* q:p.getParameters())if(auto* named=dynamic_cast<juce::AudioProcessorParameterWithID*>(q))
                if(inLockedStage(named->paramID,lock)){lockedIndices.push_back(q->getParameterIndex());q->setValueNotifyingHost(.37f);}
            seeded=values(p);
        },[&](ChimeraProcessor& p,Controller&){
            if(!editedDuringRun){for(int i:lockedIndices)p.getParameters()[i]->setValueNotifyingHost(.47f);seeded=values(p);editedDuringRun=true;}
        });
        check(!lockedIndices.empty() && r.status.phase==Controller::Phase::done,"Individual lock coverage did not run");
    }
    // Options changed during a run belong to the next run. Undo still uses the
    // run snapshot, while a newly selected lock additionally protects its stage.
    Controller::Options initial{1,true,Style::rock,true,true,false,.5f,Controller::lockTone,false,false,false,false,false,false};
    bool changed=false;
    const auto baseline=runLocked(initial);
    const auto edited=runLocked(initial,[&](ChimeraProcessor& p,Controller& c){
        auto next=initial;next.locks=Controller::lockCompressor;c.setOptions(next);
        const auto threshold=get(p,"cleanCompThreshold");c.undo();
        check(get(p,"cleanCompThreshold")==threshold,"A newly locked stage was restored by Undo");
    },{},[&](ChimeraProcessor&,Controller& c){if(!changed){auto next=initial;next.locks=127;next.strength=.25f;next.amp=true;next.instrument=2;c.setOptions(next);changed=true;}});
    check(baseline.values==edited.values && baseline.status.passes==edited.status.passes,"Changing options mutated an in-flight run");
    std::cout<<"PASS all fourteen locks x four strengths: non-default values, host edits, undo, immutable run options, newly selected undo locks\n";
}
void controllerAttackLockRegression() {
    int cases=0;
    for(auto style:{Style::rock,Style::clean,Style::metal}) {
        std::mt19937 random{417};auto attack=targetNoise(false,48000,6,random,style);
        for(size_t n=0;n<attack.size();++n) {
            const double t=double(n)/48000,cycle=std::fmod(t,.25);
            const double pick=cycle<.008?.16*std::sin(2*juce::MathConstants<double>::pi*6000*t)*std::exp(-cycle*500):0;
            attack[n]=float(attack[n]*std::exp(-cycle*45)+pick);
        }
        for(float amount:{.25f,.5f,.75f,1.f})for(unsigned finishLock:{0u,unsigned(Controller::lockDeEsser),unsigned(Controller::lockLimiter)}) {
            const unsigned upstream=Controller::lockInput|Controller::lockGate|Controller::lockTone|Controller::lockCompressor|Controller::lockMultiband;
            const auto r=runLocked({1,true,style,true,true,false,amount,upstream|finishLock},[&](ChimeraProcessor& p,Controller& c){
                check(c.lastFinishAnalysis().playingDetected && c.lastFinishAnalysis().protectedAttackWindows>0 && !c.lastFinishAnalysis().deEsser,"Locks or strength regressed pick attack protection");
                check(get(p,"deEsserOn")<.5f,"Protected attack enabled the de-esser");
                c.undo();check(get(p,"deEsserOn")<.5f,"Undo altered the protected attack state");
            },{},{},&attack);
            check(r.status.phase==Controller::Phase::done,"Attack/lock regression did not finish");++cases;
        }
    }
    std::cout<<"PASS attack/lock regression: "<<cases<<" controller cases, three styles x four strengths x de-esser/limiter lock modes\n";
}
// Phase 5 also consumes the input pass: mixed-gain evidence must not change
// any gate stage, including when host automation returns to the original value.
void controllerGateStageEditHistory() {
    const std::vector<juce::String> gateIDs{
        "gateon","gatethreshold","gateRangeDb","gateAfterRig","gateMode",
        "gate2On","gate2Mode","gate2Threshold","gate2Ratio","gate2Attack","gate2Hold","gate2Release","gate2RangeDb",
        "expOn","expAfterRig","expThreshold","expRatio","expAttack","expHold","expRelease","expRangeDb"};
    int cases=0;
    for(auto style:{Style::clean,Style::rock,Style::metal}) for(float amount:{.25f,.5f,.75f,1.f}) {
        std::vector<float> control;
        // 0 control; 1/2 manual/host input A-B-A; 3 Gate lock + host edits;
        // 4 gate host A-B-A during capture; 5 after apply; 6 options during
        // capture; 7 newly selected Undo lock; 8 input locked, gate free;
        // 9 Gate lock without host edits (separate lock-only control).
        for(int edit=0;edit<10;++edit) {
            auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
            Controller c(*p);
            Controller::Options options{1,false,style,false,false,true,amount,
                (edit==3 || edit==9)?unsigned(Controller::lockGate):edit==8?unsigned(Controller::lockInput):0u,false,false,false,false,false,false};
            c.setOptions(options);
            auto gateValues=[&]{std::vector<float> v;for(const auto& id:gateIDs)v.push_back(get(*p,id));return v;};
            auto gateABA=[&] {
                for(const auto& id:gateIDs) {
                    auto* q=p->parameters().getParameter(id);const float original=q->getValue();
                    q->setValueNotifyingHost(original<.5f?1.f:0.f);q->setValueNotifyingHost(original);
                }
            };
            if(edit==3 || edit==9)for(const auto& id:gateIDs)p->parameters().getParameter(id)->setValueNotifyingHost(.37f);
            const auto before=gateValues();auto preserved=before;
            auto* input=p->parameters().getParameter("input");const float original=input->getValue();
            double now=juce::Time::getMillisecondCounterHiRes();c.start();
            for(int offset=0;offset<48000*14;offset+=256) {
                float in[256],rig[256];
                for(int n=0;n<256;++n) {
                    const int sample=offset+n;const bool playing=sample%96000<72000;
                    const float wave=std::sin(float(2*juce::MathConstants<double>::pi*1000*sample/48000.));
                    in[n]=wave*(playing?.1f:.0001f);rig[n]=wave*(playing?.2f:.001f);
                }
                if(offset==256*500) {
                    if(edit==1 || edit==2) {
                        if(edit==1)input->beginChangeGesture();
                        input->setValueNotifyingHost(input->convertTo0to1(3.f));input->setValueNotifyingHost(original);
                        if(edit==1)input->endChangeGesture();
                    } else if(edit==3) {
                        for(const auto& id:gateIDs)p->parameters().getParameter(id)->setValueNotifyingHost(.47f);
                        preserved=gateValues();
                    } else if(edit==4){gateABA();preserved=gateValues();}
                    else if(edit==6) {
                        auto next=options;next.gates=false;next.strength=amount==1.f?.25f:1.f;
                        next.locks=Controller::lockGate;c.setOptions(next);
                    }
                }
                const float* ins[]{in};const float* rigs[]{rig};
                for(auto tap:{Tap::input,Tap::tone,Tap::compressor,Tap::deesser,Tap::output})p->autoSetCapture().push(tap,ins,1,256);
                p->autoSetCapture().push(Tap::rig,rigs,1,256);
                now+=1000.*256/48000.;c.tick(now);
            }
            check(c.current().phase==Controller::Phase::done,"Gate-stage matrix did not finish");
            const auto applied=gateValues();
            if(edit==1 || edit==2 || edit==3 || edit==4 || edit==9) {
                check(applied==preserved,"Gate-stage apply overwrote input/gate edits or a Gate lock");
                if(edit==1 || edit==2)check(input->getValue()==original,"Input A-B-A was overwritten");
                if(edit==3 || edit==9)check(std::any_of(c.current().steps.begin(),c.current().steps.end(),
                    [](const auto& s){return s.phase==Controller::Phase::gates && s.result=="kept (locked)" && !s.changed;}),
                    "Gate lock was not reported as kept");
            } else {
                check(applied!=before && c.lastGateAnalysis().measured,"Gate-stage control did not apply measured evidence");
                const bool hard=style==Style::metal;
                check(get(*p,hard?"gate2On":"expOn")>.5f,"Gate-stage fixture did not exercise its hard/soft plan");
                // The fixed signal has a -80 dBFS noise peak: the opening
                // threshold is nine dB above it, at the ACTUAL input gain.
                // Gate threshold is integer dB, so allow half a dB quantisation.
                const float opening=get(*p,hard?"gatethreshold":"expThreshold");
                check(near(opening,-71.f+get(*p,"input"),.51f),"Gate stages ignored the applied input gain");
                check(near(get(*p,hard?"gate2RangeDb":"expRangeDb"),hard?18.f:15.f,.01f),
                    "Strength weakened the gate-stage protection range");
                if(hard)check(near(get(*p,"gate2Threshold"),opening-6.f,.01f),"Gate 2 threshold lost its main-gate offset");
                if(edit==0)control=applied;
                if(edit==6)check(applied==control,"Mid-run options changed the active gate plan");
                if(edit==8)check(input->getValue()==original,"Locked input gain was changed");
            }
            if(edit==5)gateABA();
            const auto undoProtected=gateValues();
            if(edit==7){auto next=options;next.locks=Controller::lockGate;c.setOptions(next);}
            c.undo();
            const auto afterUndo=gateValues();
            if(edit==5 || edit==6 || edit==7)
                check(afterUndo==undoProtected,"Undo overwrote a newer host edit or newly selected Gate lock");
            else if(edit==1 || edit==2 || edit==3 || edit==4 || edit==9)
                check(afterUndo==preserved,"Undo changed preserved gate-stage values");
            else for(size_t i=0;i<before.size();++i)
                check(near(afterUndo[i],before[i],.001f),"Gate-stage Undo changed "+gateIDs[i].toStdString());
            p->releaseResources();++cases;
        }
    }
    std::cout<<"PASS gate-stage matrix: "<<cases<<" cases, three styles x four strengths x ten ownership/option modes; 21 gate parameters\n";
}

#include "AutoSetDynamicEQRegression.h"
#include "AutoSetDICommands.h"
#include "AutoSetSpaceRegression.h"

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc==2 && std::string(argv[1])=="--space-regression"){spaceRules();controllerSpace();spaceSignalRegression();spaceControllerRegression();return 0;}
        if(argc==3 && std::string(argv[1])=="--space-report"){spaceSignalRegression(juce::File(argv[2]));return 0;}
        if(argc==3 && std::string(argv[1])=="--space-fixtures"){spaceWriteFixtures(juce::File(argv[2]));return 0;}
        if(argc==2 && std::string(argv[1])=="--risk-probe"){riskProbe();return 0;}
        if(argc==2 && std::string(argv[1])=="--dynamic-risk-probe"){dynamicRiskProbe();return 0;}
        if(argc==2 && std::string(argv[1])=="--dynamic-ownership"){dynamicOwnershipRegression();return 0;}
        if(argc==2 && std::string(argv[1])=="--dynamic-signals"){dynamicSignalRegression();return 0;}
        if(argc==2 && std::string(argv[1])=="--dynamic-regression"){dynamicEQRules();controllerDynamicEQ();dynamicSignalRegression();dynamicOwnershipRegression();dynamicRenderRegression();dynamicEditPlacementRegression();dynamicUserCurveRegression();return 0;}
        if(argc==3 && std::string(argv[1])=="--dynamic-eq-fixtures") {
            const juce::File dir(argv[2]);check(dir.createDirectory().wasOk(),"Fixture directory");
            for(double rate:{44100.,48000.,96000.})for(int kind=0;kind<4;++kind) {
                std::mt19937 rng{43};const auto x=kind==0?dynamicSustain(rate,27.5,false,true):kind==1?dynamicSustain(rate,82.4069,false,false):playedNotes(rate,6,rng,kind==3?250.f:0.f,kind==3?12.f:0.f);
                juce::AudioBuffer<float> b(1,int(x.size()));b.copyFrom(0,0,x.data(),int(x.size()));
                autoSetDI::write(dir.getChildFile(juce::String(int(rate))+"-"+juce::String(kind)+".wav"),b,rate);
            }
            return 0;
        }
        if(argc==5 && std::string(argv[1])=="--dynamic-eq-capture") {autoSetDI::capture(juce::File(argv[2]),juce::File(argv[3]),juce::File(argv[4]));return 0;}
        if(argc==7 && std::string(argv[1])=="--dynamic-eq-wav") {
            check(std::string(argv[3])=="guitar" || std::string(argv[3])=="bass","Instrument must be guitar or bass");
            check(std::string(argv[4])=="rock" || std::string(argv[4])=="clean" || std::string(argv[4])=="metal","Unknown style");
            const float strength=std::stof(argv[5]);check(strength>=.1f && strength<=1.f,"Strength must be 0.1..1");
            autoSetDI::analyse(juce::File(argv[2]),std::string(argv[3])=="bass",std::string(argv[4])=="clean"?Style::clean:std::string(argv[4])=="metal"?Style::metal:Style::rock,strength,juce::File(argv[6]));return 0;
        }
        check(argc==1,"Unknown argument");
        parameterEditNotifications();controllerEditHistory();controllerGateStageEditHistory();analysisRules();toneRules();finishRules();attackSafetyRules();multibandRules();gateStageRules();dynamicEQRules();levelRules();ampRules();ampNativeProbe();cabRules();boostRules();spaceRules();captureWindows();controllerFinishPreservation();controllerIntegration();inputStrengthRounding();controllerLocksAndStrength();controllerStrengthMatrix();controllerEveryLock();controllerAttackLockRegression();controllerDynamicEQ();controllerLevel();controllerAmp();controllerCab();controllerBoost();controllerSpace();spaceSignalRegression();spaceControllerRegression();
        dynamicSignalRegression();dynamicOwnershipRegression();dynamicRenderRegression();dynamicEditPlacementRegression();dynamicUserCurveRegression();
        std::cout<<"PASS Auto Set\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

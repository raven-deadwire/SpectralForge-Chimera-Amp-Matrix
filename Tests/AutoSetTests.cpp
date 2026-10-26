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
        Controller c(*p);c.setOptions({bass?2:1,false,style,true,false});Gestures g;p->addListener(&g);
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
    check(c.current().phase==Controller::Phase::tone && c.current().text.contains("2/5"),"First pass did not hand over to the Tone EQ pass");
    check(get(*p,"input")>4.f && get(*p,"input")<=12.01f,"Input gain was not raised toward the target");
    // With an Expander (phase 5) it takes the threshold above the noise and the main gate sits below it.
    const float placed=get(*p,"expOn")>.5f?get(*p,"expThreshold"):get(*p,"gatethreshold");
    check(get(*p,"gateon")>.5f && placed>-70.f && placed<-35.f && get(*p,"gatethreshold")<=placed,"Gate threshold was not placed above the noise floor");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::compressor,"Tone EQ pass did not hand over to the compressor pass");
    const auto& tone=c.lastToneAnalysis();
    check(tone.playingDetected && !tone.bass,"Tone pass did not hear the guitar DI");
    check(dynamicNode()==nodeBefore && near(nodeBefore[1],700.f,.01f) && near(nodeBefore[2],-3.f,.01f),"Auto Set overwrote a Dynamic EQ node");
    int enabled=0;
    for(int b=0;b<spectralforge::graphicalEQBands;++b)if(b!=1 && get(*p,spectralforge::eqID(0,b,"enabled"))>.5f) {
        ++enabled;check(std::abs(get(*p,spectralforge::eqID(0,b,"gain")))<=toneMaxMoveDb+.01f,"Tone EQ move beyond the limit");
    }
    check(enabled==tone.activeMoves() && (enabled==0 || get(*p,spectralforge::eqID(0,-1,"bypass"))<.5f),"Applied Tone EQ does not match the analysis");
    run(*p,c,&di,7.0,now);
    check(c.current().phase==Controller::Phase::multiband && c.current().text.contains("4/5"),"Compressor pass did not hand over to the multiband pass");
    run(*p,c,&di,6.6,now);
    check(c.current().phase==Controller::Phase::finish && c.current().text.contains("5/5"),"Multiband pass did not hand over to the de-esser and limiter pass");
    const auto& mb=c.lastMultibandAnalysis();
    check(mb.playingDetected && (get(*p,"mbOn")>.5f)==(mb.activeBands()>0),"Multiband pass did not hear the guitar or did not apply its analysis");
    for(int b=0;b<4 && mb.activeBands()>0;++b)
        check((get(*p,spectralforge::multibandID(b,"bypass"))<.5f)==mb.bands[size_t(b)].active && get(*p,spectralforge::multibandID(b,"solo"))<.5f,"Multiband band state does not match the analysis");
    run(*p,c,&di,6.6,now);
    p->removeListener(&g);
    check(c.current().phase==Controller::Phase::done && c.current().canUndo,"Auto Set did not finish with undo available");
    const auto& finish=c.lastFinishAnalysis();
    check(finish.playingDetected && finish.limiterPlaying && !finish.bass,"Finish pass did not hear the de-esser and limiter inputs");
    // The live view says what each pass listens for, what it heard and how long it takes.
    {
        bool sawInput=false,sawPause=false,sawTone=false,monotonic=true;double last=0;
        for(const auto& s:statuses) {
            if(s.pass<0)continue;
            check(s.passes.size()==5 && s.phase==s.passes[size_t(s.pass)] && s.listeningFor.isNotEmpty() && s.heard.isNotEmpty(),"Listening status does not describe its pass");
            check(near(float(s.totalSeconds),5*6.f+4*.3f,.01f) && s.passSeconds<=s.passTotalSeconds+1e-6,"Listening status has the wrong duration");
            monotonic&=s.seconds+1e-6>=last;last=s.seconds;
            sawInput|=s.phase==Controller::Phase::input && s.levelDb>-30.f && s.heard.contains("Playing: heard");
            sawPause|=s.heard.contains("Pause: heard");
            sawTone|=s.phase==Controller::Phase::tone && s.heard.contains("Guitar");
        }
        check(monotonic && sawInput && sawPause,"Listening status did not report the playing level, playing and the pause");
        check(!sawTone,"Tone pass claimed an instrument before its analysis");
        const auto& steps=c.current().steps;
        check(steps.size()==6 && steps[0].phase==Controller::Phase::input && steps[0].result.contains("input") && steps[1].phase==Controller::Phase::gates
              && steps[5].phase==Controller::Phase::finish && steps[3].changed && steps[3].result.contains(":1"),"Results do not list one row per pass with the applied values");
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
    check(changed>=3 && changed<=31+5*spectralforge::graphicalEQBands+10*spectralforge::multibandBandCount,"Auto Set touched parameters outside its scope");
    c.undo();
    // Undo restores normalised values, which round-trip within float precision.
    const auto restored=values(*p);bool undone=restored.size()==before.size();
    for(size_t i=0;undone && i<before.size();++i)undone=near(restored[i],before[i],1e-6f);
    check(undone && !c.current().canUndo,"Undo did not restore every parameter");
    c.setOptions({0,false,Style::rock,false,false});c.start();
    check(c.current().phase==Controller::Phase::input && c.current().text.contains("1/2"),"Excluding the optional steps did not shorten Auto Set to two passes");
    c.cancel();check(values(*p)==restored,"Cancelling a run changed parameters");
    // Edits made while listening and after applying are not owned by Auto Set.
    // Include an untouched optional stage: the old whole-snapshot undo lost it.
    c.setOptions({1,false,Style::rock,false,false});c.start();
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

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc==2 && std::string(argv[1])=="--risk-probe"){riskProbe();return 0;}
        check(argc==1,"Unknown argument");
        analysisRules();toneRules();finishRules();attackSafetyRules();multibandRules();gateStageRules();captureWindows();controllerFinishPreservation();controllerIntegration();
        std::cout<<"PASS Auto Set\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

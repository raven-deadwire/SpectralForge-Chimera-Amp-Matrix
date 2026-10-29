// 1.4 PRE Expansion A (docs/PRE_EXPANSION_A_1_4.md): parameters of models
// from 40 on appended after the released list at version 16, old-project
// defaults; Para EQ (model 40) response, ranges, transparency and board
// integration; Multi Comp (model 41) static curve, attack, multiband
// independence, balance, TubeSim harmonics and gain-reduction metering; no
// allocation.
#include "PluginProcessor.h"
#include "EffectSelectionCatalog.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <map>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using namespace spectralforge;
constexpr int paraEQ=40,multiComp=41,tightBoost=42,powerWah=43,bassSynth=44,dreadfulPeril=45,sevenEQ=46,multiWah=47,octaveFuzz=48,violetRage=49,fireDrake=50,pitchBend=51;
bool expansionID(const juce::String& id){for(int m=releasedPedalModelCount;m<pedalModelCount;++m)if(id.startsWith("board") && id.contains("_m"+juce::String(m)+"_"))return true;return false;}
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
using Controls=std::array<float,12>;
Controls neutral() {Controls c{};const auto i=defaultPedalInstance(paraEQ);for(size_t k=0;k<c.size();++k)c[k]=i.controls[k];return c;}

void catalogAndMenu() {
    const auto& m=pedalModel(paraEQ);
    require(juce::String(m.id)=="pre.paraeq" && juce::String(m.name)=="Para EQ" && m.implemented && m.controlCount==11,"Para EQ catalog entry");
    require(releasedPedalModelCount==40 && pedalModelCount==52,"Model counts");
    require(juce::String(pedalMenuName(paraEQ))=="Para EQ" && pedalMenuEntries[paraEQ].kind==PedalMenuKind::equalizer,"Para EQ is not in the Equalizer menu");
    const auto n=neutral();
    require(n[1]==0 && n[4]==0 && n[7]==0 && juce::roundToInt(n[2])==1 && n[10]<.5f,"Para EQ does not start flat with medium Q and boost off");
    std::cout<<"PASS Para EQ catalog: model 40, 11 controls, Equalizer menu, starts flat\n";
}
void parametersAppend() {
    auto p=std::make_unique<ChimeraProcessor>();const int total=p->getParameters().size();
    // The banks need not be last: later releases append after them.
    int first=-1;for(auto* raw:p->getParameters())if(const auto* q=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);q && expansionID(q->paramID)){first=raw->getParameterIndex();break;}
    require(first>=0,"No expansion parameter found");
    require(pedalExpansionParameterCount==5*(12+6+2+5+9+11+9+6+10+7+7+4),"Expansion banks: 5 x (bypass + controls) for Para EQ (11), Multi Comp (5), Tight Boost (1), Power Wah (4), Bass Synth (8), Dreadful Peril (10), Seven EQ (8), Multi Wah (5), Octave Fuzz (9), Violet Rage (6), Fire Drake (6), Pitch Bend (3)");
    int seen=0,lastA=-1,firstB=total;
    for(auto* raw:p->getParameters()) {
        const auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);require(parameter!=nullptr,"Parameter without ID");
        const bool expansion=expansionID(parameter->paramID);
        if(expansion){
            // PRE A (models 40-45) at version 16, then PRE B (46 on) at 17:
            // a later group never sits between an earlier group's banks.
            const int model=parameter->paramID.fromFirstOccurrenceOf("_m",false,false).getIntValue();
            const int version=model<46?16:17;
            require(parameter->getParameterIndex()>=first && parameter->getVersionHint()==version,"An expansion parameter is not appended at its group's version");
            if(version==16)lastA=std::max(lastA,parameter->getParameterIndex());else firstB=std::min(firstB,parameter->getParameterIndex());
            ++seen;}
        else require(parameter->getParameterIndex()<first || parameter->getParameterIndex()>=first+pedalExpansionParameterCount,"A parameter sits inside the PRE expansion banks");
    }
    require(seen==pedalExpansionParameterCount,"Expansion banks incomplete");
    require(lastA<firstB,"A PRE B bank sits among the PRE A banks");
    for(int owner=0;owner<5;++owner)require(p->parameters().getParameter(pedalBypassID(owner,paraEQ))!=nullptr && p->parameters().getParameter(pedalControlID(owner,paraEQ,9))!=nullptr,"Para EQ bank missing for an owner");
    std::cout<<"PASS expansion parameters: "<<pedalExpansionParameterCount<<" appended as one contiguous block after every released parameter, PRE A at version 16 then PRE B at 17\n";
}
// Steady-state gain of a sine through the EQ, in dB.
double measuredDb(PedalParaEQ& eq,const Controls& c,double rate,double hz) {
    eq.reset();juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
    for(int block=0;block<int(rate*.6/256);++block) {
        for(int n=0;n<256;++n,++t){const float x=.1f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/rate));b.setSample(0,n,x);b.setSample(1,n,-x);}
        juce::AudioBuffer<float> dry;dry.makeCopyOf(b);eq.process(b,c);
        if(block>int(rate*.3/256))for(int n=0;n<256;++n){in+=double(dry.getSample(0,n))*dry.getSample(0,n);out+=double(b.getSample(0,n))*b.getSample(0,n);}
    }
    return 10*std::log10(out/in);
}
void response() {
    for(const double rate:{44100.,48000.,96000.}) {
        PedalParaEQ eq;eq.prepare(rate);
        // Band ranges and the Nyquist guard.
        require(std::abs(PedalParaEQ::frequency(0,0)-35)<1e-3f && std::abs(PedalParaEQ::frequency(0,1)-500)<1e-2f && std::abs(PedalParaEQ::frequency(1,0)-250)<1e-2f
                && std::abs(PedalParaEQ::frequency(1,1)-5000)<.1f && std::abs(PedalParaEQ::frequency(2,0)-1000)<.1f && std::abs(PedalParaEQ::frequency(2,1)-20000)<1,"Band ranges");
        // Neutral: exactly transparent.
        {const auto c=neutral();juce::AudioBuffer<float> b(2,512),d(2,512);
         for(int n=0;n<512;++n){const float x=.3f*float(std::sin(.05*n));b.setSample(0,n,x);b.setSample(1,n,.5f*x);}d.makeCopyOf(b);eq.reset();eq.process(b,c);
         float error=0;for(int ch=0;ch<2;++ch)for(int n=0;n<512;++n)error=std::max(error,std::abs(b.getSample(ch,n)-d.getSample(ch,n)));
         require(error<1e-6f,"Neutral Para EQ is not transparent");}
        // Measured gain equals the analytic peaking EQ and boost.
        double worst=0;
        const std::vector<Controls> settings{
            [&]{auto c=neutral();c[0]=0;c[1]=15;c[2]=0;return c;}(),                    // low 35 Hz +15 wide
            [&]{auto c=neutral();c[3]=.5f;c[4]=-15;c[5]=2;return c;}(),                 // mid ~1.1 kHz -15 narrow
            [&]{auto c=neutral();c[6]=1;c[7]=10;c[8]=1;c[9]=30;c[10]=1;return c;}(),    // high 20 kHz (clamped) +10, boost 30
            [&]{auto c=neutral();c[1]=-6;c[4]=9;c[7]=-12;c[9]=12;c[10]=1;return c;}()}; // all bands, boost 12
        for(const auto& c:settings)for(const double hz:{35.,120.,500.,1118.,3000.,8000.,15000.}) {
            if(hz>rate*.4)continue;
            const double expected=eq.magnitudeDb(c,hz),measured=measuredDb(eq,c,rate,hz);
            worst=std::max(worst,std::abs(expected-measured));
        }
        std::cout<<"MEASURE Para EQ "<<rate<<" Hz: largest measured-vs-analytic gain error "<<worst<<" dB\n";
        require(worst<.05,"Measured Para EQ gain differs from its analytic response");
        // The bands do what they say: +15 dB at the low band's centre, -15 at the mid's.
        {auto c=neutral();c[0]=.5f;c[1]=15;require(std::abs(eq.magnitudeDb(c,PedalParaEQ::frequency(0,.5f))-15)<.01,"Low band centre gain");}
        {auto c=neutral();c[3]=.25f;c[4]=-15;c[5]=2;require(std::abs(eq.magnitudeDb(c,PedalParaEQ::frequency(1,.25f))+15)<.01,"Mid band centre gain");}
        {auto c=neutral();c[9]=30;c[10]=1;require(std::abs(eq.magnitudeDb(c,1000)-30)<1e-6,"Boost is not 30 dB");}
        // Narrower Q affects less of the neighbourhood.
        {auto w=neutral(),n=neutral();w[3]=n[3]=.5f;w[4]=n[4]=12;w[5]=0;n[5]=2;const double f=PedalParaEQ::frequency(1,.5f)*2;
         require(eq.magnitudeDb(w,f)>eq.magnitudeDb(n,f)+3,"Q choices do not change the bandwidth");}
    }
    std::cout<<"PASS Para EQ response: band ranges, neutral transparency, measured = analytic at 44.1/48/96 kHz, Q and boost\n";
}
void boardIntegration() {
    // Through the PRE board DSP in owner 2 at position 0: the model processes,
    // a bypassed slot does not.
    PedalBoardDSP board;board.prepare({48000,256,2});
    PedalBoardState state;state.enabled=true;state.instances[2]=defaultPedalInstance(paraEQ);state.order={2,0,1,3,4};
    state.instances[2].controls[9]=12;state.instances[2].controls[10]=1;   // boost 12 dB, EQ flat
    juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
    for(int block=0;block<200;++block){
        for(int n=0;n<256;++n,++t){const float x=.05f*float(std::sin(juce::MathConstants<double>::twoPi*440.*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);if(block>100)in+=double(x)*x;}
        board.process(b,state);if(block>100)for(int n=0;n<256;++n)out+=double(b.getSample(0,n))*b.getSample(0,n);
    }
    const double gain=10*std::log10(out/in);std::cout<<"MEASURE Para EQ boost through the PRE board: "<<gain<<" dB\n";
    require(std::abs(gain-12)<.05,"Para EQ in the PRE board does not apply its boost");
    // The processor's parameter cache delivers the model and its controls.
    auto p=std::make_unique<ChimeraProcessor>();p->setPedalModel(1,paraEQ);
    auto* mid=p->parameters().getParameter(pedalControlID(1,paraEQ,4));mid->setValueNotifyingHost(mid->convertTo0to1(-7.5f));
    const auto read=p->pedalBoardState();
    require(read.instances[1].model==paraEQ && std::abs(read.instances[1].controls[4]+7.5f)<.01f,"The processor does not read the Para EQ bank");
    std::cout<<"PASS Para EQ in the PRE board: boost measured through the board; processor reads its bank\n";
}
void oldProjects() {
    // A project saved without the Para EQ banks restores their defaults, also
    // into a processor whose Para EQ was edited.
    auto source=std::make_unique<ChimeraProcessor>();juce::MemoryBlock data;source->getStateInformation(data);
    auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));require(xml!=nullptr,"State is not binary XML");
    std::vector<juce::XmlElement*> remove;for(auto* child:xml->getChildIterator())if(expansionID(child->getStringAttribute("id")))remove.push_back(child);
    require(remove.size()==size_t(pedalExpansionParameterCount),"Fixture did not hold the expansion banks");
    for(auto* child:remove)xml->removeChildElement(child,true);
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
    auto target=std::make_unique<ChimeraProcessor>();auto* gain=target->parameters().getParameter(pedalControlID(0,paraEQ,1));gain->setValueNotifyingHost(gain->convertTo0to1(9.f));
    target->setStateInformation(legacy.getData(),int(legacy.getSize()));
    require(std::abs(gain->convertFrom0to1(gain->getValue()))<.01f,"An old project kept a Para EQ value from the session");
    auto* ratio=target->parameters().getParameter(pedalControlID(3,multiComp,0));
    require(std::abs(ratio->convertFrom0to1(ratio->getValue())-3.f)<.01f,"An old project did not restore the Multi Comp default");
    std::cout<<"PASS old projects restore the expansion defaults\n";
}
// ---- Multi Comp ----
Controls comp(float ratio,float gain,float sens,int mode,float balance=0){Controls c{};c[0]=ratio;c[1]=gain;c[2]=sens;c[3]=float(mode);c[4]=balance;return c;}
// Renders `seconds` of a sum of sines (hz, dBFS peak) and returns the output.
std::vector<float> renderTones(PedalMultiComp& mc,const Controls& c,const std::vector<std::pair<double,double>>& tones,double seconds,double rate=48000) {
    mc.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;
    for(int block=0;block<int(seconds*rate/256);++block){
        for(int n=0;n<256;++n,++t){double x=0;for(const auto& [hz,db]:tones)x+=juce::Decibels::decibelsToGain(db)*std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/rate);b.setSample(0,n,float(x));b.setSample(1,n,float(x));}
        mc.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
// Level (dBFS peak-equivalent) of one frequency over the last half of a render.
// A whole number of 0.1 s windows holds whole cycles of every test tone (all
// multiples of 10 Hz), so no tone leaks into another's bin.
double toneDb(const std::vector<float>& x,double hz,double rate=48000) {
    const size_t start=x.size()/2;double re=0,im=0;
    const size_t window=size_t(rate/10),length=(x.size()-start)/window*window;
    for(size_t i=start;i<start+length;++i){const double w=juce::MathConstants<double>::twoPi*hz*double(i)/rate;re+=x[i]*std::cos(w);im+=x[i]*std::sin(w);}
    return 20*std::log10(2*std::sqrt(re*re+im*im)/double(length)+1e-12);
}
void multiCompCurve() {
    const auto& m=pedalModel(multiComp);
    require(juce::String(m.id)=="pre.multicomp" && juce::String(m.name)=="Multi Comp" && m.controlCount==5 && pedalMenuEntries[multiComp].kind==PedalMenuKind::compressor,"Multi Comp catalog entry");
    // The peak detector (4 ms attack) settles about 0.5 dB below a 1 kHz
    // sine's peak, so above threshold the curve sits up to slope x 0.5 dB
    // high; below threshold it is exact, and the ratio (slope) is exact.
    PedalMultiComp mc;mc.prepare(48000);double worst=0,worstBelow=0,worstSlope=0;
    for(const float ratio:{1.f,2.5f,5.f})for(const float gain:{0.f,6.f}) {
        std::map<double,double> measured;
        for(const double in:{-40.,-24.,-18.,-12.,-3.}) {
            const auto c=comp(ratio,gain,-20,PedalMultiComp::normal);
            const double expected=in+PedalMultiComp::gainChangeDb(float(in),-20,ratio)+gain;measured[in]=toneDb(renderTones(mc,c,{{1000.,in}},.6),1000.);
            worst=std::max(worst,std::abs(measured[in]-expected));if(in<=-24)worstBelow=std::max(worstBelow,std::abs(measured[in]-expected));
        }
        worstSlope=std::max(worstSlope,std::abs((measured[-3.]-measured[-12.])/9.-1./ratio));
    }
    std::cout<<"MEASURE Multi Comp static curve (SENS -20, ratio 1/2.5/5, gain 0/6): below threshold "<<worstBelow<<" dB, overall "<<worst<<" dB, slope error "<<worstSlope<<"\n";
    require(worstBelow<.02 && worst<.5 && worstSlope<.01,"Multi Comp static curve differs from its gain computer");
    // Attack: GR reaches 80 % of its final value within 10 ms of a step.
    {mc.reset();const auto c=comp(5,0,-25,PedalMultiComp::normal);juce::AudioBuffer<float> b(2,1);double final=0;int at80=-1;std::vector<float> gr;
     for(int n=0;n<int(48000*.3);++n){const float x=n<4800?.01f*float(std::sin(.13*n)):.5f*float(std::sin(.13*n));b.setSample(0,0,x);b.setSample(1,0,x);mc.process(b,c);if(n>=4800)gr.push_back(mc.reduction());}
     final=gr.back();for(size_t i=0;i<gr.size();++i)if(gr[i]>=.8*final){at80=int(i);break;}
     const double ms=1000.*at80/48000;std::cout<<"MEASURE Multi Comp attack to 80 % of "<<final<<" dB GR: "<<ms<<" ms\n";
     require(at80>=0 && ms<10,"Multi Comp attack is slower than 10 ms to 80 %");}
    std::cout<<"PASS Multi Comp: static curve (ratio 1:1-5:1, makeup, SENS), 1:1 transparent, attack under 10 ms to 80 %\n";
}
void multiCompModes() {
    PedalMultiComp mc;mc.prepare(48000);
    // A loud 60 Hz note with a quiet 2 kHz overtone: Normal pulls the
    // overtone down with the note, Multiband leaves it alone.
    const std::vector<std::pair<double,double>> tones{{60.,-6.},{2000.,-36.}};
    const double normalHigh=toneDb(renderTones(mc,comp(5,0,-20,PedalMultiComp::normal),tones,.8),2000.);
    const double multiHigh=toneDb(renderTones(mc,comp(5,0,-20,PedalMultiComp::multiband),tones,.8),2000.);
    const double multiLow=toneDb(renderTones(mc,comp(5,0,-20,PedalMultiComp::multiband),tones,.8),60.);
    std::cout<<"MEASURE Multi Comp 2 kHz overtone: Normal "<<normalHigh<<" dB, Multiband "<<multiHigh<<" dB (input -36); 60 Hz Multiband "<<multiLow<<" dB (input -6)\n";
    require(multiHigh-normalHigh>8 && std::abs(multiHigh+36)<1. && multiLow<-6-8,"Multiband does not compress the bands independently");
    // Balance raises the treble band in Multiband (below threshold).
    const double lifted=toneDb(renderTones(mc,comp(5,0,6,PedalMultiComp::multiband,6),{{2000.,-30.}},.5),2000.);
    require(std::abs(lifted-(-30+6))<.3,"Balance does not lift the treble band by its amount");
    // TubeSim: unity at low level, added (mostly even) harmonics when loud.
    const auto h2=[&](int mode,double in){const auto x=renderTones(mc,comp(1,0,6,mode),{{200.,in}},.5);return toneDb(x,400.)-toneDb(x,200.);};
    const double tubeLoud=h2(PedalMultiComp::tubeSim,-3),normalLoud=h2(PedalMultiComp::normal,-3);
    const double tubeQuiet=toneDb(renderTones(mc,comp(1,0,6,PedalMultiComp::tubeSim),{{200.,-40.}},.5),200.);
    std::cout<<"MEASURE Multi Comp 2nd harmonic at -3 dBFS: TubeSim "<<tubeLoud<<" dBc, Normal "<<normalLoud<<" dBc; TubeSim at -40 dBFS "<<tubeQuiet<<" dB\n";
    require(tubeLoud>-40 && tubeLoud-normalLoud>30 && std::abs(tubeQuiet+40)<.1,"TubeSim does not add harmonics only when driven");
    // Through the board: the slot reports its gain reduction.
    PedalBoardDSP board;board.prepare({48000,256,2});PedalBoardState state;state.enabled=true;state.instances[0]=defaultPedalInstance(multiComp);
    juce::AudioBuffer<float> b(2,256);for(int k=0;k<100;++k){for(int n=0;n<256;++n){const float x=.5f*float(std::sin(.1*(k*256+n)));b.setSample(0,n,x);b.setSample(1,n,x);}board.process(b,state);}
    std::cout<<"MEASURE Multi Comp GR through the PRE board: "<<board.compressorReduction(0)<<" dB\n";
    require(board.compressorReduction(0)>3,"The PRE board does not report the Multi Comp's gain reduction");
    std::cout<<"PASS Multi Comp modes: Multiband independent bands and balance, TubeSim harmonics only when driven, board GR meter\n";
}
// ---- Tight Boost ----
std::vector<float> renderBoost(PedalTightBoost& tb,float knob,double hz,double db,double seconds=.5,double rate=48000) {
    tb.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;
    for(int block=0;block<int(seconds*rate/256);++block){
        for(int n=0;n<256;++n,++t){const float x=float(juce::Decibels::decibelsToGain(db)*std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/rate));b.setSample(0,n,x);b.setSample(1,n,x);}
        tb.process(b,knob);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
void tightBoostResponse() {
    const auto& m=pedalModel(tightBoost);
    require(juce::String(m.id)=="pre.tightboost" && juce::String(m.name)=="Tight Boost" && m.controlCount==1 && pedalMenuEntries[tightBoost].kind==PedalMenuKind::boost,"Tight Boost catalog entry");
    PedalTightBoost tb;tb.prepare(48000,256);double worst=0;
    for(const float knob:{0.f,11.f,22.f})for(const double hz:{60.,100.,200.,1000.,4000.}) {
        const double measured=toneDb(renderBoost(tb,knob,hz,-50),hz)+50,expected=tb.smallSignalDb(knob,hz);
        worst=std::max(worst,std::abs(measured-expected));
    }
    std::cout<<"MEASURE Tight Boost small-signal gain vs analytic: largest error "<<worst<<" dB (knob 0/11/22 dB, 60 Hz-4 kHz)\n";
    require(worst<.1,"Tight Boost small-signal gain differs from its curve");
    for(const float knob:{0.f,11.f,22.f})require(std::abs(tb.smallSignalDb(knob,1000.)-knob)<1e-6,"The knob's dB is not the gain at 1 kHz");
    const double tightOff=tb.smallSignalDb(0,80)-tb.smallSignalDb(0,1000),tightFull=tb.smallSignalDb(22,80)-tb.smallSignalDb(22,1000);
    std::cout<<"MEASURE Tight Boost 80 Hz relative to 1 kHz: "<<tightOff<<" dB at 0 dB, "<<tightFull<<" dB at 22 dB\n";
    require(tightFull<tightOff-6,"Turning the boost up does not tighten the low end");
    // Harmonics only when driven hot.
    const auto harmonics=[&](float knob,double in){const auto x=renderBoost(tb,knob,1000.,in);return std::max(toneDb(x,2000.),toneDb(x,3000.))-toneDb(x,1000.);};
    const double clean=harmonics(0,-20),hot=harmonics(22,-20);
    std::cout<<"MEASURE Tight Boost largest of 2nd/3rd harmonic at -20 dBFS in: "<<clean<<" dBc at 0 dB, "<<hot<<" dBc at 22 dB\n";
    require(clean<-80 && hot>-40,"Tight Boost harmonics are not added only when driven");
    // Same oversampling latency as the existing drive stage, so the slot's
    // fixed latency (and every reported latency) is unchanged.
    PedalNewDrive drive;drive.prepare({48000,256,2});
    require(tb.latency()==drive.latency(),"Tight Boost changes the PRE slot latency");
    std::cout<<"PASS Tight Boost: knob = gain at 1 kHz, measured = analytic, low end tightens, harmonics only when hot, slot latency unchanged\n";
}
// ---- Power Wah ----
Controls wah(float position,float boostDb,bool wahOn,bool boostOn){Controls c{};c[0]=position;c[1]=boostDb;c[2]=wahOn?1.f:0.f;c[3]=boostOn?1.f:0.f;return c;}
double wahGainDb(PedalPowerWah& w,const Controls& c,double hz,double rate=48000) {
    w.reset();juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
    for(int block=0;block<int(rate*.5/256);++block){
        for(int n=0;n<256;++n,++t){const float x=.01f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/rate));b.setSample(0,n,x);b.setSample(1,n,x);if(block>int(rate*.25/256))in+=double(x)*x;}
        w.process(b,c);if(block>int(rate*.25/256))for(int n=0;n<256;++n)out+=double(b.getSample(0,n))*b.getSample(0,n);
    }
    return 10*std::log10(out/in);
}
void powerWahResponse() {
    const auto& m=pedalModel(powerWah);
    require(juce::String(m.id)=="pre.powerwah" && juce::String(m.name)=="Power Wah" && m.controlCount==4 && pedalMenuEntries[powerWah].kind==PedalMenuKind::wah,"Power Wah catalog entry");
    PedalPowerWah w;w.prepare(48000);
    // The measured peak of the response sits at the treadle's centre; the
    // sweep is monotonic from 350 Hz to 2.2 kHz.
    double previous=0,worstPeak=0,worstGain=0;
    for(const float position:{0.f,.25f,.5f,.75f,1.f}) {
        const auto c=wah(position,0,true,false);double bestHz=0,best=-1e9;
        for(double hz=250;hz<3200;hz*=1.01){const double g=w.magnitudeDb(c,hz);if(g>best){best=g;bestHz=hz;}}
        const double target=PedalPowerWah::centre(position);
        worstPeak=std::max(worstPeak,std::abs(bestHz/target-1));
        require(bestHz>previous,"The wah sweep is not monotonic");previous=bestHz;
        for(const double hz:{target*.5,target,target*2})worstGain=std::max(worstGain,std::abs(wahGainDb(w,c,hz)-w.magnitudeDb(c,hz)));
    }
    std::cout<<"MEASURE Power Wah: analytic peak within "<<100*worstPeak<<" % of the treadle centre (350 Hz-2.2 kHz); measured vs analytic gain "<<worstGain<<" dB\n";
    require(worstPeak<.011 && worstGain<.05,"Power Wah response differs from its design");
    // Boost alone is exact; wah off is transparent; both combine.
    require(std::abs(wahGainDb(w,wah(.5f,0,false,false),1000.))<1e-3,"Wah off, boost off is not transparent");
    require(std::abs(wahGainDb(w,wah(.5f,40,false,true),1000.)-40)<1e-3,"Boost alone is not 40 dB");
    {const auto c=wah(.5f,12,true,true);require(std::abs(wahGainDb(w,c,PedalPowerWah::centre(.5f))-12)<.05,"Wah and boost do not combine");}
    // Treadle lag: a jump reaches 63 % of its log-frequency change in ~25 ms.
    {w.reset();juce::AudioBuffer<float> b(2,1);auto c=wah(0,0,true,false);w.process(b,c);c[0]=1;int n63=-1;
     const double from=std::log(PedalPowerWah::centre(0)),to=std::log(PedalPowerWah::centre(1));
     for(int n=0;n<4800 && n63<0;++n){w.process(b,c);if((std::log(w.currentCentre())-from)/(to-from)>=.632)n63=n+1;}
     const double ms=1000.*n63/48000;std::cout<<"MEASURE Power Wah treadle lag to 63 %: "<<ms<<" ms\n";require(std::abs(ms-25)<1,"Treadle lag is not about 25 ms");}
    std::cout<<"PASS Power Wah: sweep 350 Hz-2.2 kHz at the treadle centre, measured = analytic, boost exact, wah off transparent, 25 ms treadle lag\n";
}
// ---- Bass Synth ----
Controls synth(float mix,float sub,float cutoff,float resonance,float envelope,float mod,int shape,bool voice){Controls c{};c[0]=mix;c[1]=sub;c[2]=cutoff;c[3]=resonance;c[4]=envelope;c[5]=mod;c[6]=float(shape);c[7]=voice?1.f:0.f;return c;}
// A steady bass-like note (falling harmonics, h2 weight selectable), then
// `silence` seconds of nothing. Returns the output.
std::vector<float> renderNote(PedalBassSynth& bs,const Controls& c,double hz,double seconds,double h2=.5,double silence=0,double rate=48000) {
    bs.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;const long playing=long(seconds*rate);
    for(int block=0;block<int((seconds+silence)*rate/256);++block){
        for(int n=0;n<256;++n,++t){
            double x=0;if(t<playing){const double w=juce::MathConstants<double>::twoPi*hz*double(t)/rate;x=.18*(std::sin(w)+h2*std::sin(2*w)+.33*std::sin(3*w)+.25*std::sin(4*w));}
            b.setSample(0,n,float(x));b.setSample(1,n,float(x));
        }
        bs.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
void bassSynthResponse() {
    const auto& m=pedalModel(bassSynth);
    require(juce::String(m.id)=="pre.basssynth" && juce::String(m.name)=="Bass Synth" && m.controlCount==8 && pedalMenuEntries[bassSynth].kind==PedalMenuKind::pitch,"Bass Synth catalog entry");
    PedalBassSynth bs;bs.prepare(48000);
    // Tracking, including a note whose 2nd harmonic is stronger than usual.
    double worstTrack=0;
    for(const double hz:{40.,50.,80.,110.,200.})for(const double h2:{.5,.9}) {
        renderNote(bs,synth(1,0,1,0,0,0,PedalBassSynth::saw,false),hz,.4,h2);
        worstTrack=std::max(worstTrack,std::abs(bs.trackedHz()/hz-1));
    }
    std::cout<<"MEASURE Bass Synth tracking 40-200 Hz (2nd harmonic 0.5 and 0.9): largest error "<<100*worstTrack<<" %\n";
    require(worstTrack<.01,"Bass Synth does not track the note within 1 %");
    // SHAPE: square has (almost) no even harmonics, saw does; triangle's
    // upper harmonics fall faster than square's.
    const auto rel=[&](const std::vector<float>& x,double f,double k){return toneDb(x,k*f)-toneDb(x,f);};
    const auto open=[&](int shape,bool voice,float sub=0,float cutoff=1){return renderNote(bs,synth(1,sub,cutoff,0,0,0,shape,voice),100.,.6);};
    const auto sq=open(PedalBassSynth::square,false),sw=open(PedalBassSynth::saw,false),tr=open(PedalBassSynth::triangle,false);
    std::cout<<"MEASURE Bass Synth 2nd/3rd harmonic re fundamental: square "<<rel(sq,100,2)<<" / "<<rel(sq,100,3)<<", saw "<<rel(sw,100,2)<<" / "<<rel(sw,100,3)<<", triangle - / "<<rel(tr,100,3)<<" dB\n";
    require(rel(sq,100,2)<-30 && rel(sw,100,2)>-9 && rel(tr,100,3)<rel(sq,100,3)-6,"SHAPE does not give its waveform's harmonics");
    // SUB adds the octave below; VOICE adds the octave and fifth above.
    const auto subbed=open(PedalBassSynth::square,false,1);
    std::cout<<"MEASURE Bass Synth 50 Hz (sub) re 100 Hz: sub 0 "<<rel(sq,100,.5)<<", sub 1 "<<rel(subbed,100,.5)<<" dB\n";
    require(rel(subbed,100,.5)-rel(sq,100,.5)>20,"SUB MIX does not add the octave below");
    const auto voiced=open(PedalBassSynth::square,true);
    require(rel(voiced,100,2)-rel(sq,100,2)>20 && rel(voiced,100,1.5)>-25,"VOICE does not add the octave and fifth above");
    // CUTOFF closes the filter on the upper harmonics.
    const auto dark=open(PedalBassSynth::saw,false,0,0);
    std::cout<<"MEASURE Bass Synth 3rd harmonic re fundamental: cutoff open "<<rel(sw,100,3)<<", closed "<<rel(dark,100,3)<<" dB\n";
    require(rel(dark,100,3)<rel(sw,100,3)-15,"CUTOFF does not darken the synth");
    // The synth stops when the note stops.
    // The synth follows the input level down (80 ms release), then the gate
    // closes below -50 dBFS and the tracker lets go.
    {const auto x=renderNote(bs,synth(1,1,1,0,0,0,PedalBassSynth::saw,true),80.,.4,.5,.8);
     size_t silent=x.size();while(silent>0 && x[silent-1]==0.f)--silent;
     const double ms=1000.*(double(silent)-48000*.4)/48000;
     std::cout<<"MEASURE Bass Synth after the note stops: exactly silent "<<ms<<" ms later\n";
     require(ms<600 && bs.trackedHz()==0,"Bass Synth keeps sounding after the note stops");}
    std::cout<<"PASS Bass Synth: tracking within 1 % (no octave error), SHAPE harmonics, SUB, VOICE, CUTOFF, stops with the note\n";
}
// ---- Dreadful Peril ----
Controls peril(float gain,float weight,float output,float low,float mid,float high,float gate,float midFreq,int mode,bool gateOn)
    {Controls c{};c[0]=gain;c[1]=weight;c[2]=output;c[3]=low;c[4]=mid;c[5]=high;c[6]=gate;c[7]=midFreq;c[8]=float(mode);c[9]=gateOn?1.f:0.f;return c;}
std::vector<float> renderPeril(PedalDreadfulPeril& h,const Controls& c,double hz,double db,double seconds,double silence=0,double rate=48000) {
    h.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;const long playing=long(seconds*rate);
    for(int block=0;block<int((seconds+silence)*rate/256);++block){
        for(int n=0;n<256;++n,++t){const float x=t<playing?float(juce::Decibels::decibelsToGain(db)*std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/rate)):0.f;b.setSample(0,n,x);b.setSample(1,n,x);}
        h.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
double harmonicsDb(const std::vector<float>& x,double hz){double e=0;for(int k=2;k<=5;++k)e+=std::pow(10.,toneDb(x,k*hz)/10.);return 10*std::log10(e+1e-30)-toneDb(x,hz);}
void dreadfulPerilResponse() {
    const auto& m=pedalModel(dreadfulPeril);
    require(juce::String(m.id)=="pre.dreadfulperil" && juce::String(m.name)=="Dreadful Peril" && m.controlCount==10 && pedalMenuEntries[dreadfulPeril].kind==PedalMenuKind::distortion,"Dreadful Peril catalog entry");
    PedalDreadfulPeril h;h.prepare(48000,256);
    // Small signal (LITE, gate off): measured = the filters, drive and stage slope.
    double worst=0;
    for(const auto& c:{peril(0,.5f,-12,0,0,0,-60,.5f,0,false),peril(.3f,0,-6,9,-9,6,-60,0,0,false),peril(.1f,1,-18,-6,12,-12,-60,1,0,false)})
        for(const double hz:{60.,100.,300.,700.,1000.,3000.,8000.})
            worst=std::max(worst,std::abs(toneDb(renderPeril(h,c,hz,-70,.4),hz)+70-h.smallSignalDb(c,hz)));
    std::cout<<"MEASURE Dreadful Peril small-signal gain vs analytic (LITE, EQ/WEIGHT/MID FREQ/OUTPUT settings, 60 Hz-8 kHz): largest error "<<worst<<" dB\n";
    require(worst<.1,"Dreadful Peril small-signal response differs from its design");
    {const auto light=peril(0,0,0,0,0,0,-60,.5f,0,false),heavyWeight=peril(0,1,0,0,0,0,-60,.5f,0,false);
     require(h.smallSignalDb(heavyWeight,60)-h.smallSignalDb(light,60)>10,"WEIGHT does not let more low end into the distortion");}
    // Distortion grows with GAIN and with the mode.
    const auto distortion=[&](float gain,int mode){return harmonicsDb(renderPeril(h,peril(gain,.5f,-12,0,0,0,-60,.5f,mode,false),200.,-20,.5),200.);};
    const double liteLow=distortion(.1f,0),lite=distortion(.6f,0),heavyMode=distortion(.6f,1),heavier=distortion(.6f,2);
    std::cout<<"MEASURE Dreadful Peril harmonics 2-5 re fundamental (-20 dBFS 200 Hz): LITE gain 0.1 "<<liteLow<<", LITE 0.6 "<<lite<<", HEAVY "<<heavyMode<<", HEAVIER "<<heavier<<" dB\n";
    require(lite>liteLow+6 && heavyMode>lite && heavier>heavyMode,"Distortion does not grow with GAIN and mode");
    // Gate: noise below the threshold is held back, playing opens it, it
    // closes after the note, and GATE OFF passes everything.
    {const double rms=[&]{const auto x=renderPeril(h,peril(.8f,.5f,-12,0,0,0,-60,.5f,1,true),300.,-75,.5);double e=0;for(size_t i=x.size()/2;i<x.size();++i)e+=double(x[i])*x[i];return 10*std::log10(e/double(x.size()/2)+1e-30);}();
     const double open=[&]{const auto x=renderPeril(h,peril(.8f,.5f,-12,0,0,0,-60,.5f,1,false),300.,-75,.5);double e=0;for(size_t i=x.size()/2;i<x.size();++i)e+=double(x[i])*x[i];return 10*std::log10(e/double(x.size()/2)+1e-30);}();
     std::cout<<"MEASURE Dreadful Peril -75 dBFS hum with the gate at -60 dB: "<<rms<<" dBFS (gate off: "<<open<<" dBFS)\n";
     require(open-rms>60,"The gate does not hold back sub-threshold noise");}
    {const auto x=renderPeril(h,peril(.8f,.5f,-12,0,0,0,-60,.5f,1,true),300.,-20,.3,.3);
     double playing=0;for(size_t i=size_t(48000*.2);i<size_t(48000*.3);++i)playing=std::max(playing,double(std::abs(x[i])));
     float tail=0;for(size_t i=x.size()-size_t(48000*.1);i<x.size();++i)tail=std::max(tail,std::abs(x[i]));
     std::cout<<"MEASURE Dreadful Peril gate: playing peak "<<juce::Decibels::gainToDecibels(playing)<<" dBFS, 200-300 ms after the note "<<juce::Decibels::gainToDecibels(tail,-200.f)<<" dBFS\n";
     require(playing>.05 && tail<1e-5f,"The gate does not pass playing and close after the note");}
    PedalNewDrive drive;drive.prepare({48000,256,2});
    require(h.latency()==drive.latency(),"Dreadful Peril changes the PRE slot latency");
    std::cout<<"PASS Dreadful Peril: small-signal = design, WEIGHT, distortion grows with GAIN and LITE/HEAVY/HEAVIER, gate holds noise and closes after notes, slot latency unchanged\n";
}
// ---- Seven EQ (PRE Expansion B) ----
void sevenEQResponse() {
    const auto& m=pedalModel(sevenEQ);
    require(juce::String(m.id)=="pre.seveneq" && juce::String(m.name)=="Seven EQ" && m.controlCount==8 && pedalMenuEntries[sevenEQ].kind==PedalMenuKind::equalizer,"Seven EQ catalog entry");
    PedalSevenEQ eq;eq.prepare(48000);
    const auto measured=[&](const Controls& c,double hz){
        eq.reset();juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
        for(int block=0;block<int(48000*.5/256);++block){
            for(int n=0;n<256;++n,++t){const float x=.05f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);if(block>int(48000*.25/256))in+=double(x)*x;}
            eq.process(b,c);if(block>int(48000*.25/256))for(int n=0;n<256;++n)out+=double(b.getSample(0,n))*b.getSample(0,n);
        }
        return 10*std::log10(out/in);};
    // Flat is transparent; each slider gives +-15 dB at its band (the 6.4 kHz
    // shelf at its plateau); LEVEL is exact; measured = analytic.
    Controls flat{};require(std::abs(measured(flat,1000.))<1e-4,"Flat Seven EQ is not transparent");
    double worstCentre=0,worst=0;
    for(int b=0;b<7;++b)for(const float g:{15.f,-15.f}) {
        Controls c{};c[size_t(b)]=g;
        const double at=b<6?PedalSevenEQ::centres[size_t(b)]:16000.;
        worstCentre=std::max(worstCentre,std::abs(eq.magnitudeDb(c,at)-g)*(b<6?1.:0.)+(b==6?std::max(0.,std::abs(g)-std::abs(eq.magnitudeDb(c,at))-1.):0.));
        worst=std::max(worst,std::abs(measured(c,at>12000?8000.:at)-eq.magnitudeDb(c,at>12000?8000.:at)));
    }
    {Controls c{};c[7]=-15;require(std::abs(measured(c,500.)+15)<1e-3,"LEVEL is not exact");}
    {Controls c{};c[0]=6;c[2]=-9;c[3]=12;c[5]=-4;c[6]=8;c[7]=3;for(const double hz:{80.,300.,800.,2500.,7000.})worst=std::max(worst,std::abs(measured(c,hz)-eq.magnitudeDb(c,hz)));}
    std::cout<<"MEASURE Seven EQ: centre gain error "<<worstCentre<<" dB (shelf within 1 dB of 15 at 16 kHz), measured vs analytic "<<worst<<" dB\n";
    require(worstCentre<1e-6 && worst<.02,"Seven EQ response differs from its design");
    std::cout<<"PASS Seven EQ: flat transparent, +-15 dB at each band (6.4 kHz shelf), LEVEL exact, measured = analytic\n";
}
// ---- Multi Wah (PRE Expansion B) ----
Controls multi(float position,int range,float q,float boostDb,bool boostOn){Controls c{};c[0]=position;c[1]=float(range);c[2]=q;c[3]=boostDb;c[4]=boostOn?1.f:0.f;return c;}
void multiWahResponse() {
    const auto& m=pedalModel(multiWah);
    require(juce::String(m.id)=="pre.multiwah" && juce::String(m.name)=="Multi Wah" && m.controlCount==5 && pedalMenuEntries[multiWah].kind==PedalMenuKind::wah,"Multi Wah catalog entry");
    PedalMultiWah w;w.prepare(48000);
    const auto measured=[&](const Controls& c,double hz){
        w.reset();juce::AudioBuffer<float> b(2,256);double in=0,out=0;long t=0;
        for(int block=0;block<int(48000*.5/256);++block){
            for(int n=0;n<256;++n,++t){const float x=.01f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);if(block>int(48000*.25/256))in+=double(x)*x;}
            w.process(b,c);if(block>int(48000*.25/256))for(int n=0;n<256;++n)out+=double(b.getSample(0,n))*b.getSample(0,n);
        }
        return 10*std::log10(out/in);};
    // Each RANGE sweeps its published span: the response peak sits at the
    // range's low end (heel) and high end (toe).
    double worstEdge=0,worst=0;
    for(int range=0;range<6;++range)for(const float position:{0.f,1.f}) {
        const auto c=multi(position,range,1,0,false);double bestHz=0,best=-1e9;
        for(double hz=150;hz<3000;hz*=1.002){const double g=w.magnitudeDb(c,hz);if(g>best){best=g;bestHz=hz;}}
        const auto& r=PedalMultiWah::ranges[size_t(range)];
        worstEdge=std::max(worstEdge,std::abs(bestHz/(position==0?r.low:r.high)-1));
        worst=std::max(worst,std::abs(measured(c,bestHz)-w.magnitudeDb(c,bestHz)));
    }
    // 15 dB filter gain at maximum Q (boost off), less at minimum Q; Q narrows.
    const double peakMax=w.magnitudeDb(multi(.5f,2,1,0,false),PedalMultiWah::centre(2,.5f)),peakMin=w.magnitudeDb(multi(.5f,2,0,0,false),PedalMultiWah::centre(2,.5f));
    const double octaveMax=w.magnitudeDb(multi(.5f,2,1,0,false),2*PedalMultiWah::centre(2,.5f))-peakMax,octaveMin=w.magnitudeDb(multi(.5f,2,0,0,false),2*PedalMultiWah::centre(2,.5f))-peakMin;
    std::cout<<"MEASURE Multi Wah: peak within "<<100*worstEdge<<" % of each RANGE's published ends; measured vs analytic "<<worst<<" dB; peak "<<peakMax<<" dB at max Q, "<<peakMin<<" dB at min Q; one octave up "<<octaveMax<<" / "<<octaveMin<<" dB\n";
    require(worstEdge<.005 && worst<.05 && std::abs(peakMax-15)<1e-6 && peakMin<peakMax && octaveMax<octaveMin-6,"Multi Wah ranges, filter gain or Q differ from the design");
    require(std::abs(measured(multi(.5f,2,0,16,true),PedalMultiWah::centre(2,.5f))-(peakMin+16))<.05,"BOOST does not add its 16 dB");
    std::cout<<"PASS Multi Wah: six published sweep ranges, 15 dB at max Q, Q narrows, boost up to 16 dB, measured = analytic\n";
}
// ---- Octave Fuzz (PRE Expansion B) ----
Controls octave(float volume,float fuzz,float tone,float sub,float up,bool subIntoFuzz,bool upOn,float upGain,float upTone)
    {Controls c{};c[0]=volume;c[1]=fuzz;c[2]=tone;c[3]=sub;c[4]=up;c[5]=subIntoFuzz?1.f:0.f;c[6]=upOn?1.f:0.f;c[7]=upGain;c[8]=upTone;return c;}
std::vector<float> renderOctave(PedalOctaveFuzz& f,const Controls& c,double hz,double db,double seconds=.6) {
    f.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;
    for(int block=0;block<int(seconds*48000/256);++block){
        for(int n=0;n<256;++n,++t){const float x=float(juce::Decibels::decibelsToGain(db)*std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);}
        f.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
void octaveFuzzResponse() {
    const auto& m=pedalModel(octaveFuzz);
    require(juce::String(m.id)=="pre.octavefuzz" && juce::String(m.name)=="Octave Fuzz" && m.controlCount==9 && pedalMenuEntries[octaveFuzz].kind==PedalMenuKind::fuzz,"Octave Fuzz catalog entry");
    PedalOctaveFuzz f;f.prepare(48000,256);
    // Direct signal (SUB INTO FUZZ off, no octaves): VOLUME sets it, at most +1.6 dB.
    const double dryMax=toneDb(renderOctave(f,octave(1.6f,.5f,.5f,0,0,false,false,10,.5f),1000.,-20),1000.)+20;
    const double dryMin=toneDb(renderOctave(f,octave(-60,.5f,.5f,0,0,false,false,10,.5f),1000.,-20),1000.)+20;
    std::cout<<"MEASURE Octave Fuzz direct signal: VOLUME max "<<dryMax<<" dB, min "<<dryMin<<" dB\n";
    require(std::abs(dryMax-1.6)<.05 && std::abs(dryMin+60)<.1,"VOLUME does not set the direct signal up to +1.6 dB");
    // Sub octave: half the input frequency, low-passed at 149 Hz.
    {const auto x=renderOctave(f,octave(-60,.5f,.5f,1,0,false,false,10,.5f),220.,-12);
     const double half=toneDb(x,110.),input=toneDb(x,220.);
     const double low=toneDb(renderOctave(f,octave(-60,.5f,.5f,1,0,false,false,10,.5f),120.,-12),60.),high=toneDb(renderOctave(f,octave(-60,.5f,.5f,1,0,false,false,10,.5f),880.,-12),440.);
     std::cout<<"MEASURE Octave Fuzz sub (-12 dBFS): 220 Hz in -> 110 Hz "<<half<<" dBFS, 220 Hz "<<input<<" dBFS; sub of 120 Hz "<<low<<", of 880 Hz "<<high<<" dBFS\n";
     require(half>input+30 && half>-30,"SUB OCTAVE is not one octave down");
     require(low>high+12,"The sub voice is not low-passed");}
    // Octave up: twice the input frequency, OCTAVE UP ON switches it, the
    // GAIN trim spans 20 dB (small signal).
    {const auto on=renderOctave(f,octave(-60,.5f,.5f,0,1,false,true,10,1),440.,-12),off=renderOctave(f,octave(-60,.5f,.5f,0,1,false,false,10,1),440.,-12);
     const double gainSpan=toneDb(renderOctave(f,octave(-60,.5f,.5f,0,1,false,true,20,1),440.,-80),880.)-toneDb(renderOctave(f,octave(-60,.5f,.5f,0,1,false,true,0,1),440.,-80),880.);
     std::cout<<"MEASURE Octave Fuzz up (440 Hz -12 dBFS): 880 Hz "<<toneDb(on,880.)<<" dBFS, 440 Hz "<<toneDb(on,440.)<<" dBFS; switched off 880 Hz "<<toneDb(off,880.)<<" dBFS; GAIN trim span "<<gainSpan<<" dB\n";
     require(toneDb(on,880.)>toneDb(on,440.)+25 && toneDb(off,880.)<toneDb(on,880.)-60 && std::abs(gainSpan-20)<.2,"OCTAVE UP, its switch or its GAIN trim differ from the design");}
    // Fuzz: a 40 dB gain range (small signal), it saturates, TONE brightens.
    {const auto small=[&](float fuzz,float tone,double hz){return toneDb(renderOctave(f,octave(0,fuzz,tone,0,0,true,false,10,.5f),hz,-90),hz)+90;};
     const double span=small(1,.5f,500.)-small(0,.5f,500.),bright=small(.5f,1,3000.)-small(.5f,0,3000.);
     const double soft=harmonicsDb(renderOctave(f,octave(0,0,.5f,0,0,true,false,10,.5f),200.,-40),200.),hard=harmonicsDb(renderOctave(f,octave(0,1,.5f,0,0,true,false,10,.5f),200.,-20),200.);
     std::cout<<"MEASURE Octave Fuzz: FUZZ range "<<span<<" dB (small signal), TONE at 3 kHz "<<bright<<" dB, harmonics 2-5 re fundamental "<<soft<<" (FUZZ 0, -40 dBFS) / "<<hard<<" dB (FUZZ max, -20 dBFS)\n";
     require(std::abs(span-PedalOctaveFuzz::fuzzRangeDb)<.1 && bright>15 && hard>soft+20 && hard>-10,"FUZZ range, saturation or TONE differ from the design");}
    // SUB INTO FUZZ puts the sub voice through the fuzz.
    {const double with=toneDb(renderOctave(f,octave(0,.5f,1,1,0,true,false,10,.5f),220.,-12),110.),without=toneDb(renderOctave(f,octave(0,.5f,1,0,0,true,false,10,.5f),220.,-12),110.);
     std::cout<<"MEASURE Octave Fuzz SUB INTO FUZZ: 110 Hz with the sub "<<with<<" dBFS, without "<<without<<" dBFS\n";
     require(with>without+30,"SUB INTO FUZZ does not drive the sub through the fuzz");}
    PedalNewDrive drive;drive.prepare({48000,256,2});
    require(f.latency()==drive.latency(),"Octave Fuzz changes the PRE slot latency");
    std::cout<<"PASS Octave Fuzz: VOLUME to +1.6 dB, sub one octave down low-passed, octave up and its switch and 20 dB GAIN trim, 40 dB FUZZ range, TONE, SUB INTO FUZZ, slot latency unchanged\n";
}
// ---- Violet Rage (PRE Expansion B) ----
Controls violet(float volume,float gain,float bass,float middle,float treble,int aggression)
    {Controls c{};c[0]=volume;c[1]=gain;c[2]=bass;c[3]=middle;c[4]=treble;c[5]=float(aggression);return c;}
std::vector<float> renderViolet(PedalVioletRage& v,const Controls& c,double hz,double db,double seconds=.5) {
    v.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;
    for(int block=0;block<int(seconds*48000/256);++block){
        for(int n=0;n<256;++n,++t){const float x=float(juce::Decibels::decibelsToGain(db)*std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);}
        v.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
void violetRageResponse() {
    const auto& m=pedalModel(violetRage);
    require(juce::String(m.id)=="pre.violetrage" && juce::String(m.name)=="Violet Rage" && m.controlCount==6 && pedalMenuEntries[violetRage].kind==PedalMenuKind::distortion,"Violet Rage catalog entry");
    PedalVioletRage v;v.prepare(48000,256);
    const auto small=[&](const Controls& c,double hz){return toneDb(renderViolet(v,c,hz,-90),hz)+90;};
    // BASS, MIDDLE and TREBLE after the clipping: measured EQ = the tone stack (small signal).
    double worst=0;
    for(const auto& c:{violet(-12,.5f,12,0,0,0),violet(-12,.5f,0,-12,0,0),violet(-12,.5f,-6,9,12,0),violet(-12,.5f,9,-6,-12,0)})
        for(const double hz:{80.,200.,700.,1500.,3000.,6000.})
            worst=std::max(worst,std::abs(small(c,hz)-small(violet(-12,.5f,0,0,0,0),hz)-v.toneStackDb(c,hz)));
    std::cout<<"MEASURE Violet Rage tone stack measured vs analytic (small signal, 80 Hz-6 kHz): largest error "<<worst<<" dB\n";
    require(worst<.05,"Violet Rage tone stack differs from its design");
    // GAIN range and AGGRESSION (small signal, so drive adds 1:1); VOLUME exact.
    const double gainSpan=small(violet(-12,1,0,0,0,0),1000.)-small(violet(-12,0,0,0,0,0),1000.);
    const double redStep=small(violet(-12,0,0,0,0,1),1000.)-small(violet(-12,0,0,0,0,0),1000.),blueStep=small(violet(-12,0,0,0,0,2),1000.)-small(violet(-12,0,0,0,0,0),1000.);
    const double volume=small(violet(6,.5f,0,0,0,0),1000.)-small(violet(-24,.5f,0,0,0,0),1000.);
    std::cout<<"MEASURE Violet Rage small-signal: GAIN span "<<gainSpan<<" dB, AGGRESSION red +"<<redStep<<" dB, blue +"<<blueStep<<" dB, VOLUME span "<<volume<<" dB\n";
    require(std::abs(gainSpan-34)<.1 && std::abs(redStep-8)<.1 && std::abs(blueStep-16)<.1 && std::abs(volume-30)<.05,"GAIN, AGGRESSION or VOLUME differ from the design");
    // Low end tightened before the clipping; distortion grows with GAIN and AGGRESSION.
    {const double tight=small(violet(-12,.5f,0,0,0,0),40.)-small(violet(-12,.5f,0,0,0,0),400.);
     std::cout<<"MEASURE Violet Rage 40 Hz re 400 Hz (small signal): "<<tight<<" dB\n";
     require(tight<-15,"Violet Rage does not tighten the low end");}
    const auto distortion=[&](float gain,int aggression){return harmonicsDb(renderViolet(v,violet(-12,gain,0,0,0,aggression),200.,-30),200.);};
    const double low=distortion(0,0),mid=distortion(.5f,0),red=distortion(.5f,1),blue=distortion(.5f,2);
    std::cout<<"MEASURE Violet Rage harmonics 2-5 re fundamental (-30 dBFS 200 Hz): gain 0 "<<low<<", gain 0.5 "<<mid<<", red "<<red<<", blue "<<blue<<" dB\n";
    require(mid>low+6 && red>mid && blue>red,"Distortion does not grow with GAIN and AGGRESSION");
    PedalNewDrive drive;drive.prepare({48000,256,2});
    require(v.latency()==drive.latency(),"Violet Rage changes the PRE slot latency");
    std::cout<<"PASS Violet Rage: tone stack = design, 34 dB GAIN, AGGRESSION red/blue, VOLUME exact, tight low end, distortion grows, slot latency unchanged\n";
}
// ---- Fire Drake (PRE Expansion B) ----
std::vector<float> renderDrake(PedalFireDrake& d,const Controls& c,double hz,double db,double seconds=.5) {
    d.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;
    for(int block=0;block<int(seconds*48000/256);++block){
        for(int n=0;n<256;++n,++t){const float x=float(juce::Decibels::decibelsToGain(db)*std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);}
        d.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
void fireDrakeResponse() {
    const auto& m=pedalModel(fireDrake);
    require(juce::String(m.id)=="pre.firedrake" && juce::String(m.name)=="Fire Drake" && m.controlCount==6 && pedalMenuEntries[fireDrake].kind==PedalMenuKind::distortion,"Fire Drake catalog entry");
    PedalFireDrake d;d.prepare(48000,256);
    // Same control layout as Violet Rage: volume, gain, bass, mids, treble, mode.
    const auto small=[&](const Controls& c,double hz){return toneDb(renderDrake(d,c,hz,-90),hz)+90;};
    double worst=0;
    for(const int mode:{0,1})
        for(const auto& c:{violet(-12,.5f,12,0,0,mode),violet(-12,.5f,0,-12,0,mode),violet(-12,.5f,-6,9,12,mode),violet(-12,.5f,9,-6,-12,mode)})
            for(const double hz:{200.,800.,1500.,3000.,6000.})
                worst=std::max(worst,std::abs(small(c,hz)-small(violet(-12,.5f,0,0,0,mode),hz)-d.eqDb(c,hz)));
    std::cout<<"MEASURE Fire Drake active EQ measured vs analytic (small signal, both modes, 200 Hz-6 kHz): largest error "<<worst<<" dB\n";
    require(worst<.05,"Fire Drake EQ differs from its design");
    const double gainSpan=small(violet(-12,1,0,0,0,0),1000.)-small(violet(-12,0,0,0,0,0),1000.),volume=small(violet(6,.5f,0,0,0,0),1000.)-small(violet(-24,.5f,0,0,0,0),1000.);
    const double tightLow=small(violet(-12,.5f,0,0,0,1),60.)-small(violet(-12,.5f,0,0,0,0),60.);
    std::cout<<"MEASURE Fire Drake small-signal: GAIN span "<<gainSpan<<" dB, VOLUME span "<<volume<<" dB, TIGHT vs OPEN at 60 Hz "<<tightLow<<" dB\n";
    require(std::abs(gainSpan-36)<.1 && std::abs(volume-30)<.05 && tightLow<-6,"GAIN, VOLUME or the TIGHT low cut differ from the design");
    // TIGHT compresses more: a 30 dB louder input gains less output.
    const auto swing=[&](int mode){return toneDb(renderDrake(d,violet(-12,.3f,0,0,0,mode),500.,-10),500.)-toneDb(renderDrake(d,violet(-12,.3f,0,0,0,mode),500.,-40),500.);};
    const double openSwing=swing(0),tightSwing=swing(1);
    std::cout<<"MEASURE Fire Drake output change for +30 dB input (500 Hz, GAIN 0.3): OPEN "<<openSwing<<" dB, TIGHT "<<tightSwing<<" dB\n";
    require(tightSwing<openSwing-3,"TIGHT does not compress more than OPEN");
    PedalNewDrive drive;drive.prepare({48000,256,2});
    require(d.latency()==drive.latency(),"Fire Drake changes the PRE slot latency");
    std::cout<<"PASS Fire Drake: active EQ = design in both modes, 36 dB GAIN, VOLUME exact, TIGHT cuts lows and compresses more, slot latency unchanged\n";
}
// ---- Pitch Bend (PRE Expansion B) ----
Controls bendControls(float position,int setting,int mode){Controls c{};c[0]=position;c[1]=float(setting);c[2]=float(mode);return c;}
std::vector<float> renderBend(PedalPitchBend& p,const Controls& c,double hz,double seconds=1.) {
    p.reset();std::vector<float> out;juce::AudioBuffer<float> b(2,256);long t=0;
    for(int block=0;block<int(seconds*48000/256);++block){
        for(int n=0;n<256;++n,++t){const float x=.25f*float(std::sin(juce::MathConstants<double>::twoPi*hz*double(t)/48000.));b.setSample(0,n,x);b.setSample(1,n,x);}
        p.process(b,c);for(int n=0;n<256;++n)out.push_back(b.getSample(0,n));
    }
    return out;
}
// Hann-windowed Goertzel over the second half: the strongest frequency in [lo, hi].
// centroid: the energy-weighted mean frequency in [lo, hi] instead. A coarse
// scan, then the peak refined to 1/17 of its step.
double peakHz(const std::vector<float>& x,double lo,double hi,bool centroid=false) {
    const size_t start=x.size()/2,length=x.size()-start;
    std::vector<double> h(length);for(size_t i=0;i<length;++i)h[i]=(.5-.5*std::cos(juce::MathConstants<double>::twoPi*double(i)/double(length)))*x[start+i];
    const auto energy=[&](double hz){const double k=2*std::cos(juce::MathConstants<double>::twoPi*hz/48000.);double s1=0,s2=0;for(size_t i=0;i<length;++i){const double s0=h[i]+k*s1-s2;s2=s1;s1=s0;}return s1*s1+s2*s2-k*s1*s2;};
    const double step=lo*2e-3;double best=0,bestHz=lo,sum=0,weighted=0;
    for(double hz=lo;hz<=hi;hz+=step){const double e=energy(hz);if(e>best){best=e;bestHz=hz;}sum+=e;weighted+=e*hz;}
    if(centroid)return weighted/sum;
    const double coarse=bestHz;
    for(double hz=coarse-step;hz<=coarse+step;hz+=step/17){const double e=energy(hz);if(e>best){best=e;bestHz=hz;}}
    return bestHz;
}
void pitchBendResponse() {
    const auto& m=pedalModel(pitchBend);
    require(juce::String(m.id)=="pre.pitchbend" && juce::String(m.name)=="Pitch Bend" && m.controlCount==3 && pedalMenuEntries[pitchBend].kind==PedalMenuKind::pitch,"Pitch Bend catalog entry");
    require(juce::StringArray::fromTokens(m.controls[1].choices,"|","").size()==PedalPitchBend::settingCount,"The SETTING list differs from the shifter's");
    for(int i=0;i<PedalPitchBend::settingCount;++i)require(juce::StringArray::fromTokens(m.controls[1].choices,"|","")[i]==PedalPitchBend::settings[size_t(i)].name,"A SETTING name differs from the shifter's");
    PedalPitchBend p;p.prepare(48000);
    // Every Whammy setting at the toe, both modes, single notes (tracked,
    // spliced in phase): the spectral peak and the energy centroid.
    double worst=0,worstCentroid=0;
    for(const int mode:{0,1})for(int setting=0;setting<10;++setting)for(const double hz:{82.4,220.}){
        const double target=hz*std::pow(2.,PedalPitchBend::semitones(setting,1)/12.);
        const auto x=renderBend(p,bendControls(1,setting,mode),hz,1.5);
        worst=std::max(worst,std::abs(peakHz(x,target/1.113,target*1.113)/target-1));
        worstCentroid=std::max(worstCentroid,std::abs(peakHz(x,target/1.113,target*1.113,true)/target-1));
    }
    // Low notes take the longest windows (up to one period beyond the base;
    // 52 Hz in CHORDS: four periods, 77 ms): the taps stay inside the delay
    // line, and pitch and level hold.
    double longest=0;
    for(const double hz:{41.2,52.,61.7})for(const int mode:{0,1}){
        const double target=hz*2;
        const auto x=renderBend(p,bendControls(1,1,mode),hz,1.5);
        require(p.longestWindow()<=p.capacity(),"A window reaches beyond the delay line");
        longest=std::max(longest,p.longestWindow());
        require(std::abs(toneDb(x,target)-juce::Decibels::gainToDecibels(.25))<1.,"A low note's shifted level is wrong");
        worst=std::max(worst,std::abs(peakHz(x,target/1.113,target*1.113)/target-1));}
    require(longest>48000*(PedalPitchBend::windowSeconds[1]+.01),"The low-note test does not reach the longest windows");
    std::cout<<"MEASURE Pitch Bend Whammy settings at the toe (82.4 and 220 Hz, CLASSIC and CHORDS, +24 to -36 semitones; OCT UP on 41.2-61.7 Hz): peak error "<<100*worst<<" %, energy centroid error "<<100*worstCentroid<<" %; longest window "<<1000*longest/48000<<" ms within the "<<1000*p.capacity()/48000<<" ms delay line\n";
    require(worst<.005 && worstCentroid<.01,"A Whammy setting misses its interval");
    // A chord (no single period): the base window, energy still centred on the interval.
    {std::vector<float> x;p.reset();juce::AudioBuffer<float> b(2,256);long t=0;
     for(int block=0;block<int(1.*48000/256);++block){
         for(int n=0;n<256;++n,++t){const double ph=juce::MathConstants<double>::twoPi*double(t)/48000.;const float v=.15f*float(std::sin(220.*ph)+std::sin(330.*ph));b.setSample(0,n,v);b.setSample(1,n,v);}
         p.process(b,bendControls(1,1,1));for(int n=0;n<256;++n)x.push_back(b.getSample(0,n));}
     const double low=peakHz(x,440/1.08,440*1.08,true),high=peakHz(x,660/1.08,660*1.08,true);
     std::cout<<"MEASURE Pitch Bend OCT UP on a 220+330 Hz chord (CHORDS): energy centroids "<<low<<" and "<<high<<" Hz\n";
     require(std::abs(low/440-1)<.02 && std::abs(high/660-1)<.02,"A chord is not shifted by an octave");}
    // Harmony: heel and toe intervals over the dry signal.
    double harmonyWorst=0;
    for(int setting=10;setting<19;++setting)for(const float position:{0.f,1.f}){
        const double target=220.*std::pow(2.,PedalPitchBend::semitones(setting,position)/12.);
        const auto x=renderBend(p,bendControls(position,setting,0),220.);
        harmonyWorst=std::max(harmonyWorst,std::abs(peakHz(x,target/1.0413,target*1.0413)/target-1));
        require(std::abs(toneDb(x,220.)-juce::Decibels::gainToDecibels(.25))<1.,"Harmony loses the dry signal");
    }
    std::cout<<"MEASURE Pitch Bend Harmony settings at heel and toe over the dry signal: largest pitch error "<<100*harmonyWorst<<" %\n";
    require(harmonyWorst<.005,"A Harmony setting misses its interval");
    require(PedalPitchBend::semitones(19,1)==-.1f && PedalPitchBend::semitones(20,1)==-.25f && PedalPitchBend::semitones(20,0)==0 && PedalPitchBend::semitones(1,.5f)==6,"Detune amounts or the treadle law differ from the design");
    // Whammy at the heel is unison: one tap, the input's level, no comb,
    // also after the treadle comes back from the toe.
    {std::vector<float> x;p.reset();juce::AudioBuffer<float> b(2,256);long t=0;
     for(int block=0;block<int(4.*48000/256);++block){
         for(int n=0;n<256;++n,++t){const float v=.25f*float(std::sin(juce::MathConstants<double>::twoPi*220.*double(t)/48000.));b.setSample(0,n,v);b.setSample(1,n,v);}
         p.process(b,bendControls(block<int(.5*48000/256)?1.f:0.f,1,0));for(int n=0;n<256;++n)x.push_back(b.getSample(0,n));}
     const double level=toneDb(x,220.)-juce::Decibels::gainToDecibels(.25);
     std::cout<<"MEASURE Pitch Bend heel after the toe (OCT UP, 1.5-3.5 s later): 220 Hz "<<level<<" dB re input\n";
     require(std::abs(level)<.1,"The heel is not a clean unison");}
    // A CLASSIC / CHORDS change mid-note does not jump.
    {p.reset();juce::AudioBuffer<float> b(2,256);long t=0;float steady=0,across=0,last=0;
     for(int block=0;block<int(1.*48000/256);++block){
         for(int n=0;n<256;++n,++t){const float v=.25f*float(std::sin(juce::MathConstants<double>::twoPi*220.*double(t)/48000.));b.setSample(0,n,v);b.setSample(1,n,v);}
         const bool switched=block>=int(.5*48000/256);p.process(b,bendControls(.6f,1,switched?1:0));
         for(int n=0;n<256;++n){const float y=b.getSample(0,n),d=std::abs(y-last);last=y;if(block>20 && !switched)steady=std::max(steady,d);if(switched)across=std::max(across,d);}}
     std::cout<<"MEASURE Pitch Bend mode change: largest sample step "<<across<<" against "<<steady<<" while steady\n";
     require(across<1.2f*steady,"A CLASSIC / CHORDS change jumps");}
    std::cout<<"PASS Pitch Bend: ten Whammy intervals in both modes, nine Harmony pairs over dry, detune amounts, clean unison heel, click-free mode change\n";
}
void noAllocation() {
    PedalParaEQ eq;eq.prepare(48000);juce::AudioBuffer<float> b(2,128);auto c=neutral();
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){c[4]=-15.f+30.f*float(i%20)/19.f;c[3]=float(i%7)/6.f;c[10]=float(i%2);eq.process(b,c);}
    countAllocations=false;require(allocations==0,"Para EQ allocates while processing");
    PedalMultiComp mc;mc.prepare(48000);Controls m=comp(3,3,-18,0);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){m[3]=float((i/20)%3);m[0]=1+4*float(i%9)/8.f;eq.process(b,c);mc.process(b,m);}
    countAllocations=false;require(allocations==0,"Multi Comp allocates while processing");
    PedalTightBoost tb;tb.prepare(48000,128);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i)tb.process(b,22.f*float(i%11)/10.f);
    countAllocations=false;require(allocations==0,"Tight Boost allocates while processing");
    PedalPowerWah pw;pw.prepare(48000);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i)pw.process(b,wah(float(i%10)/9.f,20,i%3!=0,i%2==0));
    countAllocations=false;require(allocations==0,"Power Wah allocates while processing");
    PedalBassSynth bs;bs.prepare(48000);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){for(int n=0;n<128;++n){const float x=.2f*float(std::sin(.015*(i*128+n)));b.setSample(0,n,x);b.setSample(1,n,x);}bs.process(b,synth(.7f,float(i%3)/2.f,float(i%5)/4.f,.3f,.4f,float(i%2),i%3,i%2==0));}
    countAllocations=false;require(allocations==0,"Bass Synth allocates while processing");
    PedalDreadfulPeril ht;ht.prepare(48000,128);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){for(int n=0;n<128;++n){const float x=(i%4==0?0.f:.2f)*float(std::sin(.03*(i*128+n)));b.setSample(0,n,x);b.setSample(1,n,x);}ht.process(b,peril(float(i%10)/9.f,.5f,-12,0,3,-3,-60,float(i%7)/6.f,i%3,i%2==0));}
    countAllocations=false;require(allocations==0,"Dreadful Peril allocates while processing");
    PedalSevenEQ se;se.prepare(48000);Controls g{};
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){g[size_t(i%7)]=-15.f+30.f*float(i%11)/10.f;g[7]=float(i%5)-2.f;se.process(b,g);}
    countAllocations=false;require(allocations==0,"Seven EQ allocates while processing");
    PedalMultiWah mw;mw.prepare(48000);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i)mw.process(b,multi(float(i%9)/8.f,i%6,float(i%4)/3.f,8,i%2==0));
    countAllocations=false;require(allocations==0,"Multi Wah allocates while processing");
    PedalOctaveFuzz of;of.prepare(48000,128);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){for(int n=0;n<128;++n){const float x=(i%4==0?0.f:.2f)*float(std::sin(.03*(i*128+n)));b.setSample(0,n,x);b.setSample(1,n,x);}of.process(b,octave(-6,float(i%10)/9.f,float(i%7)/6.f,.5f,.5f,i%3!=0,i%2==0,float(i%5)*5.f,.5f));}
    countAllocations=false;require(allocations==0,"Octave Fuzz allocates while processing");
    PedalVioletRage vr;vr.prepare(48000,128);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i)vr.process(b,violet(-12,float(i%10)/9.f,float(i%5)*6-12,float(i%3)*12-12,float(i%7)*4-12,i%3));
    countAllocations=false;require(allocations==0,"Violet Rage allocates while processing");
    PedalFireDrake fd;fd.prepare(48000,128);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i)fd.process(b,violet(-12,float(i%10)/9.f,float(i%5)*6-12,float(i%3)*12-12,float(i%7)*4-12,i%2));
    countAllocations=false;require(allocations==0,"Fire Drake allocates while processing");
    PedalPitchBend pb;pb.prepare(48000);
    allocations=0;countAllocations=true;
    for(int i=0;i<200;++i){for(int n=0;n<128;++n){const float x=.2f*float(std::sin(.03*(i*128+n)));b.setSample(0,n,x);b.setSample(1,n,x);}pb.process(b,bendControls(float(i%10)/9.f,i%21,(i/50)%2));}
    countAllocations=false;require(allocations==0,"Pitch Bend allocates while processing");
    std::cout<<"PASS Expansion A and B pedals: no allocation while automating or switching\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {catalogAndMenu();parametersAppend();response();boardIntegration();oldProjects();multiCompCurve();multiCompModes();tightBoostResponse();powerWahResponse();bassSynthResponse();dreadfulPerilResponse();sevenEQResponse();multiWahResponse();octaveFuzzResponse();violetRageResponse();fireDrakeResponse();pitchBendResponse();noAllocation();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

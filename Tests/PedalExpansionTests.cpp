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
constexpr int paraEQ=40,multiComp=41,tightBoost=42,powerWah=43;
bool expansionID(const juce::String& id){for(int m=releasedPedalModelCount;m<pedalModelCount;++m)if(id.startsWith("board") && id.contains("_m"+juce::String(m)+"_"))return true;return false;}
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
using Controls=std::array<float,12>;
Controls neutral() {Controls c{};const auto i=defaultPedalInstance(paraEQ);for(size_t k=0;k<c.size();++k)c[k]=i.controls[k];return c;}

void catalogAndMenu() {
    const auto& m=pedalModel(paraEQ);
    require(juce::String(m.id)=="pre.paraeq" && juce::String(m.name)=="Para EQ" && m.implemented && m.controlCount==11,"Para EQ catalog entry");
    require(releasedPedalModelCount==40 && pedalModelCount==44,"Model counts");
    require(juce::String(pedalMenuName(paraEQ))=="Para EQ" && pedalMenuEntries[paraEQ].kind==PedalMenuKind::equalizer,"Para EQ is not in the Equalizer menu");
    const auto n=neutral();
    require(n[1]==0 && n[4]==0 && n[7]==0 && juce::roundToInt(n[2])==1 && n[10]<.5f,"Para EQ does not start flat with medium Q and boost off");
    std::cout<<"PASS Para EQ catalog: model 40, 11 controls, Equalizer menu, starts flat\n";
}
void parametersAppend() {
    auto p=std::make_unique<ChimeraProcessor>();const int total=p->getParameters().size(),first=total-pedalExpansionParameterCount;
    require(pedalExpansionParameterCount==5*(12+6+2+5),"Expansion banks: 5 x (bypass + controls) for Para EQ (11), Multi Comp (5), Tight Boost (1), Power Wah (4)");
    int seen=0;
    for(auto* raw:p->getParameters()) {
        const auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);require(parameter!=nullptr,"Parameter without ID");
        const bool expansion=expansionID(parameter->paramID);
        if(expansion){require(parameter->getParameterIndex()>=first && parameter->getVersionHint()==pedalExpansionParameterVersion,"A Para EQ parameter is not appended at version 16");++seen;}
        else require(parameter->getParameterIndex()<first,"A released parameter moved behind the Para EQ banks");
    }
    require(seen==pedalExpansionParameterCount,"Para EQ banks incomplete");
    for(int owner=0;owner<5;++owner)require(p->parameters().getParameter(pedalBypassID(owner,paraEQ))!=nullptr && p->parameters().getParameter(pedalControlID(owner,paraEQ,9))!=nullptr,"Para EQ bank missing for an owner");
    std::cout<<"PASS expansion parameters: "<<pedalExpansionParameterCount<<" appended after every released parameter at version 16\n";
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
    std::cout<<"PASS Para EQ, Multi Comp, Tight Boost and Power Wah: no allocation while automating or switching\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {catalogAndMenu();parametersAppend();response();boardIntegration();oldProjects();multiCompCurve();multiCompModes();tightBoostResponse();powerWahResponse();noAllocation();return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

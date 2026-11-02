// Measurements for the 1.7 POST expansion (docs/POST_EXPANSION_1_7.md):
// documented control functions, not hardware response matching.
#include "PostNativeDSP.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace spectralforge;
constexpr double pi=3.14159265358979323846,rate=48000;
void require(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
int control(int s,int m,const char* id){const auto& model=postNativeModel(s,m);for(int c=0;c<model.controlCount;++c)if(std::string(model.controls[c].id)==id)return c;throw std::runtime_error(std::string("no control ")+id);}
struct Setup {int s,m;PostNativeBank bank;};
Setup make(int s,int m){Setup x{s,m,defaultPostNativeState().sections[s].banks[m]};x.bank.bypass=false;return x;}
void set(Setup& x,const char* id,float value){x.bank.values[size_t(control(x.s,x.m,id))]=value;}
// Steady sine (both channels unless rightScale differs); returns {left, right} output tones.
std::pair<std::vector<float>,std::vector<float>> run(const Setup& x,double hz,double amplitude,int samples=48000,double rightScale=1) {
    PostNativeDSP dsp;dsp.prepare({rate,256,2});auto state=defaultPostNativeState();state.sections[x.s].selected=x.m;state.sections[x.s].banks[x.m]=x.bank;
    std::vector<float> l((size_t)samples),r((size_t)samples);juce::AudioBuffer<float> b(2,256);
    for(int offset=0;offset<samples;offset+=256){const int count=std::min(256,samples-offset);b.setSize(2,count,false,false,true);
        for(int n=0;n<count;++n){const float v=float(amplitude*std::sin(2*pi*hz*(offset+n)/rate));b.setSample(0,n,v);b.setSample(1,n,float(v*rightScale));}
        dsp.processSection(b,x.s,state.sections[x.s]);for(int n=0;n<count;++n){l[size_t(offset+n)]=b.getSample(0,n);r[size_t(offset+n)]=b.getSample(1,n);}}
    return {l,r};
}
double tone(const std::vector<float>& v,double hz,int from,int length=0) {
    if(length<=0)length=int(std::round(int((v.size()-size_t(from))*hz/rate)*rate/hz))-1;double re=0,im=0;
    for(int n=0;n<length;++n){const double w=2*pi*hz*n/rate;re+=v[size_t(from+n)]*std::cos(w);im+=v[size_t(from+n)]*std::sin(w);}
    return 2*std::sqrt(re*re+im*im)/length;
}
double gainDb(const Setup& x,double hz,double amplitude){const auto out=run(x,hz,amplitude);return db(tone(out.first,hz,24000)/amplitude);}
double thd(const std::vector<float>& v,double hz,int from){const int length=int(std::round(int((v.size()-size_t(from))*hz/rate)*rate/hz))-1;double h=0;for(int k=2;k<=6;++k)h+=std::pow(tone(v,k*hz,from,length),2);return db(std::sqrt(h)/tone(v,hz,from,length));}

void dynamics() {
    // Polar Press static curve: output level change per input dB across the
    // threshold, for each Function region. Threshold 0 dB panel = -18 dBFS.
    const auto slope=[](float function) {
        auto x=make(0,3);set(x,"function",function);set(x,"threshold",0);set(x,"atten_limit",30);set(x,"gain_limit",30);
        const double a=db(.0316*std::sqrt(2))+gainDb(x,1000,.0316*std::sqrt(2)),b=db(.1*std::sqrt(2))+gainDb(x,1000,.1*std::sqrt(2));   // -30 / -20 dBFS RMS
        return (b-a)/10.;
    };
    const double neutral=slope(.5f),limit=slope(.75f),reverse=slope(1.f),expand=slope(.25f);
    std::cout<<"MEASURE Polar Press output change per input dB, -30 to -20 dBFS: neutral "<<neutral<<", 3/4 (infinite) "<<limit<<", full (reverse) "<<reverse<<", 1/4 (expand) "<<expand<<"\n";
    require(std::abs(neutral-1)<.02 && std::abs(limit)<.05 && std::abs(reverse+1)<.06 && std::abs(expand-2.5)<.08,"Function regions do not follow the documented slopes");
    auto limited=make(0,3);set(limited,"function",1);set(limited,"threshold",-25);set(limited,"atten_limit",6);set(limited,"gain_limit",30);
    const double cut=gainDb(limited,1000,.5)-0;   // far above threshold: reversal wants a huge cut, the limit holds 6 dB
    std::cout<<"MEASURE Polar Press attenuation limit 6 dB, loud tone: gain "<<cut<<" dB\n";
    require(std::abs(cut+6)<.2,"Attenuation limit not held");

    // Thrust Bus: hard-knee 4:1 static ratio, and stereo link.
    auto thrust=make(0,4);set(thrust,"knee",0);set(thrust,"ratio",3);set(thrust,"threshold",0);set(thrust,"attack",3);set(thrust,"release",3);
    const double t10=db(.4*std::sqrt(2))+gainDb(thrust,1000,.4*std::sqrt(2)),t20=db(1.26*std::sqrt(2))+gainDb(thrust,1000,1.26*std::sqrt(2));   // -8 / +2 dBFS RMS
    std::cout<<"MEASURE Thrust Bus 4:1 hard knee, 10 dB more input above threshold: output +"<<(t20-t10)<<" dB\n";
    require(std::abs((t20-t10)-2.5)<.25,"Thrust Bus ratio wrong");
    const auto rightGr=[&](float link){auto x=thrust;set(x,"link",link);const auto out=run(x,1000,.4,48000,.05);return db(tone(out.second,1000,24000)/.02);};
    const double linked=rightGr(100),independent=rightGr(0);
    std::cout<<"MEASURE Thrust Bus quiet right channel gain: link 100 % "<<linked<<" dB, link 0 % "<<independent<<" dB\n";
    require(linked<-3 && std::abs(independent)<.1,"Stereo link not applied");
    auto loud=thrust;set(loud,"thrust",2);
    const double normLow=gainDb(thrust,60,1.),loudLow=gainDb(loud,60,1.);
    std::cout<<"MEASURE Thrust Bus 60 Hz gain: NORM "<<normLow<<" dB, LOUD "<<loudLow<<" dB (less low-frequency reduction)\n";
    require(loudLow>normLow+1,"THRUST does not reduce low-frequency detection");

    // Diode Bridge: 2:1 compressor, limiter ceiling, distortion grows with reduction.
    auto diode=make(0,5);set(diode,"comp_ratio",1);set(diode,"comp_threshold",0);set(diode,"comp_attack",0);set(diode,"comp_recovery",0);
    const double d10=db(.4*std::sqrt(2))+gainDb(diode,1000,.4*std::sqrt(2)),d20=db(1.26*std::sqrt(2))+gainDb(diode,1000,1.26*std::sqrt(2));
    std::cout<<"MEASURE Diode Bridge 2:1, 10 dB more input above threshold: output +"<<(d20-d10)<<" dB\n";
    require(std::abs((d20-d10)-5)<.4,"Diode Bridge ratio wrong");
    auto ceiling=make(0,5);set(ceiling,"comp_in",0);set(ceiling,"limit_in",1);set(ceiling,"limit_threshold",8);set(ceiling,"limit_attack",0);set(ceiling,"limit_recovery",0);
    const double out=db(1.*std::sqrt(.5))+gainDb(ceiling,1000,1.);   // RMS output level
    std::cout<<"MEASURE Diode Bridge limiter at panel 8 (-10 dBFS RMS): 0 dBFS-peak tone leaves at "<<out<<" dBFS RMS\n";
    require(std::abs(out+10)<.6,"Limiter ceiling wrong");
    auto clean=make(0,5);set(clean,"comp_threshold",10);auto squeezed=make(0,5);set(squeezed,"comp_threshold",-20);set(squeezed,"comp_ratio",4);
    const double thdClean=thd(run(clean,200,.5).first,200,24000),thdSqueezed=thd(run(squeezed,200,.5).first,200,24000);
    std::cout<<"MEASURE Diode Bridge THD at 200 Hz: little reduction "<<thdClean<<" dB, heavy reduction "<<thdSqueezed<<" dB\n";
    require(thdSqueezed>thdClean+10,"Diode-bridge colour does not grow with reduction");
    std::cout<<"PASS Polar Press slopes and limits; Thrust Bus ratio, link and THRUST; Diode Bridge ratio, limiter and colour\n";
}

void preamps() {
    const auto small=[](const Setup& x){return gainDb(x,1000,1.e-3);};
    auto lift=make(1,3);const double liftUnity=small(lift);set(lift,"pad",1);const double liftPad=small(lift);
    auto desk=make(1,4);const double desk40=small(desk);set(desk,"coarse_gain",5);const double desk45=small(desk);
    auto clear=make(1,5);const double clear4=small(clear);set(clear,"gain",5);const double clear5=small(clear);
    std::cout<<"MEASURE small-signal 1 kHz: Discrete Lift default "<<liftUnity<<" dB, pad "<<(liftPad-liftUnity)<<" dB; Desk Two coarse step "<<(desk45-desk40)<<" dB (default "<<desk40<<"); Clear Line gain step "<<(clear5-clear4)<<" dB (default "<<clear4<<")\n";
    require(std::abs(liftUnity)<.3 && std::abs(desk40)<.6 && std::abs(clear4)<.3,"Default preamp gain is not near unity");
    require(std::abs(liftPad-liftUnity+20)<.05 && std::abs(desk45-desk40-5)<.05 && std::abs(clear5-clear4-5)<.05,"Stepped gains wrong");
    auto hpf=make(1,5);set(hpf,"high_pass",1);const auto flat=make(1,5);
    const double at75=small(hpf)-clear4;
    {const double low=gainDb(hpf,75,1.e-3)-gainDb(flat,75,1.e-3);std::cout<<"MEASURE Clear Line HPF at 75 Hz: "<<low<<" dB (1 kHz "<<at75<<" dB)\n";require(std::abs(low+3)<.4,"HPF corner not at 75 Hz");}
    // Colour at the same +12 dB of drive into each voicing.
    auto drivenLift=make(1,3);set(drivenLift,"gain",37);
    auto drivenDesk=make(1,4);set(drivenDesk,"fine_gain",10);set(drivenDesk,"coarse_gain",4);set(drivenDesk,"output",-12);   // +10 dB fine
    auto drivenClear=make(1,5);set(drivenClear,"trim",10);set(drivenClear,"gain",6);
    const double a=thd(run(drivenLift,200,.3).first,200,24000),b=thd(run(drivenDesk,200,.3).first,200,24000),c=thd(run(drivenClear,200,.3).first,200,24000);
    std::cout<<"MEASURE THD at 200 Hz, 0.3 input, about +10 to +12 dB drive: Discrete Lift "<<a<<" dB, Desk Two "<<b<<" dB, Clear Line "<<c<<" dB\n";
    require(b>a && a>c,"Colour order (transformer > discrete > clean) not kept");
    std::cout<<"PASS preamp gain staging, pad, steps, HPF and colour order\n";
}

void equalizers() {
    const auto change=[](Setup x,double hz){const double on=gainDb(x,hz,1.e-3);set(x,"eq_in",0);return on-gainDb(x,hz,1.e-3);};
    auto stepped=make(2,3);set(stepped,"mf_frequency",4);set(stepped,"mf_gain",10);   // +12 dB at 1.5 kHz
    const double mid=change(stepped,1500);
    auto narrow=stepped,broad=stepped;set(broad,"mf_gain",6);   // +2 dB
    const double halfBand12=change(narrow,1500*1.5),halfBand2=change(broad,1500*1.5);
    std::cout<<"MEASURE Stepped EQ +12 dB at 1.5 kHz: "<<mid<<" dB; at 2.25 kHz: "<<halfBand12<<" dB of 12 ("<<halfBand12/12<<") vs "<<halfBand2<<" of 2 ("<<halfBand2/2<<")\n";
    require(std::abs(mid-12)<.05,"Stepped EQ centre gain wrong");
    require(halfBand12/12<halfBand2/2,"Bandwidth does not narrow with gain (proportional Q)");
    auto air=make(2,4);set(air,"b650",15);const double bell=change(air,650);
    auto airShelf=make(2,4);set(airShelf,"air_gain",20);set(airShelf,"air_frequency",3);const double top=change(airShelf,20000),mid1k=change(airShelf,1000);
    std::cout<<"MEASURE Air Band 650 Hz +15: "<<bell<<" dB; AIR 10 kHz +20: "<<top<<" dB at 20 kHz, "<<mid1k<<" dB at 1 kHz\n";
    require(std::abs(bell-15)<.05 && top>17 && mid1k<1,"Air Band bell or shelf wrong");
    auto curve=make(2,5);set(curve,"presence1_gain",15);set(curve,"presence1_frequency",3);const double p1=change(curve,2800);
    auto filters=make(2,5);set(filters,"high_pass",6);const double hp=change(filters,80);   // 80 Hz HPF
    std::cout<<"MEASURE Curve Shaper Presence 1 +15 at 2.8 kHz: "<<p1<<" dB; HPF 80 Hz at its corner: "<<hp<<" dB\n";
    require(std::abs(p1-15)<.05 && std::abs(hp+3)<.3,"Curve Shaper band or HPF wrong");
    std::cout<<"PASS stepped and proportional-Q EQ, wide bands and AIR shelf, semi-parametric bands and filters\n";
}
}

int main() {
    try {dynamics();preamps();equalizers();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}

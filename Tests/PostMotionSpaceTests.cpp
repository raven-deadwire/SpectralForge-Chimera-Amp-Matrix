// Measurements for the 1.8 POST motion/space models (docs/POST_MOTION_SPACE_1_8.md).
#include "SpatialModules.h"
#include "reference/SpatialModulesReleased.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace spectralforge;
constexpr double pi=3.14159265358979323846,rate=48000;
constexpr int block=256;
void require(bool yes,const std::string& message){if(!yes)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}
struct Stereo {std::vector<float> l,r;};
// Runs a module over a generated stereo input, block by block.
template<class Module,class Input> Stereo run(Module& module,int samples,Input input,int model,float a,float b,float c,bool on=true) {
    module.prepare({rate,(juce::uint32)block,2});Stereo out{std::vector<float>((size_t)samples),std::vector<float>((size_t)samples)};
    juce::AudioBuffer<float> buffer(2,block);
    for(int offset=0;offset<samples;offset+=block){const int count=std::min(block,samples-offset);buffer.setSize(2,count,false,false,true);
        for(int n=0;n<count;++n){buffer.setSample(0,n,input(offset+n,0));buffer.setSample(1,n,input(offset+n,1));}
        module.process(buffer,on,model,a,b,c);
        for(int n=0;n<count;++n){out.l[size_t(offset+n)]=buffer.getSample(0,n);out.r[size_t(offset+n)]=buffer.getSample(1,n);}}
    return out;
}
float noise(int n,int c){std::uint32_t x=std::uint32_t(n*2654435761u)^std::uint32_t(c*40503u+12345u);x^=x>>13;x*=0x5bd1e995u;x^=x>>15;return float(x&0xffff)/32768.f-1.f;}
auto sine(double hz,double amplitude){return [=](int n,int){return float(amplitude*std::sin(2*pi*hz*n/rate));};}
auto impulse(int at=0){return [=](int n,int){return n==at?1.f:0.f;};}
double rms(const std::vector<float>& v,int from,int to){double s=0;for(int n=from;n<to;++n)s+=double(v[size_t(n)])*v[size_t(n)];return std::sqrt(s/std::max(1,to-from));}
double tone(const std::vector<float>& v,double hz,int from,int length){double re=0,im=0;for(int n=0;n<length;++n){const double w=2*pi*hz*n/rate;re+=v[size_t(from+n)]*std::cos(w);im+=v[size_t(from+n)]*std::sin(w);}return 2*std::sqrt(re*re+im*im)/length;}

// The released models must be bit-identical to the 1.7 modules.
void releasedParity() {
    size_t compared=0;
    const auto both=[&](auto& now,auto& before,int model,std::array<float,3> p,const char* name){
        const auto x=[](int n,int c){return .3f*noise(n,c)+.4f*float(std::sin(2*pi*220*n/rate));};
        const auto a=run(now,24000,x,model,p[0],p[1],p[2]),b=run(before,24000,x,model,p[0],p[1],p[2]);
        for(size_t n=0;n<a.l.size();++n)require(a.l[n]==b.l[n] && a.r[n]==b.r[n],std::string("Released model changed: ")+name+" "+std::to_string(model));
        compared+=a.l.size()*2;
    };
    for(int model=0;model<6;++model)for(auto p:{std::array<float,3>{.7f,.3f,.5f},std::array<float,3>{4.f,1.f,1.f}}){ModulationModule a;released_spatial::ModulationModule b;both(a,b,model,p,"modulation");}
    for(int model=0;model<3;++model)for(auto p:{std::array<float,3>{250.f,.3f,.4f},std::array<float,3>{900.f,.8f,1.f}}){EchoModule a;released_spatial::EchoModule b;both(a,b,model,p,"delay");}
    for(int model=0;model<3;++model)for(auto p:{std::array<float,3>{.35f,.5f,.4f},std::array<float,3>{1.f,0.f,1.f}}){SpaceModule a;released_spatial::SpaceModule b;both(a,b,model,p,"reverb");}
    std::cout<<"PASS released DELAY / REVERB / MODULATION models bit-identical ("<<compared<<" samples)\n";
}

void modulation() {
    // Spin Cabinet: the horn's amplitude rotation shows up as a level
    // modulation at the rotor rate; the outputs sit a quarter turn apart.
    // Dominant envelope frequency: DFT of 5 ms RMS frames, 0.25-10 Hz.
    const auto dominant=[](const std::vector<float>& v,int from,int to){std::vector<double> e;for(int n=from;n+240<=to;n+=240)e.push_back(rms(v,n,n+240));double mean=0;for(double x:e)mean+=x;mean/=double(e.size());
        double best=0,bestHz=0;for(double hz=.25;hz<=10;hz+=.05){double re=0,im=0;for(size_t i=0;i<e.size();++i){const double w=2*pi*hz*double(i)*240/rate;re+=(e[i]-mean)*std::cos(w);im+=(e[i]-mean)*std::sin(w);}const double m=re*re+im*im;if(m>best){best=m;bestHz=hz;}}return bestHz;};
    ModulationModule spin;const auto out=run(spin,3*48000,sine(3000,.3),6,2.f,1.f,1.f);
    const double rotation=dominant(out.l,48000,3*48000);
    double lr=0,ll=0,rr=0;for(int n=48000;n<3*48000;++n){lr+=double(out.l[size_t(n)])*out.r[size_t(n)];ll+=double(out.l[size_t(n)])*out.l[size_t(n)];rr+=double(out.r[size_t(n)])*out.r[size_t(n)];}
    std::cout<<"MEASURE Spin Cabinet RATE 2 Hz, 3 kHz tone: dominant level modulation "<<rotation<<" Hz; L/R correlation "<<lr/std::sqrt(ll*rr)<<"\n";
    require(std::abs(rotation-2)<.15,"Spin Cabinet rotation rate wrong");
    // Rotor inertia: switching RATE from 0.7 to 6 Hz, the horn spins up over about a second.
    ModulationModule accel;accel.prepare({rate,(juce::uint32)block,2});juce::AudioBuffer<float> b(2,block);std::vector<float> trace;
    for(int k=0;k<375;++k){for(int n=0;n<block;++n){const float v=float(.3*std::sin(2*pi*3000*(k*block+n)/rate));b.setSample(0,n,v);b.setSample(1,n,v);}accel.process(b,true,6,k<94?.7f:6.f,1.f,1.f);for(int n=0;n<block;++n)trace.push_back(b.getSample(0,n));}
    const double early=dominant(trace,24064,24064+12000),late=dominant(trace,72000,96000);   // first 0.25 s after the switch, and 1-1.5 s after
    std::cout<<"MEASURE Spin Cabinet spin-up 0.7 to 6 Hz: modulation "<<early<<" Hz in the first 0.25 s, "<<late<<" Hz 1-1.5 s later\n";
    require(late>5 && early<late-1,"Spin Cabinet rotors have no inertia");

    // Throb Vibe: all-pass stages keep the level and move the phase.
    ModulationModule vibe;const auto v=run(vibe,48000,sine(440,.3),7,3.f,1.f,1.f);
    const double level=db(rms(v.l,24000,48000)/(.3/std::sqrt(2)));
    std::vector<double> periods;double last=-1;for(int n=24001;n<48000;++n)if(v.l[size_t(n-1)]<0 && v.l[size_t(n)]>=0){const double t=n-1+v.l[size_t(n-1)]/(v.l[size_t(n-1)]-v.l[size_t(n)]);if(last>0)periods.push_back(rate/(t-last));last=t;}
    const auto [lo,hi]=std::minmax_element(periods.begin(),periods.end());
    std::cout<<"MEASURE Throb Vibe 440 Hz: level "<<level<<" dB; instantaneous pitch "<<*lo<<"-"<<*hi<<" Hz\n";
    require(std::abs(level)<.5 && *hi-*lo>2,"Throb Vibe does not sweep phase at constant level");

    // Ring Bell: multiplying by a unit sine halves the power (-3 dB) and moves
    // it into sidebands (the carrier drifts ±10 % at RATE); the 1 kHz input
    // itself is suppressed.
    ModulationModule ring;const auto r=run(ring,48000,sine(1000,.3),8,.05f,.5f,1.f);
    const double total=db(rms(r.l,24000,48000)/(.3/std::sqrt(2))),centre=db(tone(r.l,1000,24000,24000)/.3);
    std::cout<<"MEASURE Ring Bell, 1 kHz into a 320 Hz carrier: output level "<<total<<" dB re input, 1 kHz component "<<centre<<" dB\n";
    require(std::abs(total+3.01)<.3 && centre<-40,"Ring Bell is not a carrier multiply");
    std::cout<<"PASS Spin Cabinet rotation and inertia, Throb Vibe phase sweep, Ring Bell sidebands\n";
}

void delays() {
    // Tap Cloud: six taps across TIME (300 ms); FEEDBACK sets the taper.
    const auto taps=[](float feedback){EchoModule e;const auto out=run(e,24000,impulse(),3,300.f,feedback,1.f);std::array<double,6> t{};for(int k=1;k<=6;++k){const int at=int(std::round(rate*.3*k/6.));double peak=0;for(int n=at-2;n<=at+2;++n)peak=std::max(peak,double(std::abs(out.l[size_t(n)])));t[size_t(k-1)]=peak;}return t;};
    const auto up=taps(0),flat=taps(.425f),down=taps(.85f);
    std::cout<<"MEASURE Tap Cloud tap levels (first, last): fade up "<<up[0]<<", "<<up[5]<<"; equal "<<flat[0]<<", "<<flat[5]<<"; fade down "<<down[0]<<", "<<down[5]<<"\n";
    require(up[5]>up[0]*3 && std::abs(flat[0]-flat[5])<.01*flat[0] && down[0]>down[5]*3,"Tap Cloud taper wrong");
    // Dual Digital: first echoes at 3/4 TIME and TIME.
    EchoModule dual;const auto d=run(dual,48000,impulse(),5,400.f,.3f,1.f);
    const auto peakAt=[&](const std::vector<float>& v,int from,int to){int best=from;for(int n=from;n<to;++n)if(std::abs(v[size_t(n)])>std::abs(v[size_t(best)]))best=n;return best;};
    const int first=peakAt(d.l,100,int(rate*.35)),second=peakAt(d.l,int(rate*.35),int(rate*.45));
    std::cout<<"MEASURE Dual Digital TIME 400 ms: first echoes at "<<first*1000./rate<<" and "<<second*1000./rate<<" ms\n";
    require(std::abs(first-rate*.3)<3 && std::abs(second-rate*.4)<3,"Dual Digital line times wrong");
    // Bloom Delay: the delay time drifts, so a single echo (FEEDBACK 0) of a
    // 1 kHz tone is pitch-modulated; peak deviation from the drift design
    // (1.2 ms x (sin 0.6 Hz + 0.5 sin 0.13 Hz)) is about 4.5 Hz.
    EchoModule bloom;const auto bl=run(bloom,4*48000,sine(1000,.3),4,300.f,0.f,1.f);
    std::vector<double> pitch;double last=-1;for(int n=48001;n<4*48000;++n)if(bl.l[size_t(n-1)]<0 && bl.l[size_t(n)]>=0){const double t=n-1+bl.l[size_t(n-1)]/(bl.l[size_t(n-1)]-bl.l[size_t(n)]);if(last>0)pitch.push_back(rate/(t-last));last=t;}
    const auto [lo,hi]=std::minmax_element(pitch.begin(),pitch.end());
    // Loop colour: repeats of an impulse lose high frequencies pass by pass.
    EchoModule colour;const auto rep=run(colour,2*48000,impulse(),4,300.f,.7f,1.f);
    const auto brightness=[&](int at){double hf=0,all=0;for(int n=at-600;n<at+600;++n){const double x=rep.l[size_t(n)],dx=x-rep.l[size_t(n-1)];hf+=dx*dx;all+=x*x;}return hf/std::max(all,1.e-30);};
    const double darkening=brightness(int(rate*.3))/brightness(int(rate*1.5));
    std::cout<<"MEASURE Bloom Delay single echo of 1 kHz: pitch "<<*lo<<"-"<<*hi<<" Hz; brightness first/fifth repeat "<<darkening<<"\n";
    require(*hi-*lo>4 && *hi-*lo<14,"Bloom Delay time drift outside its design");
    require(darkening>1.5,"Bloom Delay loop does not darken the repeats");
    std::cout<<"PASS Tap Cloud taps and taper, Dual Digital line times, Bloom Delay drift and loop colour\n";
}

double decayT60(const std::vector<float>& raw,double from,double to) {
    // Low-frequency decay (below about 500 Hz, under the loop damping): slope
    // of the energy envelope between two times, extrapolated to 60 dB.
    std::vector<float> v(raw.size());float z=0;const float a=float(1-std::exp(-2*pi*500/rate));for(size_t n=0;n<raw.size();++n){z+=a*(raw[n]-z);v[n]=z;}
    const auto level=[&](double t){const int n=int(t*rate);return 10*std::log10(std::max(1.e-30,std::pow(rms(v,n,n+int(rate*.05)),2.)));};
    return 60.*(to-from)/(level(from)-level(to));
}
void reverbs() {
    const auto response=[](int model,float size,int seconds){SpaceModule s;return run(s,seconds*48000,impulse(),model,size,0.f,1.f);};
    const auto small=response(3,0.f,3),large=response(3,1.f,8);
    const double t0=decayT60(small.l,.3,1.),t1=decayT60(large.l,.5,3.);
    std::cout<<"MEASURE Hall Prime T60 (damping 0): SIZE 0 "<<t0<<" s (design 1.2 s), SIZE 1 "<<t1<<" s (design 6 s)\n";
    require(std::abs(t0/1.2-1)<.15 && std::abs(t1/6-1)<.15,"Hall Prime decay differs from its design");
    const auto huge=response(4,.5f,6);const double tv=decayT60(huge.l,1.,5.);
    std::cout<<"MEASURE Void Space SIZE 0.5 T60 "<<tv<<" s (design 33 s)\n";
    require(std::abs(tv/33-1)<.15,"Void Space decay differs from its design");
    // Gated Room: a burst opens the gate for SIZE (0.5: 350 ms), then it closes.
    SpaceModule g;const auto gated=run(g,48000,[](int n,int){return n<480?float(.5*std::sin(2*pi*500*n/rate)):0.f;},5,.5f,0.f,1.f);
    const double open=rms(gated.l,int(rate*.1),int(rate*.3)),closed=rms(gated.l,int(rate*.4),int(rate*.6));
    std::cout<<"MEASURE Gated Room SIZE 0.5: wet level 100-300 ms "<<db(open)<<" dBFS, 400-600 ms "<<db(closed)<<" dBFS\n";
    require(db(open)>-50 && closed==0,"Gated Room does not hold and then close");
    std::cout<<"PASS Hall Prime and Void Space decay times, Gated Room hold and close\n";
}

void stability() {
    // Extreme settings, silent tails and finite output for every new model.
    for(int model=6;model<9;++model){ModulationModule m;const auto out=run(m,48000,[](int n,int c){return noise(n,c);},model,5.f,1.f,1.f);for(float x:out.l)require(std::isfinite(x)&&std::abs(x)<8,"Modulation unstable");}
    for(int model=3;model<6;++model){EchoModule e;const auto out=run(e,96000,[](int n,int c){return n<48000?noise(n,c):0.f;},model,1000.f,.85f,1.f);for(float x:out.l)require(std::isfinite(x)&&std::abs(x)<8,"Delay unstable");}
    for(int model=3;model<6;++model){SpaceModule s;const auto out=run(s,96000,[](int n,int c){return n<48000?noise(n,c):0.f;},model,1.f,0.f,1.f);for(float x:out.l)require(std::isfinite(x)&&std::abs(x)<8,"Reverb unstable");}
    std::cout<<"PASS extreme settings stay finite and bounded\n";
}
}

int main() {
    try {releasedParity();modulation();delays();reverbs();stability();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}

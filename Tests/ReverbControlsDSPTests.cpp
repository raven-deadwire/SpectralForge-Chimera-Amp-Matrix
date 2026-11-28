// POST reverb controls (docs/POST_REVERB_CONTROLS.md): pre-delay, low cut, tone, width and freeze,
// measured on the reverb module itself. Synthetic input; this is not a listening test.
#include <juce_dsp/juce_dsp.h>
#include "SpatialModules.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace spectralforge;
namespace {
constexpr double rate=48000,pi=3.14159265358979323846;constexpr int block=256;constexpr float mix=.5f;
constexpr int modelCount=6;
const char* const modelNames[modelCount]{"room","hall","spring","Hall Prime","Void Space","Gated Room"};
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
std::string num(double v,int digits=2){std::ostringstream o;o<<std::fixed<<std::setprecision(digits)<<v;return o.str();}
double db(double power){return 10*std::log10(std::max(power,1e-30));}
// The wet signal of both channels: the module mixes dry*(1-mix)+wet*mix, so the dry is taken back out.
struct Wet {std::vector<float> left,right;
    double power(double fromSeconds,double toSeconds,int channel=0) const {const auto& x=channel?right:left;double e=0;const size_t a=size_t(fromSeconds*rate),b=std::min(x.size(),size_t(toSeconds*rate));
        for(size_t n=a;n<b;++n)e+=double(x[n])*x[n];return e/double(std::max<size_t>(1,b-a));}};
// controls(seconds): the controls at that time. Half a second of silence first, so the mix has settled.
template<class Controls> Wet render(int model,const std::vector<float>& left,const std::vector<float>& right,Controls&& controls,float size=.5f,float damping=.55f,int channels=2) {
    SpaceModule space;space.prepare({rate,juce::uint32(block),juce::uint32(channels)});
    juce::AudioBuffer<float> b(channels,block);
    for(int k=0;k<int(rate*.5)/block;++k){b.clear();space.process(b,true,model,size,damping,mix,controls(0.));}
    Wet wet;
    for(size_t offset=0;offset+block<=left.size();offset+=block) {
        b.copyFrom(0,0,left.data()+offset,block);if(channels>1)b.copyFrom(1,0,right.data()+offset,block);
        space.process(b,true,model,size,damping,mix,controls(double(offset)/rate));
        for(int n=0;n<block;++n) {
            const float l=(b.getSample(0,n)-left[offset+size_t(n)]*(1-mix))/mix,r=channels>1?(b.getSample(1,n)-right[offset+size_t(n)]*(1-mix))/mix:l;
            require(std::isfinite(l) && std::isfinite(r),std::string(modelNames[model])+": the reverb produced a value that is not finite");
            wet.left.push_back(l);wet.right.push_back(r);
        }
    }
    return wet;
}
const auto fixed=[](ReverbControls k){return [k](double){return k;};};
std::vector<float> noise(double seconds,double burstSeconds,unsigned seed,float level=.1f) {
    std::mt19937 rng{seed};std::normal_distribution<float> g(0,level);std::vector<float> x(size_t(seconds*rate),0.f);
    for(size_t n=0;n<std::min(x.size(),size_t(burstSeconds*rate));++n)x[n]=g(rng);return x;
}
std::vector<float> sine(double seconds,double hz,float level=.1f){std::vector<float> x(size_t(seconds*rate));for(size_t n=0;n<x.size();++n)x[n]=float(level*std::sin(2*pi*hz*double(n)/rate));return x;}

// PRE-DELAY moves the whole reverb later by its time and changes nothing else.
void preDelay() {
    std::vector<float> click(size_t(rate*1.5),0.f);click[100]=.5f;
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        const auto base=render(model,click,click,fixed({}));
        ReverbControls k;k.preDelayMs=100;const auto late=render(model,click,click,fixed(k));
        constexpr size_t shift=4800;
        double worst=0,reference=0;
        for(size_t n=0;n+shift<base.left.size();++n){worst=std::max(worst,double(std::abs(late.left[n+shift]-base.left[n])));reference=std::max(reference,double(std::abs(base.left[n])));}
        double early=0;for(size_t n=0;n<shift;++n)early=std::max(early,double(std::abs(late.left[n])));
        require(reference>1e-4,std::string(modelNames[model])+": the click made no reverb");
        require(early<reference*1e-4,std::string(modelNames[model])+": reverb was heard before the pre-delay had passed");
        // Room, spring and Gated Room do not move in time: the late reverb is the same samples. Hall, Hall Prime and
        // Void Space are slowly modulated, so theirs is compared by its level in 50 ms steps.
        const bool still=model==0 || model==2 || model==5;
        double envelope=0;
        for(double t=0;t+.05<=1.2;t+=.05){const double a=base.power(t,t+.05),b=late.power(t+.1,t+.15);if(a>reference*reference*1e-6)envelope=std::max(envelope,std::abs(db(b)-db(a)));}
        if(still)require(worst<reference*1e-3,std::string(modelNames[model])+": with 100 ms of pre-delay the reverb is not the same reverb 100 ms later (difference "+num(worst/reference*100,3)+"% of its peak)");
        require(envelope<1.5,std::string(modelNames[model])+": with 100 ms of pre-delay the reverb's level 100 ms later differs by "+num(envelope)+" dB");
        measured+=std::string(model?", ":"")+modelNames[model]+" "+(still?num(worst/reference*100,4)+"%":num(envelope)+" dB");
    }
    std::cout<<"MEASURE reverb controls pre-delay (100 ms; against the same reverb 4800 samples later: sample difference of its peak, or level difference for the modulated models): "<<measured<<'\n';
    std::cout<<"PASS reverb controls pre-delay: every model plays its reverb later by the pre-delay, silent until then\n";
}
// LOW CUT takes the lows out of the reverb and leaves the rest; TONE tilts it around 1.5 kHz.
void lowCutAndTone() {
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        // The tone in bursts of 0.2 s every half second, so the Gated Room opens on each.
        const auto level=[&](double hz,ReverbControls k){auto x=sine(3,hz);for(size_t n=0;n<x.size();++n)if(n%size_t(rate*.5)>=size_t(rate*.2))x[n]=0;return db(render(model,x,x,fixed(k)).power(1,3));};
        ReverbControls cut;cut.lowCutHz=500;ReverbControls bright;bright.tone=1;ReverbControls dark;dark.tone=-1;
        const double low=level(60,cut)-level(60,{}),high=level(4000,cut)-level(4000,{});
        require(low<-20,std::string(modelNames[model])+": LOW CUT at 500 Hz took only "+num(low)+" dB off 60 Hz in the reverb");
        require(std::abs(high)<1,std::string(modelNames[model])+": LOW CUT at 500 Hz changed 4 kHz in the reverb by "+num(high)+" dB");
        const double brightTop=level(8000,bright)-level(8000,{}),brightLow=level(150,bright)-level(150,{}),darkTop=level(8000,dark)-level(8000,{}),darkLow=level(150,dark)-level(150,{});
        require(brightTop>4.5 && brightTop<6.5 && brightLow<-4.5 && brightLow>-6.5,std::string(modelNames[model])+": TONE at +1 gave "+num(brightTop)+" dB at 8 kHz and "+num(brightLow)+" dB at 150 Hz");
        require(darkTop<-4.5 && darkTop>-6.5 && darkLow>4.5 && darkLow<6.5,std::string(modelNames[model])+": TONE at -1 gave "+num(darkTop)+" dB at 8 kHz and "+num(darkLow)+" dB at 150 Hz");
        measured+=std::string(model?" | ":"")+modelNames[model]+": cut "+num(low,1)+"/"+num(high,1)+" dB, tone +1 "+num(brightLow,1)+"/"+num(brightTop,1)+" dB";
    }
    std::cout<<"MEASURE reverb controls low cut at 500 Hz (60 Hz / 4 kHz) and tone +1 (150 Hz / 8 kHz): "<<measured<<'\n';
    std::cout<<"PASS reverb controls low cut and tone: the lows leave the reverb with the rest kept; tone tilts it by up to 6 dB each way\n";
}
// WIDTH scales the difference between the two wet channels: 0 is mono, 2 doubles it.
void width() {
    std::string measured;
    // Bursts of 0.2 s every half second, different in each channel: the Gated Room opens on each.
    auto left=noise(3,3,7),right=noise(3,3,8);for(size_t n=0;n<left.size();++n)if(n%size_t(rate*.5)>=size_t(rate*.2))left[n]=right[n]=0;
    for(int model=0;model<modelCount;++model) {
        const auto measure=[&](float w){ReverbControls k;k.width=w;const auto wet=render(model,left,right,fixed(k));double mid=0,side=0;
            for(size_t n=size_t(rate);n<wet.left.size();++n){const double m=.5*(wet.left[n]+wet.right[n]),s=.5*(wet.left[n]-wet.right[n]);mid+=m*m;side+=s*s;}
            return std::pair<double,double>{mid,side};};
        const auto made=measure(1),mono=measure(0),wide=measure(2);
        require(made.second>made.first*1e-4,std::string(modelNames[model])+": the reverb has no difference between its channels to scale");
        require(mono.second<made.second*1e-8,std::string(modelNames[model])+": WIDTH at 0 left a difference between the wet channels");
        const double sideDb=db(wide.second)-db(made.second),midDb=db(wide.first)-db(made.first),monoMidDb=db(mono.first)-db(made.first);
        require(std::abs(sideDb-6.02)<.1 && std::abs(midDb)<.01 && std::abs(monoMidDb)<.01,std::string(modelNames[model])+": WIDTH at 2 changed the difference by "+num(sideDb)+" dB and the sum by "+num(midDb,3)+" dB");
        measured+=std::string(model?", ":"")+modelNames[model]+" "+num(sideDb)+" dB";
    }
    std::cout<<"MEASURE reverb controls width (difference of the wet channels at 2, against 1): "<<measured<<'\n';
    std::cout<<"PASS reverb controls width: 0 is mono, 2 doubles the difference between the channels, the sum is kept\n";
}
// FREEZE holds the tail and takes no new input; released, the reverb decays again.
void freeze() {
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        // Noise for one second; frozen from 0.9 s to 9 s; released after.
        const auto burst=noise(14,1,11);
        const auto frozenAt=[](double from,double to){return [=](double t){ReverbControls k;k.freeze=t>=from && t<to;return k;};};
        const auto open=render(model,burst,burst,fixed({}));
        const auto held=render(model,burst,burst,frozenAt(.9,9));
        const double start=db(held.power(1.5,2.5)),end=db(held.power(8,9)),openEnd=db(open.power(8,9)),released=db(held.power(13,14));
        // Hall Prime at SIZE 0.5 decays over 3.6 s and the others faster, Void Space over 33 s.
        require(end-start>-6,std::string(modelNames[model])+": the frozen tail fell by "+num(start-end)+" dB in 6.5 s");
        require(end-start<3,std::string(modelNames[model])+": the frozen tail grew by "+num(end-start)+" dB in 6.5 s");
        if(model!=4)require(end-openEnd>20,std::string(modelNames[model])+": frozen, the tail at 8 s is only "+num(end-openEnd)+" dB over the unfrozen one");
        if(model!=4)require(released<end-20,std::string(modelNames[model])+": released, the tail fell by only "+num(end-released)+" dB in 5 s");
        // New playing while frozen is not taken in.
        auto more=burst;const auto extra=noise(14,14,12,.2f);for(size_t n=size_t(rate*3);n<size_t(rate*8);++n)more[n]=extra[n];
        const auto heldMore=render(model,more,more,frozenAt(.9,9));
        double difference=0,reference=0;for(size_t n=size_t(rate*3);n<size_t(rate*9);++n){difference=std::max(difference,double(std::abs(heldMore.left[n]-held.left[n])));reference=std::max(reference,double(std::abs(held.left[n])));}
        require(difference<reference*1e-3,std::string(modelNames[model])+": playing while frozen changed the held tail by "+num(difference/reference*100,3)+"% of its peak");
        measured+=std::string(model?" | ":"")+modelNames[model]+": held "+num(end-start,1)+" dB over 6.5 s (unfrozen "+num(openEnd-start,1)+" dB), released "+num(released-end,1)+" dB in 5 s";
    }
    std::cout<<"MEASURE reverb controls freeze: "<<measured<<'\n';
    std::cout<<"PASS reverb controls freeze: the tail is held within 6 dB over 6.5 s in every model, new playing is not taken in, and it decays again when released\n";
}
// Moving every control while playing, at their extremes, in mono and stereo: finite and bounded.
void extremes() {
    for(int channels=1;channels<=2;++channels)for(int model=0;model<modelCount;++model) {
        const auto x=noise(4,4,20+unsigned(model),.25f);
        const auto wet=render(model,x,x,[](double t){ReverbControls k;const double p=std::fmod(t*3,1.);
            k.preDelayMs=float(200*p);k.lowCutHz=float(20+480*std::fmod(t*5,1.));k.tone=float(std::sin(t*40));k.width=float(1+std::sin(t*23));k.freeze=std::fmod(t,1.)>.6;return k;},1.f,0.f,channels);
        double peak=0;for(const float v:wet.left)peak=std::max(peak,double(std::abs(v)));
        require(peak<20,std::string(modelNames[model])+": the reverb reached "+num(peak)+" with every control moving");
    }
    std::cout<<"PASS reverb controls extremes: every model stays finite and bounded in mono and stereo with every control moving and freeze switching\n";
}
// Back at the defaults the reverb is the plain one again: after the controls return, a later click gives the same reverb as one that never moved them.
void backToDefaults() {
    for(int model=0;model<modelCount;++model) {
        std::vector<float> x(size_t(rate*8),0.f);x[size_t(rate*.1)]=.5f;x[size_t(rate*6)]=.5f;
        const auto plain=render(model,x,x,fixed({}),.2f);
        const auto moved=render(model,x,x,[](double t){ReverbControls k;if(t<1){k.preDelayMs=80;k.lowCutHz=300;k.tone=.7f;k.width=1.6f;}return k;},.2f);
        double difference=0,reference=0;
        for(size_t n=size_t(rate*6);n<plain.left.size();++n){difference=std::max(difference,double(std::abs(moved.left[n]-plain.left[n])));reference=std::max(reference,double(std::abs(plain.left[n])));}
        // Void Space still carries the first click's tail, which the moved controls changed.
        require(difference<reference*(model==4?.05:1e-3),std::string(modelNames[model])+": after the controls returned to their defaults a new note's reverb differs by "+num(difference/reference*100,3)+"% of its peak");
    }
    std::cout<<"PASS reverb controls back to defaults: once the controls are back, a new note gets the plain reverb\n";
}
// The tempo shape (docs/POST_REVERB_SHAPE.md): the reverb's level follows a shape that repeats with the tempo.
void shapeLevels() {
    const auto at=[](int shape,double p){return double(reverbShapeLevel(shape,p));};
    for(int shape=0;shape<reverbShapeCount;++shape)for(double p=0;p<3;p+=.001){const double v=at(shape,p);require(v>=0 && v<=1,"A shape leaves 0 to 1");require(std::abs(v-at(shape,p-std::floor(p)))<1e-6,"A shape does not repeat");}
    for(double p=0;p<1;p+=.01)require(at(reverbShapeOff,p)==1,"Off is not level");
    require(at(reverbShapeRise,0)==0 && std::abs(at(reverbShapeRise,.5)-.5)<1e-6 && at(reverbShapeRise,.999)>.99,"Rise does not rise from nothing to full");
    require(at(reverbShapeRewind,0)==0 && at(reverbShapeRewind,.5)<.15 && at(reverbShapeRewind,.999)>.99,"Rewind does not rise late, as a decay played backwards");
    require(at(reverbShapeChop,.1)==1 && at(reverbShapeChop,.49)==1 && at(reverbShapeChop,.51)==0 && at(reverbShapeChop,.99)==0,"Chop is not open for the first half");
    require(at(reverbShapeTide,0)==0 && std::abs(at(reverbShapeTide,.5)-1)<1e-6 && std::abs(at(reverbShapeTide,.25)-.5)<1e-6,"Tide is not one rise and fall");
    require(at(reverbShapeDuck,0)==0 && at(reverbShapeDuck,.2)>.6 && at(reverbShapeDuck,.999)>.99,"Duck does not duck at the start and recover");
    for(const int shape:{reverbShapeRise,reverbShapeRewind,reverbShapeDuck})for(double p=.001;p<1;p+=.001)require(at(shape,p)>=at(shape,p-.001),"A rising shape falls");
    std::cout<<"PASS reverb shape levels: Off is level; Rise, Rewind, Chop, Tide and Duck stay within 0 to 1, repeat, and have their form\n";
}
void shapeOnTheReverb() {
    // Steady noise at 120 BPM with a length of one bar: the shape repeats every two seconds.
    const auto steady=noise(8,8,31);
    std::string measured;
    for(int model=0;model<modelCount;++model) {
        if(model==5)continue;   // the Gated Room closes on steady noise: it is covered with bursts below
        const auto with=[&](int shape,float depth=1.f,int length=2){ReverbControls k;k.shape=shape;k.shapeDepth=depth;k.shapeLength=length;return render(model,steady,steady,fixed(k));};
        const auto plain=with(reverbShapeOff);
        // Levels through one bar, against the plain reverb at the same time. render() runs half a second before the
        // playing starts, a quarter of a bar, so a bar starts at 5.5 s.
        const auto part=[&](const Wet& wet,double from,double to){return db(wet.power(5.5+2*from,5.5+2*to))-db(plain.power(5.5+2*from,5.5+2*to));};
        const auto gate=with(reverbShapeChop);
        require(std::abs(part(gate,.05,.45))<1 && part(gate,.55,.95)<-40,std::string(modelNames[model])+": Chop gave "+num(part(gate,.05,.45))+" dB in its open half and "+num(part(gate,.55,.95))+" dB in its closed half");
        const auto swell=with(reverbShapeRise);
        // A shape that ends full and starts empty falls over the 3 ms follower: the first tenth of the bar still holds a little of it.
        require(part(swell,0,.1)<-12 && std::abs(part(swell,.45,.55)-(-6.02))<1.5 && part(swell,.9,1)>-1,std::string(modelNames[model])+": Rise gave "+num(part(swell,0,.1))+", "+num(part(swell,.45,.55))+" and "+num(part(swell,.9,1))+" dB at its start, middle and end");
        const auto reverse=with(reverbShapeRewind);
        require(part(reverse,0,.1)<-12 && part(reverse,.45,.55)<-15 && part(reverse,.9,1)>-4,std::string(modelNames[model])+": Rewind gave "+num(part(reverse,0,.1))+", "+num(part(reverse,.45,.55))+" and "+num(part(reverse,.9,1))+" dB");
        const auto wave=with(reverbShapeTide);
        require(part(wave,0,.05)<-20 && part(wave,.45,.55)>-.5 && part(wave,.95,1)<-20,std::string(modelNames[model])+": Tide gave "+num(part(wave,0,.05))+", "+num(part(wave,.45,.55))+" and "+num(part(wave,.95,1))+" dB");
        const auto pump=with(reverbShapeDuck);
        require(part(pump,.004,.02)<-12 && part(pump,.5,1)>-1.5,std::string(modelNames[model])+": Duck gave "+num(part(pump,.004,.02))+" dB at its start and "+num(part(pump,.5,1))+" dB after");
        // DEPTH: at half, the Chop's closed half is 6 dB down; at none, the reverb is the plain one.
        const auto half=with(reverbShapeChop,.5f),none=with(reverbShapeChop,0.f);
        require(std::abs(part(half,.55,.95)-(-6.02))<.5,std::string(modelNames[model])+": Chop at half depth gave "+num(part(half,.55,.95))+" dB in its closed half");
        double difference=0,reference=0;for(size_t n=size_t(rate);n<plain.left.size();++n){difference=std::max(difference,double(std::abs(none.left[n]-plain.left[n])));reference=std::max(reference,double(std::abs(plain.left[n])));}
        require(difference<reference*1e-4,std::string(modelNames[model])+": a shape at no depth changed the reverb by "+num(difference/reference*100,4)+"% of its peak");
        // LENGTH: at 1/4 the Chop repeats every half second.
        const auto quarter=with(reverbShapeChop,1.f,0);
        require(std::abs(db(quarter.power(6.025,6.225))-db(plain.power(6.025,6.225)))<1 && db(quarter.power(6.275,6.475))-db(plain.power(6.275,6.475))<-40 && std::abs(db(quarter.power(6.525,6.725))-db(plain.power(6.525,6.725)))<1,
                std::string(modelNames[model])+": Chop at a length of 1/4 does not open and close every half second");
        measured+=std::string(measured.empty()?"":" | ")+modelNames[model]+": Chop "+num(part(gate,.05,.45),1)+"/"+num(part(gate,.55,.95),0)+" dB, Rise "+num(part(swell,0,.1),0)+"/"+num(part(swell,.45,.55),1)+"/"+num(part(swell,.9,1),1)+" dB";
    }
    std::cout<<"MEASURE reverb shape at 120 BPM, one bar (Chop: open / closed half; Rise: start / middle / end, against the plain reverb): "<<measured<<'\n';
    // The Gated Room, with bursts: its own gate and the shape's both apply.
    {auto bursts=noise(8,8,32);for(size_t n=0;n<bursts.size();++n)if(n%size_t(rate*.25)>=size_t(rate*.1))bursts[n]=0;
     ReverbControls k;k.shape=reverbShapeChop;const auto plain=render(5,bursts,bursts,fixed({})),gated=render(5,bursts,bursts,fixed(k));
     require(std::abs(db(gated.power(5.6,6.4))-db(plain.power(5.6,6.4)))<1 && db(gated.power(6.6,7.4))-db(plain.power(6.6,7.4))<-40,"Gated Room: the shape's gate does not apply");}
    // Locked to the host: with the host half a bar in when the playing starts, the Chop's closed half comes first.
    {const int model=0;ReverbControls k;k.shape=reverbShapeChop;
     const auto locked=render(model,steady,steady,[k](double t)mutable{k.ppq=2.+t*2.;return k;}),plain=render(model,steady,steady,fixed({}));
     require(db(locked.power(6.1,6.9))-db(plain.power(6.1,6.9))<-40 && std::abs(db(locked.power(7.1,7.9))-db(plain.power(7.1,7.9)))<1,"The shape does not follow the host's position");
     // Before the start of the song the position is negative (a count-in): the shape is locked there too, on the same bars.
     const auto before=render(model,steady,steady,[k](double t)mutable{k.ppq=2.+t*2.-32.;return k;});
     require(db(before.power(6.1,6.9))-db(plain.power(6.1,6.9))<-40 && std::abs(db(before.power(7.1,7.9))-db(plain.power(7.1,7.9)))<1,"The shape does not follow the host's position before the start of the song");}
    // Switched off again, a later note gets the plain reverb.
    for(int model=0;model<modelCount;++model) {
        std::vector<float> x(size_t(rate*8),0.f);x[size_t(rate*.1)]=.5f;x[size_t(rate*6)]=.5f;
        const auto plain=render(model,x,x,fixed({}),.2f);
        const auto moved=render(model,x,x,[](double t){ReverbControls k;if(t<1)k.shape=reverbShapeTide;return k;},.2f);
        double difference=0,reference=0;for(size_t n=size_t(rate*6);n<plain.left.size();++n){difference=std::max(difference,double(std::abs(moved.left[n]-plain.left[n])));reference=std::max(reference,double(std::abs(plain.left[n])));}
        require(difference<reference*(model==4?.05:1e-3),std::string(modelNames[model])+": after the shape was switched off a new note's reverb differs by "+num(difference/reference*100,3)+"% of its peak");
    }
    std::cout<<"PASS reverb shape on the reverb: each shape gives its level through the bar in every model; depth scales it, length sets its period, the host's position locks it, and Off returns the plain reverb\n";
}
}
// More of the tempo shape (docs/POST_REVERB_SHAPE_MORE.md): seconds instead of bars, the curve, the tone morph.
void shapeMore() {
    // CURVE: nothing at 0, the ends kept, rising with the level; hard pushes away from the middle, soft towards it.
    for(float v=0;v<=1.f;v+=.001f)require(reverbShapeCurved(v,0.f)==v,"CURVE at 0 changes a level");
    for(const float curve:{-1.f,-.4f,.4f,1.f}) {
        require(reverbShapeCurved(0.f,curve)==0.f && reverbShapeCurved(1.f,curve)==1.f && std::abs(reverbShapeCurved(.5f,curve)-.5f)<1e-6f,"CURVE moves the ends or the middle of a shape");
        for(float v=.001f;v<=1.f;v+=.001f)require(reverbShapeCurved(v,curve)>=reverbShapeCurved(v-.001f,curve),"CURVE makes a rising level fall");
        require(curve>0?reverbShapeCurved(.25f,curve)<.25f && reverbShapeCurved(.75f,curve)>.75f:reverbShapeCurved(.25f,curve)>.25f && reverbShapeCurved(.75f,curve)<.75f,"CURVE bends the wrong way");
    }
    const auto steady=noise(9,9,21);std::string measured;
    for(int model=0;model<5;++model) {
        const auto plain=render(model,steady,steady,fixed({}));
        const auto against=[&](const Wet& w,double from,double to){return db(w.power(from,to))-db(plain.power(from,to));};
        // SECONDS: a Chop one second long, whatever the tempo and the host's position. The half second render() runs first is half a pass.
        ReverbControls k;k.shape=reverbShapeChop;k.shapeInSeconds=true;k.shapeSeconds=1.f;
        const auto slow=render(model,steady,steady,[k](double){auto c=k;c.tempo=60;return c;}),fast=render(model,steady,steady,[k](double t){auto c=k;c.tempo=180;c.ppq=t*3;return c;});
        require(slow.left==fast.left && slow.right==fast.right,std::string(modelNames[model])+": a shape in seconds follows the tempo or the host's position");
        require(std::abs(against(slow,5.6,5.9))<1 && against(slow,5.1,5.4)<-40 && std::abs(against(slow,6.6,6.9))<1 && against(slow,6.1,6.4)<-40,std::string(modelNames[model])+": a Chop one second long does not open and close each second");
        // CURVE on the reverb: a hard Rise is lower than the Rise early in the bar and no lower late in it; a soft one the other way.
        ReverbControls swell;swell.shape=reverbShapeRise;auto hard=swell;hard.shapeCurve=1;auto soft=swell;soft.shapeCurve=-1;
        const auto drawn=render(model,steady,steady,fixed(swell)),harder=render(model,steady,steady,fixed(hard)),softer=render(model,steady,steady,fixed(soft));
        const double early=db(harder.power(5.8,6.0))-db(drawn.power(5.8,6.0)),earlySoft=db(softer.power(5.8,6.0))-db(drawn.power(5.8,6.0)),late=db(harder.power(7.0,7.2))-db(drawn.power(7.0,7.2));
        require(early<-6 && earlySoft>3 && late>-.5,std::string(modelNames[model])+": CURVE does not bend the Rise (hard "+num(early,1)+" dB and soft "+num(earlySoft,1)+" dB early in the bar, hard "+num(late,1)+" dB late)");
        measured+=std::string(model?" | ":"")+modelNames[model]+": hard "+num(early,1)+" dB, soft +"+num(earlySoft,1)+" dB early";
    }
    std::cout<<"MEASURE reverb shape curve (Rise, 120 BPM, one bar, against the Rise as drawn): "<<measured<<'\n';
    // MORPH: with a Chop at no depth the level stays and only the tone follows: bright in the open half, dark in the closed one.
    measured.clear();
    for(int model=0;model<5;++model) {
        // Noise in, and the reverb's level above 4 kHz and below 300 Hz: a steady tone would beat in the models that are modulated.
        ReverbControls k;k.shape=reverbShapeChop;k.shapeDepth=0;k.shapeMorph=1;auto other=k;other.shapeMorph=-1;
        const auto band=[](const Wet& w,bool top,double from,double to){const double pole=1-std::exp(-2*pi*(top?4000.:300.)/rate);double z=0,e=0;
            for(size_t n=size_t((from-.2)*rate);n<size_t(to*rate);++n){z+=pole*(w.left[n]-z);const double v=top?w.left[n]-z:z;if(n>=size_t(from*rate))e+=v*v;}return db(e);};
        const auto step=[&](bool top,const ReverbControls& c){const auto w=render(model,steady,steady,fixed(c));return band(w,top,5.8,6.3)-band(w,top,6.8,7.3);};
        const double top=step(true,k),bottom=step(false,k),topOther=step(true,other);
        require(top>4 && bottom<-4 && topOther<-4,std::string(modelNames[model])+": MORPH does not move the tone with the shape (open half against closed: above 4 kHz "+num(top,1)+" dB, below 300 Hz "+num(bottom,1)+" dB, above 4 kHz the other way "+num(topOther,1)+" dB)");
        measured+=std::string(model?" | ":"")+modelNames[model]+": above 4 kHz "+num(top,1)+" dB, below 300 Hz "+num(bottom,1)+" dB";
        // With the shape Off, MORPH, CURVE and the seconds are not there.
        ReverbControls off;off.shapeMorph=1;off.shapeCurve=1;off.shapeInSeconds=true;off.shapeSeconds=.3f;
        const auto burst=noise(3,.05,22);const auto plain=render(model,burst,burst,fixed({})),same=render(model,burst,burst,fixed(off));
        require(plain.left==same.left && plain.right==same.right,std::string(modelNames[model])+": with the shape Off its other controls changed the reverb");
    }
    std::cout<<"MEASURE reverb shape morph (Chop at no depth; open half against closed half): "<<measured<<'\n';
    std::cout<<"PASS more of the reverb shape: a length in seconds ignores the tempo and the host; CURVE bends the level and keeps its ends; MORPH moves the tone with the shape; none of them is there with the shape Off\n";
}
// DECAY (docs/POST_REVERB_DECAY.md): the time the reverb takes to fall 60 dB, whatever SIZE is.
// Measured below 300 Hz, where DAMPING does not shorten the tail, from the backward-integrated decay after a
// 50 ms burst of noise, fitted from -5 to -25 dB.
double decayTime(const Wet& wet) {
    const double pole=1-std::exp(-2*pi*300/rate);double low=0;std::vector<double> back(wet.left.size());
    for(size_t n=0;n<wet.left.size();++n){low+=pole*(wet.left[n]-low);back[n]=low*low;}
    const size_t start=size_t(.1*rate);double sum=0;for(size_t n=back.size();n-->start;){sum+=back[n];back[n]=sum;}
    double at5=-1;for(size_t n=start;n<back.size();++n){const double level=db(back[n]/back[start]);if(at5<0 && level<=-5)at5=double(n)/rate;if(level<=-25)return (double(n)/rate-at5)*3;}
    return -1;
}
void decay() {
    std::string measured;
    for(int model=0;model<5;++model) {
        measured+=std::string(model?" | ":"")+modelNames[model]+":";
        for(const float seconds:{.5f,2.f,6.f}) {
            const auto burst=noise(double(seconds)*2.5+2,.05,11);ReverbControls k;k.decaySeconds=seconds;
            const double small=decayTime(render(model,burst,burst,fixed(k),.2f)),large=decayTime(render(model,burst,burst,fixed(k),.8f));
            require(small>0 && large>0,std::string(modelNames[model])+": the reverb did not fall 25 dB in the time rendered at DECAY "+num(seconds,1)+" s");
            require(std::abs(small/seconds-1)<.2 && std::abs(large/seconds-1)<.2,std::string(modelNames[model])+": DECAY "+num(seconds,1)+" s measured "+num(small)+" s at a small SIZE and "+num(large)+" s at a large one");
            measured+=" "+num(seconds,1)+" s -> "+num(small)+" / "+num(large)+" s";
        }
        // Below its minimum DECAY is not there: the render is the one SIZE alone gives, sample for sample.
        const auto burst=noise(3,.05,12);ReverbControls under;under.decaySeconds=reverbDecayMinSeconds*.99f;
        const auto plain=render(model,burst,burst,fixed({})),same=render(model,burst,burst,fixed(under));
        require(plain.left==same.left && plain.right==same.right,std::string(modelNames[model])+": DECAY below its minimum changed the reverb");
        // And SIZE alone still sets the decay then.
        const double shortTail=decayTime(render(model,noise(30,.05,13),noise(30,.05,13),fixed({}),.1f)),longTail=decayTime(render(model,noise(30,.05,13),noise(30,.05,13),fixed({}),.9f));
        require(shortTail>0 && longTail>shortTail*1.2,std::string(modelNames[model])+": without DECAY, SIZE no longer sets the decay");
    }
    std::cout<<"MEASURE reverb decay (set -> measured below 300 Hz at SIZE 0.2 / 0.8): "<<measured<<'\n';
    // The Gated Room: its gate stays open for the decay time. Bursts a second long, three seconds apart.
    {std::vector<float> bursts(size_t(rate*9),0.f);std::mt19937 rng{14};std::normal_distribution<float> g(0,.1f);for(int burst=0;burst<3;++burst)for(size_t n=0;n<size_t(rate*.05);++n)bursts[size_t(rate*3*burst)+n]=g(rng);
     ReverbControls k;k.decaySeconds=1.5f;const auto timed=render(5,bursts,bursts,fixed(k));
     const double open=db(timed.power(3.9,4.4)),closed=db(timed.power(4.7,5.2));
     require(open>-90 && closed<open-40,"Gated Room: the gate does not stay open for the DECAY time (level "+num(open,1)+" dB before it ends, "+num(closed,1)+" dB after)");}
    // Moving DECAY while a tail rings, every model, mono and stereo: finite (render checks) and bounded.
    for(int model=0;model<modelCount;++model)for(const int channels:{1,2}) {
        const auto x=noise(6,3,15);const auto moved=render(model,x,x,[](double t){ReverbControls k;k.decaySeconds=float(10+10*std::sin(2*pi*1.3*t));return k;},.5f,.55f,channels);
        double peak=0;for(const float v:moved.left)peak=std::max(peak,double(std::abs(v)));
        require(peak<20,std::string(modelNames[model])+": moving DECAY made the reverb run away (peak "+num(peak,1)+")");
    }
    std::cout<<"PASS reverb decay: the set time is the measured one in the five decaying models whatever SIZE is; the Gated Room's gate follows it; below its minimum the render is unchanged; moving it stays bounded\n";
}
int main(int argc,char** argv) {
    try {
        const std::string only=argc>1?argv[1]:"";
        if(only.empty() || only=="--pre-delay")preDelay();
        if(only.empty() || only=="--tone")lowCutAndTone();
        if(only.empty() || only=="--width")width();
        if(only.empty() || only=="--freeze")freeze();
        if(only.empty() || only=="--extremes")extremes();
        if(only.empty() || only=="--defaults")backToDefaults();
        if(only.empty() || only=="--shape"){shapeLevels();shapeOnTheReverb();}
        if(only.empty() || only=="--decay")decay();
        if(only.empty() || only=="--shape-more")shapeMore();
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

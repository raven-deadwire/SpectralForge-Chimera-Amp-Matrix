#include "OriginalAmpDSP.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace { bool watch=false;std::size_t allocations=0; }
void* operator new(std::size_t n) {if(watch)++allocations;if(auto* p=std::malloc(std::max(n,std::size_t{1})))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace spectralforge::original;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
float stimulus(int n,double rate) {const double t=n/rate;return float(.04*std::exp(-std::fmod(t,.19)*9)*(std::sin(2*detail::pi*73.416*t)+.4*std::sin(2*detail::pi*220*t)+.2*std::sin(2*detail::pi*997*t)));}
std::vector<float> render(State state,int count=24000) {
    OriginalAmpDSP dsp;dsp.prepare(48000);dsp.set(state);dsp.reset();std::vector<float> out(std::size_t(count),0.f);
    for(int n=0;n<count;++n)out[std::size_t(n)]=dsp.tick(stimulus(n,48000),0);
    return out;
}
double rms(const std::vector<float>& x) {double e=0;for(auto v:x)e+=double(v)*v;return std::sqrt(e/x.size());}
double matchedDifference(const std::vector<float>& a,const std::vector<float>& b) {
    const double ar=rms(a),br=rms(b);double e=0;
    for(std::size_t n=0;n<a.size();++n)e+=std::pow(a[n]/ar-b[n]/br,2);
    return std::sqrt(e/a.size());
}
void defaults() {
    OriginalAmpDSP dsp;constexpr int rate=192000;std::array<double,3> energy{};double thd=0;
    for(int level=0;level<3;++level) {
        dsp.prepare(rate);dsp.reset();std::array<double,13> re{},im{};
        for(int n=0;n<rate;++n) {
            const double phase=2*detail::pi*400*n/rate;
            const float y=dsp.tick(float(std::pow(10.,(-48+level*12)/20.)*std::sin(phase)),0);
            if(n>=rate/2 && n%4==0){energy[std::size_t(level)]+=double(y)*y;
                if(level==1)for(int h=1;h<=12;++h){re[std::size_t(h)]+=y*std::cos(h*phase);im[std::size_t(h)]+=y*std::sin(h*phase);}}
        }
        if(level==1){double e=0;for(int h=2;h<=12;++h)e+=re[std::size_t(h)]*re[std::size_t(h)]+im[std::size_t(h)]*im[std::size_t(h)];thd=std::sqrt(e/(re[1]*re[1]+im[1]*im[1]));}
    }
    const double growth=10*std::log10(energy[2]/energy[0]);
    std::cout<<"DEFAULT_GAIN weak_thd="<<thd<<" growth_db="<<growth<<'\n';
    require(thd>.28 && growth>0 && growth<6,"Default must produce high gain on weak input independently of output volume");
    State low,high;low[Control::gain]=.08f;high[Control::gain]=.72f;
    require(matchedDifference(render(low),render(high))>.05,"Gain only changes volume");
}
void controlsAndPresets() {
    std::array<std::vector<float>,5> derivatives;
    for(std::size_t i=0;i<controlCount;++i) {
        State lo,hi;lo.values[i]=controls[i].minimum;hi.values[i]=controls[i].maximum;
        const auto a=render(lo),b=render(hi);
        if(i==std::size_t(Control::master)||i==std::size_t(Control::gain)){require(rms(a)<1.e-10&&rms(b)>.005,"Closed gain/master does not mute");continue;}
        const double difference=matchedDifference(a,b);
        std::cout<<"CONTROL "<<controls[i].id<<" matched_residual="<<difference<<'\n';
        require(difference>.002,"Control has no level-independent response");
        if(i>=8){const double ar=rms(a),br=rms(b);auto& d=derivatives[i-8];d.resize(a.size());for(std::size_t n=0;n<a.size();++n)d[n]=float(b[n]/br-a[n]/ar);}
    }
    for(std::size_t a=0;a<5;++a)for(std::size_t b=a+1;b<5;++b) {
        double dot=0,aa=0,bb=0;for(std::size_t n=0;n<derivatives[a].size();++n){const auto x=derivatives[a][n],y=derivatives[b][n];dot+=double(x)*y;aa+=double(x)*x;bb+=double(y)*y;}
        const double cosine=dot/std::sqrt(aa*bb);
        std::cout<<"MACRO_RESPONSE "<<controls[a+8].id<<'/'<<controls[b+8].id<<" cosine="<<cosine<<'\n';
        require(std::abs(cosine)<.995,"Two macros collapse to the same normalized response");
    }
    for(std::size_t a=0;a<presets.size();++a)for(std::size_t b=a+1;b<presets.size();++b)
        require(matchedDifference(render(presets[a].state),render(presets[b].state))>.005,"Original presets are only different labels/levels");
}
void macroVoicing() {
    // Check audible properties independently of raw output level. The two
    // inputs differ by 20 dB; neither relies on one envelope-detector threshold.
    constexpr int rate=192000,window=rate/2;
    struct Features {double fundamental{},evenOdd{},tail{};};
    for(auto macro:{Control::impact,Control::rot,Control::bloom})for(double input:{.004,.04}) {
        std::array<Features,2> measured{};
        for(int knob=0;knob<2;++knob) {
            State state;state[macro]=float(knob);OriginalAmpDSP dsp;dsp.prepare(rate);dsp.set(state);dsp.reset();
            std::array<double,13> re{},im{};double energy=0,tail=0;
            for(int n=0;n<rate*3/2;++n) {
                const double phase=2*detail::pi*(macro==Control::rot?400:80)*n/rate;
                double x=input*std::sin(phase);
                if(macro!=Control::rot)x+=input*.4*std::sin(2*detail::pi*2000*n/rate);
                const double y=dsp.tick(n<rate?float(x):0,0);
                if(n>=window&&n<rate){energy+=y*y;
                    for(int h=1;h<=12;++h){re[std::size_t(h)]+=y*std::cos(h*phase);im[std::size_t(h)]+=y*std::sin(h*phase);}}
                if(n>=rate+rate/20&&n<rate+3*rate/20)tail+=y*y;
            }
            double odd=0,even=0;
            for(int h=2;h<=12;++h)(h%2?odd:even)+=re[std::size_t(h)]*re[std::size_t(h)]+im[std::size_t(h)]*im[std::size_t(h)];
            measured[std::size_t(knob)]={std::sqrt((re[1]*re[1]+im[1]*im[1])/(energy*window)),
                                       std::sqrt(even/odd),std::sqrt(5*tail/energy)};
        }
        const auto& lo=measured[0];const auto& hi=measured[1];
        std::cout<<"VOICING "<<controls[std::size_t(macro)].id<<" input="<<input
                 <<" fundamental_ratio="<<hi.fundamental/lo.fundamental
                 <<" even_odd_ratio="<<hi.evenOdd/lo.evenOdd<<" tail="<<hi.tail<<'\n';
        if(macro==Control::impact)require(hi.fundamental>lo.fundamental*1.2,"IMPACT lost LF weight after saturation");
        if(macro==Control::rot)require(hi.evenOdd>lo.evenOdd*2,"ROT lost asymmetric harmonic response");
        if(macro==Control::bloom){
            require(hi.fundamental>lo.fundamental*1.4,"BLOOM lost broad LF response");
            require(hi.tail>lo.tail*2&&hi.tail<.006,"BLOOM lost decay or leaves a dominant ringing tail");
        }
    }
}
void realtimeAndState() {
    float peak=0;
    for(double rate:{44100.,48000.,96000.})for(int os:{1,2,4,8}) {
        OriginalAmpDSP dsp;dsp.prepare(rate*os);State state;
        watch=true;
        for(int n=0;n<20000;++n) {
            if(n%4000==0){state.values[std::size_t(n/4000)+8]=float((n/4000)%2);dsp.set(state);}
            const float left=dsp.tick(stimulus(n,rate*os),0),right=dsp.tick(0,1);
            if(!std::isfinite(left)||std::abs(left)>4||std::abs(right)>1.e-10f){watch=false;throw std::runtime_error("Nonfinite/unsafe output or stereo crosstalk");}
            peak=std::max(peak,std::abs(left));
        }
        dsp.reset();watch=false;
        for(int n=0;n<512;++n)require(dsp.tick(0,0)==0,"Reset carries stale audio");
    }
    require(allocations==0,"set/reset/tick allocated");
    for(int corner=0;corner<3;++corner) {
        State state;for(std::size_t i=0;i<controlCount;++i)state.values[i]=corner==0?controls[i].maximum:corner==1?controls[i].minimum:(i%2?controls[i].maximum:controls[i].minimum);
        state[Control::gain]=1;state[Control::master]=1;
        OriginalAmpDSP extreme;extreme.prepare(48000);extreme.set(state);extreme.reset();
        float tail=0;
        for(int n=0;n<144000;++n) {
            const float y=extreme.tick(n<48000?8*stimulus(n,48000):0,0);
            require(std::isfinite(y)&&std::abs(y)<4,"Extreme controls exceed headroom");
            if(n>140000)tail=std::max(tail,std::abs(y));
        }
        require(tail<.001f,"Bias/blocking/bloom memory does not decay to silence");
    }
    State bad;bad.values.fill(std::numeric_limits<float>::quiet_NaN());OriginalAmpDSP dsp;dsp.prepare(48000);dsp.set(bad);dsp.reset();
    require(dsp.state()==State{},"Nonfinite controls do not recover defaults");
    require(std::isfinite(dsp.tick(std::numeric_limits<float>::infinity(),0)),"Nonfinite input poisons state");
    const auto a=render(State{}),b=render(State{});require(a==b,"Fresh-instance render is nondeterministic");
    std::cout<<"REALTIME 12 rate/factor routes peak="<<peak<<" allocations="<<allocations<<'\n';
}
void movingPowerVoicing() {
    // Retarget all five macros before their 20 ms ramps finish. This exercises
    // the newly moving power filters with live filter state, not just endpoints.
    for(double rate:{44100.,48000.,96000.})for(int os:{1,2,4,8}) {
        const double processingRate=rate*os;
        OriginalAmpDSP dsp;dsp.prepare(processingRate);State state;
        const int count=int(processingRate*.12),period=std::max(1,int(processingRate*.007));
        watch=true;
        for(int n=0;n<count;++n) {
            if(n%period==0){
                state.channel=(n/period)%channelCount;state.modern=true;
                for(std::size_t i=8;i<controlCount;++i)
                    state.values[i]=float(((n/period)+int(i))%2);
                dsp.set(state);
            }
            const float y=dsp.tick(4*stimulus(n,processingRate),0);
            if(!std::isfinite(y)||std::abs(y)>=4){watch=false;throw std::runtime_error("Moving power voicing became unstable");}
        }
        dsp.set(State{});dsp.reset();watch=false;
        for(int n=0;n<512;++n)require(dsp.tick(0,0)==0,"Power-voicing filters retain audio after reset");
    }
    require(allocations==0,"Moving power voicing allocated");
    std::cout<<"PASS simultaneous macro retargeting at 12 rate/factor routes; allocations="<<allocations<<'\n';
}
void channelVoicing() {
    std::array<std::vector<float>,channelCount> audio;
    for(int channel=0;channel<channelCount;++channel) {
        auto midpoint=channelState(channel);audio[std::size_t(channel)]=render(midpoint);
        require(midpoint[Control::gain]==.5f && midpoint[Control::master]==.5f,"Channel operating point must leave upward knob travel");
        auto upper=midpoint;upper[Control::gain]=1;
        const double travel=matchedDifference(audio[std::size_t(channel)],render(upper));
        std::cout<<"CHANNEL "<<channelNames[channel]<<" upper_travel_residual="<<travel<<'\n';
        require(travel>.015,"Upper half of gain/crush is only output level or already exhausted");
        for(std::size_t macro=8;macro<controlCount;++macro) {
            auto low=midpoint,high=midpoint;low.values[macro]=0;high.values[macro]=1;
            require(matchedDifference(render(low),render(high))>.01,"Channel macro has no level-independent travel");
        }
        auto extreme=midpoint;for(std::size_t i=0;i<controlCount;++i)extreme.values[i]=controls[i].maximum;
        OriginalAmpDSP dsp;dsp.prepare(48000);dsp.set(extreme);dsp.reset();float tail=0;
        for(int n=0;n<144000;++n) {
            const float y=dsp.tick(n<48000?8*stimulus(n,48000):0,0);
            require(std::isfinite(y)&&std::abs(y)<4,"Channel maximum is unbounded");
            if(n>140000)tail=std::max(tail,std::abs(y));
        }
        require(tail<.001f,"Channel maximum does not release to silence");
    }
    for(int a=0;a<channelCount;++a)for(int b=a+1;b<channelCount;++b)
        require(matchedDifference(audio[std::size_t(a)],audio[std::size_t(b)])>.25,"Same-knob channels collapse to a weak variant after level matching");
}
void drivenChannelSeparation() {
    // The old 2% residual check admitted channels the owner could barely
    // distinguish. Check level-matched low chords even after a strong common
    // drive stage, at two input levels; never substitute output gain for voice.
    constexpr int rate=192000,count=rate/2;
    for(float level:{.012f,.12f}) {
        std::array<std::vector<float>,channelCount> audio;
        for(int channel=0;channel<channelCount;++channel) {
            OriginalAmpDSP dsp;dsp.prepare(rate);dsp.set(channelState(channel));dsp.reset();
            auto& out=audio[size_t(channel)];out.reserve(count/4);
            for(int n=0;n<count;++n) {
                const double t=double(n)/rate,local=std::fmod(t,.25),envelope=local<.16?std::exp(-18*local):0;
                const double chord=std::sin(2*detail::pi*61.735*t)+.5*std::sin(2*detail::pi*92.499*t)+.35*std::sin(2*detail::pi*155.56*t);
                const float y=dsp.tick(float(.2*std::tanh(20*level*envelope*chord)),0);
                require(std::isfinite(y),"Driven channel output is nonfinite");
                if(n%4==0)out.push_back(y);
            }
        }
        double minimum=10;
        for(int a=0;a<channelCount;++a)for(int b=a+1;b<channelCount;++b)
            minimum=std::min(minimum,matchedDifference(audio[size_t(a)],audio[size_t(b)]));
        std::cout<<"DRIVEN_CHANNEL_SEPARATION input="<<level<<" minimum_matched_residual="<<minimum<<'\n';
        require(minimum>.25,"Strong common PRE drive erased the channel distinction");
    }
}
int main(){try{defaults();controlsAndPresets();macroVoicing();realtimeAndState();movingPowerVoicing();channelVoicing();drivenChannelSeparation();std::cout<<"PASS OriginalAmp development core; musical/reference acceptance remains pending\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace spectralforge::tuning {
// Worker-only monophonic estimator. The audio callback only fills the SPSC queue.
// YIN chooses the fundamental; a long-baseline fractional delay refines it.
inline std::pair<float,float> estimate(const float* x,int count,double rate) {
    constexpr int capacity=8192;
    if(!x || count<64 || count>capacity || !std::isfinite(rate) || rate<4000) return {};
    double power=0,mean=0;
    for(int i=0;i<count;++i){if(!std::isfinite(x[i]))return {};mean+=x[i];power+=double(x[i])*x[i];}
    if(power/count-(mean/count)*(mean/count)<1e-9)return {};
    const int window=count/2,minimum=std::max(2,int(rate/2000.0));
    const int maximum=std::min(window-8,int(std::ceil(rate/16.0)));
    std::array<double,capacity/2> difference{},raw{};
    double sum=0;int selected=0;
    for(int lag=1;lag<=maximum;++lag){
        double delta=0;
        for(int n=0;n<window;++n){const double d=double(x[n])-x[n+lag];delta+=d*d;}
        raw[size_t(lag)]=delta;sum+=delta;difference[size_t(lag)]=sum>0?delta*lag/sum:1;
        if(lag>minimum && difference[size_t(lag-1)]<.12 && difference[size_t(lag)]>difference[size_t(lag-1)]){selected=lag-1;break;}
    }
    if(!selected)return {};
    const double a=raw[size_t(selected-1)],b=raw[size_t(selected)],c=raw[size_t(selected+1)];
    const double denominator=a-2*b+c;
    const double refinement=std::abs(denominator)>1e-20?.5*(a-c)/denominator:0;
    const int periods=std::clamp(int((window-12)/(selected+.5)),1,4);
    const auto cost=[&](double period){
        const double delay=period*periods;const int base=int(delay);const double t=delay-base;
        std::array<double,8> weights{};
        for(int k=0;k<8;++k){double w=1;for(int j=0;j<8;++j)if(k!=j)w*=(t-(j-3))/double(k-j);weights[size_t(k)]=w;}
        double error=0;
        const int end=std::min(window,count-base-5);
        for(int n=3;n<end;++n){double shifted=0;for(int k=0;k<8;++k)shifted+=weights[size_t(k)]*x[n+base+k-3];const double d=x[n]-shifted;error+=d*d;}
        return error/std::max(1,end-3);
    };
    double left=std::max(4.0,selected+refinement-.6),right=selected+refinement+.6;
    // Golden-section search retains one observation at each subdivision.
    // Evaluate 28 costs instead of recomputing both sides 52 times.
    constexpr double fraction=.3819660112501051;
    double l=left+(right-left)*fraction,r=right-(right-left)*fraction;
    double lc=cost(l),rc=cost(r);
    for(int i=0;i<26;++i){
        if(lc<rc){right=r;r=l;rc=lc;l=left+(right-left)*fraction;lc=cost(l);}
        else {left=l;l=r;lc=rc;r=right-(right-left)*fraction;rc=cost(r);}
    }
    const double hz=rate/((left+right)*.5);
    if(hz<16 || hz>2000)return {};
    return {float(hz),float(1-difference[size_t(selected)])};
}

enum class Profile { equal, ttChromatic, tt6, tt7, tt8, feitenElectric, feitenElectric12, custom };
inline constexpr std::array<const char*,8> profileNames{{"Equal temperament","True Temperament / chromatic","True Temperament / 6 strings","True Temperament / 7 strings","True Temperament / 8 strings","Buzz Feiten / electric open","Buzz Feiten / electric 12th fret","Custom / per string"}};
inline constexpr std::array<int,8> openMidi{{64,59,55,50,45,40,35,30}}; // high to low
inline constexpr std::array<double,8> ttOpen{{-1,-1,4,2,0,-2,-4,-8}};
// Primary sources and exact scope are recorded in docs/PRECISION_TUNER.md.
inline constexpr std::array<double,12> ttChromatic{{2,-4,2,-4,-2,0,-4,4,-4,0,-4,-1}};
inline constexpr std::array<double,6> feitenOpen{{0,1,-2,-2,-2,-2}},feiten12{{0,0,1,1,0,0}};
struct Settings {
    Profile profile{Profile::equal};double reference{440};int stringIndex{-1};
    std::array<int,8> customMidi{openMidi};std::array<double,8> customCents{};
};
struct Target {int midi{},stringIndex{-1};double offset{},hz{},cents{};bool valid{};};
inline double frequency(int midi,double reference,double cents=0){return reference*std::exp2((midi-69+cents/100)/12.0);}
inline int strings(Profile p){return p==Profile::tt8||p==Profile::custom?8:p==Profile::tt7?7:p==Profile::equal||p==Profile::ttChromatic?0:6;}
inline Target target(double hz,const Settings& s){
    if(!std::isfinite(hz)||hz<=0||!std::isfinite(s.reference)||s.reference<390||s.reference>490)return {};
    Target result;double distance=1e9;
    const int count=strings(s.profile);
    if(!count){result.midi=int(std::round(69+12*std::log2(hz/s.reference)));result.offset=s.profile==Profile::ttChromatic?ttChromatic[size_t((result.midi%12+12)%12)]:0;}
    else for(int i=0;i<count;++i){
        if(s.stringIndex>=0 && i!=s.stringIndex)continue;
        const int midi=s.profile==Profile::custom?s.customMidi[size_t(i)]:openMidi[size_t(i)]+(s.profile==Profile::feitenElectric12?12:0);
        const double offset=s.profile==Profile::custom?s.customCents[size_t(i)]:s.profile==Profile::feitenElectric?feitenOpen[size_t(i)]:s.profile==Profile::feitenElectric12?feiten12[size_t(i)]:ttOpen[size_t(i)];
        const double d=std::abs(1200*std::log2(hz/frequency(midi,s.reference,offset)));
        if(d<distance){distance=d;result.midi=midi;result.offset=offset;result.stringIndex=i;}
    }
    if(count && (result.stringIndex<0 || (s.stringIndex<0 && distance>150)))return {};
    result.hz=frequency(result.midi,s.reference,result.offset);result.cents=1200*std::log2(hz/result.hz);result.valid=true;return result;
}
// Beat-frequency-driven virtual strobe: no dead band or snapped "in tune" phase.
inline double advanceStrobe(double phase,double measured,double targetHz,double seconds,int harmonic=1){
    if(!std::isfinite(measured)||!std::isfinite(targetHz)||!std::isfinite(seconds)||seconds<=0)return phase;
    const double next=phase+(measured-targetHz)*seconds*harmonic;return next-std::floor(next);
}
}

// Frozen PR #67 36e41a50a9179b52eba21c3e8d5f99d0aad5bbef for paired cost evidence.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace baselineTuningPR67 {
// Worker-only monophonic estimator; the callback only writes the SPSC queue.
// YIN supplies a coarse period. Gain/DC-normalized fractional-delay correlation
// refines it without interpreting an ordinary decay envelope as pitch change.
inline std::pair<float,float> estimate(const float* x,int count,double rate) {
    constexpr int capacity=8192;
    if(!x || count<64 || count>capacity || !std::isfinite(rate) || rate<4000) return {};
    double power=0,mean=0;
    for(int i=0;i<count;++i){
        if(!std::isfinite(x[i]))return {};
        mean+=x[i];power+=double(x[i])*x[i];
    }
    if(power/count-(mean/count)*(mean/count)<1e-9)return {};
    const int window=count/2,minimum=std::max(2,int(rate/2000.0));
    const int maximum=std::min(window-8,int(std::ceil(rate/16.0)));
    std::array<double,capacity/2> difference{},raw{};
    double sum=0;int selected=0;
    for(int lag=1;lag<=maximum;++lag){
        double delta=0;
        for(int n=0;n<window;++n){const double d=double(x[n])-x[n+lag];delta+=d*d;}
        raw[size_t(lag)]=delta;sum+=delta;
        difference[size_t(lag)]=sum>0?delta*lag/sum:1;
        if(lag>minimum && difference[size_t(lag-1)]<.12 && difference[size_t(lag)]>difference[size_t(lag-1)]){selected=lag-1;break;}
    }
    if(!selected)return {};
    const double a=raw[size_t(selected-1)],b=raw[size_t(selected)],c=raw[size_t(selected+1)];
    const double denominator=a-2*b+c;
    const double correction=std::abs(denominator)>1e-20?.5*(a-c)/denominator:0;
    const auto refine=[&](double coarse,int periods){
        const double radius=std::max(.75,coarse*.02);
        double left=std::max(4.0,coarse-radius),right=coarse+radius;
        // Common support prevents the objective jumping as integer delay changes.
        const int end=std::min(window,count-int(std::ceil(right*periods))-5);
        const auto cost=[&](double period){
            const double delay=period*periods;
            const int base=int(delay);const double t=delay-base;
            std::array<double,8> weights{};
            for(int k=0;k<8;++k){
                double w=1;
                for(int j=0;j<8;++j)if(k!=j)w*=(t-(j-3))/double(k-j);
                weights[size_t(k)]=w;
            }
            double ab=0,aa=0,bb=0,asum=0,bsum=0;
            for(int n=3;n<end;++n){
                double shifted=0;
                for(int k=0;k<8;++k)shifted+=weights[size_t(k)]*x[n+base+k-3];
                const double sample=x[n];ab+=sample*shifted;aa+=sample*sample;
                bb+=shifted*shifted;asum+=sample;bsum+=shifted;
            }
            const int length=std::max(1,end-3);
            ab-=asum*bsum/length;aa-=asum*asum/length;bb-=bsum*bsum/length;
            return aa>1e-20 && bb>1e-20 ? std::clamp(1-ab/std::sqrt(aa*bb),0.0,2.0) : 1.0;
        };
        constexpr double ratio=.6180339887498949;
        double l=right-ratio*(right-left),r=left+ratio*(right-left);
        double fl=cost(l),fr=cost(r);
        for(int i=0;i<30;++i){
            if(fl<fr){right=r;r=l;fr=fl;l=right-ratio*(right-left);fl=cost(l);}
            else {left=l;l=r;fl=fr;r=left+ratio*(right-left);fr=cost(r);}
        }
        const double period=(left+right)*.5;
        return std::pair<double,double>{period,cost(period)};
    };
    auto first=refine(selected+correction,1);
    // A dominant second harmonic can satisfy YIN's first-dip threshold. Check
    // single-period residual BEFORE long-baseline refinement (which can hide an
    // octave mistake by comparing an even number of half-periods).
    if(first.second>.002){
        const auto original=first;
        for(int multiple=2;multiple<=3;++multiple){
            if(original.first*multiple>maximum)break;
            const auto candidate=refine(original.first*multiple,1);
            if(candidate.second<first.second*.2)first=candidate;
        }
    }
    const int periods=std::clamp(int((window-12)/(first.first*1.025)),1,4);
    const auto refined=refine(first.first,periods);
    const double hz=rate/refined.first;
    if(hz<16 || hz>2000 || first.second>.1)return {};
    return {float(hz),float(1-first.second)};
}

enum class Profile { equal, ttChromatic, tt6, tt7, tt8, feitenElectric, feitenElectric12, custom, bass5, bass6 };
inline constexpr std::array<const char*,10> profileNames{{"Equal temperament","True Temperament / chromatic","True Temperament / 6 strings","True Temperament / 7 strings","True Temperament / 8 strings","BFTS electric / Anderson open","BFTS electric / Anderson fretted 12","Custom / target per string","Equal / 5-string bass","Equal / 6-string bass"}};
inline constexpr std::array<int,8> openMidi{{64,59,55,50,45,40,35,30}}; // high to low
inline constexpr std::array<double,8> ttOpen{{-1,-1,4,2,0,-2,-4,-8}};
// Primary sources and exact scope are recorded in docs/PRECISION_TUNER.md.
inline constexpr std::array<double,12> ttChromatic{{2,-4,2,-4,-2,0,-4,4,-4,0,-4,-1}};
inline constexpr std::array<double,6> feitenOpen{{0,1,-2,-2,-2,-2}},feiten12{{0,0,1,1,0,0}};
struct Settings {
    Profile profile{Profile::equal};double reference{440};int stringIndex{-1};
    std::array<int,8> customMidi{openMidi};std::array<double,8> customCents{};
};
struct Target {int midi{},stringIndex{-1},fret{-1};double offset{},hz{},cents{};bool valid{};};
inline double frequency(int midi,double reference,double cents=0){return reference*std::exp2((midi-69+cents/100)/12.0);}
inline int strings(Profile p){
    switch(p){
        case Profile::equal: case Profile::ttChromatic:return 0;
        case Profile::tt8: case Profile::custom:return 8;
        case Profile::tt7:return 7;
        case Profile::bass5:return 5;
        default:return 6;
    }
}
// Manual string identity is needed for compensated open/fretted targets; a
// chromatic pitch alone cannot identify a physical string or fingering.
inline constexpr std::array<int,6> bassMidi{{43,38,33,28,23,0}};
inline Target target(double hz,const Settings& s){
    if(int(s.profile)<0 || int(s.profile)>int(Profile::bass6) || s.stringIndex < -1 || s.stringIndex>7)return {};
    if(!std::isfinite(hz)||hz<8||hz>5000||!std::isfinite(s.reference)||s.reference<390||s.reference>490)return {};
    Target result;double distance=1e9;
    const int count=strings(s.profile);
    if(!count){result.midi=int(std::round(69+12*std::log2(hz/s.reference)));result.offset=s.profile==Profile::ttChromatic?ttChromatic[size_t((result.midi%12+12)%12)]:0;}
    else for(int i=0;i<count;++i){
        if(s.stringIndex>=0 && i!=s.stringIndex)continue;
        const bool bass=s.profile==Profile::bass5 || s.profile==Profile::bass6;
        const int midi=bass?(s.profile==Profile::bass6?(i==0?48:bassMidi[size_t(i-1)]):bassMidi[size_t(i)]):s.profile==Profile::custom?s.customMidi[size_t(i)]:openMidi[size_t(i)]+(s.profile==Profile::feitenElectric12?12:0);
        const double offset=bass?0:s.profile==Profile::custom?s.customCents[size_t(i)]:s.profile==Profile::feitenElectric?feitenOpen[size_t(i)]:s.profile==Profile::feitenElectric12?feiten12[size_t(i)]:ttOpen[size_t(i)];
        if(midi<12 || midi>120 || !std::isfinite(offset) || std::abs(offset)>100)return {};
        const double d=std::abs(1200*std::log2(hz/frequency(midi,s.reference,offset)));
        if(d<distance){distance=d;result.midi=midi;result.offset=offset;result.stringIndex=i;}
    }
    if(count && (result.stringIndex<0 || (s.stringIndex<0 && distance>150)))return {};
    result.fret=s.profile==Profile::feitenElectric12?12:(count && s.profile!=Profile::custom?0:-1);
    result.hz=frequency(result.midi,s.reference,result.offset);result.cents=1200*std::log2(hz/result.hz);result.valid=true;return result;
}
// Beat-frequency-driven virtual strobe: no dead band or snapped "in tune" phase.
inline double advanceStrobe(double phase,double measured,double targetHz,double seconds,int harmonic=1){
    if(!std::isfinite(measured)||!std::isfinite(targetHz)||!std::isfinite(seconds)||seconds<=0)return phase;
    const double next=phase+(measured-targetHz)*seconds*harmonic;return next-std::floor(next);
}
// Display-only rate limiting prevents temporal alias/reversed motion far from
// target. The measured Hz/cents are untouched; no cent dead band is introduced.
inline double advanceDisplayStrobe(double phase,double measured,double targetHz,double seconds,int harmonic=1){
    if(!std::isfinite(measured)||!std::isfinite(targetHz)||!std::isfinite(seconds)||seconds<=0)return phase;
    const double step=std::clamp((measured-targetHz)*seconds*harmonic,-.2,.2);
    const double next=phase+step;return next-std::floor(next);
}
}

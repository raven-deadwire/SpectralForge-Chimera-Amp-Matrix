#pragma once
#include "NiflheimrDefinition.h"

namespace spectralforge::niflheimr {
namespace detail {
constexpr double pi=3.14159265358979323846;
inline double pole(double rate,double hz) noexcept {
    return -std::expm1(-2*pi*std::clamp(hz,2.,rate*.44)/rate);
}
inline double timePole(double rate,double seconds) noexcept {
    return -std::expm1(-1/(rate*seconds));
}
struct RC {
    double z{};
    double low(double x,double a) noexcept { z+=a*(x-z); return z; }
    double high(double x,double a) noexcept { return x-low(x,a); }
};
struct Coeff {
    double b0{1},b1{},b2{},a1{},a2{};
    static Coeff highPass(double rate,double hz,double q) noexcept {
        const double w=2*pi*std::clamp(hz,10.,rate*.44)/rate;
        const double c=std::cos(w),alpha=std::sin(w)/(2*q),d=1+alpha;
        return {(1+c)/(2*d),-(1+c)/d,(1+c)/(2*d),-2*c/d,(1-alpha)/d};
    }
    static Coeff peak(double rate,double hz,double db,double q) noexcept {
        if (db==0) return {};
        const double w=2*pi*std::clamp(hz,10.,rate*.44)/rate;
        const double a=std::pow(10.,db/40),alpha=std::sin(w)/(2*q),c=std::cos(w),d=1+alpha/a;
        return {(1+alpha*a)/d,-2*c/d,(1-alpha*a)/d,-2*c/d,(1-alpha/a)/d};
    }
    static Coeff shelf(double rate,double hz,double db,bool high) noexcept {
        if (db==0) return {};
        const double w=2*pi*std::clamp(hz,10.,rate*.44)/rate,a=std::pow(10.,db/40);
        const double c=std::cos(w),b=std::sqrt(2*a)*std::sin(w);
        if (high) {
            const double d=(a+1)-(a-1)*c+b;
            return {a*((a+1)+(a-1)*c+b)/d,-2*a*((a-1)+(a+1)*c)/d,
                    a*((a+1)+(a-1)*c-b)/d,2*((a-1)-(a+1)*c)/d,((a+1)-(a-1)*c-b)/d};
        }
        const double d=(a+1)+(a-1)*c+b;
        return {a*((a+1)-(a-1)*c+b)/d,2*a*((a-1)-(a+1)*c)/d,
                a*((a+1)-(a-1)*c-b)/d,-2*((a-1)+(a+1)*c)/d,((a+1)+(a-1)*c-b)/d};
    }
    void approach(const Coeff& t,double f) noexcept {
        b0+=(t.b0-b0)*f; b1+=(t.b1-b1)*f; b2+=(t.b2-b2)*f;
        a1+=(t.a1-a1)*f; a2+=(t.a2-a2)*f;
    }
};
struct Filter {
    double z1{},z2{};
    double tick(double x,const Coeff& c) noexcept {
        const double y=c.b0*x+z1;
        z1=c.b1*x-c.a1*y+z2; z2=c.b2*x-c.a2*y;
        return y;
    }
};
inline double asymmetric(double x,double bias) noexcept {
    return std::tanh(x+bias)-std::tanh(bias);
}
// Bounded cubic knee, C1 at the rails. A different transfer from tanh without
// introducing a discontinuous hard clip at the production oversampler's input.
inline double knee(double x) noexcept {
    const double v=std::clamp(x,-1.,1.);
    return v*(1.5-.5*v*v);
}
} // namespace detail

// Allocation-free prototype core. prepare() accepts the ALREADY OVERSAMPLED
// rate. The surrounding native engine owns oversampling, latency compensation
// and channel-transition crossfades. Only the selected topology runs here.
// set/reset/tick must be called on the same audio thread with coherent State
// snapshots. Initial voicing, alias performance and musical acceptance remain
// subject to measured DI/IR audition; this is not a hardware-model claim.
class NiflheimrDSP {
    enum Parameter : std::size_t {
        gain,upperGain,mass,fang,fold,thrust,blend,master,lowPole,edgePole,
        fastPole,slowAttackPole,slowReleasePole,bodyPole,edgeOutputPole,driveHighPole,parameterCount
    };
    struct Config {
        std::array<double,parameterCount> p{};
        std::array<detail::Coeff,5> tone{};
        void approach(const Config& t,double f) noexcept {
            for (std::size_t i=0;i<p.size();++i) p[i]+=(t.p[i]-p[i])*f;
            for (std::size_t i=0;i<tone.size();++i) tone[i].approach(t.tone[i],f);
        }
    };
    struct Dirty {
        std::array<detail::RC,3> coupling{},bandwidth{};
        std::array<detail::RC,2> driveHighPass{};
        detail::RC body,edge,dc;
        detail::Filter bodyContour;
        std::array<detail::Filter,4> lowProtection{};
        double fast{},slow{};
    };
    struct Channel {
        detail::RC input,low,body;
        std::array<detail::Filter,5> tone{};
        Dirty dirty;
        Config config;
        int remaining{};
    };
    State target;
    Config targetConfig;
    std::array<Channel,2> channels{};
    double rate=48000,inputPole{},dirtyDCPole{};
    std::array<double,3> couplingPole{},bandwidthPole{};
    std::array<detail::Coeff,channelCount> bodyContourCoeffs{};
    std::array<detail::Coeff,4> protectionCoeffs{};
    int smoothing=960;

    Config configure(const State& s) const noexcept {
        using C=Control;
        Config c;
        auto& p=c.p;
        p[gain]=2*std::exp2(8*s[C::gain]); // 2 / 32 / 512; noon is already driven.
        p[upperGain]=std::max(0.,double(s[C::gain])-.5)*2;
        p[mass]=s[C::mass]; p[fang]=s[C::fang]; p[fold]=s[C::fold];
        p[thrust]=s[C::thrust]; p[blend]=s[C::blend];
        p[master]=4*double(s[C::master])*s[C::master]; // unity at noon; zero mutes.
        p[lowPole]=detail::pole(rate,80+140*s[C::split]);
        p[edgePole]=detail::pole(rate,700+1300*s[C::split]);
        p[fastPole]=detail::timePole(rate,.0007);
        // THRUST changes recovery and transient transmission together. Channel
        // timing is intentionally distinct even with identical panel settings.
        const double attack[]{.002,.003,.004,.001,.030};
        const double release[]{.035,.045,.090,.018,.240};
        p[slowAttackPole]=detail::timePole(rate,attack[s.channel]);
        p[slowReleasePole]=detail::timePole(rate,release[s.channel]*(1.35-.7*s[C::thrust]));
        p[bodyPole]=detail::pole(rate,170+210*s[C::mass]);
        p[edgeOutputPole]=detail::pole(rate,4000+4000*s[C::fang]);
        const double driveCutoff[]{0,210,0,270,170};
        // Extra upper-GAIN saturation must not reintroduce bass-driven mush.
        p[driveHighPole]=detail::pole(rate,driveCutoff[s.channel]*(1+1.2*p[upperGain]));
        c.tone={detail::Coeff::shelf(rate,120,(s[C::bass]-.5)*24,false),
                detail::Coeff::peak(rate,s[C::midFrequency],(s[C::middle]-.5)*24,.75),
                detail::Coeff::shelf(rate,2400,(s[C::treble]-.5)*24,true),
                detail::Coeff::peak(rate,3300,(s[C::presence]-.5)*18,.85),
                detail::Coeff::peak(rate,72,(s[C::depth]-.5)*18,.60+.65*s[C::depth])};
        return c;
    }

    double processDirty(Dirty& d,double body,double edge,const Config& c) noexcept {
        const auto& p=c.p;
        const double residual=body+edge;
        // Garmr/Ymir/Hel previously drove their nonlinear cells with leaked
        // fundamental energy. Protecting LOW only AFTER clipping cannot undo
        // the resulting upper-band intermodulation or bass-driven compression.
        // Condition BODY before both drive and envelope detection; EDGE retains
        // its articulation. The original residual is restored below as before.
        // Hrímfaxi and Nidavellir retain their accepted signal paths.
        if(target.channel==1||target.channel==3||target.channel==4)
            for(auto& filter:d.driveHighPass)
                body=filter.high(body,p[driveHighPole]);
        const double level=std::abs(body+edge);
        d.fast+=p[fastPole]*(level-d.fast);
        d.slow+=(level>d.slow?p[slowAttackPole]:p[slowReleasePole])*(level-d.slow);
        const double attack=std::clamp((d.fast-d.slow)/std::max(1.e-8,d.fast),0.,1.);
        // MASS colours the low part of BODY entering saturation. It never
        // changes the common protected LOW or inserts nonlinear clean gain.
        body+=(p[mass]-.5)*1.6*d.body.low(body,p[bodyPole]);
        const double g=p[gain],f=p[fold],bite=p[fang],hit=p[thrust];
        double y{};
        switch (target.channel) {
            case 0: { // Connected body/edge, two symmetric cells; rapid recovery.
                const double supply=1/(1+2*d.slow*(1.1-hit));
                const double b=std::tanh(body*g*(.30+.65*f)*supply);
                const double e=std::tanh(edge*g*(1.1+1.8*bite)*supply);
                y=.15*b+.105*e;
                y*=1+.25*hit*attack;
                break;
            }
            case 1: { // Garmr: three serial asymmetric stages; tight grind.
                double v=(body+edge)*g*.7/(1+2.8*d.slow*(1.2-hit));
                const double drives[]{1.,1.3+3.0*f+.8*p[upperGain],1.6+2.7*f+1.2*p[upperGain]};
                const double biases[]{.10+.20*bite,-.18-.14*f,.07+.22*bite};
                for (std::size_t i=0;i<3;++i) {
                    v=detail::asymmetric(v*drives[i],biases[i]);
                    v=d.bandwidth[i].low(d.coupling[i].high(v,couplingPole[i]),bandwidthPole[i]);
                }
                y=.18*v*(1+.20*hit*attack)+.12*edge;
                break;
            }
            case 2: { // Clean centre with a separate, narrowly driven upper path.
                const double bodyDrive=12+36*f;
                const double centre=body+.5*f*(std::tanh(bodyDrive*body)/bodyDrive-body);
                double e=edge*g*(1.4+2.6*bite)/(1+1.4*d.slow*(1-hit));
                const double positive=.95-.45*bite,negative=.72-.25*bite;
                e=e>=0?positive*detail::knee(e/positive):negative*detail::knee(e/negative);
                e=d.edge.low(e,p[edgeOutputPole]);
                y=centre+.22*e*(1+.35*hit*attack);
                break;
            }
            case 3: { // Ymir: parallel knees with short, percussive recovery.
                const double relief=1/(1+3*hit*attack);
                const double v=(body+edge)*g*relief;
                const double hard=detail::knee(v*(.8+.7*bite));
                const double round=detail::asymmetric(v*(.45+.9*f),.13+.2*f);
                const double hardMix=.68-.35*f;
                y=.20*(hardMix*hard+(1-hardMix)*round)*(1+.55*hit*attack);
                y+=.5*hit*attack*(body+edge); // conditioned first hit, no bass pumping.
                break;
            }
            default: { // Hel: asymmetric mid-body fuzz; separate articulated EDGE.
                const double supply=1/(1+4.5*d.slow*(.5+f)*(1-.35*hit));
                double v=body*g*(.8+1.6*f)*supply;
                v=v>=0?detail::knee(v):.68*detail::knee(v/.68);
                v=d.coupling[0].high(v,couplingPole[0]);
                v=detail::asymmetric(v*(1.1+2.2*f+.8*p[upperGain]),-.12-.2*f);
                v=d.bandwidth[0].low(v,bandwidthPole[0]);
                const double e=std::tanh(edge*g*(.6+1.2*bite));
                y=.20*(.85+.3*p[mass])*v+.045*d.edge.low(e,p[edgeOutputPole]);
                break;
            }
        }
        // Remove rectification DC inside DIRTY, before adding the common LOW.
        y=d.dc.high(y,dirtyDCPole);
        if(target.channel==1||target.channel==3||target.channel==4)
            y=d.bodyContour.tick(y,bodyContourCoeffs[std::size_t(target.channel)]);
        // Band-limit the nonlinear DIFFERENCE, not the entire DIRTY signal.
        // Keep the original residual below this steep transition, so strong
        // clipping cannot amplify leaked sub-bass in opposition to the LOW.
        // A Butterworth eighth-order 180 Hz boundary rejects difference
        // products through the first bass octave while preserving higher BODY.
        // The clean residual is restored after the difference filter; this
        // avoids a missing low-mid region between two unrelated crossovers.
        y-=residual;
        for(std::size_t i=0;i<d.lowProtection.size();++i)
            y=d.lowProtection[i].tick(y,protectionCoeffs[i]);
        return residual+y;
    }
public:
    NiflheimrDSP() noexcept { prepare(48000); }
    void prepare(double sampleRate) noexcept {
        // Preserve the native engine's true inner rate (including 384 kHz
        // offline rendering at 8x). A lower cap would retune every time constant.
        rate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,3072000.):48000.;
        smoothing=std::max(1,int(rate*.020));
        inputPole=detail::pole(rate,10); dirtyDCPole=detail::pole(rate,8);
        couplingPole={detail::pole(rate,45),detail::pole(rate,95),detail::pole(rate,65)};
        bandwidthPole={detail::pole(rate,5800),detail::pole(rate,4800),detail::pole(rate,6500)};
        // Wet-only low-mid contours retain each voice's density while removing
        // its broad masking region. CLEAN and the common LOW remain untouched.
        bodyContourCoeffs={detail::Coeff{},detail::Coeff::peak(rate,300,-4.5,.8),
                           detail::Coeff{},detail::Coeff::peak(rate,260,-4.,.9),
                           detail::Coeff::peak(rate,340,-3.5,.7)};
        protectionCoeffs={detail::Coeff::highPass(rate,180,.509795579104159),
                          detail::Coeff::highPass(rate,180,.601344886935045),
                          detail::Coeff::highPass(rate,180,.899976223136416),
                          detail::Coeff::highPass(rate,180,2.562915447741506)};
        targetConfig=configure(target);
        reset();
    }
    void set(State s) noexcept {
        s.sanitise();
        if (s==target) return;
        const bool switched=s.channel!=target.channel;
        target=s; targetConfig=configure(target);
        for (auto& c:channels) {
            c.remaining=smoothing;
            // The enclosing native engine crossfades a saved old instance.
            // Discard incompatible nonlinear memory in the new selected voice.
            if (switched) c.dirty={};
        }
    }
    void reset() noexcept {
        channels={};
        for (auto& c:channels) c.config=targetConfig;
    }
    const State& state() const noexcept { return target; }
    float tick(float input,int channel) noexcept {
        if (channel<0||channel>=2) return 0;
        auto& c=channels[std::size_t(channel)];
        if (c.remaining>0) {
            c.config.approach(targetConfig,1./c.remaining);
            if (--c.remaining==0) c.config=targetConfig;
        }
        const auto& p=c.config.p;
        double x=std::isfinite(input)?std::clamp(double(input),-16.,16.):0.;
        x=c.input.high(x,inputPole);
        const double low=c.low.low(x,p[lowPole]);
        const double residual=x-low;
        const double body=c.body.low(residual,p[edgePole]);
        const double edge=residual-body;
        const double dirty=processDirty(c.dirty,body,edge,c.config);
        // C=LOW+residual, D=LOW+dirty. One common LOW, linear BLEND, no
        // equal-power +3 dB hump. Write the clean endpoint explicitly so even
        // moving crossover coefficients/macros cannot modulate BLEND=0.
        double y=p[blend]==0 ? x : low+(1-p[blend])*residual+p[blend]*dirty;
        for (std::size_t i=0;i<c.tone.size();++i) y=c.tone[i].tick(y,c.config.tone[i]);
        y*=p[master];
        if (!std::isfinite(y)) { c={}; c.config=targetConfig; return 0; }
        // Emergency numerical rail only: no intentional post-BLEND saturation.
        // Normal audio, including the clean path, stays linear below this rail.
        return float(std::clamp(y,-32.,32.));
    }
};
} // namespace spectralforge::niflheimr

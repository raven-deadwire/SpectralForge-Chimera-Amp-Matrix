#pragma once
#include "OriginalAmpDefinition.h"

namespace spectralforge::original {
namespace detail {
constexpr double pi=3.14159265358979323846;
inline double pole(double rate,double hz) noexcept { return -std::expm1(-2*pi*std::clamp(hz,2.,rate*.44)/rate); }
inline double timePole(double rate,double seconds) noexcept { return -std::expm1(-1/(rate*seconds)); }
struct RC {
    double z{};
    double low(double x,double a) noexcept { z+=a*(x-z); return z; }
    double high(double x,double a) noexcept { return x-low(x,a); }
};
struct Coeff {
    double b0{1},b1{},b2{},a1{},a2{};
    static Coeff peak(double rate,double hz,double amount,double q=.7) noexcept {
        const double w=2*pi*std::clamp(hz,10.,rate*.44)/rate, a=std::pow(10.,amount/40.);
        const double alpha=std::sin(w)/(2*q), c=std::cos(w), d=1+alpha/a;
        return {(1+alpha*a)/d,-2*c/d,(1-alpha*a)/d,-2*c/d,(1-alpha/a)/d};
    }
    static Coeff shelf(double rate,double hz,double amount,bool high) noexcept {
        const double w=2*pi*std::clamp(hz,10.,rate*.44)/rate,a=std::pow(10.,amount/40.);
        const double c=std::cos(w),b=std::sqrt(a)*std::sin(w)/.707;
        if(high){const double d=(a+1)-(a-1)*c+b;
            return {a*((a+1)+(a-1)*c+b)/d,-2*a*((a-1)+(a+1)*c)/d,
                    a*((a+1)+(a-1)*c-b)/d,2*((a-1)-(a+1)*c)/d,((a+1)-(a-1)*c-b)/d};}
        const double d=(a+1)+(a-1)*c+b;
        return {a*((a+1)-(a-1)*c+b)/d,2*a*((a-1)-(a+1)*c)/d,
                a*((a+1)-(a-1)*c-b)/d,-2*((a-1)+(a+1)*c)/d,((a+1)+(a-1)*c-b)/d};
    }
};
struct Filter {
    double z1{},z2{};
    double tick(double x,const Coeff& c) noexcept { const double y=c.b0*x+z1;z1=c.b1*x-c.a1*y+z2;z2=c.b2*x-c.a2*y;return y; }
};
struct GainCell {
    RC coupling,bandwidth;
    double biasMemory{},blockingMemory{};
    double tick(double x,const GainCellDefinition& d,double crush,double rot,
                double hp,double lp,double charge,double recover,double biasPole) noexcept {
        const double v=std::clamp(x*d.drive*(.65+crush*1.25),-40.,40.);
        const double demand=std::max(0.,v-.65);
        blockingMemory+=(demand>blockingMemory?charge:recover)*(demand-blockingMemory);
        biasMemory+=biasPole*(v/(1+std::abs(v))-biasMemory);
        // Alternate the static operating point between gain cells. ROT changes
        // even harmonics as well as recovery, with bounded blocking bias.
        const double polarity=d.bias<0 ? -1. : 1.;
        const double point=d.bias+rot*(polarity*.20+.20*biasMemory-.16*std::tanh(.5*blockingMemory));
        const double y=std::tanh(v+point)-std::tanh(point);
        return bandwidth.low(coupling.high(y,hp),lp);
    }
};
}

// Allocation-free sample core. prepare() takes the *oversampled* sample rate.
// set(), reset() and tick() are audio-thread operations; callers hand over a
// coherent state snapshot instead of mutating this object from a UI thread.
class OriginalAmpDSP {
    struct Config {
        std::array<double,4> hp{},lp{};
        std::array<GainCellDefinition,4> stages;
        double sagScale=1;
        detail::Coeff middle;
        std::array<detail::Coeff,5> voicing;
        double gain{},bass{},treble{},presence{},depth{},master{},clank{},crush{},impact{},rot{},bloom{};
        double tightenPole{},rotRecovery{},bloomRadius{},bloomCos{},bloomFeed{};
    };
    struct Channel {
        std::array<detail::GainCell,4> cells;
        detail::RC input,tightener,bass,treble,feedbackLow,impactLow,impactSub,presence,output,dc,sagLow;
        detail::Filter middle;
        std::array<detail::Filter,5> voicing;
        State current;
        std::array<double,channelCount> voiceMix{};
        double modernMix=0;
        Config config;
        double fast{},slow{},sag{},feedback{},bloom1{},bloom2{};
        int remaining{},phase{};
    };
    Definition definition=nastrond;
    State target;
    std::array<Channel,2> channels{};
    double rate=48000,inputPole{},outputPole{},bassPole{},treblePole{},presencePole{},dcPole{};
    double fastPole{},slowPole{},sagAttack{},sagRelease{},chargePole{},biasPole{},depthPole{};
    int smoothing=960;

    Config configure(const State& s,const std::array<double,channelCount>& mix,double modern) const noexcept {
        using C=Control;Config c;
        double couplingScale=0,bandwidthScale=0,asymmetryScale=0;c.sagScale=0;
        std::array<double,4> driveScale{};
        for(int voice=0;voice<channelCount;++voice) {
            const auto& v=channelVoices[std::size_t(voice)];const auto w=mix[std::size_t(voice)];
            couplingScale+=w*v.coupling;bandwidthScale+=w*v.bandwidth;asymmetryScale+=w*v.asymmetry;c.sagScale+=w*v.sag;
            for(std::size_t stage=0;stage<4;++stage)driveScale[stage]+=w*v.drive[stage];
        }
        c.stages=definition.stages;
        for(std::size_t stage=0;stage<4;++stage){c.stages[stage].drive*=float(driveScale[stage]);c.stages[stage].bias*=float(asymmetryScale);}
        // Piecewise response keeps zero usable, places the owner's operating
        // point at noon, and reserves the upper half for additional drive.
        const auto taper=[](double value,double midpoint,double maximum){return value<=.5 ? value*2*midpoint : midpoint+(value-.5)*2*(maximum-midpoint);};
        const auto response=[&](double value,double midpoint,double maximum){return value+modern*(taper(value,midpoint,maximum)-value);};
        const auto gain=response(s[C::gain],1.,1.6);
        c.gain=8*gain*gain;
        // With an OD in front the first cell is already saturated at noon.
        // The upper GAIN half therefore also drives the later cells, retaining
        // useful travel on the owner's boosted chain instead of louder input
        // into the same already-flat first-stage transfer.
        const double upperDrive=modern*std::max(0.,double(s[C::gain])-.5)*2;
        for(std::size_t stage=1;stage<4;++stage)c.stages[stage].drive*=float(1+upperDrive*(.25+.20*double(stage)));
        c.bass=std::pow(10.,(s[C::bass]-.5)*18/20)-1;
        c.treble=std::pow(10.,(s[C::treble]-.5)*18/20)-1;
        c.middle=detail::Coeff::peak(rate,s[C::midFrequency],(s[C::middle]-.5)*20);
        c.presence=(s[C::presence]-.5)*1.4;c.depth=s[C::depth];
        // Preserve the pre-voicing default pluck RMS; this trim is after drive.
        const auto master=response(s[C::master],.695,1.);
        c.master=master*master*1.5*.886;
        c.clank=s[C::clank];c.crush=s[C::crush];c.impact=s[C::impact];c.rot=s[C::rot];c.bloom=s[C::bloom];
        std::array<double,5> midpoint{};
        for(int voice=0;voice<channelCount;++voice)for(std::size_t macro=0;macro<5;++macro)midpoint[macro]+=mix[std::size_t(voice)]*channelMidpoints[std::size_t(voice)][macro];
        c.clank=response(c.clank,midpoint[0],1.18);c.crush=response(c.crush,midpoint[1],1.65);
        c.impact=response(c.impact,midpoint[2],1.55);c.rot=response(c.rot,midpoint[3],1.5);c.bloom=response(c.bloom,midpoint[4],1.5);
        c.tightenPole=detail::pole(rate,75+200*c.clank);
        c.rotRecovery=detail::timePole(rate,.012+.16*c.rot);
        const double bloomHz=95+40*c.bloom;
        c.bloomRadius=std::exp(-1/(rate*(.004+.04*c.bloom)));
        c.bloomCos=2*c.bloomRadius*std::cos(2*detail::pi*bloomHz/rate);
        c.bloomFeed=2*(1-c.bloomRadius)*std::sin(2*detail::pi*bloomHz/rate);
        // Macro-dependent power voicing, centred on the existing defaults.
        // IMPACT focuses the LF hit below the low-mid congestion; BLOOM adds
        // low-mid body and relaxes the upper mids without increasing stage gain.
        // ROT retains its asymmetric cells and exposes their upper harmonics.
        // The modern high-gain midpoint already supplies upper harmonics: keep
        // IMPACT on the LF shelf/notch and use a gentler ROT shelf to avoid
        // overshooting the chord references. Legacy response stays exact.
        // Use the stored float defaults so the neutral contour stays exact.
        const double impact=c.impact-controls[std::size_t(C::impact)].initial;
        const double bloom=c.bloom-controls[std::size_t(C::bloom)].initial;
        const double rot=c.rot-controls[std::size_t(C::rot)].initial;
        c.voicing={detail::Coeff::shelf(rate,100,1.95+(4+modern)*impact,false),
                   detail::Coeff::peak(rate,500,-2.11,.65),
                   detail::Coeff::shelf(rate,2000,6.+3*(1-modern)*impact+(3-modern)*rot,true),
                   detail::Coeff::peak(rate,180,-(5+modern)*impact+3*bloom,.8),
                   detail::Coeff::peak(rate,1400,-3*bloom,.6)};
        const double coupling=.8+.4*c.clank-.45*c.bloom;
        for(std::size_t i=0;i<4;++i){c.hp[i]=detail::pole(rate,definition.stages[i].couplingHz*coupling*couplingScale);c.lp[i]=detail::pole(rate,definition.stages[i].bandwidthHz*bandwidthScale);}
        return c;
    }
public:
    void prepare(double sampleRate,const Definition& d=nastrond) noexcept {
        definition=d;rate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,768000.):48000.;
        smoothing=std::max(1,int(rate*definition.smoothingSeconds));
        inputPole=detail::pole(rate,d.inputHighPassHz);outputPole=detail::pole(rate,d.outputLowPassHz);
        bassPole=detail::pole(rate,180);treblePole=detail::pole(rate,2300);presencePole=detail::pole(rate,3200);
        dcPole=detail::pole(rate,8);depthPole=detail::pole(rate,120);
        fastPole=detail::timePole(rate,.001);slowPole=detail::timePole(rate,.035);
        sagAttack=detail::timePole(rate,.010);sagRelease=detail::timePole(rate,.120);
        chargePole=detail::timePole(rate,.002);biasPole=detail::timePole(rate,.045);
        reset();
    }
    void set(State s) noexcept {
        s.sanitise();if(s==target)return;target=s;
        for(auto& c:channels)c.remaining=smoothing;
    }
    void reset() noexcept {
        channels={};for(auto& c:channels){c.current=target;c.voiceMix[std::size_t(target.channel)]=1;c.modernMix=target.modern?1:0;c.config=configure(target,c.voiceMix,c.modernMix);}
    }
    const State& state() const noexcept { return target; }
    float tick(float input,int channel) noexcept {
        if(channel<0||channel>=2)return 0;
        auto& c=channels[std::size_t(channel)];
        if(c.remaining>0){
            for(std::size_t i=0;i<controlCount;++i)c.current.values[i]+=(target.values[i]-c.current.values[i])/float(c.remaining);
            for(int voice=0;voice<channelCount;++voice)c.voiceMix[std::size_t(voice)]+=((voice==target.channel?1.:0.)-c.voiceMix[std::size_t(voice)])/c.remaining;
            c.modernMix+=((target.modern?1.:0.)-c.modernMix)/c.remaining;
            --c.remaining;
            if((++c.phase%16)==0 || c.remaining==0)c.config=configure(c.current,c.voiceMix,c.modernMix);
        }
        const auto& p=c.config;
        double x=std::isfinite(input)?std::clamp(double(input),-16.,16.):0.;
        x=c.input.high(x,inputPole);
        c.fast+=fastPole*(std::abs(x)-c.fast);c.slow+=slowPole*(std::abs(x)-c.slow);
        const double tighten=std::min(.95,p.clank*(.30+.60*std::clamp(c.slow*5.,0.,1.)));
        x-=tighten*c.tightener.low(x,p.tightenPole);
        x*=p.gain;
        for(std::size_t i=0;i<4;++i)
            x=c.cells[i].tick(x,p.stages[i],p.crush,p.rot,p.hp[i],p.lp[i],chargePole,p.rotRecovery,biasPole);
        x+=p.bass*c.bass.low(x,bassPole);
        x=c.middle.tick(x,p.middle);
        x+=p.treble*c.treble.high(x,treblePole);
        // IMPACT changes post-distortion LF drive and feedback damping, not
        // input gain. CRUSH controls the serial cells before the tone stack.
        const double low=c.feedbackLow.low(x,depthPole);
        const double attack=std::clamp((c.fast-c.slow)/std::max(c.slow,.0001),0.,1.);
        x+=low*p.depth*(.15+.6*p.impact);
        const double demand=std::abs(x);
        c.sag+=(demand>c.sag?sagAttack:sagRelease)*(demand-c.sag);
        const double supply=1/(1+(.04+.22*p.bloom)*c.sag*p.sagScale);
        x=std::tanh((x-c.feedback*(.14-.09*p.impact))*supply*(1+.8*p.crush));
        c.feedback=x;
        // Keep the resonant feedback response audible after power saturation.
        // Subsonic content is excluded; attack gain is bounded and level-relative.
        const double body=c.impactLow.low(x,depthPole);
        x+=p.depth*p.impact*(.8+.4*attack)*(body-c.impactSub.low(body,inputPole));
        x+=p.presence*c.presence.high(x,presencePole);
        // Stable damped LF resonator: BLOOM controls decay independently of
        // IMPACT's initial LF drive. At zero input it releases to silence.
        const double bloomInput=c.sagLow.low(x,bassPole);
        const double resonant=p.bloomFeed*bloomInput+p.bloomCos*c.bloom1-p.bloomRadius*p.bloomRadius*c.bloom2;
        c.bloom2=c.bloom1;c.bloom1=resonant;
        x+=.25*p.bloom*resonant;
        for(std::size_t i=0;i<p.voicing.size();++i)x=c.voicing[i].tick(x,p.voicing[i]);
        const double output=c.dc.high(c.output.low(x,outputPole),dcPole)*p.master;
        // The contour can overshoot at maximum EQ/depth/master. Leave normal
        // preset levels untouched and bend extreme peaks into a bounded rail.
        const double magnitude=std::abs(output);
        return float(magnitude<=1.5 ? output : std::copysign(1.5+2*std::tanh((magnitude-1.5)/2),output));
    }
};
} // namespace spectralforge::original

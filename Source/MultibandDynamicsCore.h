#pragma once

#include <algorithm>
#include <array>
#include <complex>
#include <cmath>
#include <cstddef>
#include <limits>

namespace spectralforge {

inline constexpr int multibandBandCount = 4;

// Independent implementation. The four fixed, contiguous bands are an audio
// processor; they are not the amplifier Matrix routes. No lookahead, FIR delay,
// external sidechain, or proprietary "Dynamic Phase" behaviour is claimed.
struct MultibandBandSettings {
    bool bypass{false};
    // 0: downward compression, 1: upward compression,
    // 2: downward expansion,   3: upward expansion.
    int mode{0};
    float threshold{-24.f};
    float range{6.f};
    float ratio{2.f};
    float attack{10.f};
    float release{120.f};
    float knee{6.f};
    float makeup{0.f};
    bool solo{false};
    // 0: both channels, 1: mid (M/S) or left (L/R) only, 2: side or right only.
    int channels{0};
};

struct MultibandSettings {
    bool on{false};
    float mix{1.f};
    int detector{1}; // 0: linked peak, 1: max of the two channel RMS detectors.
    int stereo{0};   // 0: left/right, 1: mid/side (M = (L + R) / 2, S = (L - R) / 2).
    float link{1.f}; // 1: both channels follow the louder detector; 0: each its own.
    std::array<float, 3> crossovers{120.f, 700.f, 3500.f};
    std::array<MultibandBandSettings, multibandBandCount> bands{};
};

namespace multiband_detail {

inline constexpr double pi = 3.1415926535897932384626433832795;
inline constexpr double butterworthK = 1.4142135623730950488;

inline double finiteClamp(double x, double lo, double hi, double fallback) noexcept {
    return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback;
}

inline double validRate(double sr) noexcept {
    return finiteClamp(sr, 8000., 384000., 48000.);
}

inline double gainFromDB(double db) noexcept { return std::pow(10., db * .05); }
inline double levelDB(double level) noexcept { return 20. * std::log10(std::max(1.e-12, level)); }

inline double coefficient(double rate, double milliseconds) noexcept {
    return std::exp(-1. / (rate * milliseconds * .001));
}

// The knee is a continuous, differentiable transition between the two slopes.
inline double softHinge(double above, double knee) noexcept {
    if (knee <= 0.) return std::max(0., above);
    if (above <= -.5 * knee) return 0.;
    if (above >= .5 * knee) return above;
    const double position = above + .5 * knee;
    return position * position / (2. * knee);
}

// Signed applied gain in dB: negative means attenuation, positive means boost.
inline double dynamicsGainDB(double level, const MultibandBandSettings& s) noexcept {
    const double ratio = finiteClamp(s.ratio, 1., 20., 2.);
    const double threshold = finiteClamp(s.threshold, -72., 0., -24.);
    const double knee = finiteClamp(s.knee, 0., 24., 6.);
    const double range = finiteClamp(s.range, 0., 36., 6.);
    const double below = softHinge(threshold - level, knee);
    const double above = softHinge(level - threshold, knee);
    double change = 0.;
    switch (std::clamp(s.mode, 0, 3)) {
        case 0: change = -(1. - 1. / ratio) * above; break;
        case 1: change =  (1. - 1. / ratio) * below; break;
        case 2: change = -(ratio - 1.) * below; break;
        case 3: change =  (ratio - 1.) * above; break;
    }
    return std::clamp(change, -range, range);
}

// A finite linear ramp with exact endpoints, including exact steady bypass.
class Ramp {
public:
    void reset(double value) noexcept { current = target = value; step = 0.; remaining = 0; }
    void setTarget(double value, int samples) noexcept {
        if (value == target) return;
        target = value;
        remaining = std::max(1, samples);
        step = (target - current) / remaining;
    }
    double next() noexcept {
        if (remaining > 0) {
            current += step;
            if (--remaining == 0) current = target;
        }
        return current;
    }
    double value() const noexcept { return current; }
    bool moving() const noexcept { return remaining > 0; }
    int remainingSamples() const noexcept { return remaining; }
    double valueAfter(int n) const noexcept { return n >= remaining ? target : current + step * n; }
private:
    double current{}, target{}, step{};
    int remaining{};
};

struct SVFCoefficients {
    double a1{1.}, a2{}, a3{};
    void setG(double g) noexcept {
        a1 = 1. / (1. + g * (g + butterworthK));
        a2 = g * a1;
        a3 = g * a2;
    }
};

// Topology-preserving state-variable sections retain stable poles while their
// positive frequency parameter is smoothed. Coefficients are shared across
// every section at a given crossover, including its phase compensators.
class CrossoverFrequency {
public:
    void prepare(double sr, double hz) noexcept {
        rate = sr;
        smoothing = coefficient(rate, 20.);
        g = target = std::tan(pi * hz / rate);
        coefficients.setG(g);
    }
    void setTarget(double hz) noexcept { target = std::tan(pi * hz / rate); }
    const SVFCoefficients& next() noexcept {
        if (g != target) {
            g = target + smoothing * (g - target);
            if (std::abs(g - target) <= 1.e-10 * std::max(1., target)) g = target;
            coefficients.setG(g);
        }
        return coefficients;
    }
    float frequency() const noexcept { return float(rate * std::atan(g) / pi); }
private:
    double rate{48000.}, g{}, target{}, smoothing{};
    SVFCoefficients coefficients{};
};

struct SVFOutput { double low{}, band{}, high{}; };

class SVF {
public:
    void reset() noexcept { z1 = z2 = 0.; }
    SVFOutput tick(double x, const SVFCoefficients& c) noexcept {
        const double v3 = x - z2;
        const double v1 = c.a1 * z1 + c.a2 * v3;
        const double v2 = z2 + c.a2 * z1 + c.a3 * v3;
        z1 = 2. * v1 - z1;
        z2 = 2. * v2 - z2;
        if (std::abs(z1) < 1.e-30) z1 = 0.;
        if (std::abs(z2) < 1.e-30) z2 = 0.;
        return {v2, v1, x - butterworthK * v1 - v2};
    }
    double allpass(double x, const SVFCoefficients& c) noexcept {
        return x - 2. * butterworthK * tick(x, c).band;
    }
private:
    double z1{}, z2{};
};

class LinkwitzRileySplit {
public:
    void reset() noexcept { first.reset(); low.reset(); high.reset(); }
    std::array<double, 2> tick(double x, const SVFCoefficients& c) noexcept {
        const auto a = first.tick(x, c);
        return {low.tick(a.low, c).low, high.tick(a.high, c).high};
    }
private:
    SVF first, low, high;
};

// L_i + H_i = A_i, where A_i is a second-order allpass. The compensated
// cascade is [A3 A2 L1, A3 L2 H1, L3 H2 H1, H3 H2 H1]. Its unity-gain sum
// is A3 A2 A1, not the misaligned sum from a naive three-way LR cascade.
// Hence settled crossovers reconstruct with flat MAGNITUDE. They do rotate
// phase: zero added sample delay does not mean linear/zero phase. Automation
// is smoothed, but the LTI allpass identity is not claimed during motion.
class FourBandSplit {
public:
    void reset() noexcept {
        for (auto& s : split) s.reset();
        for (auto& a : compensation) a.reset();
    }
    std::array<double, 4> tick(double input,
                               const std::array<const SVFCoefficients*, 3>& c) noexcept {
        const auto first = split[0].tick(input, *c[0]);
        const auto second = split[1].tick(first[1], *c[1]);
        const auto third = split[2].tick(second[1], *c[2]);
        const double low = compensation[1].allpass(
            compensation[0].allpass(first[0], *c[1]), *c[2]);
        const double lowMid = compensation[2].allpass(second[0], *c[2]);
        return {low, lowMid, third[0], third[1]};
    }
private:
    std::array<LinkwitzRileySplit, 3> split;
    std::array<SVF, 3> compensation;
};

struct Detector {
    std::array<double, 2> energy{}, channelPeak{};
    double peak{};
    void reset() noexcept { energy = {}; channelPeak = {}; peak = 0.; }
    // Per-channel levels blended towards the louder one by `link` (amplitude).
    // At link 1 both equal tick()'s linked level exactly: the maximum of
    // per-channel peaks decayed alike is the shared decayed peak.
    std::array<double, 2> levels(const std::array<double, 2>& sample, int channels, int mode,
                                 double rmsCoefficient, double peakCoefficient, double link) noexcept {
        std::array<double, 2> own{};
        for (int c = 0; c < channels; ++c) {
            const auto index = size_t(c);
            const double x = sample[index];
            energy[index] = rmsCoefficient * energy[index] + (1. - rmsCoefficient) * x * x;
            if (energy[index] < 1.e-30) energy[index] = 0.;
            channelPeak[index] = std::max(std::abs(x), channelPeak[index] * peakCoefficient);
            if (channelPeak[index] < 1.e-30) channelPeak[index] = 0.;
            own[index] = mode == 0 ? channelPeak[index] : std::sqrt(energy[index]);
        }
        if (channels == 1) { energy[1] = 0.; channelPeak[1] = 0.; return {own[0], own[0]}; }
        const double linked = std::max(own[0], own[1]);
        if (link >= 1.) return {linked, linked};
        return {link * linked + (1. - link) * own[0], link * linked + (1. - link) * own[1]};
    }
    double tick(const std::array<double, 2>& sample, int channels, int mode,
                double rmsCoefficient, double peakCoefficient) noexcept {
        double peakSample = 0., maximumEnergy = 0.;
        for (int c = 0; c < channels; ++c) {
            const auto index = size_t(c);
            const double x = sample[index];
            peakSample = std::max(peakSample, std::abs(x));
            energy[index] = rmsCoefficient * energy[index]
                          + (1. - rmsCoefficient) * x * x;
            if (energy[index] < 1.e-30) energy[index] = 0.;
            maximumEnergy = std::max(maximumEnergy, energy[index]);
        }
        if (channels == 1) energy[1] = 0.;
        peak = std::max(peakSample, peak * peakCoefficient);
        if (peak < 1.e-30) peak = 0.;
        return mode == 0 ? peak : std::sqrt(maximumEnergy);
    }
};

inline float safeOutput(double value) noexcept {
    if (!std::isfinite(value)) return 0.f;
    constexpr double bound = std::numeric_limits<float>::max();
    return float(std::clamp(value, -bound, bound));
}

inline MultibandBandSettings sanitiseBand(MultibandBandSettings s) noexcept {
    s.mode = std::clamp(s.mode, 0, 3);
    s.threshold = float(finiteClamp(s.threshold, -72., 0., -24.));
    s.range = float(finiteClamp(s.range, 0., 36., 6.));
    s.ratio = float(finiteClamp(s.ratio, 1., 20., 2.));
    s.attack = float(finiteClamp(s.attack, .1, 200., 10.));
    s.release = float(finiteClamp(s.release, 5., 2000., 120.));
    s.knee = float(finiteClamp(s.knee, 0., 24., 6.));
    s.makeup = float(finiteClamp(s.makeup, -24., 24., 0.));
    s.channels = std::clamp(s.channels, 0, 2);
    return s;
}

// The LR4 analysis bank is unchanged. Its positive, phase-aligned band weights
// have magnitude N(w^4)/D(w^4). Spectral factorisation preserves that magnitude
// while choosing stable minimum-phase zeros. Unity gain is exact raw audio.
class MinimumPhaseSynthesis {
public:
    void reset() noexcept {
        for(auto& ch:filter)for(auto& pair:ch)for(auto& s:pair)s.reset();
        initialized=false; countdown=0;
    }
    void invalidateSchedule() noexcept { countdown=0; }
    // Gain schedule (not the filter states) shared by both channels while
    // their band gains are equal; one calculation serves both.
    void copySchedule(const MinimumPhaseSynthesis& o) noexcept {
        weight=o.weight; overall=o.overall; targetGain=o.targetGain; gainStep=o.gainStep;
        weightStep=o.weightStep; targetWeight=o.targetWeight; countdown=o.countdown; initialized=o.initialized;
    }
    bool sameSchedule(const MinimumPhaseSynthesis& o) const noexcept {
        return weight==o.weight && overall==o.overall && targetGain==o.targetGain && gainStep==o.gainStep
            && weightStep==o.weightStep && targetWeight==o.targetWeight && countdown==o.countdown && initialized==o.initialized;
    }
    void unity() noexcept {
        weight={}; overall=1.; initialized=true; countdown=0;
    }
    void warm(double x,int channel,const std::array<const SVFCoefficients*,3>& c) noexcept {
        for(size_t i=0;i<3;++i) {
            auto& pair=filter[size_t(channel)][i];
            pair[1].tick(pair[0].tick(x,*c[i]).low,*c[i]);
        }
    }
    void configure(const std::array<double,4>& actual,
                   const std::array<double,4>& forecast,
                   const std::array<const SVFCoefficients*,3>& c, int length) noexcept {
        if(!initialized) {
            calculate(actual,c); initialized=true; countdown=0; weightStep={}; gainStep=0.;
            return;
        }
        if(countdown==0) {
            const auto previous=weight; const double previousGain=overall;
            calculate(forecast,c);
            for(size_t i=0;i<3;++i)for(size_t j=0;j<4;++j)weightStep[i][j]=(weight[i][j]-previous[i][j])/length;
            gainStep=(overall-previousGain)/length;
            targetWeight=weight; targetGain=overall;
            weight=previous; overall=previousGain; countdown=length;
        }
        for(size_t i=0;i<3;++i)for(size_t j=0;j<4;++j)weight[i][j]+=weightStep[i][j];
        overall+=gainStep;
        if(--countdown==0){weight=targetWeight;overall=targetGain;}
    }
private:
    void calculate(const std::array<double,4>& inputGain,
                   const std::array<const SVFCoefficients*,3>& coefficients) noexcept {
        std::array<double,3> g{},q{};
        for(size_t i=0;i<3;++i)g[i]=coefficients[i]->a2/coefficients[i]->a1;
        const double scale=g[1];
        for(size_t i=0;i<3;++i){const double a=g[i]/scale;q[i]=a*a*a*a;}
        std::array<double,4> gain{};
        for(size_t i=0;i<4;++i)gain[i]=std::max(1.e-8,inputGain[i]);
        overall=gain[3];
        if(gain[0]==gain[1]&&gain[1]==gain[2]&&gain[2]==gain[3]) {
            weight={};
            return;
        }
        const double a=(gain[0]*q[0]+gain[1]*q[1]+gain[2]*q[2])/gain[3];
        const double b=(gain[0]*q[0]*(q[1]+q[2])+gain[1]*q[1]*q[2])/gain[3];
        const double c=gain[0]*q[0]*q[1]*q[2]/gain[3];
        // Scale the monic cubic, solve its largest-magnitude negative real root,
        // then deflate using Vieta (avoids subtracting almost equal large roots).
        const double norm=std::max({a,std::sqrt(b),std::cbrt(c),1.e-30});
        const double aa=a/norm,bb=b/(norm*norm),cc=c/(norm*norm*norm);
        const double p=bb-aa*aa/3.,r=2.*aa*aa*aa/27.-aa*bb/3.+cc;
        const double d=r*r/4.+p*p*p/27.;
        double root;
        if(d>=0.) {
            const double u=std::cbrt(-r*.5-std::copysign(std::sqrt(d),r));
            root=u+(std::abs(u)>1.e-30?-p/(3.*u):0.)-aa/3.;
        } else {
            const double radius=2.*std::sqrt(-p/3.);
            const double angle=std::acos(std::clamp(-r*.5/std::sqrt(-p*p*p/27.),-1.,1.))/3.;
            root=radius*std::cos(angle+2.*pi/3.)-aa/3.;
        }
        for(int i=0;i<3;++i){
            const double f=((root+aa)*root+bb)*root+cc;
            const double derivative=(3.*root+2.*aa)*root+bb;
            if(std::abs(derivative)>1.e-30)root-=f/derivative;
        }
        const double v=-cc/root, u=(v-bb)/root;
        const double discriminant=u*u-4.*v;
        // Each stable conjugate zero pair is [s^2 + B*s + C], used twice.
        std::array<std::array<double,2>,3> factors{};
        const auto realFactor=[&](double value){
            const double radius=std::sqrt(std::sqrt(std::max(0.,-value*norm)))*scale;
            return std::array<double,2>{butterworthK*radius,radius*radius};
        };
        factors[0]=realFactor(root);
        if(discriminant>=0.) {
            const double second=(-u-std::sqrt(discriminant))*.5;
            factors[1]=realFactor(second);
            factors[2]=realFactor(v/second);
        } else {
            const double magnitude=std::sqrt(v);
            const double radius=std::sqrt(std::sqrt(magnitude*norm))*scale;
            const double halfCosine=std::sqrt(std::clamp((1.-u/(2.*magnitude))*.5,0.,1.));
            const double cosine=std::sqrt((1.+halfCosine)*.5);
            const double sine=std::sqrt((1.-halfCosine)*.5);
            const double square=radius*radius;
            factors[1]={2.*radius*sine,square};
            factors[2]={2.*radius*cosine,square};
        }
        // Fixed parallel poles: the synthesis states depend only on the input,
        // never on gain automation. Residues avoid moving zeros between filter
        // sections and the energy injection of time-varying cascade numerators.
        using C = std::complex<double>;
        for(size_t i=0;i<3;++i) {
            const double gi=g[i]/scale;
            const C pole(-gi/butterworthK,gi/butterworthK);
            C numerator=overall, derivative=0.;
            for(const auto& f:factors) {
                const double b=f[0]/scale, c=f[1]/(scale*scale);
                const C q=pole*pole+b*pole+c, dq=2.*pole+b;
                derivative=derivative*q*q+numerator*2.*q*dq;
                numerator*=q*q;
            }
            const C separation=pole-std::conj(pole);
            C denominator=separation*separation, logDerivative=2./separation;
            for(size_t j=0;j<3;++j) if(j!=i) {
                const double gj=g[j]/scale;
                const C q=pole*pole+butterworthK*gj*pole+gj*gj;
                denominator*=q*q;
                logDerivative+=2.*(2.*pole+butterworthK*gj)/q;
            }
            const C second=numerator/denominator;
            const C first=(derivative-numerator*logDerivative)/denominator;
            const C conjugate=std::conj(pole);
            weight[i]={2.*first.real()/gi,
                (2.*second.real()-2.*(first*conjugate).real())/(gi*gi),
                (-4.*(second*conjugate).real()-2.*second.real()*butterworthK*gi)/(gi*gi*gi),
                (2.*(second*conjugate*conjugate).real()-2.*second.real()*gi*gi)/(gi*gi*gi*gi)};
        }
    }
public:
    double tick(double x,int channel,const std::array<const SVFCoefficients*,3>& c) noexcept {
        double y=overall*x;
        for(size_t i=0;i<3;++i) {
            auto& pair=filter[size_t(channel)][i];
            const auto first=pair[0].tick(x,*c[i]);
            const auto second=pair[1].tick(first.low,*c[i]);
            y+=weight[i][0]*first.band+weight[i][1]*first.low
               +weight[i][2]*second.band+weight[i][3]*second.low;
        }
        return y;
    }

private:
    std::array<std::array<std::array<SVF,2>,3>,2> filter{};
    std::array<std::array<double,4>,3> weight{};
    double overall{1.}, targetGain{1.}, gainStep{};
    std::array<std::array<double,4>,3> weightStep{},targetWeight{};
    int countdown{};
    bool initialized{};
};

} // namespace multiband_detail

class MultibandDynamicsCore {
public:
    // No delay line/lookahead. IIR frequency-dependent group delay remains.
    static constexpr int latencySamples() noexcept { return 0; }

    MultibandDynamicsCore() noexcept { prepare(48000.); }

    static std::array<float, 3> validatedCrossovers(
        std::array<float, 3> input, double sampleRate) noexcept {
        using namespace multiband_detail;
        const double maximum = std::min(16000., .4 * validRate(sampleRate));
        constexpr double spacing = 1.4;
        const double f0 = finiteClamp(input[0], 40., maximum / (spacing * spacing), 120.);
        const double f1 = std::clamp(finiteClamp(input[1], 40., maximum, 700.),
                                   f0 * spacing, maximum / spacing);
        const double f2 = std::clamp(finiteClamp(input[2], 40., maximum, 3500.),
                                   f1 * spacing, maximum);
        return {float(f0), float(f1), float(f2)};
    }

    void setSettings(const MultibandSettings& next) noexcept {
        using namespace multiband_detail;
        settings = next;
        settings.mix = float(finiteClamp(settings.mix, 0., 1., 1.));
        settings.detector = std::clamp(settings.detector, 0, 1);
        settings.stereo = std::clamp(settings.stereo, 0, 1);
        settings.link = float(finiteClamp(settings.link, 0., 1., 1.));
        settings.crossovers = validatedCrossovers(settings.crossovers, rate);
        for (auto& b : settings.bands) b = sanitiseBand(b);
    }

    const MultibandSettings& getSettings() const noexcept { return settings; }

    void prepare(double sampleRate) noexcept {
        using namespace multiband_detail;
        rate = validRate(sampleRate);
        setSettings(settings);
        rampSamples = std::max(1, int(std::ceil(rate * .01)));
        settleSamples = std::max(1, int(std::ceil(rate * .03)));
        synthesisInterval = std::max(1, int(std::ceil(rate / 6000.)));
        rmsCoefficient = coefficient(rate, 5.);
        peakCoefficient = coefficient(rate, 30.);
        reset();
    }

    void reset() noexcept {
        for (auto& s : splitter) s.reset();
        for (auto& s : keySplitter) s.reset();
        for (size_t i = 0; i < frequencies.size(); ++i)
            frequencies[i].prepare(rate, settings.crossovers[i]);
        wet.reset(settings.on ? 1. : 0.);
        lastOn=settings.on;
        domain = settings.stereo; switching = 0; settleLeft = 0;
        mix.reset(settings.mix);
        const bool solo = anySolo();
        for (size_t i = 0; i < bandState.size(); ++i) {
            auto& s = bandState[i];
            s.detector.reset();
            s.gainDB = {};
            s.makeup.reset(settings.bands[i].makeup);
            s.wet.reset(settings.bands[i].bypass ? 0. : 1.);
            s.solo.reset(!solo || settings.bands[i].solo ? 1. : 0.);
        }
        meters = {};
        for (auto& synthesis : synth) synthesis.reset();
        shared = true;
    }

    // In-place mono/stereo; extra channels are untouched. Size is independent
    // of a prepared maximum: zero, ragged, and oversized blocks are bounded.
    // All storage is fixed; no allocation, lock, logging or parameter lookup.
    // `key` (optional, `keyChannels` mono or stereo, `sampleCount` long) is an
    // external detector input: it is split by its own crossovers and each
    // band's detector reads the key's band instead of the signal's.
    void process(float* const* audio, int channelCount, int sampleCount, const float* const* key = nullptr, int keyChannels = 0) noexcept {
        using namespace multiband_detail;
        const int channels = std::clamp(channelCount, 0, 2);
        if (channels == 0 || sampleCount <= 0 || audio == nullptr) return;
        for (int c = 0; c < channels; ++c) if (audio[c] == nullptr) return;
        keyChannels = key != nullptr ? std::clamp(keyChannels, 0, 2) : 0;
        if (keyChannels == 0) key = nullptr;
        if ((key != nullptr) != keyed) { for (auto& split : keySplitter) split.reset(); keyed = key != nullptr; }

        // A stereo-domain change (L/R <-> M/S) while running fades the wet
        // path out, swaps the domain, feeds the new domain silently for 30 ms
        // so its filters settle, and fades back in. Switched off, it swaps at once.
        const bool stereo = channels == 2;
        if (settings.stereo != domain && switching == 0) {
            if (!settings.on && !wet.moving()) domain = settings.stereo;
            else switching = 1;
        }
        if (settings.on && wet.value() == 0. && !wet.moving() && switching == 0) {
            for (auto& split : splitter) split.reset();
            for (auto& split : keySplitter) split.reset();   // no stale key state either
            for (auto& state : bandState) { state.detector.reset(); state.gainDB = {}; }
        }
        if (settings.on != lastOn) { for (auto& synthesis : synth) synthesis.invalidateSchedule(); lastOn=settings.on; }
        wet.setTarget(settings.on && switching == 0 ? 1. : 0., rampSamples);
        if (!settings.on && !wet.moving()) {
            meters = {};
            std::array<const SVFCoefficients*,3> c{};
            // Keep only six fixed synthesis SVFs/channel warm. OFF samples are
            // never rewritten; analysis/detectors remain asleep until enabled.
            if (switching != 0) { domain = settings.stereo; switching = 0; }
            for (auto& synthesis : synth) synthesis.unity();
            shared = true;
            for(int n=0;n<sampleCount;++n) {
                for(size_t i=0;i<3;++i)c[i]=&frequencies[i].next();
                const auto in = domainInput(audio, channels, n, stereo);
                for(int channel=0;channel<channels;++channel)
                    synth[size_t(channel)].warm(in[size_t(channel)],channel,c);
            }
            return;
        }

        mix.setTarget(settings.mix, rampSamples);
        for (size_t i = 0; i < frequencies.size(); ++i)
            frequencies[i].setTarget(settings.crossovers[i]);
        const bool solo = anySolo();
        for (size_t i = 0; i < bandState.size(); ++i) {
            auto& s = bandState[i];
            const auto& v = settings.bands[i];
            s.attack = coefficient(rate, v.attack);
            s.release = coefficient(rate, v.release);
            s.makeup.setTarget(v.makeup, rampSamples);
            s.wet.setTarget(v.bypass ? 0. : 1., rampSamples);
            s.solo.setTarget(!solo || v.solo ? 1. : 0., rampSamples);
        }

        for (int n = 0; n < sampleCount; ++n) {
            std::array<const SVFCoefficients*, 3> coefficients{};
            for (size_t i = 0; i < frequencies.size(); ++i)
                coefficients[i] = &frequencies[i].next();
            // Domain samples: L/R as they are, or M/S. In double, M + S and M - S
            // return L and R except for rounding (below 1e-16) where one channel
            // is many orders of magnitude smaller than the other.
            const auto dry = domainInput(audio, channels, n, stereo);
            std::array<std::array<double, 4>, 2> samples{};
            for (int c = 0; c < channels; ++c) samples[size_t(c)] = splitter[size_t(c)].tick(dry[size_t(c)], coefficients);
            std::array<std::array<double, 4>, 2> keySamples{};
            if (key != nullptr) {
                std::array<double, 2> k{};
                for (int c = 0; c < channels; ++c) { const float x = key[std::min(c, keyChannels - 1)][n];k[size_t(c)] = std::isfinite(x) ? double(x) : 0.; }
                if (stereo && domain == 1) k = {(k[0] + k[1]) * .5, (k[0] - k[1]) * .5};
                for (int c = 0; c < channels; ++c) keySamples[size_t(c)] = keySplitter[size_t(c)].tick(k[size_t(c)], coefficients);
            }
            std::array<std::array<double, 4>, 2> gains{};
            const double parallel = mix.next();
            for (size_t i = 0; i < bandState.size(); ++i) {
                auto& state = bandState[i];
                const auto& v = settings.bands[i];
                const auto& source = key != nullptr ? keySamples : samples;
                const std::array<double, 2> detectorInput{source[0][i], source[1][i]};
                const auto level = state.detector.levels(detectorInput, channels,
                    settings.detector, rmsCoefficient, peakCoefficient, settings.link);
                const double bandWet = state.wet.next();
                const double makeup = state.makeup.next();
                const double solo = state.solo.next();
                for (int c = 0; c < 2; ++c) {
                    const auto index = size_t(c);
                    // A band set to one channel leaves the other at unity (and
                    // without makeup), so that channel passes unchanged.
                    const bool processed = v.channels == 0 || v.channels == c + 1 || channels == 1;
                    const double desired = v.bypass || !processed ? 0. : dynamicsGainDB(levelDB(level[index]), v);
                    auto& g = state.gainDB[index];
                    const bool increasing = std::abs(desired) > std::abs(g) || desired * g < 0.;
                    const double smoothing = increasing ? state.attack : state.release;
                    g = desired + smoothing * (g - desired);
                    if (std::abs(g) < 1.e-10) g = 0.;
                    const double branch = gainFromDB(g + (processed ? makeup : 0.));
                    // The dry component is each identically phase-compensated band,
                    // not unfiltered input. Parallel mix therefore cannot comb.
                    gains[index][i] = (1. + parallel * bandWet * (branch - 1.)) * solo;
                }
            }
            const double active = wet.next();
            // Approximately 6 kHz control rate at every sample rate; interpolation
            // has no host-block dependency and reaches the bypass endpoint exactly.
            const int length=wet.moving()?std::min(synthesisInterval,wet.remainingSamples()+1):synthesisInterval;
            // Quintic endpoints remove the gain-ramp slope at both boundaries.
            const auto fade=[](double x){return x*x*x*(10.+x*(-15.+6.*x));};
            const double future=fade(wet.valueAfter(length-1));
            const double amount=fade(active);
            auto forecast=gains;
            for(size_t c=0;c<2;++c)for(size_t i=0;i<4;++i) {
                forecast[c][i]=1.+future*(gains[c][i]-1.);
                gains[c][i]=1.+amount*(gains[c][i]-1.);
            }
            // Equal channel gains: one schedule for both (the linked default,
            // bit-identical to a single shared synthesis).
            if (shared && (channels == 1 || (gains[0] == gains[1] && forecast[0] == forecast[1]))) {
                synth[0].configure(gains[0], forecast[0], coefficients, length);
                synth[1].copySchedule(synth[0]);
            } else {
                for (size_t c = 0; c < 2; ++c) synth[c].configure(gains[c], forecast[c], coefficients, length);
                shared = gains[0] == gains[1] && forecast[0] == forecast[1] && synth[0].sameSchedule(synth[1]);
            }
            std::array<double, 2> wetOut{};
            for (int c = 0; c < channels; ++c) wetOut[size_t(c)] = synth[size_t(c)].tick(dry[size_t(c)], c, coefficients);
            if (stereo && domain == 1) wetOut = {wetOut[0] + wetOut[1], wetOut[0] - wetOut[1]};
            for (int c = 0; c < channels; ++c) if(active>0.) audio[c][n] = safeOutput(wetOut[size_t(c)]);
            if (switching == 1 && wet.value() == 0. && !wet.moving()) {
                // Faded out: change domain, settle silently, then fade in.
                domain = settings.stereo; switching = 2; settleLeft = settleSamples;
                for (auto& state : bandState) { state.detector.reset(); state.gainDB = {}; }
            } else if (switching == 2 && --settleLeft <= 0) {
                switching = 0;
                if (settings.on) wet.setTarget(1., rampSamples);
            }
        }
        for(size_t i=0;i<bandState.size();++i) {
            const auto& state=bandState[i];
            // The more reduced channel's change.
            const double g = std::abs(state.gainDB[0]) >= std::abs(state.gainDB[1]) ? state.gainDB[0] : state.gainDB[1];
            meters[i]=float(-levelDB(1.+mix.value()*state.wet.value()*(gainFromDB(g)-1.)));
        }
        if (!settings.on && !wet.moving()) meters = {};
    }

    float reduction(int band) const noexcept {
        return meters[size_t(std::clamp(band, 0, multibandBandCount - 1))];
    }
    std::array<float, 3> crossovers() const noexcept {
        return {frequencies[0].frequency(), frequencies[1].frequency(), frequencies[2].frequency()};
    }
    double sampleRate() const noexcept { return rate; }

private:
    struct BandState {
        multiband_detail::Detector detector;
        multiband_detail::Ramp makeup, wet, solo;
        std::array<double, 2> gainDB{};
        double attack{}, release{};
    };
    std::array<double, 2> domainInput(float* const* audio, int channels, int n, bool stereo) const noexcept {
        std::array<double, 2> x{};
        for (int c = 0; c < channels; ++c) x[size_t(c)] = std::isfinite(audio[c][n]) ? double(audio[c][n]) : 0.;
        if (stereo && domain == 1) return {(x[0] + x[1]) * .5, (x[0] - x[1]) * .5};
        return x;
    }
    bool anySolo() const noexcept {
        for (const auto& b : settings.bands) if (b.solo) return true;
        return false;
    }

    MultibandSettings settings{};
    double rate{48000.}, rmsCoefficient{}, peakCoefficient{};
    int rampSamples{480}, synthesisInterval{8}, settleSamples{1440};
    bool lastOn{}, shared{true};
    int domain{0}, switching{0}, settleLeft{0};   // switching: 0 idle, 1 fading out, 2 settling
    std::array<multiband_detail::CrossoverFrequency, 3> frequencies;
    std::array<multiband_detail::FourBandSplit, 2> splitter, keySplitter;
    bool keyed{};
    std::array<BandState, multibandBandCount> bandState;
    multiband_detail::Ramp wet, mix;
    std::array<multiband_detail::MinimumPhaseSynthesis, 2> synth;
    std::array<float, multibandBandCount> meters{};
};

struct DeEsserSettings {
    bool on{false};
    float frequency{6000.f};
    float threshold{-24.f};
    float range{6.f};
    float attack{2.f};
    float release{80.f};
    float ratio{4.f};
    float knee{6.f};
    bool listen{false};
};

// Split-band de-esser: only the high branch is attenuated. This is not a
// broadband compressor whose detector merely happens to be high-passed.
class DeEsserCore {
public:
    static constexpr int latencySamples() noexcept { return 0; }
    DeEsserCore() noexcept { prepare(48000.); }

    void setSettings(DeEsserSettings next) noexcept {
        using namespace multiband_detail;
        settings = next;
        settings.frequency = float(finiteClamp(settings.frequency, 1500.,
            std::min(16000., rate * .4), std::min(6000., rate * .4)));
        settings.threshold = float(finiteClamp(settings.threshold, -72., 0., -24.));
        settings.range = float(finiteClamp(settings.range, 0., 24., 6.));
        settings.attack = float(finiteClamp(settings.attack, .1, 100., 2.));
        settings.release = float(finiteClamp(settings.release, 5., 1000., 80.));
        settings.ratio = float(finiteClamp(settings.ratio, 1., 20., 4.));
        settings.knee = float(finiteClamp(settings.knee, 0., 24., 6.));
    }
    const DeEsserSettings& getSettings() const noexcept { return settings; }

    void prepare(double sampleRate) noexcept {
        using namespace multiband_detail;
        rate = validRate(sampleRate);
        setSettings(settings);
        rampSamples = std::max(1, int(std::ceil(rate * .01)));
        rmsCoefficient = coefficient(rate, 2.);
        peakCoefficient = coefficient(rate, 5.);
        reset();
    }
    void reset() noexcept {
        for (auto& s : splitter) s.reset();
        detector.reset();
        frequencySmoother.prepare(rate, settings.frequency);
        wet.reset(settings.on ? 1. : 0.);
        listen.reset(settings.listen ? 1. : 0.);
        gainDB = 0.;
        meter = 0.f;
    }

    void process(float* const* audio, int channelCount, int sampleCount) noexcept {
        using namespace multiband_detail;
        const int channels = std::clamp(channelCount, 0, 2);
        if (channels == 0 || sampleCount <= 0 || audio == nullptr) return;
        for (int c = 0; c < channels; ++c) if (audio[c] == nullptr) return;
        if (settings.on && wet.value() == 0. && !wet.moving()) {
            for (auto& s : splitter) s.reset();
            detector.reset();
            gainDB = 0.;
        }
        wet.setTarget(settings.on ? 1. : 0., rampSamples);
        if (!settings.on && !wet.moving()) { meter = 0.f; return; }
        listen.setTarget(settings.listen ? 1. : 0., rampSamples);
        frequencySmoother.setTarget(settings.frequency);
        const double attack = coefficient(rate, settings.attack);
        const double release = coefficient(rate, settings.release);
        MultibandBandSettings dynamics;
        dynamics.threshold = settings.threshold;
        dynamics.range = settings.range;
        dynamics.ratio = settings.ratio;
        dynamics.knee = settings.knee;

        for (int n = 0; n < sampleCount; ++n) {
            const auto& coefficients = frequencySmoother.next();
            std::array<double, 2> dry{}, low{}, high{};
            for (int c = 0; c < channels; ++c) {
                const auto i = size_t(c);
                dry[i] = std::isfinite(audio[c][n]) ? double(audio[c][n]) : 0.;
                const auto split = splitter[i].tick(dry[i], coefficients);
                low[i] = split[0];
                high[i] = split[1];
            }
            const double level = detector.tick(high, channels, 0, rmsCoefficient, peakCoefficient);
            const double desired = dynamicsGainDB(levelDB(level), dynamics);
            const double smoothing = desired < gainDB ? attack : release;
            gainDB = desired + smoothing * (gainDB - desired);
            if (std::abs(gainDB) < 1.e-10) gainDB = 0.;
            const double gain = gainFromDB(gainDB);
            const double audition = listen.next(), active = wet.next();
            for (int c = 0; c < channels; ++c) {
                const auto i = size_t(c);
                const double processed = low[i] + gain * high[i];
                // Listen auditions the unattenuated high-band detector signal.
                const double output = processed + audition * (high[i] - processed);
                if (active >= 1.) audio[c][n] = safeOutput(output);
                else if (active > 0.) audio[c][n] = safeOutput(dry[i] + active * (output - dry[i]));
            }
        }
        meter = !settings.on && !wet.moving() ? 0.f : float(-gainDB);
    }

    float reduction() const noexcept { return meter; }
    float frequency() const noexcept { return frequencySmoother.frequency(); }
    double sampleRate() const noexcept { return rate; }
private:
    DeEsserSettings settings{};
    double rate{48000.}, rmsCoefficient{}, peakCoefficient{}, gainDB{};
    int rampSamples{480};
    std::array<multiband_detail::LinkwitzRileySplit, 2> splitter;
    multiband_detail::CrossoverFrequency frequencySmoother;
    multiband_detail::Detector detector;
    multiband_detail::Ramp wet, listen;
    float meter{};
};

} // namespace spectralforge

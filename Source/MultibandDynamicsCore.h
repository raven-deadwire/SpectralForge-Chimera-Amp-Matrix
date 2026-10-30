#pragma once

#include <algorithm>
#include <array>
#include <complex>
#include <cmath>
#include <cstddef>
#include <limits>

namespace spectralforge {

inline constexpr int multibandBandCount = 4;
// Six-band expansion (docs/SIX_BAND_1_5.md): up to six bands, five crossovers.
inline constexpr int multibandMaxBandCount = 6;
inline constexpr int multibandMaxCrossovers = multibandMaxBandCount - 1;

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
    std::array<MultibandBandSettings, multibandMaxBandCount> bands{};
    // 4 or 6 bands. Crossovers 4 and 5 (`upperCrossovers`) and bands 5 and 6
    // apply only with six; four bands process exactly as before.
    int bandCount{4};
    std::array<float, 2> upperCrossovers{6000.f, 11000.f};
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

using CrossoverCoefficients = std::array<const SVFCoefficients*, multibandMaxCrossovers>;

// L_i + H_i = A_i, where A_i is a second-order allpass. With three crossovers
// the compensated cascade is [A3 A2 L1, A3 L2 H1, L3 H2 H1, H3 H2 H1]; with
// K crossovers band k is L_k H_(k-1)..H_1 followed by A_(k+1)..A_K. Its
// unity-gain sum is A_K..A_1, not the misaligned sum of a naive LR cascade.
// Hence settled crossovers reconstruct with flat MAGNITUDE. They do rotate
// phase: zero added sample delay does not mean linear/zero phase. Automation
// is smoothed, but the LTI allpass identity is not claimed during motion.
// Three crossovers compute exactly what the original four-band split did.
class BandSplit {
public:
    void reset() noexcept {
        for (auto& s : split) s.reset();
        for (auto& a : compensation) a.reset();
    }
    std::array<double, multibandMaxBandCount> tick(double input, const CrossoverCoefficients& c,
                                                   int crossovers) noexcept {
        std::array<double, multibandMaxBandCount> band{};
        double carry = input;
        for (int i = 0; i < crossovers; ++i) {
            const auto parts = split[size_t(i)].tick(carry, *c[size_t(i)]);
            band[size_t(i)] = parts[0];
            carry = parts[1];
        }
        band[size_t(crossovers)] = carry;
        size_t stage = 0;
        for (int k = 0; k + 1 < crossovers; ++k)
            for (int j = k + 1; j < crossovers; ++j)
                band[size_t(k)] = compensation[stage++].allpass(band[size_t(k)], *c[size_t(j)]);
        return band;
    }
private:
    std::array<LinkwitzRileySplit, multibandMaxCrossovers> split;
    std::array<SVF, multibandMaxCrossovers * (multibandMaxCrossovers - 1) / 2> compensation;
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

// Roots of a monic real polynomial of degree 1..5 (Aberth-Ehrlich), started
// from `roots` (previous solution or a guess). Returns false if the iteration
// did not converge to finite roots. Complex arithmetic is written out: the
// library operators' special-value handling costs several times more.
inline bool aberthRoots(const std::array<double, multibandMaxCrossovers + 1>& monic, int degree,
                        std::array<std::complex<double>, multibandMaxCrossovers>& roots,
                        int iterations) noexcept {
    std::array<double, multibandMaxCrossovers> re{}, im{};
    for (int i = 0; i < degree; ++i) { re[size_t(i)] = roots[size_t(i)].real(); im[size_t(i)] = roots[size_t(i)].imag(); }
    double previous = std::numeric_limits<double>::infinity();
    bool converged = false;
    for (int pass = 0; pass < iterations && !converged; ++pass) {
        double largest = 0.;
        for (int i = 0; i < degree; ++i) {
            const double zr = re[size_t(i)], zi = im[size_t(i)];
            double pr = 1., pi_ = 0., dr = 0., di = 0.;
            for (int j = degree - 1; j >= 0; --j) {
                const double nr = dr * zr - di * zi + pr, ni = dr * zi + di * zr + pi_;
                dr = nr; di = ni;
                const double qr = pr * zr - pi_ * zi + monic[size_t(j)], qi = pr * zi + pi_ * zr;
                pr = qr; pi_ = qi;
            }
            if (pr == 0. && pi_ == 0.) continue;
            const double dn = dr * dr + di * di;
            if (!(dn > 0.)) return false;
            const double rr = (pr * dr + pi_ * di) / dn, ri = (pi_ * dr - pr * di) / dn;   // p / p'
            double sr = 0., si = 0.;                                                    // sum 1 / (z - z_j)
            for (int j = 0; j < degree; ++j) if (j != i) {
                const double ar = zr - re[size_t(j)], ai = zi - im[size_t(j)], an = ar * ar + ai * ai;
                if (an > 0.) { sr += ar / an; si -= ai / an; }
            }
            const double er = 1. - (rr * sr - ri * si), ei = -(rr * si + ri * sr);       // 1 - ratio * sum
            const double en = er * er + ei * ei;
            if (!(en > 0.)) return false;
            const double wr = (rr * er + ri * ei) / en, wi = (ri * er - rr * ei) / en;
            if (!std::isfinite(wr) || !std::isfinite(wi)) return false;
            re[size_t(i)] = zr - wr; im[size_t(i)] = zi - wi;
            // A real root's imaginary part decays towards subnormal values,
            // which are slow to compute with; it is zero to working precision.
            if (std::abs(im[size_t(i)]) < 1.e-200 * std::abs(re[size_t(i)])) im[size_t(i)] = 0.;
            const double size = re[size_t(i)] * re[size_t(i)] + im[size_t(i)] * im[size_t(i)];
            largest = std::max(largest, std::sqrt((wr * wr + wi * wi) / std::max(1.e-300, size)));
        }
        // Converged, or stalled at rounding level (nearly equal roots limit
        // the attainable step, about 1e-14 relative).
        converged = largest < 1.e-13 || (largest < 1.e-8 && largest >= .5 * previous);
        previous = largest;
    }
    for (int i = 0; i < degree; ++i) roots[size_t(i)] = {re[size_t(i)], im[size_t(i)]};
    return converged;
}

// The LR4 analysis bank is unchanged. Its positive, phase-aligned band weights
// have magnitude N(w^4)/D(w^4). Spectral factorisation preserves that magnitude
// while choosing stable minimum-phase zeros. Unity gain is exact raw audio.
// Three crossovers solve the cubic N in closed form (the original four-band
// arithmetic); five solve the quintic numerically (Aberth-Ehrlich, warm-started
// from the previous schedule) and take each root's two left-half-plane fourth
// roots as zeros.
class MinimumPhaseSynthesis {
public:
    void reset() noexcept {
        for(auto& ch:filter)for(auto& pair:ch)for(auto& s:pair)s.reset();
        initialized=false; countdown=0; rootsValid=false;
    }
    // 3 or 5 crossovers. A change resets the filters.
    void setCrossoverCount(int count) noexcept {
        count = count == 5 ? 5 : 3;
        if (count != crossovers) { crossovers = count; reset(); weight={}; weightStep={}; targetWeight={}; }
    }
    int crossoverCount() const noexcept { return crossovers; }
    void invalidateSchedule() noexcept { countdown=0; }
    // Gain schedule (not the filter states) shared by both channels while
    // their band gains are equal; one calculation serves both.
    void copySchedule(const MinimumPhaseSynthesis& o) noexcept {
        weight=o.weight; overall=o.overall; targetGain=o.targetGain; gainStep=o.gainStep;
        weightStep=o.weightStep; targetWeight=o.targetWeight; countdown=o.countdown; initialized=o.initialized;
        roots=o.roots; rootsValid=o.rootsValid;
    }
    bool sameSchedule(const MinimumPhaseSynthesis& o) const noexcept {
        return weight==o.weight && overall==o.overall && targetGain==o.targetGain && gainStep==o.gainStep
            && weightStep==o.weightStep && targetWeight==o.targetWeight && countdown==o.countdown && initialized==o.initialized;
    }
    void unity() noexcept {
        weight={}; overall=1.; initialized=true; countdown=0;
    }
    void warm(double x,int channel,const CrossoverCoefficients& c) noexcept {
        for(size_t i=0;i<size_t(crossovers);++i) {
            auto& pair=filter[size_t(channel)][i];
            pair[1].tick(pair[0].tick(x,*c[i]).low,*c[i]);
        }
    }
    void configure(const std::array<double,multibandMaxBandCount>& actual,
                   const std::array<double,multibandMaxBandCount>& forecast,
                   const CrossoverCoefficients& c, int length) noexcept {
        const size_t k=size_t(crossovers);
        if(!initialized) {
            calculate(actual,c); initialized=true; countdown=0; weightStep={}; gainStep=0.;
            return;
        }
        if(countdown==0) {
            const auto previous=weight; const double previousGain=overall;
            calculate(forecast,c);
            for(size_t i=0;i<k;++i)for(size_t j=0;j<4;++j)weightStep[i][j]=(weight[i][j]-previous[i][j])/length;
            gainStep=(overall-previousGain)/length;
            targetWeight=weight; targetGain=overall;
            weight=previous; overall=previousGain; countdown=length;
        }
        for(size_t i=0;i<k;++i)for(size_t j=0;j<4;++j)weight[i][j]+=weightStep[i][j];
        overall+=gainStep;
        if(--countdown==0){weight=targetWeight;overall=targetGain;}
    }
    // Schedules whose quintic did not converge (the previous schedule is kept).
    int solverFailures() const noexcept { return failures; }
private:
    void calculate(const std::array<double,multibandMaxBandCount>& inputGain,
                   const CrossoverCoefficients& coefficients) noexcept {
        const size_t k=size_t(crossovers);
        std::array<double,multibandMaxCrossovers> g{},q{};
        for(size_t i=0;i<k;++i)g[i]=coefficients[i]->a2/coefficients[i]->a1;
        const double scale=g[k/2];
        for(size_t i=0;i<k;++i){const double a=g[i]/scale;q[i]=a*a*a*a;}
        std::array<double,multibandMaxBandCount> gain{};
        for(size_t i=0;i<=k;++i)gain[i]=std::max(1.e-8,inputGain[i]);
        bool equal=true;
        for(size_t i=1;i<=k;++i)equal=equal&&gain[i]==gain[0];
        if(equal) {
            overall=gain[k];
            weight={};
            return;
        }
        using C = std::complex<double>;
        // Numerator at a pole and its derivative: overall * prod(zero factor)^2.
        std::array<std::array<double,2>,3> factors{};
        std::array<C,2*multibandMaxCrossovers> zeros{};
        if(k==3) {
            overall=gain[3];
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
        } else if(!quinticZeros(gain,q,zeros)) {
            ++failures;
            return;   // keep the previous schedule
        } else overall=gain[k];
        // Fixed parallel poles: the synthesis states depend only on the input,
        // never on gain automation. Residues avoid moving zeros between filter
        // sections and the energy injection of time-varying cascade numerators.
        for(size_t i=0;i<k;++i) {
            const double gi=g[i]/scale;
            const C pole(-gi/butterworthK,gi/butterworthK);
            C numerator=overall, derivative=0.;
            if(k==3) {
                for(const auto& f:factors) {
                    const double b=f[0]/scale, c=f[1]/(scale*scale);
                    const C q=pole*pole+b*pole+c, dq=2.*pole+b;
                    derivative=derivative*q*q+numerator*2.*q*dq;
                    numerator*=q*q;
                }
            } else {
                for(size_t z=0;z<2*k;++z) {
                    const C linear=pole-zeros[z];
                    derivative=derivative*linear*linear+numerator*2.*linear;
                    numerator*=linear*linear;
                }
            }
            const C separation=pole-std::conj(pole);
            C denominator=separation*separation, logDerivative=2./separation;
            for(size_t j=0;j<k;++j) if(j!=i) {
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
    // Zeros (scaled s units) of the five-crossover numerator. N(x)/g5 is the
    // monic quintic sum over k<5 of g_k q_k x^k prod_(i>k)(q_i + x), plus x^5;
    // each root z of it contributes the two fourth roots of z with negative
    // real part.
    bool quinticZeros(const std::array<double,multibandMaxBandCount>& gain,
                      const std::array<double,multibandMaxCrossovers>& q,
                      std::array<std::complex<double>,2*multibandMaxCrossovers>& zeros) noexcept {
        using C = std::complex<double>;
        constexpr int degree=5;
        std::array<double,degree+1> poly{};
        poly[degree]=gain[degree];
        for(int band=0;band<degree;++band) {
            std::array<double,degree+1> term{};
            term[size_t(band)]=gain[size_t(band)]*q[size_t(band)];
            for(int i=band+1;i<degree;++i)
                for(int j=degree;j>=0;--j)
                    term[size_t(j)]=term[size_t(j)]*q[size_t(i)]+(j>0?term[size_t(j-1)]:0.);
            for(int j=0;j<=degree;++j)poly[size_t(j)]+=term[size_t(j)];
        }
        for(int j=0;j<degree;++j)poly[size_t(j)]/=poly[degree];
        // Scale the roots to about unit size: norm = max |a_j|^(1/(5-j)).
        const double norm=std::max({std::abs(poly[4]),std::sqrt(std::abs(poly[3])),std::cbrt(std::abs(poly[2])),
                                    std::sqrt(std::sqrt(std::abs(poly[1]))),std::pow(std::abs(poly[0]),.2),1.e-30});
        std::array<double,degree+1> monic{};
        double power=norm;
        for(int j=degree-1;j>=0;--j){monic[size_t(j)]=poly[size_t(j)]/power;power*=norm;}
        monic[degree]=1.;
        auto start=roots;
        bool solved=false;
        if(rootsValid) {
            for(auto& r:start)r/=norm;
            solved=aberthRoots(monic,degree,start,24);
        }
        if(!solved) {
            // The equal-gain roots are -q_i; rotated apart so complex roots are reachable.
            for(size_t i=0;i<size_t(degree);++i)start[i]=std::polar(std::max(1.e-300,q[i]/norm),pi-.3-.1*double(i));
            solved=aberthRoots(monic,degree,start,200);
        }
        if(!solved){rootsValid=false;return false;}
        for(size_t i=0;i<size_t(degree);++i) {
            roots[i]=start[i]*norm;
            const double radius=std::sqrt(std::sqrt(std::abs(roots[i])));
            const double angle=std::arg(roots[i]);
            std::array<C,4> fourth{};
            for(int m=0;m<4;++m)fourth[size_t(m)]=std::polar(radius,(angle+2.*pi*m)/4.);
            std::sort(fourth.begin(),fourth.end(),[](const C& a,const C& b){return a.real()<b.real();});
            zeros[2*i]=fourth[0]; zeros[2*i+1]=fourth[1];
        }
        rootsValid=true;
        return true;
    }
public:
    double tick(double x,int channel,const CrossoverCoefficients& c) noexcept {
        double y=overall*x;
        for(size_t i=0;i<size_t(crossovers);++i) {
            auto& pair=filter[size_t(channel)][i];
            const auto first=pair[0].tick(x,*c[i]);
            const auto second=pair[1].tick(first.low,*c[i]);
            y+=weight[i][0]*first.band+weight[i][1]*first.low
               +weight[i][2]*second.band+weight[i][3]*second.low;
        }
        return y;
    }

private:
    std::array<std::array<std::array<SVF,2>,multibandMaxCrossovers>,2> filter{};
    std::array<std::array<double,4>,multibandMaxCrossovers> weight{};
    double overall{1.}, targetGain{1.}, gainStep{};
    std::array<std::array<double,4>,multibandMaxCrossovers> weightStep{},targetWeight{};
    std::array<std::complex<double>,multibandMaxCrossovers> roots{};
    int countdown{}, crossovers{3}, failures{};
    bool initialized{}, rootsValid{};
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
    // Five ordered crossovers for six bands, the same rules (1.4 x spacing).
    static std::array<float, multibandMaxCrossovers> validatedSixBandCrossovers(
        std::array<float, multibandMaxCrossovers> input, double sampleRate) noexcept {
        using namespace multiband_detail;
        const double maximum = std::min(16000., .4 * validRate(sampleRate));
        constexpr double spacing = 1.4;
        constexpr std::array<double, multibandMaxCrossovers> defaults{120., 700., 3500., 6000., 11000.};
        std::array<float, multibandMaxCrossovers> out{};
        double previous = 0.;
        for (size_t i = 0; i < out.size(); ++i) {
            const double top = maximum / std::pow(spacing, double(out.size() - 1 - i));
            const double low = i == 0 ? 40. : previous * spacing;
            previous = std::clamp(finiteClamp(input[i], 40., maximum, defaults[i]), low, top);
            out[i] = float(previous);
        }
        return out;
    }

    void setSettings(const MultibandSettings& next) noexcept {
        using namespace multiband_detail;
        settings = next;
        settings.mix = float(finiteClamp(settings.mix, 0., 1., 1.));
        settings.detector = std::clamp(settings.detector, 0, 1);
        settings.stereo = std::clamp(settings.stereo, 0, 1);
        settings.link = float(finiteClamp(settings.link, 0., 1., 1.));
        settings.bandCount = settings.bandCount == 6 ? 6 : 4;
        if (settings.bandCount == 6) {
            const auto all = validatedSixBandCrossovers({settings.crossovers[0], settings.crossovers[1], settings.crossovers[2],
                                                         settings.upperCrossovers[0], settings.upperCrossovers[1]}, rate);
            settings.crossovers = {all[0], all[1], all[2]};
            settings.upperCrossovers = {all[3], all[4]};
        } else {
            settings.crossovers = validatedCrossovers(settings.crossovers, rate);
            settings.upperCrossovers = {float(finiteClamp(settings.upperCrossovers[0], 40., 16000., 6000.)),
                                        float(finiteClamp(settings.upperCrossovers[1], 40., 16000., 11000.))};
        }
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
            frequencies[i].prepare(rate, crossoverTarget(i));
        wet.reset(settings.on ? 1. : 0.);
        lastOn=settings.on;
        domain = settings.stereo; switching = 0; settleLeft = 0;
        active = targetCrossovers();
        for (auto& synthesis : synth) synthesis.setCrossoverCount(active);
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

        // A stereo-domain change (L/R <-> M/S) or band-count change while
        // running fades the wet path out, swaps the layout, feeds it silently
        // for 30 ms so its filters settle, and fades back in. Switched off, it
        // swaps at once.
        const bool stereo = channels == 2;
        if ((settings.stereo != domain || targetCrossovers() != active) && switching == 0) {
            if (!settings.on && !wet.moving()) swapLayout();
            else switching = 1;
        }
        if (settings.on && wet.value() == 0. && !wet.moving() && switching == 0) {
            for (auto& split : splitter) split.reset();
            for (auto& state : bandState) { state.detector.reset(); state.gainDB = {}; }
        }
        if (settings.on != lastOn) { for (auto& synthesis : synth) synthesis.invalidateSchedule(); lastOn=settings.on; }
        wet.setTarget(settings.on && switching == 0 ? 1. : 0., rampSamples);
        if (!settings.on && !wet.moving()) {
            meters = {};
            CrossoverCoefficients c{};
            // Keep only the fixed synthesis SVFs (two per crossover and channel)
            // warm. OFF samples are never rewritten; analysis/detectors remain
            // asleep until enabled.
            if (switching != 0) { swapLayout(); switching = 0; }
            for (auto& synthesis : synth) synthesis.unity();
            shared = true;
            for(int n=0;n<sampleCount;++n) {
                for(size_t i=0;i<size_t(active);++i)c[i]=&frequencies[i].next();
                const auto in = domainInput(audio, channels, n, stereo);
                for(int channel=0;channel<channels;++channel)
                    synth[size_t(channel)].warm(in[size_t(channel)],channel,c);
            }
            return;
        }

        mix.setTarget(settings.mix, rampSamples);
        for (size_t i = 0; i < frequencies.size(); ++i)
            frequencies[i].setTarget(crossoverTarget(i));
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
            const size_t crossoverCount = size_t(active), bandCount = crossoverCount + 1;
            CrossoverCoefficients coefficients{};
            for (size_t i = 0; i < crossoverCount; ++i)
                coefficients[i] = &frequencies[i].next();
            // Domain samples: L/R as they are, or M/S. In double, M + S and M - S
            // return L and R except for rounding (below 1e-16) where one channel
            // is many orders of magnitude smaller than the other.
            const auto dry = domainInput(audio, channels, n, stereo);
            std::array<std::array<double, multibandMaxBandCount>, 2> samples{};
            for (int c = 0; c < channels; ++c) samples[size_t(c)] = splitter[size_t(c)].tick(dry[size_t(c)], coefficients, active);
            std::array<std::array<double, multibandMaxBandCount>, 2> keySamples{};
            if (key != nullptr) {
                std::array<double, 2> k{};
                for (int c = 0; c < channels; ++c) { const float x = key[std::min(c, keyChannels - 1)][n];k[size_t(c)] = std::isfinite(x) ? double(x) : 0.; }
                if (stereo && domain == 1) k = {(k[0] + k[1]) * .5, (k[0] - k[1]) * .5};
                for (int c = 0; c < channels; ++c) keySamples[size_t(c)] = keySplitter[size_t(c)].tick(k[size_t(c)], coefficients, active);
            }
            std::array<std::array<double, multibandMaxBandCount>, 2> gains{};
            const double parallel = mix.next();
            for (size_t i = 0; i < bandCount; ++i) {
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
            for(size_t c=0;c<2;++c)for(size_t i=0;i<bandCount;++i) {
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
                // Faded out: change the layout, settle silently, then fade in.
                swapLayout(); switching = 2; settleLeft = settleSamples;
                for (auto& state : bandState) { state.detector.reset(); state.gainDB = {}; }
            } else if (switching == 2 && --settleLeft <= 0) {
                switching = 0;
                if (settings.on) wet.setTarget(1., rampSamples);
            }
        }
        meters = {};
        for(size_t i=0;i<=size_t(active);++i) {
            const auto& state=bandState[i];
            // The more reduced channel's change.
            const double g = std::abs(state.gainDB[0]) >= std::abs(state.gainDB[1]) ? state.gainDB[0] : state.gainDB[1];
            meters[i]=float(-levelDB(1.+mix.value()*state.wet.value()*(gainFromDB(g)-1.)));
        }
        if (!settings.on && !wet.moving()) meters = {};
    }

    float reduction(int band) const noexcept {
        return meters[size_t(std::clamp(band, 0, multibandMaxBandCount - 1))];
    }
    std::array<float, 3> crossovers() const noexcept {
        return {frequencies[0].frequency(), frequencies[1].frequency(), frequencies[2].frequency()};
    }
    // Applied crossovers 4 and 5 (their targets while four bands run).
    std::array<float, 2> upperCrossovers() const noexcept {
        if (active < multibandMaxCrossovers) return settings.upperCrossovers;
        return {frequencies[3].frequency(), frequencies[4].frequency()};
    }
    // Bands being processed (the setting applies after its fade).
    int activeBandCount() const noexcept { return active + 1; }
    int synthesisSolverFailures() const noexcept { return synth[0].solverFailures() + synth[1].solverFailures(); }
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
        for (int i = 0; i <= active; ++i) if (settings.bands[size_t(i)].solo) return true;
        return false;
    }
    int targetCrossovers() const noexcept { return settings.bandCount - 1; }
    double crossoverTarget(size_t i) const noexcept {
        return i < 3 ? settings.crossovers[i] : settings.upperCrossovers[i - 3];
    }
    // Applies the pending stereo domain and band count. A band-count change
    // starts the new filters from silence at their target frequencies.
    void swapLayout() noexcept {
        domain = settings.stereo;
        if (targetCrossovers() == active) return;
        active = targetCrossovers();
        for (auto& s : splitter) s.reset();
        for (auto& s : keySplitter) s.reset();
        for (size_t i = 3; i < frequencies.size(); ++i) frequencies[i].prepare(rate, crossoverTarget(i));
        for (auto& synthesis : synth) synthesis.setCrossoverCount(active);
        for (auto& state : bandState) { state.detector.reset(); state.gainDB = {}; }
    }

    MultibandSettings settings{};
    double rate{48000.}, rmsCoefficient{}, peakCoefficient{};
    int rampSamples{480}, synthesisInterval{8}, settleSamples{1440};
    bool lastOn{}, shared{true};
    int domain{0}, switching{0}, settleLeft{0};   // switching: 0 idle, 1 fading out, 2 settling
    int active{3};                                // crossovers being processed: 3 or 5
    std::array<multiband_detail::CrossoverFrequency, multibandMaxCrossovers> frequencies;
    std::array<multiband_detail::BandSplit, 2> splitter, keySplitter;
    bool keyed{};
    std::array<BandState, multibandMaxBandCount> bandState;
    multiband_detail::Ramp wet, mix;
    std::array<multiband_detail::MinimumPhaseSynthesis, 2> synth;
    std::array<float, multibandMaxBandCount> meters{};
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

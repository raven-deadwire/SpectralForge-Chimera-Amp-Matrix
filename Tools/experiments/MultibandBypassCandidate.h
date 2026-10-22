#pragma once
// Offline research only. Deliberately not included by Source/ or product CMake.
// Reuses the production LR4/dynamics core. No host parameters or saved state.
#include "MultibandDynamicsCore.h"

namespace spectralforge::research {
// Staged phase bridge: remove dynamics into the existing allpass reference,
// sweep that reference towards identity, then return exact raw samples.
// Cold ON warms the real core while sweeping the bridge in the other direction.
// This trades cancellation for time-varying phase (and potentially modulation).
// It is NOT a proven transparent bypass or a release implementation.
class MultibandBypassCandidate {
public:
    static constexpr int latencySamples() noexcept { return 0; }
    void setSettings(const MultibandSettings& s) noexcept {
        const bool on = s.on;
        requested = s;
        auto active = s; active.on = true;
        core.setSettings(active);
        requested = core.getSettings(); requested.on = on;
    }
    void prepare(double sr) noexcept {
        rate = multiband_detail::validRate(sr);
        core.prepare(rate); setSettings(requested);
        phaseSamples = int(std::ceil(rate * .100));
        effectSamples = int(std::ceil(rate * .010));
        settleSamples = int(std::ceil(rate * .040));
        reset();
    }
    void reset() noexcept {
        core.reset(); running = requested.on;
        phase.reset(running ? 1. : 0.); effect.reset(running ? 1. : 0.);
        settled = running ? settleSamples : 0;
        for (auto& channel : bridge) for (auto& f : channel) f.reset();
        for (size_t i=0;i<3;++i) frequency[i].prepare(rate, requested.crossovers[i]);
    }
    float reduction(int band) const noexcept { return running ? core.reduction(band) : 0.f; }
    void process(float* const* audio, int channelCount, int count) noexcept {
        using namespace multiband_detail;
        const int channels=std::clamp(channelCount,0,2);
        if (!audio || channels==0 || count<=0) return;
        for(int c=0;c<channels;++c) if(!audio[c]) return;
        if (!running) {
            if (!requested.on) return;
            reset(); // reset() starts active; explicitly start the cold bridge.
            phase.reset(0.); effect.reset(0.); settled=0; running=true;
        }
        for(size_t i=0;i<3;++i) frequency[i].setTarget(requested.crossovers[i]);
        // Bounded stack scratch, no heap/locks; independent of host block size.
        constexpr int capacity=128;
        std::array<std::array<float,capacity>,2> dry{};
        for(int offset=0;offset<count;offset+=capacity) {
            const int length=std::min(capacity,count-offset);
            float* p[2]{};
            for(int c=0;c<channels;++c) {
                p[c]=audio[c]+offset;
                std::copy_n(p[c],length,dry[size_t(c)].data());
            }
            core.process(p,channels,length);
            for(int n=0;n<length;++n) {
                if(requested.on) {
                    phase.setTarget(1.,phaseSamples);
                    if(phase.value()==1. && !phase.moving()) {
                        if(settled<settleSamples) ++settled;
                        if(settled>=settleSamples) effect.setTarget(1.,effectSamples);
                    }
                } else {
                    effect.setTarget(0.,effectSamples);
                    if(effect.value()==0. && !effect.moving()) phase.setTarget(0.,phaseSamples);
                }
                const double position=phase.next();
                if(position!=1.) settled=0;
                const double a=position*position*(3.-2.*position);
                const double blend=effect.next();
                std::array<SVFCoefficients,3> coefficients{};
                for(size_t i=0;i<3;++i) {
                    const auto& fixed=frequency[i].next();
                    if(a==1.) coefficients[i]=fixed;
                    else {
                        // Interpolate log(tan(pi*f/Fs)); fixed g=a2/a1.
                        constexpr double topG=636.6192487687196; // tan(.4995*pi)
                        const double g=std::exp((1.-a)*std::log(topG)+a*std::log(fixed.a2/fixed.a1));
                        coefficients[i].setG(g);
                    }
                }
                for(int c=0;c<channels;++c) {
                    const double raw=dry[size_t(c)][size_t(n)];
                    double reference=std::isfinite(raw)?raw:0.;
                    for(size_t i=0;i<3;++i) reference=bridge[size_t(c)][i].allpass(reference,coefficients[i]);
                    // Fade only the tiny near-Nyquist phase error at identity.
                    const double edge=std::min(1.,position*100.);
                    reference=raw+edge*(reference-raw);
                    if(position==0. && blend==0.) p[c][n]=dry[size_t(c)][size_t(n)];
                    else if(blend<1.) p[c][n]=safeOutput(reference+blend*(double(p[c][n])-reference));
                    // blend==1 leaves the production core output bit-exact.
                }
                if(!requested.on && position==0. && blend==0.) running=false;
            }
        }
    }
private:
    MultibandDynamicsCore core;
    MultibandSettings requested{};
    double rate{48000.};
    int phaseSamples{4800}, effectSamples{480}, settleSamples{1920}, settled{};
    bool running{};
    multiband_detail::Ramp phase,effect;
    std::array<multiband_detail::CrossoverFrequency,3> frequency;
    std::array<std::array<multiband_detail::SVF,3>,2> bridge;
};
} // namespace spectralforge::research

#include "MultibandDynamicsCore.h"

#include <array>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

static thread_local bool countAllocations = false;
static thread_local size_t allocations = 0;
// The indirect release also prevents GCC from mistaking this matched custom
// new/delete instrumentation for an accidental new/free mismatch after LTO.
static void (*volatile releaseMemory)(void*) = &std::free;
void* operator new(std::size_t n) {
    if (countAllocations) ++allocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { releaseMemory(p); }
void operator delete[](void* p) noexcept { releaseMemory(p); }
void operator delete(void* p, std::size_t) noexcept { releaseMemory(p); }
void operator delete[](void* p, std::size_t) noexcept { releaseMemory(p); }

using namespace spectralforge;
namespace {
constexpr double pi = multiband_detail::pi;
void check(bool okay, const char* message) {
    if (!okay) { countAllocations = false; throw std::runtime_error(message); }
}

template<class Processor>
void process(Processor& p, std::array<std::vector<float>, 2>& samples, int channels,
             const std::vector<int>& blocks = {137}) {
    size_t offset = 0, block = 0;
    while (offset < samples[0].size()) {
        const int n = std::min(int(samples[0].size() - offset), blocks[block++ % blocks.size()]);
        float* audio[2]{samples[0].data() + offset, samples[1].data() + offset};
        countAllocations = true;
        p.process(audio, channels, n);
        countAllocations = false;
        offset += size_t(n);
    }
}

std::array<std::vector<float>, 2> tones(double rate, double seconds,
                                      const std::vector<double>& frequencies,
                                      double amplitude, float stereoScale = -.25f) {
    const size_t length = size_t(rate * seconds);
    std::array<std::vector<float>, 2> audio{std::vector<float>(length), std::vector<float>(length)};
    for (size_t i = 0; i < length; ++i) {
        double x = 0.;
        for (const auto frequency : frequencies) x += amplitude * std::sin(2. * pi * frequency * double(i) / rate);
        audio[0][i] = float(x);
        audio[1][i] = audio[0][i] * stereoScale;
    }
    return audio;
}

double amplitude(const std::vector<float>& audio, double rate, double hz, size_t start = 0) {
    std::complex<double> sum{};
    for (size_t n = start; n < audio.size(); ++n)
        sum += double(audio[n]) * std::polar(1., -2. * pi * hz * double(n) / rate);
    return 2. * std::abs(sum) / double(audio.size() - start);
}

void bypassAndBlockShape() {
    for (const double rate : {44100., 48000., 96000., 192000.}) {
        for (const int channels : {1, 2}) {
            MultibandDynamicsCore mb;
            DeEsserCore ds;
            mb.prepare(rate); ds.prepare(rate);
            auto audio = tones(rate, .125, {40., 1300., 10000.}, .1);
            const auto dry = audio;
            process(mb, audio, channels, {0, 1, 31, 257, 2049});
            process(ds, audio, channels, {0, 1, 31, 257, 2049});
            check(audio == dry, "Disabled defaults did not preserve exact samples");
            check(mb.latencySamples() == 0 && ds.latencySamples() == 0, "Unexpected explicit delay");

            MultibandSettings setting;
            setting.on = true;
            mb.setSettings(setting); mb.reset();
            DeEsserSettings ess; ess.on = true;
            ds.setSettings(ess); ds.reset();
            auto single = dry, ragged = dry;
            process(mb, single, channels, {int(single[0].size())});
            mb.reset();
            process(mb, ragged, channels, {0, 1, 7, 63, 257, 1025});
            check(single == ragged, "Multiband output depends on host block partition");
            single = dry; ragged = dry;
            process(ds, single, channels, {int(single[0].size())});
            ds.reset();
            process(ds, ragged, channels, {0, 1, 7, 63, 257, 1025});
            check(single == ragged, "De-esser output depends on host block partition");
            if (channels == 1) check(single[1] == dry[1], "Mono process wrote another channel");

            setting.on = false; mb.setSettings(setting);
            ess.on = false; ds.setSettings(ess);
            auto settling = dry;
            process(mb, settling, channels); process(ds, settling, channels);
            auto settled = dry;
            process(mb, settled, channels); process(ds, settled, channels);
            check(settled == dry, "Effect did not return to exact bypass after ramp");
            check(mb.reduction(0) == 0.f && ds.reduction() == 0.f, "Bypassed meter is not zero");
        }
    }
    std::cout << "PASS exact default and restored bypass; zero/ragged/oversized blocks, mono/stereo, four rates\n";
}

void reconstructionAndParallelAlignment() {
    double worst = 0.;
    for (const double rate : {44100., 48000., 96000., 192000.}) {
        MultibandDynamicsCore mb;
        MultibandSettings s;
        s.on = true;
        for (auto& band : s.bands) band.ratio = 1.f;
        mb.setSettings(s); mb.prepare(rate);
        std::array<std::vector<float>, 2> impulse{std::vector<float>(65536), std::vector<float>(65536)};
        impulse[0][0] = .125f; impulse[1][0] = -.03125f;
        process(mb, impulse, 2, {0, 1, 31, 257, 2049});
        check(std::abs(impulse[0][0]) > 1.e-7f, "Neutral splitter inserted an explicit sample delay");
        double energy = 0.;
        for (size_t n = 0; n < impulse[0].size(); ++n) {
            energy += double(impulse[0][n]) * impulse[0][n];
            check(std::abs(impulse[1][n] + .25f * impulse[0][n]) < 1.e-8f, "Neutral stereo image changed");
        }
        check(std::abs(energy / (.125 * .125) - 1.) < 1.e-5, "Unity split sum is not allpass in energy");
        for (const double hz : {25., 40., 80., 120., 180., 350., 700., 1300., 2500., 3500., 5000., 10000., 15000., 18000.}) {
            std::complex<double> response{};
            for (size_t n = 0; n < impulse[0].size(); ++n)
                response += double(impulse[0][n]) * std::polar(1., -2. * pi * hz * double(n) / rate);
            const double db = 20. * std::log10(std::abs(response) / .125);
            worst = std::max(worst, std::abs(db));
            check(std::abs(db) < .001, "Phase-compensated LR sum has a crossover magnitude notch");
        }
        s.mix = .5f;
        for (auto& band : s.bands) band.makeup = -6.f;
        mb.setSettings(s); mb.reset();
        const std::vector<double> frequencies{40., 120., 700., 3500., 11000.};
        auto audio = tones(rate, .5, frequencies, .08);
        process(mb, audio, 2);
        const double expected = 20. * std::log10(.5 + .5 * std::pow(10., -6. / 20.));
        for (const double hz : frequencies) {
            const double measured = 20. * std::log10(amplitude(audio[0], rate, hz, size_t(rate * .25)) / .08);
            check(std::abs(measured - expected) < .01, "Parallel mix blended an unaligned dry reference");
        }
    }
    std::cout << "PASS LR4 magnitude reconstruction, magnitude error <= " << worst
              << " dB; parallel mix flat across crossovers at four rates\n";
}

// Independent z-domain oracle: bilinear transformed Butterworth^2, rather
// than a second copy of the production state-variable filter recurrence.
std::array<std::complex<double>, 4> analyticalBands(
        double rate, double hz, const std::array<float, 3>& crossovers) {
    const auto zInverse = std::polar(1., -2. * pi * hz / rate);
    const auto q = (1. - zInverse) / (1. + zInverse);
    std::array<std::complex<double>, 3> low, high, allpass;
    for (size_t i = 0; i < 3; ++i) {
        const auto u = q / std::tan(pi * crossovers[i] / rate);
        const auto denominator = 1. + std::sqrt(2.) * u + u * u;
        low[i] = 1. / (denominator * denominator);
        high[i] = u * u * u * u * low[i];
        allpass[i] = low[i] + high[i];
    }
    return {allpass[2] * allpass[1] * low[0], allpass[2] * low[1] * high[0],
            low[2] * high[1] * high[0], high[2] * high[1] * high[0]};
}

void weightedPhaseAndMixAudit() {
    double worstComplexError = 0.;
    for (const double rate : {44100., 48000., 96000., 192000.}) {
        for (const auto crossovers : {std::array<float, 3>{120, 700, 3500},
                                     std::array<float, 3>{40, 56, 78.4f},
                                     std::array<float, 3>{7000, 10000, 16000}}) {
            MultibandSettings s; s.on = true; s.crossovers = crossovers;
            for (size_t b = 0; b < 4; ++b) {
                s.bands[b].ratio = 1.f;
                s.bands[b].makeup = std::array<float, 4>{-6, 3, -9, 6}[b];
            }
            for (float mix : {0.f, .5f, 1.f}) {
                s.mix = mix;
                MultibandDynamicsCore mb; mb.setSettings(s); mb.prepare(rate);
                std::array<std::vector<float>, 2> impulse{std::vector<float>(65536), std::vector<float>(65536)};
                impulse[0][0] = .125f;
                process(mb, impulse, 1, {0, 1, 31, 257, 2049});
                for (double hz : {31., 40., 56., 80., 120., 700., 3500., 10000., 16000.}) {
                    const auto bands = analyticalBands(rate, hz, mb.getSettings().crossovers);
                    std::complex<double> expected{}, measured{};
                    for (size_t b = 0; b < 4; ++b)
                        expected += bands[b] * (1. + mix * (std::pow(10., s.bands[b].makeup / 20.) - 1.));
                    for (size_t n = 0; n < impulse[0].size(); ++n)
                        measured += double(impulse[0][n]) * std::polar(1., -2. * pi * hz * double(n) / rate) / .125;
                    const double error = std::abs(std::abs(measured) - std::abs(expected));
                    worstComplexError = std::max(worstComplexError, error);
                    check(error < 2.e-6, "Weighted band sum has incorrect LR4 magnitude or internal mix");
                }
            }
        }
    }
    // Mix operates on the aligned band gains before minimum-phase synthesis.
    // Its zero endpoint and all-band bypass must both reconstruct raw samples.
    MultibandSettings s; s.on = true; s.mix = 0.f;
    MultibandDynamicsCore mb; mb.setSettings(s); mb.prepare(48000.);
    auto dry = tones(48000., .25, {80., 700., 3500.}, .1);
    auto zeroMix = dry; process(mb, zeroMix, 2);
    s.mix = 1.f; for (auto& b : s.bands) b.bypass = true;
    mb.setSettings(s); mb.reset(); auto bandBypass = dry; process(mb, bandBypass, 2);
    check(zeroMix == bandBypass, "All-band bypass and zero mix lost their common phase reference");
    check(zeroMix == dry, "Neutral synthesis is not exact raw audio");
    std::cout << "PASS weighted LR4 magnitude and exact raw endpoints; max amplitude error "
              << worstComplexError << '\n';
}

void bypassTransitionAudit() {
    for (const double rate : {44100., 48000., 96000., 192000.}) {
        MultibandSettings s; s.on = true;
        for (auto& band : s.bands) band.ratio = 1.f;
        MultibandDynamicsCore mb; mb.setSettings(s); mb.prepare(rate);
        auto input = tones(rate, .55, {700.}, .25, 1.f);
        // Independent quadrature signals measure the neutral LTV envelope;
        // two identical linked channels would hide a phase-dependent notch.
        for (size_t n = 0; n < input[1].size(); ++n)
            input[1][n] = float(.25 * std::cos(2. * pi * 700. * double(n) / rate));
        auto fullWet = input; process(mb, fullWet, 2);
        mb.reset(); auto switched = input;
        const int at = int(rate * .5), ramp = int(std::ceil(rate * .01));
        float* audio[]{switched[0].data(), switched[1].data()};
        mb.process(audio, 2, at); s.on = false; mb.setSettings(s);
        const auto before = mb;
        auto ragged = switched;
        audio[0] += at; audio[1] += at;
        mb.process(audio, 2, int(switched[0].size()) - at);
        auto fragmented = before;
        for (int n = at; n < int(ragged[0].size()); ++n) {
            float* p[]{&ragged[0][size_t(n)], &ragged[1][size_t(n)]};
            fragmented.process(p, 2, 1);
        }
        check(switched == ragged, "Bypass transition depends on host block boundaries");
        double minimum = 1.;
        for (int n = at; n < int(switched[0].size()); ++n) {
            const double wet = std::max(0., 1. - double(n - at + 1) / ramp);
            for (size_t c = 0; c < 2; ++c) {
                const size_t i = size_t(n);
                const double expected = input[c][i] + wet * (double(fullWet[c][i]) - input[c][i]);
                check(std::abs(switched[c][i] - expected) < 1.e-7,
                      "Global bypass has an extra discontinuity beyond its defined phase crossfade");
                if (n >= at + ramp - 1)
                    check(switched[c][i] == input[c][i], "Bypass failed to reach exact raw endpoint in 10 ms");
            }
            if (n < at + ramp)
                minimum = std::min(minimum, std::hypot(switched[0][size_t(n)], switched[1][size_t(n)]) / .25);
        }
        check(switched == input, "Neutral global transition modulates audio");
        std::cout << "CHARACTERIZATION " << rate << " Hz: 700 Hz global OFF min envelope "
                  << 20. * std::log10(minimum) << " dB (fixed neutral bypass)\n";
    }
    std::cout << "PASS bypass ramp continuity/partition and exact endpoint; neutral samples unchanged\n";
}

// Nonuniform dynamics, Solo and crossover motion exercise the synthesis beyond
// the neutral regression. Same event samples, two host block partitions.
void synthesisAutomationAndPartition() {
    for(double rate : {44100.,48000.,96000.,192000.}) {
        auto render = [&](bool fragmented) {
            MultibandDynamicsCore core;
            MultibandSettings settings;
            settings.crossovers={40,56,78.4f};
            for(size_t b=0;b<4;++b) {
                auto& band=settings.bands[b];
                band.makeup=b%2 ? 24.f : -24.f;
                band.ratio=4;band.range=36;band.attack=.1f;band.release=5;
                band.mode=int(b);band.threshold=-48;
            }
            core.setSettings(settings);core.prepare(rate);
            auto audio=tones(rate,.24,{27.5,41.2034,700,3500},.0001);
            const int event=std::max(1,int(rate*.003));
            for(int offset=0, index=0;offset<int(audio[0].size());++index) {
                settings.on=(index%7)<4;
                if(index%9==0)settings.crossovers=index%18==0
                    ?std::array<float,3>{40,56,78.4f}:std::array<float,3>{7000,10000,16000};
                settings.bands[size_t(index%4)].solo=index%5==0;
                settings.mix=float(index%11)/10.f;
                core.setSettings(settings);
                const int length=std::min(event,int(audio[0].size())-offset);
                for(int n=0;n<length;) {
                    const int count=fragmented?std::min(1+n%31,length-n):length;
                    float* p[]{audio[0].data()+offset+n,audio[1].data()+offset+n};
                    countAllocations=true;core.process(p,2,count);countAllocations=false;
                    n+=count;
                }
                offset+=length;
            }
            for(size_t n=0;n<audio[0].size();++n) {
                check(std::isfinite(audio[0][n])&&std::abs(audio[0][n])<8.f,
                      "Nonuniform synthesis automation became unstable");
                check(audio[1][n]==-.25f*audio[0][n],"Synthesis broke linked stereo");
            }
            return audio;
        };
        check(render(false)==render(true),"Synthesis scheduling depends on host block size");
    }
    std::cout<<"PASS nonuniform extremes, four modes, Solo, crossover/mix motion and 3 ms reversals; exact partition/stereo\n";
}

void bandIsolation() {
    const double rate = 48000.;
    const std::vector<double> frequencies{40., 300., 1600., 12000.};
    for (int selected = 0; selected < 4; ++selected) {
        MultibandDynamicsCore mb;
        MultibandSettings settings;
        settings.on = true;
        settings.detector = 0;
        for (auto& b : settings.bands) { b.bypass = true; b.knee = 0.f; }
        auto& band = settings.bands[size_t(selected)];
        band.bypass = false; band.threshold = -48.f; band.range = 6.f;
        band.ratio = 20.f; band.attack = .1f;
        mb.setSettings(settings); mb.prepare(rate);
        auto audio = tones(rate, 1., frequencies, .1);
        process(mb, audio, 2);
        std::cout << "band " << selected + 1 << " measured dB:";
        for (int f = 0; f < 4; ++f) {
            const double db = 20. * std::log10(amplitude(audio[0], rate, frequencies[size_t(f)], 24000) / .1);
            std::cout << ' ' << db;
            if (f == selected) check(db < -5. && db > -6.05, "Selected multiband tone did not receive independent compression");
            else check(std::abs(db) < .45, "Multiband processing attenuated a remote band");
        }
        std::cout << '\n';
        check(std::abs(mb.reduction(selected) - 6.f) < .005f, "Selected band range or GR meter incorrect");
        for (size_t n = 0; n < audio[0].size(); ++n)
            check(std::abs(audio[1][n] + .25f * audio[0][n]) < 2.e-7f, "Independent band dynamics broke stereo linking");
        settings.bands[size_t(selected)].solo = true;
        settings.bands[size_t(selected)].bypass = true;
        mb.setSettings(settings); mb.reset();
        audio = tones(rate, 1., frequencies, .1);
        process(mb, audio, 1);
        const double own = amplitude(audio[0], rate, frequencies[size_t(selected)], 24000);
        check(own > .09, "Band solo did not audition selected band");
        const int distant = selected < 2 ? 3 : 0;
        check(amplitude(audio[0], rate, frequencies[size_t(distant)], 24000) < .001,
              "Band solo retained an unrelated distant band");
    }
    std::cout << "PASS four independent band cuts, linked stereo, range meters and band solo\n";
}

void curvesAndEnvelopes() {
    MultibandBandSettings curve;
    curve.threshold = -24.f; curve.ratio = 4.f; curve.knee = 6.f; curve.range = 36.f;
    check(std::abs(multiband_detail::dynamicsGainDB(-24., curve) + .5625) < 1.e-12,
          "Soft-knee midpoint differs from the documented continuous curve");
    const std::array<float, 4> inputDB{-12.f, -50.f, -50.f, -12.f};
    const std::array<float, 4> threshold{-30.f, -30.f, -40.f, -22.f};
    const std::array<float, 4> ratio{4.f, 4.f, 2.f, 2.f};
    const std::array<double, 4> expected{-13.5, 15., -10., 10.};
    for (int mode = 0; mode < 4; ++mode) {
        MultibandDynamicsCore mb;
        MultibandSettings setting;
        setting.on = true; setting.detector = 1;
        setting.crossovers = {80.f, 400.f, 1200.f};
        for (auto& b : setting.bands) b.bypass = true;
        auto& b = setting.bands[3];
        b.bypass = false; b.mode = mode; b.threshold = threshold[size_t(mode)];
        b.ratio = ratio[size_t(mode)]; b.range = 36.f; b.knee = 0.f; b.attack = .1f;
        mb.setSettings(setting); mb.prepare(48000.);
        // The declared input level is RMS. A 12 kHz sample-peak detector would
        // vary with the splitter's phase at only four samples per cycle.
        const double in = std::sqrt(2.) * std::pow(10., inputDB[size_t(mode)] / 20.);
        auto audio = tones(48000., 1., {12000.}, in);
        process(mb, audio, 2);
        const double measured = 20. * std::log10(amplitude(audio[0], 48000., 12000., 24000) / in);
        std::cout << "mode " << mode << " gain " << measured << " dB; expected "
                  << expected[size_t(mode)] << "; meter " << mb.reduction(3) << '\n';
        check(std::abs(measured - expected[size_t(mode)]) < .08,
              "Measured downward/upward compression or expansion slope is incorrect");
        check(std::abs(measured + mb.reduction(3)) < .06, "Signed band meter differs from applied measured gain");
        b.range = 3.f; mb.setSettings(setting); mb.reset();
        audio = tones(48000., 1., {12000.}, in);
        process(mb, audio, 2);
        check(std::abs(mb.reduction(3)) <= 3.001f && std::abs(mb.reduction(3)) > 2.99f,
              "Compression/expansion gain exceeded or failed to reach its finite range");
    }

    MultibandDynamicsCore mb;
    MultibandSettings s; s.on = true; s.detector = 0;
    for (auto& band : s.bands) band.bypass = true;
    auto& b = s.bands[3]; b.bypass = false; b.threshold = -48.f;
    b.attack = 100.f; b.release = 80.f; b.ratio = 20.f; b.knee = 0.f;
    mb.setSettings(s); mb.prepare(48000.);
    auto first = tones(48000., .01, {12000.}, .25);
    process(mb, first, 2);
    const float early = mb.reduction(3);
    auto sustained = tones(48000., .5, {12000.}, .25);
    process(mb, sustained, 2);
    check(early < 1.f && mb.reduction(3) > 5.8f, "Band attack did not follow the specified slow envelope");
    for (auto& channel : sustained) std::fill(channel.begin(), channel.end(), 0.f);
    process(mb, sustained, 2);
    process(mb, sustained, 2);
    check(mb.reduction(3) < .02f, "Band release did not recover after silence");
    std::cout << "PASS measured four-mode slopes, soft knee, finite range, signed GR, attack and release\n";
}

void deEsserSelectivity() {
    for (const double rate : {44100., 48000., 96000., 192000.}) {
        DeEsserCore ds;
        DeEsserSettings settings;
        settings.on = true; settings.frequency = 4000.f; settings.threshold = -48.f;
        settings.range = 9.f; settings.ratio = 20.f; settings.knee = 0.f; settings.attack = .1f;
        ds.setSettings(settings); ds.prepare(rate);
        auto audio = tones(rate, .5, {220., 12000.}, .2);
        process(ds, audio, 2);
        const double low = 20. * std::log10(amplitude(audio[0], rate, 220., size_t(rate * .25)) / .2);
        const double high = 20. * std::log10(amplitude(audio[0], rate, 12000., size_t(rate * .25)) / .2);
        check(std::abs(low) < .02, "De-esser compressed low frequencies along with high frequencies");
        check(high < -8.4 && high > -9.02, "De-esser did not attenuate its high band");
        check(ds.reduction() > 8.99f && ds.reduction() < 9.001f, "De-esser range or GR mismatch");
        for (size_t n = 0; n < audio[0].size(); ++n)
            check(std::abs(audio[1][n] + .25f * audio[0][n]) < 2.e-7f, "De-esser stereo linking changed image");
        ds.reset();
        // Let the release finish after the unavoidable hard start of the
        // sine. That start contains a short HF transient; its later release
        // is distinct from false triggering by the sustained low-frequency tone.
        audio = tones(rate, 1., {220.}, .9);
        process(ds, audio, 2);
        check(ds.reduction() < .001f, "Low-frequency signal triggered the high-band detector");
        settings.listen = true; ds.setSettings(settings); ds.reset();
        audio = tones(rate, .5, {220., 12000.}, .2);
        process(ds, audio, 1);
        check(amplitude(audio[0], rate, 220., size_t(rate * .25)) < .0001,
              "De-esser listen did not isolate detector frequencies");
        check(amplitude(audio[0], rate, 12000., size_t(rate * .25)) > .19,
              "De-esser listen did not audition the unattenuated detector band");
    }
    std::cout << "PASS de-esser split-band selectivity, detector rejection, finite range, stereo link and listen at four rates\n";
}

void automatedCrossoversAndInvalidParameters() {
    const double rate = 192000.;
    MultibandDynamicsCore mb;
    MultibandSettings s; s.on = true;
    for (auto& band : s.bands) band.ratio = 1.f;
    mb.setSettings(s); mb.prepare(rate);
    std::array<std::array<float, 257>, 3> block{};
    uint32_t rng = 0x923451;
    float maximum = 0.f;
    for (int k = 0; k < 1800; ++k) {
        rng = 1664525u * rng + 1013904223u;
        s.crossovers = {float(40 + (rng & 16383)), float(40 + ((rng >> 8) & 16383)),
                        float(40 + ((rng >> 16) & 16383))};
        if (k % 97 == 0) s.crossovers = {16000.f, 40.f, std::numeric_limits<float>::quiet_NaN()};
        mb.setSettings(s);
        const auto target = mb.getSettings().crossovers;
        check(target[1] >= 1.3999f * target[0] && target[2] >= 1.3999f * target[1]
              && target[2] <= float(rate * .4), "Reversed/malformed crossovers were not constrained");
        for (size_t n = 0; n < block[0].size(); ++n) {
            const double sample = double(k * 257 + int(n));
            block[0][n] = float(.12 * (std::sin(2. * pi * 50. * sample / rate)
                                    + std::sin(2. * pi * 12000. * sample / rate)));
            block[1][n] = -.25f * block[0][n];
            block[2][n] = .123f;
        }
        float* audio[]{block[0].data(), block[1].data(), block[2].data()};
        countAllocations = true; mb.process(audio, 3, 257); countAllocations = false;
        const auto applied = mb.crossovers();
        check(applied[0] < applied[1] && applied[1] < applied[2], "Smoothed crossovers crossed each other");
        for (size_t n = 0; n < block[0].size(); ++n) {
            check(std::isfinite(block[0][n]) && std::abs(block[0][n]) < 2.f,
                  "Automated crossover became unstable or produced excessive transient gain");
            check(block[2][n] == .123f, "Processor touched an unsupported extra channel");
            maximum = std::max(maximum, std::abs(block[0][n]));
        }
    }
    for (auto& band : s.bands) {
        band.threshold = std::numeric_limits<float>::quiet_NaN();
        band.ratio = std::numeric_limits<float>::infinity();
        band.attack = -1.f; band.release = 0.f;
        band.makeup = std::numeric_limits<float>::quiet_NaN();
    }
    s.mix = std::numeric_limits<float>::quiet_NaN();
    mb.setSettings(s); mb.reset();
    auto audio = tones(rate, .05, {100., 10000.}, .1);
    process(mb, audio, 2);
    for (const auto& channel : audio) for (const auto value : channel)
        check(std::isfinite(value), "Malformed host parameter poisoned multiband state");
    std::cout << "PASS rapidly automated/reversed crossovers, finite host sanitation, bounded extra-channel handling; peak="
              << maximum << '\n';
}
}

// Stereo link and M/S (docs/MULTIBAND_STEREO_1_5.md). Band 3 (700 Hz-3.5 kHz)
// compresses; the others are bypassed.
double toneAmplitude(const std::vector<double>& x, double rate, double hz, size_t start) {
    double re = 0., im = 0.;
    for (size_t i = start; i < x.size(); ++i) {const double w = 2. * pi * hz * double(i) / rate; re += x[i] * std::cos(w); im += x[i] * std::sin(w);}
    return 2. * std::sqrt(re * re + im * im) / double(x.size() - start);
}
MultibandSettings stereoSettings(int stereo, float link, int channels) {
    MultibandSettings s; s.on = true; s.mix = 1.f; s.detector = 1; s.stereo = stereo; s.link = link;
    for (auto& b : s.bands) b.bypass = true;
    auto& b = s.bands[2];
    b.bypass = false; b.mode = 0; b.threshold = -40.f; b.ratio = 4.f; b.range = 24.f; b.knee = 0.f; b.attack = 1.f; b.release = 50.f; b.channels = channels;
    return s;
}
// Two tones: `a` on the first channel (L or M) and `b` on the second (R or S).
std::array<std::vector<float>, 2> pair(double rate, double seconds, double hzA, double dbA, double hzB, double dbB, bool midSide) {
    const size_t length = size_t(rate * seconds);
    std::array<std::vector<float>, 2> audio{std::vector<float>(length), std::vector<float>(length)};
    const double ga = std::pow(10., dbA / 20.), gb = std::pow(10., dbB / 20.);
    for (size_t i = 0; i < length; ++i) {
        const double a = ga * std::sin(2. * pi * hzA * double(i) / rate), b = gb * std::sin(2. * pi * hzB * double(i) / rate);
        audio[0][i] = float(midSide ? a + b : a); audio[1][i] = float(midSide ? a - b : b);
    }
    return audio;
}
void stereoLinkAndMidSide() {
    constexpr double rate = 48000.;
    const size_t tail = size_t(rate * 1.);
    // Link: a loud left and a quiet right (below threshold).
    double linked = 0., free = 0., half = 0., left = 0.;
    for (const float link : {1.f, 0.f, .5f}) {
        MultibandDynamicsCore core; core.prepare(rate); core.setSettings(stereoSettings(0, link, 0)); core.reset();
        auto input = pair(rate, 1.5, 1000., -12., 1000., -48., false), output = input;
        process(core, output, 2);
        std::vector<double> inL(input[0].begin(), input[0].end()), inR(input[1].begin(), input[1].end()), outL(output[0].begin(), output[0].end()), outR(output[1].begin(), output[1].end());
        const double l = 20. * std::log10(toneAmplitude(outL, rate, 1000., tail) / toneAmplitude(inL, rate, 1000., tail));
        const double r = 20. * std::log10(toneAmplitude(outR, rate, 1000., tail) / toneAmplitude(inR, rate, 1000., tail));
        (link == 1.f ? linked : link == 0.f ? free : half) = r;
        if (link == 1.f) left = l;
    }
    std::cout << "MEASURE multiband link: loud left " << left << " dB; quiet right " << linked << " dB at 100 %, " << half << " dB at 50 %, " << free << " dB at 0 %\n";
    check(std::abs(linked - left) < .05 && std::abs(free) < .05 && half < free - 1. && half > linked + 1., "Stereo link does not blend the detectors");
    // M/S: a 1 kHz mid tone and a 1.5 kHz side tone, both loud; a band set to
    // one of them leaves the other unchanged.
    for (const int channels : {1, 2}) {
        MultibandDynamicsCore core; core.prepare(rate); core.setSettings(stereoSettings(1, 1.f, channels)); core.reset();
        auto input = pair(rate, 1.5, 1000., -12., 1500., -12., true), output = input;
        process(core, output, 2);
        std::vector<double> mid(output[0].size()), side(output[0].size());
        for (size_t i = 0; i < mid.size(); ++i) {mid[i] = (double(output[0][i]) + output[1][i]) * .5; side[i] = (double(output[0][i]) - output[1][i]) * .5;}
        const double m = 20. * std::log10(toneAmplitude(mid, rate, 1000., tail) / std::pow(10., -12. / 20.));
        const double sd = 20. * std::log10(toneAmplitude(side, rate, 1500., tail) / std::pow(10., -12. / 20.));
        std::cout << "MEASURE multiband M/S, band set to " << (channels == 1 ? "Mid" : "Side") << ": mid " << m << " dB, side " << sd << " dB\n";
        check(channels == 1 ? (m < -6. && std::abs(sd) < .05) : (sd < -6. && std::abs(m) < .05), "A Mid- or Side-only band touches the other channel");
    }
    // M/S at unity returns the input: L/R -> M/S -> L/R rounds only where one
    // channel is many orders of magnitude below the other.
    {
        auto settings = stereoSettings(1, 1.f, 0); settings.bands[2].bypass = true;
        MultibandDynamicsCore core; core.prepare(rate); core.setSettings(settings); core.reset();
        auto input = pair(rate, .5, 1000., -6., 1700., -9., false), output = input;
        process(core, output, 2);
        double worst = 0.;
        for (size_t c = 0; c < 2; ++c) for (size_t i = 0; i < input[c].size(); ++i) worst = std::max(worst, std::abs(double(input[c][i]) - output[c][i]));
        std::cout << "MEASURE multiband M/S at unity: largest |output - input| " << worst << "\n";
        check(worst < 1e-12, "M/S at unity does not return the input");
    }
    // Switching L/R -> M/S mid-programme: fade out, 30 ms settle, fade in.
    // During those ~50 ms the stage passes the input, so the level may rise to
    // the unprocessed signal's; there must be no discontinuity: the output's
    // second difference never exceeds the input's own.
    {
        MultibandDynamicsCore core; core.prepare(rate); core.setSettings(stereoSettings(0, 1.f, 0)); core.reset();
        auto audio = pair(rate, 1.5, 1000., -12., 1500., -12., true);
        const auto input = audio;
        const auto curvature = [](const std::vector<float>& x, size_t from, size_t to) {
            double m = 0.; for (size_t i = std::max<size_t>(from, 2); i < to; ++i) m = std::max(m, std::abs(double(x[i]) - 2. * x[i - 1] + x[i - 2])); return m; };
        for (size_t offset = 0; offset < audio[0].size(); offset += 256) {
            if (offset >= size_t(rate * .75)) core.setSettings(stereoSettings(1, 1.f, 2));
            const int n = int(std::min<size_t>(256, audio[0].size() - offset));
            float* buffers[2]{audio[0].data() + offset, audio[1].data() + offset};
            countAllocations = true; core.process(buffers, 2, n); countAllocations = false;
        }
        const size_t from = size_t(rate * .75), to = size_t(rate * .9);
        const double across = std::max(curvature(audio[0], from, to), curvature(audio[1], from, to));
        const double reference = std::max(curvature(input[0], from, to), curvature(input[1], from, to));
        std::cout << "MEASURE multiband L/R -> M/S switch: largest second difference during the change " << across << " (input's own " << reference << ")\n";
        check(across <= 1.02 * reference, "Changing the stereo mode makes a discontinuity");
    }
    std::cout << "PASS multiband stereo link 0-100 %, Mid/Side-only bands, M/S at unity returns the input, click-free mode change\n";
}

int main() {
    try {
        const auto start = std::chrono::steady_clock::now();
        bypassAndBlockShape();
        reconstructionAndParallelAlignment();
        weightedPhaseAndMixAudit();
        bypassTransitionAudit();
        synthesisAutomationAndPartition();
        bandIsolation();
        curvesAndEnvelopes();
        deEsserSelectivity();
        automatedCrossoversAndInvalidParameters();
        stereoLinkAndMidSide();
        check(allocations == 0, "Multiband or de-esser processing allocated memory");
        std::cout << "PASS no callback allocations; all independent dynamics core contracts ("
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s test runtime)\n";
        return 0;
    } catch (const std::exception& e) {
        countAllocations = false;
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}

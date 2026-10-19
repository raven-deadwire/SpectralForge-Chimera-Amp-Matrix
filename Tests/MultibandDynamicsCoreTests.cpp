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
    std::cout << "PASS compensated LR allpass reconstruction, magnitude error <= " << worst
              << " dB; parallel mix flat across crossovers at four rates\n";
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

int main() {
    try {
        const auto start = std::chrono::steady_clock::now();
        bypassAndBlockShape();
        reconstructionAndParallelAlignment();
        bandIsolation();
        curvesAndEnvelopes();
        deEsserSelectivity();
        automatedCrossoversAndInvalidParameters();
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

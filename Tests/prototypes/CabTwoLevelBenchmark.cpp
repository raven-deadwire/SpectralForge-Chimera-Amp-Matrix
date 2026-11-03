// PROTOTYPE benchmark (docs/CAB_TWO_LEVEL_CONVOLUTION_DESIGN.md). Not a CTest.
// Compares the product's uniform ModeledCabConvolution with the two-level
// prototype on six independent 85 ms responses (three cabs x two mics), stereo:
//  - accuracy against the current engine and against direct double-precision
//    convolution;
//  - audio-thread wall and thread-CPU time per callback, with callbacks paced
//    in real time as a host does (the tail worker's deadline is real time);
//  - tail samples the worker delivered late (must be zero).
#include "CabTwoLevelConvolution.h"
#include "../CabTimingDiagnostics.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <thread>

using namespace spectralforge;
using Clock = std::chrono::steady_clock;

namespace {
juce::AudioBuffer<float> response(double rate, unsigned seed) {
    // Same length as the authored modeled responses: ceil(rate * 0.085).
    const int length = int(std::ceil(rate * .085));
    juce::AudioBuffer<float> r(1, length);
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.f, 1.f);
    for (int i = 0; i < length; ++i)
        r.setSample(0, i, noise(rng) * std::exp(-float(i) / float(rate * .012)) * .05f);
    return r;
}
struct Stats { double p50{}, p99{}, cpuP99{}, max{}; int misses{}; };
Stats stats(std::vector<double> wall, std::vector<double> cpu, double budget) {
    Stats s; s.misses = int(std::count_if(wall.begin(), wall.end(), [budget](double v) { return v > budget; }));
    std::sort(wall.begin(), wall.end()); std::sort(cpu.begin(), cpu.end());
    const auto at = [](const std::vector<double>& v, double q) { return v[size_t(double(v.size() - 1) * q)]; };
    s.p50 = at(wall, .5); s.p99 = at(wall, .99); s.cpuP99 = at(cpu, .99); s.max = wall.back(); return s;
}
void fillInput(juce::AudioBuffer<float>& b, std::mt19937& rng) {
    std::uniform_real_distribution<float> d(-.25f, .25f);
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) b.setSample(c, i, d(rng));
}

// Run `callbacks` paced blocks through `process(lane, buffer)` for six lanes.
template <class Process>
Stats timed(int callbacks, double rate, int block, Process&& process, std::vector<std::vector<float>>* capture = nullptr) {
    const juce::ScopedNoDenormals noDenormals;
    std::array<juce::AudioBuffer<float>, 6> lanes;
    for (auto& l : lanes) l.setSize(2, block);
    std::vector<double> wall, cpu; wall.reserve(size_t(callbacks)); cpu.reserve(size_t(callbacks));
    std::mt19937 rng(7);
    const auto period = std::chrono::duration<double>(double(block) / rate);
    const auto origin = Clock::now();
    for (int n = 0; n < callbacks; ++n) {
        std::this_thread::sleep_until(origin + std::chrono::duration_cast<Clock::duration>(period * n));
        for (auto& l : lanes) fillInput(l, rng);
        const auto before = cabTiming::readThreadClock(); const auto start = Clock::now();
        for (int lane = 0; lane < 6; ++lane) process(lane, lanes[size_t(lane)]);
        const auto end = Clock::now(); const auto after = cabTiming::readThreadClock();
        wall.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        cpu.push_back(cabTiming::Sample{size_t(n), wall.back(), before, after}.cpuUs());
        if (capture) for (int lane = 0; lane < 6; ++lane) for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i) (*capture)[size_t(lane * 2 + c)].push_back(lanes[size_t(lane)].getSample(c, i));
    }
    return stats(wall, cpu, 1e6 * block / rate);
}

double directMaxError(const juce::AudioBuffer<float>& ir, const std::vector<float>& input, const std::vector<float>& output) {
    double worst = 0, peak = 0;
    for (size_t t = 0; t < output.size(); ++t) {
        double y = 0;
        for (int k = 0; k < ir.getNumSamples() && size_t(k) <= t; ++k) y += double(ir.getSample(0, k)) * input[t - size_t(k)];
        worst = std::max(worst, std::abs(y - output[t])); peak = std::max(peak, std::abs(y));
    }
    return worst / std::max(peak, 1e-12);
}
} // namespace

int main(int argc, char** argv) {
    const int callbacks = argc > 1 ? std::atoi(argv[1]) : 1600;
    const int partition = argc > 2 ? std::atoi(argv[2]) : 512;
    const int headLength = 2 * partition;
    std::printf("TWO_LEVEL_PROTOCOL callbacks=%d lanes=6 channels=2 partition=%d head=%d paced=realtime\n", callbacks, partition, headLength);
    int failures = 0;
    for (double rate : {44100., 48000., 96000.}) for (int block : {32, 64, 128, 256}) {
        if (block > partition) continue;
        std::array<juce::AudioBuffer<float>, 6> irs;
        for (unsigned m = 0; m < 6; ++m) irs[m] = response(rate, 100 + m);

        // Accuracy: both engines, one stereo lane, identical input; and the
        // prototype against direct convolution for one second.
        double vsCurrent = 0, vsDirect = 0; unsigned accuracyLate = 0;
        {
            ModeledCabConvolution current(irs[0], block, false);
            prototype::TwoLevelConvolution proto(irs[0], block, headLength, partition);
            std::counting_semaphore<> idle{0};
            const int samples = int(rate);
            std::vector<float> in(static_cast<size_t>(samples)), outCurrent, outProto;
            std::mt19937 rng(11); std::uniform_real_distribution<float> d(-.25f, .25f);
            for (auto& x : in) x = d(rng);
            juce::AudioBuffer<float> a(2, block), b(2, block);
            for (int at = 0; at + block <= samples; at += block) {
                for (int i = 0; i < block; ++i) { a.setSample(0, i, in[size_t(at + i)]); a.setSample(1, i, -in[size_t(at + i)]); }
                b.makeCopyOf(a, true);
                current.process(a);
                proto.process(b, idle);
                // Single-threaded accuracy run: service the tail right away.
                if (auto* t = proto.tailPart()) while (t->service()) {}
                for (int i = 0; i < block; ++i) { outCurrent.push_back(a.getSample(0, i)); outProto.push_back(b.getSample(0, i)); }
            }
            double peak = 0;
            for (size_t i = 0; i < outCurrent.size(); ++i) { vsCurrent = std::max(vsCurrent, double(std::abs(outCurrent[i] - outProto[i]))); peak = std::max(peak, double(std::abs(outCurrent[i]))); }
            vsCurrent /= std::max(peak, 1e-12);
            in.resize(outProto.size());
            vsDirect = directMaxError(irs[0], in, outProto);
            if (auto* t = proto.tailPart()) accuracyLate = t->lateSamples();
        }

        // Timing: current engine.
        std::vector<std::unique_ptr<ModeledCabConvolution>> current;
        for (auto& ir : irs) current.push_back(std::make_unique<ModeledCabConvolution>(ir, block, false));
        const auto base = timed(callbacks, rate, block, [&](int lane, juce::AudioBuffer<float>& b) { current[size_t(lane)]->process(b); });

        // Timing: two-level, one shared worker.
        std::vector<std::unique_ptr<prototype::TwoLevelConvolution>> proto;
        prototype::TailWorker worker;
        for (auto& ir : irs) { proto.push_back(std::make_unique<prototype::TwoLevelConvolution>(ir, block, headLength, partition)); if (auto* t = proto.back()->tailPart()) worker.add(t); }
        worker.start();
        const auto two = timed(callbacks, rate, block, [&](int lane, juce::AudioBuffer<float>& b) { proto[size_t(lane)]->process(b, worker.semaphore()); });
        worker.stop();
        unsigned late = 0; for (auto& p : proto) if (auto* t = p->tailPart()) late += t->lateSamples();

        const double budget = 1e6 * block / rate;
        const bool accurate = vsCurrent < 2e-5 && vsDirect < 2e-5;
        if (!accurate || late || accuracyLate) ++failures;
        std::printf("TWO_LEVEL sr=%.0f block=%d budget_us=%.1f | current p50=%.1f p99=%.1f cpu_p99=%.1f misses=%d | two_level p50=%.1f p99=%.1f cpu_p99=%.1f misses=%d late_samples=%u worker_blocks=%lld | p99_ratio=%.2f | rel_err_vs_current=%.2e rel_err_vs_direct=%.2e\n",
                    rate, block, budget, base.p50, base.p99, base.cpuP99, base.misses,
                    two.p50, two.p99, two.cpuP99, two.misses, late, worker.blocks.load(),
                    two.p99 / base.p99, vsCurrent, vsDirect);
        std::fflush(stdout);
    }
    std::printf(failures ? "TWO_LEVEL_RESULT failures=%d\n" : "TWO_LEVEL_RESULT ok failures=%d\n", failures);
    return failures ? 1 : 0;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "realtime-stats.hpp"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace spectralforge::obs;
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
bool near(double a, double b, double tolerance = 1e-9) { return std::abs(a - b) <= tolerance; }

constexpr double rate = 48000;
constexpr int block = 480;                          // 10 ms
constexpr std::uint64_t blockNs = 10'000'000;

void continuousInputHasNoGaps() {
    RealtimeStats stats;
    std::uint64_t t = 5'000'000'000ull;
    for (int i = 0; i < 1000; ++i, t += blockNs) check(stats.arrive(t, block, rate) == InputGap::None, "continuous input");
    // Device timestamp jitter inside the tolerance is not a gap.
    check(stats.arrive(t + 3'000'000, block, rate) == InputGap::None, "3 ms jitter is not a gap");
    check(stats.snapshot().gaps == 0, "no gaps counted");
}

void gapsAreClassified() {
    RealtimeStats stats;
    std::uint64_t t = 1'000'000'000ull;
    stats.arrive(t, block, rate);
    stats.finish(block, rate, 0, true);
    t += blockNs + 20'000'000; // 20 ms of input missing
    check(stats.arrive(t, block, rate) == InputGap::Discontinuity, "forward jump");
    t += blockNs - 20'000'000; // overlapping timestamps
    check(stats.arrive(t, block, rate) == InputGap::Discontinuity, "backward jump");
    t += blockNs + 2'000'000'000ull; // the device stopped for two seconds
    check(stats.arrive(t, block, rate) == InputGap::Restart, "resume after silence");
    t += blockNs;
    check(stats.arrive(t, block, rate) == InputGap::None, "continuous again");
    // A media source looping back to its start is not a restart.
    check(stats.arrive(t - 60'000'000'000ull, block, rate) == InputGap::Discontinuity, "large backward jump");
    const auto s = stats.snapshot();
    check(s.gaps == 4 && s.restarts == 1, "gap counters");
}

void loadIsMeasuredAgainstAudioTime() {
    RealtimeStats stats;
    stats.finish(block, rate, 1'000'000, true);   // first 20 ms window: 4 ms busy
    stats.finish(block, rate, 3'000'000, true);
    stats.finish(block, rate, 12'000'000, false); // second window: 22 ms busy, over real time, one block muted
    stats.finish(block, rate, 10'000'000, true);
    stats.finish(block, rate, 5'000'000, true);   // window still open: not yet in the peak
    const auto s = stats.snapshot();
    check(s.blocks == 5 && s.frames == 5 * block, "block and frame counts");
    check(near(s.audioSeconds, 0.05), "audio duration");
    check(near(s.averageLoad, 31.0 / 50.0, 1e-6), "average load");
    check(near(s.peakLoad, 1.1, 1e-6), "peak load over a 20 ms window");
    check(s.overruns == 1 && s.unprocessed == 1, "overrun and unprocessed counts");
}

// One sample per call costs a fixed overhead far above that sample's 21 us;
// the window judges the stretch of audio, not the call.
void tinyBlocksAreJudgedByWindow() {
    RealtimeStats stats;
    stats.finish(1, rate, 100'000, true); // 100 us for one sample
    for (int i = 0; i < 2000; ++i) stats.finish(1, rate, 1'000, true);
    const auto s = stats.snapshot();
    check(s.overruns == 0 && s.peakLoad < 0.2, "a single slow tiny block is not an overrun");
}

void reportsOncePerPeriod() {
    RealtimeStats stats(100'000'000); // every 100 ms of audio
    int reports = 0;
    for (int i = 0; i < 100; ++i) reports += stats.finish(block, rate, 0, true) ? 1 : 0; // one second
    check(reports == 10, "one report per period of audio");
    check(!RealtimeStats(0).finish(block, rate, 0, true), "period 0 never reports");
}

void countingStartsWithTheFirstProcessedBlock() {
    RealtimeStats stats;
    std::uint64_t t = 0;
    for (int i = 0; i < 50; ++i, t += blockNs) { // Chimera still loading
        stats.arrive(t, block, rate);
        stats.finish(block, rate, 0, false);
    }
    stats.arrive(t + 30'000'000, block, rate);   // a gap before Chimera ever processed
    check(stats.snapshot().blocks == 0 && stats.snapshot().gaps == 0, "nothing counted before the first processed block");
    stats.finish(block, rate, 1'000'000, true);
    stats.arrive(t + 40'000'000, block, rate);
    stats.finish(block, rate, 1'000'000, false); // a later failure is counted
    const auto s = stats.snapshot();
    check(s.blocks == 2 && s.unprocessed == 1 && s.gaps == 0, "counting after Chimera started");
}

void resetAppliesAtTheNextBlock() {
    RealtimeStats stats;
    stats.arrive(0, block, rate);
    stats.finish(block, rate, 20'000'000, true);
    stats.finish(block, rate, 20'000'000, false);
    stats.requestReset();
    check(stats.snapshot().blocks == 0, "a pending reset reads as zero");
    stats.arrive(blockNs, block, rate);
    stats.finish(block, rate, 1'000'000, true);
    const auto s = stats.snapshot();
    check(s.blocks == 1 && s.overruns == 0 && s.unprocessed == 0 && s.peakLoad == 0 && near(s.averageLoad, 0.1, 1e-6),
          "cleared, then counting");
}

// A reader on another thread (the OBS UI or a script) never blocks the writer
// and never sees counters run backwards. Run under TSan in CI.
void concurrentReadersAreSafe() {
    RealtimeStats stats;
    std::atomic<bool> done{false};
    std::thread reader([&] {
        std::uint64_t last = 0;
        while (!done.load()) {
            const auto s = stats.snapshot();
            check(s.blocks >= last, "counter ran backwards");
            last = s.blocks;
        }
    });
    std::uint64_t t = 0;
    for (int i = 0; i < 200000; ++i, t += blockNs) {
        stats.arrive(t, block, rate);
        stats.finish(block, rate, 1'000'000, true);
    }
    done.store(true);
    reader.join();
    check(stats.snapshot().blocks == 200000, "every block counted");
}
}

int main() {
    try {
        continuousInputHasNoGaps();
        gapsAreClassified();
        loadIsMeasuredAgainstAudioTime();
        tinyBlocksAreJudgedByWindow();
        reportsOncePerPeriod();
        countingStartsWithTheFirstProcessedBlock();
        resetAppliesAtTheNextBlock();
        concurrentReadersAreSafe();
        std::cout << "PASS realtime stats: gap classification, load/overrun/unprocessed counts, windowed peak, start at first processed block, periodic report, reset, concurrent readers\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}

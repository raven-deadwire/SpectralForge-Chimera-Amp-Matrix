// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace spectralforge::obs {
// Long-session evidence for the OBS filter (add-on plan stage D): how much of
// the audio's real time Chimera used, 20 ms stretches of audio (about one OBS
// audio tick) that took longer than real time to process, gaps in the input's timestamps (a stopped or reconnected device, a paused
// media source) and blocks that could not be processed. No libobs dependency.
//
// Counting starts with the first block Chimera processes, so the seconds while
// OBS starts and the plugin loads are not reported as unprocessed audio, and a
// filter that never finds Chimera reports no audio at all.
//
// One writer, the OBS audio thread (arrive/finish). snapshot() and
// requestReset() are safe from any thread. Allocation- and lock-free.
enum class InputGap {
    None,
    Discontinuity, // timestamps jumped by more than gapTolerance
    Restart,       // input resumed after at least restartGap of silence
};

struct RealtimeSnapshot {
    std::uint64_t blocks{}, frames{}, overruns{}, gaps{}, restarts{}, unprocessed{};
    // Load is processing time / audio time; the peak and the overruns are
    // measured over windows of windowNs of audio, so a source that delivers
    // tiny blocks is not judged by one call's fixed cost.
    double audioSeconds{}, averageLoad{}, peakLoad{};
};

class RealtimeStats {
public:
    static constexpr std::int64_t gapToleranceNs = 5'000'000;     // 5 ms
    static constexpr std::int64_t restartGapNs = 1'000'000'000;   // 1 s
    static constexpr std::uint64_t windowNs = 20'000'000;         // 20 ms

    explicit RealtimeStats(std::uint64_t reportEveryNs = 600'000'000'000ull) noexcept : reportEvery(reportEveryNs) {}

    // Audio thread, before processing a block that starts at timestampNs.
    InputGap arrive(std::uint64_t timestampNs, int frames, double sampleRate) noexcept {
        if (resetRequested.exchange(false)) clear();
        InputGap gap = InputGap::None;
        if (haveExpected) {
            const auto delta = static_cast<std::int64_t>(timestampNs - expectedNext);
            if (delta >= restartGapNs) gap = InputGap::Restart;
            else if (delta > gapToleranceNs || delta < -gapToleranceNs) gap = InputGap::Discontinuity;
            if (started && gap != InputGap::None) add(gaps);
            if (started && gap == InputGap::Restart) add(restarts);
        }
        expectedNext = timestampNs + durationNs(frames, sampleRate);
        haveExpected = true;
        return gap;
    }

    // Audio thread, after the block. Returns true once per reportEvery of audio
    // so the owner can log a progress line off this thread.
    bool finish(int frames, double sampleRate, std::int64_t elapsedNs, bool processed) noexcept {
        if (!started && !processed) return false;
        started = true;
        const std::uint64_t audio = durationNs(frames, sampleRate);
        const std::uint64_t busy = static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsedNs));
        add(blocks);
        add(framesTotal, static_cast<std::uint64_t>(std::max(0, frames)));
        add(audioNs, audio);
        add(busyNs, busy);
        windowAudio += audio;
        windowBusy += busy;
        if (windowAudio >= windowNs) {
            const std::uint64_t ppm = windowBusy * 1'000'000ull / windowAudio;
            if (ppm > peakPpm.load(std::memory_order_relaxed)) peakPpm.store(ppm, std::memory_order_relaxed);
            if (windowBusy > windowAudio) add(overruns);
            windowAudio = windowBusy = 0;
        }
        if (!processed) add(unprocessed);
        if (reportEvery == 0 || audioNs.load(std::memory_order_relaxed) < nextReport) return false;
        nextReport += reportEvery;
        return true;
    }

    // Any thread. Takes effect at the next block; reads as zero until then.
    void requestReset() noexcept { resetRequested.store(true); }

    RealtimeSnapshot snapshot() const noexcept {
        RealtimeSnapshot s;
        if (resetRequested.load()) return s;
        const auto get = [](const std::atomic<std::uint64_t>& v) { return v.load(std::memory_order_relaxed); };
        s.blocks = get(blocks);
        s.frames = get(framesTotal);
        s.overruns = get(overruns);
        s.gaps = get(gaps);
        s.restarts = get(restarts);
        s.unprocessed = get(unprocessed);
        const auto audio = get(audioNs);
        s.audioSeconds = static_cast<double>(audio) * 1e-9;
        s.averageLoad = audio > 0 ? static_cast<double>(get(busyNs)) / static_cast<double>(audio) : 0.0;
        s.peakLoad = static_cast<double>(get(peakPpm)) * 1e-6;
        return s;
    }

private:
    static std::uint64_t durationNs(int frames, double sampleRate) noexcept {
        return frames > 0 && sampleRate > 0 ? static_cast<std::uint64_t>(frames * 1.0e9 / sampleRate + 0.5) : 0;
    }
    // Single writer: a relaxed load/store pair is enough and never locks.
    static void add(std::atomic<std::uint64_t>& v, std::uint64_t n = 1) noexcept {
        v.store(v.load(std::memory_order_relaxed) + n, std::memory_order_relaxed);
    }
    void clear() noexcept {
        for (auto* v : {&blocks, &framesTotal, &overruns, &gaps, &restarts, &unprocessed, &audioNs, &busyNs, &peakPpm})
            v->store(0, std::memory_order_relaxed);
        nextReport = reportEvery;
        windowAudio = windowBusy = 0;
    }

    std::atomic<std::uint64_t> blocks{0}, framesTotal{0}, overruns{0}, gaps{0}, restarts{0}, unprocessed{0};
    std::atomic<std::uint64_t> audioNs{0}, busyNs{0}, peakPpm{0};
    std::atomic<bool> resetRequested{false};
    // Audio thread only.
    std::uint64_t expectedNext{}, reportEvery, nextReport{reportEvery}, windowAudio{}, windowBusy{};
    bool haveExpected{}, started{};
};
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio-bridge.hpp"
#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace spectralforge::obs;
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }

// Doubles every sample and records the contract the bridge actually honoured.
struct FakeEngine {
    double rate{}; int channels{}, capacity{}, largestBlock{}, calls{}, failAfter{-1}, reportedLatency{64};
    bool prepare(double r, int c, int m) { rate = r; channels = c; capacity = m; largestBlock = 0; calls = 0; return true; }
    bool process(float* const* data, int c, int frames) noexcept {
        if (c != channels || frames > capacity) return false;
        largestBlock = std::max(largestBlock, frames);
        if (failAfter >= 0 && calls++ >= failAfter) return false;
        for (int ch = 0; ch < c; ++ch) for (int n = 0; n < frames; ++n) data[ch][n] *= 2.f;
        return true;
    }
    int latency() const noexcept { return reportedLatency; }
};

// A failing processor is allowed to have already modified its in-place input.
// Also changes the UI policy mid-call to check the tick uses one policy snapshot.
struct PartialFailureEngine {
    bool fail{}, switchPolicy{};
    AudioBridge<PartialFailureEngine>* bridge{};
    int calls{};
    bool prepare(double, int, int) { calls = 0; return true; }
    int latency() const noexcept { return 0; }
    bool process(float* const* data, int channels, int frames) noexcept {
        ++calls;
        for (int c = 0; c < channels; ++c)
            for (int n = 0; n < frames; ++n) data[c][n] *= 2.f;
        if (switchPolicy) bridge->setFailurePolicy(FailurePolicy::Mute);
        return !fail;
    }
};

float input(int channel, int sample) { return static_cast<float>((sample * 37 + channel * 19) % 101 - 50) / 128.f; }

struct Planes {
    std::vector<std::vector<float>> data; std::array<float*, 8> pointers{};
    Planes(int channels, int frames, int start = 0) : data(static_cast<size_t>(channels)) {
        for (int c = 0; c < channels; ++c) {
            data[c].resize(static_cast<size_t>(frames)); pointers[c] = data[c].data();
            for (int n = 0; n < frames; ++n) data[c][n] = input(c, start + n);
        }
    }
    float at(int c, int n) const { return data[c][n]; }
};
}

int main() {
    try {
        FakeEngine engine;
        AudioBridge<FakeEngine> bridge(engine);
        { Planes p(2, 64); bridge.process(p.pointers.data(), 2, 64, 48000);
          check(bridge.status() == BridgeStatus::Unprepared && p.at(0, 3) == 0.f && p.at(1, 9) == 0.f, "unprepared must mute"); }

        // OBS 5.1: Chimera gets front L/R only; other planes pass through untouched.
        check(bridge.prepare(48000, 6, 1024) && engine.channels == 2 && bridge.latencySamples() == 64, "prepare 5.1");
        for (int frames : {1, 480, 1024, 1025, 4801}) {
            Planes p(6, frames); bridge.process(p.pointers.data(), 6, frames, 48000);
            for (int c = 0; c < 6; ++c) for (int n = 0; n < frames; ++n)
                check(p.at(c, n) == input(c, n) * (c < 2 ? 2.f : 1.f), "stereo processing / surround passthrough");
            check(engine.largestBlock <= 1024, "oversized OBS tick must be chunked to prepared capacity");
        }
        check(bridge.status() == BridgeStatus::Processing, "processing status");
        { Planes p(6, 256); bridge.process(p.pointers.data(), 6, 256, 44100);
          for (int c = 0; c < 6; ++c) check(p.at(c, 100) == 0.f, "mute policy must silence every surround plane"); }

        // Interface input 1 carries the guitar: copy it to both sides before DSP.
        const std::array<std::pair<InputMapping, const char*>, 3> mappings{{{InputMapping::LeftToBoth, "left"}, {InputMapping::RightToBoth, "right"}, {InputMapping::SumToBoth, "sum"}}};
        for (const auto& [m, name] : mappings) {
            bridge.setInputMapping(m);
            Planes p(2, 300); bridge.prepare(48000, 2, 256); bridge.process(p.pointers.data(), 2, 300, 48000);
            for (int n = 0; n < 300; ++n) {
                const float l = input(0, n), r = input(1, n);
                const float want = 2.f * (m == InputMapping::LeftToBoth ? l : m == InputMapping::RightToBoth ? r : 0.5f * (l + r));
                check(p.at(0, n) == want && p.at(1, n) == want, name);
            }
        }
        bridge.setInputMapping(InputMapping::Stereo);

        // Mono OBS layout prepares a mono engine; mapping cannot read a missing plane.
        bridge.setInputMapping(InputMapping::RightToBoth);
        check(bridge.prepare(44100, 1, 512) && engine.channels == 1, "mono prepare");
        { Planes p(1, 700); bridge.process(p.pointers.data(), 1, 700, 44100);
          for (int n = 0; n < 700; ++n) check(p.at(0, n) == 2.f * input(0, n), "mono processing"); }
        bridge.setInputMapping(InputMapping::Stereo);

        // A layout or rate change needs an allocating prepare: mute (default) or
        // pass dry, report it, then resume once the original format returns.
        bridge.prepare(48000, 2, 1024);
        for (auto policy : {FailurePolicy::Mute, FailurePolicy::Dry}) {
            bridge.setFailurePolicy(policy);
            Planes rate(2, 128); bridge.process(rate.pointers.data(), 2, 128, 44100);
            check(bridge.needsPrepare() && rate.at(0, 5) == (policy == FailurePolicy::Dry ? input(0, 5) : 0.f), "rate change policy");
            Planes layout(1, 128); bridge.process(layout.pointers.data(), 1, 128, 48000);
            check(bridge.needsPrepare() && layout.at(0, 5) == (policy == FailurePolicy::Dry ? input(0, 5) : 0.f), "layout change policy");
            Planes missing(2, 128); missing.pointers[1] = nullptr; bridge.process(missing.pointers.data(), 2, 128, 48000);
            check(bridge.needsPrepare(), "missing plane is a format fault");
            Planes back(2, 128); bridge.process(back.pointers.data(), 2, 128, 48000);
            check(bridge.status() == BridgeStatus::Processing && back.at(1, 7) == 2.f * input(1, 7), "resume on original format");
        }
        bridge.setFailurePolicy(FailurePolicy::Mute);

        // Engine failure mutes the whole tick (including earlier wet chunks) and
        // stays latched until re-prepared, so the stream does not flap wet/dry.
        engine.failAfter = 1; bridge.prepare(48000, 2, 256); engine.failAfter = 1;
        { Planes p(2, 1000); bridge.process(p.pointers.data(), 2, 1000, 48000);
          check(bridge.status() == BridgeStatus::EngineFailed && p.at(0, 0) == 0.f && p.at(1, 999) == 0.f, "engine failure mutes tick"); }
        engine.failAfter = -1;
        { Planes p(2, 100); bridge.process(p.pointers.data(), 2, 100, 48000);
          check(bridge.status() == BridgeStatus::EngineFailed && p.at(0, 1) == 0.f, "failure stays latched"); }
        check(bridge.prepare(48000, 2, 256) && bridge.status() == BridgeStatus::Processing, "prepare clears failure");

        { Planes p(2, 4); bridge.process(p.pointers.data(), 2, 0, 48000); bridge.process(nullptr, 2, 4, 48000);
          check(p.at(0, 1) == input(0, 1), "zero frames / null planes are no-ops"); }
        check(!bridge.prepare(48000, 9, 256) && bridge.status() == BridgeStatus::Unprepared, "reject >8 OBS channels");
        check(!bridge.prepare(0, 2, 256), "reject zero rate");
        bridge.prepare(48000, 2, 256); bridge.suspend();
        { Planes p(2, 64); bridge.process(p.pointers.data(), 2, 64, 48000);
          check(bridge.status() == BridgeStatus::Unprepared && p.at(1, 3) == 0.f, "suspended bridge never calls the engine"); }
        check(AudioBridge<FakeEngine>::latencyNanoseconds(48, 48000) == 1000000, "latency ns");

        // Dry preserves the ORIGINAL input, including before input mapping, even
        // when the engine writes output and then reports failure. Surround planes
        // must remain untouched on both success and failure.
        PartialFailureEngine partial;
        AudioBridge<PartialFailureEngine> dryBridge(partial);
        partial.bridge = &dryBridge;
        for (int channels : {1, 2, 6}) {
            for (auto m : {InputMapping::Stereo, InputMapping::LeftToBoth,
                           InputMapping::RightToBoth, InputMapping::SumToBoth}) {
                dryBridge.setFailurePolicy(FailurePolicy::Dry);
                dryBridge.setInputMapping(m);
                check(dryBridge.prepare(48000, channels, 64), "Dry prepare");
                partial.fail = false;
                Planes good(channels, 64);
                check(dryBridge.process(good.pointers.data(), channels, 64, 48000), "Dry success result");
                for (int c = 0; c < channels; ++c) for (int n = 0; n < 64; ++n) {
                    float want = input(c, n);
                    if (c < 2) {
                        if (channels > 1 && m != InputMapping::Stereo)
                            want = m == InputMapping::LeftToBoth ? input(0, n)
                                 : m == InputMapping::RightToBoth ? input(1, n)
                                 : 0.5f * (input(0, n) + input(1, n));
                        want *= 2.f;
                    }
                    check(good.at(c, n) == want, "Dry successful mapping and surround");
                }
                partial.fail = true;
                partial.switchPolicy = true;
                Planes bad(channels, 64);
                check(!dryBridge.process(bad.pointers.data(), channels, 64, 48000), "Dry failure result");
                check(dryBridge.status() == BridgeStatus::EngineFailed, "Dry failure latches");
                for (int c = 0; c < channels; ++c) for (int n = 0; n < 64; ++n)
                    check(bad.at(c, n) == input(c, n), "Dry failure must retain unmapped original");
                dryBridge.setFailurePolicy(FailurePolicy::Dry);
                partial.switchPolicy = false;
                const int calls = partial.calls;
                Planes latched(channels, 64);
                check(!dryBridge.process(latched.pointers.data(), channels, 64, 48000)
                      && partial.calls == calls, "Dry latched failure never calls engine");
                for (int c = 0; c < channels; ++c) for (int n = 0; n < 64; ++n)
                    check(latched.at(c, n) == input(c, n), "Dry latched original");
            }
        }
        // A Dry tick cannot be committed in pieces. A tick larger than the prepared
        // engine block (OBS hands media sources' audio to filters in ticks of about
        // 4096 frames on Windows and macOS) is processed whole, in engine-sized
        // chunks. Ticks over a second bypass without invoking the engine, report
        // unprocessed, and allow the next normal tick.
        partial.fail = false;
        dryBridge.setInputMapping(InputMapping::LeftToBoth);
        dryBridge.prepare(48000, 6, 64);
        { Planes p(6, 4096);
          check(dryBridge.process(p.pointers.data(), 6, 4096, 48000) && partial.calls == 4096 / 64,
                "Dry tick larger than the engine block is processed in engine-sized chunks");
          for (int c = 0; c < 6; ++c) for (int n = 0; n < 4096; ++n)
              check(p.at(c, n) == (c < 2 ? 2.f * input(0, n) : input(c, n)), "large Dry tick: mapped, processed, surround untouched"); }
        partial.calls = 0;
        { Planes p(6, 48001);
          check(!dryBridge.process(p.pointers.data(), 6, 48001, 48000) && partial.calls == 0,
                "Dry tick over a second bypasses without engine call");
          for (int c = 0; c < 6; ++c) for (int n = 0; n < 48001; ++n)
              check(p.at(c, n) == input(c, n), "oversized Dry must preserve all channels");
          check(dryBridge.status() == BridgeStatus::Processing && !dryBridge.needsPrepare(),
                "oversized Dry does not latch or request reprepare"); }
        { Planes p(6, 64);
          check(dryBridge.process(p.pointers.data(), 6, 64, 48000), "Dry resumes next bounded tick"); }
        dryBridge.suspend();
        { Planes p(6, 64);
          check(!dryBridge.process(p.pointers.data(), 6, 64, 48000), "suspended Dry result");
          for (int c = 0; c < 6; ++c) for (int n = 0; n < 64; ++n)
              check(p.at(c, n) == input(c, n), "suspended Dry original"); }
        dryBridge.setFailurePolicy(FailurePolicy::Mute);
        dryBridge.prepare(48000, 6, 64);
        partial.fail = true;
        { Planes p(6, 65);
          check(!dryBridge.process(p.pointers.data(), 6, 65, 48000), "Mute failure result");
          for (int c = 0; c < 6; ++c) for (int n = 0; n < 65; ++n)
              check(p.at(c, n) == 0.f, "Mute still silences entire tick and surround"); }

        // Settings updates re-prepare while the OBS audio thread keeps calling:
        // every sample must agree with the per-tick result (wet or policy
        // fallback), never with a stale/concurrently updated global status.
        for (auto tickPolicy : {FailurePolicy::Mute, FailurePolicy::Dry}) {
            bridge.setFailurePolicy(tickPolicy);
            bridge.prepare(48000, 2, 512);
            std::atomic<bool> stop{false}, started{false}; std::atomic<int> torn{0};
            std::thread audio([&] {
                do {
                    Planes p(2, 480, 17);
                    const bool wet = bridge.process(p.pointers.data(), 2, 480, 48000);
                    for (int c = 0; c < 2; ++c) for (int n = 0; n < 480; ++n) {
                        const float want = wet ? 2.f * input(c, 17 + n)
                                         : tickPolicy == FailurePolicy::Dry ? input(c, 17 + n) : 0.f;
                        if (p.at(c, n) != want) torn.fetch_add(1);
                    }
                    started.store(true);
                } while (!stop.load());
            });
            while (!started.load()) std::this_thread::yield();
            for (int i = 0; i < 2000; ++i) bridge.prepare(48000, 2, 256 + i % 512);
            stop.store(true); audio.join();
            check(torn.load() == 0 && bridge.status() == BridgeStatus::Processing, "concurrent prepare");
        }

        std::cout << "PASS OBS audio bridge: unprepared mute, 5.1 passthrough, chunking, input mapping, mono, format-change and "
                     "engine-failure policies, transactional Dry and mapping, large Dry ticks chunked, over-a-second Dry bypass, per-tick results, "
                     "latched failure, bounds, latency ns, concurrent re-prepare\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}

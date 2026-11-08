// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace spectralforge::obs {
// Host-neutral boundary between an OBS audio filter callback and the Chimera
// VST3 loading adapter (Issue #131, structure 2). It has
// no libobs or JUCE dependency so the realtime contract is testable without
// either: OBS delivers planar float audio for every channel of its speaker
// layout (up to 8), at the OBS output rate, with a frame count that is not
// guaranteed to match any hint. Chimera processes mono or stereo only.
//
// Engine contract:
//   bool prepare(double sampleRate, int channels, int maximumFrames);  // may allocate
//   bool process(float* const* channels, int channels, int frames) noexcept;
//   int latency() const noexcept;                                       // samples
enum class InputMapping { Stereo, LeftToBoth, RightToBoth, SumToBoth };
// What the listener hears when the engine cannot process a block. Mute is the
// default: silently switching to dry can expose an unprocessed microphone or
// guitar signal on stream. Dry leaves every OBS channel untouched.
enum class FailurePolicy { Mute, Dry };
enum class BridgeStatus { Unprepared, Processing, FormatChanged, EngineFailed };

template <class Engine>
class AudioBridge {
public:
    explicit AudioBridge(Engine& engine) noexcept : engine(engine) {}

    // One non-audio thread at a time (filter create/update, OBS audio reset). Waits for
    // an in-flight callback to leave before touching the engine, so process()
    // never observes a half-prepared engine and never waits itself.
    bool prepare(double sampleRate, int obsChannels, int maximumFrames) {
        suspend();
        rate = sampleRate;
        hostChannels = obsChannels;
        capacity = std::max(1, maximumFrames);
        engineChannels = obsChannels >= 2 ? 2 : 1;
        // A whole Dry tick is transactional: never commit mapped/processed samples
        // until the engine succeeds. Allocate off the audio thread, even if the
        // policy is currently Mute (it may change without re-preparing). OBS hands
        // a media source's audio to its filters in ticks larger than one output
        // tick (about 4096 frames on Windows and macOS), so the work buffer holds
        // up to a second; the engine still gets chunks of at most `capacity`. A rate
        // outside (0, 1 MHz), NaN included, is not cast (the cast would be undefined).
        const bool sensibleRate = sampleRate > 0.0 && sampleRate < 1000000.0;
        dryCapacity = std::max(capacity, sensibleRate ? static_cast<int>(std::ceil(sampleRate)) : capacity);
        for (auto& channel : dryWork) channel.resize(static_cast<size_t>(dryCapacity));
        const bool ok = sampleRate > 0 && obsChannels >= 1 && obsChannels <= maxPlanes
            && engine.prepare(sampleRate, engineChannels, capacity);
        latency.store(ok ? std::max(0, engine.latency()) : 0);
        state.store(ok ? BridgeStatus::Processing : BridgeStatus::Unprepared);
        ready.store(ok);
        return ok;
    }

    // Stops engine use before the owner unloads or swaps it. Ticks until the
    // next successful prepare() follow the failure policy.
    void suspend() noexcept {
        ready.store(false);
        while (active.load() != 0) std::this_thread::yield();
        state.store(BridgeStatus::Unprepared);
    }

    void setInputMapping(InputMapping value) noexcept { mapping.store(value); }
    void setFailurePolicy(FailurePolicy value) noexcept { policy.store(value); }

    // OBS audio thread. In-place, allocation-free and lock-free. Channels
    // beyond the first two (surround layouts) are passed through unchanged.
    // Returns true only if this tick was processed, independently of status()
    // which a concurrent prepare() can change before the caller reads it.
    bool process(float* const* planes, int obsChannels, int frames, double sampleRate) noexcept {
        if (frames <= 0 || planes == nullptr) return false;
        active.fetch_add(1);
        const auto tickPolicy = policy.load();
        bool processed = false;
        if (!ready.load()) {
            // Mute or pass through, but leave the status alone: suspend() and a failed
            // prepare() already set Unprepared, and a tick that saw ready == false just
            // before a prepare() finished must not overwrite that prepare's Processing.
            silence(planes, obsChannels, frames, tickPolicy);
        } else if (obsChannels != hostChannels || sampleRate != rate || !planesPresent(planes, obsChannels)) {
            // Re-preparing allocates; the owner must do it off this thread.
            fail(planes, obsChannels, frames, BridgeStatus::FormatChanged, tickPolicy);
        } else if (state.load() == BridgeStatus::EngineFailed) {
            fail(planes, obsChannels, frames, BridgeStatus::EngineFailed, tickPolicy);
        } else {
            // Dry ticks longer than the work buffer (a second) bypass DSP
            // unchanged, return false for statistics, and do not latch a fault.
            // Mute retains allocation-free chunking for arbitrarily large ticks.
            if (tickPolicy == FailurePolicy::Dry && frames > dryCapacity) {
                active.fetch_sub(1);
                return false;
            }
            float* work[2]{dryWork[0].data(), dryWork[1].data()};
            float* const* target = planes;
            if (tickPolicy == FailurePolicy::Dry) {
                for (int c = 0; c < engineChannels; ++c)
                    std::copy_n(planes[c], frames, work[c]);
                target = work;
            }
            map(target, frames);
            processed = true;
            for (int offset = 0; offset < frames; offset += capacity) {
                const int count = std::min(capacity, frames - offset);
                float* chunk[2]{target[0] + offset, engineChannels > 1 ? target[1] + offset : nullptr};
                if (!engine.process(chunk, engineChannels, count)) {
                    // Latched until the next prepare(): no wet/dry flapping on air.
                    fail(planes, obsChannels, frames, BridgeStatus::EngineFailed, tickPolicy);
                    processed = false;
                    break;
                }
            }
            if (processed && tickPolicy == FailurePolicy::Dry)
                for (int c = 0; c < engineChannels; ++c)
                    std::copy_n(work[c], frames, planes[c]);
            if (state.load() == BridgeStatus::FormatChanged) state.store(BridgeStatus::Processing);
        }
        active.fetch_sub(1);
        return processed;
    }

    BridgeStatus status() const noexcept { return state.load(); }
    bool needsPrepare() const noexcept { return state.load() == BridgeStatus::FormatChanged; }
    int latencySamples() const noexcept { return latency.load(); }

    // OBS has no plugin delay compensation. This is the processed audio's delay
    // relative to its timestamp, for display and the (unmeasured) sync offset.
    static std::int64_t latencyNanoseconds(int samples, double sampleRate) noexcept {
        return sampleRate > 0 ? static_cast<std::int64_t>(samples * 1.0e9 / sampleRate + 0.5) : 0;
    }

    static constexpr int maxPlanes = 8; // libobs MAX_AV_PLANES / MAX_AUDIO_CHANNELS

private:
    bool planesPresent(float* const* planes, int channels) const noexcept {
        for (int c = 0; c < channels; ++c) if (planes[c] == nullptr) return false;
        return true;
    }

    void map(float* const* planes, int frames) const noexcept {
        if (engineChannels < 2) return;
        const auto m = mapping.load();
        for (int n = 0; n < frames && m != InputMapping::Stereo; ++n) {
            const float l = planes[0][n], r = planes[1][n];
            const float v = m == InputMapping::LeftToBoth ? l : m == InputMapping::RightToBoth ? r : 0.5f * (l + r);
            planes[0][n] = planes[1][n] = v;
        }
    }

    void fail(float* const* planes, int obsChannels, int frames, BridgeStatus reason, FailurePolicy tickPolicy) noexcept {
        state.store(reason);
        silence(planes, obsChannels, frames, tickPolicy);
    }

    void silence(float* const* planes, int obsChannels, int frames, FailurePolicy tickPolicy) const noexcept {
        if (tickPolicy == FailurePolicy::Dry) return;
        for (int c = 0; c < std::clamp(obsChannels, 0, maxPlanes); ++c)
            if (planes[c] != nullptr) std::fill(planes[c], planes[c] + frames, 0.f);
    }

    Engine& engine;
    double rate{};
    int hostChannels{}, engineChannels{1}, capacity{1}, dryCapacity{1};
    std::vector<float> dryWork[2];
    std::atomic<bool> ready{false};
    std::atomic<int> active{0}, latency{0};
    std::atomic<BridgeStatus> state{BridgeStatus::Unprepared};
    std::atomic<InputMapping> mapping{InputMapping::Stereo};
    std::atomic<FailurePolicy> policy{FailurePolicy::Mute};
};
}

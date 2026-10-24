// SPDX-License-Identifier: GPL-2.0-or-later
// Integration test of the VST3 host adapter against a real Chimera bundle:
// identity checks, processing, block-size independence, mono, state round
// trip, reload cycles and concurrent instances. Usage: <path/to/Chimera.vst3>
#include "chimera-host.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using spectralforge::obs::ChimeraHost;
constexpr double rate = 48000;
constexpr int capacity = 1024;
constexpr double pi = 3.14159265358979323846;

void check(bool ok, const std::string& what) { if (!ok) throw std::runtime_error(what); }

// Deterministic DI-like input: plucked low-E/A/D notes with harmonics and decay.
std::vector<float> guitar(int frames, int channel) {
    std::vector<float> out(static_cast<size_t>(frames));
    const double notes[] = {82.41, 110.0, 146.83, 98.0};
    for (int n = 0; n < frames; ++n) {
        const int note = (n / 24000) % 4;
        const double t = (n % 24000) / rate, f = notes[note];
        double v = 0;
        for (int h = 1; h <= 6; ++h) v += std::sin(2 * pi * f * h * t + channel * 0.1) / h;
        out[static_cast<size_t>(n)] = static_cast<float>(0.25 * v * std::exp(-3.0 * t));
    }
    return out;
}

// Runs `frames` of input through the host using the given block pattern.
std::vector<std::vector<float>> run(ChimeraHost& host, int channels, int frames, const std::vector<int>& blocks) {
    std::vector<std::vector<float>> audio;
    for (int c = 0; c < channels; ++c) audio.push_back(guitar(frames, c));
    for (int offset = 0, i = 0; offset < frames; ++i) {
        const int count = std::min(blocks[static_cast<size_t>(i) % blocks.size()], frames - offset);
        float* pointers[2]{};
        for (int c = 0; c < channels; ++c) pointers[c] = audio[static_cast<size_t>(c)].data() + offset;
        check(host.process(pointers, channels, count), "process() failed");
        offset += count;
    }
    return audio;
}

// Chimera loads IRs and models on background threads after prepare().
void prepareAndSettle(ChimeraHost& host, int channels) {
    check(host.prepare(rate, channels, capacity), "prepare() failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

double rms(const std::vector<float>& x, size_t from) {
    double sum = 0;
    for (size_t n = from; n < x.size(); ++n) sum += double(x[n]) * x[n];
    return std::sqrt(sum / double(std::max<size_t>(1, x.size() - from)));
}

double maxDifference(const std::vector<float>& a, const std::vector<float>& b, size_t from) {
    double worst = 0;
    for (size_t n = from; n < a.size(); ++n) worst = std::max(worst, std::abs(double(a[n]) - b[n]));
    return worst;
}
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: chimera-obs-host-tests <Chimera.vst3>\n"; return 2; }
    const std::string bundle = argv[1];
    try {
        std::string error;
        {
            ChimeraHost missing;
            check(!missing.load(bundle + ".missing", error) && !error.empty() && !missing.loaded(), "missing bundle must be rejected");
            check(!missing.prepare(rate, 2, capacity), "unloaded host must not prepare");
        }

        ChimeraHost a;
        error.clear();
        check(a.load(bundle, error), "load: " + error);
        const auto id = a.identity();
        check(id.name == spectralforge::obs::chimeraClassName && id.vendor == spectralforge::obs::chimeraVendor, "identity");
        std::cout << "loaded " << id.name << " " << id.version << " (" << id.vendor << ", VST3 SDK " << id.sdkVersion << "), "
                  << a.parameterCount() << " parameters\n";
        check(a.parameterCount() > 0, "Chimera must expose parameters");

        const int frames = int(rate * 4);
        const size_t settled = size_t(rate); // compare after the first second
        prepareAndSettle(a, 2);
        std::cout << "latency " << a.latency() << " samples\n";
        const auto reference = run(a, 2, frames, {capacity});
        const auto dry = guitar(frames, 0);
        for (const auto& channel : reference)
            for (float v : channel) check(std::isfinite(v), "non-finite output");
        const double wet = rms(reference[0], settled), diff = maxDifference(reference[0], dry, settled);
        std::cout << "output rms " << wet << " (input " << rms(dry, settled) << "), max |wet-dry| " << diff << '\n';
        check(wet > 1e-4, "Chimera output is silent (beta lifecycle blocked or not processing)");
        check(diff > 1e-3, "output equals input: Chimera did not process");

        // OBS ticks vary in size and are split at the prepared capacity: the
        // result must not depend on the block pattern.
        ChimeraHost b;
        check(b.load(bundle, error), "second instance: " + error);
        prepareAndSettle(b, 2);
        const auto chunked = run(b, 2, frames, {1, 480, capacity, 333, 64, 1000});
        double blockDiff = 0;
        for (int c = 0; c < 2; ++c) blockDiff = std::max(blockDiff, maxDifference(reference[c], chunked[c], settled));
        std::cout << "block-size independence: max diff " << blockDiff << '\n';
        check(blockDiff < 1e-4, "output depends on host block size");

        // State: change a parameter, restore into a fresh instance.
        std::uint32_t paramId = 0;
        std::string title;
        bool found = false;
        int steps = 0;
        for (int i = 0; i < a.parameterCount() && !found; ++i)
            if (a.parameterInfo(i, paramId, title, steps) && steps == 0) {
                const double before = a.parameter(paramId);
                const double target = before < 0.5 ? 0.75 : 0.25;
                if (a.setParameter(paramId, target) && std::abs(a.parameter(paramId) - target) < 1e-6) found = true;
            }
        check(found, "no writable parameter");
        const double stored = a.parameter(paramId);
        std::vector<std::uint8_t> component, controller;
        check(a.getState(component, controller) && !component.empty(), "getState");
        ChimeraHost c;
        check(c.load(bundle, error) && c.setState(component, controller), "setState: " + error);
        check(std::abs(c.parameter(paramId) - stored) < 1e-6, "parameter '" + title + "' not restored");
        std::vector<std::uint8_t> again, againController;
        check(c.getState(again, againController) && again == component, "state round trip is not byte-identical");
        std::cout << "state: " << component.size() << " + " << controller.size() << " bytes, '" << title << "' restored\n";
        check(!c.setState({}, {}), "empty state must be rejected");

        // Reset: a host drops Chimera's tails without re-preparing it.
        const auto silence = [&](ChimeraHost& host, int frames) {
            std::vector<float> left(static_cast<size_t>(frames)), right(static_cast<size_t>(frames));
            for (int offset = 0; offset < frames; offset += capacity) {
                float* pointers[2]{left.data() + offset, right.data() + offset};
                check(host.process(pointers, 2, std::min(capacity, frames - offset)), "process() failed on silence");
            }
            return left;
        };
        const double tail = rms(silence(b, int(rate / 10)), 0);
        run(b, 2, int(rate), {capacity});
        b.requestReset();
        const double afterReset = rms(silence(b, int(rate / 10)), 0);
        std::cout << "100 ms after playing: tail rms " << tail << " without reset, " << afterReset << " after reset\n";
        check(tail > 1e-4, "Chimera left no tail to measure");
        check(afterReset < tail * 1e-2, "reset did not drop Chimera's tails");

        // Mono OBS layout.
        ChimeraHost mono;
        check(mono.load(bundle, error), "mono instance: " + error);
        prepareAndSettle(mono, 1);
        const auto monoOut = run(mono, 1, int(rate * 2), {capacity});
        check(rms(monoOut[0], settled) > 1e-4, "mono output is silent");
        check(!mono.process(nullptr, 2, 16), "channel count mismatch must fail");

        // Filter add/remove and scene switching reload repeatedly.
        for (int i = 0; i < 5; ++i) {
            ChimeraHost cycle;
            check(cycle.load(bundle, error) && cycle.prepare(rate, 2, capacity), "reload cycle: " + error);
            run(cycle, 2, capacity * 4, {capacity});
            cycle.unload();
            check(!cycle.loaded() && !cycle.prepare(rate, 2, capacity), "unload");
        }

        std::cout << "PASS Chimera VST3 host: identity, processing, block-size independence, state round trip, tail reset, mono, reload cycles, "
                     "concurrent instances\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Processes audio through the Chimera VST3 outside OBS, for obs_vst3_match_test.py:
//   chimera-obs-render-tool <SpectralForge Chimera.vst3> <sample rate> <channels> <block frames>
//                           <input.f32> <output.f32> [<state.json>]
// Input and output are raw interleaved 32-bit float. state.json is what
// chimera-obs-state-tool prints ({"component": ..., "controller": ...}); without
// it Chimera keeps its default state, as a newly added OBS filter does.
// Prints Chimera's reported latency in samples.
#include "base64.hpp"
#include "chimera-host.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace spectralforge::obs;

namespace {
// The base64 text after "key": in the state tool's one-line JSON.
std::string field(const std::string& json, const std::string& key) {
    const auto at = json.find("\"" + key + "\"");
    if (at == std::string::npos) return {};
    const auto open = json.find('"', json.find(':', at) + 1);
    const auto close = json.find('"', open + 1);
    return open == std::string::npos || close == std::string::npos ? std::string{} : json.substr(open + 1, close - open - 1);
}
}

int main(int argc, char** argv) {
    if (argc != 7 && argc != 8) {
        std::cerr << "usage: chimera-obs-render-tool <bundle> <rate> <channels> <block> <input.f32> <output.f32> [<state.json>]\n";
        return 2;
    }
    const double rate = std::atof(argv[2]);
    const int channels = std::atoi(argv[3]);
    const int block = std::atoi(argv[4]);
    if (rate <= 0 || channels < 1 || channels > 2 || block < 1) {
        std::cerr << "FAIL rate must be positive, channels 1 or 2, block at least 1\n";
        return 2;
    }
    std::ifstream in(argv[5], std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto frames = bytes.size() / (sizeof(float) * static_cast<size_t>(channels));
    std::vector<std::vector<float>> audio(static_cast<size_t>(channels), std::vector<float>(frames));
    const auto* samples = reinterpret_cast<const float*>(bytes.data());
    for (size_t n = 0; n < frames; ++n)
        for (int c = 0; c < channels; ++c) audio[static_cast<size_t>(c)][n] = samples[n * static_cast<size_t>(channels) + static_cast<size_t>(c)];

    ChimeraHost host;
    std::string error;
    if (!host.load(argv[1], error)) {
        std::cerr << "FAIL load Chimera: " << error << '\n';
        return 1;
    }
    if (argc == 8) {
        std::ifstream stateFile(argv[7]);
        const std::string json((std::istreambuf_iterator<char>(stateFile)), std::istreambuf_iterator<char>());
        const auto component = base64Decode(field(json, "component"));
        const auto controller = base64Decode(field(json, "controller"));
        if (!component || !controller || !host.setState(*component, *controller)) {
            std::cerr << "FAIL could not apply the state in " << argv[7] << '\n';
            return 1;
        }
    }
    // As the OBS filter does: whole OBS ticks of `block` frames at most.
    if (!host.prepare(rate, channels, block)) {
        std::cerr << "FAIL prepare\n";
        return 1;
    }
    // Chimera loads IRs and models on background threads after prepare().
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    for (size_t offset = 0; offset < frames; offset += static_cast<size_t>(block)) {
        const int count = static_cast<int>(std::min(frames - offset, static_cast<size_t>(block)));
        float* pointers[2]{audio[0].data() + offset, channels > 1 ? audio[1].data() + offset : nullptr};
        if (!host.process(pointers, channels, count)) {
            std::cerr << "FAIL process at frame " << offset << '\n';
            return 1;
        }
    }
    std::vector<float> interleaved(frames * static_cast<size_t>(channels));
    for (size_t n = 0; n < frames; ++n)
        for (int c = 0; c < channels; ++c) interleaved[n * static_cast<size_t>(channels) + static_cast<size_t>(c)] = audio[static_cast<size_t>(c)][n];
    std::ofstream out(argv[6], std::ios::binary);
    out.write(reinterpret_cast<const char*>(interleaved.data()), static_cast<std::streamsize>(interleaved.size() * sizeof(float)));
    if (!out) {
        std::cerr << "FAIL write " << argv[6] << '\n';
        return 1;
    }
    std::cout << "latency " << host.latency() << '\n';
    return 0;
}

#include "HostBypass.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
float input(int channel, int sample) {
    return sample < 0 ? 0.f : static_cast<float>((sample * 37 + channel * 19) % 101 - 50) / 128.f;
}
void check(bool ok) { if (!ok) throw std::runtime_error("Host bypass sample alignment mismatch"); }
}
int main() {
    try {
        for (int channels : {1, 2}) for (int delay : {0, 1, 57, 2048, 4096}) {
            spectralforge::HostBypass bypass;
            bypass.prepare(4096);
            int elapsed = 0;
            // Includes unprimed startup, wet -> bypass, delay changes, ring wraps
            // and callbacks larger than both the preparation hint and ring.
            for (int block = 0; block < 36; ++block) {
                const int count = std::array<int, 4>{1, 128, 257, 5003}[block % 4];
                const bool isBypassed = block % 3 != 1;
                const int currentDelay = block % 5 == 0 ? 0 : delay;
                std::array<std::vector<float>, 2> data;
                std::array<float*, 2> pointers{};
                for (int c = 0; c < channels; ++c) {
                    data[c].resize(static_cast<size_t>(count)); pointers[c] = data[c].data();
                    for (int n = 0; n < count; ++n) data[c][n] = input(c, elapsed + n);
                }
                bypass.process(pointers.data(), channels, count, currentDelay, isBypassed);
                for (int c = 0; c < channels; ++c) for (int n = 0; n < count; ++n)
                    check(data[c][n] == input(c, elapsed + n - (isBypassed ? currentDelay : 0)));
                elapsed += count;
            }
            bypass.prepare(4096);
            float impulse = 1.f; float* channel = &impulse;
            bypass.process(&channel, 1, 1, 1, true);
            check(impulse == 0.f); // reprepare must not leak old input history
            std::vector<float> oldAudio(5003, 1.f);
            float* oldChannel = oldAudio.data();
            bypass.process(&oldChannel, 1, static_cast<int>(oldAudio.size()), 1, false);
            bypass.reset();
            impulse = 0.f;
            bypass.process(&channel, 1, 1, 1, true);
            check(impulse == 0.f); // reset must clear the prior impulse without prepare
            impulse = .25f;
            bypass.process(&channel, 1, 1, 1, true);
            check(impulse == 0.f);
            impulse = 0.f;
            bypass.process(&channel, 1, 1, 1, true);
            check(impulse == .25f); // preserve alignment for newly arriving audio
        }
        std::cout << "PASS host bypass: exact delayed input, wet priming, mono/stereo, zero/dynamic delay, oversized blocks, reprepare and reset\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

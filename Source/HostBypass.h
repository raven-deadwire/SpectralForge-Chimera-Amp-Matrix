#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

namespace spectralforge {
// Host bypass must retain the delay reported to PDC. Keep raw input history
// while the wet graph runs so the first bypass block does not start with zeros.
// Allocation happens only in prepare(). A requested reset clears the bounded
// history in place on the audio thread, including while the host bypasses us.
class HostBypass {
public:
    void prepare(int maximumLatency) {
        capacity = std::max(1, maximumLatency + 1);
        for (auto& channel : history) channel.assign(static_cast<std::size_t>(capacity), 0.f);
        position = 0;
    }

    void reset() noexcept {
        for (auto& channel : history) std::fill(channel.begin(), channel.end(), 0.f);
        position = 0;
    }

    void process(float* const* data, int channels, int samples, int latency, bool bypass) noexcept {
        assert(channels >= 0 && channels <= 2 && latency >= 0 && latency < capacity);
        if (capacity == 0) return; // A host must prepare before processing.
        const int delay = std::clamp(latency, 0, capacity - 1);
        for (int n = 0; n < samples; ++n) {
            int read = position - delay;
            if (read < 0) read += capacity;
            for (int c = 0; c < std::min(channels, 2); ++c) {
                auto& channel = history[static_cast<std::size_t>(c)];
                channel[static_cast<std::size_t>(position)] = data[c][n];
                if (bypass) data[c][n] = channel[static_cast<std::size_t>(read)];
            }
            if (++position == capacity) position = 0;
        }
    }

private:
    std::array<std::vector<float>, 2> history;
    int capacity{}, position{};
};
}

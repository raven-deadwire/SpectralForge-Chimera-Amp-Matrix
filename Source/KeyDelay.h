#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>

namespace spectralforge {

// The sidechain key delayed by the latency of the modules ahead of a keyed
// detector (docs/SIGNAL_ORDER_1_4.md), so moving a module with latency (HQ
// saturation, an oversampled POST preamp) in front of a keyed compressor
// does not make the key arrive early. Fed every chunk so the delay stays
// continuous; storage is sized in prepare().
class KeyDelay {
public:
    void prepare(int maximumDelay, int maximumBlock) {
        ring.setSize(2, std::max(1, maximumDelay) + std::max(1, maximumBlock) + 1);ring.clear();
        out.setSize(2, std::max(1, maximumBlock));out.clear();write = 0;fed = false;
    }
    // Feeds this chunk's key; returns it delayed by `latency` samples (the
    // key itself at 0, nullptr without a key).
    const juce::AudioBuffer<float>* process(const juce::AudioBuffer<float>* key, int latency) noexcept {
        if (key == nullptr || key->getNumChannels() == 0) { fed = false;return nullptr; }
        if (!fed) { ring.clear();fed = true; }   // a key that returns starts from silence, not from old audio
        const int n = std::min(key->getNumSamples(), out.getNumSamples()), channels = std::min(2, key->getNumChannels());
        const int size = ring.getNumSamples();
        const int delay = std::clamp(latency, 0, size - n - 1);
        for (int c = 0; c < channels; ++c) {
            const float* in = key->getReadPointer(c);float* r = ring.getWritePointer(c);float* o = out.getWritePointer(c);
            for (int i = 0; i < n; ++i) {
                const int at = (write + i) % size;
                r[at] = in[i];
                o[i] = r[(at - delay + size) % size];
            }
        }
        write = (write + n) % size;
        if (delay == 0) return key;
        view.setDataToReferTo(out.getArrayOfWritePointers(), channels, n);
        return &view;
    }
private:
    juce::AudioBuffer<float> ring, out, view;
    int write{};bool fed{};
};

} // namespace spectralforge

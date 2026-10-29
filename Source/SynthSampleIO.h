#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SynthLayer.h"

// SYNTH sample oscillators (docs/SYNTH_V2.md): building a sample from audio,
// reading audio files and keeping samples in the plugin state. Message thread.
namespace spectralforge::synthSample {

// Builds a sample from up to two channels (more are folded into two). The
// pitch is found with the synth's own tracker over the whole sample: the
// median of its confident estimates, or 0 when there are none.
inline std::unique_ptr<SynthSample> make(const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name) {
    const int length = std::min(audio.getNumSamples(), int(SynthSample::maximumSeconds * sampleRate));
    if (length < 16 || audio.getNumChannels() < 1 || sampleRate < 8000) return nullptr;
    auto sample = std::make_unique<SynthSample>();
    sample->name = name;sample->sampleRate = sampleRate;sample->length = length;
    const int channels = std::min(2, audio.getNumChannels());
    for (int c = 0; c < channels; ++c) sample->data[size_t(c)].assign(size_t(length), 0.f);
    for (int c = 0; c < audio.getNumChannels(); ++c) {
        const float* in = audio.getReadPointer(c);auto& out = sample->data[size_t(c % channels)];
        const float share = channels == 2 ? 1.f / float((audio.getNumChannels() + 1 - c % 2) / 2) : 1.f / float(audio.getNumChannels());
        for (int n = 0; n < length; ++n) out[size_t(n)] += std::isfinite(in[n]) ? in[n] * share : 0.f;
    }
    for (int k = 0; k < SynthSample::overviewColumns; ++k) {
        const int from = int(int64_t(k) * length / SynthSample::overviewColumns), to = std::max(from + 1, int(int64_t(k + 1) * length / SynthSample::overviewColumns));
        float lo = 0, hi = 0;
        for (int n = from; n < to; ++n) { const float x = channels == 2 ? .5f * (sample->data[0][size_t(n)] + sample->data[1][size_t(n)]) : sample->data[0][size_t(n)];lo = std::min(lo, x);hi = std::max(hi, x); }
        sample->overview[size_t(k)] = {lo, hi};
    }
    PitchTracker tracker;tracker.prepare(sampleRate);std::vector<double> found;
    constexpr int hop = 256;
    for (int n = 0; n + hop <= length; n += hop) {
        tracker.push(sample->data[0].data() + n, channels == 2 ? sample->data[1].data() + n : nullptr, hop);
        if (tracker.hz() > 0 && tracker.clarity() > .9 && tracker.rms() > 1e-3) found.push_back(tracker.hz());
    }
    if (found.size() >= 4) { std::sort(found.begin(), found.end());sample->detectedHz = found[found.size() / 2]; }
    return sample;
}
// The nearest MIDI note to the detected pitch, or 60 (C4) when none was found.
inline int rootNote(const SynthSample& sample) {
    if (sample.detectedHz <= 0) return 60;
    return juce::jlimit(0, 127, juce::roundToInt(69.0 + 12.0 * std::log2(sample.detectedHz / 440.0)));
}

inline juce::AudioFormatManager& formats() {
    static juce::AudioFormatManager manager;
    if (manager.getNumKnownFormats() == 0) manager.registerBasicFormats();
    return manager;
}
inline juce::Result load(const juce::File& file, std::unique_ptr<SynthSample>& result) {
    std::unique_ptr<juce::AudioFormatReader> reader(formats().createReaderFor(file));
    if (reader == nullptr) return juce::Result::fail("Not an audio file the synth can read");
    if (reader->sampleRate < 8000 || reader->lengthInSamples < 16) return juce::Result::fail("The audio file is empty or its rate is too low");
    const int length = int(std::min<juce::int64>(reader->lengthInSamples, juce::int64(SynthSample::maximumSeconds * reader->sampleRate)));
    juce::AudioBuffer<float> audio(int(std::max(1u, reader->numChannels)), length);
    if (!reader->read(&audio, 0, length, 0, true, true)) return juce::Result::fail("The audio file could not be read");
    result = make(audio, reader->sampleRate, file.getFileNameWithoutExtension());
    return result != nullptr ? juce::Result::ok() : juce::Result::fail("The audio file could not be used");
}

// State: one SAMPLE child per loaded oscillator, the audio as 24-bit FLAC.
inline juce::ValueTree save(const SynthLayer& synth) {
    juce::ValueTree tree("SYNTH_SAMPLES");
    for (int o = 0; o < synthOscCount; ++o) {
        const auto* sample = synth.sample(o);if (sample == nullptr) continue;
        juce::MemoryBlock encoded;
        {
            auto stream = std::make_unique<juce::MemoryOutputStream>(encoded, false);
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::AudioFormatWriter> writer(flac.createWriterFor(stream.get(), sample->sampleRate, sample->stereo() ? 2u : 1u, 24, {}, 0));
            if (writer == nullptr) continue;
            stream.release();
            const float* channels[2]{sample->data[0].data(), sample->stereo() ? sample->data[1].data() : nullptr};
            if (!writer->writeFromFloatArrays(channels, sample->stereo() ? 2 : 1, sample->length)) continue;
        }
        juce::ValueTree item("SAMPLE");
        item.setProperty("osc", o, nullptr);item.setProperty("name", sample->name, nullptr);item.setProperty("flac", encoded, nullptr);
        tree.appendChild(item, nullptr);
    }
    return tree;
}
// Decodes every SAMPLE child; oscillators without one get none.
inline std::array<std::unique_ptr<SynthSample>, synthOscCount> restore(const juce::ValueTree& tree) {
    std::array<std::unique_ptr<SynthSample>, synthOscCount> result;
    for (const auto item : tree) {
        const int o = int(item.getProperty("osc", -1));
        const auto* block = item.getProperty("flac").getBinaryData();
        if (o < 0 || o >= synthOscCount || block == nullptr) continue;
        juce::FlacAudioFormat flac;
        std::unique_ptr<juce::AudioFormatReader> reader(flac.createReaderFor(new juce::MemoryInputStream(*block, false), true));
        if (reader == nullptr || reader->lengthInSamples > juce::int64(SynthSample::maximumSeconds * reader->sampleRate) + 1) continue;
        juce::AudioBuffer<float> audio(int(std::max(1u, reader->numChannels)), int(reader->lengthInSamples));
        if (!reader->read(&audio, 0, audio.getNumSamples(), 0, true, true)) continue;
        result[size_t(o)] = make(audio, reader->sampleRate, item.getProperty("name").toString());
    }
    return result;
}
} // namespace spectralforge::synthSample

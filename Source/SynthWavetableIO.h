#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SynthSampleIO.h"

// SYNTH wavetable import (docs/SYNTH_WAVETABLE_IMPORT.md): cutting an audio
// file into single-cycle frames, and keeping imported tables in the plugin
// state. Message thread.
namespace spectralforge::synthTableIO {
// How a file is cut into frames. Auto: the frame size a Serum-style `clm `
// chunk names; else 2048 when the length is a multiple of it; else the whole
// file as one cycle when it is short; else 2048 with the remainder left out.
enum FrameSize { frameAuto, frame256, frame512, frame1024, frame2048, frameOneCycle, frameSizeCount };
inline juce::StringArray frameSizeNames() { return {"Auto", "256 samples", "512 samples", "1024 samples", "2048 samples", "One cycle"}; }
inline constexpr int minimumCycle = 16, maximumCycle = 16384, maximumSourceFrames = 256;

// The frame size in a WAV file's `clm ` chunk ("<!>2048 ..."), or 0.
inline int clmFrameSize(const juce::File& file) {
    juce::FileInputStream in(file);
    if (!in.openedOk() || in.getTotalLength() < 12) return 0;
    char id[4];
    if (in.read(id, 4) != 4 || std::memcmp(id, "RIFF", 4) != 0) return 0;
    in.readInt();
    if (in.read(id, 4) != 4 || std::memcmp(id, "WAVE", 4) != 0) return 0;
    for (int chunks = 0; chunks < 64 && in.getPosition() + 8 <= in.getTotalLength(); ++chunks) {
        if (in.read(id, 4) != 4) return 0;
        const auto size = juce::int64(juce::uint32(in.readInt()));
        const auto next = in.getPosition() + size + (size & 1);
        if (std::memcmp(id, "clm ", 4) == 0) {
            char text[32]{};
            const int got = in.read(text, int(std::min<juce::int64>(size, 31)));
            const juce::String s(juce::CharPointer_UTF8(text), size_t(std::max(0, got)));
            if (!s.startsWith("<!>")) return 0;
            const int value = s.substring(3).upToFirstOccurrenceOf(" ", false, false).getIntValue();
            return value >= minimumCycle && value <= maximumCycle ? value : 0;
        }
        if (!in.setPosition(next)) return 0;
    }
    return 0;
}

// One cycle of `length` samples as synthTableSize samples holding the same
// harmonics (up to 1023, or as many as the cycle has).
inline void resampleCycle(const float* in, int length, float* out) {
    if (length == synthTableSize) { std::copy_n(in, synthTableSize, out);return; }
    constexpr double twoPi = 6.283185307179586;
    const int harmonics = std::min(synthTableSize / 2 - 1, (length - 1) / 2);
    std::vector<float> spectrum(size_t(2 * synthTableSize), 0.f);
    const float scale = float(synthTableSize) / float(length);
    const auto put = [&](int h, double re, double im) {
        spectrum[size_t(2 * h)] = float(re) * scale;spectrum[size_t(2 * h + 1)] = float(im) * scale;
        if (h > 0) { const int mirror = synthTableSize - h;spectrum[size_t(2 * mirror)] = float(re) * scale;spectrum[size_t(2 * mirror + 1)] = -float(im) * scale; }
    };
    if (juce::isPowerOfTwo(length)) {
        int order = 0;while ((1 << order) < length) ++order;
        juce::dsp::FFT fft(order);std::vector<float> work(size_t(2 * length), 0.f);
        std::copy_n(in, length, work.data());fft.performRealOnlyForwardTransform(work.data());
        for (int h = 0; h <= harmonics; ++h) put(h, work[size_t(2 * h)], work[size_t(2 * h + 1)]);
    } else {
        // Any other length: the harmonics one by one.
        std::vector<double> cosine, sine;cosine.resize(size_t(length));sine.resize(size_t(length));
        for (int n = 0; n < length; ++n) { cosine[size_t(n)] = std::cos(twoPi * n / length);sine[size_t(n)] = std::sin(twoPi * n / length); }
        for (int h = 0; h <= harmonics; ++h) {
            double re = 0, im = 0;
            for (int n = 0; n < length; ++n) { const auto k = size_t((juce::int64(h) * n) % length);re += in[n] * cosine[k];im -= in[n] * sine[k]; }
            put(h, re, im);
        }
    }
    juce::dsp::FFT inverse(synthTableOrder);
    inverse.performRealOnlyInverseTransform(spectrum.data());
    std::copy_n(spectrum.data(), synthTableSize, out);
}

// Cuts mono audio into frames of `frameSize` samples and builds the table.
// More than synthUserTableMaxFrames frames: that many, evenly spaced, the
// first and the last kept. `note` says what was left out, if anything.
inline juce::Result make(const std::vector<float>& audio, int frameSize, const juce::String& name, std::unique_ptr<SynthUserTable>& result, juce::String* note = nullptr) {
    if (frameSize < minimumCycle || frameSize > maximumCycle) return juce::Result::fail("The frame size must be " + juce::String(minimumCycle) + " to " + juce::String(maximumCycle) + " samples");
    const int found = int(std::min<size_t>(audio.size() / size_t(frameSize), size_t(maximumSourceFrames)));
    if (found < 1) return juce::Result::fail("The file is shorter than one frame of " + juce::String(frameSize) + " samples");
    const int frames = std::min(found, synthUserTableMaxFrames);
    std::vector<float> cycles(size_t(frames) * synthTableSize, 0.f);
    for (int k = 0; k < frames; ++k) {
        const int source = frames == found ? k : int(std::llround(double(k) * (found - 1) / (frames - 1)));
        const float* in = audio.data() + size_t(source) * size_t(frameSize);
        for (int n = 0; n < frameSize; ++n) if (!std::isfinite(in[n])) return juce::Result::fail("The file holds values that are not numbers");
        resampleCycle(in, frameSize, cycles.data() + size_t(k) * synthTableSize);
    }
    auto table = SynthUserTable::build(std::move(cycles), frames, name);
    if (table == nullptr) return juce::Result::fail("The file is silent");
    table->sourceFrames = found;table->sourceFrameSize = frameSize;
    if (note != nullptr) {
        juce::StringArray notes;
        const auto used = juce::int64(found) * frameSize, left = juce::int64(audio.size()) - used;
        if (found > frames) notes.add(juce::String(frames) + " of " + juce::String(found) + " frames kept");
        if (left > 0) notes.add(juce::String(left) + " samples after the last whole frame left out");
        *note = notes.joinIntoString("; ");
    }
    result = std::move(table);
    return juce::Result::ok();
}

inline juce::Result load(const juce::File& file, int frameSize, std::unique_ptr<SynthUserTable>& result, juce::String* note = nullptr) {
    std::unique_ptr<juce::AudioFormatReader> reader(synthSample::formats().createReaderFor(file));
    if (reader == nullptr) return juce::Result::fail("Not an audio file the synth can read");
    if (reader->lengthInSamples < minimumCycle) return juce::Result::fail("The audio file is empty or too short");
    const auto total = reader->lengthInSamples;
    int size = 0;
    switch (juce::jlimit(0, frameSizeCount - 1, frameSize)) {
        case frame256: size = 256;break;
        case frame512: size = 512;break;
        case frame1024: size = 1024;break;
        case frame2048: size = 2048;break;
        case frameOneCycle:
            if (total > maximumCycle) return juce::Result::fail("Too long for one cycle: " + juce::String(total) + " samples (at most " + juce::String(maximumCycle) + ")");
            size = int(total);break;
        default:
            size = clmFrameSize(file);
            if (size == 0) size = total % synthTableSize == 0 ? synthTableSize : total <= 4 * synthTableSize ? int(total) : synthTableSize;
            break;
    }
    // Beyond the frames a table can come from, the file is not read.
    const int length = int(std::min<juce::int64>(total, juce::int64(maximumSourceFrames) * size));
    juce::AudioBuffer<float> buffer(int(std::max(1u, reader->numChannels)), length);
    if (!reader->read(&buffer, 0, length, 0, true, true)) return juce::Result::fail("The audio file could not be read");
    std::vector<float> mono(size_t(length), 0.f);
    const int channels = std::min(2, buffer.getNumChannels());
    for (int c = 0; c < channels; ++c) { const float* in = buffer.getReadPointer(c);for (int n = 0; n < length; ++n) mono[size_t(n)] += in[n] / float(channels); }
    juce::String cut;
    const auto made = make(mono, size, file.getFileNameWithoutExtension().substring(0, 48), result, &cut);
    if (made.failed()) return made;
    if (note != nullptr) {
        juce::StringArray notes;if (cut.isNotEmpty()) notes.add(cut);
        if (total > length) notes.add("only the first " + juce::String(maximumSourceFrames) + " frames read");
        *note = notes.joinIntoString("; ");
    }
    return made;
}

// State: one TABLE child per oscillator with an imported table - its cycles
// as 32-bit floats, least significant byte first. The table is rebuilt from
// them, so it comes back exactly.
inline juce::ValueTree save(const SynthLayer& synth) {
    juce::ValueTree tree("SYNTH_TABLES");
    for (int o = 0; o < synthOscCount; ++o) {
        const auto table = synth.sharedTable(o);if (table == nullptr) continue;
        const juce::MemoryBlock block(table->cycles.data(), table->cycles.size() * sizeof(float));   // every supported platform is little-endian
        juce::ValueTree item("TABLE");
        item.setProperty("osc", o, nullptr);item.setProperty("name", table->name, nullptr);item.setProperty("frames", table->frames, nullptr);
        item.setProperty("sourceFrames", table->sourceFrames, nullptr);item.setProperty("sourceFrameSize", table->sourceFrameSize, nullptr);
        item.setProperty("cycles", block, nullptr);
        tree.appendChild(item, nullptr);
    }
    return tree;
}
// Rebuilds every TABLE child that is whole; oscillators without one get none.
inline std::array<std::unique_ptr<SynthUserTable>, synthOscCount> restore(const juce::ValueTree& tree) {
    std::array<std::unique_ptr<SynthUserTable>, synthOscCount> result;
    for (const auto item : tree) {
        const int o = int(item.getProperty("osc", -1)), frames = int(item.getProperty("frames", 0));
        const auto* block = item.getProperty("cycles").getBinaryData();
        if (o < 0 || o >= synthOscCount || block == nullptr || frames < 1 || frames > synthUserTableMaxFrames) continue;
        if (block->getSize() != size_t(frames) * synthTableSize * 4) continue;
        std::vector<float> cycles(size_t(frames) * synthTableSize);
        std::memcpy(cycles.data(), block->getData(), block->getSize());
        auto table = SynthUserTable::build(std::move(cycles), frames, item.getProperty("name").toString().substring(0, 48));
        if (table == nullptr) continue;
        table->sourceFrames = juce::jlimit(frames, maximumSourceFrames, int(item.getProperty("sourceFrames", frames)));
        table->sourceFrameSize = juce::jlimit(minimumCycle, maximumCycle, int(item.getProperty("sourceFrameSize", synthTableSize)));
        result[size_t(o)] = std::move(table);
    }
    return result;
}
} // namespace spectralforge::synthTableIO

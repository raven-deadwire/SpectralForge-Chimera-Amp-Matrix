#include "OriginalAmpProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <fstream>
#include <iostream>

// Offline audit of the actual development wrapper. Raw float output preserves
// headroom; it must not be normalised or clipped before gain measurements.
int main(int argc, char** argv) {
    using namespace spectralforge::original;
    if (argc != 4) {
        std::cerr << "Usage: ChimeraOriginalAmpRender input.wav output.f32 state.json\n";
        return 1;
    }
    juce::AudioFormatManager formats;
    formats.registerFormat(new juce::WavAudioFormat(), true);
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(argv[1])));
    if (!reader || reader->numChannels != 1 || reader->lengthInSamples <= 0
        || reader->lengthInSamples > reader->sampleRate * 600) return 2;
    const auto config = juce::JSON::parse(juce::File(argv[3]));
    if (!config.isObject()) return 3;
    State state;
    if (config.hasProperty("preset")) {
        bool found = false;
        for (const auto& preset : presets)
            if (config["preset"].toString() == preset.id) { state = preset.state; found = true; }
        if (!found) return 4;
    }
    if (auto* values = config["controls"].getDynamicObject()) {
        for (const auto& item : values->getProperties()) {
            bool found = false;
            for (std::size_t i = 0; i < controlCount; ++i)
                if (item.name.toString() == controls[i].id) {
                    const auto v = float(item.value);
                    if (!std::isfinite(v) || v < controls[i].minimum || v > controls[i].maximum) return 5;
                    state.values[i] = v; found = true;
                }
            if (!found) return 5;
        }
    }
    OriginalAmpProcessor processor;
    processor.prepare(reader->sampleRate, 256, 4);
    processor.set(state);
    processor.reset();
    juce::AudioBuffer<float> audio(1, int(reader->lengthInSamples));
    if (!reader->read(&audio, 0, audio.getNumSamples(), 0, true, false)) return 6;
    for (int pos = 0; pos < audio.getNumSamples(); pos += 256) {
        float* data = audio.getWritePointer(0, pos);
        juce::AudioBuffer<float> block(&data, 1, std::min(256, audio.getNumSamples() - pos));
        processor.process(block);
    }
    for (int i = 0; i < audio.getNumSamples(); ++i)
        if (!std::isfinite(audio.getSample(0, i))) return 7;
    std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(audio.getReadPointer(0)),
              std::streamsize(audio.getNumSamples() * sizeof(float)));
    if (!out) return 8;
    std::cout << "rate=" << reader->sampleRate << " oversampling=4 latency=" << processor.latency()
              << " gain=" << state[Control::gain] << " samples=" << audio.getNumSamples() << '\n';
}

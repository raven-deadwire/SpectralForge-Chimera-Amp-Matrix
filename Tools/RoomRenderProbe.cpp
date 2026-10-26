// Renders a mono WAV through the full Chimera processor with parameter
// overrides, for room listening material (docs/ROOM_CORE_1_3_3.md).
//
//   ChimeraRoomRender <in.wav> <out.wav> [id=value ...]
//
// The input is played on both channels at its own sample rate in 128-sample
// blocks; the stereo output is written as 24-bit WAV. Values are plain
// parameter units (e.g. roomOn=1 roomSize=2 roomSend1=50).
#include "PluginProcessor.h"
#include <iostream>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::cerr << "usage: ChimeraRoomRender <in.wav> <out.wav> [id=value ...]\n"; return 2; }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[1])));
    if (!reader) { std::cerr << "cannot read " << argv[1] << "\n"; return 1; }
    const int total = int(reader->lengthInSamples); const double rate = reader->sampleRate;
    juce::AudioBuffer<float> input(1, total); reader->read(&input, 0, total, 0, true, false);
    ChimeraProcessor p;
    for (int i = 3; i < argc; ++i) {
        const juce::String arg(argv[i]); const auto id = arg.upToFirstOccurrenceOf("=", false, false);
        auto* parameter = p.parameters().getParameter(id);
        if (parameter == nullptr) { std::cerr << "unknown parameter " << id << "\n"; return 1; }
        parameter->setValueNotifyingHost(parameter->convertTo0to1(arg.fromFirstOccurrenceOf("=", false, false).getFloatValue()));
    }
    const int block = 128; p.setRateAndBufferSizeDetails(rate, block); p.prepareToPlay(rate, block);
    const int latency = p.getLatencySamples();
    juce::AudioBuffer<float> output(2, total + latency), work(2, block); juce::MidiBuffer midi;
    for (int at = 0; at < total + latency; at += block) {
        const int n = std::min(block, total + latency - at); work.setSize(2, n, false, false, true);
        for (int i = 0; i < n; ++i) { const float x = at + i < total ? input.getSample(0, at + i) : 0.f; work.setSample(0, i, x); work.setSample(1, i, x); }
        p.processBlock(work, midi);
        for (int c = 0; c < 2; ++c) output.copyFrom(c, at, work, c, 0, n);
    }
    // Drop the reported latency so every render lines up with the input.
    juce::File out(argv[2]); out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(out), rate, 2, 24, {}, 0));
    if (!writer) { std::cerr << "cannot write " << argv[2] << "\n"; return 1; }
    writer->writeFromAudioSampleBuffer(output, latency, total);
    std::cout << "latency " << latency << "\n";
    return 0;
}

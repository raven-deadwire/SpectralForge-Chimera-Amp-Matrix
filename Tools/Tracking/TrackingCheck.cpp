// ChimeraTrackingCheck (docs/TRACKING_1_10.md): measures the SYNTH guitar
// tracker on recordings.
//
//   ChimeraTrackingCheck [--sensitivity dB] [--labels notes.csv] di.wav ...
//   ChimeraTrackingCheck --synthetic [--write directory]
//   --frames frames.csv  also writes every analysis frame (time, Hz, clarity,
//                        level, gate, attacks) of every recording
//
// Each WAV is read (mono: channels are averaged) and run through the
// tracker and guitar gate. With --labels (one file per WAV, in order) the
// notes come from the file: `start_s,end_s,pitch[,name]`, pitch in Hz or as a
// MIDI note. Without one, an offline reference tracker estimates the notes
// (steady notes only; marked "reference" in the report). --synthetic runs
// the built-in corpus; --write also saves it as WAV + CSV, so the file path
// can be checked end to end.
#include "TrackingEvaluation.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

using namespace spectralforge::tracking;

namespace {
bool readWav(const juce::File& file, std::vector<float>& audio, double& rate) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > juce::int64(reader->sampleRate * 60 * 30)) return false;
    juce::AudioBuffer<float> buffer(int(reader->numChannels), int(reader->lengthInSamples));
    if (!reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true)) return false;
    rate = reader->sampleRate;audio.assign(size_t(buffer.getNumSamples()), 0.f);
    for (int c = 0; c < buffer.getNumChannels(); ++c) for (int n = 0; n < buffer.getNumSamples(); ++n) audio[size_t(n)] += buffer.getSample(c, n) / float(buffer.getNumChannels());
    return true;
}
bool writeWav(const juce::File& file, const std::vector<float>& audio, double rate) {
    file.deleteFile();
    auto stream = file.createOutputStream();if (stream == nullptr) return false;
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), rate, 1, 24, {}, 0));
    if (writer == nullptr) return false;stream.release();
    const float* channels[1]{audio.data()};
    return writer->writeFromFloatArrays(channels, 1, int(audio.size()));
}
int usage() {
    std::cout << "ChimeraTrackingCheck [--sensitivity dB] [--labels notes.csv] di.wav ...\n"
                 "ChimeraTrackingCheck --synthetic [--write directory]\n"
                 "Label file: start_s,end_s,pitch[,name] per line; pitch in Hz or a MIDI note; # comments.\n";
    return 2;
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    float sensitivity = -40.f;bool synthetic = false;juce::File writeTo, framesTo;juce::String frameText("recording,time_s,hz,clarity,rms,gate,attacks\n");
    const auto keep=[&](const std::string& name,const std::vector<Frame>& frames){if(framesTo==juce::File())return;for(const auto& f:frames)frameText<<juce::String(name).replaceCharacter(',',' ')<<","<<juce::String(f.time,5)<<","<<juce::String(f.hz,3)<<","<<juce::String(f.clarity,3)<<","<<juce::String(f.rms,5)<<","<<int(f.gate)<<","<<f.attacks<<"\n";};std::vector<juce::File> wavs, labelFiles;
    for (int i = 1; i < argc; ++i) {
        const juce::String a(argv[i]);
        if (a == "--help" || a == "-h") return usage();
        else if (a == "--sensitivity" && i + 1 < argc) sensitivity = juce::String(argv[++i]).getFloatValue();
        else if (a == "--labels" && i + 1 < argc) labelFiles.push_back(juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]));
        else if (a == "--synthetic") synthetic = true;
        else if (a == "--frames" && i + 1 < argc) framesTo = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]);
        else if (a == "--write" && i + 1 < argc) writeTo = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]);
        else wavs.push_back(juce::File::getCurrentWorkingDirectory().getChildFile(a));
    }
    if (!synthetic && wavs.empty()) return usage();
    const Targets targets;std::vector<Report> reports;
    if (synthetic) {
        if (writeTo != juce::File()) writeTo.createDirectory();
        int index = 0;
        for (const auto& p : corpus()) {
            const auto frames = analyse(p.audio, p.rate, sensitivity);keep(p.name, frames);
            reports.push_back(evaluate(p.name, frames, p.labels, p.outOfScope, sensitivity));
            if (writeTo != juce::File()) {
                const auto base = juce::String(++index).paddedLeft('0', 2) + "-" + juce::File::createLegalFileName(p.name).replaceCharacter(' ', '_');
                if (!writeWav(writeTo.getChildFile(base + ".wav"), p.audio, p.rate)) { std::cerr << "Cannot write " << base << ".wav\n";return 1; }
                juce::String csv("# start_s,end_s,pitch_hz,name (pitch at the note start)\n");
                for (const auto& l : p.labels) csv << juce::String(l.start, 4) << "," << juce::String(l.end, 4) << "," << juce::String(l.pitchAt(l.start), 3) << "," << juce::String(l.name).replaceCharacter(',', ' ') << "\n";
                writeTo.getChildFile(base + ".csv").replaceWithText(csv);
            }
        }
    }
    for (size_t i = 0; i < wavs.size(); ++i) {
        std::vector<float> audio;double rate = 48000;
        if (!readWav(wavs[i], audio, rate)) { std::cerr << "Cannot read " << wavs[i].getFullPathName() << "\n";return 1; }
        std::vector<Label> labels;std::string name = wavs[i].getFileName().toStdString();
        if (i < labelFiles.size()) labels = readLabels(labelFiles[i].loadFileAsString().toStdString());
        else { labels = referenceLabels(audio, rate, sensitivity);name += " (reference labels)"; }
        const auto frames = analyse(audio, rate, sensitivity);keep(name, frames);
        reports.push_back(evaluate(name, frames, labels, false, sensitivity));
    }
    if (framesTo != juce::File()) framesTo.replaceWithText(frameText);
    std::cout << markdown(reports, targets);
    std::cout << "\nTargets (notes in 55-1400 Hz): lock within " << targets.lockMs << " ms, median error " << targets.medianCents << " cents after a tracking lag of at most " << targets.lagMs << " ms, octave errors "
              << targets.octaveShare * 100 << " %, dropouts " << targets.dropoutShare * 100 << " %, one attack per picked note (at most one for legato), gate open outside notes "
              << targets.falseGateSeconds * 1000 << " ms per recording.\n";
    return 0;
}

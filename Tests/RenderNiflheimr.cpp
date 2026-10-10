#include "Amplifier.h"
#include "NiflheimrMeasurementBuild.h"
#include <chrono>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

// CMake binds each configured build to the selected DSP source snapshots.
#ifndef CHIMERA_NIFLHEIMR_GIT_HEAD
 #define CHIMERA_NIFLHEIMR_GIT_HEAD "unavailable"
#endif
#ifndef CHIMERA_NIFLHEIMR_CORE_SHA256
 #define CHIMERA_NIFLHEIMR_CORE_SHA256 "unavailable"
#endif
#ifndef CHIMERA_NIFLHEIMR_DEFINITION_SHA256
 #define CHIMERA_NIFLHEIMR_DEFINITION_SHA256 "unavailable"
#endif
#ifndef CHIMERA_NIFLHEIMR_WRAPPER_SHA256
 #define CHIMERA_NIFLHEIMR_WRAPPER_SHA256 "unavailable"
#endif

// Head-only development renderer. Uses the production native oversampler and
// compensation, writes unnormalised IEEE float WAV, and never authenticates a
// supplied WAV as a real instrument recording or claims musical acceptance.
namespace {
using namespace spectralforge;
void require(bool condition, const juce::String& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}
juce::var observeFile(const juce::File& file, const char* phase, const juce::String& logicalName = {}) {
    auto row = std::make_unique<juce::DynamicObject>();
    row->setProperty("phase", phase);
    row->setProperty("file", logicalName.isEmpty() ? file.getFileName() : logicalName);
    row->setProperty("physical_filename", file.getFileName());
    row->setProperty("bytes", file.getSize());
    row->setProperty("file_identifier", juce::String(file.getFileIdentifier()));
    auto input = file.createInputStream();
    require(input != nullptr && input->openedOk(), "Cannot inspect output WAV.");
    row->setProperty("sha256", juce::SHA256(*input).toHexString());
    row->setProperty("bytes_read", input->getPosition());
    row->setProperty("post_read_bytes", file.getSize());
    const juce::var result(row.release());
    std::cout << "NIFLHEIMR_IO " << juce::JSON::toString(result, true) << std::endl;
    return result;
}
void verifyObservation(const juce::var& row, juce::int64 expectedBytes, const juce::String& expectedHash = {}) {
    require(juce::int64(row["bytes"]) == expectedBytes && juce::int64(row["bytes_read"]) == expectedBytes
            && juce::int64(row["post_read_bytes"]) == expectedBytes
            && (expectedHash.isEmpty() || row["sha256"].toString() == expectedHash),
            "Output integrity mismatch at " + row["phase"].toString() + ": " + row["file"].toString());
}
void usage() {
    std::cout << "Usage: ChimeraNiflheimrRender input.wav NEW-output-directory\n"
                 "  [--oversampling 1|2|4|8] [--block-size 1..8192]\n"
                 "  [--controls controls.json] [--tail-seconds 0..10]\n"
                 "  [--input-kind supplied|synthetic]\n"
                 "  [--benchmark-repeats 0..30] [--benchmark-blocks 16..8192]\n"
                 "  [--benchmark-warmup-blocks 1..8192]\n"
                 "controls.json: {\"controls\": {\"gain\": 0.5, \"blend\": 0.65}}\n"
                 "All five channels receive identical audio and control overrides.\n"
                 "Outputs are head-only float WAVs, without Cab/IR, PRE or POST.\n";
}
double number(const juce::String& value, const char* option) {
    std::size_t consumed = 0;
    double parsed = 0;
    try { parsed = std::stod(value.toStdString(), &consumed); }
    catch (...) { throw std::runtime_error(std::string("Invalid number for ") + option); }
    require(consumed == value.toStdString().size() && std::isfinite(parsed), juce::String("Invalid number for ") + option);
    return parsed;
}
struct Options {
    juce::File input, destination, controls;
    int factor = 4, block = 256;
    int repeats = 0, measuredBlocks = 512, warmupBlocks = 512;
    double tail = .5;
    juce::String inputKind = "supplied";
};
Options parse(int argc, char** argv) {
    require(argc >= 3, "Input WAV and new output directory are required; use --help.");
    Options options;
    options.input = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    options.destination = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
    juce::StringArray seen;
    for (int i = 3; i < argc; i += 2) {
        const juce::String key(argv[i]);
        require(i + 1 < argc, "Missing value for " + key);
        require(!seen.contains(key), "Duplicate option " + key);
        seen.add(key);
        const juce::String value(argv[i + 1]);
        if (key == "--benchmark-repeats" || key == "--benchmark-blocks" || key == "--benchmark-warmup-blocks") {
            const auto n = number(value, key.toRawUTF8());
            const int low = key == "--benchmark-repeats" ? 0 : key == "--benchmark-blocks" ? 16 : 1;
            const int high = key == "--benchmark-repeats" ? 30 : 8192;
            require(n >= low && n <= high && n == std::floor(n), "Invalid benchmark integer: " + key);
            if (key == "--benchmark-repeats") options.repeats = int(n);
            else if (key == "--benchmark-blocks") options.measuredBlocks = int(n);
            else options.warmupBlocks = int(n);
        } else if (key == "--oversampling") {
            const auto n = number(value, "--oversampling");
            require(n == 1 || n == 2 || n == 4 || n == 8, "Oversampling must be 1, 2, 4 or 8.");
            options.factor = int(n);
        } else if (key == "--block-size") {
            const auto n = number(value, "--block-size");
            require(n >= 1 && n <= 8192 && n == std::floor(n), "Block size must be an integer from 1 to 8192.");
            options.block = int(n);
        } else if (key == "--tail-seconds") {
            options.tail = number(value, "--tail-seconds");
            require(options.tail >= 0 && options.tail <= 10, "Tail must be from 0 to 10 seconds.");
        } else if (key == "--controls") {
            options.controls = juce::File::getCurrentWorkingDirectory().getChildFile(value);
        } else if (key == "--input-kind") {
            require(value == "supplied" || value == "synthetic", "Input kind must be supplied or synthetic.");
            options.inputKind = value;
        } else throw std::runtime_error("Unknown option " + key.toStdString());
    }
    require(options.input.existsAsFile(), "Input WAV does not exist.");
    require(!options.destination.exists(), "Output path already exists; choose a new directory.");
    return options;
}
juce::var configuration(const juce::File& file) {
    if (file == juce::File{}) return juce::var(new juce::DynamicObject());
    require(file.existsAsFile(), "Controls JSON does not exist.");
    const auto config = juce::JSON::parse(file);
    require(config.getDynamicObject() != nullptr, "Controls JSON must contain an object.");
    for (const auto& property : config.getDynamicObject()->getProperties())
        require(property.name.toString() == "controls", "Unknown configuration key " + property.name.toString());
    if (config.hasProperty("controls")) require(config["controls"].getDynamicObject() != nullptr, "controls must be an object.");
    return config;
}
AmpNativeState stateFor(int channel, const juce::var& config) {
    auto state = defaultAmpNativeState(niflheimrAmpModel);
    state.channel = channel;
    const auto defaults = niflheimr::channelState(channel);
    for (std::size_t i = 0; i < niflheimr::controlCount; ++i) state.values[i] = defaults.values[i];
    if (const auto* values = config["controls"].getDynamicObject())
        for (const auto& item : values->getProperties()) {
            bool found = false;
            for (std::size_t i = 0; i < niflheimr::controlCount; ++i)
                if (item.name.toString() == niflheimr::controls[i].id) {
                    require(item.value.isDouble() || item.value.isInt() || item.value.isInt64(), "Control must be numeric: " + item.name.toString());
                    const double value = double(item.value);
                    const auto& control = niflheimr::controls[i];
                    require(std::isfinite(value) && value >= control.minimum && value <= control.maximum,
                            "Control out of range: " + item.name.toString());
                    state.values[i] = float(value);
                    found = true;
                }
            require(found, "Unknown control " + item.name.toString());
        }
    return state;
}
// Format 3 + fact chunk explicitly preserves values outside [-1, 1]. JUCE's
// ordinary 32-bit WAV writer is PCM, which would clip the measured head output.
class FloatWav {
    std::unique_ptr<juce::FileOutputStream> stream;
    std::vector<char> bytes;
public:
    FloatWav(const juce::File& file, int channels, int rate, int frames, int block)
        : stream(file.createOutputStream()), bytes(std::size_t(channels * block * 4)) {
        require(stream != nullptr && stream->openedOk(), "Cannot create output WAV.");
        const auto dataBytes = std::uint32_t(std::uint64_t(frames) * std::uint64_t(channels) * 4);
        stream->write("RIFF", 4); stream->writeInt(int(dataBytes + 50)); stream->write("WAVEfmt ", 8);
        stream->writeInt(18); stream->writeShort(3); stream->writeShort(short(channels));
        stream->writeInt(rate); stream->writeInt(rate * channels * 4); stream->writeShort(short(channels * 4));
        stream->writeShort(32); stream->writeShort(0);
        stream->write("fact", 4); stream->writeInt(4); stream->writeInt(frames);
        stream->write("data", 4); stream->writeInt(int(dataBytes));
    }
    void write(const juce::AudioBuffer<float>& audio) {
        std::size_t offset = 0;
        for (int n = 0; n < audio.getNumSamples(); ++n)
            for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
                const auto bits = std::bit_cast<std::uint32_t>(audio.getSample(channel, n));
                for (int byte = 0; byte < 4; ++byte) bytes[offset++] = char((bits >> (8 * byte)) & 255);
            }
        require(stream->write(bytes.data(), offset), "Output WAV write failed.");
    }
    void finish() {
        stream->flush();
        require(stream->getStatus().wasOk(), "Output WAV flush failed.");
        stream.reset();
    }
};
// Fresh production wrapper per repetition; all reads/copies/statistics are outside
// the measured region. Timings include scheduler interruptions and clock overhead.
// This is an offline head benchmark, not an audio-device callback or DAW test.
juce::var benchmark(const Options& options, juce::AudioFormatReader& reader,
                    const AmpNativeState& state) {
    if (options.repeats == 0) return {};
    juce::ScopedNoDenormals noDenormals;
    using Clock = std::chrono::steady_clock;
    require(Clock::is_steady, "Benchmark requires a monotonic clock.");
    juce::AudioBuffer<float> source(int(reader.numChannels), int(reader.lengthInSamples));
    require(reader.read(&source, 0, source.getNumSamples(), 0, true, reader.numChannels == 2), "Benchmark source read failed.");
    juce::Array<juce::var> repetitions;
    for (int repetition = 0; repetition < options.repeats; ++repetition) {
        Amp amp;
        amp.prepare({reader.sampleRate, juce::uint32(options.block), reader.numChannels});
        amp.setNative(state);
        amp.setOversampling(options.factor == 1 ? 0 : options.factor == 2 ? 1 : options.factor == 4 ? 2 : 3);
        amp.reset();
        juce::AudioBuffer<float> buffer(int(reader.numChannels), options.block);
        std::vector<double> timings(std::size_t(options.measuredBlocks), 0.0);
        double checksum = 0;
        int cursor = 0;
        for (int block = -options.warmupBlocks; block < options.measuredBlocks; ++block) {
            for (int n = 0; n < options.block; ++n) {
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    buffer.setSample(channel, n, source.getSample(channel, cursor));
                cursor = (cursor + 1) % source.getNumSamples();
            }
            if (block < 0) amp.process(buffer);
            else {
                const auto start = Clock::now();
                amp.process(buffer);
                const auto stop = Clock::now();
                timings[std::size_t(block)] = std::chrono::duration<double, std::micro>(stop - start).count();
            }
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int n = 0; n < options.block; ++n) {
                    const double value = buffer.getSample(channel, n);
                    require(std::isfinite(value), "Benchmark produced nonfinite output.");
                    checksum += value * value;
                }
        }
        juce::Array<juce::var> raw;
        for (const auto us : timings) raw.add(us);
        auto row = std::make_unique<juce::DynamicObject>();
        row->setProperty("repeat", repetition);
        row->setProperty("block_us", raw);
        row->setProperty("output_energy_checksum", checksum);
        repetitions.add(juce::var(row.release()));
    }
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("protocol", "production-process-steady-clock.v1");
    result->setProperty("clock", "std::chrono::steady_clock");
    result->setProperty("denormals", "juce::ScopedNoDenormals");
    result->setProperty("clock_period_seconds", double(Clock::period::num) / double(Clock::period::den));
    result->setProperty("timed_scope", "Amp::process only; buffer copy, file I/O, setup, checksum and statistics excluded");
    result->setProperty("warmup_blocks_per_repeat", options.warmupBlocks);
    result->setProperty("measured_blocks_per_repeat", options.measuredBlocks);
    result->setProperty("block_budget_us", 1.0e6 * options.block / reader.sampleRate);
    result->setProperty("repetitions", repetitions);
    return juce::var(result.release());
}
juce::var render(const Options& options, const juce::File& staging, juce::AudioFormatReader& reader,
                 const AmpNativeState& state, int outputFrames) {
    const auto index = state.channel;
    const auto filename = "CH" + juce::String(index + 1) + "-" + juce::String(niflheimr::channelKeys[index]) + ".wav";
    const auto output = staging.getChildFile(filename);
    // Keep the final WAV name immutable: an observer must never see it growing.
    // Closing a stream alone cannot protect a live pathname from replacement.
    const auto writing = staging.getChildFile(filename + ".writing");
    Amp amp;
    amp.prepare({reader.sampleRate, juce::uint32(options.block), reader.numChannels});
    amp.setNative(state);
    amp.setOversampling(options.factor == 1 ? 0 : options.factor == 2 ? 1 : options.factor == 4 ? 2 : 3);
    amp.reset();
    FloatWav wav(writing, int(reader.numChannels), int(reader.sampleRate), outputFrames, options.block);
    juce::AudioBuffer<float> buffer(int(reader.numChannels), options.block);
    double peak = 0, energy = 0;
    juce::int64 overUnity = 0;
    for (int offset = 0; offset < outputFrames; offset += options.block) {
        const int count = std::min(options.block, outputFrames - offset);
        buffer.setSize(int(reader.numChannels), count, false, false, true);
        buffer.clear();
        const auto available = int(std::max<juce::int64>(0, std::min<juce::int64>(count, reader.lengthInSamples - offset)));
        if (available > 0) require(reader.read(&buffer, 0, available, offset, true, reader.numChannels == 2), "Cannot read source WAV.");
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int n = 0; n < count; ++n) require(std::isfinite(buffer.getSample(channel, n)), "Input contains NaN or infinity.");
        amp.process(buffer);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int n = 0; n < count; ++n) {
                const double value = buffer.getSample(channel, n);
                require(std::isfinite(value), "DSP produced NaN or infinity.");
                peak = std::max(peak, std::abs(value)); energy += value * value;
                if (std::abs(value) > 1) ++overUnity;
            }
        wav.write(buffer);
    }
    wav.finish();
    const auto expectedBytes = juce::int64(58) + juce::int64(outputFrames) * reader.numChannels * 4;
    const auto observation = observeFile(writing, "post_flush", filename);
    verifyObservation(observation, expectedBytes);
    require(!output.exists() && writing.moveFileTo(output), "Cannot publish complete WAV.");
    verifyObservation(observeFile(output, "post_file_publish"), expectedBytes, observation["sha256"].toString());
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("channel_index", index);
    result->setProperty("channel_name", juce::String(niflheimr::channelNames[index]));
    result->setProperty("channel_key", juce::String(niflheimr::channelKeys[index]));
    result->setProperty("file", filename);
    result->setProperty("sha256", observation["sha256"]);
    result->setProperty("post_flush", observation);
    result->setProperty("latency_samples", amp.latency());
    result->setProperty("peak", peak);
    result->setProperty("rms", std::sqrt(energy / (double(outputFrames) * reader.numChannels)));
    result->setProperty("samples_over_unity", overUnity);
    auto settings = std::make_unique<juce::DynamicObject>();
    for (std::size_t i = 0; i < niflheimr::controlCount; ++i) settings->setProperty(niflheimr::controls[i].id, state.values[i]);
    result->setProperty("controls", juce::var(settings.release()));
    if (options.repeats > 0) result->setProperty("cpu", benchmark(options, reader, state));
    return juce::var(result.release());
}
} // namespace

int main(int argc, char** argv) {
    juce::File staging;
    try {
        if (argc == 2 && juce::String(argv[1]) == "--help") { usage(); return 0; }
        const auto options = parse(argc, argv);
        const auto config = configuration(options.controls);
        std::array<AmpNativeState, niflheimr::channelCount> states;
        for (int channel = 0; channel < niflheimr::channelCount; ++channel) states[std::size_t(channel)] = stateFor(channel, config);
        juce::WavAudioFormat format;
        auto input = options.input.createInputStream();
        require(input != nullptr, "Cannot open input WAV.");
        auto reader = std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(input.release(), true));
        require(reader != nullptr, "Input must be a readable WAV.");
        require(reader->numChannels == 1 || reader->numChannels == 2, "Only mono and stereo WAV are supported.");
        require(reader->sampleRate >= 8000 && reader->sampleRate <= 384000 && reader->sampleRate == std::floor(reader->sampleRate), "Unsupported WAV sample rate.");
        require(reader->lengthInSamples > 0 && reader->lengthInSamples <= reader->sampleRate * 600, "WAV duration must be greater than zero and at most 600 seconds.");
        const auto frames = reader->lengthInSamples + juce::int64(std::llround(options.tail * reader->sampleRate));
        require(std::uint64_t(frames) * reader->numChannels * 4 + 50 < std::uint64_t(0xffffffff), "Output would exceed RIFF size limit.");
        const auto inputHash = juce::SHA256(options.input).toHexString();
        staging = options.destination.getSiblingFile(options.destination.getFileName() + ".partial-" + juce::Uuid().toString());
        require(staging.createDirectory().wasOk(), "Cannot create staging directory.");
        juce::Array<juce::var> outputs;
        for (const auto& state : states) outputs.add(render(options, staging, *reader, state, int(frames)));
        require(juce::SHA256(options.input).toHexString() == inputHash, "Input WAV changed during rendering.");
        auto manifest = std::make_unique<juce::DynamicObject>();
        manifest->setProperty("schema", "spectralforge.niflheimr.head-render.v1");
        manifest->setProperty("configured_git_head", CHIMERA_NIFLHEIMR_GIT_HEAD);
        manifest->setProperty("build", juce::JSON::parse(CHIMERA_NIFLHEIMR_BUILD_JSON));
        manifest->setProperty("release_approved", false);
        auto sourceHashes = std::make_unique<juce::DynamicObject>();
        sourceHashes->setProperty("NiflheimrDSP.h", CHIMERA_NIFLHEIMR_CORE_SHA256);
        sourceHashes->setProperty("NiflheimrDefinition.h", CHIMERA_NIFLHEIMR_DEFINITION_SHA256);
        sourceHashes->setProperty("Amplifier.h", CHIMERA_NIFLHEIMR_WRAPPER_SHA256);
        manifest->setProperty("source_hashes", juce::var(sourceHashes.release()));
        manifest->setProperty("source_identity_note", "Configured HEAD may have local modifications; source hashes are authoritative for the listed files.");
        manifest->setProperty("engine", "production Amplifier.h / AmpNativeDSP / NiflheimrDSP");
        manifest->setProperty("scope", "head-only; no Cab/IR, PRE or POST; no normalization or intentional output limiter; DSP emergency safety rails remain");
        manifest->setProperty("musical_acceptance", "PENDING actual instrument DI / same IR / level-matched listening");
        manifest->setProperty("input_kind", options.inputKind == "synthetic" ? "synthetic test signal (caller declared)" : "supplied WAV; instrument provenance not verified");
        manifest->setProperty("input_file", options.input.getFileName());
        manifest->setProperty("input_sha256", inputHash);
        manifest->setProperty("sample_rate", reader->sampleRate);
        manifest->setProperty("audio_channels", int(reader->numChannels));
        manifest->setProperty("input_frames", reader->lengthInSamples);
        manifest->setProperty("output_frames", frames);
        manifest->setProperty("tail_frames", frames - reader->lengthInSamples);
        manifest->setProperty("oversampling_factor", options.factor);
        manifest->setProperty("block_size", options.block);
        manifest->setProperty("latency_compensated", false);
        manifest->setProperty("format", "WAV IEEE float32 little endian; values beyond unity preserved");
        manifest->setProperty("same_input_all_channels", true);
        manifest->setProperty("outputs", outputs);
        const auto text = juce::JSON::toString(juce::var(manifest.release()), true);
        require(staging.getChildFile("manifest.json").replaceWithText(text), "Cannot write manifest.");
        for (const auto& row : outputs)
            verifyObservation(observeFile(staging.getChildFile(row["file"].toString()), "pre_publish"),
                              58 + frames * reader->numChannels * 4, row["sha256"].toString());
        require(!options.destination.exists() && staging.moveFileTo(options.destination), "Cannot publish output directory.");
        staging = options.destination;
        for (const auto& row : outputs)
            verifyObservation(observeFile(options.destination.getChildFile(row["file"].toString()), "post_publish"),
                              58 + frames * reader->numChannels * 4, row["sha256"].toString());
        staging = juce::File{};
        std::cout << "Rendered 5 Niflheimr channels: " << options.destination.getFullPathName()
                  << "\nHead-only prototype renders; musical acceptance remains pending.\n";
        return 0;
    } catch (const std::exception& error) {
        if (staging.exists()) {
            const auto name = staging.getFileName();
            const auto failed = staging.getSiblingFile(name.contains(".partial-")
                ? name.replace(".partial-", ".failed-") : name + ".failed-" + juce::Uuid().toString());
            const auto preserved = staging.moveFileTo(failed) ? failed : staging;
            std::cerr << "Incomplete render preserved: " << preserved.getFullPathName() << '\n';
        }
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}

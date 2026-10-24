// SPDX-License-Identifier: GPL-2.0-or-later
// Headless libobs integration test. Loads the built module through libobs's
// own module loader, attaches the Chimera filter to an audio source and
// checks, through OBS's post-filter audio capture:
//   - OBS output equals the same input run directly through the Chimera VST3
//   - input mapping, mute/dry policy without Chimera, scene save/restore
//   - destroy on the OBS destroy thread hands teardown to the UI thread
//   - realtime counters, tail reset after an input gap, OBS sample rate and
//     speaker layout changes, and a long session with random block sizes
//     (CHIMERA_OBS_SOAK_SECONDS of audio, default 120)
// Usage: <chimera-obs module> <module data dir> <Chimera.vst3>
#include "base64.hpp"
#include "chimera-host.hpp"

#include <obs.h>
#include <util/base.h>
#include <util/bmem.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {
constexpr const char* filterId = "spectralforge_chimera_filter";
constexpr int rate = 48000;
constexpr int tick = 1024;
constexpr double pi = 3.14159265358979323846;

void check(bool ok, const std::string& what) { if (!ok) throw std::runtime_error(what); }

// --- OBS UI thread stand-in: tasks queued from other threads run here. ---
std::mutex uiLock;
std::deque<std::pair<obs_task_t, void*>> uiTasks;
void uiHandler(obs_task_t task, void* param, bool wait) {
    check(!wait, "the module must never block on the UI thread");
    std::lock_guard<std::mutex> guard(uiLock);
    uiTasks.emplace_back(task, param);
}
void drainUi() {
    for (;;) {
        std::pair<obs_task_t, void*> next;
        {
            std::lock_guard<std::mutex> guard(uiLock);
            if (uiTasks.empty()) return;
            next = uiTasks.front();
            uiTasks.pop_front();
        }
        next.first(next.second);
    }
}

std::mutex logLock;
std::vector<std::string> logLines;
void logHandler(int level, const char* format, va_list args, void*) {
    char line[4096];
    std::vsnprintf(line, sizeof(line), format, args);
    std::lock_guard<std::mutex> guard(logLock);
    logLines.emplace_back(line);
    if (level <= LOG_WARNING || std::string(line).find("[chimera-obs]") != std::string::npos || getenv("CHIMERA_OBS_TEST_VERBOSE")) std::cerr << line << '\n';
}
size_t logCount(const std::string& needle) {
    std::lock_guard<std::mutex> guard(logLock);
    return static_cast<size_t>(std::count_if(logLines.begin(), logLines.end(), [&](const std::string& l) { return l.find(needle) != std::string::npos; }));
}

// --- A minimal async audio source; the test pushes audio into it. ---
const char* testName(void*) { return "Chimera test input"; }
void* testCreate(obs_data_t*, obs_source_t* source) { return source; }
void testDestroy(void*) {}

std::vector<float> guitar(int frames, int channel, int start = 0) {
    std::vector<float> out(static_cast<size_t>(frames));
    const double notes[] = {82.41, 110.0, 146.83, 98.0};
    for (int i = 0; i < frames; ++i) {
        const int n = start + i;
        const double t = (n % 24000) / double(rate), f = notes[(n / 24000) % 4];
        double v = 0;
        for (int h = 1; h <= 6; ++h) v += std::sin(2 * pi * f * h * t + channel * 0.1) / h;
        out[static_cast<size_t>(i)] = static_cast<float>(0.25 * v * std::exp(-3.0 * t));
    }
    return out;
}

struct Capture {
    std::vector<float> channel[2];
};
void captureAudio(void* param, obs_source_t*, const audio_data* audio, bool) {
    auto* c = static_cast<Capture*>(param);
    for (int ch = 0; ch < 2; ++ch) {
        const auto* p = reinterpret_cast<const float*>(audio->data[ch]);
        c->channel[ch].insert(c->channel[ch].end(), p, p + audio->frames);
    }
}

// Source timestamps; a test skips ahead to simulate a stopped input.
uint64_t nextTimestamp = 1000000000ull;

// Pushes `frames` of the stereo test signal (starting at `start`) through the
// source, `gapNs` after the previous push ended; returns what OBS delivered
// after the filter chain.
Capture push(obs_source_t* source, int start, int frames, bool silent = false, uint64_t gapNs = 0) {
    Capture captured;
    obs_source_add_audio_capture_callback(source, captureAudio, &captured);
    auto left = guitar(start + frames, 0), right = guitar(start + frames, 1);
    if (silent) {
        std::fill(left.begin(), left.end(), 0.f);
        std::fill(right.begin(), right.end(), 0.f);
    }
    nextTimestamp += gapNs;
    for (int offset = 0; offset < frames; offset += tick) {
        obs_source_audio audio{};
        audio.data[0] = reinterpret_cast<const uint8_t*>(left.data() + start + offset);
        audio.data[1] = reinterpret_cast<const uint8_t*>(right.data() + start + offset);
        audio.frames = static_cast<uint32_t>(std::min(tick, frames - offset));
        audio.speakers = SPEAKERS_STEREO;
        audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
        audio.samples_per_sec = rate;
        audio.timestamp = nextTimestamp;
        nextTimestamp += uint64_t(audio.frames) * 1000000000ull / rate;
        obs_source_output_audio(source, &audio);
    }
    obs_source_remove_audio_capture_callback(source, captureAudio, &captured);
    check(captured.channel[0].size() == size_t(frames), "OBS did not deliver every frame through the filter");
    return captured;
}

double rms(const std::vector<float>& x) {
    double sum = 0;
    for (float v : x) sum += double(v) * v;
    return x.empty() ? 0.0 : std::sqrt(sum / double(x.size()));
}

double maxDifference(const std::vector<float>& a, const std::vector<float>& b, size_t from) {
    double worst = 0;
    for (size_t n = from; n < std::min(a.size(), b.size()); ++n) worst = std::max(worst, std::abs(double(a[n]) - b[n]));
    return worst;
}

obs_source_t* makeFilter(const std::string& name, const std::string& bundle, long long mapping = 0, long long policy = 0,
                         const std::string& componentState = {}, long long schema = 0) {
    obs_data_t* settings = obs_data_create();
    obs_data_set_string(settings, "bundle_path", bundle.c_str());
    obs_data_set_int(settings, "input_mapping", mapping);
    obs_data_set_int(settings, "failure_policy", policy);
    if (!componentState.empty()) obs_data_set_string(settings, "vst3_component_state", componentState.c_str());
    if (schema != 0) obs_data_set_int(settings, "chimera_obs_schema", schema);
    obs_source_t* filter = obs_source_create(filterId, name.c_str(), settings, nullptr);
    obs_data_release(settings);
    check(filter != nullptr, "filter creation failed");
    drainUi(); // create() hands the plugin load to the UI thread
    return filter;
}

std::string propertyText(obs_source_t* filter, const char* name) {
    obs_properties_t* props = obs_source_properties(filter);
    obs_property_t* p = obs_properties_get(props, name);
    std::string text = p != nullptr ? obs_property_description(p) : "";
    obs_properties_destroy(props);
    return text;
}

void settle() { std::this_thread::sleep_for(std::chrono::milliseconds(1500)); }

// The filter's script interface (proc handler), as an OBS script would call it.
bool setParameter(obs_source_t* filter, std::uint32_t id, double value) {
    calldata_t call;
    calldata_init(&call);
    calldata_set_int(&call, "id", id);
    calldata_set_float(&call, "value", value);
    proc_handler_call(obs_source_get_proc_handler(filter), "chimera_set_parameter", &call);
    const bool ok = calldata_bool(&call, "ok");
    calldata_free(&call);
    return ok;
}

double getParameter(obs_source_t* filter, std::uint32_t id) {
    calldata_t call;
    calldata_init(&call);
    calldata_set_int(&call, "id", id);
    proc_handler_call(obs_source_get_proc_handler(filter), "chimera_get_parameter", &call);
    const double value = calldata_bool(&call, "ok") ? calldata_float(&call, "value") : -1.0;
    calldata_free(&call);
    return value;
}

bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

struct Stats {
    double audioSeconds{}, averageLoad{}, peakLoad{};
    long long blocks{}, overruns{}, gaps{}, restarts{}, unprocessed{};
};
Stats stats(obs_source_t* filter) {
    calldata_t call;
    calldata_init(&call);
    proc_handler_call(obs_source_get_proc_handler(filter), "chimera_get_stats", &call);
    Stats s{calldata_float(&call, "audio_seconds"), calldata_float(&call, "average_load"), calldata_float(&call, "peak_load"),
            calldata_int(&call, "blocks"), calldata_int(&call, "overruns"), calldata_int(&call, "gaps"),
            calldata_int(&call, "restarts"), calldata_int(&call, "unprocessed")};
    calldata_free(&call);
    return s;
}

// Resident memory of this process in MiB (Linux without AddressSanitizer,
// whose quarantine keeps freed memory resident; 0 elsewhere).
double residentMiB() {
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
    long pages = 0, resident = 0;
    if (FILE* f = std::fopen("/proc/self/statm", "r")) {
        if (std::fscanf(f, "%ld %ld", &pages, &resident) != 2) resident = 0;
        std::fclose(f);
    }
    return double(resident) * 4096.0 / (1024.0 * 1024.0);
#else
    return 0.0;
#endif
}

// Pushes a stereo signal through the source in the given block sizes and
// returns the filtered output (any OBS speaker layout; first two channels).
Capture pushBlocks(obs_source_t* source, const std::vector<float>& left, const std::vector<float>& right,
                   const std::vector<int>& sizes, uint64_t gapNs = 0) {
    Capture captured;
    for (auto& channel : captured.channel) channel.reserve(left.size()); // same allocations every call
    obs_source_add_audio_capture_callback(source, captureAudio, &captured);
    nextTimestamp += gapNs;
    size_t offset = 0;
    for (int size : sizes) {
        obs_source_audio audio{};
        audio.data[0] = reinterpret_cast<const uint8_t*>(left.data() + offset);
        audio.data[1] = reinterpret_cast<const uint8_t*>(right.data() + offset);
        audio.frames = static_cast<uint32_t>(size);
        audio.speakers = SPEAKERS_STEREO;
        audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
        audio.samples_per_sec = rate;
        audio.timestamp = nextTimestamp;
        nextTimestamp += uint64_t(size) * 1000000000ull / rate;
        obs_source_output_audio(source, &audio);
        offset += size_t(size);
    }
    obs_source_remove_audio_capture_callback(source, captureAudio, &captured);
    check(captured.channel[0].size() == offset, "OBS did not deliver every frame of the session");
    return captured;
}

// Captures every OBS output channel (after a speaker layout change).
struct Planes {
    std::vector<std::vector<float>> channel;
};
void capturePlanes(void* param, obs_source_t*, const audio_data* audio, bool) {
    auto* c = static_cast<Planes*>(param);
    for (size_t ch = 0; ch < c->channel.size(); ++ch) {
        const auto* p = reinterpret_cast<const float*>(audio->data[ch]);
        c->channel[ch].insert(c->channel[ch].end(), p, p + audio->frames);
    }
}

// Pushes `frames` of a per-channel test tone in the given OBS format and
// returns every output channel.
Planes pushFormat(obs_source_t* source, int sampleRate, speaker_layout layout, int channels, int frames) {
    std::vector<std::vector<float>> in(size_t(channels), std::vector<float>(static_cast<size_t>(frames)));
    for (int c = 0; c < channels; ++c)
        for (int n = 0; n < frames; ++n) in[size_t(c)][size_t(n)] = float(0.2 * std::sin(2 * pi * (110.0 * (c + 1)) * n / sampleRate));
    Planes captured{std::vector<std::vector<float>>(size_t(channels))};
    obs_source_add_audio_capture_callback(source, capturePlanes, &captured);
    for (int offset = 0; offset < frames; offset += tick) {
        obs_source_audio audio{};
        for (int c = 0; c < channels; ++c) audio.data[c] = reinterpret_cast<const uint8_t*>(in[size_t(c)].data() + offset);
        audio.frames = static_cast<uint32_t>(std::min(tick, frames - offset));
        audio.speakers = layout;
        audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
        audio.samples_per_sec = static_cast<uint32_t>(sampleRate);
        audio.timestamp = nextTimestamp;
        nextTimestamp += uint64_t(audio.frames) * 1000000000ull / uint64_t(sampleRate);
        obs_source_output_audio(source, &audio);
        drainUi(); // the re-prepare queued by the first block of a new format
    }
    obs_source_remove_audio_capture_callback(source, capturePlanes, &captured);
    for (int c = 0; c < channels; ++c) {
        check(captured.channel[size_t(c)].size() == size_t(frames), "OBS did not deliver every frame in the new format");
        for (int n = 0; n < frames; ++n) captured.channel[size_t(c)][size_t(n)] -= in[size_t(c)][size_t(n)]; // keep only the change
    }
    return captured;
}

bool finite(const std::vector<float>& x, float limit) {
    return std::all_of(x.begin(), x.end(), [&](float v) { return std::isfinite(v) && std::abs(v) < limit; });
}
}

int main(int argc, char** argv) {
    if (argc < 4) { std::cerr << "usage: chimera-obs-filter-tests <module> <data dir> <Chimera.vst3>\n"; return 2; }
    const std::string modulePath = argv[1], dataPath = argv[2], bundle = argv[3];
    base_set_log_handler(logHandler, nullptr);
    try {
        check(obs_startup("en-US", nullptr, nullptr), "obs_startup");
        obs_audio_info audioInfo{rate, SPEAKERS_STEREO};
        check(obs_reset_audio(&audioInfo), "obs_reset_audio");
        obs_set_ui_task_handler(uiHandler);

        obs_source_info input{};
        input.id = "chimera_test_input";
        input.type = OBS_SOURCE_TYPE_INPUT;
        input.output_flags = OBS_SOURCE_AUDIO;
        input.get_name = testName;
        input.create = testCreate;
        input.destroy = testDestroy;
        obs_register_source(&input);

        obs_module_t* module = nullptr;
        check(obs_open_module(&module, modulePath.c_str(), dataPath.c_str()) == MODULE_SUCCESS, "obs_open_module");
        check(obs_init_module(module), "obs_init_module");
        check(std::string(obs_source_get_display_name(filterId)) == "SpectralForge Chimera", "filter is not registered");
        std::cout << "module loaded through libobs " << obs_get_version_string() << '\n';

        // 1. OBS path vs direct VST3 path on the same input.
        obs_source_t* source = obs_source_create("chimera_test_input", "DI", nullptr, nullptr);
        obs_source_t* filter = makeFilter("Chimera", bundle);
        obs_source_filter_add(source, filter);
        obs_set_output_source(0, source);
        settle();
        check(propertyText(filter, "status_info").find("Processing") != std::string::npos, "status is not Processing: " + propertyText(filter, "status_info"));
        const int frames = rate * 4;
        const auto viaObs = push(source, 0, frames);

        spectralforge::obs::ChimeraHost direct;
        std::string error;
        check(direct.load(bundle, error) && direct.prepare(rate, 2, tick), "direct host: " + error);
        settle();
        auto left = guitar(frames, 0), right = guitar(frames, 1);
        for (int offset = 0; offset < frames; offset += tick) {
            float* p[2]{left.data() + offset, right.data() + offset};
            check(direct.process(p, 2, std::min(tick, frames - offset)), "direct process");
        }
        const double obsDiff = std::max(maxDifference(viaObs.channel[0], left, size_t(rate)), maxDifference(viaObs.channel[1], right, size_t(rate)));
        const double processed = maxDifference(viaObs.channel[0], guitar(frames, 0), size_t(rate));
        std::cout << "OBS filter vs direct VST3: max diff " << obsDiff << " (wet-dry " << processed << ")\n";
        check(processed > 1e-3, "OBS output is unprocessed");
        check(obsDiff < 1e-4, "OBS filter output differs from the direct Chimera VST3 output");

        // 2. Input mapping applies live through obs_source_update.
        obs_data_t* update = obs_data_create();
        obs_data_set_int(update, "input_mapping", 1); // left to both
        obs_source_update(filter, update);
        obs_data_release(update);
        drainUi();
        const auto mapped = push(source, frames, rate);
        // Same continuation through the direct host, with L fed to both inputs.
        auto continued = guitar(frames + rate, 0);
        std::vector<float> leftIn(continued.begin() + frames, continued.end()), rightIn = leftIn;
        for (int offset = 0; offset < rate; offset += tick) {
            float* p[2]{leftIn.data() + offset, rightIn.data() + offset};
            check(direct.process(p, 2, std::min(tick, rate - offset)), "direct process (mapped)");
        }
        const double mapDiff = std::max(maxDifference(mapped.channel[0], leftIn, 0), maxDifference(mapped.channel[1], rightIn, 0));
        std::cout << "left-to-both mapping vs direct VST3 fed L/L: max diff " << mapDiff << '\n';
        check(mapDiff < 1e-4, "left-to-both mapping does not match Chimera fed the left channel on both inputs");

        // 2b. OBS skips a disabled filter; re-enabling it must not replay the
        // tail of the audio from before it was disabled.
        push(source, 0, rate);
        const double tail = rms(push(source, 0, rate / 10, true).channel[0]);
        push(source, 0, rate);
        obs_source_set_enabled(filter, false);
        obs_source_set_enabled(filter, true);
        const double afterToggle = rms(push(source, 0, rate / 10, true).channel[0]);
        std::cout << "100 ms of silence: tail rms " << tail << " while enabled, " << afterToggle << " after disable/enable\n";
        check(tail > 1e-4, "Chimera left no tail to measure");
        check(afterToggle < tail * 1e-2, "re-enabled filter replayed the old tail");

        // 3. Scene save/restore keeps the Chimera state.
        obs_data_t* saved = obs_save_source(filter);
        obs_data_t* savedSettings = obs_data_get_obj(saved, "settings");
        const std::string state = obs_data_get_string(savedSettings, "vst3_component_state");
        check(!state.empty() && spectralforge::obs::base64Decode(state).has_value(), "scene does not store Chimera state");
        check(std::string(obs_data_get_string(savedSettings, "vst3_version")) == direct.identity().version, "stored version");
        // A non-default parameter written by another host must survive OBS.
        std::uint32_t paramId = 0;
        std::string title;
        double target = 0;
        int steps = 0;
        for (int i = 0; i < direct.parameterCount(); ++i)
            if (direct.parameterInfo(i, paramId, title, steps) && steps == 0) {
                target = direct.parameter(paramId) < 0.5 ? 0.75 : 0.25;
                if (direct.setParameter(paramId, target) && std::abs(direct.parameter(paramId) - target) < 1e-6) break;
            }
        std::vector<std::uint8_t> component, controller;
        check(direct.getState(component, controller), "direct getState");
        obs_source_t* restored = makeFilter("Restored", bundle, 0, 0, spectralforge::obs::base64Encode(component));
        obs_data_t* resaved = obs_save_source(restored);
        obs_data_t* resavedSettings = obs_data_get_obj(resaved, "settings");
        const auto restoredState = spectralforge::obs::base64Decode(obs_data_get_string(resavedSettings, "vst3_component_state"));
        spectralforge::obs::ChimeraHost verify;
        check(restoredState && verify.load(bundle, error) && verify.setState(*restoredState, {}), "restored state unreadable");
        check(std::abs(verify.parameter(paramId) - target) < 1e-6, "parameter '" + title + "' lost through OBS save/restore");
        std::cout << "scene restore: '" << title << "' kept through OBS settings (" << state.size() << " base64 chars)\n";
        obs_data_release(resavedSettings);
        obs_data_release(resaved);
        obs_data_release(savedSettings);
        obs_data_release(saved);

        // 4. Edits reach the scene settings at once: OBS copies settings without
        // calling save() when it duplicates a source or copies its filters.
        check(setParameter(filter, paramId, 0.31) && near(getParameter(filter, paramId), 0.31), "set_parameter proc");
        obs_source_t* copy = obs_source_duplicate(source, "DI copy", false);
        drainUi();
        obs_source_t* copyFilter = obs_source_get_filter_by_name(copy, "Chimera");
        check(copyFilter != nullptr, "duplicated source lost its Chimera filter");
        check(near(getParameter(copyFilter, paramId), 0.31), "duplicated filter did not get the edited sound");
        check(setParameter(copyFilter, paramId, 0.62) && near(getParameter(filter, paramId), 0.31),
              "instances are not independent");
        bool offUiThread = true;
        std::thread([&] { offUiThread = setParameter(filter, paramId, 0.9); }).join();
        check(!offUiThread && near(getParameter(filter, paramId), 0.31), "set_parameter must refuse non-UI threads");
        std::cout << "duplicate/copy: edited '" << title << "' carried over; instances independent\n";

        // 5. A state pushed through settings (script, obs-websocket, pasted
        // filter settings) is applied to a running filter.
        obs_data_t* preset = obs_data_create();
        obs_data_set_string(preset, "vst3_component_state", spectralforge::obs::base64Encode(component).c_str());
        obs_source_update(filter, preset);
        obs_data_release(preset);
        drainUi();
        check(near(getParameter(filter, paramId), target), "state pushed through settings was not applied");

        // 6. A scene from a newer module: its state is neither applied nor
        // overwritten on save.
        const std::string stored = spectralforge::obs::base64Encode(component);
        obs_source_t* future = makeFilter("Future", bundle, 0, 0, stored, 99);
        check(!near(getParameter(future, paramId), target), "newer-schema state must not be applied");
        obs_data_t* futureSaved = obs_save_source(future);
        obs_data_t* futureSettings = obs_data_get_obj(futureSaved, "settings");
        check(obs_data_get_string(futureSettings, "vst3_component_state") == stored
                  && obs_data_get_int(futureSettings, "chimera_obs_schema") == 99,
              "newer-schema scene state was overwritten");
        obs_data_release(futureSettings);
        obs_data_release(futureSaved);
        std::cout << "settings preset push applied; newer-schema scene kept untouched\n";

        // 6b. The Chimera Setup records a custom VST3 folder for the filter
        // (UTF-8 with BOM, CRLF); an empty bundle setting then finds Chimera.
        const std::string hintFile = dataPath + "/default-vst3-path.txt";
        {
            std::ofstream hint(hintFile, std::ios::binary);
            hint << "\xEF\xBB\xBF" << bundle << "\r\n";
        }
        obs_source_t* hinted = makeFilter("Installed", "");
        const bool hintedLoaded = propertyText(hinted, "status_info").find("Processing") != std::string::npos;
        std::remove(hintFile.c_str());
        check(hintedLoaded, "filter did not use the VST3 path recorded by Setup: " + propertyText(hinted, "status_info"));
        std::cout << "Setup-recorded VST3 path used when the bundle setting is empty\n";

        // 7. Without Chimera: Mute by default, Dry when chosen; never a crash.
        obs_source_t* other = obs_source_create("chimera_test_input", "No Chimera", nullptr, nullptr);
        obs_source_t* missing = makeFilter("Missing", bundle + ".missing");
        obs_source_filter_add(other, missing);
        const auto muted = push(other, 0, tick * 4);
        check(*std::max_element(muted.channel[0].begin(), muted.channel[0].end()) == 0.f, "missing Chimera must mute by default");
        check(propertyText(missing, "status_info").find("not loaded") != std::string::npos, "missing status");
        obs_data_t* dry = obs_data_create();
        obs_data_set_int(dry, "failure_policy", 1);
        obs_source_update(missing, dry);
        obs_data_release(dry);
        drainUi();
        const auto passed = push(other, 0, tick * 4);
        check(maxDifference(passed.channel[0], guitar(tick * 4, 0), 0) == 0.0, "dry policy must pass input unchanged");

        // 7b. Realtime counters through the script interface.
        const Stats main = stats(filter), noChimera = stats(missing);
        std::cout << "realtime counters: " << main.blocks << " blocks, " << main.audioSeconds << " s, load average "
                  << 100 * main.averageLoad << "% peak " << 100 * main.peakLoad << "%, unprocessed " << main.unprocessed << "\n";
        check(main.blocks > 0 && main.audioSeconds > 5 && main.averageLoad > 0 && main.peakLoad >= main.averageLoad,
              "realtime counters of a processing filter");
        check(main.unprocessed == 0 && main.gaps == 0, "continuous processed input has no gaps or unprocessed blocks");
        // A filter that never found Chimera reports no audio: a long recording
        // through it can never pass as processed.
        check(noChimera.blocks == 0 && noChimera.audioSeconds == 0, "a filter without Chimera must not report processed audio");
        check(propertyText(filter, "realtime_info").find("Realtime processing") != std::string::npos, "realtime counters in properties");

        // 7c. Input that resumes after a stop must not start with the old
        // tails; a short timestamp jump only counts as a gap.
        // (The harness clock is shared by every test source: the first push
        // after another source's audio is itself a gap.)
        push(source, 0, rate);
        const Stats beforeGaps = stats(filter);
        const double keptTail = rms(push(source, 0, rate / 10, true, 20000000ull).channel[0]);
        push(source, 0, rate);
        const double afterStop = rms(push(source, 0, rate / 10, true, 2000000000ull).channel[0]);
        const Stats gapped = stats(filter);
        std::cout << "100 ms of silence: tail rms " << keptTail << " after a 20 ms gap, " << afterStop << " after a 2 s stop\n";
        check(keptTail > 1e-4, "a 20 ms gap must not reset Chimera");
        check(afterStop < keptTail * 1e-2, "input resumed after a stop replayed the old tail");
        check(gapped.gaps - beforeGaps.gaps == 2 && gapped.restarts - beforeGaps.restarts == 1, "gap counters");

        // 7d. Long session: random OBS block sizes (1-1024 frames), live input
        // mapping changes, no drift or state build-up. The same passage after a
        // stop sounds the same at the start and at the end of the session.
        obs_source_t* soakSource = obs_source_create("chimera_test_input", "Long session", nullptr, nullptr);
        obs_source_t* soak = makeFilter("Soak", bundle);
        obs_source_filter_add(soakSource, soak);
        settle();
        const int passage = rate * 2;
        const auto passageL = guitar(passage, 0), passageR = guitar(passage, 1);
        const std::vector<int> whole(size_t(passage / tick), tick);
        const auto first = pushBlocks(soakSource, passageL, passageR, whole, 2000000000ull);
        const char* soakEnv = std::getenv("CHIMERA_OBS_SOAK_SECONDS");
        const int soakSeconds = soakEnv != nullptr ? std::max(1, std::atoi(soakEnv)) : 120;
        double memoryBefore = 0;
        const auto wallStart = std::chrono::steady_clock::now();
        uint32_t lcg = 12345;
        long long soakBlocks = 0;
        for (int done = 0; done < soakSeconds; done += 10) {
            // Memory is compared from the end of the first 10 s, once every
            // buffer of the session has been allocated once.
            if (done == 10) memoryBefore = residentMiB();
            const auto chunkL = guitar(rate * 10, 0, rate * done), chunkR = guitar(rate * 10, 1, rate * done);
            std::vector<int> sizes;
            for (int remaining = rate * 10; remaining > 0;) {
                lcg = lcg * 1664525u + 1013904223u;
                const int size = std::min(remaining, 1 + int(lcg >> 22)); // 1..1024
                sizes.push_back(size);
                remaining -= size;
            }
            soakBlocks += (long long)sizes.size();
            obs_data_t* mapping = obs_data_create();
            obs_data_set_int(mapping, "input_mapping", (done / 10) % 4);
            obs_source_update(soak, mapping);
            obs_data_release(mapping);
            drainUi();
            const auto out = pushBlocks(soakSource, chunkL, chunkR, sizes);
            check(finite(out.channel[0], 4.f) && finite(out.channel[1], 4.f), "long session output is not finite and bounded");
        }
        const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
        const double memoryGrowth = memoryBefore > 0 ? residentMiB() - memoryBefore : 0.0;
        obs_data_t* stereo = obs_data_create();
        obs_data_set_int(stereo, "input_mapping", 0);
        obs_source_update(soak, stereo);
        obs_data_release(stereo);
        drainUi();
        const auto last = pushBlocks(soakSource, passageL, passageR, whole, 2000000000ull);
        const double drift = std::max(maxDifference(first.channel[0], last.channel[0], 0), maxDifference(first.channel[1], last.channel[1], 0));
        const Stats soaked = stats(soak);
        std::cout << "long session: " << soakSeconds << " s of audio in " << soakBlocks << " random blocks, " << wall << " s wall time; load average "
                  << 100 * soaked.averageLoad << "% peak " << 100 * soaked.peakLoad << "% (20 ms windows), " << soaked.overruns << " windows over real time; "
                  << "resident memory +" << memoryGrowth << " MiB; same passage start vs end: max diff " << drift << "\n";
        check(soaked.blocks == soakBlocks + 2 * (long long)whole.size(), "every block of the session counted");
        // The first passage is this filter's first audio; the second follows a stop.
        check(soaked.unprocessed == 0 && soaked.restarts == 1 && soaked.gaps == 1,
              "long session: " + std::to_string(soaked.unprocessed) + " unprocessed blocks, " + std::to_string(soaked.gaps) + " gaps, "
                  + std::to_string(soaked.restarts) + " restarts");
        check(drift < 1e-4, "the same passage sounds different at the end of the session (drift or state build-up)");
        check(memoryGrowth < 8.0, "resident memory grew during the long session");

        // 7e. Other OBS sample rates and speaker layouts. OBS applies these on
        // restart, so each format gets a new source and filter (libobs does not
        // re-convert an existing source's audio after a reset). Chimera
        // processes the first two channels; the others pass unchanged.
        const size_t unloadsBeforeFormats = logCount("unloaded Chimera");
        obs_source_filter_remove(soakSource, soak);
        obs_source_release(soak);
        obs_source_release(soakSource);
        for (const auto& format : {std::make_tuple(44100, SPEAKERS_MONO, 1), std::make_tuple(48000, SPEAKERS_5POINT1, 6),
                                   std::make_tuple(48000, SPEAKERS_STEREO, 2)}) {
            const int newRate = std::get<0>(format), channels = std::get<2>(format);
            obs_audio_info reset{static_cast<uint32_t>(newRate), std::get<1>(format)};
            check(obs_reset_audio(&reset), "obs_reset_audio to a new format");
            obs_source_t* formatSource = obs_source_create("chimera_test_input", "Format", nullptr, nullptr);
            obs_source_t* formatFilter = makeFilter("Format filter", bundle);
            obs_source_filter_add(formatSource, formatFilter);
            settle();
            const std::string status = propertyText(formatFilter, "status_info");
            check(status.find("Processing") != std::string::npos, "filter is not prepared for the OBS format: " + status);
            const auto changed = pushFormat(formatSource, newRate, std::get<1>(format), channels, newRate);
            obs_source_filter_remove(formatSource, formatFilter);
            obs_source_release(formatFilter);
            obs_source_release(formatSource);
            const size_t settled = size_t(tick) * 4;
            double processedChange = 0, passedChange = 0;
            for (int c = 0; c < channels; ++c) {
                std::vector<float> tailPart(changed.channel[size_t(c)].begin() + long(settled), changed.channel[size_t(c)].end());
                check(finite(tailPart, 4.f), "non-finite output after a format change");
                (c < 2 ? processedChange : passedChange) = std::max(c < 2 ? processedChange : passedChange, rms(tailPart));
            }
            std::cout << "OBS format " << newRate << " Hz, " << channels << " ch: Chimera channels changed (rms " << processedChange
                      << "), other channels changed rms " << passedChange << "\n";
            check(processedChange > 1e-3, "Chimera did not process after the format change");
            check(passedChange == 0.0, "channels beyond the first two must pass unchanged");
        }

        // The long-session and format filters are gone before teardown is counted.
        for (int i = 0; i < 500 && logCount("unloaded Chimera") < unloadsBeforeFormats + 4; ++i) {
            drainUi();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(logCount("unloaded Chimera") == unloadsBeforeFormats + 4, "long-session and format filters were not released");

        // 8. Teardown: OBS destroys filters on its destroy thread; the plugin
        // must be unloaded on the UI thread, without blocking either.
        const size_t unloadsBefore = logCount("unloaded Chimera");
        for (obs_source_t* f : {filter, restored, missing, future, hinted}) {
            obs_source_t* parent = f == missing ? other : source;
            if (f == filter || f == missing) obs_source_filter_remove(parent, f);
            obs_source_release(f);
        }
        obs_source_release(copyFilter); // get_filter_by_name's reference; the copy owns it
        obs_set_output_source(0, nullptr);
        obs_source_release(source);
        obs_source_release(copy);
        obs_source_release(other);
        // obs_wait_for_destroy_queue() returns at once without a video thread,
        // so poll the destroy thread's hand-offs instead.
        for (int i = 0; i < 500 && logCount("unloaded Chimera") < unloadsBefore + 6; ++i) {
            drainUi();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(logCount("unloaded Chimera") == unloadsBefore + 6,
              "every filter must release its plugin on the UI thread (released " + std::to_string(logCount("unloaded Chimera") - unloadsBefore) + " of 6)");

        obs_shutdown();
        check(bnum_allocs() == 0, "libobs allocations leaked: " + std::to_string(bnum_allocs()));
        std::cout << "PASS OBS filter: libobs module load, OBS output equals direct VST3, live input mapping, tail reset on re-enable, scene restore, "
                     "duplicate/copy with edited state, independent instances, settings preset push, newer-schema guard, Setup-recorded VST3 path, "
                     "mute/dry without Chimera, realtime counters, tail reset after an input stop, long session without drift, "
                     "OBS rate/layout changes, UI-thread teardown\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}

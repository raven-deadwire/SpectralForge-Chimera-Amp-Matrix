// SPDX-License-Identifier: GPL-2.0-or-later
// Headless libobs integration test. Loads the built module through libobs's
// own module loader, attaches the Chimera filter to an audio source and
// checks, through OBS's post-filter audio capture:
//   - OBS output equals the same input run directly through the Chimera VST3
//   - input mapping, mute/dry policy without Chimera, scene save/restore
//   - destroy on the OBS destroy thread hands teardown to the UI thread
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
#include <cstdio>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
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

std::vector<float> guitar(int frames, int channel) {
    std::vector<float> out(static_cast<size_t>(frames));
    const double notes[] = {82.41, 110.0, 146.83, 98.0};
    for (int n = 0; n < frames; ++n) {
        const double t = (n % 24000) / double(rate), f = notes[(n / 24000) % 4];
        double v = 0;
        for (int h = 1; h <= 6; ++h) v += std::sin(2 * pi * f * h * t + channel * 0.1) / h;
        out[static_cast<size_t>(n)] = static_cast<float>(0.25 * v * std::exp(-3.0 * t));
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

// Pushes `frames` of the stereo test signal (starting at `start`) through the
// source; returns what OBS delivered after the filter chain.
Capture push(obs_source_t* source, int start, int frames) {
    Capture captured;
    obs_source_add_audio_capture_callback(source, captureAudio, &captured);
    const auto left = guitar(start + frames, 0), right = guitar(start + frames, 1);
    static uint64_t timestamp = 1000000000ull;
    for (int offset = 0; offset < frames; offset += tick) {
        obs_source_audio audio{};
        audio.data[0] = reinterpret_cast<const uint8_t*>(left.data() + start + offset);
        audio.data[1] = reinterpret_cast<const uint8_t*>(right.data() + start + offset);
        audio.frames = static_cast<uint32_t>(std::min(tick, frames - offset));
        audio.speakers = SPEAKERS_STEREO;
        audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
        audio.samples_per_sec = rate;
        audio.timestamp = timestamp;
        timestamp += uint64_t(audio.frames) * 1000000000ull / rate;
        obs_source_output_audio(source, &audio);
    }
    obs_source_remove_audio_capture_callback(source, captureAudio, &captured);
    check(captured.channel[0].size() == size_t(frames), "OBS did not deliver every frame through the filter");
    return captured;
}

double maxDifference(const std::vector<float>& a, const std::vector<float>& b, size_t from) {
    double worst = 0;
    for (size_t n = from; n < std::min(a.size(), b.size()); ++n) worst = std::max(worst, std::abs(double(a[n]) - b[n]));
    return worst;
}

obs_source_t* makeFilter(const std::string& name, const std::string& bundle, long long mapping = 0, long long policy = 0,
                         const std::string& componentState = {}) {
    obs_data_t* settings = obs_data_create();
    obs_data_set_string(settings, "bundle_path", bundle.c_str());
    obs_data_set_int(settings, "input_mapping", mapping);
    obs_data_set_int(settings, "failure_policy", policy);
    if (!componentState.empty()) obs_data_set_string(settings, "vst3_component_state", componentState.c_str());
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

        // 4. Without Chimera: Mute by default, Dry when chosen; never a crash.
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

        // 5. Teardown: OBS destroys filters on its destroy thread; the plugin
        // must be unloaded on the UI thread, without blocking either.
        const size_t unloadsBefore = logCount("unloaded Chimera");
        for (obs_source_t* f : {filter, restored, missing}) {
            obs_source_t* parent = f == missing ? other : source;
            if (f != restored) obs_source_filter_remove(parent, f);
            obs_source_release(f);
        }
        obs_set_output_source(0, nullptr);
        obs_source_release(source);
        obs_source_release(other);
        // obs_wait_for_destroy_queue() returns at once without a video thread,
        // so poll the destroy thread's hand-offs instead.
        for (int i = 0; i < 500 && logCount("unloaded Chimera") < unloadsBefore + 3; ++i) {
            drainUi();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(logCount("unloaded Chimera") == unloadsBefore + 3,
              "every filter must release its plugin on the UI thread (released " + std::to_string(logCount("unloaded Chimera") - unloadsBefore) + " of 3)");

        obs_shutdown();
        check(bnum_allocs() == 0, "libobs allocations leaked: " + std::to_string(bnum_allocs()));
        std::cout << "PASS OBS filter: libobs module load, OBS output equals direct VST3, live input mapping, scene restore, "
                     "mute/dry without Chimera, UI-thread teardown\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}

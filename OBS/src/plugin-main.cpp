// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
//
// SpectralForge Chimera audio filter for OBS Studio (Issue #131, structure 2).
// This GPL module hosts the separately installed, proprietary Chimera VST3;
// it contains no Chimera DSP. See OBS/README.md.
#include "audio-bridge.hpp"
#include "base64.hpp"
#include "chimera-host.hpp"
#include "editor-window.hpp"

#include <obs-module.h>
#include <media-io/audio-io.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("chimera-obs", "en-US")

MODULE_EXPORT const char* obs_module_description(void) {
    return "SpectralForge Chimera audio filter (hosts the installed Chimera VST3)";
}

namespace {
using namespace spectralforge::obs;

constexpr const char* filterId = "spectralforge_chimera_filter";
constexpr const char* keyBundle = "bundle_path";
constexpr const char* keyMapping = "input_mapping";
constexpr const char* keyPolicy = "failure_policy";
constexpr const char* keyComponent = "vst3_component_state";
constexpr const char* keyController = "vst3_controller_state";
constexpr const char* keyVersion = "vst3_version";
// Scene settings layout written by this module. Scenes saved by a newer module
// are left untouched: their state is neither applied nor overwritten.
constexpr const char* keySchema = "chimera_obs_schema";
constexpr long long schemaVersion = 1;

struct Filter {
    obs_source_t* source{};
    obs_weak_source_t* weak{};
    ChimeraHost host;
    AudioBridge<ChimeraHost> bridge{host};
    EditorWindow* editor{};
    std::string loadedPath, loadError;
    // UI thread. The component state (base64) last applied from or written to
    // the scene settings, and the edit count it reflects.
    std::string syncedComponent;
    std::uint64_t syncedEdits{};
    bool newerSchema{};
    // The filter outlives destroy() until every queued UI task has run.
    std::atomic<int> references{1};
    std::atomic<bool> destroyed{false}, reprepareQueued{false};
    ~Filter() { obs_weak_source_release(weak); }
};

void release(Filter* f) {
    if (f->references.fetch_sub(1) == 1) delete f;
}

// Plugin lifecycle (load/unload/editor) stays on the OBS UI thread. Never block
// on it from another thread: OBS's UI thread can itself be waiting for the
// destroy queue during shutdown.
void runOnUi(obs_task_t task, Filter* f) {
    f->references.fetch_add(1);
    if (obs_in_task_thread(OBS_TASK_UI)) task(f);
    else obs_queue_task(OBS_TASK_UI, task, f, false);
}

// UI tasks may run while OBS's destroy thread frees the source. A task takes a
// strong reference first and skips its work once the source is going away.
obs_source_t* acquire(Filter* f) {
    return f->destroyed.load() ? nullptr : obs_weak_source_get_source(f->weak);
}

void prepareFromObs(Filter* f) {
    audio_t* audio = obs_get_audio();
    if (audio == nullptr || !f->host.loaded()) { f->bridge.suspend(); return; }
    const double rate = audio_output_get_sample_rate(audio);
    const int channels = static_cast<int>(audio_output_get_channels(audio));
    if (!f->bridge.prepare(rate, channels, AUDIO_OUTPUT_FRAMES))
        blog(LOG_WARNING, "[chimera-obs] '%s': Chimera could not be prepared for %.0f Hz, %d channels",
             obs_source_get_name(f->source), rate, channels);
}

std::string resolveBundle(obs_data_t* settings) {
    const char* chosen = obs_data_get_string(settings, keyBundle);
    if (chosen != nullptr && *chosen != '\0') return chosen;
    std::error_code ignored;
    for (const auto& path : defaultBundlePaths())
        if (std::filesystem::exists(std::filesystem::u8path(path), ignored)) return path;
    return {};
}

// UI thread. Applies the state in the settings when it differs from what this
// filter last applied or wrote: scene load, a pasted/duplicated filter, or a
// preset pushed by a script or obs-websocket.
void restoreState(Filter* f, obs_data_t* settings) {
    f->newerSchema = obs_data_get_int(settings, keySchema) > schemaVersion;
    if (f->newerSchema) {
        blog(LOG_WARNING, "[chimera-obs] '%s': scene was saved by a newer Chimera OBS module; its Chimera state is "
             "kept but not applied", obs_source_get_name(f->source));
        return;
    }
    const std::string encoded = obs_data_get_string(settings, keyComponent);
    if (encoded.empty() || encoded == f->syncedComponent) return;
    const auto component = base64Decode(encoded);
    const auto controller = base64Decode(obs_data_get_string(settings, keyController));
    if (component && !component->empty()
        && f->host.setState(*component, controller.value_or(std::vector<std::uint8_t>{}))) {
        f->syncedComponent = encoded;
        f->syncedEdits = f->host.editGeneration();
        return;
    }
    blog(LOG_WARNING, "[chimera-obs] '%s': stored Chimera state was rejected; the current sound is kept",
         obs_source_get_name(f->source));
}

// Writes Chimera's current state into the scene settings. OBS copies a
// filter's settings without calling save() when a source is duplicated or
// filters are copied, so edits must reach the settings as they happen.
bool writeState(Filter* f, obs_data_t* settings) {
    std::vector<std::uint8_t> component, controller;
    if (f->newerSchema || !f->host.loaded() || !f->host.getState(component, controller)) return false;
    f->syncedComponent = base64Encode(component);
    f->syncedEdits = f->host.editGeneration();
    obs_data_set_string(settings, keyComponent, f->syncedComponent.c_str());
    obs_data_set_string(settings, keyController, base64Encode(controller).c_str());
    obs_data_set_string(settings, keyVersion, f->host.identity().version.c_str());
    obs_data_set_int(settings, keySchema, schemaVersion);
    return true;
}

// UI thread, after an edit: cheap when nothing changed since the last sync.
void syncState(Filter* f, obs_source_t* source) {
    if (!f->host.loaded() || f->host.editGeneration() == f->syncedEdits) return;
    obs_data_t* settings = obs_source_get_settings(source);
    writeState(f, settings);
    obs_data_release(settings);
}

void editorChanged(void* data) {
    auto* f = static_cast<Filter*>(data);
    if (obs_source_t* source = acquire(f)) {
        syncState(f, source);
        obs_source_release(source);
    }
}

// UI thread. Reloads only when the bundle path changes; restores stored state
// once per load, before the plugin is activated.
void reload(Filter* f, obs_data_t* settings) {
    const std::string path = resolveBundle(settings);
    if (f->host.loaded() && path == f->loadedPath) {
        restoreState(f, settings);
        // A latched engine failure is retried when the user changes a setting.
        if (f->bridge.status() == BridgeStatus::EngineFailed) prepareFromObs(f);
        return;
    }

    f->bridge.suspend();
    closeEditor(f->editor);
    f->loadedPath = path;
    f->loadError.clear();
    f->syncedComponent.clear();
    if (path.empty()) {
        f->host.unload();
        f->loadError = obs_module_text("Error.NotInstalled");
    } else if (f->host.load(path, f->loadError)) {
        const auto& id = f->host.identity();
        blog(LOG_INFO, "[chimera-obs] '%s': loaded %s %s (%s, VST3 SDK %s) from %s", obs_source_get_name(f->source),
             id.name.c_str(), id.version.c_str(), id.vendor.c_str(), id.sdkVersion.c_str(), path.c_str());
        restoreState(f, settings);
    }
    if (!f->loadError.empty())
        blog(LOG_WARNING, "[chimera-obs] '%s': %s (%s)", obs_source_get_name(f->source), f->loadError.c_str(), path.c_str());
    prepareFromObs(f);
}

void reloadTask(void* data) {
    auto* f = static_cast<Filter*>(data);
    if (obs_source_t* source = acquire(f)) {
        obs_data_t* settings = obs_source_get_settings(source);
        reload(f, settings);
        obs_data_release(settings);
        obs_source_release(source);
    }
    release(f);
}

// Any thread (OBS UI, scene loading, obs-websocket). Mapping and policy apply
// at once; loading the plugin always happens on the UI thread.
void configure(Filter* f, obs_data_t* settings) {
    // Out-of-range values from a hand-edited scene fall back to the defaults.
    const long long mapping = obs_data_get_int(settings, keyMapping);
    f->bridge.setInputMapping(mapping >= 0 && mapping <= static_cast<long long>(InputMapping::SumToBoth)
                                  ? static_cast<InputMapping>(mapping) : InputMapping::Stereo);
    f->bridge.setFailurePolicy(obs_data_get_int(settings, keyPolicy) == static_cast<long long>(FailurePolicy::Dry)
                                   ? FailurePolicy::Dry : FailurePolicy::Mute);
    runOnUi(reloadTask, f);
}

void teardownTask(void* data) {
    auto* f = static_cast<Filter*>(data);
    closeEditor(f->editor);
    f->host.unload();
    // f->source is already gone here; OBS frees it after destroy() returns.
    blog(LOG_INFO, "[chimera-obs] unloaded Chimera filter instance");
    release(f); // this task's reference
    release(f); // the reference create() returned to OBS
}

void reprepareTask(void* data) {
    auto* f = static_cast<Filter*>(data);
    if (obs_source_t* source = acquire(f)) {
        prepareFromObs(f);
        obs_source_release(source);
    }
    f->reprepareQueued.store(false);
    release(f);
}

// Script/plugin access through the source's proc handler (OBS UI thread only;
// other threads get ok = false). A changed value reaches the scene settings.
//   void chimera_set_parameter(in int id, in float value, out bool ok)
//   void chimera_get_parameter(in int id, out float value, out bool ok)
void procSetParameter(void* data, calldata_t* call) {
    auto* f = static_cast<Filter*>(data);
    bool ok = false;
    if (obs_in_task_thread(OBS_TASK_UI) && !f->destroyed.load() && f->host.loaded()) {
        const auto id = static_cast<std::uint32_t>(calldata_int(call, "id"));
        ok = f->host.setParameter(id, std::clamp(calldata_float(call, "value"), 0.0, 1.0));
        if (ok) {
            obs_data_t* settings = obs_source_get_settings(f->source);
            writeState(f, settings);
            obs_data_release(settings);
        }
    }
    calldata_set_bool(call, "ok", ok);
}

void procGetParameter(void* data, calldata_t* call) {
    auto* f = static_cast<Filter*>(data);
    const bool ok = obs_in_task_thread(OBS_TASK_UI) && !f->destroyed.load() && f->host.loaded();
    calldata_set_float(call, "value", ok ? f->host.parameter(static_cast<std::uint32_t>(calldata_int(call, "id"))) : 0.0);
    calldata_set_bool(call, "ok", ok);
}

// OBS skips a disabled filter entirely, so Chimera would still hold the delay,
// reverb and cabinet tails of the audio from before it was disabled. Any
// thread; the reset itself happens on the audio thread's next block.
void enabledChanged(void* data, calldata_t* call) {
    if (calldata_bool(call, "enabled")) static_cast<Filter*>(data)->host.requestReset();
}

const char* getName(void*) { return obs_module_text("Chimera"); }

void* create(obs_data_t* settings, obs_source_t* source) {
    auto* f = new Filter();
    f->source = source;
    f->weak = obs_source_get_weak_source(source);
    proc_handler_t* procs = obs_source_get_proc_handler(source);
    proc_handler_add(procs, "void chimera_set_parameter(in int id, in float value, out bool ok)", procSetParameter, f);
    proc_handler_add(procs, "void chimera_get_parameter(in int id, out float value, out bool ok)", procGetParameter, f);
    signal_handler_connect(obs_source_get_signal_handler(source), "enable", enabledChanged, f);
    configure(f, settings);
    return f;
}

void destroy(void* data) {
    auto* f = static_cast<Filter*>(data);
    f->destroyed.store(true);
    signal_handler_disconnect(obs_source_get_signal_handler(f->source), "enable", enabledChanged, f);
    f->bridge.suspend();
    runOnUi(teardownTask, f);
}

void update(void* data, obs_data_t* settings) { configure(static_cast<Filter*>(data), settings); }

void save(void* data, obs_data_t* settings) {
    // Keeps the last stored state when Chimera is missing or the scene is from
    // a newer module, so opening it here does not erase the user's sound.
    writeState(static_cast<Filter*>(data), settings);
}

void getDefaults(obs_data_t* settings) {
    obs_data_set_default_string(settings, keyBundle, "");
    obs_data_set_default_int(settings, keyMapping, static_cast<long long>(InputMapping::Stereo));
    obs_data_set_default_int(settings, keyPolicy, static_cast<long long>(FailurePolicy::Mute));
}

bool openEditorClicked(obs_properties_t*, obs_property_t*, void* data) {
    auto* f = static_cast<Filter*>(data);
    std::string error;
    if (!f->host.loaded()) error = f->loadError;
    else if (openEditor(f->editor, f->host, obs_source_get_name(f->source), editorChanged, f, error)) return false;
    blog(LOG_WARNING, "[chimera-obs] '%s': editor: %s", obs_source_get_name(f->source), error.c_str());
    return false;
}

const char* statusText(const Filter* f) {
    if (!f->host.loaded()) return obs_module_text("Status.NotLoaded");
    switch (f->bridge.status()) {
    case BridgeStatus::Processing: return obs_module_text("Status.Processing");
    case BridgeStatus::FormatChanged: return obs_module_text("Status.FormatChanged");
    case BridgeStatus::EngineFailed: return obs_module_text("Status.EngineFailed");
    case BridgeStatus::Unprepared: break;
    }
    return obs_module_text("Status.Unprepared");
}

obs_properties_t* getProperties(void* data) {
    auto* f = static_cast<Filter*>(data);
    obs_properties_t* props = obs_properties_create();
    obs_properties_add_path(props, keyBundle, obs_module_text("BundlePath"), OBS_PATH_DIRECTORY, nullptr, nullptr);

    obs_property_t* mapping = obs_properties_add_list(props, keyMapping, obs_module_text("InputMapping"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mapping, obs_module_text("InputMapping.Stereo"), static_cast<long long>(InputMapping::Stereo));
    obs_property_list_add_int(mapping, obs_module_text("InputMapping.Left"), static_cast<long long>(InputMapping::LeftToBoth));
    obs_property_list_add_int(mapping, obs_module_text("InputMapping.Right"), static_cast<long long>(InputMapping::RightToBoth));
    obs_property_list_add_int(mapping, obs_module_text("InputMapping.Sum"), static_cast<long long>(InputMapping::SumToBoth));

    obs_property_t* policy = obs_properties_add_list(props, keyPolicy, obs_module_text("FailurePolicy"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(policy, obs_module_text("FailurePolicy.Mute"), static_cast<long long>(FailurePolicy::Mute));
    obs_property_list_add_int(policy, obs_module_text("FailurePolicy.Dry"), static_cast<long long>(FailurePolicy::Dry));

    obs_property_t* editor = obs_properties_add_button2(props, "open_editor", obs_module_text("OpenEditor"), openEditorClicked, f);
    obs_property_set_enabled(editor, editorSupported() && f->host.loaded());

    // Read-only diagnostics. OBS has no plugin delay compensation: this delay
    // is not corrected automatically (see the add-on plan, stage D).
    std::string info = std::string(obs_module_text("Status")) + ": " + statusText(f);
    if (f->host.loaded()) {
        const auto& id = f->host.identity();
        const int samples = f->bridge.latencySamples();
        audio_t* audio = obs_get_audio();
        const double rate = audio != nullptr ? audio_output_get_sample_rate(audio) : 0.0;
        info += "\n" + id.name + " " + id.version + "\n" + obs_module_text("Latency") + ": " + std::to_string(samples) + " samples ("
            + std::to_string(AudioBridge<ChimeraHost>::latencyNanoseconds(samples, rate) / 1000) + " us)";
    } else if (!f->loadError.empty()) {
        info += "\n" + f->loadError;
    }
    obs_property_t* status = obs_properties_add_text(props, "status_info", info.c_str(), OBS_TEXT_INFO);
    obs_property_text_set_info_word_wrap(status, true);
    return props;
}

obs_audio_data* filterAudio(void* data, obs_audio_data* audio) {
    auto* f = static_cast<Filter*>(data);
    audio_t* output = obs_get_audio();
    const int channels = output != nullptr ? static_cast<int>(audio_output_get_channels(output)) : 0;
    const double rate = output != nullptr ? audio_output_get_sample_rate(output) : 0.0;
    float* planes[MAX_AV_PLANES]{};
    for (int c = 0; c < channels && c < MAX_AV_PLANES; ++c) planes[c] = reinterpret_cast<float*>(audio->data[c]);
    f->bridge.process(planes, channels, static_cast<int>(audio->frames), rate);
    // OBS audio was reset to a new rate/layout: re-prepare off this thread.
    // Queuing allocates, but only once per format change, never per tick.
    if (f->bridge.needsPrepare() && !f->reprepareQueued.exchange(true)) runOnUi(reprepareTask, f);
    return audio;
}
}

bool obs_module_load(void) {
    obs_source_info info{};
    info.id = filterId;
    info.type = OBS_SOURCE_TYPE_FILTER;
    info.output_flags = OBS_SOURCE_AUDIO;
    info.get_name = getName;
    info.create = create;
    info.destroy = destroy;
    info.update = update;
    info.save = save;
    info.get_defaults = getDefaults;
    info.get_properties = getProperties;
    info.filter_audio = filterAudio;
    obs_register_source(&info);
    blog(LOG_INFO, "[chimera-obs] registered %s", filterId);
    return true;
}

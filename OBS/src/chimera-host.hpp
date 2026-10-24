// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Steinberg { class IPlugView; }

namespace spectralforge::obs {
// The installed product identity this module accepts. The add-on is not a
// general VST3 host: any other bundle, or a Chimera class from another vendor,
// is rejected before a component is created.
inline constexpr const char* chimeraClassName = "SpectralForge Chimera";
inline constexpr const char* chimeraVendor = "RavenForge Luthier Intelligence";

struct ChimeraIdentity {
    std::string bundlePath, name, vendor, version, sdkVersion;
};

// Hosts the separately installed SpectralForge Chimera VST3 through the MIT
// Steinberg VST3 SDK (no JUCE in this module). Satisfies the AudioBridge
// engine contract. load/unload/prepare/state/editor run on the OBS UI thread;
// process() runs on the OBS audio thread and only while the bridge holds it.
class ChimeraHost {
public:
    ChimeraHost();
    ~ChimeraHost();
    ChimeraHost(const ChimeraHost&) = delete;
    ChimeraHost& operator=(const ChimeraHost&) = delete;

    bool load(const std::string& bundlePath, std::string& error);
    void unload() noexcept;
    bool loaded() const noexcept;
    const ChimeraIdentity& identity() const noexcept;

    bool prepare(double sampleRate, int channels, int maximumFrames);
    bool process(float* const* data, int channels, int frames) noexcept;
    int latency() const noexcept;

    // Component (processor) and controller state, exactly as a DAW stores them,
    // so the same .chimera-compatible VST3 state moves between hosts.
    bool getState(std::vector<std::uint8_t>& component, std::vector<std::uint8_t>& controller) const;
    bool setState(const std::vector<std::uint8_t>& component, const std::vector<std::uint8_t>& controller);

    // Controller-side parameter access (UI thread). Chimera's JUCE controller
    // shares its processor's parameters, so a value set here is what the
    // processor uses and what getState() stores.
    int parameterCount() const noexcept;
    // stepCount 0 means continuous; discrete values are quantized when stored.
    bool parameterInfo(int index, std::uint32_t& id, std::string& title, int& stepCount) const;
    double parameter(std::uint32_t id) const noexcept;
    bool setParameter(std::uint32_t id, double normalized) noexcept;

    // Increments whenever Chimera reports an edit (its editor's gestures or a
    // parameter/state reload). Lets the UI thread sync scene settings cheaply.
    std::uint64_t editGeneration() const noexcept;

    // A new editor view (caller releases), or nullptr when unavailable.
    Steinberg::IPlugView* createEditorView() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Standard per-OS VST3 install locations of the product bundle, in search order.
std::vector<std::string> defaultBundlePaths();
}

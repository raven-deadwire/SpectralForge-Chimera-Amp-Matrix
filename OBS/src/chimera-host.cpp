// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#include "chimera-host.hpp"
#include "capture-host-application.hpp"

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/connectionproxy.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace spectralforge::obs {
namespace {
// Chimera's JUCE editor writes its own processor parameters; edits reported
// here need no forwarding, only counting so scene settings can follow them.
// Latency changes are re-read by the next prepare().
class ComponentHandler : public U::Implements<U::Directly<IComponentHandler>> {
public:
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID, ParamValue) override { edits.fetch_add(1); return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { edits.fetch_add(1); return kResultOk; }
    tresult PLUGIN_API restartComponent(int32 flags) override {
        restartFlags.fetch_or(flags);
        if (flags & (kParamValuesChanged | kReloadComponent)) edits.fetch_add(1);
        return kResultOk;
    }
    std::atomic<int32> restartFlags{0};
    std::atomic<std::uint64_t> edits{0};
};

// One loaded module per bundle path, shared by every filter instance.
VST3::Hosting::Module::Ptr loadModule(const std::string& path, std::string& error) {
    static std::mutex lock;
    static std::map<std::string, std::weak_ptr<VST3::Hosting::Module>> modules;
    std::lock_guard<std::mutex> guard(lock);
    if (auto existing = modules[path].lock()) return existing;
    auto module = VST3::Hosting::Module::create(path, error);
    if (module) modules[path] = module;
    return module;
}

std::vector<std::uint8_t> drain(MemoryStream& stream) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(stream.getData());
    return {data, data + stream.getSize()};
}

bool fill(MemoryStream& stream, const std::vector<std::uint8_t>& bytes) {
    int32 written = 0;
    if (!bytes.empty() && stream.write(const_cast<std::uint8_t*>(bytes.data()), static_cast<int32>(bytes.size()), &written) != kResultOk)
        return false;
    stream.seek(0, IBStream::kIBSeekSet, nullptr);
    return written == static_cast<int32>(bytes.size());
}
}

struct ChimeraHost::Impl {
    VST3::Hosting::Module::Ptr module;
    IPtr<CaptureHostApplication> hostApplication;
    IPtr<ComponentHandler> handler = owned(new ComponentHandler());
    IPtr<IComponent> component;
    IPtr<IEditController> controller;
    IPtr<IAudioProcessor> processor;
    IPtr<ConnectionProxy> componentConnection, controllerConnection;
    bool separateController{}, active{};
    ChimeraIdentity identity;

    // Allocated by prepare(); process() only re-points and copies.
    std::vector<std::vector<float>> output;
    std::vector<AudioBusBuffers> inputBuses, outputBuses;
    float* inputPointers[2]{};
    float* outputPointers[2]{};
    ProcessContext context{};
    ProcessData data{};
    int channels{}, capacity{};
    std::atomic<bool> resetRequested{false};

    void deactivate() noexcept {
        if (!active) return;
        processor->setProcessing(false);
        component->setActive(false);
        active = false;
    }
};

ChimeraHost::ChimeraHost() : impl(std::make_unique<Impl>()) {}
ChimeraHost::~ChimeraHost() { unload(); }

bool ChimeraHost::load(const std::string& bundlePath, std::string& error) {
    unload();
    auto& m = *impl;
    // A fresh acknowledgement per load: a previous compatible bundle must not
    // authorize a different or older plugin on reload.
    m.hostApplication = owned(new CaptureHostApplication());
    m.module = loadModule(bundlePath, error);
    if (!m.module) { if (error.empty()) error = "VST3 module could not be loaded"; return false; }
    const auto& factory = m.module->getFactory();
    factory.setHostContext(static_cast<HostApplication*>(m.hostApplication.get()));
    const VST3::Hosting::ClassInfo* found = nullptr;
    const auto classes = factory.classInfos();
    for (const auto& info : classes)
        if (info.category() == kVstAudioEffectClass && info.name() == chimeraClassName && info.vendor() == chimeraVendor) found = &info;
    if (found == nullptr) { error = "bundle does not contain SpectralForge Chimera by RavenForge Luthier Intelligence"; unload(); return false; }
    m.identity = {bundlePath, found->name(), found->vendor(), found->version(), found->sdkVersion()};

    m.component = factory.createInstance<IComponent>(found->ID());
    if (!m.component || m.component->initialize(static_cast<HostApplication*>(m.hostApplication.get())) != kResultOk) {
        error = "Chimera component failed to initialize"; m.component = nullptr; unload(); return false;
    }
    m.processor = U::cast<IAudioProcessor>(m.component);
    TUID controllerId{};
    if (m.component->getControllerClassId(controllerId) == kResultTrue) {
        m.controller = factory.createInstance<IEditController>(VST3::UID::fromTUID(controllerId));
        m.separateController = m.controller != nullptr;
        if (m.controller && m.controller->initialize(static_cast<HostApplication*>(m.hostApplication.get())) != kResultOk) m.controller = nullptr;
    }
    if (!m.controller) { m.controller = U::cast<IEditController>(m.component); m.separateController = false; }
    if (!m.processor || !m.controller) { error = "Chimera does not expose an audio processor and controller"; unload(); return false; }
    m.controller->setComponentHandler(m.handler);

    if (m.separateController) {
        auto componentPoint = U::cast<IConnectionPoint>(m.component);
        auto controllerPoint = U::cast<IConnectionPoint>(m.controller);
        if (componentPoint && controllerPoint) {
            m.componentConnection = owned(new ConnectionProxy(componentPoint));
            m.controllerConnection = owned(new ConnectionProxy(controllerPoint));
            m.componentConnection->connect(controllerPoint);
            m.controllerConnection->connect(componentPoint);
        }
    }
    if (!m.hostApplication->capturePolicyAcknowledged()) {
        error = "Chimera VST3 does not support capture safety. Update the Chimera VST3 and OBS add-on together; MATCH output protection could not be confirmed.";
        unload();return false;
    }
    // A DAW host synchronises the controller with the processor's state.
    MemoryStream state;
    if (m.component->getState(&state) == kResultOk) {
        state.seek(0, IBStream::kIBSeekSet, nullptr);
        m.controller->setComponentState(&state);
    }
    return true;
}

void ChimeraHost::unload() noexcept {
    auto& m = *impl;
    if (m.component) m.deactivate();
    if (m.componentConnection) m.componentConnection->disconnect();
    if (m.controllerConnection) m.controllerConnection->disconnect();
    m.componentConnection = m.controllerConnection = nullptr;
    if (m.controller) {
        m.controller->setComponentHandler(nullptr);
        if (m.separateController) m.controller->terminate();
    }
    if (m.component) m.component->terminate();
    m.controller = nullptr;
    m.processor = nullptr;
    m.component = nullptr;
    m.separateController = false;
    m.module = nullptr;
    m.identity = {};
}

bool ChimeraHost::loaded() const noexcept { return impl->component != nullptr; }
const ChimeraIdentity& ChimeraHost::identity() const noexcept { return impl->identity; }

bool ChimeraHost::prepare(double sampleRate, int channels, int maximumFrames) {
    auto& m = *impl;
    if (!m.processor || channels < 1 || channels > 2 || maximumFrames < 1) return false;
    m.deactivate();

    const int inputs = m.component->getBusCount(kAudio, kInput);
    const int outputs = m.component->getBusCount(kAudio, kOutput);
    if (inputs < 1 || outputs < 1) return false;
    const SpeakerArrangement main = channels == 2 ? SpeakerArr::kStereo : SpeakerArr::kMono;
    // Chimera's main bus only; any auxiliary bus is left empty and inactive.
    std::vector<SpeakerArrangement> in(static_cast<size_t>(inputs), SpeakerArr::kEmpty), out(static_cast<size_t>(outputs), SpeakerArr::kEmpty);
    in[0] = out[0] = main;
    if (m.processor->setBusArrangements(in.data(), inputs, out.data(), outputs) != kResultTrue) return false;
    SpeakerArrangement accepted{};
    if (m.processor->getBusArrangement(kOutput, 0, accepted) != kResultTrue || accepted != main) return false;
    for (int b = 0; b < inputs; ++b) m.component->activateBus(kAudio, kInput, b, b == 0);
    for (int b = 0; b < outputs; ++b) m.component->activateBus(kAudio, kOutput, b, b == 0);

    ProcessSetup setup{kRealtime, kSample32, maximumFrames, sampleRate};
    if (m.processor->setupProcessing(setup) != kResultOk) return false;

    m.channels = channels;
    m.capacity = maximumFrames;
    m.output.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(maximumFrames)));
    m.inputBuses.assign(static_cast<size_t>(inputs), AudioBusBuffers{});
    m.outputBuses.assign(static_cast<size_t>(outputs), AudioBusBuffers{});
    for (int c = 0; c < channels; ++c) m.outputPointers[c] = m.output[static_cast<size_t>(c)].data();
    m.inputBuses[0].numChannels = m.outputBuses[0].numChannels = channels;
    m.inputBuses[0].channelBuffers32 = m.inputPointers;
    m.outputBuses[0].channelBuffers32 = m.outputPointers;

    m.context = {};
    m.context.sampleRate = sampleRate;
    m.context.tempo = 120.0;
    m.context.timeSigNumerator = m.context.timeSigDenominator = 4;
    m.context.state = ProcessContext::kTempoValid | ProcessContext::kTimeSigValid;
    m.data = {};
    m.data.processMode = kRealtime;
    m.data.symbolicSampleSize = kSample32;
    m.data.numInputs = inputs;
    m.data.numOutputs = outputs;
    m.data.inputs = m.inputBuses.data();
    m.data.outputs = m.outputBuses.data();
    m.data.processContext = &m.context;

    if (m.component->setActive(true) != kResultOk) return false;
    m.active = true;
    m.processor->setProcessing(true); // kNotImplemented is permitted by the VST3 spec.
    return true;
}

bool ChimeraHost::process(float* const* data, int channels, int frames) noexcept {
    auto& m = *impl;
    if (!m.active || channels != m.channels || frames < 0 || frames > m.capacity) return false;
    for (int c = 0; c < channels; ++c) m.inputPointers[c] = data[c];
    m.inputBuses[0].silenceFlags = m.outputBuses[0].silenceFlags = 0;
    m.data.numSamples = frames;
    if (m.resetRequested.exchange(false)) {
        m.processor->setProcessing(false);
        m.processor->setProcessing(true);
    }
    // Separate output buffers: the result never depends on in-place support.
    if (m.processor->process(m.data) != kResultOk) return false;
    for (int c = 0; c < channels; ++c) std::memcpy(data[c], m.outputPointers[c], sizeof(float) * static_cast<size_t>(frames));
    m.context.projectTimeSamples += frames;
    m.context.continousTimeSamples += frames;
    return true;
}

void ChimeraHost::requestReset() noexcept { impl->resetRequested.store(true); }

int ChimeraHost::latency() const noexcept {
    return impl->processor ? static_cast<int>(impl->processor->getLatencySamples()) : 0;
}

bool ChimeraHost::getState(std::vector<std::uint8_t>& component, std::vector<std::uint8_t>& controller) const {
    auto& m = *impl;
    if (!m.component) return false;
    MemoryStream processorState, controllerState;
    if (m.component->getState(&processorState) != kResultOk) return false;
    component = drain(processorState);
    controller.clear();
    if (m.separateController && m.controller->getState(&controllerState) == kResultOk) controller = drain(controllerState);
    return true;
}

bool ChimeraHost::setState(const std::vector<std::uint8_t>& component, const std::vector<std::uint8_t>& controller) {
    auto& m = *impl;
    if (!m.component || component.empty()) return false;
    MemoryStream processorState;
    if (!fill(processorState, component) || m.component->setState(&processorState) != kResultOk) return false;
    processorState.seek(0, IBStream::kIBSeekSet, nullptr);
    m.controller->setComponentState(&processorState);
    if (m.separateController && !controller.empty()) {
        MemoryStream controllerState;
        if (fill(controllerState, controller)) m.controller->setState(&controllerState);
    }
    return true;
}

int ChimeraHost::parameterCount() const noexcept {
    return impl->controller ? static_cast<int>(impl->controller->getParameterCount()) : 0;
}

bool ChimeraHost::parameterInfo(int index, std::uint32_t& id, std::string& title, int& stepCount) const {
    ParameterInfo info{};
    if (!impl->controller || impl->controller->getParameterInfo(index, info) != kResultOk) return false;
    id = info.id;
    stepCount = static_cast<int>(info.stepCount);
    title = Steinberg::Vst::StringConvert::convert(info.title);
    return true;
}

double ChimeraHost::parameter(std::uint32_t id) const noexcept {
    return impl->controller ? impl->controller->getParamNormalized(id) : 0.0;
}

bool ChimeraHost::setParameter(std::uint32_t id, double normalized) noexcept {
    return impl->controller && impl->controller->setParamNormalized(id, normalized) == kResultOk;
}

std::uint64_t ChimeraHost::editGeneration() const noexcept { return impl->handler->edits.load(); }

IPlugView* ChimeraHost::createEditorView() const {
    return impl->controller ? impl->controller->createView(ViewType::kEditor) : nullptr;
}

std::vector<std::string> defaultBundlePaths() {
    const std::string bundle = std::string(chimeraClassName) + ".vst3";
    std::vector<std::string> paths;
    const auto add = [&](const char* root, const char* suffix) {
        if (root != nullptr && *root != '\0') paths.push_back(std::string(root) + suffix + bundle);
    };
#if defined(_WIN32)
    add(std::getenv("CommonProgramFiles"), "\\VST3\\");
#elif defined(__APPLE__)
    add(std::getenv("HOME"), "/Library/Audio/Plug-Ins/VST3/");
    add("/Library/Audio/Plug-Ins/VST3", "/");
#else
    add(std::getenv("HOME"), "/.vst3/");
    add("/usr/local/lib/vst3", "/");
    add("/usr/lib/vst3", "/");
#endif
    return paths;
}
}

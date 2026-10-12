// A small, real Windows VST3 host. Deliberately does not link JUCE or Chimera:
// plugin-created threads, HWNDs, timers and module globals must all disappear
// through the public VST3 lifetime, including the last FreeLibrary().
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

// The SDK's vstinitiids.cpp does not instantiate the platform view interfaces.
namespace Steinberg
{
DEF_CLASS_IID (IPlugView)
DEF_CLASS_IID (IPlugFrame)
}

namespace
{
constexpr DWORD watchdogMilliseconds = 45000;
const std::vector<std::string> scenarios {
    "headless", "editor-pumping", "editor-pending", "multiple-instances", "reopen-project",
    "live-remove-controller-first", "live-remove-component-first", "headless-component-first",
    "editor-closed-before-project-controller-first", "editor-closed-before-project-component-first",
    "remove-last-controller-first", "remove-last-component-first"
};

std::string currentScenario;
std::atomic<int> currentRound { 0 };
std::atomic<uint64_t> nextOperation { 0 };
std::mutex eventMutex;
uint64_t eventSequence = 0;

int64_t microseconds()
{
    return std::chrono::duration_cast<std::chrono::microseconds> (
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string jsonString (const std::string& value)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string output = "\"";
    for (const auto c : value)
    {
        const auto byte = static_cast<unsigned char> (c);
        if (c == '"' || c == '\\') { output += '\\'; output += c; }
        else if (byte < 0x20) { output += "\\u00"; output += hex[byte >> 4]; output += hex[byte & 15]; }
        else output += c;
    }
    return output + '"';
}

// A return is emitted only after a successful API call. An exception or a hang
// leaves its begin record pending, including when main/audio operations overlap.
void event (const char* name, const char* edge, int instance = 0,
            uint64_t operation = 0, int64_t elapsed = -1)
{
    const std::lock_guard<std::mutex> lock (eventMutex);
    std::ostringstream output;
    output << "LIFECYCLE_EVENT {\"schema\":1,\"scenario\":" << jsonString (currentScenario)
           << ",\"pid\":" << GetCurrentProcessId() << ",\"tid\":" << GetCurrentThreadId()
           << ",\"seq\":" << ++eventSequence << ",\"round\":" << currentRound.load()
           << ",\"instance\":" << instance << ",\"operation_id\":" << operation
           << ",\"phase\":" << jsonString (name) << ",\"edge\":" << jsonString (edge)
           << ",\"monotonic_us\":" << microseconds();
    if (elapsed >= 0) output << ",\"elapsed_us\":" << elapsed;
    std::cout << output.str() << '}' << std::endl;
}

template <typename Function> auto phase (const char* name, int instance, Function&& function)
{
    const auto operation = ++nextOperation;
    const auto started = microseconds();
    event (name, "begin", instance, operation);
    if constexpr (std::is_void_v<std::invoke_result_t<Function>>)
    {
        function();
        event (name, "return", instance, operation, microseconds() - started);
    }
    else
    {
        auto result = function();
        event (name, "return", instance, operation, microseconds() - started);
        return result;
    }
}

void require (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}

void step (const char* message)
{
    const std::lock_guard<std::mutex> lock (eventMutex);
    std::cout << "STAGE " << message << std::endl;
}

bool matches (const TUID actual, const FUID& expected)
{
    return std::memcmp (actual, expected.toTUID(), sizeof (TUID)) == 0;
}

// All host objects keep an owner reference until after module unload. They
// genuinely account for plugin references rather than pretending addRef is 1.
#define HOST_REFS \
    uint32 PLUGIN_API addRef() override { return ++refs; } \
    uint32 PLUGIN_API release() override { const auto n = --refs; if (n == 0) delete this; return n; } \
    std::atomic<uint32> refs { 1 }

class Attributes final : public IAttributeList
{
public:
    tresult PLUGIN_API queryInterface (const TUID id, void** out) override
    {
        *out = nullptr;
        if (! matches (id, FUnknown::iid) && ! matches (id, IAttributeList::iid)) return kNoInterface;
        *out = static_cast<IAttributeList*> (this); addRef(); return kResultOk;
    }
    HOST_REFS;
    tresult PLUGIN_API setInt (AttrID id, int64 value) override { integers[id] = value; return kResultOk; }
    tresult PLUGIN_API getInt (AttrID id, int64& value) override
    { const auto it = integers.find (id); if (it == integers.end()) return kResultFalse; value = it->second; return kResultOk; }
    tresult PLUGIN_API setFloat (AttrID id, double value) override { floats[id] = value; return kResultOk; }
    tresult PLUGIN_API getFloat (AttrID id, double& value) override
    { const auto it = floats.find (id); if (it == floats.end()) return kResultFalse; value = it->second; return kResultOk; }
    tresult PLUGIN_API setString (AttrID id, const TChar* value) override { strings[id] = value; return kResultOk; }
    tresult PLUGIN_API getString (AttrID id, TChar* value, uint32 bytes) override
    {
        const auto it = strings.find (id); if (it == strings.end() || bytes < sizeof (TChar)) return kResultFalse;
        const auto count = std::min<size_t> (it->second.size(), bytes / sizeof (TChar) - 1);
        std::copy_n (it->second.data(), count, value); value[count] = 0; return kResultOk;
    }
    tresult PLUGIN_API setBinary (AttrID id, const void* data, uint32 size) override
    { const auto* first = static_cast<const uint8*> (data); binaries[id] = { first, first + size }; return kResultOk; }
    tresult PLUGIN_API getBinary (AttrID id, const void*& data, uint32& size) override
    { const auto it = binaries.find (id); if (it == binaries.end()) return kResultFalse; data = it->second.data(); size = static_cast<uint32> (it->second.size()); return kResultOk; }
private:
    std::map<std::string, int64> integers;
    std::map<std::string, double> floats;
    std::map<std::string, std::basic_string<TChar>> strings;
    std::map<std::string, std::vector<uint8>> binaries;
};

class Message final : public IMessage
{
public:
    ~Message() { attributes->release(); }
    tresult PLUGIN_API queryInterface (const TUID id, void** out) override
    {
        *out = nullptr;
        if (! matches (id, FUnknown::iid) && ! matches (id, IMessage::iid)) return kNoInterface;
        *out = static_cast<IMessage*> (this); addRef(); return kResultOk;
    }
    HOST_REFS;
    FIDString PLUGIN_API getMessageID() override { return messageId.c_str(); }
    void PLUGIN_API setMessageID (FIDString id) override { messageId = id != nullptr ? id : ""; }
    IAttributeList* PLUGIN_API getAttributes() override { return attributes; }
private:
    Attributes* attributes = new Attributes;
    std::string messageId;
};

class Host final : public IHostApplication, public IComponentHandler, public IPlugFrame
{
public:
    explicit Host (bool neutral) : neutralName (neutral) {}
    tresult PLUGIN_API queryInterface (const TUID id, void** out) override
    {
        *out = nullptr;
        if (matches (id, FUnknown::iid) || matches (id, IHostApplication::iid)) *out = static_cast<IHostApplication*> (this);
        else if (matches (id, IComponentHandler::iid)) *out = static_cast<IComponentHandler*> (this);
        else if (matches (id, IPlugFrame::iid)) *out = static_cast<IPlugFrame*> (this);
        if (*out == nullptr) return kNoInterface;
        addRef(); return kResultOk;
    }
    HOST_REFS;
    tresult PLUGIN_API getName (String128 name) override
    {
        // Retain the previous probes' Studio One compatibility path, and use a
        // neutral host for the new Issue #3 cases. Neither name simulates a DAW.
        const auto* title = neutralName ? "Chimera VST3 Lifecycle Harness" : "Studio One Chimera Lifecycle Harness";
        for (size_t i = 0; i <= std::strlen (title); ++i) name[i] = static_cast<TChar> (title[i]);
        return kResultOk;
    }
    tresult PLUGIN_API createInstance (TUID cid, TUID iid, void** out) override
    {
        *out = nullptr;
        if (matches (cid, IMessage::iid) && matches (iid, IMessage::iid)) *out = new Message;
        else if (matches (cid, IAttributeList::iid) && matches (iid, IAttributeList::iid)) *out = new Attributes;
        return *out != nullptr ? kResultOk : kNoInterface;
    }
    tresult PLUGIN_API beginEdit (ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit (ParamID, ParamValue) override { return kResultOk; }
    tresult PLUGIN_API endEdit (ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent (int32) override { ++restarts; return kResultOk; }
    tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* rect) override { return view->onSize (rect); }
    std::atomic<int> restarts { 0 };
private:
    bool neutralName = false;
};

class Stream final : public IBStream
{
public:
    tresult PLUGIN_API queryInterface (const TUID id, void** out) override
    {
        *out = nullptr;
        if (! matches (id, FUnknown::iid) && ! matches (id, IBStream::iid)) return kNoInterface;
        *out = static_cast<IBStream*> (this); addRef(); return kResultOk;
    }
    HOST_REFS;
    tresult PLUGIN_API read (void* output, int32 requested, int32* readBytes) override
    {
        if (requested < 0) return kInvalidArgument;
        const auto count = std::min<size_t> (static_cast<size_t> (requested), bytes.size() - std::min (position, bytes.size()));
        if (count != 0) std::memcpy (output, bytes.data() + position, count);
        position += count; if (readBytes != nullptr) *readBytes = static_cast<int32> (count);
        return count != 0 || requested == 0 ? kResultOk : kResultFalse;
    }
    tresult PLUGIN_API write (void* input, int32 count, int32* written) override
    {
        if (count < 0) return kInvalidArgument;
        bytes.resize (std::max (bytes.size(), position + static_cast<size_t> (count)));
        if (count != 0) std::memcpy (bytes.data() + position, input, static_cast<size_t> (count));
        position += static_cast<size_t> (count); if (written != nullptr) *written = count; return kResultOk;
    }
    tresult PLUGIN_API seek (int64 offset, int32 mode, int64* result) override
    {
        const auto base = mode == kIBSeekSet ? int64{} : mode == kIBSeekCur ? static_cast<int64> (position) : static_cast<int64> (bytes.size());
        if (offset + base < 0) return kInvalidArgument;
        position = static_cast<size_t> (base + offset); if (result != nullptr) *result = static_cast<int64> (position); return kResultOk;
    }
    tresult PLUGIN_API tell (int64* result) override { if (result == nullptr) return kInvalidArgument; *result = static_cast<int64> (position); return kResultOk; }
    std::vector<uint8> bytes;
    size_t position = 0;
};

template <typename T> T* query (FUnknown* object)
{
    T* result = nullptr;
    require (object->queryInterface (T::iid.toTUID(), reinterpret_cast<void**> (&result)) == kResultOk && result != nullptr,
             "required VST3 interface unavailable");
    return result;
}

template <typename T> void release (T*& object)
{
    if (object != nullptr) std::exchange (object, nullptr)->release();
}

void pump (DWORD milliseconds)
{
    const auto until = GetTickCount64() + milliseconds;
    do
    {
        MSG message {};
        while (PeekMessageW (&message, nullptr, 0, 0, PM_REMOVE))
        { TranslateMessage (&message); DispatchMessageW (&message); }
        Sleep (1);
    } while (GetTickCount64() < until);
}

// A single point of parameter automation makes processor-side notifications
// pending at project close instead of testing only a never-used constructor.
class ParameterQueue final : public IParamValueQueue
{
public:
    ParamID id = 0; ParamValue value = 0.0;
    tresult PLUGIN_API queryInterface (const TUID iid, void** out) override
    { *out = nullptr; if (! matches (iid, FUnknown::iid) && ! matches (iid, IParamValueQueue::iid)) return kNoInterface; *out = this; addRef(); return kResultOk; }
    HOST_REFS;
    ParamID PLUGIN_API getParameterId() override { return id; }
    int32 PLUGIN_API getPointCount() override { return 1; }
    tresult PLUGIN_API getPoint (int32 index, int32& offset, ParamValue& output) override
    { if (index != 0) return kInvalidArgument; offset = 0; output = value; return kResultOk; }
    tresult PLUGIN_API addPoint (int32, ParamValue v, int32& index) override { value = v; index = 0; return kResultOk; }
};

class Changes final : public IParameterChanges
{
public:
    ~Changes() { for (auto* queue : queues) queue->release(); }
    tresult PLUGIN_API queryInterface (const TUID id, void** out) override
    { *out = nullptr; if (! matches (id, FUnknown::iid) && ! matches (id, IParameterChanges::iid)) return kNoInterface; *out = this; addRef(); return kResultOk; }
    HOST_REFS;
    int32 PLUGIN_API getParameterCount() override { return static_cast<int32> (queues.size()); }
    IParamValueQueue* PLUGIN_API getParameterData (int32 index) override
    { return index >= 0 && static_cast<size_t> (index) < queues.size() ? queues[static_cast<size_t> (index)] : nullptr; }
    IParamValueQueue* PLUGIN_API addParameterData (const ParamID& id, int32& index) override
    { auto* queue = new ParameterQueue; queue->id = id; index = static_cast<int32> (queues.size()); queues.push_back (queue); return queue; }
    void add (ParamID id, double value)
    { int32 index = 0; auto* queue = static_cast<ParameterQueue*> (addParameterData (id, index)); queue->value = value; }
private:
    std::vector<ParameterQueue*> queues;
};

std::string ascii (const TChar* value)
{
    std::string text;
    while (*value != 0) { const auto c = *value++; text += c < 128 ? static_cast<char> (c) : '?'; }
    return text;
}

struct Instance
{
    explicit Instance (int identity) : id (identity) {}
    int id = 0;
    IComponent* component = nullptr;
    IAudioProcessor* audio = nullptr;
    IEditController* controller = nullptr;
    IConnectionPoint* componentConnection = nullptr;
    IConnectionPoint* controllerConnection = nullptr;
    IPlugView* view = nullptr;
    HWND window = nullptr;
    Changes* changes = new Changes;

    void create (IPluginFactory* factory, const TUID classID, Host& host)
    {
        step ("create component/controller");
        require (factory->createInstance (classID, IComponent::iid.toTUID(), reinterpret_cast<void**> (&component)) == kResultOk, "create component");
        require (component->initialize (static_cast<IHostApplication*> (&host)) == kResultOk, "initialize component");
        audio = query<IAudioProcessor> (component);
        TUID controllerID {};
        require (component->getControllerClassId (controllerID) == kResultOk, "controller class ID");
        require (factory->createInstance (controllerID, IEditController::iid.toTUID(), reinterpret_cast<void**> (&controller)) == kResultOk, "create controller");
        require (controller->initialize (static_cast<IHostApplication*> (&host)) == kResultOk, "initialize controller");
        require (controller->setComponentHandler (&host) == kResultOk, "set component handler");
        componentConnection = query<IConnectionPoint> (component);
        controllerConnection = query<IConnectionPoint> (controller);
        require (componentConnection->connect (controllerConnection) == kResultOk, "connect component");
        require (controllerConnection->connect (componentConnection) == kResultOk, "connect controller");

        step ("state roundtrip and prepare");
        auto* state = new Stream;
        require (component->getState (state) == kResultOk && ! state->bytes.empty(), "save component state");
        state->position = 0;
        require (component->setState (state) == kResultOk, "restore component state");
        state->position = 0;
        require (controller->setComponentState (state) == kResultOk, "restore controller component state");
        require (state->refs == 1, "plugin retained state stream");
        state->release();

        const auto count = controller->getParameterCount();
        require (count > 100, "controller parameter bank was not connected");
        int targeted = 0;
        for (int32 i = 0; i < count; ++i)
        {
            ParameterInfo info {};
            if (controller->getParameterInfo (i, info) != kResultOk) continue;
            auto name = ascii (info.title);
            std::transform (name.begin(), name.end(), name.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
            // Exercise changing latency/IR/oversampling while audio is active.
            if (name == "transpose semitones" || name == "oversampling" || name == "routing")
            { changes->add (info.id, name == "routing" ? 1.0 : 0.65); ++targeted; }
        }
        std::cout << "PARAMETERS " << count << " automated=" << targeted << std::endl;
        require (targeted == 3, "latency/oversampling/routing automation targets missing");
        SpeakerArrangement arrangement = SpeakerArr::kStereo;
        require (audio->setBusArrangements (&arrangement, 1, &arrangement, 1) == kResultOk, "stereo arrangement");
        require (component->activateBus (kAudio, kInput, 0, true) == kResultOk, "activate input");
        require (component->activateBus (kAudio, kOutput, 0, true) == kResultOk, "activate output");
        ProcessSetup setup { kRealtime, kSample32, 256, 48000.0 };
        require (audio->setupProcessing (setup) == kResultOk, "setup processing");
        require (component->setActive (true) == kResultOk, "activate component");
    }

    void openEditor (Host& host)
    {
        step ("attach real HWND editor");
        phase ("editor.create", id, [&]
        {
            view = controller->createView (ViewType::kEditor);
            require (view != nullptr && view->isPlatformTypeSupported (kPlatformTypeHWND) == kResultOk, "create Windows editor");
        });
        window = CreateWindowExW (0, L"STATIC", L"Chimera VST3 lifecycle regression", WS_OVERLAPPEDWINDOW,
                                  0, 0, 1200, 850, nullptr, nullptr, GetModuleHandleW (nullptr), nullptr);
        require (window != nullptr, "create host HWND");
        require (view->setFrame (&host) == kResultOk, "set plug frame");
        phase ("editor.attach", id, [&] { require (view->attached (window, kPlatformTypeHWND) == kResultOk, "attach plugin editor"); });
        ShowWindow (window, SW_SHOWNOACTIVATE);
        UpdateWindow (window);
        pump (100);
    }

    void closeEditor()
    {
        if (view == nullptr) return;
        step ("remove editor without pumping pending messages");
        phase ("view.removed", id, [&] { require (view->removed() == kResultOk, "remove plugin editor"); });
        phase ("view.set_frame_null", id, [&] { require (view->setFrame (nullptr) == kResultOk, "detach plug frame"); });
        phase ("view.release", id, [&] { release (view); });
        phase ("host_window.destroy", id, [&] { require (DestroyWindow (window) != 0, "destroy host HWND"); });
        window = nullptr;
    }

    void process()
    {
        step ("audio worker start/process/stop");
        phase ("audio.start_processing", id, [&] { require (audio->setProcessing (true) == kResultOk, "start processing"); });
        std::array<float, 256> left {}, right {}, outputL {}, outputR {};
        std::array<float*, 2> inputs { left.data(), right.data() }, outputs { outputL.data(), outputR.data() };
        AudioBusBuffers input {}; input.numChannels = 2; input.channelBuffers32 = inputs.data();
        AudioBusBuffers output {}; output.numChannels = 2; output.channelBuffers32 = outputs.data();
        ProcessContext context {}; context.sampleRate = 48000.0; context.tempo = 120.0;
        ProcessData data {}; data.processMode = kRealtime; data.symbolicSampleSize = kSample32;
        data.numSamples = 256; data.numInputs = 1; data.numOutputs = 1; data.inputs = &input; data.outputs = &output;
        data.processContext = &context;
        for (int block = 0; block < 48; ++block)
        {
            for (int frame = 0; frame < 256; ++frame)
            { left[static_cast<size_t> (frame)] = right[static_cast<size_t> (frame)] = 0.1f * std::sin (static_cast<float> (block * 256 + frame) * 0.04f); }
            data.inputParameterChanges = block == 0 || block == 47 ? changes : nullptr;
            require (audio->process (data) == kResultOk, "process audio");
            for (auto sample : outputL) require (std::isfinite (sample), "non-finite processed audio");
            context.projectTimeSamples += 256;
        }
        phase ("audio.stop_processing", id, [&] { require (audio->setProcessing (false) == kResultOk, "stop processing"); });
        step ("audio worker stopped");
    }

    void destroy (bool controllerFirst)
    {
        closeEditor();
        step ("deactivate component");
        phase ("component.deactivate", id, [&] { require (component->setActive (false) == kResultOk, "deactivate component"); });
        step ("disconnect controller/component");
        phase ("component.disconnect", id, [&] { require (componentConnection->disconnect (controllerConnection) == kResultOk, "disconnect component"); });
        phase ("controller.disconnect", id, [&] { require (controllerConnection->disconnect (componentConnection) == kResultOk, "disconnect controller"); });
        phase ("connections.release", id, [&] { release (componentConnection); release (controllerConnection); });
        phase ("controller.clear_handler", id, [&] { require (controller->setComponentHandler (nullptr) == kResultOk, "clear component handler"); });
        const auto destroyController = [&]
        {
            phase ("controller.terminate", id, [&] { require (controller->terminate() == kResultOk, "terminate controller"); });
            phase ("controller.release", id, [&] { release (controller); });
        };
        const auto destroyComponent = [&]
        {
            phase ("component.terminate", id, [&] { require (component->terminate() == kResultOk, "terminate component"); });
            phase ("audio.release", id, [&] { release (audio); });
            phase ("component.release", id, [&] { release (component); });
        };
        if (controllerFirst)
        {
            step ("terminate/release controller first");
            destroyController();
            step ("terminate/release component last");
            destroyComponent();
        }
        else
        {
            step ("terminate/release component first");
            destroyComponent();
            step ("terminate/release controller last");
            destroyController();
        }
        require (changes->refs == 1, "plugin retained parameter changes");
        release (changes);
        event ("instance.destroyed", "point", id);
        step ("instance destroyed");
    }
};

void processOnWorker (Instance& instance, bool pumping = false)
{
    std::exception_ptr error;
    std::atomic<bool> done { false };
    std::thread worker ([&]
    {
        try { phase ("audio.worker", instance.id, [&] { instance.process(); }); }
        catch (...) { error = std::current_exception(); }
        done = true;
    });
    // Main-thread VST3 lifetime calls remain on the creating UI thread. Only
    // audio callbacks and setProcessing execute on this separate worker.
    phase ("audio.worker_join", instance.id, [&]
    {
        if (pumping) while (! done) pump (5);
        worker.join(); // the parent watchdog retains both pending operations
        if (error != nullptr) std::rethrow_exception (error);
    });
}

int child (const std::filesystem::path& path, const std::string& scenario,
           const std::filesystem::path& tracePath)
{
    currentScenario = scenario;
    require (std::find (scenarios.begin(), scenarios.end(), scenario) != scenarios.end(), "unknown child scenario");
    if (! tracePath.empty())
        require (SetEnvironmentVariableW (L"CHIMERA_LIFECYCLE_TRACE", tracePath.c_str()) != 0, "set per-scenario plugin trace");
    event ("host.started", "point");
    phase ("host.com_initialize", 0, [&] { require (CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED) == S_OK, "initialize COM"); });
    const bool editor = scenario != "headless" && scenario != "headless-component-first";
    const bool pumping = scenario == "editor-pumping";
    const bool liveRemoval = scenario == "live-remove-controller-first" || scenario == "live-remove-component-first";
    const bool closedEarly = scenario.find ("editor-closed-before-project-") == 0;
    const bool removeLast = scenario.find ("remove-last-") == 0;
    const bool multi = scenario == "multiple-instances" || liveRemoval;
    const bool componentFirst = scenario.find ("component-first") != std::string::npos;
    const bool neutralHost = closedEarly || removeLast || scenario == "headless-component-first";
    const int rounds = scenario == "reopen-project" ? 3 : 1;
    int nextInstance = 0;
    for (int round = 0; round < rounds; ++round)
    {
        currentRound = round + 1;
        event ("round.started", "point");
        step ("LoadLibrary/InitDll/GetPluginFactory");
        auto module = phase ("module.load_library", 0, [&]
        {
            const auto loaded = LoadLibraryW (path.c_str());
            require (loaded != nullptr, "LoadLibrary failed");
            return loaded;
        });
        const auto init = reinterpret_cast<bool (*)()> (GetProcAddress (module, "InitDll"));
        const auto exit = reinterpret_cast<bool (*)()> (GetProcAddress (module, "ExitDll"));
        const auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API*)()> (GetProcAddress (module, "GetPluginFactory"));
        require (init != nullptr && exit != nullptr && getFactory != nullptr, "VST3 entry points");
        phase ("module.init", 0, [&] { require (init(), "InitDll failed"); });
        auto* factory = phase ("factory.get", 0, [&]
        {
            auto* result = getFactory(); require (result != nullptr, "null factory"); return result;
        });
        TUID componentClass {};
        bool found = false;
        for (int32 i = 0; i < factory->countClasses(); ++i)
        {
            PClassInfo info {};
            if (factory->getClassInfo (i, &info) == kResultOk && std::strcmp (info.category, kVstAudioEffectClass) == 0)
            { std::memcpy (componentClass, info.cid, sizeof (TUID)); found = true; break; }
        }
        require (found, "no audio component factory class");
        auto* host = new Host (neutralHost);
        event (neutralHost ? "host.name_neutral" : "host.name_studio_one_compatibility", "point");
        auto* factory3 = query<IPluginFactory3> (factory);
        require (factory3->setHostContext (static_cast<IHostApplication*> (host)) == kResultOk, "set factory host context");
        release (factory3);
        std::vector<std::unique_ptr<Instance>> instances;
        const auto createInstance = [&]
        {
            auto instance = std::make_unique<Instance> (++nextInstance);
            phase ("instance.create", instance->id, [&] { instance->create (factory, componentClass, *host); });
            if (editor) instance->openEditor (*host);
            processOnWorker (*instance, pumping);
            return instance;
        };
        const auto emptyProjectIdle = [&]
        {
            phase ("project.empty_idle", 0, [&]
            {
                require (GetModuleHandleW (path.c_str()) == module, "empty project unexpectedly unloaded module");
                pump (250);
                require (GetModuleHandleW (path.c_str()) == module, "idle after removal unloaded module");
            });
        };
        for (int i = 0; i < (multi ? 3 : 1); ++i)
            instances.push_back (createInstance());

        if (closedEarly)
        {
            // These are distinct from the old editor-pending test: destroy the
            // view, then keep the instance, project and module active, process
            // additional audio and dispatch UI callbacks before project close.
            instances[0]->closeEditor();
            phase ("project.idle_after_editor_close", instances[0]->id, [&]
            {
                pump (250);
                processOnWorker (*instances[0], true);
                pump (100);
            });
        }
        if (liveRemoval)
        {
            // Delete one previously opened instance while the factory, module
            // and two peers remain alive. No pumping hides the removal call.
            for (int cycle = 0; cycle < 3; ++cycle)
            {
                step ("live project remove opened victim; retain factory and peers");
                phase ("plugin.remove", instances[0]->id, [&] { instances[0]->destroy (! componentFirst); });
                instances[0].reset();
                require (GetModuleHandleW (path.c_str()) != nullptr, "live removal unloaded peer module");
                pump (100); // stale victim callbacks must fail in this child
                for (size_t peer = 1; peer < instances.size(); ++peer)
                {
                    step ("survivor audio after live removal");
                    processOnWorker (*instances[peer]);
                    step ("survivor save and restore after live removal");
                    auto* state = new Stream;
                    require (instances[peer]->component->getState (state) == kResultOk && !state->bytes.empty(), "survivor save");
                    state->position = 0;
                    require (instances[peer]->component->setState (state) == kResultOk, "survivor restore");
                    state->position = 0;
                    require (instances[peer]->controller->setComponentState (state) == kResultOk, "survivor controller restore");
                    require (state->refs == 1, "survivor retained state stream");
                    state->release();
                    instances[peer]->closeEditor();
                    instances[peer]->openEditor (*host);
                }
                // Recreate in the same project/factory, then delete it again.
                phase ("instance.recreate", 0, [&] { instances[0] = createInstance(); });
            }
            std::cout << "PASS live removal cycles=3 peers=2 scenario=" << scenario << std::endl;
        }

        if (removeLast)
        {
            // An empty still-running project is essential to Issue #3: removing
            // the final plugin is observable independently of DLL/host shutdown.
            // Recreate once without reloading, remove again, then close empty.
            for (int cycle = 0; cycle < 2; ++cycle)
            {
                instances[0]->closeEditor();
                pump (100);
                phase ("plugin.remove", instances[0]->id, [&] { instances[0]->destroy (! componentFirst); });
                instances.clear();
                emptyProjectIdle();
                if (cycle == 0)
                    phase ("instance.recreate", 0, [&] { instances.push_back (createInstance()); });
            }
        }
        // After normal operation, project close must not require the host to
        // pump messages between remove/deactivate/terminate/release calls.
        phase ("project.close", 0, [&]
        {
            for (size_t i = 0; i < instances.size(); ++i)
            {
                const bool controllerFirst = closedEarly || scenario == "headless-component-first"
                    ? ! componentFirst : (i + static_cast<size_t> (round)) % 2 == 0;
                phase ("instance.destroy", instances[i]->id, [&] { instances[i]->destroy (controllerFirst); });
            }
            instances.clear();
        });
        step ("release plugin factory"); phase ("factory.release", 0, [&] { release (factory); });
        step ("ExitDll"); phase ("module.exit", 0, [&] { require (exit(), "ExitDll failed"); });
        step ("FreeLibrary (last module reference)");
        phase ("module.free_library", 0, [&] { require (FreeLibrary (module) != 0, "FreeLibrary failed"); });
        // Give Windows/UI deferred cleanup an opportunity to finish after all
        // public plugin lifetime calls have returned. Never call FreeLibrary a
        // second time or accept a DLL that remains mapped after this deadline.
        const auto moduleName = path.wstring();
        const auto releaseStarted = GetTickCount64();
        const auto immediatelyUnmapped = GetModuleHandleW (moduleName.c_str()) == nullptr;
        step ("bounded host UI cleanup after FreeLibrary");
        phase ("module.unmapped_check", 0, [&]
        {
            while (GetModuleHandleW (moduleName.c_str()) != nullptr && GetTickCount64() - releaseStarted < 2000)
                pump (10);
            const auto finallyUnmapped = GetModuleHandleW (moduleName.c_str()) == nullptr;
            std::cout << "MODULE_RELEASE immediate_unmapped=" << immediatelyUnmapped
                      << " final_unmapped=" << finallyUnmapped
                      << " elapsed_ms=" << GetTickCount64() - releaseStarted << std::endl;
            require (finallyUnmapped, "plugin DLL remained loaded after FreeLibrary and 2-second host cleanup");
        });
        phase ("host.references_check", 0, [&] { require (host->refs == 1, "plugin retained host interfaces after unload"); });
        phase ("host.release", 0, [&] { host->release(); });
        // Any stale JUCE window callback into the unloaded DLL now crashes this
        // child instead of being hidden by process termination immediately after.
        step ("pump host after module unload"); phase ("host.after_unload_pump", 0, [&] { pump (150); });
        event ("round.completed", "point");
        std::cout << "PASS round=" << round + 1 << " scenario=" << scenario << std::endl;
    }
    phase ("host.com_uninitialize", 0, [&] { CoUninitialize(); });
    // This point is intentionally not called process-exit success. The parent
    // must observe the process object signalled and read its actual exit code.
    event ("host.return_ready", "point");
    return 0;
}

std::wstring quote (const std::wstring& text)
{
    require (text.find (L'"') == std::wstring::npos, "unsupported quote in Windows path");
    return L"\"" + text + L"\"";
}

struct Handle
{
    explicit Handle (HANDLE valueIn = nullptr) : value (valueIn) {}
    ~Handle() { if (value != nullptr && value != INVALID_HANDLE_VALUE) CloseHandle (value); }
    Handle (const Handle&) = delete;
    Handle& operator= (const Handle&) = delete;
    HANDLE value = nullptr;
};

// Only the harness' restricted ASCII names/numbers are read here. The portable
// evidence verifier parses every complete JSON record independently and rejects
// corrupt output; this parent parser merely retains a useful timeout snapshot.
std::string stringField (const std::string& json, const char* name)
{
    const auto key = std::string ("\"") + name + "\":\"";
    const auto start = json.find (key);
    if (start == std::string::npos) return {};
    const auto first = start + key.size();
    const auto last = json.find ('"', first);
    return last == std::string::npos ? std::string{} : json.substr (first, last - first);
}

uint64_t integerField (const std::string& json, const char* name)
{
    const auto key = std::string ("\"") + name + "\":";
    const auto start = json.find (key);
    if (start == std::string::npos) return 0;
    const auto first = start + key.size();
    const auto last = json.find_first_not_of ("0123456789", first);
    if (last == first) return 0;
    try { return std::stoull (json.substr (first, last - first)); }
    catch (...) { return 0; }
}

struct ScenarioResult
{
    std::string scenario, logFile, traceFile, lastPhase, lastEdge, lastStage;
    DWORD pid = 0, exitCode = STILL_ACTIVE, watchdogMs = watchdogMilliseconds;
    bool timedOut = false, processExitConfirmed = false, forceTerminated = false, returnReadySeen = false, captureError = false;
    uint64_t elapsedMilliseconds = 0;
    std::map<uint64_t, std::string> pending;

    bool passed() const { return processExitConfirmed && ! timedOut && ! captureError && exitCode == 0 && returnReadySeen && pending.empty(); }

    std::string json() const
    {
        std::ostringstream output;
        output << "{\"scenario\":" << jsonString (scenario) << ",\"pid\":" << pid
               << ",\"exit_code\":" << exitCode << ",\"timed_out\":" << (timedOut ? "true" : "false")
               << ",\"process_exit_confirmed\":" << (processExitConfirmed ? "true" : "false")
               << ",\"force_terminated\":" << (forceTerminated ? "true" : "false")
               << ",\"return_ready_seen\":" << (returnReadySeen ? "true" : "false")
               << ",\"capture_error\":" << (captureError ? "true" : "false")
               << ",\"elapsed_ms\":" << elapsedMilliseconds << ",\"watchdog_ms\":" << watchdogMs
               << ",\"last_phase\":" << jsonString (lastPhase) << ",\"last_edge\":" << jsonString (lastEdge)
               << ",\"last_stage\":" << jsonString (lastStage) << ",\"log_file\":" << jsonString (logFile)
               << ",\"plugin_trace_file\":" << jsonString (traceFile) << ",\"pending_phases\":[";
        bool first = true;
        for (const auto& item : pending)
        {
            if (! first) output << ',';
            first = false; output << item.second;
        }
        output << "]}";
        return output.str();
    }
};

ScenarioResult runScenario (const std::filesystem::path& executable, const std::filesystem::path& module,
                            const std::string& scenario, const std::filesystem::path& evidenceDirectory,
                            std::ofstream& eventFile, bool watchdogProbe = false)
{
    ScenarioResult result; result.scenario = scenario;
    // Synthetic watchdog probes use the exact pipe/wait/kill path but cannot
    // change the 45-second deadline of any real VST3 scenario.
    if (watchdogProbe) result.watchdogMs = 250;
    std::ofstream log;
    std::filesystem::path tracePath;
    if (! evidenceDirectory.empty())
    {
        result.logFile = scenario + ".log";
        log.open (evidenceDirectory / result.logFile, std::ios::binary | std::ios::trunc);
        require (log.is_open(), "open child evidence log");
        if (! watchdogProbe)
        {
            result.traceFile = scenario + ".trace.log";
            tracePath = evidenceDirectory / result.traceFile;
            std::ofstream trace (tracePath, std::ios::binary | std::ios::trunc);
            require (trace.is_open(), "initialize per-scenario plugin trace");
        }
    }
    std::cout << "SCENARIO " << scenario << std::endl;
    auto command = quote (executable.wstring())
        + (watchdogProbe ? L" --watchdog-child " : L" --child " + quote (module.wstring()) + L" ")
        + std::wstring (scenario.begin(), scenario.end());
    if (! tracePath.empty()) command += L" --trace-file " + quote (tracePath.wstring());

    SECURITY_ATTRIBUTES security { sizeof (SECURITY_ATTRIBUTES), nullptr, TRUE };
    Handle readPipe, writePipe;
    require (CreatePipe (&readPipe.value, &writePipe.value, &security, 0) != 0, "create child output pipe");
    require (SetHandleInformation (readPipe.value, HANDLE_FLAG_INHERIT, 0) != 0, "make parent pipe non-inheritable");
    Handle nullInput (CreateFileW (L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    require (nullInput.value != INVALID_HANDLE_VALUE, "open child stdin");
    STARTUPINFOW startup {}; startup.cb = sizeof (startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nullInput.value;
    startup.hStdOutput = writePipe.value; startup.hStdError = writePipe.value;
    PROCESS_INFORMATION process {};
    const auto started = GetTickCount64();
    require (CreateProcessW (nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0,
             "launch lifecycle child");
    Handle processHandle (process.hProcess), threadHandle (process.hThread);
    result.pid = process.dwProcessId;
    CloseHandle (std::exchange (writePipe.value, nullptr));
    std::string incompleteLine;
    ULONGLONG probeBlockedAt = 0;
    const auto acceptLine = [&] (std::string line)
    {
        if (! line.empty() && line.back() == '\r') line.pop_back();
        if (! watchdogProbe) std::cout << line << std::endl;
        constexpr const char* prefix = "LIFECYCLE_EVENT ";
        if (line.find (prefix) == 0)
        {
            const auto json = line.substr (std::strlen (prefix));
            if (eventFile.is_open())
            {
                eventFile << json << '\n'; eventFile.flush();
                if (! eventFile.good()) result.captureError = true;
            }
            result.lastPhase = stringField (json, "phase");
            result.lastEdge = stringField (json, "edge");
            if (watchdogProbe && result.lastPhase == "watchdog.blocked" && result.lastEdge == "begin" && probeBlockedAt == 0)
                probeBlockedAt = GetTickCount64();
            if (result.lastPhase == "host.return_ready" && result.lastEdge == "point"
                && integerField (json, "pid") == result.pid && stringField (json, "scenario") == scenario)
                result.returnReadySeen = true;
            const auto operation = integerField (json, "operation_id");
            if (result.lastEdge == "begin" && operation != 0) result.pending[operation] = json;
            else if (result.lastEdge == "return") result.pending.erase (operation);
        }
        else if (line.find ("STAGE ") == 0) result.lastStage = line.substr (6);
    };
    const auto drain = [&]
    {
        std::array<char, 8192> bytes {};
        size_t drained = 0;
        for (;;)
        {
            DWORD available = 0;
            if (! PeekNamedPipe (readPipe.value, nullptr, 0, nullptr, &available, nullptr) || available == 0) break;
            DWORD received = 0;
            if (! ReadFile (readPipe.value, bytes.data(), std::min<DWORD> (available, static_cast<DWORD> (bytes.size())), &received, nullptr)
                || received == 0) break;
            if (log.is_open())
            {
                log.write (bytes.data(), received); log.flush();
                if (! log.good()) result.captureError = true;
            }
            drained += received;
            incompleteLine.append (bytes.data(), received);
            for (auto newline = incompleteLine.find ('\n'); newline != std::string::npos; newline = incompleteLine.find ('\n'))
            {
                acceptLine (incompleteLine.substr (0, newline));
                incompleteLine.erase (0, newline + 1);
            }
            if (incompleteLine.size() > 64 * 1024)
            {
                // Preserve raw bytes in the log, but a broken producer cannot
                // allocate unbounded RAM by omitting newlines. Such capture is
                // explicitly invalid and can never satisfy passed().
                result.captureError = true;
                incompleteLine.erase (0, incompleteLine.size() - 64 * 1024);
            }
            // Even a faulty plugin that continually writes stdout must not
            // prevent the parent from evaluating the process deadline.
            if (drained >= 256 * 1024) break;
        }
    };

    DWORD wait = WAIT_TIMEOUT;
    for (;;)
    {
        drain();
        wait = WaitForSingleObject (processHandle.value, 20);
        if (wait != WAIT_TIMEOUT) break;
        const auto now = GetTickCount64();
        // Give the synthetic child a bounded launch window, then time the
        // intended blocked operation itself. Real VST3 deadlines still include
        // process startup and remain exactly 45 seconds.
        const bool deadlineExpired = watchdogProbe
            ? (probeBlockedAt != 0 ? now - probeBlockedAt >= result.watchdogMs : now - started >= 5000)
            : now - started >= result.watchdogMs;
        if (deadlineExpired)
        {
            result.timedOut = true;
            // Exclusively a failed-test watchdog, never product cleanup or a
            // way to turn a hang into PASS. Preserve every outstanding phase.
            result.forceTerminated = TerminateProcess (processHandle.value, 124) != 0;
            wait = WaitForSingleObject (processHandle.value, 5000);
            break;
        }
    }
    if (wait == WAIT_OBJECT_0)
        result.processExitConfirmed = GetExitCodeProcess (processHandle.value, &result.exitCode) != 0;
    drain();
    if (! incompleteLine.empty()) acceptLine (incompleteLine);
    result.elapsedMilliseconds = GetTickCount64() - started;

    // The child cannot certify its own process exit. This record belongs to the
    // parent and is emitted only after the OS wait/exit-code result is known.
    std::ostringstream exitEvent;
    exitEvent << "{\"schema\":1,\"scenario\":" << jsonString (scenario)
              << ",\"pid\":" << GetCurrentProcessId() << ",\"tid\":" << GetCurrentThreadId()
              << ",\"child_pid\":" << result.pid << ",\"seq\":" << ++eventSequence
              << ",\"round\":0,\"instance\":0,\"operation_id\":0,\"phase\":\"host.process_exit\",\"edge\":\"point\""
              << ",\"monotonic_us\":" << microseconds()
              << ",\"process_exit_confirmed\":" << (result.processExitConfirmed ? "true" : "false")
              << ",\"exit_code\":" << result.exitCode << ",\"timed_out\":" << (result.timedOut ? "true" : "false") << '}';
    if (eventFile.is_open()) { eventFile << exitEvent.str() << '\n'; eventFile.flush(); }
    std::cout << "LIFECYCLE_EVENT " << exitEvent.str() << std::endl;
    if (result.timedOut)
    {
        std::cout << (watchdogProbe ? "WATCHDOG_SELF_TEST" : "FAIL watchdog") << " scenario=" << scenario << " pid=" << result.pid << " after_ms=" << result.elapsedMilliseconds
                  << " last_phase=" << result.lastPhase << " last_edge=" << result.lastEdge << std::endl;
        for (const auto& item : result.pending) std::cout << "PENDING_PHASE " << item.second << std::endl;
    }
    std::cout << (watchdogProbe ? "WATCHDOG_OBSERVED" : result.passed() ? "PASS" : "FAIL") << " scenario=" << scenario
              << " pid=" << result.pid << " process_exit_confirmed=" << result.processExitConfirmed
              << " exit=" << result.exitCode << " elapsed_ms=" << result.elapsedMilliseconds << std::endl;
    return result;
}

int watchdogChild (const std::string& scenario)
{
    currentScenario = scenario; currentRound = 1;
    require (scenario == "watchdog-sleep" || scenario == "watchdog-flood", "unknown watchdog child");
    event ("host.started", "point");
    phase ("watchdog.blocked", 0, [&]
    {
        if (scenario == "watchdog-sleep") Sleep (INFINITE);
        else
        {
            const std::string noise = "WATCHDOG_FLOOD " + std::string (1008, 'x') + '\n';
            for (;;) { std::cout << noise << std::flush; Sleep (0); }
        }
    });
    return 1; // reaching here would mean the intended hang was not exercised
}

int watchdogSelfTest (const std::filesystem::path& executable, const std::filesystem::path& directory)
{
    std::filesystem::create_directories (directory);
    std::ofstream events (directory / "events.jsonl", std::ios::binary | std::ios::trunc);
    require (events.is_open(), "open watchdog self-test events");
    std::vector<ScenarioResult> results;
    int passed = 0;
    for (const auto* scenario : { "watchdog-sleep", "watchdog-flood" })
    {
        results.push_back (runScenario (executable, {}, scenario, directory, events, true));
        const auto& result = results.back();
        const auto blocked = std::any_of (result.pending.begin(), result.pending.end(), [] (const auto& item)
        { return stringField (item.second, "phase") == "watchdog.blocked"; });
        // Also require that the child actually began its blocked operation. A
        // slow process launch terminated before main is not a watchdog PASS.
        if (result.timedOut && result.forceTerminated && result.processExitConfirmed && result.exitCode == 124
            && blocked && result.elapsedMilliseconds < 5000 && ! result.returnReadySeen && ! result.captureError)
            ++passed;
    }
    std::ofstream summary (directory / "watchdog-self-test.json", std::ios::binary | std::ios::trunc);
    require (summary.is_open(), "open watchdog self-test summary");
    summary << "{\"schema\":1,\"kind\":\"watchdog-self-test\",\"parent_pid\":" << GetCurrentProcessId()
            << ",\"total\":2,\"passed\":" << passed << ",\"scenarios\":[";
    for (size_t i = 0; i < results.size(); ++i) { if (i != 0) summary << ','; summary << results[i].json(); }
    summary << "]}\n"; summary.flush();
    require (summary.good(), "write watchdog self-test summary");
    std::cout << "Watchdog self-test: " << passed << "/2 expected hangs detected with pending phase evidence" << std::endl;
    return passed == 2 ? 0 : 1;
}
}

int wmain (int argc, wchar_t** argv)
{
    try
    {
        if (argc >= 2 && std::wstring (argv[1]) == L"--watchdog-child")
        {
            require (argc == 3, "invalid watchdog child arguments");
            const std::wstring scenario (argv[2]);
            return watchdogChild (std::string (scenario.begin(), scenario.end()));
        }
        std::array<wchar_t, 32768> executable {};
        require (GetModuleFileNameW (nullptr, executable.data(), static_cast<DWORD> (executable.size())) != 0, "get harness path");
        if (argc >= 2 && std::wstring (argv[1]) == L"--watchdog-self-test")
        {
            require (argc == 3, "usage: ChimeraVST3LifecycleTests --watchdog-self-test <evidence-directory>");
            return watchdogSelfTest (executable.data(), std::filesystem::absolute (argv[2]));
        }
        if (argc >= 2 && std::wstring (argv[1]) == L"--child")
        {
            require (argc == 4 || (argc == 6 && std::wstring (argv[4]) == L"--trace-file"), "invalid child arguments");
            const auto scenarioWide = std::wstring (argv[3]);
            return child (argv[2], std::string (scenarioWide.begin(), scenarioWide.end()), argc == 6 ? argv[5] : std::filesystem::path{});
        }
        require (argc >= 2, "usage: ChimeraVST3LifecycleTests <plugin-module.vst3> [--evidence-dir <directory>] [--scenario <name>]");
        require (std::filesystem::is_regular_file (argv[1]), "VST3 module file missing");
        const auto module = std::filesystem::absolute (argv[1]);
        std::filesystem::path evidenceDirectory;
        std::string selectedScenario;
        for (int i = 2; i < argc; i += 2)
        {
            require (i + 1 < argc, "missing option value");
            const std::wstring option (argv[i]);
            if (option == L"--evidence-dir")
            {
                require (evidenceDirectory.empty(), "duplicate evidence directory option");
                evidenceDirectory = std::filesystem::absolute (argv[i + 1]);
            }
            else if (option == L"--scenario")
            {
                require (selectedScenario.empty(), "duplicate scenario option");
                const std::wstring name (argv[i + 1]);
                selectedScenario.assign (name.begin(), name.end());
                require (std::find (scenarios.begin(), scenarios.end(), selectedScenario) != scenarios.end(), "unknown scenario option");
            }
            else require (false, "unknown lifecycle option");
        }
        std::ofstream eventFile;
        if (! evidenceDirectory.empty())
        {
            std::filesystem::create_directories (evidenceDirectory);
            std::filesystem::remove (evidenceDirectory / "summary.json");
            eventFile.open (evidenceDirectory / "events.jsonl", std::ios::binary | std::ios::trunc);
            require (eventFile.is_open(), "open lifecycle event evidence");
        }
        int failures = 0;
        std::vector<ScenarioResult> results;
        for (const auto& scenario : scenarios)
        {
            if (! selectedScenario.empty() && selectedScenario != scenario) continue;
            results.push_back (runScenario (executable.data(), module, scenario, evidenceDirectory, eventFile));
            if (! results.back().passed()) ++failures;
            if (! results.back().processExitConfirmed) break;
        }
        if (! evidenceDirectory.empty())
        {
            std::ofstream summary (evidenceDirectory / "summary.json", std::ios::binary | std::ios::trunc);
            require (summary.is_open(), "open lifecycle summary evidence");
            summary << "{\"schema\":1,\"parent_pid\":" << GetCurrentProcessId()
                    << ",\"total\":" << results.size() << ",\"passed\":" << results.size() - static_cast<size_t> (failures)
                    << ",\"full_suite\":" << (selectedScenario.empty() && results.size() == scenarios.size() ? "true" : "false") << ",\"scenarios\":[";
            for (size_t i = 0; i < results.size(); ++i) { if (i != 0) summary << ','; summary << results[i].json(); }
            summary << "]}\n"; summary.flush();
            require (summary.good(), "write lifecycle summary evidence");
        }
        std::cout << "VST3 lifecycle scenarios: " << results.size() - static_cast<size_t> (failures)
                  << '/' << results.size() << " passed" << (selectedScenario.empty() ? "" : " (selected diagnostic only)") << std::endl;
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
}

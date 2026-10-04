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
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
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
void require (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}

void step (const char* message)
{
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
        // Match Studio One's compatibility branch while remaining an explicit
        // regression harness; this is not a claim of validation inside that DAW.
        constexpr char title[] = "Studio One Chimera Lifecycle Harness";
        for (size_t i = 0; i < sizeof (title); ++i) name[i] = static_cast<TChar> (title[i]);
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
        view = controller->createView (ViewType::kEditor);
        require (view != nullptr && view->isPlatformTypeSupported (kPlatformTypeHWND) == kResultOk, "create Windows editor");
        window = CreateWindowExW (0, L"STATIC", L"Chimera VST3 lifecycle regression", WS_OVERLAPPEDWINDOW,
                                  0, 0, 1200, 850, nullptr, nullptr, GetModuleHandleW (nullptr), nullptr);
        require (window != nullptr, "create host HWND");
        require (view->setFrame (&host) == kResultOk, "set plug frame");
        require (view->attached (window, kPlatformTypeHWND) == kResultOk, "attach plugin editor");
        ShowWindow (window, SW_SHOWNOACTIVATE);
        UpdateWindow (window);
        pump (100);
    }

    void closeEditor()
    {
        if (view == nullptr) return;
        step ("remove editor without pumping pending messages");
        require (view->removed() == kResultOk, "remove plugin editor");
        require (view->setFrame (nullptr) == kResultOk, "detach plug frame");
        release (view);
        require (DestroyWindow (window) != 0, "destroy host HWND");
        window = nullptr;
    }

    void process()
    {
        step ("audio worker start/process/stop");
        require (audio->setProcessing (true) == kResultOk, "start processing");
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
        require (audio->setProcessing (false) == kResultOk, "stop processing");
        step ("audio worker stopped");
    }

    void destroy (bool controllerFirst)
    {
        closeEditor();
        step ("deactivate component");
        require (component->setActive (false) == kResultOk, "deactivate component");
        step ("disconnect controller/component");
        require (componentConnection->disconnect (controllerConnection) == kResultOk, "disconnect component");
        require (controllerConnection->disconnect (componentConnection) == kResultOk, "disconnect controller");
        release (componentConnection); release (controllerConnection);
        require (controller->setComponentHandler (nullptr) == kResultOk, "clear component handler");
        if (controllerFirst)
        {
            step ("terminate/release controller first");
            require (controller->terminate() == kResultOk, "terminate controller"); release (controller);
            step ("terminate/release component last");
            require (component->terminate() == kResultOk, "terminate component"); release (audio); release (component);
        }
        else
        {
            step ("terminate/release component first");
            require (component->terminate() == kResultOk, "terminate component"); release (audio); release (component);
            step ("terminate/release controller last");
            require (controller->terminate() == kResultOk, "terminate controller"); release (controller);
        }
        require (changes->refs == 1, "plugin retained parameter changes");
        release (changes);
        step ("instance destroyed");
    }
};

int child (const std::filesystem::path& path, const std::string& scenario)
{
    require (CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED) == S_OK, "initialize COM");
    const bool editor = scenario != "headless";
    const bool pumping = scenario == "editor-pumping";
    const bool liveRemoval = scenario == "live-remove-controller-first" || scenario == "live-remove-component-first";
    const bool multi = scenario == "multiple-instances" || liveRemoval;
    const int rounds = scenario == "reopen-project" ? 3 : 1;
    for (int round = 0; round < rounds; ++round)
    {
        step ("LoadLibrary/InitDll/GetPluginFactory");
        auto module = LoadLibraryW (path.c_str());
        require (module != nullptr, "LoadLibrary failed");
        const auto init = reinterpret_cast<bool (*)()> (GetProcAddress (module, "InitDll"));
        const auto exit = reinterpret_cast<bool (*)()> (GetProcAddress (module, "ExitDll"));
        const auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API*)()> (GetProcAddress (module, "GetPluginFactory"));
        require (init != nullptr && exit != nullptr && getFactory != nullptr && init(), "VST3 entry points");
        auto* factory = getFactory(); require (factory != nullptr, "null factory");
        TUID componentClass {};
        bool found = false;
        for (int32 i = 0; i < factory->countClasses(); ++i)
        {
            PClassInfo info {};
            if (factory->getClassInfo (i, &info) == kResultOk && std::strcmp (info.category, kVstAudioEffectClass) == 0)
            { std::memcpy (componentClass, info.cid, sizeof (TUID)); found = true; break; }
        }
        require (found, "no audio component factory class");
        auto* host = new Host;
        auto* factory3 = query<IPluginFactory3> (factory);
        require (factory3->setHostContext (static_cast<IHostApplication*> (host)) == kResultOk, "set factory host context");
        release (factory3);
        std::vector<std::unique_ptr<Instance>> instances;
        for (int i = 0; i < (multi ? 3 : 1); ++i)
        {
            auto instance = std::make_unique<Instance>();
            instance->create (factory, componentClass, *host);
            if (editor) instance->openEditor (*host);
            std::exception_ptr error;
            std::atomic<bool> done { false };
            std::thread worker ([&]
            {
                try { instance->process(); } catch (...) { error = std::current_exception(); }
                done = true;
            });
            // Audio callbacks (including setProcessing(false)) may run on an
            // audio thread. Main-thread component/controller/editor calls remain
            // on the creating UI thread, as required by the VST3 contract.
            if (pumping) while (! done) pump (5);
            worker.join(); // parent process watchdog catches UI/audio deadlocks
            if (error != nullptr) std::rethrow_exception (error);
            instances.push_back (std::move (instance));
        }
        if (liveRemoval)
        {
            // Delete one previously opened instance while the factory, module
            // and two peers remain alive. No pumping hides the removal call.
            const bool controllerFirst = scenario == "live-remove-controller-first";
            for (int cycle = 0; cycle < 3; ++cycle)
            {
                step ("live project remove opened victim; retain factory and peers");
                instances[0]->destroy (controllerFirst);
                instances[0].reset();
                require (GetModuleHandleW (path.c_str()) != nullptr, "live removal unloaded peer module");
                pump (100); // stale victim callbacks must fail in this child
                for (size_t peer = 1; peer < instances.size(); ++peer)
                {
                    step ("survivor audio after live removal");
                    std::exception_ptr error;
                    std::thread worker ([&] { try { instances[peer]->process(); } catch (...) { error = std::current_exception(); } });
                    worker.join();
                    if (error) std::rethrow_exception (error);
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
                instances[0] = std::make_unique<Instance>();
                instances[0]->create (factory, componentClass, *host);
                instances[0]->openEditor (*host);
            }
            std::cout << "PASS live removal cycles=3 peers=2 scenario=" << scenario << std::endl;
        }
        // After normal operation, project close must not require the host to
        // pump messages between remove/deactivate/terminate/release calls.
        for (size_t i = 0; i < instances.size(); ++i) instances[i]->destroy ((i + static_cast<size_t> (round)) % 2 == 0);
        instances.clear();
        step ("release plugin factory"); release (factory);
        step ("ExitDll"); require (exit(), "ExitDll failed");
        step ("FreeLibrary (last module reference)"); require (FreeLibrary (module) != 0, "FreeLibrary failed");
        // Give Windows/UI deferred cleanup an opportunity to finish after all
        // public plugin lifetime calls have returned. Never call FreeLibrary a
        // second time or accept a DLL that remains mapped after this deadline.
        const auto moduleName = path.wstring();
        const auto releaseStarted = GetTickCount64();
        const auto immediatelyUnmapped = GetModuleHandleW (moduleName.c_str()) == nullptr;
        step ("bounded host UI cleanup after FreeLibrary");
        while (GetModuleHandleW (moduleName.c_str()) != nullptr && GetTickCount64() - releaseStarted < 2000)
            pump (10);
        const auto finallyUnmapped = GetModuleHandleW (moduleName.c_str()) == nullptr;
        std::cout << "MODULE_RELEASE immediate_unmapped=" << immediatelyUnmapped
                  << " final_unmapped=" << finallyUnmapped
                  << " elapsed_ms=" << GetTickCount64() - releaseStarted << std::endl;
        require (finallyUnmapped, "plugin DLL remained loaded after FreeLibrary and 2-second host cleanup");
        require (host->refs == 1, "plugin retained host interfaces after unload");
        host->release();
        // Any stale JUCE window callback into the unloaded DLL now crashes this
        // child instead of being hidden by process termination immediately after.
        step ("pump host after module unload"); pump (150);
        std::cout << "PASS round=" << round + 1 << " scenario=" << scenario << std::endl;
    }
    CoUninitialize();
    return 0;
}

std::wstring quote (const std::wstring& text)
{
    require (text.find (L'"') == std::wstring::npos, "unsupported quote in Windows path");
    return L"\"" + text + L"\"";
}
}

int wmain (int argc, wchar_t** argv)
{
    try
    {
        if (argc == 4 && std::wstring (argv[1]) == L"--child")
        {
            const auto scenarioWide = std::wstring (argv[3]);
            return child (argv[2], std::string (scenarioWide.begin(), scenarioWide.end()));
        }
        require (argc == 2, "usage: ChimeraVST3LifecycleTests <plugin-module.vst3>");
        require (std::filesystem::is_regular_file (argv[1]), "VST3 module file missing");
        std::array<wchar_t, 32768> executable {};
        require (GetModuleFileNameW (nullptr, executable.data(), static_cast<DWORD> (executable.size())) != 0, "get harness path");
        int failures = 0;
        for (const auto* scenario : { L"headless", L"editor-pumping", L"editor-pending", L"multiple-instances", L"reopen-project", L"live-remove-controller-first", L"live-remove-component-first" })
        {
            const std::wstring name (scenario);
            std::cout << "SCENARIO " << std::string (name.begin(), name.end()) << std::endl;
            auto command = quote (executable.data()) + L" --child " + quote (argv[1]) + L" " + name;
            STARTUPINFOW startup {}; startup.cb = sizeof (startup);
            PROCESS_INFORMATION process {};
            const auto started = GetTickCount64();
            require (CreateProcessW (nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0,
                     "launch lifecycle child");
            const auto wait = WaitForSingleObject (process.hProcess, 45000);
            DWORD exitCode = 1;
            if (wait == WAIT_TIMEOUT)
            {
                // Force-killing is exclusively a failed-test watchdog, never a
                // production cleanup strategy or a way to turn a hang into PASS.
                std::cout << "FAIL watchdog timeout after 45 seconds; last STAGE identifies blocked lifetime operation" << std::endl;
                TerminateProcess (process.hProcess, 124); WaitForSingleObject (process.hProcess, 5000);
                exitCode = 124;
            }
            else if (wait == WAIT_OBJECT_0) GetExitCodeProcess (process.hProcess, &exitCode);
            CloseHandle (process.hThread); CloseHandle (process.hProcess);
            if (exitCode != 0) ++failures;
            std::cout << (exitCode == 0 ? "PASS" : "FAIL") << " scenario=" << std::string (name.begin(), name.end())
                      << " exit=" << exitCode << " elapsed_ms=" << GetTickCount64() - started << std::endl;
        }
        std::cout << "VST3 lifecycle scenarios: " << 7 - failures << "/7 passed" << std::endl;
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
}

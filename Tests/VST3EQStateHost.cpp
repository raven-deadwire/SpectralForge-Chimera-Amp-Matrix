// Test-only Linux VST3 host. Reuses stream/message/parameter queue contracts
// from VST3LifecycleTests.cpp; never links the product or reconstructs its state.
#include <dlfcn.h>
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
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace Steinberg { DEF_CLASS_IID(IPlugView) DEF_CLASS_IID(IPlugFrame) }
namespace {
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
        // Explicit ABI harness identity; this is not a DAW.
        constexpr char title[] = "Chimera EQ ABI State Harness";
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


}
int main(int argc, char** argv) {
    try {
        require(argc == 5 || argc == 6, "usage: host plugin.so output-dir profile(default|pattern|replay|dirty-restore|render|render-replay) input-state(or -) [legacy-lanes.tsv]");
        const std::string profile(argv[3]);
        require(profile=="default" || profile=="pattern" || profile=="replay" || profile=="dirty-restore" || profile=="render" || profile=="render-replay", "unknown profile");
        const std::filesystem::path output(argv[2]);
        std::filesystem::create_directories(output);
        void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module) throw std::runtime_error(dlerror());
        const auto enter = reinterpret_cast<bool(*)(void*)>(dlsym(module, "ModuleEntry"));
        const auto leave = reinterpret_cast<bool(*)()>(dlsym(module, "ModuleExit"));
        const auto factoryFn = reinterpret_cast<IPluginFactory*(PLUGIN_API*)()>(dlsym(module, "GetPluginFactory"));
        require(enter && leave && factoryFn && enter(module), "module entry");
        auto* factory = factoryFn();
        require(factory != nullptr, "factory");
        TUID cid{}; bool found = false;
        for (int i = 0; i < factory->countClasses(); ++i) {
            PClassInfo info{}; factory->getClassInfo(i, &info);
            if (std::strcmp(info.category, kVstAudioEffectClass) == 0) {
                std::memcpy(cid, info.cid, sizeof(TUID)); found = true; break;
            }
        }
        require(found, "audio class");
        auto* host = new Host;
        auto* f3 = query<IPluginFactory3>(factory);
        require(f3->setHostContext(static_cast<IHostApplication*>(host)) == kResultOk, "host context");
        release(f3);
        IComponent* component = nullptr;
        require(factory->createInstance(cid, IComponent::iid.toTUID(), reinterpret_cast<void**>(&component)) == kResultOk, "component");
        require(component->initialize(static_cast<IHostApplication*>(host)) == kResultOk, "initialize");
        TUID controllerID{}; component->getControllerClassId(controllerID);
        IEditController* controller = nullptr;
        require(factory->createInstance(controllerID, IEditController::iid.toTUID(), reinterpret_cast<void**>(&controller)) == kResultOk, "controller");
        require(controller->initialize(static_cast<IHostApplication*>(host)) == kResultOk, "controller init");
        require(controller->setComponentHandler(host) == kResultOk, "handler");
        auto* cc = query<IConnectionPoint>(component); auto* ec = query<IConnectionPoint>(controller);
        require(cc->connect(ec) == kResultOk && ec->connect(cc) == kResultOk, "connect");
        if (std::string(argv[4]) != "-") {
            auto* in = new Stream;
            std::ifstream file(argv[4], std::ios::binary);
            require(bool(file), "input state missing");
            in->bytes.assign(std::istreambuf_iterator<char>(file), {});
            require(!in->bytes.empty(), "input state empty");
            require(component->setState(in) == kResultOk, "setState");
            in->position = 0;
            require(controller->setComponentState(in) == kResultOk, "controller state");
            release(in);
        }
        auto* changes = new Changes;
        std::ofstream contract(output / "parameters.tsv");
        contract << "index\tid\ttitle\tsteps\tdefault\tflags\n" << std::setprecision(17);
        int eqCount = 0;
        std::ofstream lanes(output / "lanes.tsv");
        lanes << "id\tvalue\ttitle\n";
        for (int i = 0; i < controller->getParameterCount(); ++i) {
            ParameterInfo info{};
            require(controller->getParameterInfo(i, info) == kResultOk, "parameter info");
            const auto name = ascii(info.title);
            contract << i << '\t' << info.id << '\t' << name << '\t' << info.stepCount << '\t' << info.defaultNormalizedValue << '\t' << info.flags << '\n';
            if (name.starts_with("Tone EQ ") || name.starts_with("Final EQ ")) ++eqCount;
            if (profile == "pattern" || profile == "dirty-restore") {
                if (name.starts_with("Tone EQ ") || name.starts_with("Final EQ ")) {
                    double value = (i % 11 + 1) / 13.0;
                    if (name.ends_with("bypass")) value = 0;
                    if (name.ends_with("enabled")) value = i % 2 ? 1 : 0;
                    changes->add(info.id, value);
                }
                if (name == "Output gain" || name == "Gate threshold" || name == "Delay mix" || name == "Reverb mix") changes->add(info.id, .42);
            }
            if (name == "Tone EQ 4 frequency" || name == "Final EQ 8 frequency") lanes << info.id << "\t0.61\t" << name << '\n';
            if (name == "Tone EQ 4 gain" || name == "Final EQ 8 gain") lanes << info.id << "\t0.31\t" << name << '\n';
        }
        if (profile == "replay" || profile == "render-replay") {
            require(argc == 6, "replay requires original binary lane IDs");
            std::ifstream laneFile(argv[5]); require(bool(laneFile), "missing legacy lane file");
            std::string line; std::getline(laneFile,line); int count=0;
            while(std::getline(laneFile,line)) {
                const auto tab=line.find('\t'); require(tab!=std::string::npos,"lane format");
                changes->add(static_cast<ParamID>(std::stoul(line.substr(0,tab))),std::stod(line.substr(tab+1))); ++count;
            }
            require(count==4,"expected four saved beta.2 automation lanes");
        }
        require(eqCount == 122, "public binary does not expose the 122 EQ parameters");
        auto* audio = query<IAudioProcessor>(component);
        SpeakerArrangement stereo = SpeakerArr::kStereo;
        require(audio->setBusArrangements(&stereo, 1, &stereo, 1) == kResultOk, "stereo");
        component->activateBus(kAudio, kInput, 0, true); component->activateBus(kAudio, kOutput, 0, true);
        ProcessSetup setup{kRealtime, kSample32, 256, 48000};
        require(audio->setupProcessing(setup) == kResultOk && component->setActive(true) == kResultOk, "prepare");
        const bool rendering = profile == "render" || profile == "render-replay";
        std::ofstream rendered;
        if (rendering) rendered.open(output / "audio.f32", std::ios::binary);
        std::exception_ptr error;
        std::thread worker([&] {
            try {
                require(audio->setProcessing(true) == kResultOk, "start");
                float left[256]{}, right[256]{}, outL[256]{}, outR[256]{};
                float* ins[]{left,right}; float* outs[]{outL,outR};
                AudioBusBuffers input{}, out{}; input.numChannels=out.numChannels=2;
                input.channelBuffers32=ins; out.channelBuffers32=outs;
                ProcessData data{}; data.processMode=kRealtime; data.symbolicSampleSize=kSample32;
                data.numSamples=256; data.numInputs=data.numOutputs=1; data.inputs=&input; data.outputs=&out;
                uint32_t random = 0x13579bdu;
                const int blocks = rendering ? 64 : 4;
                for(int b=0;b<blocks;++b) {
                    if (rendering) for (int n=0;n<256;++n) {
                        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                        left[n] = float(int32_t(random & 0xffffffu) - 0x800000) / 67108864.f;
                        right[n] = -left[n] * .5f;
                    }
                    data.inputParameterChanges = (b == (profile == "render-replay" ? 32 : 0)) ? changes : nullptr;
                    require(audio->process(data)==kResultOk, "process queue");
                    for(int n=0;n<256;++n) {
                        require(std::isfinite(outL[n]) && std::isfinite(outR[n]), "finite stereo output");
                        if (rendering) {
                            const float pair[]{outL[n],outR[n]};
                            rendered.write(reinterpret_cast<const char*>(pair), sizeof pair);
                        }
                    }
                }
                if (rendering) { rendered.close(); require(bool(rendered), "write render"); }
                require(audio->setProcessing(false)==kResultOk,"stop");
            } catch(...) { error=std::current_exception(); }
        });
        worker.join(); if(error) std::rethrow_exception(error);
        require(component->setActive(false)==kResultOk,"deactivate");
        if(profile=="dirty-restore") {
            require(std::string(argv[4])!="-", "dirty restore requires original state");
            auto* again=new Stream;std::ifstream file(argv[4],std::ios::binary);
            again->bytes.assign(std::istreambuf_iterator<char>(file),{});
            require(!again->bytes.empty() && component->setState(again)==kResultOk,"dirty restore");
            again->position=0;require(controller->setComponentState(again)==kResultOk,"dirty controller restore");
            release(again);
        }
        if (rendering) {
            std::ofstream metadata(output / "render.txt");
            metadata << "sample_rate=48000\nblock_size=256\nchannels=2\nframes=16384\nlatency="
                     << audio->getLatencySamples() << "\nactual_daw=NOT_ASSESSED\n";
            std::ofstream normalized(output / "normalized.tsv");
            normalized << "id\tvalue\n" << std::setprecision(17);
            for(int i=0;i<controller->getParameterCount();++i) {
                ParameterInfo info{}; require(controller->getParameterInfo(i,info)==kResultOk,"normalized parameter info");
                normalized << info.id << '\t' << controller->getParamNormalized(info.id) << '\n';
            }
        }
        auto* saved = new Stream;
        require(component->getState(saved)==kResultOk && !saved->bytes.empty(), "getState");
        std::ofstream state(output / "component.bin",std::ios::binary);
        state.write(reinterpret_cast<const char*>(saved->bytes.data()), saved->bytes.size()); state.close();
        require(bool(state), "write state"); release(saved);
        cc->disconnect(ec); ec->disconnect(cc); release(cc);release(ec);
        controller->setComponentHandler(nullptr); controller->terminate(); release(controller);
        component->terminate(); release(audio);release(component);release(changes);
        release(factory); require(leave(),"module exit");dlclose(module);release(host);
        std::cout << "PASS real VST3 state capture, EQ parameters=" << eqCount << " profile=" << argv[3] << '\n';
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}

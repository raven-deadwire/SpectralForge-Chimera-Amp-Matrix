// Diagnostic console workload. This is deliberately not a product acceptance
// target and never changes the frozen OriginalCabIntegrationTests assertions.
#include "CabCallbackTrace.h"
#include "CabTimingDiagnostics.h"
#include "IRLibrary.h"
#include "CabLayoutModel.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if defined(__linux__) || defined(__APPLE__)
 #include <unistd.h>
#endif
#if defined(__linux__)
 #include <sys/resource.h>
#elif defined(__APPLE__)
 #include <mach/mach.h>
 #include <mach/task_info.h>
 #include <mach/thread_info.h>
 #include <pthread.h>
#endif

thread_local bool allocationWatch = false;
thread_local uint64_t watchedAllocations = 0, watchedDeletions = 0;
void* operator new(std::size_t size)
{
    if (allocationWatch) ++watchedAllocations;
    if (void* p = std::malloc(std::max(size, size_t(1)))) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { if (allocationWatch && p) ++watchedDeletions; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace attribution
{
using namespace spectralforge;
constexpr size_t callbacks = 1600, workerCallbacks = 1200, automationCallbacks = 80;
constexpr double sampleRate = 96000;
#ifndef CHIMERA_ATTRIBUTION_BLOCK_SIZE
#define CHIMERA_ATTRIBUTION_BLOCK_SIZE 64
#endif
constexpr int blockSize = CHIMERA_ATTRIBUTION_BLOCK_SIZE;
static_assert(blockSize == 32 || blockSize == 64 || blockSize == 256);
constexpr size_t eventCapacity = callbacks * 256;
constexpr size_t workerEventCapacity = 65536;
constexpr size_t callerEventCapacity = 16384;
constexpr size_t memoryWordsPerSlot = 65536;
constexpr unsigned arithmeticIterations = 256;
constexpr unsigned memoryTouchesPerSlot = 256;
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
constexpr bool hooksCompiled = true;
#else
constexpr bool hooksCompiled = false;
#endif

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Options
{
    std::string mode = "cab", traceLevel = "stages", engine = "all", cadence = "legacy";
    int channels = 0;
    bool canonicalReset = false;
    std::filesystem::path directory = "cab-callback-attribution";
};

Options parse(int argc, char** argv)
{
    Options options;
    if (!hooksCompiled) options.traceLevel = "off";
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        require(i + 1 < argc, "Every argument requires a value");
        const std::string value = argv[++i];
        if (argument == "--mode") options.mode = value;
        else if (argument == "--trace-level") options.traceLevel = value;
        else if (argument == "--engine") options.engine = value;
        else if (argument == "--cadence") options.cadence = value;
        else if (argument == "--canonical-reset") {
            require(value == "yes" || value == "no", "--canonical-reset must be yes or no");
            options.canonicalReset = value == "yes";
        }
        else if (argument == "--channels")
        {
            require(value == "1" || value == "2" || value == "all", "--channels must be 1, 2 or all");
            options.channels = value == "all" ? 0 : std::stoi(value);
        }
        else if (argument == "--output-dir") options.directory = value;
        else throw std::runtime_error("Unknown probe argument: " + argument);
    }
    require(options.mode == "cab" || options.mode == "arithmetic" || options.mode == "memory"
        || options.mode == "clock", "--mode must be cab, arithmetic, memory or clock");
    require(options.traceLevel == "off" || options.traceLevel == "stages" || options.traceLevel == "resources",
        "--trace-level must be off, stages or resources");
    require(hooksCompiled || options.traceLevel == "off", "The compile-off control supports only --trace-level off");
    require(options.engine == "all" || options.engine == "original-v1" || options.engine == "expanded-v2"
        || options.engine == "array-v3-6x10", "Unknown CAB engine");
    require(options.cadence == "legacy" || options.cadence == "continuous", "Unknown callback cadence");
    return options;
}

// Both binaries use identical outer clocks. Resource and trace sampling can
// never disappear into an unexplained wall-minus-CPU subtraction. Raw clock
// samples and each distinct envelope are written without offline correction.
using Clock = cabTrace::ClockSample;
int64_t wallDelta(const Clock& before, const Clock& after) noexcept
{
    return before.wallValid && after.wallValid && after.wallNs >= before.wallNs
        ? int64_t(after.wallNs - before.wallNs) : -1;
}
int64_t cpuDelta(const Clock& before, const Clock& after) noexcept
{
    return before.cpuValid && after.cpuValid && after.cpuNs >= before.cpuNs
        ? int64_t(after.cpuNs - before.cpuNs) : -1;
}

struct ResourceSample
{
    Clock queryStart{}, queryEnd{};
    bool valid = false, threadStateValid = false;
    int error = 0, threadStateError = 0;
    int64_t minorFaults = -1, majorFaults = -1, voluntarySwitches = -1, involuntarySwitches = -1;
    int64_t taskFaults = -1, taskPageins = -1;
    int runState = -1, suspendCount = -1;
};

const char* resourceScope() noexcept
{
#if defined(__linux__)
    return "thread_rusage";
#elif defined(__APPLE__)
    return "process_task_events";
#else
    return "unavailable";
#endif
}

ResourceSample resources() noexcept
{
    ResourceSample result;
    result.queryStart = cabTrace::clockSample();
#if defined(__linux__)
    rusage usage{};
    if (::getrusage(RUSAGE_THREAD, &usage) == 0)
    {
        result.valid = true;
        result.minorFaults = usage.ru_minflt;
        result.majorFaults = usage.ru_majflt;
        result.voluntarySwitches = usage.ru_nvcsw;
        result.involuntarySwitches = usage.ru_nivcsw;
    }
    else result.error = errno;
#elif defined(__APPLE__)
    // Darwin marks rusage fault counters NU. TASK_EVENTS_INFO reports actual
    // task-wide faults/pageins instead: neither is labelled thread/minor/major.
    // A concurrent IR worker may contribute to these task-level counters.
    task_events_info_data_t events{};
    mach_msg_type_number_t eventCount = TASK_EVENTS_INFO_COUNT;
    const auto eventStatus = task_info(mach_task_self(), TASK_EVENTS_INFO,
        reinterpret_cast<task_info_t>(&events), &eventCount);
    result.error = int(eventStatus);
    if (eventStatus == KERN_SUCCESS && eventCount >= TASK_EVENTS_INFO_COUNT)
    {
        result.valid = true;
        result.taskFaults = events.faults;
        result.taskPageins = events.pageins;
    }
    thread_basic_info_data_t basic{};
    mach_msg_type_number_t basicCount = THREAD_BASIC_INFO_COUNT;
    const auto basicStatus = thread_info(pthread_mach_thread_np(pthread_self()), THREAD_BASIC_INFO,
        reinterpret_cast<thread_info_t>(&basic), &basicCount);
    result.threadStateError = int(basicStatus);
    if (basicStatus == KERN_SUCCESS && basicCount >= THREAD_BASIC_INFO_COUNT)
    {
        result.threadStateValid = true;
        result.runState = basic.run_state;
        result.suspendCount = basic.suspend_count;
    }
#else
    result.error = -1;
#endif
    result.queryEnd = cabTrace::clockSample();
    return result;
}

int64_t resourceDelta(int64_t before, int64_t after) noexcept
{
    return before >= 0 && after >= before ? after - before : -1;
}

struct CallbackRow
{
    size_t index{};
    Clock entry{}, bodyStart{}, bodyEnd{}, exit{};
    Clock sleepStart{}, sleepEnd{};
    ResourceSample resourceBefore{}, resourceAfter{};
    uint64_t allocations{}, deletions{}, checksum{};
    size_t traceFirst{}, traceCount{};
    // Set by trace API snapshots after CallbackCapture::stop().
    uint64_t workerSequenceBefore{}, workerSequenceAfter{};
    bool workerSnapshotBeforeValid{}, workerSnapshotAfterValid{};
    int workerStateBefore{}, workerStateAfter{}, workerLaneBefore = -1, workerLaneAfter = -1;
    int workerMicBefore = -1, workerMicAfter = -1;
};

struct SlotRow
{
    size_t callbackIndex{};
    int lane{}, mic{};
    bool after{};
    uint64_t requestedKey{}, activeKey{}, fadingKey{};
    unsigned activeGeneration{}, fadingGeneration{}, rejected{};
    bool activePresent{}, fadingPresent{}, pendingPresent{};
    int fadeRemaining{}, fadeLength{};
};

struct PageTouchConfiguration
{
    // If a native page-size query fails, touching every byte still covers all
    // pages. The fallback is deliberately explicit rather than guessing a
    // page size that could skip pages on an unfamiliar platform.
    size_t strideBytes = 1, nativePageSizeBytes = 0;
};

PageTouchConfiguration pageTouchConfiguration() noexcept
{
#if defined(_WIN32)
    SYSTEM_INFO information{};
    ::GetSystemInfo(&information);
    if (information.dwPageSize > 0)
        return {size_t(information.dwPageSize), size_t(information.dwPageSize)};
#elif defined(__linux__) || defined(__APPLE__)
    const auto pageSize = ::sysconf(_SC_PAGESIZE);
    if (pageSize > 0) return {size_t(pageSize), size_t(pageSize)};
#endif
    return {};
}

template<class T>
size_t touchPreparedStorage(std::vector<T>& storage, size_t strideBytes) noexcept
{
    if (storage.empty()) return 0;
    auto* bytes = reinterpret_cast<volatile unsigned char*>(storage.data());
    const auto byteCount = storage.size() * sizeof(T);
    size_t operations = 0;
    // A volatile read/write preserves every object's representation and makes
    // first writes observable even when zero initialization used demand-zero
    // storage. The final byte also covers a last partially occupied page when
    // the vector allocation is not page aligned.
    for (size_t offset = 0; offset < byteCount;)
    {
        bytes[offset] = bytes[offset];
        ++operations;
        if (byteCount - offset <= strideBytes) break;
        offset += strideBytes;
    }
    bytes[byteCount - 1] = bytes[byteCount - 1];
    ++operations;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return operations;
}

struct Route
{
    const Options& options;
    std::string engine;
    int channels{};
    std::vector<CallbackRow> rows{callbacks};
    std::vector<cabTrace::Event> events{eventCapacity}, workerEvents{workerEventCapacity}, callerEvents{callerEventCapacity};
    std::vector<SlotRow> slots{callbacks * 12};
    std::vector<uint32_t> memory = std::vector<uint32_t>(6 * memoryWordsPerSlot);
    cabTrace::Buffer trace{}, workerTrace{}, callerTrace{};
    PageTouchConfiguration pageTouch{};
    size_t pageTouchOperations{};
    size_t rowsWritten{}, slotsWritten{}, unmeasuredProcessCalls{};
    uint64_t totalAllocations{}, totalDeletions{}, checksum{};
    double kernelResidual{}, peak{}, maxStep{};
    float previous{};
    bool semanticValid = false, workerSchedulingValid = false;
    unsigned cancelledBuilds{};
    int observedWorkerPriority{};
    bool workerStartSucceeded{}, workerEnteredRun{};

    Route(const Options& o, std::string e, int ch) : options(o), engine(std::move(e)), channels(ch)
    {
        trace.events = events.data(); trace.capacity = events.size();
        workerTrace.events = workerEvents.data(); workerTrace.capacity = workerEvents.size();
        callerTrace.events = callerEvents.data(); callerTrace.capacity = callerEvents.size();
        for (size_t i = 0; i < memory.size(); ++i) memory[i] = uint32_t(i * 2654435761u);
        // Both binaries touch every prepared recorder/control array before
        // measurement. No memory is locked; later residency is not guaranteed.
        pageTouch = pageTouchConfiguration();
        pageTouchOperations += touchPreparedStorage(events, pageTouch.strideBytes);
        pageTouchOperations += touchPreparedStorage(workerEvents, pageTouch.strideBytes);
        pageTouchOperations += touchPreparedStorage(callerEvents, pageTouch.strideBytes);
        pageTouchOperations += touchPreparedStorage(rows, pageTouch.strideBytes);
        pageTouchOperations += touchPreparedStorage(slots, pageTouch.strideBytes);
        pageTouchOperations += touchPreparedStorage(memory, pageTouch.strideBytes);
    }
    bool traced() const noexcept { return hooksCompiled && options.traceLevel != "off"; }
    bool resourceEnabled() const noexcept { return options.traceLevel == "resources"; }
    juce::dsp::ProcessSpec spec() const { return {sampleRate, blockSize, juce::uint32(channels)}; }
    std::filesystem::path prefix() const
    {
        return options.directory / (options.mode + "-" + engine + "-96000-" + std::to_string(blockSize) + "-"
            + std::to_string(channels) + "-" + options.traceLevel);
    }
    uint64_t key(originalCab::Settings settings) const
    {
        if (engine == "array-v3-6x10")
            return cabLayout::key({{settings, 9, settings.mic == 1 ? 10 : 20, 2}, 7,
                settings.mic == 1 ? 7 : settings.unit});
        if (engine == "expanded-v2")
            return cabExpansion::key({settings, 12, settings.mic == 1 ? 10 : 20, 2});
        return originalCab::key(settings);
    }
};

void processUnmeasured(Route& route, Cab& cab, juce::AudioBuffer<float>& buffer)
{
    allocationWatch = true;
    cab.process(buffer);
    allocationWatch = false;
    ++route.unmeasuredProcessCalls;
    require(cab.transitioningMicCount() <= 1, "modeled A/B crossfades must be serialized per rig");
}

std::vector<float> render(Route& route, Cab& cab)
{
    juce::AudioBuffer<float> audio(route.channels, blockSize);
    for (int n = 0; n < 200; ++n) { audio.clear(); processUnmeasured(route, cab, audio); }
    cab.reset();
    const int responseBlocks = 150;
    std::vector<float> result; result.reserve(responseBlocks * blockSize);
    for (int block = 0; block < responseBlocks; ++block)
    {
        audio.clear(); if (block == 0) audio.setSample(0, 0, .25f);
        processUnmeasured(route, cab, audio);
        for (int n = 0; n < blockSize; ++n) result.push_back(audio.getSample(0, n));
        if (route.channels == 2)
            require(audio.getMagnitude(1, 0, blockSize) < 1e-8, "no stereo crosstalk");
    }
    return result;
}

double delta(const std::vector<float>& a, const std::vector<float>& b)
{
    require(a.size() == b.size(), "response length mismatch");
    double difference = 0;
    for (size_t i = 0; i < a.size(); ++i) difference = std::max(difference, std::abs(double(a[i] - b[i])));
    return difference;
}

void recordSlots(Route& route, const std::array<Cab, 3>& cabs, size_t callback, bool after)
{
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
    if (!route.traced()) return;
    for (int lane = 0; lane < 3; ++lane)
        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& cab = mic == 0 ? cabs[size_t(lane)] : *cabs[size_t(lane)].secondMic();
            const auto state = cab.traceSnapshot();
            require(route.slotsWritten < route.slots.size(), "Slot trace overflow");
            auto& row = route.slots[route.slotsWritten++];
            row.callbackIndex = callback; row.lane = lane; row.mic = mic; row.after = after;
            row.requestedKey = state.requestedModel; row.activeKey = state.activeModel; row.fadingKey = state.fadingModel;
            row.activeGeneration = state.activeGeneration; row.fadingGeneration = state.fadingGeneration;
            row.pendingPresent = state.pendingPresent; row.fadeRemaining = state.fadeRemaining;
            row.fadeLength = state.fadeLength; row.rejected = state.rejected;
            row.activePresent = state.activePresent; row.fadingPresent = state.fadingPresent;
        }
#else
    (void) route; (void) cabs; (void) callback; (void) after;
#endif
}

void observeOutput(Route& route, const juce::AudioBuffer<float>& audio)
{
    for (int n = 0; n < blockSize; ++n)
    {
        const float value = audio.getSample(0, n);
        require(std::isfinite(value), "finite swap output");
        route.peak = std::max(route.peak, std::abs(double(value)));
        route.maxStep = std::max(route.maxStep, std::abs(double(value - route.previous)));
        route.previous = value;
    }
}

uint64_t bufferChecksum(const std::array<juce::AudioBuffer<float>, 3>& lanes) noexcept
{
    uint64_t result = UINT64_C(1469598103934665603);
    for (const auto& lane : lanes)
        for (int ch = 0; ch < lane.getNumChannels(); ++ch)
            for (int n = 0; n < lane.getNumSamples(); ++n)
            {
                uint32_t value{};
                const float sample = lane.getSample(ch, n);
                std::memcpy(&value, &sample, sizeof(value));
                result = (result ^ value) * UINT64_C(1099511628211);
            }
    return result;
}

template<class Work>
void measure(Route& route, size_t index, const std::array<Cab, 3>* cabs,
             const IRLibrary* library, Work&& work)
{
    auto& row = route.rows[index]; row.index = index;
    row.entry = cabTrace::clockSample();
    const auto allocationsBefore = watchedAllocations, deletionsBefore = watchedDeletions;
    if (cabs) recordSlots(route, *cabs, index, false);
    if (route.resourceEnabled()) row.resourceBefore = resources();
    row.traceFirst = route.trace.used;
    row.bodyStart = cabTrace::clockSample();
    allocationWatch = true;
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
    cabTrace::CallbackCapture capture(route.traced() ? &route.trace : nullptr, index,
        library && route.traced() ? &library->workerProgress() : nullptr);
#endif
    work();
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
    capture.stop();
    row.workerSequenceBefore = capture.workerEntry.sequence;
    row.workerSequenceAfter = capture.workerExit.sequence;
    row.workerSnapshotBeforeValid = capture.workerEntry.valid;
    row.workerSnapshotAfterValid = capture.workerExit.valid;
    row.workerStateBefore = int(capture.workerEntry.state); row.workerStateAfter = int(capture.workerExit.state);
    row.workerLaneBefore = capture.workerEntry.lane; row.workerLaneAfter = capture.workerExit.lane;
    row.workerMicBefore = capture.workerEntry.mic; row.workerMicAfter = capture.workerExit.mic;
#else
    (void) library;
#endif
    allocationWatch = false;
    row.bodyEnd = cabTrace::clockSample();
    row.traceCount = route.trace.used - row.traceFirst;
    if (route.resourceEnabled()) row.resourceAfter = resources();
    if (cabs) recordSlots(route, *cabs, index, true);
    row.allocations = watchedAllocations - allocationsBefore; row.deletions = watchedDeletions - deletionsBefore;
    row.exit = cabTrace::clockSample();
    ++route.rowsWritten;
}

void cab(Route& route)
{
    if (route.engine == "array-v3-6x10")
    {
        originalCab::Settings preview{}; preview.cabinet = 1; preview.mic = 1;
        const auto restored = cabLayout::settings(route.key(preview));
        require(cabLayout::geometry(restored).count == 6 && cabLayout::effectiveUnit(restored) == 5,
            "production 6x10 / previous bottom-right microphone migration");
    }
    const auto spec = route.spec();
    std::array<Cab, 3> cabs;
    for (int lane = 0; lane < 3; ++lane)
    {
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
        cabs[size_t(lane)].setTraceContext(lane);
#endif
        cabs[size_t(lane)].prepare(spec);
    }
    IRLibrary library({&cabs[0], &cabs[1], &cabs[2]});
    originalCab::Settings a{true, 1, 0, 0, 0, .3, .25, 10}, b = a;
    b.unit = 1; b.mic = 1; b.distanceCm = 18;
    for (auto& c : cabs)
    {
        c.requestedModel = route.key(a); c.secondMic()->requestedModel = route.key(b); c.blend = .5;
    }
    library.prepare(spec, {0, 0, 0}); library.stop();
    auto& first = cabs[0]; first.blend = 0; const auto outA = render(route, first);
    first.blend = 1; const auto outB = render(route, first);
    require(delta(outA, outB) > 1e-4, "independent same cabinet / different unit / mic");
    first.blend = .5; const auto mix = render(route, first); auto expected = outA;
    for (size_t i = 0; i < expected.size(); ++i) expected[i] = .5f * (outA[i] + outB[i]);
    require(delta(mix, expected) < 2e-6, "modeled constant-sum blend");
    first.secondMic()->requestedModel = route.key(a); library.prepare(spec, {0, 0, 0}); library.stop();
    require(delta(outA, render(route, first)) < 2e-6, "same unit and same mic unity at midpoint");
    for (auto& c : cabs) c.blend = .5;
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
    // Earlier prepares belong to unmeasured response checks. Start this single
    // worker trace before the final prepare, retaining its complete run/exit.
    library.traceSink = route.traced() ? &route.workerTrace : nullptr;
#endif
    library.prepare(spec, {0, 0, 0});
    juce::AudioBuffer<float> audio(route.channels, blockSize);
    std::array<juce::AudioBuffer<float>, 3> lanes;
    for (auto& lane : lanes) lane.setSize(route.channels, blockSize);
    for (size_t n = 0; n < workerCallbacks; ++n)
    {
        if (n < automationCallbacks)
            for (auto& c : cabs)
            {
                a.position = double(n % 60) / 60; c.requestedModel = route.key(a);
                b.distanceCm = 5 + double(n) * .2; c.secondMic()->requestedModel = route.key(b);
            }
        for (int ch = 0; ch < route.channels; ++ch)
            for (int k = 0; k < blockSize; ++k)
                audio.setSample(ch, k, float(.05 * std::sin(double(n * blockSize + size_t(k)) * .07)));
        for (auto& lane : lanes) lane.makeCopyOf(audio, true);
        measure(route, n, &cabs, &library, [&] {
            for (int lane = 0; lane < 3; ++lane)
            {
                cabs[size_t(lane)].process(lanes[size_t(lane)]);
                require(cabs[size_t(lane)].transitioningMicCount() <= 1,
                    "modeled A/B crossfades must be serialized per rig");
            }
        });
        observeOutput(route, lanes[0]);
        route.rows[n].checksum = bufferChecksum(lanes); route.checksum ^= route.rows[n].checksum;
        if (n % 8 == 0 && route.options.cadence == "legacy") {
            route.rows[n].sleepStart = cabTrace::clockSample();
            juce::Thread::sleep(1);
            route.rows[n].sleepEnd = cabTrace::clockSample();
        }
    }
    bool ready = false;
    for (int n = 0; n < 10000 && !ready; ++n)
    {
        ready = true;
        for (auto& c : cabs)
        {
            audio.clear(); processUnmeasured(route, c, audio);
            ready = ready && c.activeModel == c.requestedModel && c.secondMic()->activeModel == c.secondMic()->requestedModel;
        }
        if (!ready) juce::Thread::sleep(1);
    }
    require(ready, "latest generation converges after automation storm");
    require(route.peak < 2 && route.maxStep < .3, "bounded swap transition");
    for (int n = 0; n < 200; ++n)
        for (auto& c : cabs) { audio.clear(); processUnmeasured(route, c, audio); }
    for (auto& c : cabs) require(c.transitioningMicCount() == 0, "pre-publication fades drained");
    library.stop();
    // Optional diagnostic replay boundary, entirely outside measurement.
    // reset() alone retains the mic swap preference from asynchronous history.
    // Reprepare and install identical outgoing IRs so both private histories and
    // swap order are fixed. Share only the two immutable spectra, as the worker
    // cache does. No product/default acceptance path uses this intervention.
    if (route.options.canonicalReset) {
        std::array<uint64_t,2> keys{route.key(a),route.key(b)};
        std::array<juce::AudioBuffer<float>,2> waves;
        std::array<std::shared_ptr<const ModeledCabConvolution::Prepared>,2> prepared;
        for (int mic=0;mic<2;++mic) {
            const auto data=cabLayout::generate(keys[mic],sampleRate);
            waves[mic].setSize(1,int(data.size()));waves[mic].copyFrom(0,0,data.data(),int(data.size()));
            prepared[mic]=std::make_shared<const ModeledCabConvolution::Prepared>(waves[mic],blockSize,route.channels==1);
        }
        for (auto& c : cabs) {
            c.prepare(spec);
            for (int mic=0;mic<2;++mic) {
                auto* slot=mic==0 ? &c : c.secondMic();
                juce::AudioBuffer<float> data;data.makeCopyOf(waves[mic]);
                slot->requestedModel=keys[mic];
                slot->install(std::make_unique<Cab::Kernel>(std::move(data),sampleRate,spec,0,1,keys[mic],prepared[mic]));
            }
        }
    }
    a.position = .85; const auto nextKey = route.key(a); const auto wave = cabLayout::generate(nextKey, sampleRate);
    juce::AudioBuffer<float> referenceSamples(1, int(wave.size()));
    referenceSamples.copyFrom(0, 0, wave.data(), int(wave.size()));
    Cab::Kernel reference(std::move(referenceSamples), sampleRate, spec, 0, 1, nextKey);
    require(reference.getLatency() == 0, "modeled convolution adds processing latency");
    juce::AudioBuffer<float> impulse(route.channels, blockSize);
    for (int offset = 0; offset < int(wave.size()) + blockSize; offset += blockSize)
    {
        impulse.clear(); if (offset == 0) impulse.setSample(0, 0, 1);
        {
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
            // The exact authored impulse/channel oracle also runs with actual
            // recording enabled, not merely with instrumentation compiled.
            cabTrace::ThreadCapture referenceCapture(route.traced() ? &route.callerTrace : nullptr);
            cabTrace::ContextScope referenceContext(cabTrace::Context{-1, -1, nextKey, 1, cabTrace::Role::Active});
#endif
            allocationWatch = true; reference.process(impulse); allocationWatch = false;
        }
        for (int k = 0; k < blockSize; ++k)
            route.kernelResidual = std::max(route.kernelResidual,
                std::abs(double(impulse.getSample(0, k)) - (offset + k < int(wave.size()) ? wave[size_t(offset + k)] : 0)));
        if (route.channels == 2) require(impulse.getMagnitude(1, 0, blockSize) < 1e-8, "prepared kernel stereo crosstalk");
    }
    require(route.kernelResidual < 3e-6, "prepared kernel differs from generated acoustic response");
    {
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
        // The joined worker and the caller have different thread CPU clocks.
        // Caller-prepared publications use a distinct stream; never combine
        // their CPU endpoints into the IR worker's event timeline.
        cabTrace::ThreadCapture publishCapture(route.traced() ? &route.callerTrace : nullptr);
#endif
        for (int lane = 0; lane < 3; ++lane)
            for (int mic = 0; mic < 2; ++mic)
            {
                auto* slot = mic == 0 ? &cabs[size_t(lane)] : cabs[size_t(lane)].secondMic();
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
                cabTrace::ContextScope publicationContext(cabTrace::Context{lane, mic, nextKey, 100, cabTrace::Role::Pending});
#endif
                juce::AudioBuffer<float> samples(1, int(wave.size())); samples.copyFrom(0, 0, wave.data(), int(wave.size()));
                slot->requestedModel = nextKey;
                slot->publish(std::make_unique<Cab::Kernel>(std::move(samples), sampleRate, spec, 0, 100, nextKey));
            }
    }
    for (size_t n = workerCallbacks; n < callbacks; ++n)
    {
        for (auto& lane : lanes)
            for (int ch = 0; ch < route.channels; ++ch)
                for (int k = 0; k < blockSize; ++k)
                    lane.setSample(ch, k, float(.05 * std::sin(double(n * blockSize + size_t(k)) * .07)));
        measure(route, n, &cabs, &library, [&] {
            for (int lane = 0; lane < 3; ++lane)
            {
                cabs[size_t(lane)].process(lanes[size_t(lane)]);
                require(cabs[size_t(lane)].transitioningMicCount() <= 1,
                    "modeled A/B crossfades must be serialized per rig");
            }
        });
        observeOutput(route, lanes[0]);
        route.rows[n].checksum = bufferChecksum(lanes); route.checksum ^= route.rows[n].checksum;
    }
    require(route.peak < 2 && route.maxStep < .3, "bounded six-slot swap");
    for (auto& c : cabs)
        require(c.activeModel == nextKey && c.secondMic()->activeModel == nextKey, "six queued mic swaps converge");
    for (auto& c : cabs) c.clear();
    require(library.resourcesReleased(), "worker teardown");
    const auto scheduling = library.workerScheduling();
    route.workerStartSucceeded = scheduling.startSucceeded; route.workerEnteredRun = scheduling.enteredRun;
    route.observedWorkerPriority = scheduling.observedPriority;
    route.workerSchedulingValid = scheduling.startSucceeded && scheduling.enteredRun
        && scheduling.observedPriority == int(juce::Thread::Priority::low);
    require(route.workerSchedulingValid, "IR worker did not retain the low priority request/observation");
    route.cancelledBuilds = library.cancelledBuildCount();
    route.semanticValid = true;
}

void control(Route& route)
{
    for (size_t n = 0; n < callbacks; ++n)
    {
        uint64_t checksum = 0;
        measure(route, n, nullptr, nullptr, [&] {
            std::atomic_signal_fence(std::memory_order_seq_cst);
            if (route.options.mode == "arithmetic")
            {
                cabTrace::Scope arithmetic(cabTrace::Stage::ArithmeticControl);
                volatile uint64_t value = UINT64_C(0x9e3779b97f4a7c15) ^ uint64_t(n);
                for (unsigned step = 0; step < arithmeticIterations; ++step)
                    value = (value ^ (value >> 13)) * UINT64_C(0x2545f4914f6cdd1d) + step;
                checksum = value;
            }
            else if (route.options.mode == "memory")
            {
                cabTrace::Scope memory(cabTrace::Stage::MemoryControl,
                    6 * memoryTouchesPerSlot * 2 * sizeof(uint32_t));
                // Deterministic six independent memory regions, bounded 64 B
                // stride. Logical traffic is known; cache misses/DRAM traffic
                // are NOT inferred from these source-level accesses.
                auto* volatileMemory = static_cast<volatile uint32_t*>(route.memory.data());
                for (size_t slot = 0; slot < 6; ++slot)
                    for (unsigned touch = 0; touch < memoryTouchesPerSlot; ++touch)
                    {
                        const size_t offset = slot * memoryWordsPerSlot
                            + ((n * 4099 + size_t(touch) * 16) % memoryWordsPerSlot);
                        const auto value = volatileMemory[offset] + uint32_t(n + touch + 1);
                        volatileMemory[offset] = value; checksum ^= value;
                    }
            }
            else
            {
                // Empty nested scopes calibrate recording and clock overhead.
                // They use exactly the same event format/storage and APIs as
                // CAB scopes, without pretending to reproduce DSP execution.
#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
                if (route.traced())
                    for (unsigned scope = 0; scope < 64; ++scope)
                    {
                        cabTrace::Scope empty(cabTrace::Stage::ScopeCalibration);
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                    }
#endif
                checksum = n;
            }
            std::atomic_signal_fence(std::memory_order_seq_cst);
        });
        route.rows[n].checksum = checksum; route.checksum ^= checksum;
        if (n < workerCallbacks && n % 8 == 0 && route.options.cadence == "legacy") {
            route.rows[n].sleepStart = cabTrace::clockSample();
            juce::Thread::sleep(1);
            route.rows[n].sleepEnd = cabTrace::clockSample();
        }
    }
    route.semanticValid = true;
}

std::ofstream output(const std::filesystem::path& path)
{
    require(!std::filesystem::exists(path), "Refusing to overwrite attribution evidence");
    std::ofstream stream(path, std::ios::out);
    require(bool(stream), "Cannot create attribution evidence");
    stream << std::setprecision(17);
    return stream;
}
void close(std::ofstream& stream)
{
    stream.close(); require(bool(stream), "Attribution evidence write/flush failed");
}

void writeEvents(const std::filesystem::path& path, const cabTrace::Buffer& buffer)
{
    auto stream = output(path);
    stream << "schema_version,event_id,parent_id,depth,callback_index,lane,mic,stage,role,model_key,generation,logical_bytes,"
        "wall_start_ns,wall_end_ns,cpu_start_ns,cpu_end_ns,wall_valid,cpu_valid,wall_ns,cpu_ns,"
        "wall_start_error,wall_end_error,cpu_start_error,cpu_end_error,complete,worker_sequence_start,worker_sequence_end,"
        "wall_start_valid,wall_end_valid,cpu_start_valid,cpu_end_valid,wait_requested_ns,wait_notified\n";
    for (size_t i = 0; i < buffer.used; ++i)
    {
        const auto& event = buffer.events[i];
        stream << "1," << event.eventId << ',' << event.parentId << ',' << event.depth << ',' << event.callbackId << ','
            << event.lane << ',' << event.mic << ',' << cabTrace::name(event.stage) << ',' << cabTrace::name(event.role) << ','
            << event.modelKey << ',' << event.generation << ',' << event.logicalBytes << ','
            << event.start.wallNs << ',' << event.end.wallNs << ',' << event.start.cpuNs << ',' << event.end.cpuNs << ','
            << (wallDelta(event.start, event.end) >= 0) << ',' << (cpuDelta(event.start, event.end) >= 0) << ','
            << wallDelta(event.start, event.end) << ',' << cpuDelta(event.start, event.end) << ','
            << event.start.wallError << ',' << event.end.wallError << ',' << event.start.cpuError << ',' << event.end.cpuError << ','
            << event.complete << ',' << event.workerSequenceStart << ',' << event.workerSequenceEnd << ','
            << event.start.wallValid << ',' << event.end.wallValid << ',' << event.start.cpuValid << ',' << event.end.cpuValid
            << ',' << event.waitRequestedNs << ',' << event.waitNotified << '\n';
    }
    close(stream);
}

void write(Route& route)
{
    std::filesystem::create_directories(route.options.directory);
    const auto prefix = route.prefix().string();
    auto stream = output(prefix + ".callbacks.csv");
    stream << "schema_version,mode,engine,channels,sample_rate,block_size,callback_index,phase,phase_index,"
        "automation_requested,publication_requested,wall_start_ns,wall_end_ns,wall_ns,cpu_start_ns,cpu_end_ns,cpu_ns,"
        "cpu_valid,wall_valid,wall_start_error,wall_end_error,cpu_start_error,cpu_end_error,deadline_ns,deadline_miss,"
        "entry_wall_ns,exit_wall_ns,entry_cpu_ns,exit_cpu_ns,envelope_wall_ns,envelope_cpu_ns,"
        "worker_seq_before,worker_seq_after,worker_state_before,worker_state_after,worker_lane_before,worker_lane_after,"
        "worker_mic_before,worker_mic_after,trace_event_first,trace_event_count,resource_scope,resource_valid,"
        "minor_faults_delta,major_faults_delta,voluntary_switches_delta,involuntary_switches_delta,"
        "task_faults_delta,task_pageins_delta,thread_state_valid,thread_run_state_before,thread_run_state_after,"
        "thread_suspend_count_before,thread_suspend_count_after,resource_before_error,resource_after_error,"
        "thread_state_before_error,thread_state_after_error,resource_before_start_wall_ns,resource_before_end_wall_ns,"
        "resource_after_start_wall_ns,resource_after_end_wall_ns,resource_before_query_wall_ns,resource_after_query_wall_ns,"
        "allocations,deletions,checksum,wall_start_valid,wall_end_valid,cpu_start_valid,cpu_end_valid,"
        "entry_wall_valid,exit_wall_valid,entry_cpu_valid,exit_cpu_valid,entry_wall_error,exit_wall_error,entry_cpu_error,exit_cpu_error,"
        "worker_snapshot_before_valid,worker_snapshot_after_valid,resource_before_valid,resource_after_valid,"
        "minor_faults_before,minor_faults_after,major_faults_before,major_faults_after,"
        "voluntary_switches_before,voluntary_switches_after,involuntary_switches_before,involuntary_switches_after,"
        "task_faults_before,task_faults_after,task_pageins_before,task_pageins_after";
    for (const char* endpoint : {"resource_before_start", "resource_before_end", "resource_after_start", "resource_after_end"})
        stream << ',' << endpoint << "_cpu_ns," << endpoint << "_wall_valid," << endpoint << "_cpu_valid,"
            << endpoint << "_wall_error," << endpoint << "_cpu_error";
    stream << '\n';
    const bool isCab = route.options.mode == "cab";
    uint64_t clockErrors = route.trace.clockErrors + route.workerTrace.clockErrors + route.callerTrace.clockErrors;
    size_t misses = 0;
    for (size_t i = 0; i < route.rowsWritten; ++i)
    {
        const auto& row = route.rows[i]; const auto& before = row.resourceBefore; const auto& after = row.resourceAfter;
        const auto wall = wallDelta(row.bodyStart, row.bodyEnd), cpu = cpuDelta(row.bodyStart, row.bodyEnd);
        const bool miss = double(wall) > 1e9 * double(blockSize) / sampleRate; misses += miss;
        clockErrors += wall < 0 || cpu < 0 || wallDelta(row.entry, row.exit) < 0 || cpuDelta(row.entry, row.exit) < 0;
        if (route.resourceEnabled())
            clockErrors += wallDelta(before.queryStart, before.queryEnd) < 0 || cpuDelta(before.queryStart, before.queryEnd) < 0
                || wallDelta(after.queryStart, after.queryEnd) < 0 || cpuDelta(after.queryStart, after.queryEnd) < 0;
        const char* phase = i < workerCallbacks
            ? (isCab ? "worker_and_automation" : "paced_control")
            : (isCab ? "forced_six_slot_publication" : "burst_control");
        stream << "1," << route.options.mode << ',' << route.engine << ',' << route.channels << ",96000," << blockSize << "," << row.index
            << ',' << phase << ',' << (i < workerCallbacks ? i : i - workerCallbacks) << ',' << (isCab && i < automationCallbacks)
            << ',' << (isCab && i == workerCallbacks) << ',' << row.bodyStart.wallNs << ',' << row.bodyEnd.wallNs << ','
            << wall << ',' << row.bodyStart.cpuNs << ',' << row.bodyEnd.cpuNs << ',' << cpu << ',' << (cpu >= 0) << ',' << (wall >= 0)
            << ',' << row.bodyStart.wallError << ',' << row.bodyEnd.wallError << ',' << row.bodyStart.cpuError << ',' << row.bodyEnd.cpuError
            << ',' << 1e9 * double(blockSize) / sampleRate << ',' << miss << ',' << row.entry.wallNs << ',' << row.exit.wallNs
            << ',' << row.entry.cpuNs << ',' << row.exit.cpuNs << ',' << wallDelta(row.entry, row.exit) << ',' << cpuDelta(row.entry, row.exit)
            << ',' << row.workerSequenceBefore << ',' << row.workerSequenceAfter << ',' << row.workerStateBefore << ',' << row.workerStateAfter
            << ',' << row.workerLaneBefore << ',' << row.workerLaneAfter << ',' << row.workerMicBefore << ',' << row.workerMicAfter
            << ',' << row.traceFirst << ',' << row.traceCount << ',' << (route.resourceEnabled() ? resourceScope() : "disabled")
            << ',' << (before.valid && after.valid) << ',' << resourceDelta(before.minorFaults, after.minorFaults)
            << ',' << resourceDelta(before.majorFaults, after.majorFaults) << ',' << resourceDelta(before.voluntarySwitches, after.voluntarySwitches)
            << ',' << resourceDelta(before.involuntarySwitches, after.involuntarySwitches) << ',' << resourceDelta(before.taskFaults, after.taskFaults)
            << ',' << resourceDelta(before.taskPageins, after.taskPageins) << ',' << (before.threadStateValid && after.threadStateValid)
            << ',' << before.runState << ',' << after.runState << ',' << before.suspendCount << ',' << after.suspendCount
            << ',' << before.error << ',' << after.error << ',' << before.threadStateError << ',' << after.threadStateError
            << ',' << before.queryStart.wallNs << ',' << before.queryEnd.wallNs << ',' << after.queryStart.wallNs << ',' << after.queryEnd.wallNs
            << ',' << wallDelta(before.queryStart, before.queryEnd) << ',' << wallDelta(after.queryStart, after.queryEnd)
            << ',' << row.allocations << ',' << row.deletions << ',' << row.checksum
            << ',' << row.bodyStart.wallValid << ',' << row.bodyEnd.wallValid << ',' << row.bodyStart.cpuValid << ',' << row.bodyEnd.cpuValid
            << ',' << row.entry.wallValid << ',' << row.exit.wallValid << ',' << row.entry.cpuValid << ',' << row.exit.cpuValid
            << ',' << row.entry.wallError << ',' << row.exit.wallError << ',' << row.entry.cpuError << ',' << row.exit.cpuError
            << ',' << row.workerSnapshotBeforeValid << ',' << row.workerSnapshotAfterValid << ',' << before.valid << ',' << after.valid
            << ',' << before.minorFaults << ',' << after.minorFaults << ',' << before.majorFaults << ',' << after.majorFaults
            << ',' << before.voluntarySwitches << ',' << after.voluntarySwitches << ',' << before.involuntarySwitches << ',' << after.involuntarySwitches
            << ',' << before.taskFaults << ',' << after.taskFaults << ',' << before.taskPageins << ',' << after.taskPageins;
        for (const auto* endpoint : {&before.queryStart, &before.queryEnd, &after.queryStart, &after.queryEnd})
            stream << ',' << endpoint->cpuNs << ',' << endpoint->wallValid << ',' << endpoint->cpuValid
                << ',' << endpoint->wallError << ',' << endpoint->cpuError;
        stream << '\n';
    }
    close(stream);
    stream = output(prefix + ".pacing.csv");
    stream << "callback_index,cadence,sleep_requested_ns,sleep_start_ns,sleep_end_ns,sleep_clock_valid,next_callback_start_ns\n";
    for (size_t i = 0; i < route.rowsWritten; ++i) {
        const auto& row = route.rows[i];
        const bool slept = route.options.cadence == "legacy" && i < workerCallbacks && i % 8 == 0;
        stream << i << ',' << route.options.cadence << ',' << (slept ? 1000000 : 0) << ','
            << row.sleepStart.wallNs << ',' << row.sleepEnd.wallNs << ','
            << (row.sleepStart.wallValid && row.sleepEnd.wallValid) << ','
            << (i + 1 < route.rowsWritten ? route.rows[i+1].bodyStart.wallNs : 0) << '\n';
    }
    close(stream);
    writeEvents(prefix + ".stages.csv", route.trace);
    writeEvents(prefix + ".worker.csv", route.workerTrace);
    writeEvents(prefix + ".caller.csv", route.callerTrace);
    stream = output(prefix + ".slots.csv");
    stream << "schema_version,callback_index,point,lane,mic,requested_model_key,active_model_key,active_generation,"
        "fading_model_key,fading_generation,pending_present,pending_identity_available,fade_remaining,fade_length,rejected_models,active_present,fading_present\n";
    for (size_t i = 0; i < route.slotsWritten; ++i)
    {
        const auto& row = route.slots[i];
        stream << "1," << row.callbackIndex << ',' << (row.after ? "after" : "before") << ',' << row.lane << ',' << row.mic
            << ',' << row.requestedKey << ',' << row.activeKey << ',' << row.activeGeneration << ',' << row.fadingKey << ',' << row.fadingGeneration
            << ',' << row.pendingPresent << ",0," << row.fadeRemaining << ',' << row.fadeLength << ',' << row.rejected
            << ',' << row.activePresent << ',' << row.fadingPresent << '\n';
    }
    close(stream);
    const uint64_t dropped = route.trace.dropped + route.workerTrace.dropped + route.callerTrace.dropped;
    const uint64_t nestingErrors = route.trace.nestingErrors + route.workerTrace.nestingErrors + route.callerTrace.nestingErrors;
    bool resourceValid = true;
    if (route.resourceEnabled())
        for (size_t i = 0; i < route.rowsWritten; ++i)
            resourceValid = resourceValid && route.rows[i].resourceBefore.valid && route.rows[i].resourceAfter.valid;
    const bool complete = route.rowsWritten == callbacks && route.semanticValid && dropped == 0 && clockErrors == 0
        && nestingErrors == 0 && route.totalAllocations == 0 && route.totalDeletions == 0 && resourceValid;
    stream = output(prefix + ".metadata.json");
    stream << std::boolalpha << "{\n  \"schema_version\": 1,\n  \"mode\": \"" << route.options.mode << "\",\n  \"engine\": \"" << route.engine
        << "\",\n  \"channels\": " << route.channels << ",\n  \"sample_rate\": 96000,\n  \"block_size\": " << blockSize << ",\n  \"trace_level\": \"" << route.options.traceLevel
        << "\",\n  \"complete\": " << complete << ",\n  \"callback_count\": " << route.rowsWritten << ",\n  \"stage_count\": " << route.trace.used
        << ",\n  \"worker_event_count\": " << route.workerTrace.used << ",\n  \"caller_event_count\": " << route.callerTrace.used
        << ",\n  \"slot_count\": " << route.slotsWritten << ",\n  \"dropped\": " << dropped
        << ",\n  \"clock_errors\": " << clockErrors << ",\n  \"nesting_errors\": " << nestingErrors
        << ",\n  \"audio_host_validation\": \"not_executed\",\n  \"product_acceptance\": false,\n  \"product_performance_change\": false,"
        << "\n  \"worker_trace_enabled\": " << (isCab && route.traced()) << ",\n  \"hooks_compiled\": " << hooksCompiled
        << ",\n  \"source_commit\": \"" << CHIMERA_ATTRIBUTION_SOURCE_SHA << "\",\n  \"compiler\": \"" << CHIMERA_ATTRIBUTION_COMPILER
        << "\",\n  \"system\": \"" << CHIMERA_ATTRIBUTION_SYSTEM << "\",\n  \"wall_clock\": \"" << cabTrace::wallClockName << "\","
        << "\n  \"thread_cpu_clock\": \"" << cabTrace::threadClockName << "\",\n  \"clock_read_order\": \"wall_then_cpu_at_each_endpoint\","
        << "\n  \"wall_minus_cpu_is_exclusive_scheduling_time\": false,"
        << "\n  \"resource_scope\": \"" << (route.resourceEnabled() ? resourceScope() : "disabled")
        << "\",\n  \"resource_capture_requested\": " << route.resourceEnabled()
        << ",\n  \"resource_capture_valid\": " << (route.resourceEnabled() && resourceValid)
        << ",\n  \"macos_minor_major_faults_supported\": false,\n  \"macos_resource_counters_include_worker\": true,"
        << "\n  \"macos_thread_state_is_continuous_scheduler_trace\": false,\n  \"hardware_cache_counters_available\": false,"
        << "\n  \"logical_bytes_are_hardware_traffic\": false,\n  \"pending_identity_available_in_snapshot\": false,"
        << "\n  \"pending_identity_source\": \"publication_swap_rejection_events\",\n  \"semantic_valid\": " << route.semanticValid
        << ",\n  \"recording_on_reference_oracle\": " << (isCab && route.traced())
        << ",\n  \"caller_stream_work\": \"reference_impulse_and_forced_publication\""
        << ",\n  \"worker_scheduling_valid\": " << route.workerSchedulingValid << ",\n  \"worker_start_succeeded\": " << route.workerStartSucceeded
        << ",\n  \"worker_entered_run\": " << route.workerEnteredRun << ",\n  \"worker_observed_priority\": " << route.observedWorkerPriority
        << ",\n  \"worker_cancelled_builds\": " << route.cancelledBuilds << ",\n  \"qos_setter_called\": false,\n  \"worker_priority_changed\": false,"
        << "\n  \"callback_allocations\": " << route.totalAllocations << ",\n  \"callback_deletions\": " << route.totalDeletions
        << ",\n  \"allocation_guard\": \"thread_local_cpp_new_delete_not_universal_malloc_or_lock_tracing\","
        << "\n  \"callbacks_worker_phase\": 1200,\n  \"callbacks_forced_phase\": 400,\n  \"automation_callbacks\": " << (isCab ? 80 : 0)
        << ",\n  \"parameter_refresh_per_callback\": false"
        << ",\n  \"cadence\": \"" << route.options.cadence << "\",\n  \"canonical_reset_before_forced_phase\": " << route.options.canonicalReset
        << ",\n  \"wake_extension_schema\": 1"
        << ",\n  \"paced_sleep_every\": " << (route.options.cadence == "legacy" ? 8 : 0)
        << ",\n  \"paced_sleep_ms\": " << (route.options.cadence == "legacy" ? 1 : 0)
        << ",\n  \"unmeasured_process_calls\": " << route.unmeasuredProcessCalls
        << ",\n  \"unmeasured_work\": \"response_checks_convergence_fade_drain_reference_impulse\","
        << "\n  \"event_capacity\": " << eventCapacity << ",\n  \"worker_event_capacity\": " << workerEventCapacity
        << ",\n  \"caller_event_capacity\": " << callerEventCapacity << ",\n  \"event_size_bytes\": " << sizeof(cabTrace::Event)
        << ",\n  \"allocated_event_bytes\": " << (eventCapacity + workerEventCapacity + callerEventCapacity) * sizeof(cabTrace::Event)
        << ",\n  \"prepared_array_pages_touched_before_measurement\": true"
        << ",\n  \"page_touch_stride_bytes\": " << route.pageTouch.strideBytes
        << ",\n  \"native_page_size_bytes\": " << route.pageTouch.nativePageSizeBytes
        << ",\n  \"page_touch_operations\": " << route.pageTouchOperations
        << ",\n  \"prepared_memory_locked\": false,\n  \"prepared_memory_residency_guaranteed\": false"
        << ",\n  \"control_memory_bytes\": " << route.memory.size() * sizeof(uint32_t)
        << ",\n  \"arithmetic_iterations\": " << arithmeticIterations << ",\n  \"memory_touches_per_slot\": " << memoryTouchesPerSlot
        << ",\n  \"memory_logical_bytes_per_callback\": " << (route.options.mode == "memory" ? 6 * memoryTouchesPerSlot * 2 * sizeof(uint32_t) : 0)
        << ",\n  \"calibration_empty_scopes\": " << (route.options.mode == "clock" && route.traced() ? 64 : 0)
        << ",\n  \"kernel_residual\": " << route.kernelResidual << ",\n  \"peak\": " << route.peak << ",\n  \"max_step\": " << route.maxStep
        << ",\n  \"deadline_misses\": " << misses << ",\n  \"deadline_misses_are_product_gate\": false,\n  \"checksum\": " << route.checksum << "\n}\n";
    close(stream);
    std::cout << "ATTRIBUTION_ROUTE mode=" << route.options.mode << " engine=" << route.engine << " channels=" << route.channels
        << " hooks_compiled=" << hooksCompiled << " trace_level=" << route.options.traceLevel << " callbacks=" << route.rowsWritten
        << " stage_events=" << route.trace.used << " worker_events=" << route.workerTrace.used << " misses=" << misses
        << " complete=" << complete << " diagnostic_only=1 audio_host_validation=not_executed path=" << prefix << ".metadata.json\n";
    require(complete, "Attribution evidence incomplete; no acceptance or performance claim may be made");
}
} // namespace attribution

int main(int argc, char** argv)
{
    try
    {
        const auto options = attribution::parse(argc, argv);
        attribution::require(!spectralforge::lifecycle::enabled(), "Run probe with lifecycle tracing disabled");
        const juce::ScopedNoDenormals noDenormals;
        std::cout << std::setprecision(17) << "ATTRIBUTION_PROTOCOL schema_version=1 diagnostic_only=1 sample_rate=96000 block_size=" << attribution::blockSize
            << " three_lanes=1 two_mics_per_lane=1 authored_ir_truncation=0 fade_ms=50 product_gate=unchanged_external"
            " host_callback=0 realtime_policy_requested=0 wall_cpu_raw_pairs=1\n";
        cabTiming::appendCallerScheduling(std::cout); std::cout << '\n';
        std::vector<std::string> engines = options.engine == "all"
            ? std::vector<std::string>{"original-v1", "expanded-v2", "array-v3-6x10"}
            : std::vector<std::string>{options.engine};
        for (const auto& engine : engines)
            for (int channels : {1, 2})
            {
                if (options.channels != 0 && options.channels != channels) continue;
                attribution::Route route(options, engine, channels);
                const auto allocatedBefore = watchedAllocations, deletedBefore = watchedDeletions;
                try
                {
                    if (options.mode == "cab") attribution::cab(route); else attribution::control(route);
                }
                catch (...)
                {
                    allocationWatch = false;
                    route.totalAllocations = watchedAllocations - allocatedBefore;
                    route.totalDeletions = watchedDeletions - deletedBefore;
                    // Preserve partial raw rows and explicit incomplete status.
                    // Writing fails closed; it cannot turn a failed route green.
                    try { attribution::write(route); } catch (...) {}
                    throw;
                }
                route.totalAllocations = watchedAllocations - allocatedBefore;
                route.totalDeletions = watchedDeletions - deletedBefore;
                attribution::write(route);
            }
        std::cout << "ATTRIBUTION_COMPLETE diagnostic_only=1 product_acceptance=false audio_host_validation=not_executed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        allocationWatch = false;
        std::cerr << "ATTRIBUTION_ERROR " << error.what() << '\n';
        return 2;
    }
}

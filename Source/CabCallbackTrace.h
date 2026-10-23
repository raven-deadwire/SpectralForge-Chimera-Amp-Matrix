#pragma once

// Diagnostic attribution only. Product/acceptance builds leave
// CHIMERA_CAB_ATTRIBUTION undefined (or 0), so every source hook below vanishes.
// The POD/API remain available to the separate probe so hook-off controls can
// reserve and touch exactly the same bounded storage as hook-on runs.
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <limits>
#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace spectralforge::cabTrace {
inline constexpr std::uint64_t noCallback=std::numeric_limits<std::uint64_t>::max();
enum class Stage : unsigned {
    Callback, CabProcess, Parameters, KernelSwap, KernelAccept, KernelReject,
    KernelRetire, Filters, CopyDry, CopySecond, CopyFade, MicWake, Reset,
    ActiveConvolution, FadingConvolution, Convolution, FFTForward, FFTInverse,
    SpectralMAC, InputPack, SpectrumPack, SpectrumUnpack, HistoryClear,
    SpectrumCopy, OutputMix, OverlapAdd, OverlapCopy, FadeMix, MicPost,
    DelayHistory, MicGain, MicBlend, FractionalDelayMix, DryMix,
    WorkerBuild, ResponseGenerate, SpectraPrepare, KernelPrepare, WorkerPublish,
    WorkerCollect, WorkerWait, WorkerRun, WorkerExit, WorkerCancel, WorkerError,
    KernelDestroy, ScopeCalibration, ArithmeticControl, MemoryControl, Count
};
inline constexpr std::array<const char*,unsigned(Stage::Count)> names{{
    "callback", "cab_process", "parameters", "kernel_swap", "kernel_accept", "kernel_reject",
    "kernel_retire", "filters", "copy_dry", "copy_second", "copy_fade", "mic_wake", "reset",
    "active_convolution", "fading_convolution", "convolution", "fft_forward", "fft_inverse",
    "spectral_mac", "input_pack", "spectrum_pack", "spectrum_unpack", "history_clear",
    "spectrum_copy", "output_mix", "overlap_add", "overlap_copy", "fade_mix", "mic_post",
    "delay_history", "mic_gain", "mic_blend", "fractional_delay_mix", "dry_mix",
    "worker_build", "response_generate", "spectra_prepare", "kernel_prepare", "worker_publish",
    "worker_collect", "worker_wait", "worker_run", "worker_exit", "worker_cancel", "worker_error",
    "kernel_destroy", "scope_calibration", "arithmetic_control", "memory_control"
}};
inline const char* name(Stage stage) noexcept {
    const auto value=unsigned(stage);return value<names.size() ? names[value] : "invalid";
}
enum class Role : unsigned {None, Active, Fading, Pending, Rejected, Retired, Worker};
inline const char* name(Role role) noexcept {
    constexpr const char* values[]{"none","active","fading","pending","rejected","retired","worker"};
    const auto value=unsigned(role);return value<std::size(values) ? values[value] : "invalid";
}
struct ClockSample {
    std::uint64_t wallNs{},cpuNs{};
    bool wallValid{},cpuValid{};
    int wallError{},cpuError{};
};
inline constexpr const char* wallClockName=
#if defined(_WIN32)
    "std::chrono::steady_clock";
#else
    "CLOCK_MONOTONIC";
#endif
inline constexpr const char* threadClockName=
#if defined(_WIN32)
    "GetThreadTimes_user_plus_kernel_100ns_units_coarse_resolution";
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    "CLOCK_THREAD_CPUTIME_ID";
#else
    "unavailable";
#endif
inline ClockSample clockSample() noexcept {
    ClockSample result;
    // Keep the read order explicit: wall then thread CPU at both endpoints.
    // These envelopes differ by clock-read cost/resolution. Preserve raw values
    // even when the CPU delta exceeds the wall delta; never label wall-CPU as
    // exclusive scheduler, hypervisor, or DSP time.
#if defined(_WIN32)
    result.wallNs=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    result.wallValid=true;
    FILETIME creation{},exit{},kernel{},user{};
    if(::GetThreadTimes(::GetCurrentThread(),&creation,&exit,&kernel,&user)) {
        const auto value=[](FILETIME t) {return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        result.cpuNs=(value(kernel)+value(user))*100u;result.cpuValid=true;
    } else result.cpuError=int(::GetLastError());
#else
    timespec wall{};
    if(::clock_gettime(CLOCK_MONOTONIC,&wall)==0) {
        result.wallNs=std::uint64_t(wall.tv_sec)*1000000000u+std::uint64_t(wall.tv_nsec);result.wallValid=true;
    } else result.wallError=errno;
 #if defined(CLOCK_THREAD_CPUTIME_ID)
    timespec cpu{};
    if(::clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu)==0) {
        result.cpuNs=std::uint64_t(cpu.tv_sec)*1000000000u+std::uint64_t(cpu.tv_nsec);result.cpuValid=true;
    } else result.cpuError=errno;
 #else
    result.cpuError=ENOTSUP;
 #endif
#endif
    return result;
}

enum class WorkerState : unsigned {Idle, Run, Build, Generate, Prepare, Publish, Collect, Wait, Cancelled, Error, Exit};
inline const char* name(WorkerState state) noexcept {
    constexpr const char* values[]{"idle","run","build","generate","prepare","publish","collect","wait","cancelled","error","exit"};
    const auto value=unsigned(state);return value<std::size(values) ? values[value] : "invalid";
}
struct WorkerSnapshot {
    std::uint64_t sequence{};
    WorkerState state{WorkerState::Idle};
    int lane{-1},mic{-1};
    bool valid{};
};
class WorkerProgress {
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    // One packed atomic is the complete observation. No spin, seqlock retry,
    // worker-owned pointer, mutex-protected generation, or torn identity read.
    // Low 8 bits: state; next 4+4: lane+1, mic+1; high 48: sequence.
    std::atomic<std::uint64_t> packed{};
public:
    WorkerSnapshot snapshot() const noexcept {
        const auto value=packed.load(std::memory_order_relaxed);
        return {value>>16,WorkerState(value&255),int((value>>8)&15)-1,int((value>>12)&15)-1,true};
    }
    // Sole writer is run(), except resetting while the worker is stopped.
    void transition(WorkerState state,int lane=-1,int mic=-1) noexcept {
        const auto sequence=(packed.load(std::memory_order_relaxed)>>16)+1;
        packed.store((sequence<<16)|(std::uint64_t(mic+1)<<12)|(std::uint64_t(lane+1)<<8)|unsigned(state),std::memory_order_relaxed);
    }
    void reset() noexcept {packed.store(0,std::memory_order_relaxed);}
};
struct SlotSnapshot {
    std::uint64_t activeModel{},fadingModel{},requestedModel{};
    unsigned activeGeneration{},fadingGeneration{},rejected{};
    int activeSource{-1},fadingSource{-1},requestedSource{-1};
    int fadeRemaining{},fadeLength{};
    bool activePresent{},fadingPresent{},pendingPresent{};
    // Publication events own exact pending identity. A pending pointer loaded
    // by audio can be replaced/destroyed by the worker before dereference.
    bool pendingIdentityValid{false};
};
struct Event {
    Stage stage{};
    std::uint64_t callbackId{noCallback},eventId{},parentId{};
    unsigned depth{};
    int lane{-1},mic{-1};
    std::uint64_t modelKey{};
    unsigned generation{};
    Role role{Role::None};
    // Sum of algorithmic reads+writes for known source loops, not unique RAM,
    // actual cache traffic/misses, physical memory bandwidth or FFT internals.
    std::uint64_t logicalBytes{};
    std::uint64_t workerSequenceStart{},workerSequenceEnd{};
    ClockSample start,end;
    bool complete{};
    // WorkerWait extension: requested timeout and actual wait return. A
    // timeout overshoot is not an OS runnable-to-running scheduling trace.
    std::uint64_t waitRequestedNs{};
    int waitNotified{-1};
};
struct Buffer {
    Event* events{};
    std::size_t capacity{},used{};
    std::uint64_t dropped{},clockErrors{},nestingErrors{};
    // Single writer. Read/reset only after this thread is stopped or after the
    // callback returns. The caller allocates/touches storage before measuring.
};
struct Context {
    int lane{-1},mic{-1};
    std::uint64_t modelKey{};
    unsigned generation{};
    Role role{Role::None};
};
struct CaptureState {
    Buffer* buffer{};
    std::uint64_t callbackId{noCallback},parentId{};
    unsigned depth{};
    Context context;
    WorkerProgress* progress{};
    const WorkerProgress* observedProgress{};
};
inline thread_local CaptureState* current=nullptr;
class ThreadCapture {
    CaptureState state;
    CaptureState* previous;
    bool stopped{};
public:
    explicit ThreadCapture(Buffer* buffer,std::uint64_t callbackId=noCallback,WorkerProgress* progress=nullptr,
                           const WorkerProgress* observedProgress=nullptr) noexcept
        : state{buffer,callbackId,0,0,{},progress,observedProgress ? observedProgress : progress},previous(current) {current=&state;}
    void stop() noexcept {if(!stopped) {current=previous;stopped=true;}}
    ~ThreadCapture() {stop();}
    ThreadCapture(const ThreadCapture&)=delete;
    ThreadCapture& operator=(const ThreadCapture&)=delete;
};
class ContextScope {
    CaptureState* capture;
    Context previous;
public:
    explicit ContextScope(Context value) noexcept : capture(current) {
        if(capture) {previous=capture->context;capture->context=value;}
    }
    ContextScope(std::uint64_t key,unsigned generation,Role role) noexcept : capture(current) {
        if(capture) {previous=capture->context;capture->context.modelKey=key;capture->context.generation=generation;capture->context.role=role;}
    }
    ~ContextScope() {if(capture)capture->context=previous;}
};
class Scope {
    CaptureState* capture{};
    Event* event{};
    std::uint64_t previousParent{};
    unsigned previousDepth{};
public:
    explicit Scope(Stage stage,std::uint64_t logicalBytes=0) noexcept : capture(current) {
        if(!capture || !capture->buffer) {capture=nullptr;return;}
        auto& buffer=*capture->buffer;
        previousParent=capture->parentId;previousDepth=capture->depth;
        ++capture->depth;
        if(!buffer.events || buffer.used>=buffer.capacity) {++buffer.dropped;return;}
        const auto id=++buffer.used;
        event=buffer.events+id-1;*event=Event{};
        event->stage=stage;event->callbackId=capture->callbackId;event->eventId=id;
        event->parentId=previousParent;event->depth=previousDepth;
        event->lane=capture->context.lane;event->mic=capture->context.mic;
        event->modelKey=capture->context.modelKey;event->generation=capture->context.generation;
        event->role=capture->context.role;event->logicalBytes=logicalBytes;
        if(capture->observedProgress)event->workerSequenceStart=capture->observedProgress->snapshot().sequence;
        capture->parentId=id;
        event->start=clockSample();
        buffer.clockErrors+=!event->start.wallValid+!event->start.cpuValid;
    }
    void stop() noexcept {
        if(!capture)return;
        if(event) {
            event->end=clockSample();event->complete=true;
            if(capture->observedProgress)event->workerSequenceEnd=capture->observedProgress->snapshot().sequence;
            capture->buffer->clockErrors+=!event->end.wallValid+!event->end.cpuValid;
            if(capture->parentId!=event->eventId || capture->depth!=previousDepth+1)
                ++capture->buffer->nestingErrors;
        }
        capture->parentId=previousParent;capture->depth=previousDepth;capture=nullptr;
    }
    ~Scope() {stop();}
    void waitResult(unsigned milliseconds,bool notified) noexcept {
        if(event) {event->waitRequestedNs=std::uint64_t(milliseconds)*1000000u;
            event->waitNotified=notified ? 1 : 0;}
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
class WorkerScope {
    WorkerProgress* progress{};
    WorkerSnapshot previous;
    Scope scope;
public:
    WorkerScope(Stage stage,WorkerState state) noexcept
        : progress(current ? current->progress : nullptr),previous(progress ? progress->snapshot() : WorkerSnapshot{}),
          scope((progress ? (progress->transition(state,current->context.lane,current->context.mic),stage) : stage)) {}
    ~WorkerScope() {
        scope.stop();
        if(progress)progress->transition(previous.state,previous.lane,previous.mic);
    }
    void waitResult(unsigned milliseconds,bool notified) noexcept {scope.waitResult(milliseconds,notified);}
};
class CallbackCapture {
    const WorkerProgress* progress;
    ThreadCapture capture;
    Scope root;
    bool stopped{};
public:
    WorkerSnapshot workerEntry,workerExit;
    CallbackCapture(Buffer* buffer,std::uint64_t callbackId,const WorkerProgress* worker=nullptr) noexcept
        : progress(worker),capture(buffer,callbackId,nullptr,worker),root(Stage::Callback),workerEntry(worker ? worker->snapshot() : WorkerSnapshot{}) {}
    void stop() noexcept {
        if(stopped)return;
        workerExit=progress ? progress->snapshot() : WorkerSnapshot{};
        root.stop();capture.stop();stopped=true;
    }
    ~CallbackCapture() {stop();}
};
}

#if defined(CHIMERA_CAB_ATTRIBUTION) && CHIMERA_CAB_ATTRIBUTION
 #define SF_CAB_TRACE_JOIN_IMPL(a,b) a##b
 #define SF_CAB_TRACE_JOIN(a,b) SF_CAB_TRACE_JOIN_IMPL(a,b)
 #define SF_CAB_TRACE_SCOPE(stage) ::spectralforge::cabTrace::Scope SF_CAB_TRACE_JOIN(cabTraceScope,__LINE__){::spectralforge::cabTrace::Stage::stage}
 #define SF_CAB_TRACE_BYTES(stage,bytes) ::spectralforge::cabTrace::Scope SF_CAB_TRACE_JOIN(cabTraceScope,__LINE__){::spectralforge::cabTrace::Stage::stage,static_cast<std::uint64_t>(bytes)}
 #define SF_CAB_TRACE_BEGIN(stage,name) ::spectralforge::cabTrace::Scope name{::spectralforge::cabTrace::Stage::stage}
 #define SF_CAB_TRACE_END(name) name.stop()
 #define SF_CAB_TRACE_CONTEXT(lane,mic,key,generation,role) ::spectralforge::cabTrace::ContextScope SF_CAB_TRACE_JOIN(cabTraceContext,__LINE__){::spectralforge::cabTrace::Context{lane,mic,key,generation,::spectralforge::cabTrace::Role::role}}
 #define SF_CAB_TRACE_KERNEL(kernel,role) ::spectralforge::cabTrace::ContextScope SF_CAB_TRACE_JOIN(cabTraceContext,__LINE__){kernel ? kernel->modelKey : 0,kernel ? kernel->generation : 0,::spectralforge::cabTrace::Role::role}
 #define SF_CAB_TRACE_WORKER(stage,state) ::spectralforge::cabTrace::WorkerScope SF_CAB_TRACE_JOIN(cabTraceWorker,__LINE__){::spectralforge::cabTrace::Stage::stage,::spectralforge::cabTrace::WorkerState::state}
#else
 #define SF_CAB_TRACE_SCOPE(stage)
 #define SF_CAB_TRACE_BYTES(stage,bytes)
 #define SF_CAB_TRACE_BEGIN(stage,name)
 #define SF_CAB_TRACE_END(name)
 #define SF_CAB_TRACE_CONTEXT(lane,mic,key,generation,role)
 #define SF_CAB_TRACE_KERNEL(kernel,role)
 #define SF_CAB_TRACE_WORKER(stage,state)
#endif

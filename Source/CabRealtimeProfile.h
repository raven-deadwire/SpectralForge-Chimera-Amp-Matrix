#pragma once

// The normal plugin and deadline contracts compile every hook away. The
// separate profiling executable enables thread-local counters; it is never
// substituted for the uninstrumented p99 acceptance test.
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
#include <array>
#include <chrono>
#include <cstdint>

namespace spectralforge::cabProfile {
enum class Stage : unsigned {
    Parameters, KernelSwap, Filters, ActiveConvolution, FadingConvolution,
    MicPost, FFTForward, FFTInverse, SpectralMAC,
    WorkerBuild, ResponseGenerate, KernelPrepare, WorkerPublish, WorkerCollect,
    Count
};
inline constexpr std::array<const char*, unsigned(Stage::Count)> names{{
    "parameters", "kernel_swap", "filters", "active_convolution", "fading_convolution",
    "mic_post", "fft_forward", "fft_inverse", "spectral_mac",
    "worker_build", "response_generate", "kernel_prepare", "worker_publish", "worker_collect"
}};
struct Counters {
    std::array<std::uint64_t, unsigned(Stage::Count)> nanoseconds{}, calls{};
};
inline thread_local Counters* sink = nullptr;
class ThreadCapture {
    Counters* previous;
public:
    explicit ThreadCapture(Counters* destination) noexcept : previous(sink) {sink=destination;}
    ~ThreadCapture() {sink=previous;}
};
class Scope {
    using Clock=std::chrono::steady_clock;
    Counters* destination;
    Stage stage;
    Clock::time_point start;
public:
    explicit Scope(Stage value) noexcept : destination(sink), stage(value) {
        if(destination) start=Clock::now();
    }
    void stop() noexcept {
        if(!destination)return;
        const auto index=unsigned(stage);
        destination->nanoseconds[index]+=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());
        ++destination->calls[index]; destination=nullptr;
    }
    ~Scope() {stop();}
};
}
#define SF_CAB_PROFILE_JOIN_IMPL(a,b) a##b
#define SF_CAB_PROFILE_JOIN(a,b) SF_CAB_PROFILE_JOIN_IMPL(a,b)
#define SF_CAB_PROFILE_SCOPE(stage) ::spectralforge::cabProfile::Scope SF_CAB_PROFILE_JOIN(cabProfileScope,__LINE__){::spectralforge::cabProfile::Stage::stage}
#define SF_CAB_PROFILE_BEGIN(stage,name) ::spectralforge::cabProfile::Scope name{::spectralforge::cabProfile::Stage::stage}
#define SF_CAB_PROFILE_END(name) name.stop()
#else
#define SF_CAB_PROFILE_SCOPE(stage)
#define SF_CAB_PROFILE_BEGIN(stage,name)
#define SF_CAB_PROFILE_END(name)
#endif

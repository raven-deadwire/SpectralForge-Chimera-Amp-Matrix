// Independent research executable: never linked into a plug-in or substituted
// for deployed CTest acceptance. The frozen test body below is not edited.
#include "CabTimingDiagnostics.h"
#include "IRLibrary.h"
#include "CabLayoutModel.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <new>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <array>
#include <string>
#include <vector>
#if defined(__APPLE__)
 #include <pthread.h>
 #include <pthread/qos.h>
 #include <mach/mach.h>
 #include <mach/thread_policy.h>
 #include <sys/qos.h>
#endif

#ifndef FROZEN_CAB_TEST_FILE
 #error The controller must supply the exact frozen test source path.
#endif
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
 #error Scheduling comparisons must not enable stage profiling.
#endif

#define main frozenCabMain
#include FROZEN_CAB_TEST_FILE
#undef main

namespace schedulingDiagnostic
{
struct Options
{
    std::string qos = "inherited";
    bool sentinel = false;
    bool expanded = false;
    bool layouts = false;
    bool extended = false;
    std::vector<char*> originalArguments;
};

Options parse(int argc, char** argv)
{
    Options options;
    options.originalArguments.reserve(static_cast<size_t>(argc) + 1);
    options.originalArguments.push_back(argv[0]);
    bool hasQos = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--caller-qos")
        {
            if (hasQos || ++index >= argc)
                throw std::runtime_error("--caller-qos requires one value and may occur only once");
            hasQos = true;
            options.qos = argv[index];
            if (options.qos != "inherited" && options.qos != "user-interactive")
                throw std::runtime_error("--caller-qos must be inherited or user-interactive");
        }
        else if (argument == "--scheduler-sentinel")
        {
            if (options.sentinel)
                throw std::runtime_error("--scheduler-sentinel may occur only once");
            options.sentinel = true;
        }
        else
        {
            if (argument == "--expanded") options.expanded = true;
            else if (argument == "--layouts") options.layouts = true;
            else if (argument == "--extended-buffers") options.extended = true;
            // Frozen main owns validation of its arguments in CAB mode. In the
            // distinct sentinel mode, reject anything it does not understand.
            else if (options.sentinel)
                throw std::runtime_error("Unknown scheduler sentinel argument");
            options.originalArguments.push_back(argv[index]);
        }
    }
    if (options.sentinel)
    {
        for (size_t index = 1; index < options.originalArguments.size(); ++index)
        {
            const std::string argument = options.originalArguments[index];
            if (argument != "--expanded" && argument != "--layouts"
                && argument != "--extended-buffers")
                throw std::runtime_error("Unknown scheduler sentinel argument");
        }
        if (options.expanded && options.layouts)
            throw std::runtime_error("Select one scheduler sentinel route group");
    }
    options.originalArguments.push_back(nullptr);
    return options;
}

void context(const char* phase)
{
    std::cout << "SCHEDULING_DIAGNOSTIC_CONTEXT phase=" << phase;
    cabTiming::appendCallerScheduling(std::cout);
#if defined(__APPLE__)
    int relativePriority = 0;
    qos_class_t requestedQos = QOS_CLASS_UNSPECIFIED;
    const auto qosQuery = pthread_get_qos_class_np(pthread_self(), &requestedQos, &relativePriority);
    std::cout << " pthread_qos_query=" << qosQuery
              << " pthread_requested_qos=" << static_cast<unsigned>(requestedQos)
              << " pthread_relative_priority=" << relativePriority;
    thread_time_constraint_policy_data_t policy{};
    mach_msg_type_number_t count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
    boolean_t returnedDefault = FALSE;
    const auto query = thread_policy_get(pthread_mach_thread_np(pthread_self()),
        THREAD_TIME_CONSTRAINT_POLICY, reinterpret_cast<thread_policy_t>(&policy),
        &count, &returnedDefault);
    std::cout << " mach_time_constraint_query=" << query
              << " mach_time_constraint_count=" << count
              << " mach_time_constraint_default=" << returnedDefault;
    if (query == KERN_SUCCESS)
        std::cout << " mach_period_ticks=" << policy.period
                  << " mach_computation_ticks=" << policy.computation
                  << " mach_constraint_ticks=" << policy.constraint
                  << " mach_preemptible=" << policy.preemptible;
    // These are read-only snapshots of requested policy. They are not a trace
    // of temporary priority boosts, CPU migration, hypervisor or host activity.
#else
    std::cout << " mach_time_constraint_query=not_applicable";
#endif
    std::cout << '\n';
}

void clocks()
{
    std::cout << "SCHEDULING_DIAGNOSTIC_CLOCK steady_clock_is_steady="
              << std::chrono::steady_clock::is_steady
              << " steady_clock_period_num=" << std::chrono::steady_clock::period::num
              << " steady_clock_period_den=" << std::chrono::steady_clock::period::den;
#if !defined(_WIN32)
    timespec resolution{};
    errno = 0;
    const auto result = clock_getres(CLOCK_THREAD_CPUTIME_ID, &resolution);
    const auto error = result == 0 ? 0 : errno;
    std::cout << " thread_cpu_clock_getres_result=" << result
              << " thread_cpu_clock_getres_error=" << error
              << " thread_cpu_resolution_ns="
              << (result == 0 ? double(resolution.tv_sec) * 1e9 + double(resolution.tv_nsec) : -1.0);
#endif
    std::cout << " clocks_have_different_envelopes=1"
                 " wall_minus_cpu_not_exclusive_scheduler_time=1"
                 " cpu_clock_not_proof_of_dsp_execution=1\n";
}

void requestQos(const std::string& requested)
{
    if (requested == "inherited")
    {
        std::cout << "SCHEDULING_DIAGNOSTIC_QOS mode=inherited setter_called=0\n";
        return;
    }
#if defined(__APPLE__)
    const auto result = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    const auto observed = qos_class_self();
    std::cout << "SCHEDULING_DIAGNOSTIC_QOS mode=user-interactive setter_called=1"
              << " setter_result=" << result
              << " requested=" << static_cast<unsigned>(QOS_CLASS_USER_INTERACTIVE)
              << " observed=" << static_cast<unsigned>(observed) << '\n';
    if (result != 0 || observed != QOS_CLASS_USER_INTERACTIVE)
        throw std::runtime_error("Diagnostic caller QoS request was not established; no fallback measurements");
#else
    throw std::runtime_error("user-interactive caller QoS is a macOS-only diagnostic");
#endif
}

std::filesystem::path saveSentinelCsv(const cabTiming::Configuration& config,
                                      const std::vector<cabTiming::Sample>& samples)
{
    cabTiming::validate(samples);
    const auto directory = std::filesystem::path("cab-timing-diagnostics") / config.suite / config.engine;
    std::filesystem::create_directories(directory);
    const auto path = directory / (std::to_string(static_cast<int>(config.sampleRate)) + "-"
        + std::to_string(config.blockSize) + "-" + std::to_string(config.channels) + ".csv");
    std::ofstream stream(path, std::ios::trunc);
    if (!stream) throw std::runtime_error("Cannot create scheduler sentinel CSV");
    stream << std::setprecision(17)
        << "schema_version,engine,suite,sample_rate,block_size,channels,callback_index,phase,phase_index,"
           "automation_requested,wall_us,budget_us,deadline_miss,thread_cpu_us,thread_cpu_valid,"
           "thread_cycles,thread_cycles_valid,clock_start_error,clock_end_error,"
           "thread_cpu_start_us,thread_cpu_end_us,thread_cycles_start,thread_cycles_end,workload,worker_running\n";
    for (const auto& sample : samples)
    {
        const bool paced = sample.index < cabTiming::workerCallbacks;
        stream << "1," << config.engine << ',' << config.suite << ',' << config.sampleRate << ','
               << config.blockSize << ',' << config.channels << ',' << sample.index << ','
               << (paced ? "paced_control" : "burst_control") << ','
               << (paced ? sample.index : sample.index - cabTiming::workerCallbacks) << ",0,"
               << sample.wallUs << ',' << config.budgetUs() << ',' << (sample.wallUs > config.budgetUs()) << ','
               << sample.cpuUs() << ',' << sample.cpuValid() << ',' << sample.cycles() << ','
               << sample.cyclesValid() << ',' << sample.begin.error << ',' << sample.end.error << ','
               << sample.begin.cpuUs << ',' << sample.end.cpuUs << ',' << sample.begin.cycles << ','
               << sample.end.cycles << ",arithmetic_only,0\n";
    }
    if (!stream) throw std::runtime_error("Scheduler sentinel CSV write failed");
    stream.close();
    if (!stream) throw std::runtime_error("Scheduler sentinel CSV flush failed");
    return path;
}

int sentinel(const Options& options)
{
    constexpr unsigned arithmeticIterations = 256;
    const char* engine = options.layouts ? "sentinel-array-v3-6x10"
                         : options.expanded ? "sentinel-expanded-v2" : "sentinel-original-v1";
    const char* suite = options.extended ? "scheduler-sentinel-extended"
                                        : "scheduler-sentinel-integration";
    const std::vector<int> blocks = options.extended ? std::vector<int>{32, 64, 128, 256, 512}
                                                   : std::vector<int>{64, 256};
    std::cout << "SCHEDULER_SENTINEL_PROTOCOL diagnostic_only=1 acceptance_gate=none"
                 " audio_callbacks=0 cab_instances=0 ir_workers=0 ui_instances=0"
                 " measured_intervals=1600 paced_intervals=1200 unpaced_intervals=400"
                 " sleep_ms=1 sleep_every=8 sleep_implementation=std_this_thread_sleep_for"
                 " arithmetic_iterations=" << arithmeticIterations
              << " intervals_are_synthetic=1 csv_phase_names=paced_control_and_burst_control"
                 " synthetic_exposure_differs_from_cab=1 no_elapsed_time_subtraction=1\n";
    for (const double sampleRate : {44100.0, 48000.0, 96000.0})
        for (const int blockSize : blocks)
            for (const int channels : {1, 2})
            {
                const cabTiming::Configuration configuration{engine, suite, sampleRate, blockSize, channels};
                std::vector<cabTiming::Sample> samples(cabTiming::measuredCallbacks);
                std::vector<double> walls(cabTiming::measuredCallbacks);
                uint64_t checksum = 0;
                for (size_t index = 0; index < samples.size(); ++index)
                {
                    // The same deterministic bounded scalar arithmetic in every
                    // route: block/rate/channel only provide deadline references.
                    const auto cpuBefore = cabTiming::readThreadClock();
                    const auto begin = std::chrono::steady_clock::now();
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    volatile uint64_t value = UINT64_C(0x9e3779b97f4a7c15) ^ static_cast<uint64_t>(index);
                    for (unsigned step = 0; step < arithmeticIterations; ++step)
                        value = (value ^ (value >> 13)) * UINT64_C(0x2545f4914f6cdd1d) + step;
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    const auto end = std::chrono::steady_clock::now();
                    const auto cpuAfter = cabTiming::readThreadClock();
                    const auto wallUs = std::chrono::duration<double, std::micro>(end - begin).count();
                    samples[index] = {index, wallUs, cpuBefore, cpuAfter};
                    walls[index] = wallUs;
                    checksum ^= value;
                    if (index < cabTiming::workerCallbacks && index % 8 == 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                // Same paired-clock columns, but explicit synthetic phase and
                // workload labels: no worker/publication/automation is implied.
                const auto csv = saveSentinelCsv(configuration, samples);
                cabTiming::reportPairs(std::cout, configuration, "all", samples, 0, samples.size());
                cabTiming::reportPairs(std::cout, configuration, "paced_synthetic", samples,
                                       0, cabTiming::workerCallbacks);
                cabTiming::reportPairs(std::cout, configuration, "unpaced_synthetic", samples,
                                       cabTiming::workerCallbacks, samples.size());
                const auto misses = std::count_if(walls.begin(), walls.end(), [&configuration](double wall) {
                    return wall > configuration.budgetUs();
                });
                std::sort(walls.begin(), walls.end());
                std::cout << std::setprecision(17)
                          << "SENTINEL_TIMING engine=" << engine << " suite=" << suite
                          << " sr=" << sampleRate << " block=" << blockSize << " channels=" << channels
                          << " samples=" << samples.size() << " p50_us=" << walls[800]
                          << " p99_us=" << walls[1584] << " max_us=" << walls.back()
                          << " budget_us=" << configuration.budgetUs()
                          << " misses=" << misses << "/1600 checksum=" << checksum
                          << " acceptance_gate=none path=" << csv.generic_string() << '\n';
            }
    return 0; // Completion only; no synthetic result authorizes CAB acceptance.
}
}

int main(int argc, char** argv)
{
    try
    {
        auto options = schedulingDiagnostic::parse(argc, argv);
        std::cout << std::setprecision(17)
                  << "SCHEDULING_DIAGNOSTIC_PROTOCOL diagnostic_only=1 product_change=0"
                     " frozen_source=" << CHIMERA_FROZEN_SHA
                  << " caller_qos=" << options.qos
                  << " workload=" << (options.sentinel ? "synthetic-sentinel" : "unchanged-frozen-cab")
                  << " worker_priority_unchanged=1 process_priority_unchanged=1"
                     " realtime_policy_requested=0 no_daw_or_coreaudio_callback=1\n";
        schedulingDiagnostic::context("entry");
        schedulingDiagnostic::clocks();
        schedulingDiagnostic::requestQos(options.qos);
        schedulingDiagnostic::context("before-workload");
        const auto code = options.sentinel ? schedulingDiagnostic::sentinel(options)
            : frozenCabMain(static_cast<int>(options.originalArguments.size()) - 1,
                            options.originalArguments.data());
        schedulingDiagnostic::context("after-workload");
        std::cout << "SCHEDULING_DIAGNOSTIC_RESULT workload_exit=" << code
                  << " diagnostic_only=1 product_acceptance=unchanged_external_gate\n";
        return code;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SCHEDULING_DIAGNOSTIC_ERROR " << error.what() << '\n';
        return 2;
    }
}

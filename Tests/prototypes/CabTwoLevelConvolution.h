#pragma once
// PROTOTYPE ONLY (docs/CAB_TWO_LEVEL_CONVOLUTION_DESIGN.md). Not part of the
// product or of any CTest. It exists to measure whether a two-level partition
// lowers the audio-thread cost of long modeled CAB responses at small blocks.
//
//   y = head * x  +  tail * x
//
// head: the first H samples of the response through the unchanged
//       ModeledCabConvolution on the audio thread (zero latency, the product's
//       current uniform partition at block B).
// tail: the remaining samples in K-sample partitions (2K FFT), computed on a
//       background worker one K block behind the audio thread. Tail block m
//       (input samples [mK, (m+1)K)) first contributes at output time mK + H.
//       It can start when its input is complete, at (m+1)K, so with H = 2K the
//       worker has K samples of time. No latency is added. If the worker is
//       late the audio thread outputs the head alone for the missing samples
//       and counts it (a production version must define a safe policy).
//
// Stereo is carried as one complex stream (left + i*right), as the product does.
#include <juce_dsp/juce_dsp.h>
#include "ModeledCabConvolution.h"
#include <atomic>
#include <complex>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

namespace spectralforge::prototype {

class TwoLevelTail {
    using Complex = std::complex<float>;
public:
    // tailSamples: the response from sample H on. K: tail partition.
    TwoLevelTail(const float* tailSamples, int tailLength, int partition, int headLength)
        : K(partition), N(2 * partition), H(headLength),
          P(std::max(1, (tailLength + partition - 1) / partition)),
          fft(juce::roundToInt(std::log2(double(2 * partition)))),
          impulse(size_t(P) * size_t(N)), history(size_t(P) * size_t(N)),
          inputRing(size_t(ringBlocks) * size_t(partition)), outputRing(size_t(outputSize())),
          scratchTime(size_t(N)), scratchSpectrum(size_t(N)), sum(size_t(N))
    {
        jassert(H >= 2 * K);
        std::vector<Complex> time(static_cast<size_t>(N));
        for (int p = 0; p < P; ++p) {
            std::fill(time.begin(), time.end(), Complex{});
            for (int i = 0; i < K && p * K + i < tailLength; ++i) time[size_t(i)] = {tailSamples[p * K + i], 0.f};
            fft.perform(time.data(), impulse.data() + size_t(p) * size_t(N), false);
        }
    }

    // ---- audio thread -------------------------------------------------
    // Push one input sample pair; returns the tail output for this time.
    // Called sample by sample inside the block loop (cheap: two ring writes,
    // one ring read). Signals the worker when a K block completes.
    inline Complex step(Complex x, std::counting_semaphore<>& wake) noexcept
    {
        inputRing[size_t(audioTime % (long long)(ringBlocks * K))] = x;
        const auto slot = size_t(audioTime % (long long)outputSize());
        Complex y{};
        const long long block = (audioTime - H) / K; // the tail block that first reaches this time
        if (audioTime < H || completed.load(std::memory_order_acquire) >= block) y = outputRing[slot];
        else if (audioTime >= H) ++late; // worker behind: tail missing here
        outputRing[slot] = {};
        ++audioTime;
        if (audioTime % K == 0) { submitted.store(audioTime / K - 1, std::memory_order_release); wake.release(); }
        return y;
    }
    unsigned lateSamples() const noexcept { return late; }

    // ---- worker thread ------------------------------------------------
    // Returns true when it computed a block.
    bool service() noexcept
    {
        const long long next = completed.load(std::memory_order_relaxed) + 1;
        if (submitted.load(std::memory_order_acquire) < next) return false;
        // Input block `next` from the ring (the audio thread is at most a
        // few K ahead; the ring holds ringBlocks K blocks).
        std::fill(scratchTime.begin(), scratchTime.end(), Complex{});
        const auto start = size_t((next * K) % (long long)(ringBlocks * K));
        for (int i = 0; i < K; ++i) scratchTime[size_t(i)] = inputRing[start + size_t(i)];
        auto* current = history.data() + size_t(next % P) * size_t(N);
        fft.perform(scratchTime.data(), current, false);
        std::fill(sum.begin(), sum.end(), Complex{});
        for (int p = 0; p < P; ++p) {
            const auto* x = history.data() + size_t(((next - p) % P + P) % P) * size_t(N);
            const auto* h = impulse.data() + size_t(p) * size_t(N);
            if (next - p < 0) continue;
            for (int i = 0; i < N; ++i) sum[size_t(i)] += x[i] * h[i];
        }
        fft.perform(sum.data(), scratchTime.data(), true);
        // juce::dsp::FFT's inverse is scaled by 1/N already.
        const long long at = next * K + H;
        for (int i = 0; i < N; ++i) outputRing[size_t((at + i) % (long long)outputSize())] += scratchTime[size_t(i)];
        completed.store(next, std::memory_order_release);
        return true;
    }

private:
    static constexpr int ringBlocks = 8;
    int outputSize() const noexcept { return 8 * K + H; }
    const int K, N, H, P;
    juce::dsp::FFT fft;
    std::vector<Complex> impulse, history, inputRing, outputRing, scratchTime, scratchSpectrum, sum;
    long long audioTime{};
    unsigned late{};
    std::atomic<long long> submitted{-1}, completed{-1};
};

// One worker services every registered tail (production would share one per
// plug-in instance, at a priority below the audio thread).
class TailWorker {
public:
    ~TailWorker() { stop(); }
    void add(TwoLevelTail* tail) { tails.push_back(tail); }
    void start() { running = true; thread = std::thread([this] { run(); }); }
    void stop() { if (thread.joinable()) { running = false; wake.release(); thread.join(); } }
    std::counting_semaphore<>& semaphore() noexcept { return wake; }
    std::atomic<long long> blocks{};
private:
    void run() {
        while (running) {
            wake.acquire();
            bool any = true;
            while (any) { any = false; for (auto* t : tails) if (t->service()) { any = true; ++blocks; } }
        }
    }
    std::vector<TwoLevelTail*> tails;
    std::counting_semaphore<> wake{0};
    std::atomic<bool> running{};
    std::thread thread;
};

// head + tail for one microphone, stereo (or mono in the left channel).
class TwoLevelConvolution {
    using Complex = std::complex<float>;
public:
    TwoLevelConvolution(const juce::AudioBuffer<float>& response, int blockSize, int headLength, int partition)
        : headSamples(1, std::min(headLength, response.getNumSamples()))
    {
        headSamples.copyFrom(0, 0, response, 0, 0, headSamples.getNumSamples());
        head = std::make_unique<ModeledCabConvolution>(headSamples, blockSize, false);
        if (response.getNumSamples() > headLength)
            tail = std::make_unique<TwoLevelTail>(response.getReadPointer(0) + headLength,
                                                  response.getNumSamples() - headLength, partition, headLength);
    }
    TwoLevelTail* tailPart() noexcept { return tail.get(); }
    void process(juce::AudioBuffer<float>& buffer, std::counting_semaphore<>& wake) noexcept
    {
        const int n = buffer.getNumSamples();
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        // The tail reads this block's input before the head overwrites it.
        if (tail) for (int i = 0; i < n; ++i) tailOut[size_t(i)] = tail->step({left[i], right ? right[i] : 0.f}, wake);
        head->process(buffer);
        if (tail) for (int i = 0; i < n; ++i) { left[i] += tailOut[size_t(i)].real(); if (right) right[i] += tailOut[size_t(i)].imag(); }
    }
private:
    juce::AudioBuffer<float> headSamples;
    std::unique_ptr<ModeledCabConvolution> head;
    std::unique_ptr<TwoLevelTail> tail;
    std::array<Complex, 512> tailOut{};
};

} // namespace spectralforge::prototype

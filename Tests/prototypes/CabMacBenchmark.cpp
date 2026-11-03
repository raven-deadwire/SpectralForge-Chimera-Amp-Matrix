// PROTOTYPE (docs/CAB_TWO_LEVEL_CONVOLUTION_DESIGN.ko.md, option a). Not a CTest.
// Isolates the spectral multiply-accumulate of ModeledCabConvolution with the
// same data shapes and access order (96 kHz, block 32, stereo: FFT 128, 128
// complex bins, 86 IR partitions, a 3-bank input ring), and compares variants
// that keep the arithmetic bit-identical:
//   base  : the product loop (GCC auto-vectorised, SSE2 baseline)
//   avx2  : the same loop compiled for AVX2 (no FMA: -ffp-contract=off)
//   fuse2 : two partitions per pass, same per-element operation order
//   fuse4 : four partitions per pass, same order
// Each variant is timed per callback (all mics) for 1 and 6 mics, and its
// output spectrum is compared bit for bit with base.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

namespace {
constexpr int bins = 128, segments = 86, step = 3, inputSegments = step * segments;

struct Mic {
    std::vector<float> input, pastSum, output;
    const float* impulse{};
    int current{};
};

#define MAC_BODY(i, in, h, out) { \
    const float re=(out[i]+in[i]*h[i])-in[bins+i]*h[bins+i]; \
    const float im=(out[bins+i]+in[i]*h[bins+i])+in[bins+i]*h[i]; \
    out[i]=re;out[bins+i]=im; }

inline void accumulateBase(const float* __restrict in, const float* __restrict h, float* __restrict out) {
    for (int i = 0; i < bins; ++i) MAC_BODY(i, in, h, out)
}
__attribute__((target("avx2"))) void accumulateAvx2(const float* __restrict in, const float* __restrict h, float* __restrict out) {
    for (int i = 0; i < bins; ++i) MAC_BODY(i, in, h, out)
}
inline void accumulate2(const float* __restrict a, const float* __restrict ha,
                        const float* __restrict b, const float* __restrict hb, float* __restrict out) {
    for (int i = 0; i < bins; ++i) {
        float re = (out[i] + a[i] * ha[i]) - a[bins + i] * ha[bins + i];
        float im = (out[bins + i] + a[i] * ha[bins + i]) + a[bins + i] * ha[i];
        re = (re + b[i] * hb[i]) - b[bins + i] * hb[bins + i];
        im = (im + b[i] * hb[bins + i]) + b[bins + i] * hb[i];
        out[i] = re; out[bins + i] = im;
    }
}
inline void accumulate4(const float* const* x, const float* const* h, float* __restrict out) {
    const float* __restrict a = x[0]; const float* __restrict b = x[1]; const float* __restrict c = x[2]; const float* __restrict d = x[3];
    const float* __restrict ha = h[0]; const float* __restrict hb = h[1]; const float* __restrict hc = h[2]; const float* __restrict hd = h[3];
    for (int i = 0; i < bins; ++i) {
        float re = (out[i] + a[i] * ha[i]) - a[bins + i] * ha[bins + i];
        float im = (out[bins + i] + a[i] * ha[bins + i]) + a[bins + i] * ha[i];
        re = (re + b[i] * hb[i]) - b[bins + i] * hb[bins + i]; im = (im + b[i] * hb[bins + i]) + b[bins + i] * hb[i];
        re = (re + c[i] * hc[i]) - c[bins + i] * hc[bins + i]; im = (im + c[i] * hc[bins + i]) + c[bins + i] * hc[i];
        re = (re + d[i] * hd[i]) - d[bins + i] * hd[bins + i]; im = (im + d[i] * hd[bins + i]) + d[bins + i] * hd[i];
        out[i] = re; out[bins + i] = im;
    }
}

enum class Variant { base, avx2, fuse2, fuse4 };
const char* name(Variant v) { return v == Variant::base ? "base" : v == Variant::avx2 ? "avx2" : v == Variant::fuse2 ? "fuse2" : "fuse4"; }

// One mic's sumPartitions(firstChunk=true) + the current partition, as the product does.
void callback(Mic& m, Variant v) {
    const int bankStart = (m.current % step) * segments, bankOffset = m.current / step;
    const float* bank = m.input.data() + size_t(bankStart) * 2 * bins;
    std::fill(m.pastSum.begin(), m.pastSum.end(), 0.f);
    int index = bankOffset;
    static thread_local std::vector<const float*> xs; xs.clear();
    for (int p = 1; p < segments; ++p) { if (++index >= segments) index = 0; xs.push_back(bank + size_t(index) * 2 * bins); }
    const float* h0 = m.impulse;
    const auto h = [&](int p) { return m.impulse + size_t(p) * 2 * bins; };
    float* out = m.pastSum.data();
    int p = 1;
    if (v == Variant::base) for (; p < segments; ++p) accumulateBase(xs[size_t(p - 1)], h(p), out);
    else if (v == Variant::avx2) for (; p < segments; ++p) accumulateAvx2(xs[size_t(p - 1)], h(p), out);
    else if (v == Variant::fuse2) {
        for (; p + 1 < segments; p += 2) accumulate2(xs[size_t(p - 1)], h(p), xs[size_t(p)], h(p + 1), out);
        for (; p < segments; ++p) accumulateBase(xs[size_t(p - 1)], h(p), out);
    } else {
        for (; p + 3 < segments; p += 4) {
            const float* x4[4] = {xs[size_t(p - 1)], xs[size_t(p)], xs[size_t(p + 1)], xs[size_t(p + 2)]};
            const float* h4[4] = {h(p), h(p + 1), h(p + 2), h(p + 3)};
            accumulate4(x4, h4, out);
        }
        for (; p < segments; ++p) accumulateBase(xs[size_t(p - 1)], h(p), out);
    }
    std::copy(m.pastSum.begin(), m.pastSum.end(), m.output.begin());
    const float* currentInput = m.input.data() + size_t(bankStart + bankOffset) * 2 * bins;
    if (v == Variant::avx2) accumulateAvx2(currentInput, h0, m.output.data()); else accumulateBase(currentInput, h0, m.output.data());
    m.current = m.current > 0 ? m.current - 1 : inputSegments - 1;
}

std::vector<Mic> makeMics(int count, const std::vector<std::vector<float>>& impulses) {
    std::mt19937 rng(3); std::normal_distribution<float> d(0.f, 1.f);
    std::vector<Mic> mics(static_cast<size_t>(count));
    for (int m = 0; m < count; ++m) {
        auto& mic = mics[size_t(m)];
        mic.input.resize(size_t(inputSegments) * 2 * bins); for (auto& x : mic.input) x = d(rng);
        mic.pastSum.assign(2 * bins, 0.f); mic.output.assign(2 * bins, 0.f);
        mic.impulse = impulses[size_t(m % 2)].data(); // three cabs share the two mic models, as in the CAB test
    }
    return mics;
}
} // namespace

int main(int argc, char** argv) {
    const int callbacks = argc > 1 ? std::atoi(argv[1]) : 1600;
    std::mt19937 rng(5); std::normal_distribution<float> d(0.f, .01f);
    std::vector<std::vector<float>> impulses(2, std::vector<float>(size_t(segments) * 2 * bins));
    for (auto& h : impulses) for (auto& x : h) x = d(rng);
    int mismatches = 0;
    std::printf("MAC_PROTOCOL bins=%d segments=%d input_banks=%d callbacks=%d sleep_ms_every_8=1\n", bins, segments, step, callbacks);
    for (int count : {1, 6}) {
        // Bit-identity against base over a full ring cycle.
        for (Variant v : {Variant::avx2, Variant::fuse2, Variant::fuse4}) {
            auto a = makeMics(count, impulses), b = makeMics(count, impulses);
            for (int n = 0; n < inputSegments * 2; ++n) for (int m = 0; m < count; ++m) {
                callback(a[size_t(m)], Variant::base); callback(b[size_t(m)], v);
                if (std::memcmp(a[size_t(m)].output.data(), b[size_t(m)].output.data(), sizeof(float) * 2 * bins) != 0) { ++mismatches; std::printf("MISMATCH variant=%s mics=%d\n", name(v), count); n = 1 << 30; break; }
            }
        }
        for (int repeat = 0; repeat < 2; ++repeat) for (Variant v : {Variant::base, Variant::avx2, Variant::fuse2, Variant::fuse4}) {
            auto mics = makeMics(count, impulses);
            std::vector<double> t; t.reserve(size_t(callbacks));
            for (int n = 0; n < callbacks; ++n) {
                const auto s = std::chrono::steady_clock::now();
                for (auto& m : mics) callback(m, v);
                t.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - s).count());
                if (n % 8 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::sort(t.begin(), t.end());
            const double macs = double(count) * segments * bins;
            std::printf("MAC mics=%d variant=%-5s repeat=%d p50_us=%.1f p99_us=%.1f ns_per_complex_mac=%.3f\n",
                        count, name(v), repeat, t[t.size() / 2], t[size_t(double(t.size() - 1) * .99)], t[t.size() / 2] * 1000. / macs);
        }
    }
    std::printf(mismatches ? "MAC_RESULT mismatches=%d\n" : "MAC_RESULT bit_identical mismatches=%d\n", mismatches);
    return mismatches ? 1 : 0;
}

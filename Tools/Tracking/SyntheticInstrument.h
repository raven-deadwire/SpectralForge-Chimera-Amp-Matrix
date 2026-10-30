#pragma once
// Synthetic plucked-string performances with exact pitch labels, for the
// 1.10 tracking checks (docs/TRACKING_1_10.md). Not a guitar model: an
// additive string (inharmonic partials, pluck-position comb, per-partial
// decay, pick noise) whose instantaneous frequency is known at every sample,
// so bends and vibrato have an exact ground truth.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace spectralforge::tracking {

constexpr double pi = 3.14159265358979323846;

// A labelled note: [start, end) seconds; `hz(t)` the true pitch at t seconds
// after the start; `picked` false for a hammer-on / pull-off (no new attack).
struct Label {
    double start{}, end{};
    std::function<double(double)> hz;
    std::string name;
    bool picked{true};
    double pitchAt(double time) const { return hz ? hz(time - start) : 0.0; }
};

struct Performance {
    std::string name;
    double rate{48000};
    std::vector<float> audio;
    std::vector<Label> labels;
    // Notes the tracker is not expected to follow (outside 55-1400 Hz, or a
    // chord): measured and reported, not held to the acceptance targets.
    bool outOfScope{};
};

// One string note. `palm`: palm-muted (fast decay, dark). `evenOnly`: only
// even partials (a natural harmonic at the 12th fret sounds an octave up).
struct StringNote {
    double start{}, duration{};
    std::function<double(double)> hz;   // Hz at t seconds after the start
    double amplitude{.3};
    bool picked{true}, palm{}, evenOnly{};
    double pluckPosition{.18}, inharmonicity{.00012};
};

class StringRenderer {
public:
    explicit StringRenderer(double sampleRate, double seconds) : rate(sampleRate), out(size_t(sampleRate * seconds), 0.f) {}
    // Renders a note. A legato note (not picked) continues the level of the
    // note sounding at its start (`carried`), without a pick transient.
    void add(const StringNote& note, double carried = 0.0) {
        const int first = int(note.start * rate), length = int(note.duration * rate);
        const double f0 = note.hz(0.0);
        const double tau0 = note.palm ? .07 : std::clamp(3.0 * std::sqrt(82.4 / std::max(f0, 20.0)), .4, 4.0);
        const int partials = 40;
        std::vector<double> phase(size_t(partials + 1), 0.0), amp(size_t(partials + 1), 0.0), decay(size_t(partials + 1), 0.0), stretch(size_t(partials + 1), 1.0);
        for (int k = 1; k <= partials; ++k) {
            const bool silent = note.evenOnly && k % 2 == 1;
            amp[size_t(k)] = silent ? 0.0 : std::abs(std::sin(pi * k * note.pluckPosition)) / k;
            if (note.palm && k * f0 > 1200.0) amp[size_t(k)] = 0.0;
            decay[size_t(k)] = 1.0 / (rate * tau0 / (1.0 + .15 * std::pow(double(k), 1.2)));
            stretch[size_t(k)] = std::sqrt(1.0 + note.inharmonicity * k * k);
        }
        double norm = 0;for (int k = 1; k <= partials; ++k) norm += amp[size_t(k)];
        const double level = note.picked ? note.amplitude : std::max(carried, .05);
        const int attack = int((note.picked ? .002 : .001) * rate), release = int(.015 * rate);
        for (int n = 0; n < length && first + n < int(out.size()); ++n) {
            const double t = n / rate, f = note.hz(t);
            double y = 0;
            for (int k = 1; k <= partials; ++k) {
                if (amp[size_t(k)] == 0.0) continue;
                const double fk = f * k * stretch[size_t(k)];
                if (fk > .45 * rate) continue;
                phase[size_t(k)] += 2 * pi * fk / rate;
                y += amp[size_t(k)] * std::exp(-decay[size_t(k)] * n) * std::sin(phase[size_t(k)]);
            }
            double envelope = level / std::max(norm, 1e-9);
            if (n < attack) envelope *= double(n) / attack;
            if (n > length - release) envelope *= double(length - n) / release;
            out[size_t(first + n)] += float(envelope * y);
        }
        if (note.picked) {   // pick noise: a 4 ms band-limited burst
            std::uint32_t seed = std::uint32_t(first) * 2654435761u + 7u;double low = 0, prev = 0;
            for (int n = 0; n < int(.004 * rate) && first + n < int(out.size()); ++n) {
                seed = seed * 1664525u + 1013904223u;
                const double white = double(seed >> 8) / 8388608.0 - 1.0;
                low += .3 * (white - low);
                const double band = low - prev;prev = low;
                out[size_t(first + n)] += float(.25 * note.amplitude * band * (1.0 - double(n) / (.004 * rate)));
            }
        }
    }
    // The level a note started at `start` has after `t` seconds (for legato).
    static double levelAfter(const StringNote& note, double t) {
        const double f0 = note.hz(0.0);
        const double tau0 = note.palm ? .07 : std::clamp(3.0 * std::sqrt(82.4 / std::max(f0, 20.0)), .4, 4.0);
        return note.amplitude * std::exp(-t / tau0);
    }
    // Mains hum (50 Hz and odd harmonics) and hiss under everything.
    void addNoise(double humDb, double hissDb) {
        const double hum = std::pow(10.0, humDb / 20.0), hiss = std::pow(10.0, hissDb / 20.0);
        std::uint32_t seed = 12345u;
        for (size_t n = 0; n < out.size(); ++n) {
            const double t = n / rate;
            seed = seed * 1664525u + 1013904223u;
            out[n] += float(hum * (std::sin(2 * pi * 50 * t) + .5 * std::sin(2 * pi * 150 * t) + .25 * std::sin(2 * pi * 250 * t))
                            + hiss * (double(seed >> 8) / 8388608.0 - 1.0));
        }
    }
    std::vector<float> take() { return std::move(out); }
private:
    double rate;
    std::vector<float> out;
};

inline std::function<double(double)> steady(double hz) { return [hz](double) { return hz; }; }
inline double noteHz(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }

// The corpus: one performance per playing situation.
inline std::vector<Performance> corpus(double rate = 48000) {
    std::vector<Performance> result;
    const auto single = [&](const std::string& name, std::vector<StringNote> notes, double seconds, bool outOfScope = false,
                            std::vector<std::string> names = {}) {
        StringRenderer r(rate, seconds);
        Performance p;p.name = name;p.rate = rate;p.outOfScope = outOfScope;
        double carried = 0;
        for (size_t i = 0; i < notes.size(); ++i) {
            const auto& note = notes[i];
            if (!note.picked && i > 0) carried = StringRenderer::levelAfter(notes[i - 1], note.start - notes[i - 1].start);
            r.add(note, carried);
            p.labels.push_back({note.start, note.start + note.duration, note.evenOnly ? std::function<double(double)>([h = note.hz](double t) { return 2 * h(t); }) : note.hz,
                                i < names.size() ? names[i] : name, note.picked});
        }
        r.addNoise(-62, -78);
        p.audio = r.take();
        result.push_back(std::move(p));
    };
    const auto sustained = [&](const std::string& name, double hz, bool outOfScope = false) {
        single(name, {{.2, 1.0, steady(hz), .3}}, 1.6, outOfScope, {name});
    };
    sustained("B1 (7-string low B)", noteHz(35));
    sustained("A1 (drop A)", noteHz(33));
    sustained("E2 (open low E)", noteHz(40));
    sustained("E1 (bass open E, below range)", noteHz(28), true);
    sustained("B0 (5-string bass low B, below range)", noteHz(23), true);
    sustained("E4 (open high e)", noteHz(64));
    sustained("E6 (high e, 24th fret)", noteHz(88));
    // Bend: G3 up a whole tone over 150 ms from 250 ms, held.
    single("G3 bend +2 semitones", {{.2, 1.1, [](double t) { const double x = std::clamp((t - .25) / .15, 0.0, 1.0);return noteHz(55) * std::pow(2.0, 2.0 * (x * x * (3 - 2 * x)) / 12.0); }, .3}}, 1.5);
    // Vibrato: E4, +-30 cents at 6 Hz after 150 ms.
    single("E4 vibrato +-30 cents 6 Hz", {{.2, 1.2, [](double t) { return noteHz(64) * std::pow(2.0, (t > .15 ? .3 * std::sin(2 * pi * 6 * (t - .15)) : 0.0) / 12.0); }, .3}}, 1.6);
    // Legato: A3 picked, hammer-on B3, hammer-on C#4, pull-off B3.
    {
        std::vector<StringNote> n{{.2, .25, steady(noteHz(57)), .3}, {.45, .25, steady(noteHz(59)), .3, false}, {.7, .25, steady(noteHz(61)), .3, false}, {.95, .3, steady(noteHz(59)), .3, false}};
        single("A3-B3-C#4-B3 hammer-ons / pull-off", n, 1.5, false, {"A3 (picked)", "B3 (hammer-on)", "C#4 (hammer-on)", "B3 (pull-off)"});
    }
    // Palm mutes: four 90 ms E2 chugs, 125 ms apart (120 BPM 16ths).
    {
        std::vector<StringNote> n;
        for (int i = 0; i < 4; ++i) { StringNote s{.2 + .125 * i, .09, steady(noteHz(40)), .35};s.palm = true;n.push_back(s); }
        single("E2 palm-muted 16ths at 120 BPM", n, 1.0, false, {"E2 chug 1", "E2 chug 2", "E2 chug 3", "E2 chug 4"});
    }
    // Fast alternate picking: 16ths at 160 BPM (94 ms), A2 B2 C#3 D3 E3 D3 C#3 B3.
    {
        const std::array<int, 8> midi{45, 47, 49, 50, 52, 50, 49, 47};std::vector<StringNote> n;std::vector<std::string> names;
        for (int i = 0; i < 8; ++i) { n.push_back({.2 + .09375 * i, .09, steady(noteHz(midi[size_t(i)])), .3});names.push_back("16th " + std::to_string(i + 1));}
        single("16ths at 160 BPM, A2 to E3", n, 1.2, false, names);
    }
    // Natural harmonic at the A string's 12th fret: partials 2, 4, 6... of 110 Hz.
    {
        StringNote h{.2, 1.0, steady(110.0), .3};h.evenOnly = true;
        single("A string 12th-fret harmonic (220 Hz)", {h}, 1.5, false, {"harmonic 220 Hz"});
    }
    // Power chord E2 + B2 (a fifth): one label at the root; a mono tracker may
    // report either note or the common octave below.
    {
        StringRenderer r(rate, 1.6);Performance p;p.name = "E2 + B2 power chord";p.rate = rate;p.outOfScope = true;
        r.add({.2, 1.0, steady(noteHz(40)), .25});r.add({.2, 1.0, steady(noteHz(47)), .25});r.addNoise(-62, -78);
        p.audio = r.take();p.labels.push_back({.2, 1.2, steady(noteHz(40)), "E2+B2 (root)", true});
        result.push_back(std::move(p));
    }
    // Hum and hiss alone: the gate must stay shut.
    {
        StringRenderer r(rate, 1.5);r.addNoise(-62, -78);Performance p;p.name = "hum and hiss only";p.rate = rate;p.audio = r.take();
        result.push_back(std::move(p));
    }
    return result;
}

} // namespace spectralforge::tracking

#pragma once
// 1.10 tracking checks (docs/TRACKING_1_10.md): runs the SYNTH layer's own
// tracker and guitar gate over a recording and measures each labelled note.
// Shared by ChimeraTrackingTests (synthetic corpus) and the
// ChimeraTrackingCheck tool (recorded DI with a label file, or labels
// estimated by an offline reference tracker).
#include "SynthLayer.h"
#include "SyntheticInstrument.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace spectralforge::tracking {

// A minimal host for the SYNTH parameters: synth on, source Guitar.
struct TrackingHost final : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() { juce::AudioProcessorValueTreeState::ParameterLayout l;appendSynthParameters(l);return l; }
    explicit TrackingHost(float sensitivityDb) : state(*this, nullptr, "SYNTH", layout()) {
        set("synthOn", 1);set("synthSource", 1);set("synthSensitivity", sensitivityDb);
    }
    void set(const char* id, float v) { if (auto* p = state.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); }
    const juce::String getName() const override { return "tracking"; }
    void prepareToPlay(double, int) override {}void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }bool acceptsMidi() const override { return false; }bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }int getCurrentProgram() override { return 0; }void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}void setStateInformation(const void*, int) override {}
};

// One analysis frame per block.
struct Frame { double time{}, hz{}, clarity{}, rms{}, voiceHz{};bool gate{};int attacks{}; };

inline std::vector<Frame> analyse(const std::vector<float>& audio, double rate, float sensitivityDb = -40.f, int block = 64) {
    TrackingHost host(sensitivityDb);
    SynthLayer synth;synth.bind(host.state);synth.prepare(rate, block);
    juce::AudioBuffer<float> in(1, block);juce::MidiBuffer midi;std::vector<Frame> frames;
    for (size_t at = 0; at + size_t(block) <= audio.size(); at += size_t(block)) {
        for (int n = 0; n < block; ++n) in.setSample(0, n, audio[at + size_t(n)]);
        synth.handleMidi(midi);synth.analyse(in);synth.render(block, 120);
        const auto& t = synth.pitchTracker();
        frames.push_back({double(at + size_t(block)) / rate, t.hz(), t.clarity(), t.rms(), synth.voicePitch(-1), synth.guitarGateOpen(), synth.guitarAttacks()});
    }
    return frames;
}

inline double cents(double hz, double reference) { return hz > 0 && reference > 0 ? 1200.0 * std::log2(hz / reference) : 1e9; }

struct NoteResult {
    Label label;
    double lockMs{-1}, gateMs{-1};   // -1: never
    double medianCents{1e9}, worstCents{1e9};
    double lagMs{};   // the tracking lag that best explains a moving pitch (bends, vibrato)
    double octaveShare{1}, dropoutShare{1};
    int attacks{};
};
struct Report {
    std::string name;
    std::vector<NoteResult> notes;
    double falseGateSeconds{}, seconds{};
    bool outOfScope{};
};

// Per note: lock = the first frame within +-50 cents of the label (and
// clarity above 0.8); pitch error over the frames after the lock until 10 ms
// before the end; octave errors = frames off by more than 600 cents;
// dropouts = frames after the lock with the gate shut while the level is
// above SENSITIVITY (a note that has decayed below it may close); attacks = new guitar
// voice starts from 5 ms before the start to the end. A gate open outside
// every note (and 120 ms of release after it) is a false trigger.
inline Report evaluate(const std::string& name, const std::vector<Frame>& frames, const std::vector<Label>& labels, bool outOfScope = false, double sensitivityDb = -40) {
    const double threshold = std::pow(10.0, sensitivityDb / 20.0);
    Report report;report.name = name;report.outOfScope = outOfScope;
    report.seconds = frames.empty() ? 0 : frames.back().time;
    for (const auto& label : labels) {
        NoteResult r;r.label = label;
        int attacksBefore = -1, attacksEnd = 0;
        std::vector<std::pair<double, double>> tracked;   // time, Hz after the lock
        int octave = 0, dropped = 0, counted = 0;
        for (const auto& f : frames) {
            if (attacksBefore < 0 && f.time >= label.start - .005) attacksBefore = f.attacks;
            if (f.time < label.start || f.time > label.end) continue;
            attacksEnd = f.attacks;
            const double since = (f.time - label.start) * 1000;
            if (r.gateMs < 0 && f.gate) r.gateMs = since;
            const double error = cents(f.hz, label.pitchAt(f.time));
            if (r.lockMs < 0 && std::abs(error) <= 50 && f.clarity > .8) r.lockMs = since;
            if (r.lockMs < 0 || f.time > label.end - .01) continue;
            ++counted;
            if (!f.gate && f.rms > threshold * 1.12) ++dropped;   // 1 dB above it, so a level at the threshold is not a dropout
            if (std::abs(error) > 600) ++octave;
            tracked.push_back({f.time, f.hz});
        }
        if (!tracked.empty()) {
            // A tracker reports a moving pitch late: compare with the label
            // delayed by the lag (0-40 ms) that fits best, and report the lag.
            for (double lag = 0; lag <= .0401; lag += .001) {
                std::vector<double> errors;
                for (const auto& [time, hz] : tracked) errors.push_back(std::abs(cents(hz, label.pitchAt(std::max(label.start, time - lag)))));
                std::sort(errors.begin(), errors.end());
                const double median = errors[errors.size() / 2];
                if (median < r.medianCents - 1e-9) { r.medianCents = median;r.worstCents = errors[size_t(.95 * double(errors.size() - 1))];r.lagMs = lag * 1000; }
            }
            r.octaveShare = double(octave) / counted;r.dropoutShare = double(dropped) / counted;
        }
        r.attacks = attacksBefore < 0 ? 0 : attacksEnd - attacksBefore;
        report.notes.push_back(r);
    }
    for (size_t i = 1; i < frames.size(); ++i) {
        if (!frames[i].gate) continue;
        bool inside = false;
        for (const auto& l : labels) inside |= frames[i].time >= l.start && frames[i].time <= l.end + .12;
        if (!inside) report.falseGateSeconds += frames[i].time - frames[i - 1].time;
    }
    return report;
}

// Labels for a recording without a label file: an offline reference (YIN at
// a 24 kHz analysis rate, 42 ms window, 5 ms hop, non-causal median of five)
// segmented into notes where the pitch holds within 40 cents for 40 ms. They
// are estimates, reported as such: good for steady notes, not for bends.
inline std::vector<Label> referenceLabels(const std::vector<float>& audio, double rate, double thresholdDb = -40) {
    const int factor = std::max(1, int(std::lround(rate / 24000.0)));
    std::vector<float> x;for (size_t n = 0; n < audio.size(); n += size_t(factor)) { double s = 0;for (int k = 0; k < factor && n + size_t(k) < audio.size(); ++k) s += audio[n + size_t(k)];x.push_back(float(s / factor)); }
    const double r = rate / factor;
    const int window = int(.042 * r), tauMin = int(r / 2000), tauMax = int(r / 27.5), hop = int(.005 * r);
    std::vector<double> pitch;std::vector<double> d(size_t(tauMax + 2));
    const double threshold = std::pow(10.0, thresholdDb / 20.0);
    for (int start = 0; start + window + tauMax < int(x.size()); start += hop) {
        double energy = 0;for (int j = 0; j < window; ++j) energy += double(x[size_t(start + j)]) * x[size_t(start + j)];
        if (std::sqrt(energy / window) < threshold) { pitch.push_back(0);continue; }
        double running = 0;int best = -1;
        for (int tau = 1; tau <= tauMax; ++tau) {
            double s = 0;for (int j = 0; j < window; ++j) { const double e = double(x[size_t(start + j)]) - x[size_t(start + j + tau)];s += e * e; }
            running += s;d[size_t(tau)] = running > 0 ? s * tau / running : 1;
        }
        for (int tau = std::max(2, tauMin); tau < tauMax; ++tau) if (d[size_t(tau)] < .1) { while (tau + 1 < tauMax && d[size_t(tau + 1)] < d[size_t(tau)]) ++tau;best = tau;break; }
        if (best < 0) { pitch.push_back(0);continue; }
        const double a = d[size_t(best - 1)], b = d[size_t(best)], c = d[size_t(best + 1)], den = a - 2 * b + c;
        pitch.push_back(r / (best + (std::abs(den) > 1e-12 ? std::clamp(.5 * (a - c) / den, -.5, .5) : 0.0)));
    }
    std::vector<double> smooth(pitch.size());
    for (size_t i = 0; i < pitch.size(); ++i) { std::vector<double> w;for (size_t j = i < 2 ? 0 : i - 2; j <= std::min(pitch.size() - 1, i + 2); ++j) w.push_back(pitch[j]);std::sort(w.begin(), w.end());smooth[i] = w[w.size() / 2]; }
    std::vector<Label> labels;size_t i = 0;const double hopSeconds = double(hop) / r;
    while (i < smooth.size()) {
        if (smooth[i] <= 0) { ++i;continue; }
        size_t j = i;std::vector<double> run;
        while (j < smooth.size() && smooth[j] > 0 && std::abs(cents(smooth[j], smooth[i])) < 40) run.push_back(smooth[j++]);
        if (double(run.size()) * hopSeconds >= .04) {
            std::sort(run.begin(), run.end());const double hz = run[run.size() / 2];
            const int midi = int(std::lround(69 + 12 * std::log2(hz / 440)));
            labels.push_back({double(i) * hopSeconds, double(j) * hopSeconds, steady(hz), "reference MIDI " + std::to_string(midi), true});
        }
        i = std::max(j, i + 1);
    }
    return labels;
}

// Label file: one note per line, `start_seconds,end_seconds,pitch[,name]`
// where pitch is Hz (a number with a decimal point or above 127) or a MIDI
// note number. Lines starting with # are comments.
inline std::vector<Label> readLabels(const std::string& text) {
    std::vector<Label> labels;std::istringstream lines(text);std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> cells;std::stringstream row(line);std::string cell;
        while (std::getline(row, cell, ',')) cells.push_back(cell);
        if (cells.size() < 3) continue;
        try {
            const double start = std::stod(cells[0]), end = std::stod(cells[1]), value = std::stod(cells[2]);
            const bool isHz = cells[2].find('.') != std::string::npos || value > 127;
            labels.push_back({start, end, steady(isHz ? value : noteHz(value)), cells.size() > 3 ? cells[3] : cells[2], true});
        } catch (...) { continue; }
    }
    return labels;
}

// Acceptance targets for notes inside the tracker's range (55-1400 Hz).
// Below 55 Hz (bass E1, B0) the tracker needs its long window, two cycles
// at 27.5 Hz (73 ms): those notes may take 80 ms to lock. Up to B1 (62 Hz)
// more inharmonic partials fall in the band, which reads the period short:
// those notes may read 12 cents off.
struct Targets { double lockMs{60}, medianCents{10}, lagMs{25}, octaveShare{.02}, dropoutShare{.05}; double falseGateSeconds{.05};
                 double extendedBelowHz{55}, extendedLockMs{80}, lowBelowHz{62}, lowMedianCents{12}; };
inline bool meets(const NoteResult& r, const Targets& t, bool legato) {
    const double pitch = r.label.pitchAt(r.label.start);
    const double lockTarget = pitch < t.extendedBelowHz ? t.extendedLockMs : t.lockMs, centsTarget = pitch < t.lowBelowHz ? t.lowMedianCents : t.medianCents;
    return r.lockMs >= 0 && r.lockMs <= lockTarget && r.medianCents <= centsTarget && r.lagMs <= t.lagMs && r.octaveShare <= t.octaveShare
        && r.dropoutShare <= t.dropoutShare && (legato ? r.attacks <= 1 : r.attacks == 1);
}

inline std::string markdown(const std::vector<Report>& reports, const Targets& targets) {
    std::ostringstream o;o.setf(std::ios::fixed);o.precision(1);
    o << "| Recording | Note | Lock ms | Gate ms | Lag ms | Median cents | 95% cents | Octave % | Dropout % | Attacks | Meets |\n|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (const auto& r : reports) {
        for (const auto& n : r.notes) {
            const auto number = [&](double v) { std::ostringstream s;s.setf(std::ios::fixed);s.precision(1);if (v < 0 || v >= 1e8) s << "-";else s << v;return s.str(); };
            o << "| " << r.name << " | " << n.label.name << " | " << number(n.lockMs) << " | " << number(n.gateMs) << " | " << number(n.lockMs < 0 ? -1 : n.lagMs) << " | " << number(n.medianCents) << " | " << number(n.worstCents)
              << " | " << number(n.lockMs < 0 ? -1 : n.octaveShare * 100) << " | " << number(n.lockMs < 0 ? -1 : n.dropoutShare * 100) << " | " << n.attacks << " | "
              << (r.outOfScope ? "out of scope" : meets(n, targets, !n.label.picked) ? "yes" : "**no**") << " |\n";
        }
 if (r.falseGateSeconds > 0 || r.notes.empty()) o << "| " << r.name << " | (false triggers) | | | | | | | | | " << int(std::lround(r.falseGateSeconds * 1000)) << " ms gate open outside notes |\n";
    }
    return o.str();
}

} // namespace spectralforge::tracking

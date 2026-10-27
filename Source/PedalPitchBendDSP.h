#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

namespace spectralforge {

// Pitch Bend (PRE model 51, docs/PRE_EXPANSION_B_1_5.md): an original
// treadle pitch shifter laid out after the published DigiTech Whammy (5th
// generation): one selector over 10 Whammy, 9 Harmony and 2 Detune settings,
// a CLASSIC / CHORDS switch and a treadle that sets the bend. The interval
// list follows the commonly published Whammy 5 layout but was not checked
// against the manual itself. The shifter (two crossfaded delay taps), its
// window lengths, the detune amounts and the dive-bomb depth are Chimera
// choices; no hardware response is claimed. On a tracked single note the
// taps crossfade in phase (even-period windows); chords use the base window,
// which CHORDS makes longer. Whammy
// settings output only the shifted signal, Harmony and Detune add it to the
// dry signal. The shifted signal lags the input by up to one window (not
// reported as latency).
class PedalPitchBend {
public:
    enum Kind { whammy, harmony, detune };
    struct Setting { const char* name; Kind kind; float heel, toe; };   // semitones at heel and toe
    static constexpr int settingCount = 21;
    static constexpr std::array<Setting, settingCount> settings{{
        {"2 OCT UP", whammy, 0, 24}, {"OCT UP", whammy, 0, 12}, {"5TH UP", whammy, 0, 7}, {"4TH UP", whammy, 0, 5},
        {"2ND DN", whammy, 0, -2}, {"4TH DN", whammy, 0, -5}, {"5TH DN", whammy, 0, -7}, {"OCT DN", whammy, 0, -12},
        {"2 OCT DN", whammy, 0, -24}, {"DIVE BOMB", whammy, 0, -36},
        {"2ND UP-3RD UP", harmony, 2, 4}, {"b3 UP-3RD UP", harmony, 3, 4}, {"3RD UP-4TH UP", harmony, 4, 5}, {"4TH UP-5TH UP", harmony, 5, 7},
        {"5TH UP-6TH UP", harmony, 7, 9}, {"5TH UP-7TH UP", harmony, 7, 10}, {"4TH DN-3RD DN", harmony, -5, -4}, {"5TH DN-4TH DN", harmony, -7, -5},
        {"OCT DN-OCT UP", harmony, -12, 12},
        {"DETUNE SHALLOW", detune, 0, -.1f}, {"DETUNE DEEP", detune, 0, -.25f}}};
    // Base windows: CLASSIC short, CHORDS longer (smoother on chords, slower
    // attack).
    static constexpr std::array<float, 2> windowSeconds{.025f, .06f};
    static float semitones(int setting, float position) noexcept {
        const auto& s = settings[size_t(juce::jlimit(0, settingCount - 1, setting))];
        return s.heel + (s.toe - s.heel) * juce::jlimit(0.f, 1.f, position);
    }

    void prepare(double sampleRate) {
        rate = sampleRate;
        length = int(std::ceil(rate * (windowSeconds[1] + 1. / 40.))) + 4;   // an even-period window can reach one 40 Hz period beyond the base
        for (auto& line : lines) line.assign(size_t(length), 0.f);
        trackerPole = float(1. - std::exp(-juce::MathConstants<double>::twoPi * 650. / rate));
        envelopeRelease = float(std::exp(-1. / (rate * .05)));
        bend.reset(rate, .02); dryLevel.reset(rate, .01); fade.reset(rate, .005);
        reset();
    }
    void reset() noexcept {
        for (auto& line : lines) std::fill(line.begin(), line.end(), 0.f);
        write = 0; primed = false; activeMode = -1; longest = 0.;
        low1 = low2 = envelope = previous = 0.f; armed = false; clock = 0.; lastCross = -1.; period = 0.;
        fade.setCurrentAndTargetValue(1.f);
    }

    // controls: position (treadle), setting (0-20), mode (0 classic, 1 chords).
    void process(juce::AudioBuffer<float>& buffer, const std::array<float, 12>& c) noexcept {
        const int setting = juce::jlimit(0, settingCount - 1, juce::roundToInt(c[1])), mode = juce::jlimit(0, 1, juce::roundToInt(c[2]));
        bend.setTargetValue(semitones(setting, c[0]));
        dryLevel.setTargetValue(settings[size_t(setting)].kind == whammy ? 0.f : 1.f);
        if (!primed) {
            bend.setCurrentAndTargetValue(bend.getTargetValue()); dryLevel.setCurrentAndTargetValue(dryLevel.getTargetValue());
            activeMode = mode; primed = true;
            const double base = rate * windowSeconds[size_t(mode)];
            taps = {{{base / 2, base}, {0., base}}}; longest = base;
        }
        // A CLASSIC / CHORDS change fades the shifted signal out, swaps the
        // mode and fades back in.
        if (mode != activeMode) fade.setTargetValue(0.f);
        const int channels = std::min(2, buffer.getNumChannels());
        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            if (mode != activeMode && fade.getCurrentValue() <= 0.f) { activeMode = mode; fade.setTargetValue(1.f); }
            float mono = 0.f;
            for (int ch = 0; ch < channels; ++ch) mono += buffer.getSample(ch, n);
            track(mono / float(channels));
            // With a tracked period the window is an even number of periods, so
            // the two taps, half a window apart, read a single note in phase
            // while they crossfade; untracked input (chords) keeps the base
            // window. CHORDS uses the longer base.
            const double base = rate * windowSeconds[size_t(activeMode)];
            const double window = std::min(double(length - 4), period > 0. ? 2. * std::max(1., std::round(base / (2. * period))) * period : base);
            const double ratio = std::pow(2., bend.getNextValue() / 12.);
            // Each tap's delay moves by (1 - ratio) per sample. Leaving its window
            // (where its gain is zero) it takes the current window and restarts
            // half of it from the other tap. At exact unison the taps drift
            // slowly until one sits mid-window alone (no comb).
            const double step = std::abs(ratio - 1.) > 1e-6 ? 1. - ratio : juce::jlimit(-unisonDrift, unisonDrift, taps[0].window / 2 - taps[0].delay);
            for (auto& t : taps) t.delay += step;
            for (size_t i = 0; i < taps.size(); ++i) {
                auto& t = taps[i];
                if (t.delay >= 0. && t.delay < t.window) continue;
                const auto& other = taps[1 - i];
                t.window = window; longest = std::max(longest, window);
                t.delay = juce::jlimit(0., window * (1. - 1e-9), step > 0. ? other.delay - window / 2 : other.delay + window / 2);
            }
            const float gainA = sine2(taps[0]), gainB = sine2(taps[1]), norm = 1.f / std::max(.5f, gainA + gainB);
            const float wet = fade.getNextValue() * norm, dry = dryLevel.getNextValue();
            for (int ch = 0; ch < channels; ++ch) {
                auto& line = lines[size_t(ch)];
                const float x = buffer.getSample(ch, n);
                line[size_t(write)] = x;
                buffer.setSample(ch, n, dry * x + wet * (gainA * read(line, taps[0].delay) + gainB * read(line, taps[1].delay)));
            }
            if (++write == length) write = 0;
        }
    }
    // Tracked input period in samples (0 when nothing is tracked), the longest
    // window used since reset and the delay line's reach (tests).
    double trackedPeriod() const noexcept { return period; }
    double longestWindow() const noexcept { return longest; }
    double capacity() const noexcept { return double(length - 1); }

private:
    struct Tap { double delay, window; };
    static constexpr double unisonDrift = .01;   // about 17 cents while settling at unison
    static float sine2(const Tap& t) noexcept { const float s = float(std::sin(juce::MathConstants<double>::pi * t.delay / t.window)); return s * s; }
    // Period from interpolated upward zero crossings of the low-passed input,
    // armed by a swing below a level-relative threshold; 40 Hz-1.5 kHz.
    void track(float x) noexcept {
        low1 += trackerPole * (x - low1); low2 += trackerPole * (low1 - low2);
        envelope = std::max(std::abs(low2), envelope * envelopeRelease);
        const float threshold = std::max(1.0e-4f, envelope * .1f);
        if (low2 < -threshold) armed = true;
        if (armed && previous <= 0.f && low2 > 0.f) {
            const double cross = clock - double(low2) / double(low2 - previous);
            if (lastCross >= 0.) {
                const double p = cross - lastCross;
                period = p >= rate / 1500. && p <= rate / 40. ? p : 0.;
            }
            lastCross = cross; armed = false;
        }
        if (lastCross >= 0. && clock - lastCross > rate / 20.) { period = 0.; lastCross = -1.; }
        previous = low2; clock += 1.;
    }
    float read(const std::vector<float>& line, double delay) const noexcept {
        double position = double(write) - delay;
        if (position < 0) position += length;
        const int i = int(position);
        const float f = float(position - i), a = line[size_t(i)], b = line[size_t(i + 1 == length ? 0 : i + 1)];
        return a + f * (b - a);
    }
    double rate{48000.}, clock{}, lastCross{-1.}, period{}, longest{};
    int length{1}, write{}, activeMode{-1};
    bool primed{}, armed{};
    float trackerPole{}, envelopeRelease{}, low1{}, low2{}, envelope{}, previous{};
    std::array<Tap, 2> taps{};
    std::array<std::vector<float>, 2> lines;
    juce::SmoothedValue<float> bend, dryLevel, fade;
};

} // namespace spectralforge

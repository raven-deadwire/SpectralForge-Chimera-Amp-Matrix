#include "Amplifier.h"
#include "NewAmpDSP.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Synthetic numerical regression checks. These do not certify a match to any
// physical amplifier or replace listening tests and host validation.
namespace {
using namespace spectralforge;
constexpr std::array<int, 8> channelCounts{{4, 2, 4, 1, 3, 3, 1, 2}};
constexpr std::array<int, 8> defaultChannels{{2, 1, 2, 0, 2, 0, 0, 1}};
void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}
std::string label(int model, int channel)
{
    return "model " + std::to_string(model) + " channel " + std::to_string(channel);
}
float stimulus(int sample, double rate)
{
    const double t = double(sample) / rate;
    const auto partial = [t](double frequency) {
        return std::sin(juce::MathConstants<double>::twoPi * frequency * t);
    };
    const double envelope = .18 * (.65 + .35 * std::sin(juce::MathConstants<double>::twoPi * 13.1 * t));
    return float(envelope * (partial(73.4162) + .4 * partial(220) + .19 * partial(997) + .07 * partial(3217)));
}
using Samples = std::array<std::vector<float>, 2>;
Samples process(Amp& amp, double rate, int blockSize, int length, int stereoMode = 0,
                float scale = 1.f, int inputOffset = 0)
{
    Samples result;
    for (auto& channel : result) channel.resize((size_t)length);
    juce::AudioBuffer<float> block(2, blockSize);
    for (int offset = 0; offset < length; offset += blockSize) {
        const int count = std::min(blockSize, length - offset);
        block.setSize(2, count, false, false, true);
        for (int n = 0; n < count; ++n) {
            const float x = scale * stimulus(inputOffset + offset + n, rate);
            block.setSample(0, n, stereoMode == 2 ? 0.f : x);
            block.setSample(1, n, stereoMode == 0 ? 0.f : x);
        }
        amp.process(block);
        for (int c = 0; c < 2; ++c)
            std::copy_n(block.getReadPointer(c), count, result[(size_t)c].begin() + offset);
    }
    return result;
}
Samples render(int model, int channel, double rate, int blockSize, int oversampling,
               float drive = .68f, int stereoMode = 0, double seconds = .14,
               bool explicitChannel = true, bool extremeTone = false, float scale = 1.f)
{
    Amp amp;
    amp.prepare({rate, (juce::uint32)blockSize, 2});
    if (explicitChannel) amp.set(static_cast<AmpModel>(model), drive, channel);
    else amp.set(static_cast<AmpModel>(model), drive);
    amp.setDriveImmediately(drive);
    amp.setOversampling(oversampling);
    amp.reset();
    amp.tone(extremeTone ? 12.f : 0.f, extremeTone ? -12.f : 0.f,
             extremeTone ? 12.f : 0.f, extremeTone ? -12.f : 0.f,
             extremeTone ? 10.f : 0.f, extremeTone ? 10.f : 0.f);
    return process(amp, rate, blockSize, int(rate * seconds), stereoMode, scale);
}
double rms(const std::vector<float>& audio, int start = 0)
{
    double sum = 0;
    for (size_t n = (size_t)start; n < audio.size(); ++n) sum += double(audio[n]) * audio[n];
    return std::sqrt(sum / double(audio.size() - (size_t)start));
}
double difference(const std::vector<float>& a, const std::vector<float>& b, int start = 0)
{
    require(a.size() == b.size(), "Mismatched comparison length");
    double maximum = 0;
    for (size_t n = (size_t)start; n < a.size(); ++n)
        maximum = std::max(maximum, std::abs(double(a[n]) - b[n]));
    return maximum;
}
double normalisedDifference(const std::vector<float>& a, const std::vector<float>& b, int start)
{
    const double ar = rms(a, start), br = rms(b, start);
    require(ar > 1.e-8 && br > 1.e-8, "Cannot compare a silent amp voice");
    double sum = 0;
    for (size_t n = (size_t)start; n < a.size(); ++n)
        sum += std::pow(a[n] / ar - b[n] / br, 2);
    return std::sqrt(sum / double(a.size() - (size_t)start));
}
void contracts()
{
    static_assert(static_cast<int>(AmpModel::glass) == 0);
    static_assert(static_cast<int>(AmpModel::britEdge) == 1);
    static_assert(static_cast<int>(AmpModel::tight515) == 2);
    static_assert(static_cast<int>(AmpModel::wideRect) == 3);
    static_assert(static_cast<int>(AmpModel::liquidLead) == 4);
    static_assert(static_cast<int>(AmpModel::ironTube) == 5);
    static_assert(static_cast<int>(AmpModel::solidPunch) == 6);
    static_assert(static_cast<int>(AmpModel::modernBass) == 7);
    static_assert(static_cast<int>(AmpModel::chime30) == 8);
    static_assert(static_cast<int>(AmpModel::orangeCrown) == 9);
    static_assert(static_cast<int>(AmpModel::bassmanValve) == 10);
    static_assert(static_cast<int>(AmpModel::subwayClean) == 11);
    static_assert(static_cast<int>(AmpModel::matchChime) == 12);
    static_assert(static_cast<int>(AmpModel::silkODS) == 13);
    static_assert(static_cast<int>(AmpModel::tastePunch) == 14);
    static_assert(static_cast<int>(AmpModel::zutaCinder) == 15);
    static_assert(static_cast<int>(AmpModel::ironCompact) == 16);
    static_assert(static_cast<int>(AmpModel::fourChannel) == 17);
    static_assert(static_cast<int>(AmpModel::classicTube) == 18);
    static_assert(static_cast<int>(AmpModel::sunMonolith) == 19);
    static_assert(static_cast<int>(AmpModel::evilHarvest) == 20);
    static_assert(static_cast<int>(AmpModel::hotLead) == 21);
    static_assert(static_cast<int>(AmpModel::blueStorm) == 22);
    for (int model = 15; model < 23; ++model) {
        const size_t index = (size_t)(model - 15);
        require(newAmpChannelCount(model) == channelCounts[index], "Wrong channel count: " + label(model, 0));
        require(newAmpDefaultChannel(model) == defaultChannels[index], "Wrong default channel: " + label(model, 0));
        for (int channel = 0; channel < channelCounts[index]; ++channel) {
            const auto* name = newAmpChannelName(model, channel);
            require(name != nullptr && *name != '\0', "Unnamed native channel: " + label(model, channel));
        }
        const auto implicit = render(model, 0, 48000, 257, 2, .68f, 0, .1, false);
        const auto explicitDefault = render(model, defaultChannels[index], 48000, 257, 2, .68f, 0, .1);
        require(difference(implicit[0], explicitDefault[0]) == 0, "Two-argument set changed native default: " + label(model, 0));
        Amp amp;
        amp.prepare({48000, 257, 2});
        amp.set(static_cast<AmpModel>(model), .5f, -20);
        require(amp.nativeChannel() == 0, "Negative native channel did not clamp");
        amp.setNativeChannel(200);
        require(amp.nativeChannel() == channelCounts[index] - 1, "High native channel did not clamp");
    }
    std::cout << "PASS: stable 0..14 indices; eight appended models, 20 native channels, defaults, names and channel clamping\n";
}
void numericalMatrix()
{
    double worstPartition = 0, worstStereo = 0, peak = 0;
    int cases = 0;
    for (int model = 15; model < 23; ++model)
        for (int channel = 0; channel < newAmpChannelCount(model); ++channel)
            for (double rate : {48000., 96000.}) for (int os : {0, 1, 2, 3}) {
                const auto left = render(model, channel, rate, 64, os);
                const auto mirror = render(model, channel, rate, 257, os, .68f, 1);
                const auto right = render(model, channel, rate, 257, os, .68f, 2);
                const std::string where = label(model, channel) + " at " + std::to_string(int(rate)) + " Hz OS " + std::to_string(os);
                require(rms(left[0]) > 1.e-5, "Unexpectedly silent " + where);
                for (size_t n = 0; n < left[0].size(); ++n) {
                    for (const auto* output : {&left, &mirror, &right}) for (int c = 0; c < 2; ++c) {
                        const float x = (*output)[(size_t)c][n];
                        require(std::isfinite(x) && std::abs(x) < 8.f, "Non-finite or excessive output: " + where);
                        peak = std::max(peak, std::abs(double(x)));
                    }
                    require(std::abs(left[1][n]) < 1.e-9f && std::abs(right[0][n]) < 1.e-9f,
                            "Silent stereo channel has signal: " + where);
                }
                const auto partition = difference(left[0], mirror[0]);
                const auto stereo = std::max(difference(mirror[0], mirror[1]), difference(mirror[1], right[1]));
                require(partition < 2.e-5, "Block partition changes output: " + where);
                require(stereo < 1.e-7, "Stereo states are coupled: " + where);
                worstPartition = std::max(worstPartition, partition);
                worstStereo = std::max(worstStereo, stereo);
                ++cases;
            }
    for (int model = 15; model < 23; ++model) for (int channel = 0; channel < newAmpChannelCount(model); ++channel) {
        const auto corner = render(model, channel, 48000, 257, 3, 1.f, 1, .15, true, true, 4.f);
        for (float x : corner[0])
            require(std::isfinite(x) && std::abs(x) < 64.f, "Extreme drive/tone instability: " + label(model, channel));
        const auto silence = render(model, channel, 48000, 64, 0, 1.f, 1, .04, true, false, 0.f);
        require(rms(silence[0]) < 1.e-10 && rms(silence[1]) < 1.e-10,
                "Reset silence generates audio: " + label(model, channel));
    }
    std::cout << "MEASURE new amp matrix: " << cases << " rate/channel/OS cases, three stereo/block arrangements each; peak "
              << peak << ", partition residual " << worstPartition << ", stereo residual " << worstStereo << '\n';
    std::cout << "PASS: all native channels, 48/96 kHz, 1x/2x/4x/8x, 64/257 samples, finite bounds, silence, stereo isolation and tone/drive corners\n";
}
void distinctVoicesAndDrive()
{
    std::array<Samples, 8> defaults;
    double leastVoice = std::numeric_limits<double>::max(), leastChannel = leastVoice, leastDrive = leastVoice;
    for (int model = 15; model < 23; ++model) {
        std::vector<Samples> channels;
        for (int channel = 0; channel < newAmpChannelCount(model); ++channel) {
            channels.push_back(render(model, channel, 48000, 257, 2));
            const auto lowDrive = render(model, channel, 48000, 257, 2, .05f);
            const auto highDrive = render(model, channel, 48000, 257, 2, .95f);
            const auto response = normalisedDifference(lowDrive[0], highDrive[0], 2400);
            require(response > .001, "Drive has no meaningful shape response: " + label(model, channel));
            leastDrive = std::min(leastDrive, response);
            for (int previous = 0; previous < channel; ++previous) {
                const auto residual = normalisedDifference(channels[(size_t)previous][0], channels.back()[0], 2400);
                require(residual > .01, "Native channels differ only in gain: " + label(model, channel));
                leastChannel = std::min(leastChannel, residual);
            }
        }
        defaults[(size_t)(model - 15)] = channels[(size_t)newAmpDefaultChannel(model)];
        for (int previous = 15; previous < model; ++previous) {
            const auto residual = normalisedDifference(defaults[(size_t)(previous - 15)][0], defaults[(size_t)(model - 15)][0], 2400);
            require(residual > .02, "New default voices differ only in gain: " + label(model, 0));
            leastVoice = std::min(leastVoice, residual);
        }
    }
    std::cout << "MEASURE minimum RMS-matched residual: default voices " << leastVoice
              << ", native channels " << leastChannel << ", drive .05/.95 " << leastDrive << '\n';
    std::cout << "PASS: native channels and new default voices differ beyond output level; drive changes waveform shape\n";
}
void resetAndRecovery()
{
    double worstReset = 0, largestHistory = 0, worstRecovery = 0;
    for (int model = 15; model < 23; ++model) for (int channel = 0; channel < newAmpChannelCount(model); ++channel)
        for (float drive : {.8f, 1.f}) {
        const auto where = label(model, channel) + " drive " + std::to_string(drive);
        Amp amp;
        amp.prepare({48000, 257, 2});
        amp.set(static_cast<AmpModel>(model), drive, channel);
        amp.setDriveImmediately(drive);amp.setOversampling(1);amp.reset();amp.tone(0, 0, 0, 0, 0, 0);
        const auto cold = process(amp, 48000, 257, 4800, 0, .1f);
        process(amp, 48000, 257, 24000, 0, 4.f);
        amp.reset();
        const auto reset = process(amp, 48000, 257, 4800, 0, .1f);
        const auto resetError = difference(cold[0], reset[0]);
        require(resetError == 0, "Reset retains DSP history: " + where);
        worstReset = std::max(worstReset, resetError);

        amp.reset();process(amp, 48000, 257, 24000, 0, 4.f);
        const auto hot = process(amp, 48000, 257, 4800, 0, .1f);
        const double history = difference(cold[0], hot[0]);
        require(history > 1.e-6, "Signal history has no dynamic effect: " + where);
        largestHistory = std::max(largestHistory, history);

        amp.reset();process(amp, 48000, 257, 24000, 0, 4.f);
        const auto decay = process(amp, 48000, 257, 96000, 0, 0.f);
        require(rms(decay[0], 90000) < 1.e-5, "Dynamic state does not decay: " + where);
        const auto recovered = process(amp, 48000, 257, 4800, 0, .1f);
        const double recovery = difference(cold[0], recovered[0]) / std::max(1.e-8, rms(cold[0]));
        require(recovery < .005, "State failed to recover after two seconds: " + where);
        worstRecovery = std::max(worstRecovery, recovery);
    }
    std::cout << "MEASURE reset residual " << worstReset << ", maximum conditioned-history residual " << largestHistory
              << ", worst recovered residual/cold RMS " << worstRecovery << '\n';
    std::cout << "PASS: exact reset, signal-history response and two-second recovery for all native channels at .8/1 drive (combined filter/nonlinear state; not isolated sag measurement)\n";
}
void repeatedControlsAndSwitches()
{
    for (int model = 15; model < 23; ++model) for (int channel = 0; channel < newAmpChannelCount(model); ++channel) {
        Amp amp;
        amp.prepare({48000, 64, 2});amp.set(static_cast<AmpModel>(model), .68f, channel);
        amp.setDriveImmediately(.68f);amp.setOversampling(2);amp.reset();amp.tone(0, 0, 0, 0, 0, 0);
        const auto reference = render(model, channel, 48000, 64, 2);
        std::vector<float> repeated;
        for (int offset = 0; offset < (int)reference[0].size(); offset += 64) {
            const int count = std::min(64, int(reference[0].size()) - offset);
            // The processor sends current parameter values on every host block.
            amp.set(static_cast<AmpModel>(model), .68f, channel);
            const auto block = process(amp, 48000, 64, count, 0, 1.f, offset);
            repeated.insert(repeated.end(), block[0].begin(), block[0].end());
        }
        require(difference(reference[0], repeated) == 0,
                "Repeated host control values restart circuit state: " + label(model, channel));
        // Requests arriving faster than the 20 ms fade must remain bounded and
        // respect the final target, including switching back to the first route.
        for (int block = 0; block < 50; ++block) {
            const int next = block % newAmpChannelCount(model);
            amp.setNativeChannel(next);
            require(amp.nativeChannel() == next, "Native channel getter lost requested target");
            const auto changed = process(amp, 48000, 64, 64, 1, 1.f, block * 64);
            for (float x : changed[0])
                require(std::isfinite(x) && std::abs(x) < 8.f, "Rapid native channel switch is unstable");
        }
        amp.setNativeChannel(channel);amp.reset();
        const auto restored = process(amp, 48000, 64, (int)reference[0].size());
        require(difference(reference[0], restored[0]) == 0,
                "Reset after channel switches did not install final requested route: " + label(model, channel));
    }
    std::cout << "PASS: repeated host parameter updates preserve samples; rapid native-channel changes stay bounded and reset installs the final route\n";
}
void sustainedDynamics()
{
    double minimumRatio = std::numeric_limits<double>::max(), maximumRatio = 0, maximumDC = 0;
    for (int model = 15; model < 23; ++model) for (int channel = 0; channel < newAmpChannelCount(model); ++channel) {
        std::array<double, 2> levels{};
        for (int loud = 0; loud < 2; ++loud) {
            // Keep the integrated FeatureTests fixture: steady 997 Hz at full
            // drive, 4x oversampling, first 64 of 192 blocks discarded.
            Amp amp; amp.prepare({48000, 256, 1});
            amp.set(static_cast<AmpModel>(model), 1.f, channel);amp.setOversampling(2);
            juce::AudioBuffer<float> buffer(1, 256);
            const float amplitude = loud ? .3f : .015f;
            double energy = 0, sum = 0;
            int count = 0;
            for (int block = 0; block < 192; ++block) {
                for (int n = 0; n < 256; ++n)
                    buffer.setSample(0, n, amplitude * float(std::sin(juce::MathConstants<double>::twoPi * 997 * (block * 256 + n) / 48000.)));
                amp.process(buffer, false);
                if (block >= 64) for (int n = 0; n < 256; ++n) {
                    const float x = buffer.getSample(0, n);
                    require(std::isfinite(x) && std::abs(x) < 3.f,
                            "Unstable sustained output: " + label(model, channel));
                    energy += double(x) * x;sum += x;++count;
                }
            }
            levels[(size_t)loud] = std::sqrt(energy / count);
            if (loud) {
                const double dc = std::abs(sum / count);
                require(dc < .001, "Persistent DC in sustained output: " + label(model, channel));
                maximumDC = std::max(maximumDC, dc);
            }
        }
        require(levels[0] > 1.e-8, "Quiet sustained probe is silent: " + label(model, channel));
        const double ratio = levels[1] / levels[0];
        require(ratio > 1.05 && ratio < 21,
                "Sustained dynamics inverted or over-compressed/expanding: " + label(model, channel) + " ratio " + std::to_string(ratio));
        minimumRatio = std::min(minimumRatio, ratio);maximumRatio = std::max(maximumRatio, ratio);
    }
    std::cout << "MEASURE sustained 997 Hz loud/quiet RMS ratio: minimum " << minimumRatio
              << ", maximum " << maximumRatio << "; maximum loud-output DC " << maximumDC << '\n';
    std::cout << "PASS: all 20 native routes preserve the integrated 1.05 < loud/quiet RMS < 21 and DC < .001 gates at full drive\n";
}
}
int main()
{
    const std::array<std::pair<const char*, void (*)()>, 6> groups{{
        {"contracts", contracts}, {"numerical matrix", numericalMatrix},
        {"voice and drive distinction", distinctVoicesAndDrive},
        {"reset and recovery", resetAndRecovery}, {"host controls and switches", repeatedControlsAndSwitches},
        {"sustained dynamics", sustainedDynamics}
    }};
    int failures = 0;
    for (const auto& group : groups) {
        try { group.second(); }
        catch (const std::exception& error) {
            std::cerr << "FAIL [" << group.first << "]: " << error.what() << '\n';
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}

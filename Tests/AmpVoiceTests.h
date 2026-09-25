#pragma once
#include "Amplifier.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ampVoiceTests {
inline void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
inline juce::AudioBuffer<float> render(int model, double rate, int blockSize, int oversampling, bool mirror = false, bool extremeTone = false)
{
    spectralforge::Amp amp;
    amp.prepare({rate, (juce::uint32)blockSize, 2});
    amp.set(static_cast<spectralforge::AmpModel>(model), .68f);
    amp.setDriveImmediately(.68f);
    amp.setOversampling(oversampling);
    amp.reset();
    amp.tone(extremeTone ? 12.f : 0.f, extremeTone ? -12.f : 0.f,
             extremeTone ? 12.f : 0.f, extremeTone ? -12.f : 0.f,
             extremeTone ? 10.f : 0.f, extremeTone ? 10.f : 0.f);
    const int length = int(rate * .2);
    juce::AudioBuffer<float> result(2, length), block(2, blockSize);
    for (int offset = 0; offset < length; offset += blockSize) {
        const int count = juce::jmin(blockSize, length - offset);
        block.setSize(2, count, false, false, true);
        for (int n = 0; n < count; ++n) {
            const double t = double(offset + n) / rate;
            // Multiple bass and guitar partials and a changing envelope exercise
            // coupling, bias and sag, rather than comparing one sine's level.
            const double envelope = .3 * (.65 + .35 * std::sin(juce::MathConstants<double>::twoPi * 11 * t));
            const auto partial = [t](double hz) { return std::sin(juce::MathConstants<double>::twoPi * hz * t); };
            const float x = float(envelope * (partial(61.7354) + .35 * partial(184.997) + .25 * partial(997) + .10 * partial(3520)));
            block.setSample(0, n, x);
            block.setSample(1, n, mirror ? x : 0.f);
        }
        amp.process(block);
        for (int channel = 0; channel < 2; ++channel) result.copyFrom(channel, offset, block, channel, 0, count);
    }
    return result;
}
inline void run()
{
    using namespace spectralforge;
    static_assert(static_cast<int>(AmpModel::modernBass) == 7, "Existing project model indices changed");
    static_assert(ampCatalog.size() == ampVoices.size() && ampCatalog.size() == referenceContours.size());
    double worstPartition = 0, worstStereo = 0;
    for (double rate : {44100., 48000., 96000.}) {
        for (int model = 0; model < ampModelCount; ++model) {
            for (int oversampling : {0, 2, 3}) {
                const auto split = render(model, rate, 17, oversampling);
                const auto larger = render(model, rate, 512, oversampling, true);
                require(split.getRMSLevel(0, 0, split.getNumSamples()) > .0001f, "Amp voice unexpectedly silent");
                for (int n = 0; n < split.getNumSamples(); ++n) {
                    const double x = split.getSample(0, n);
                    require(std::isfinite(x) && std::abs(x) < 8., "Amp voice non-finite or excessive level");
                    require(std::abs(split.getSample(1, n)) < 1e-10f, "Amp voice leaks to silent stereo channel");
                    worstPartition = juce::jmax(worstPartition, std::abs(x - larger.getSample(0, n)));
                    worstStereo = juce::jmax(worstStereo, std::abs(double(larger.getSample(0, n) - larger.getSample(1, n))));
                }
            }
            const auto corner = render(model, rate, 128, 2, false, true);
            for (int n = 0; n < corner.getNumSamples(); ++n)
                require(std::isfinite(corner.getSample(0, n)) && std::abs(corner.getSample(0, n)) < 64.f, "Amp tone corner became unstable");
        }
    }
    require(worstPartition < 2e-5, "Amp output depends on audio block partition");
    require(worstStereo < 1e-7, "Amp changes identical stereo channels differently");

    std::array<juce::AudioBuffer<float>, ampModelCount> voices;
    for (int model = 0; model < ampModelCount; ++model) voices[(size_t)model] = render(model, 48000, 256, 2);
    double leastDifference = 100.;
    for (int model = 8; model < ampModelCount; ++model) {
        const auto& candidate = voices[(size_t)model];
        const int start = 2400, length = candidate.getNumSamples() - start;
        const double candidateRms = candidate.getRMSLevel(0, start, length);
        for (int prior = 0; prior < model; ++prior) {
            const auto& other = voices[(size_t)prior];
            const double otherRms = other.getRMSLevel(0, start, length);
            double energy = 0;
            for (int n = start; n < candidate.getNumSamples(); ++n)
                energy += std::pow(candidate.getSample(0, n) / candidateRms - other.getSample(0, n) / otherRms, 2);
            leastDifference = juce::jmin(leastDifference, std::sqrt(energy / length));
        }
    }
    require(leastDifference > .03, "New amp voice differs only by output level");
    for (const auto indexPair : {std::pair<int, int>{-1, 0}, {999, ampModelCount - 1}}) {
        const auto invalid = render(indexPair.first, 48000, 256, 2);
        const auto valid = render(indexPair.second, 48000, 256, 2);
        for (int n = 0; n < valid.getNumSamples(); ++n)
            require(invalid.getSample(0, n) == valid.getSample(0, n), "Invalid amp index is not safely clamped");
    }
    std::cout << "MEASURE " << ampModelCount << " amp voices: block residual " << worstPartition
              << ", stereo residual " << worstStereo << ", minimum RMS-matched new-voice residual " << leastDifference << "\n";
    std::cout << "PASS: amp voices at 44.1/48/96 kHz, 1x/4x/8x oversampling, 17/512 sample blocks, stereo isolation, EQ corners and invalid indices\n";
}
}

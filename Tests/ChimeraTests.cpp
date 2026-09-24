#include "ChimeraDSP.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "FeatureTests.h"
#include "AlignmentTests.h"
#include "ReferenceTests.h"
#include "StudioTests.h"

namespace {
void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
constexpr int blockSize = 256;
constexpr double pi = 3.14159265358979323846;

void fillSine(juce::AudioBuffer<float>& buffer, double sr, double frequency, int offset)
{
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            buffer.setSample(c, n, 0.01f * static_cast<float>(std::sin(2.0*pi*frequency*(offset+n)/sr)));
}

double measureTone(spectralforge::MatrixTone& tone, double sr, float frequency)
{
    juce::AudioBuffer<float> buffer(2, blockSize);
    double output = 0.0, input = 0.0;
    const int blocks = static_cast<int>(sr * 0.35 / blockSize) + 1;
    for (int b = 0; b < blocks; ++b)
    {
        fillSine(buffer, sr, frequency, b * blockSize);
        if (b > blocks / 2)
            for (int n = 0; n < blockSize; ++n) input += std::pow(buffer.getSample(0,n),2);
        tone.process(buffer);
        if (b > blocks / 2)
            for (int n = 0; n < blockSize; ++n) output += std::pow(buffer.getSample(0,n),2);
    }
    return 10.0 * std::log10(output / input);
}

std::vector<float> render(spectralforge::Engine& engine, double sr, spectralforge::RoutingMode mode,
                          const std::array<spectralforge::LaneState,3>& states, float hz = 70.0f)
{
    std::vector<float> result;
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int b = 0; b < 48; ++b)
    {
        fillSine(buffer, sr, hz, b * blockSize);
        engine.process(buffer, mode, 150, 1200, states);
        if (b > 24)
            result.insert(result.end(), buffer.getReadPointer(0), buffer.getReadPointer(0)+blockSize);
    }
    return result;
}
float difference(const std::vector<float>& a, const std::vector<float>& b)
{
    float peak = 0;
    for (size_t i = 0; i < a.size(); ++i) peak = juce::jmax(peak, std::abs(a[i]-b[i]));
    return peak;
}
double energy(const std::vector<float>& values)
{
    double result = 0;
    for (auto value : values) result += value*value;
    return result;
}

void checkBandTone(double sr)
{
    for (const auto crossovers : {std::pair<float,float>{60,500}, {150,1200}, {350,4000}})
    {
        for (int lane = 0; lane < 3; ++lane)
        {
            spectralforge::MatrixTone tone;
            tone.prepare({sr,blockSize,2});
            const auto pivot = spectralforge::matrixTonePivot(lane, crossovers.first, crossovers.second, sr);
            const float lower = pivot * 0.65f, upper = pivot * 1.5f;
            tone.set(pivot, 0);
            require(std::abs(measureTone(tone,sr,lower)) < 0.08, "Neutral band tone changes level");
            // Change the SAME prepared instance: catches stale filter-state pointers.
            tone.set(pivot, 12);
            const auto bright = measureTone(tone,sr,upper) - measureTone(tone,sr,lower);
            tone.set(pivot, -12);
            const auto dark = measureTone(tone,sr,upper) - measureTone(tone,sr,lower);
            require(bright > 2.0 && dark < -2.0, "Band tone does not tilt in the labelled direction");
        }
    }
    spectralforge::MatrixTone moving;
    moving.prepare({sr,blockSize,2});
    moving.set(spectralforge::matrixTonePivot(1,60,500,sr),12);
    const auto lowerPivot = measureTone(moving,sr,450);
    moving.set(spectralforge::matrixTonePivot(1,350,4000,sr),12);
    require(lowerPivot - measureTone(moving,sr,450) > 3.0, "Tone pivot did not follow the crossovers");
}

void checkRouting(double sr)
{
    using namespace spectralforge;
    std::array<LaneState,3> flat{}, eq{};
    for (auto& lane : flat) { lane.amp = 0; lane.cab = false; lane.drive = 0; }
    eq = flat;
    for (auto& lane : eq)
    { lane.bass=12; lane.lowMid=-12; lane.highMid=12; lane.treble=-12; lane.presence=10; lane.resonance=10; }
    Engine a,b;
    a.prepare({sr,blockSize,2}); b.prepare({sr,blockSize,2});
    require(difference(render(a,sr,RoutingMode::matrix,flat),render(b,sr,RoutingMode::matrix,eq)) < 1e-6f,
            "Hidden full-range EQ affects Matrix");
    eq = flat;
    for (auto& lane : eq) lane.bandTone = 12;
    a.reset(); b.reset();
    require(difference(render(a,sr,RoutingMode::matrix,flat),render(b,sr,RoutingMode::matrix,eq)) > 0.0001f,
            "Matrix band tone not connected to the engine");
    for (auto mode : {RoutingMode::classic, RoutingMode::dual})
    {
        a.reset(); b.reset();
        require(difference(render(a,sr,mode,flat),render(b,sr,mode,eq)) < 1e-6f,
                "Hidden Matrix tone affects Classic/Dual");
    }
    eq = flat;
    eq[0].bass = 12;
    a.reset(); b.reset();
    const auto baseline = render(a,sr,RoutingMode::classic,flat,40);
    const auto boosted = render(b,sr,RoutingMode::classic,eq,40);
    require(10.0*std::log10(energy(boosted)/energy(baseline)) > 5.0,
            "Classic EQ knob does not update prepared filters");
    eq = flat; eq[2].solo = true;
    a.reset(); b.reset();
    require(difference(render(a,sr,RoutingMode::classic,flat),render(b,sr,RoutingMode::classic,eq)) < 1e-6f,
            "Inactive lane solo silenced Classic");
}

void smoke(double sr)
{
    using namespace spectralforge;
    Engine engine; engine.prepare({sr,blockSize,2});
    std::array<LaneState,3> states{};
    juce::AudioBuffer<float> buffer(2,blockSize);
    for (auto mode : {RoutingMode::classic,RoutingMode::dual,RoutingMode::matrix})
        for (int model = 0; model < 8; ++model)
            for (int block = 0; block < 12; ++block)
            {
                for (auto& lane : states)
                {
                    lane.amp=model; lane.drive=1;
                    lane.bandTone = block % 2 == 0 ? -12.0f : 12.0f;
                }
                fillSine(buffer,sr,700,block*blockSize);
                engine.process(buffer,mode,block%2 ? 60.0f : 350.0f,block%2 ? 500.0f : 4000.0f,states);
                for (int c=0;c<2;++c) for (int n=0;n<blockSize;++n)
                    require(std::isfinite(buffer.getSample(c,n)) && std::abs(buffer.getSample(c,n)) < 100,
                            "Non-finite or unstable output during mode/tone changes");
            }
}
}

int main()
{
    try
    {
        for (const double sr : {44100.0,48000.0,96000.0,192000.0})
        {
            checkBandTone(sr); checkRouting(sr); smoke(sr);
            std::cout << "PASS " << sr << " Hz: band response, routing isolation, live EQ, all amp models\n";
        }
        featureTests::run();
        alignmentTests::run();
        referenceTests::run();
        studioTests::run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

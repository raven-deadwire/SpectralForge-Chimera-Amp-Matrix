#include "PluginProcessor.h"
#include "EQNormalizationContract.h"
#include "IRStateAssets.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool b, const juce::String& message) {
    if (!b) throw std::runtime_error(message.toStdString());
}
struct Snapshot { std::vector<float> raw, host; };
Snapshot snapshot(ChimeraProcessor& p) {
    Snapshot result;
    for (auto* parameter : p.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        require(ranged != nullptr, "identified ranged parameter");
        result.raw.push_back(p.parameters().getRawParameterValue(ranged->paramID)->load());
        result.host.push_back(ranged->getValue());
    }
    require(result.raw.size() == 4967+19+spectralforge::commonDynamicsParameterCount, "append-only candidate parameter count");
    return result;
}
void exact(const std::vector<float>& a, const std::vector<float>& b, const char* contract) {
    require(a.size() == b.size(), "vector size");
    for (size_t i = 0; i < a.size(); ++i)
        require(std::isfinite(a[i]) && std::isfinite(b[i]) && eqNormalization::bits(a[i], b[i]),
                juce::String(contract) + " index=" + juce::String(int(i)));
}
juce::MemoryBlock read(const juce::File& path) {
    juce::MemoryBlock result;
    require(path.loadFileAsData(result) && result.getSize() > 8, "fixture missing " + path.getFullPathName());
    return result;
}
void load(ChimeraProcessor& p, const juce::MemoryBlock& b) {
    p.setStateInformation(b.getData(), int(b.getSize()));
}
juce::MemoryBlock save(ChimeraProcessor& p) {
    juce::MemoryBlock result; require(p.tryGetStateInformation(result), "serialize"); return result;
}
juce::ValueTree tree(const juce::MemoryBlock& b) {
    auto xml = juce::AudioProcessor::getXmlFromBinary(b.getData(), int(b.getSize()));
    require(xml != nullptr, "state XML");
    auto t = juce::ValueTree::fromXml(*xml); require(spectralforge::irState::unpack(t), "IR state"); return t;
}
void storedExact(const juce::MemoryBlock& a, const juce::MemoryBlock& b) {
    // XML attribute order / wrapper trailer is not a state contract. Every
    // property, parameter decimal, child order, A/B and auxiliary node IS.
    require(tree(a).isEquivalentTo(tree(b)), "serialized semantic idempotence");
}
std::vector<float> render(ChimeraProcessor& p, double rate, int block) {
    // Exercise the actual two GraphicalEQ DSP objects bound to restored APVTS,
    // excluding unrelated amp/cab workers. The ABI test covers processBlock.
    p.graphicalEQ(0).prepare(rate); p.graphicalEQ(1).prepare(rate);
    std::vector<float> result; result.reserve(32768);
    uint32_t random = 0x13579bdu;
    for (int offset = 0; offset < 16384; offset += block) {
        juce::AudioBuffer<float> buffer(2, block);
        for (int n = 0; n < block; ++n) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            const float value = float(int32_t(random & 0xffffffu) - 0x800000) / 67108864.f;
            buffer.setSample(0, n, value); buffer.setSample(1, n, -value * .5f);
        }
        p.graphicalEQ(0).process(buffer); p.graphicalEQ(1).process(buffer);
        for (int n = 0; n < block; ++n) for (int c = 0; c < 2; ++c) {
            const auto value = buffer.getSample(c, n);
            require(std::isfinite(value), "non-finite EQ render"); result.push_back(value);
        }
    }
    double energy = 0; for (float value : result) energy += double(value) * value;
    require(energy > 1.e-12, "silent EQ render cannot prove equivalence");
    return result;
}
void cycle(const juce::File& path) {
    auto p = std::make_unique<ChimeraProcessor>(); load(*p, read(path));
    const auto baseline = snapshot(*p); const auto canonical = save(*p);
    auto next = canonical;
    for (int pass = 0; pass < 8; ++pass) {
        auto fresh = std::make_unique<ChimeraProcessor>(); load(*fresh, next);
        const auto actual = snapshot(*fresh);
        exact(baseline.raw, actual.raw, "fresh raw idempotence");
        exact(baseline.host, actual.host, "host normalized idempotence");
        next = save(*fresh); storedExact(canonical, next);
        // Same-instance repeated reload must also be a fixed point.
        load(*p, save(*p));
        exact(baseline.raw, snapshot(*p).raw, "same-instance raw idempotence");
        exact(baseline.host, snapshot(*p).host, "same-instance host idempotence");
        storedExact(canonical, save(*p));
        if (pass == 0) for (double rate : {44100., 48000., 96000., 192000.}) for (int block : {32, 256}) {
            const auto a = render(*p, rate, block);
            exact(a, render(*p, rate, block), "EQ A/A determinism");
            exact(a, render(*fresh, rate, block), "EQ canonical reload render");
        }
    }
    // A/B is a separate history-dependent path. Establish one full A/B pass,
    // then demand exact fixed points, without comparing against a fresh cache.
    p->selectComparison(1); p->selectComparison(0);
    const auto abBaseline = snapshot(*p); const auto abState = save(*p);
    for (int pass = 0; pass < 8; ++pass) {
        p->selectComparison(1); p->selectComparison(0);
        exact(abBaseline.raw, snapshot(*p).raw, "A/B raw idempotence");
        exact(abBaseline.host, snapshot(*p).host, "A/B host idempotence");
        storedExact(abState, save(*p));
    }
    std::cout << "PASS_NORMALIZATION " << path.getFullPathName()
              << " fresh_cycles=8 same_instance_cycles=8 ab_cycles=8"
              << " render_routes=8 samples_per_render=32768 exact_bits=true\n";
}
void negativeControls() {
    auto p = std::make_unique<ChimeraProcessor>();
    for (auto* parameter : p->getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        require(ranged != nullptr, "ranged parameter");
        if (!eqNormalization::retainedDefaultDomain(*ranged)) continue;
        const auto initial = eqNormalization::constructorRaw(*ranged);
        const auto actual = p->parameters().getRawParameterValue(ranged->paramID)->load();
        if (!eqNormalization::retainsConstructorOnTreeInit(*ranged)) {
            require(!eqNormalization::retainedConstructor(*ranged, initial, initial),
                    "non-retained constructor representation must be rejected");
            continue;
        }
        require(eqNormalization::bits(initial, actual),
                "constructor formula must match actual fresh APVTS raw cache " + ranged->paramID
                    + " predicted=" + juce::String(initial,10) + " actual=" + juce::String(actual,10));
    }
    auto* q = p->parameters().getParameter("toneEQ_b4_q");
    const float requested = q->convertFrom0to1(q->convertTo0to1(.70710678f));
    require(eqNormalization::restoredRepresentation(*q, requested, eqNormalization::constructorRaw(*q)),
            "constructor representation is supported");
    require(!eqNormalization::restoredRepresentation(*q, requested, q->convertFrom0to1(.8f)),
            "changed normalized value must be rejected");
    int rejectedEquivalentNeighbours = 0;
    float other = requested;
    for (int i = 0; i < 16; ++i) {
        other = std::nextafter(other, 2.f);
        if (eqNormalization::bits(q->convertTo0to1(requested), q->convertTo0to1(other))) {
            require(!eqNormalization::restoredRepresentation(*q, requested, other),
                    "arbitrary same-normalized neighbour must be rejected");
            ++rejectedEquivalentNeighbours;
        }
    }
    require(rejectedEquivalentNeighbours > 0, "same-normalized negative control coverage");
    require(!eqNormalization::restoredRepresentation(*q, requested, std::numeric_limits<float>::quiet_NaN()), "NaN rejected");
    auto a = save(*p); auto t = tree(a);
    auto outputNode = t.getChildWithProperty("id", "output");
    require(outputNode.isValid(), "output parameter for corruption control");
    outputNode.setProperty("value", float(outputNode.getProperty("value")) + 3.f, nullptr);
    juce::MemoryBlock b; juce::AudioProcessor::copyXmlToBinary(*t.createXml(), b);
    require(!tree(a).isEquivalentTo(tree(b)), "serialized parameter corruption detected");
    const auto dry = render(*p, 48000, 256);
    for (const auto& item : std::array<std::pair<juce::String, float>, 3>{{
             {"toneEQ_bypass", 0}, {"toneEQ_b4_enabled", 1}, {"toneEQ_b4_gain", 12}}}) {
        auto* parameter = p->parameters().getParameter(item.first);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(item.second));
    }
    const auto wet = render(*p, 48000, 256);
    require(dry != wet, "render negative control must detect changed EQ gain");
    std::cout << "PASS_NEGATIVE_CONTROLS normalized drift / same-normalized neighbours / NaN / state corruption / audible-path EQ change\n";
}
void preSaveObservation() {
    auto edited = std::make_unique<ChimeraProcessor>();
    for (const auto& item : std::array<std::pair<juce::String, float>, 5>{{
             {"toneEQ_bypass", 0}, {"toneEQ_b4_enabled", 1}, {"toneEQ_b4_frequency", 1234},
             {"toneEQ_b4_gain", 6}, {"toneEQ_b4_q", .70710678f}}}) {
        auto* parameter = edited->parameters().getParameter(item.first);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(item.second));
    }
    const auto saved = save(*edited);
    auto reopened = std::make_unique<ChimeraProcessor>(); load(*reopened, saved);
    exact(snapshot(*edited).host, snapshot(*reopened).host, "pre-save/reopened normalized host values");
    for (double rate : {44100., 48000., 96000., 192000.}) {
        const auto a = render(*edited, rate, 256), b = render(*reopened, rate, 256);
        size_t changed = 0; double maxError = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            changed += !eqNormalization::bits(a[i], b[i]);
            maxError = std::max(maxError, std::abs(double(a[i])-double(b[i])));
        }
        // Measurement, never a tolerant PASS: pre-save raw history is not the
        // canonical fresh-load domain tested above. Keep any residual visible.
        std::cout << "OBSERVE_PRE_SAVE rate=" << rate << " changed_samples=" << changed
                  << " max_abs=" << std::setprecision(17) << maxError
                  << " bit_identical=" << (changed == 0) << '\n';
    }
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "usage: fixtures-dir existing-roundtrip-dir");
        juce::ScopedJuceInitialiser_GUI init; negativeControls(); preSaveObservation();
        for (const auto* name : {"default", "pattern", "ab-seeded"}) {
            cycle(juce::File(argv[1]).getChildFile(juce::String(name) + ".chimera"));
            for (const auto* phase : {"deleted", "reused", "automated"})
                cycle(juce::File(argv[2]).getChildFile(name).getChildFile(juce::String(phase) + ".chimera"));
        }
        std::cout << "PASS normalization fixed-point and EQ render contracts; actual DAW NOT_ASSESSED; release_approved=false\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}

#include "PluginProcessor.h"

#include <array>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

static thread_local bool counting = false;
static thread_local size_t allocations = 0;
static void (*volatile releaseMemory)(void*) = &std::free;
void* operator new(std::size_t n) {
    if (counting) ++allocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { releaseMemory(p); }
void operator delete[](void* p) noexcept { releaseMemory(p); }
void operator delete(void* p, std::size_t) noexcept { releaseMemory(p); }
void operator delete[](void* p, std::size_t) noexcept { releaseMemory(p); }

using namespace spectralforge;
namespace {
constexpr int originalParameterCount = 5026;
constexpr int newParameterCount = multibandParameterCount + deEsserParameterCount;
constexpr double pi = multiband_detail::pi;

void check(bool okay, const char* message) {
    if (!okay) { counting = false; throw std::runtime_error(message); }
}
void set(ChimeraProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.parameters().getParameter(id);
    check(parameter != nullptr, "Missing multiband/de-esser host parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(ChimeraProcessor& processor, const juce::String& id) {
    auto* value = processor.parameters().getRawParameterValue(id);
    check(value != nullptr, "Missing host value");
    return value->load();
}

void parameterContract(ChimeraProcessor& processor) {
    check(processor.getParameters().size() == originalParameterCount + newParameterCount + extraGateParameterCount+spectralforge::multibandStereoParameterCount+spectralforge::saturationParameterCount+spectralforge::referenceAmpParameterCount()+spectralforge::filterModulationParameterCount+spectralforge::postExpansionParameterCount()+spectralforge::effectExtensionParameterCount,
          "New dynamics did not append exactly 55 parameters after the released prefix");
    check(processor.parameters().getParameter("liveLimiterRelease")->getParameterIndex() == 5025,
          "Released common-dynamics parameter moved");
    int index = originalParameterCount;
    const auto checkParameter = [&](const juce::String& id) {
        const auto* p = processor.parameters().getParameter(id);
        check(p != nullptr && p->getParameterIndex() == index++ && p->getVersionHint() == 13
              && p->isAutomatable(), "Appended dynamics parameter order/version/automation contract changed");
    };
    for (const char* id : {"mbOn", "mbMix", "mbDetector", "mbCrossover1", "mbCrossover2", "mbCrossover3"})
        checkParameter(id);
    for (int band = 0; band < 4; ++band)
        for (const char* field : {"bypass", "mode", "threshold", "range", "ratio", "attack",
                                 "release", "knee", "makeup", "solo"})
            checkParameter(multibandID(band, field));
    for (const char* id : {"deEsserOn", "deEsserFrequency", "deEsserThreshold", "deEsserRange",
                         "deEsserAttack", "deEsserRelease", "deEsserRatio", "deEsserKnee", "deEsserListen"})
        checkParameter(id);
    check(index == originalParameterCount + newParameterCount, "Missing appended parameter coverage");
    check(get(processor, "mbOn") == 0.f && get(processor, "deEsserOn") == 0.f,
          "New processing is enabled by default");
    std::cout << "PASS fixed released prefix, 55 append-only parameters, stable IDs/version 13 and automation\n";
}

void fill(juce::AudioBuffer<float>& audio, int offset, double rate) {
    for (int n = 0; n < audio.getNumSamples(); ++n) {
        const float sample = float(.2 * std::sin(2. * pi * 220. * (offset + n) / rate)
                                 + .2 * std::sin(2. * pi * 12000. * (offset + n) / rate));
        audio.setSample(0, n, sample);
        if (audio.getNumChannels() > 1) audio.setSample(1, n, -.25f * sample);
    }
}

void wrapperAudio(ChimeraProcessor& processor) {
    auto& mb = processor.multibandDynamics();
    auto& ds = processor.deEsser();
    for (const double rate : {44100., 48000., 96000., 192000.}) {
        for (const int channels : {1, 2}) {
            set(processor, "mbOn", 0.f); set(processor, "deEsserOn", 0.f);
            mb.prepare(rate, 64); ds.prepare(rate, 64);
            juce::AudioBuffer<float> audio(channels, 257), dry;
            fill(audio, 0, rate); dry.makeCopyOf(audio);
            counting = true; mb.process(audio); ds.process(audio); counting = false;
            for (int c = 0; c < channels; ++c) for (int n = 0; n < audio.getNumSamples(); ++n)
                check(audio.getSample(c, n) == dry.getSample(c, n), "APVTS-bound disabled stages changed audio");
            set(processor, "mbOn", 1.f); set(processor, "deEsserOn", 1.f);
            set(processor, "deEsserFrequency", 4000.f); set(processor, "deEsserThreshold", -48.f);
            set(processor, "deEsserRange", 9.f); set(processor, "deEsserRatio", 20.f);
            for (int band = 0; band < 4; ++band) {
                set(processor, multibandID(band, "threshold"), -48.f);
                set(processor, multibandID(band, "ratio"), 20.f);
                set(processor, multibandID(band, "range"), 6.f);
                set(processor, multibandID(band, "attack"), .1f);
            }
            mb.reset(); ds.reset();
            for (int block = 0; block < 1000; ++block) {
                fill(audio, block * 257, rate);
                counting = true; mb.process(audio); ds.process(audio); counting = false;
                for (int n = 0; n < audio.getNumSamples(); ++n) {
                    check(std::isfinite(audio.getSample(0, n)), "Host-bound dynamics emitted nonfinite audio");
                    if (channels == 2)
                        check(std::abs(audio.getSample(1, n) + .25f * audio.getSample(0, n)) < 2.e-7f,
                              "Host-bound dynamics stereo linkage diverged");
                }
            }
            check(mb.reduction(3) > 5.9f && ds.reduction() > 8.f,
                  "APVTS parameter edits did not reach the new audible DSP stages");
            check(mb.latencySamples() == 0 && ds.latencySamples() == 0, "Wrapper reported an added sample delay");
        }
    }
    check(allocations == 0, "APVTS-bound multiband/de-esser allocated during processing");
    std::cout << "PASS APVTS-to-DSP controls, exact disabled audio, measured active GR, mono/stereo and zero allocations at four rates\n";
}

void recall(ChimeraProcessor& processor) {
    for (int i = originalParameterCount; i < processor.getParameters().size(); ++i) {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(processor.getParameters()[i]);
        check(p != nullptr, "Appended parameter is not ranged");
        p->setValueNotifyingHost(.15f + .07f * float((i - originalParameterCount) % 11));
    }
    set(processor, "mbOn", 1.f); set(processor, "deEsserOn", 1.f);
    set(processor, multibandID(1, "mode"), 3.f); set(processor, "deEsserListen", 1.f);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    auto restored = std::make_unique<ChimeraProcessor>();
    restored->setStateInformation(state.getData(), int(state.getSize()));
    for (int i = originalParameterCount; i < processor.getParameters().size(); ++i) {
        const auto* p = dynamic_cast<juce::RangedAudioParameter*>(processor.getParameters()[i]);
        check(std::abs(get(*restored, p->paramID) - get(processor, p->paramID)) < .002f,
              "New dynamics parameter did not round-trip through a project state");
    }
    processor.copyComparison();
    processor.selectComparison(1);
    set(processor, multibandID(1, "mode"), 1.f); set(processor, "deEsserListen", 0.f);
    set(processor, "mbCrossover2", 2300.f);
    processor.selectComparison(0);
    check(get(processor, multibandID(1, "mode")) == 3.f && get(processor, "deEsserListen") == 1.f,
          "A recall lost multiband/de-esser choices");
    processor.selectComparison(1);
    check(get(processor, multibandID(1, "mode")) == 1.f && get(processor, "deEsserListen") == 0.f
          && std::abs(get(processor, "mbCrossover2") - 2300.f) < .01f,
          "B recall lost multiband/de-esser choices or crossovers");

    auto legacy = processor.parameters().copyState();
    for (int i = legacy.getNumChildren(); --i >= 0;) {
        const auto id = legacy.getChild(i).getProperty("id").toString();
        const auto* p = processor.parameters().getParameter(id);
        if (p != nullptr && p->getParameterIndex() >= originalParameterCount) legacy.removeChild(i, nullptr);
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), old);
    // Load the old state into an instance where the new effects were active.
    // APVTS alone may otherwise retain missing values from that newer session.
    restored->setStateInformation(old.getData(), int(old.getSize()));
    for (int i = originalParameterCount; i < restored->getParameters().size(); ++i) {
        const auto* p = dynamic_cast<juce::RangedAudioParameter*>(restored->getParameters()[i]);
        check(std::abs(p->getValue() - p->getDefaultValue()) < 1.e-5f,
              "Legacy project inherited a new dynamics value from the previous session");
    }
    check(!restored->multibandDynamics().enabled() && !restored->deEsser().enabled(),
          "Legacy project enabled newly added dynamics");
    std::cout << "PASS all 55 project parameters, A/B choices/crossovers and legacy missing-parameter default migration\n";
}

void isolate(ChimeraProcessor& processor) {
    set(processor, "mode", 0.f); set(processor, "input", 0.f); set(processor, "output", 0.f);
    set(processor, "gateon", 0.f); set(processor, "boardEnabled", 0.f);
    set(processor, "transposeon", 0.f); set(processor, "oversampling", 0.f);
    set(processor, "doubleron", 0.f); set(processor, "metronome", 0.f);
    set(processor, "tuneron", 0.f); set(processor, "cleanCompOn", 0.f); set(processor, "liveLimiterOn", 0.f);
    for (const auto& spec : fxSpecs) if (spec.toggle) set(processor, spec.id, 0.f);
    for (int i = 1; i <= 3; ++i) {
        set(processor, "cab" + juce::String(i), 0.f);
        set(processor, "ampon" + juce::String(i), 0.f);
        set(processor, "level" + juce::String(i), 0.f);
    }
    for (auto* p : processor.getParameters()) {
        const auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p);
        if (parameter == nullptr) continue;
        const auto& id = parameter->paramID;
        if ((id.startsWith("nativeAmp_") && id.endsWith("_enabled"))
            || (id.startsWith("pn_") && id.endsWith("_native"))) set(processor, id, 0.f);
    }
    set(processor, eqID(0, -1, "bypass"), 1.f); set(processor, eqID(1, -1, "bypass"), 1.f);
    set(processor, "mbOn", 0.f); set(processor, "deEsserOn", 0.f);
}

void productionPlacement() {
    auto baseline = std::make_unique<ChimeraProcessor>();
    auto candidate = std::make_unique<ChimeraProcessor>();
    isolate(*baseline); isolate(*candidate);
    for (int stage = 0; stage < 2; ++stage) {
        set(*candidate, "mbOn", stage == 0 ? 1.f : 0.f);
        set(*candidate, "deEsserOn", stage == 1 ? 1.f : 0.f);
        for (int b = 0; b < 4; ++b) set(*candidate, multibandID(b, "bypass"), b == 3 ? 0.f : 1.f);
        set(*candidate, multibandID(3, "threshold"), -48.f);
        set(*candidate, multibandID(3, "range"), 9.f);
        set(*candidate, multibandID(3, "ratio"), 20.f);
        set(*candidate, "deEsserFrequency", 4000.f);
        set(*candidate, "deEsserThreshold", -48.f);
        set(*candidate, "deEsserRange", 9.f);
        set(*candidate, "deEsserRatio", 20.f);
        baseline->prepareToPlay(48000., 64); candidate->prepareToPlay(48000., 64);
        juce::AudioBuffer<float> a(2, 257), b(2, 257);
        juce::MidiBuffer midi;
        std::array<std::complex<double>, 2> reference{}, actual{};
        const std::array<double, 2> frequencies{220., 12000.};
        for (int offset = 0; offset < 96000; offset += 257) {
            const int count = std::min(257, 96000 - offset);
            a.setSize(2, count, false, false, true); b.setSize(2, count, false, false, true);
            fill(a, offset, 48000.); fill(b, offset, 48000.);
            baseline->processBlock(a, midi); candidate->processBlock(b, midi);
            if (offset + count <= 48000) continue;
            for (int n = 0; n < count; ++n) {
                if (offset + n < 48000) continue;
                for (size_t f = 0; f < frequencies.size(); ++f) {
                    const auto oscillator = std::polar(1., -2. * pi * frequencies[f] * (offset + n) / 48000.);
                    reference[f] += double(a.getSample(0, n)) * oscillator;
                    actual[f] += double(b.getSample(0, n)) * oscillator;
                }
            }
        }
        check(std::abs(reference[0]) > 100. && std::abs(reference[1]) > 100.,
              "Full processor fixture did not produce measurable reference tones");
        const double low = 20. * std::log10(std::abs(actual[0]) / std::abs(reference[0]));
        const double high = 20. * std::log10(std::abs(actual[1]) / std::abs(reference[1]));
        check(std::abs(low) < .1 && high < -8. && high > -9.1,
              "Production processor did not apply the requested independent high-band dynamics");
        check(candidate->getLatencySamples() == baseline->getLatencySamples(),
              "Enabling multiband/de-esser changed reported host sample latency");
        std::cout << "production " << (stage == 0 ? "multiband" : "de-esser")
                  << " low=" << low << " dB high=" << high << " dB\n";
        baseline->releaseResources(); candidate->releaseResources();
    }
    std::cout << "PASS production placement, frequency-selective processing, oversized host blocks and unchanged reported latency\n";
}

void analysisSourceOwnership() {
    auto processor = std::make_unique<ChimeraProcessor>();
    isolate(*processor);
    processor->setPedalModel(2, 6);
    processor->setPedalModel(4, 7); // A second compressor must not share the selected owner's stream.
    processor->setPedalBoardEnabled(false);
    processor->prepareToPlay(48000., 64);
    auto& analysis = processor->dynamicsAnalysis();
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    int offset = 0;
    const auto capturedSamples = [&] {
        analysis.fifo.discardPending();
        fill(audio, offset, 48000.); offset += audio.getNumSamples();
        processor->processBlock(audio, midi);
        return analysis.fifo.pendingSamples();
    };
    analysis.select(DynamicsStage::preCompressor);
    analysis.fifo.enabled.store(true);
    analysis.preOwner.store(2);
    check(capturedSamples() == 0, "Inactive native PRE view displayed legacy compressor samples");
    analysis.preOwner.store(-1);
    check(capturedSamples() == 64, "Legacy PRE view lost its own actual compressor boundary");
    processor->setPedalBoardEnabled(true);
    analysis.preOwner.store(2);
    check(capturedSamples() == 64, "Selected native PRE owner did not yield exactly one block");
    processor->setPedalModel(2, 1); // Replace only the viewed compressor with a drive.
    check(capturedSamples() == 0, "Old PRE compressor view displayed its replacement drive");
    analysis.preOwner.store(4);
    check(capturedSamples() == 64, "Another native compressor owner could not be selected independently");
    analysis.preOwner.store(-1);
    check(capturedSamples() == 0, "Legacy PRE view displayed an active native pedalboard owner");

    analysis.select(DynamicsStage::postCompressor);
    set(*processor, postNativeModeID(0), 0.f);
    analysis.postNativeSource.store(1);
    check(capturedSamples() == 0, "Inactive native POST view displayed legacy compressor samples");
    analysis.postNativeSource.store(0);
    check(capturedSamples() == 64, "Legacy POST view lost the active legacy boundary");
    set(*processor, postNativeModeID(0), 1.f);
    check(capturedSamples() == 0, "Legacy POST view displayed a native compressor");
    analysis.postNativeSource.store(1);
    check(capturedSamples() == 64, "Native POST view lost the active native boundary");
    analysis.postNativeSource.store(-1);
    check(capturedSamples() == 64, "Generic POST slot observation lost native audio");
    set(*processor, postNativeModeID(0), 0.f);
    check(capturedSamples() == 64, "Generic POST slot observation lost legacy audio");
    analysis.disable();
    processor->releaseResources();
    std::cout << "PASS production analyzer isolation for exact PRE owner/model and legacy/native POST source\n";
}
}

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        auto processor = std::make_unique<ChimeraProcessor>();
        parameterContract(*processor);
        wrapperAudio(*processor);
        recall(*processor);
        productionPlacement();
        analysisSourceOwnership();
        std::cout << "PASS multiband/de-esser host integration contracts\n";
        return 0;
    } catch (const std::exception& e) {
        counting = false;
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}

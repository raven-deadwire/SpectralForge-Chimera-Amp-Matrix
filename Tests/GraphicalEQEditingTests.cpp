#include "PluginProcessor.h"
#include "GuitarSignaturePresets.h"
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

using namespace spectralforge;

namespace {
constexpr uint16_t allSlots = (1u << graphicalEQBands) - 1u;
constexpr std::array<const char*, 5> fields { "enabled", "frequency", "gain", "q", "type" };

void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

bool near(float a, float b, float tolerance = .002f) {
    return std::abs(a - b) <= tolerance;
}

float get(ChimeraProcessor& processor, int bank, int band, const char* field) {
    const auto* value = processor.parameters().getRawParameterValue(eqID(bank, band, field));
    check(value != nullptr, "Missing released EQ parameter");
    return value->load();
}

void hostSet(ChimeraProcessor& processor, int bank, int band, const char* field, float value) {
    auto* parameter = processor.parameters().getParameter(eqID(bank, band, field));
    check(parameter != nullptr, "Missing host-write target");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void localSet(ChimeraProcessor& processor, int bank, int band, const char* field, float value,
              const char* label = "Numeric edit") {
    auto& edits = processor.eqEditing();
    check(edits.beginEdit(bank, label), "Failed to begin local edit");
    check(edits.setValue(bank, band, field, value), "Failed to write local edit");
    edits.endEdit();
}

struct HostEvents : juce::AudioProcessorParameter::Listener {
    std::atomic<int> changes { 0 }, starts { 0 }, ends { 0 }, depth { 0 };
    std::atomic<bool> badNesting { false };

    void parameterValueChanged(int, float) override { ++changes; }
    void parameterGestureChanged(int, bool starting) override {
        if (starting) {
            ++starts;
            if (depth.fetch_add(1) != 0) badNesting.store(true);
        } else {
            ++ends;
            if (depth.fetch_sub(1) != 1) badNesting.store(true);
        }
    }
    void clear() {
        check(depth.load() == 0 && !badNesting.load(), "Cannot clear an unbalanced host trace");
        changes.store(0); starts.store(0); ends.store(0);
    }
};

// Simulates a synchronous host reaction to a plugin parameter notification.
// The nested write is host-owned even though it runs on the editor's thread.
struct ReentrantHostChange : juce::AudioProcessorParameter::Listener {
    explicit ReentrantHostChange(ChimeraProcessor& p) : processor(p) {}
    ChimeraProcessor& processor;
    int targetBank { 1 }, targetBand { 4 };
    const char* targetField { "gain" };
    float targetValue { -3.f };
    bool armed { true };
    void parameterValueChanged(int, float) override {
        if (armed) {
            armed = false;
            hostSet(processor, targetBank, targetBand, targetField, targetValue);
        }
    }
    void parameterGestureChanged(int, bool) override {}
};

class EQHostTrace {
public:
    explicit EQHostTrace(ChimeraProcessor& processor) {
        int ordinal = 0;
        for (int bank = 0; bank < 2; ++bank) {
            attach(processor, ordinal++, eqID(bank, -1, "bypass"));
            for (int band = 0; band < graphicalEQBands; ++band)
                for (const auto* field : fields) attach(processor, ordinal++, eqID(bank, band, field));
        }
    }
    ~EQHostTrace() {
        for (size_t i = 0; i < parameters.size(); ++i)
            if (parameters[i] != nullptr) parameters[i]->removeListener(&events[i]);
    }
    HostEvents& event(int bank, int band, const char* field) {
        if (band < 0) return events[size_t(bank * 61)];
        for (size_t index = 0; index < fields.size(); ++index)
            if (std::string(field) == fields[index]) return events[size_t(bank * 61 + 1 + band * 5) + index];
        throw std::runtime_error("Invalid test trace field");
    }
    void clear() { for (auto& item : events) item.clear(); }
    void balanced(const char* operation) const {
        for (const auto& item : events)
            check(!item.badNesting.load() && item.depth.load() == 0 && item.starts.load() == item.ends.load(),
                  std::string(operation) + ": unbalanced or nested host gestures");
    }
private:
    void attach(ChimeraProcessor& processor, int ordinal, const juce::String& id) {
        auto* parameter = processor.parameters().getParameter(id);
        check(parameter != nullptr, "Trace target missing");
        parameters[size_t(ordinal)] = parameter;
        parameter->addListener(&events[size_t(ordinal)]);
    }
    std::array<juce::RangedAudioParameter*, graphicalEQParameterCount> parameters {};
    std::array<HostEvents, graphicalEQParameterCount> events;
};

juce::ValueTree stateTree(ChimeraProcessor& processor) {
    juce::MemoryBlock bytes;
    processor.getStateInformation(bytes);
    const auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), int(bytes.getSize()));
    check(xml != nullptr, "Project serialization failed");
    return juce::ValueTree::fromXml(*xml);
}

void restore(ChimeraProcessor& processor, const juce::ValueTree& tree) {
    juce::MemoryBlock bytes;
    const auto xml = tree.createXml();
    check(xml != nullptr, "Test state XML failed");
    juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    processor.setStateInformation(bytes.getData(), int(bytes.getSize()));
}

void removeMetadata(juce::ValueTree tree) {
    for (int index = tree.getNumChildren() - 1; index >= 0; --index)
        if (tree.getChild(index).hasType("EQ_EDIT_STATE")) tree.removeChild(index, nullptr);
}

void stripEQ(juce::ValueTree tree) {
    removeMetadata(tree);
    for (int index = tree.getNumChildren() - 1; index >= 0; --index) {
        const auto id = tree.getChild(index).getProperty("id").toString();
        if (id.startsWith("toneEQ_") || id.startsWith("finalEQ_")) tree.removeChild(index, nullptr);
    }
}

void hostParameterContract() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage;
    check(processor.getParameters().size() == 4967+19+spectralforge::commonDynamicsParameterCount+spectralforge::multibandParameterCount+spectralforge::deEsserParameterCount+spectralforge::extraGateParameterCount+spectralforge::multibandStereoParameterCount+spectralforge::saturationParameterCount, "Unexpected total host-parameter count after append-only tuner/dynamics");
    std::set<std::string> unique;
    for (auto* parameter : processor.getParameters()) {
        auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        check(identified != nullptr && unique.insert(identified->paramID.toStdString()).second,
              "Missing or duplicate released host parameter ID");
    }
    int ordinal = 4845;
    for (int bank = 0; bank < 2; ++bank) {
        const juce::String title = bank == 0 ? "Tone EQ " : "Final EQ ";
        auto* bypass = processor.parameters().getParameter(eqID(bank, -1, "bypass"));
        check(bypass->getParameterIndex() == ordinal++ && bypass->getVersionHint() == 10
              && bypass->isAutomatable() && bypass->getDefaultValue() == 1.f
              && bypass->getName(128) == title + "bypass", "Released bypass contract changed");
        for (int band = 0; band < graphicalEQBands; ++band) {
            for (size_t field = 0; field < fields.size(); ++field) {
                const auto id = eqID(bank, band, fields[field]);
                auto* parameter = processor.parameters().getParameter(id);
                check(parameter != nullptr && parameter->getParameterIndex() == ordinal++
                      && parameter->getVersionHint() == 10 && parameter->isAutomatable(),
                      "Released EQ ID/order/version/automation changed: " + id.toStdString());
                const auto name = field == 3 ? "Q" : field == 4 ? "filter" : fields[field];
                check(parameter->getName(128) == title + juce::String(band + 1) + " " + name,
                      "Released EQ parameter display name changed");
                const auto& range = parameter->getNormalisableRange();
                constexpr std::array<float, 5> lower { 0, 20, -24, .1f, 0 };
                constexpr std::array<float, 5> upper { 1, 20000, 24, 18, 6 };
                check(range.start == lower[field] && range.end == upper[field], "Released EQ range changed");
                const float expected = field == 1 ? graphicalEQFrequencies[size_t(band)]
                                     : field == 3 ? .70710678f : 0.f;
                check(near(parameter->convertFrom0to1(parameter->getDefaultValue()), expected, field == 1 ? .02f : .0011f),
                      "Released EQ default changed");
                if (field == 1 || field == 3) {
                    const float centre = field == 1 ? 1000.f : 1.f;
                    check(std::abs(parameter->convertTo0to1(centre) - .5f) < 1.e-5f,
                          "Released frequency/Q normalization changed");
                }
                if (field > 0 && field < 4) {
                    const float interval = field == 3 ? .001f : .01f;
                    check(range.interval == interval, "Released EQ step interval changed");
                }
                if (field == 4) {
                    const auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter);
                    const juce::StringArray released { "Bell", "Low shelf", "High shelf", "High pass 12", "Low pass 12", "Notch", "Band pass" };
                    check(choice != nullptr && choice->choices == released, "Released filter-choice meanings changed");
                }
            }
        }
    }
    check(ordinal == 4967, "Incomplete released EQ parameter coverage");
    std::cout << "PASS 4,967 unique host IDs / all 122 EQ ordinals, names, version hints, ranges, defaults, steps and choices\n";
}

void occupancyAndSlotReuse() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage; auto& edits = processor.eqEditing();
    EQHostTrace trace(processor);
    check(edits.occupiedMask(0) == 0 && edits.occupiedMask(1) == 0, "Fresh workspace is not empty");
    const float bypass = get(processor, 0, -1, "bypass");
    check(edits.addBand(0, 437.5f, 6.f) == 0, "First available slot is not Band 1");
    check(edits.isOccupied(0, 0) && !edits.isOccupied(1, 0) && get(processor, 0, 0, "enabled") == 1.f,
          "Creation did not independently establish occupancy and enable");
    check(get(processor, 0, -1, "bypass") == bypass, "Creation silently changed the global EQ bypass");
    localSet(processor, 0, 0, "q", 3.5f); localSet(processor, 0, 0, "type", 2.f);
    localSet(processor, 0, 0, "enabled", 0.f, "Bypass band");
    check(edits.isOccupied(0, 0) && get(processor, 0, 0, "enabled") == 0.f,
          "Band bypass was confused with deletion");
    check(edits.removeBand(0, 0), "Delete rejected an occupied, bypassed band");
    check(!edits.isOccupied(0, 0) && get(processor, 0, 0, "enabled") == 0.f
          && near(get(processor, 0, 0, "frequency"), 437.5f, .02f)
          && near(get(processor, 0, 0, "gain"), 6.f)
          && near(get(processor, 0, 0, "q"), 3.5f) && get(processor, 0, 0, "type") == 2.f,
          "Deletion did not hide/disable while retaining the slot's tuning values");
    check(!edits.removeBand(0, 0), "Deleting an absent slot created an operation");
    check(edits.addBand(0, 1000.f, -4.f) == 0, "Deletion renumbered or failed to recycle the first vacancy");
    check(near(get(processor, 0, 0, "frequency"), 1000.f, .02f)
          && near(get(processor, 0, 0, "gain"), -4.f)
          && near(get(processor, 0, 0, "q"), .70710678f)
          && get(processor, 0, 0, "type") == 0.f && get(processor, 0, 0, "enabled") == 1.f,
          "Reused slot retained stale filter type/Q or ignored creation values");
    for (int band = 1; band < graphicalEQBands; ++band)
        check(edits.addBand(0, 100.f + 110.f * band, float(band)) == band, "Vacancies are not allocated deterministically");
    check(edits.occupiedMask(0) == allSlots && edits.occupiedMask(1) == 0, "Tone slot capacity leaked into Final");
    check(edits.addBand(0, 2000.f, 3.f) == -1, "A thirteenth band exceeded fixed host capacity");
    check(edits.undo() && edits.occupiedMask(0) == (allSlots & ~(1u << 11)),
          "Rejected thirteenth creation polluted history");
    check(edits.addBand(1, 3000.f, 2.f) == 0 && edits.occupiedMask(1) == 1,
          "Final does not have its independent 12 slots");
    check(edits.nextUndoBank() == 1, "Shared history failed to identify the owning bank");
    trace.balanced("Create, bypass, delete, reuse and capacity");
    std::cout << "PASS occupied/bypassed/deleted distinction / first-vacancy reuse / 12-slot cap / independent banks / preserved bypass\n";
}

void transactionGrouping() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage; auto& edits = processor.eqEditing();
    EQHostTrace trace(processor);
    check(edits.addBand(0, 1000.f, 0.f) == 0, "Grouping fixture creation");
    trace.clear();
    check(edits.beginEdit(0, "Move band"), "Graph drag transaction rejected");
    for (int step = 1; step <= 80; ++step) {
        check(edits.setValue(0, 0, "frequency", 1000.f + 10.f * step), "Graph frequency write rejected");
        check(edits.setValue(0, 0, "gain", float(step) / 10), "Graph gain write rejected");
    }
    edits.endEdit();
    check(trace.event(0, 0, "frequency").starts.load() == 1 && trace.event(0, 0, "frequency").ends.load() == 1
          && trace.event(0, 0, "gain").starts.load() == 1 && trace.event(0, 0, "gain").ends.load() == 1,
          "A multi-update drag was not a single host gesture per changed parameter");
    check(trace.event(0, 0, "q").changes.load() == 0, "Drag touched an unrelated EQ field");
    check(edits.canUndo() && edits.undoDescription().isNotEmpty(), "Completed graph edit has no history entry");
    check(edits.undo() && near(get(processor, 0, 0, "frequency"), 1000.f, .02f)
          && near(get(processor, 0, 0, "gain"), 0.f) && edits.isOccupied(0, 0),
          "Undo did not revert the whole drag in one step");
    check(edits.redo() && near(get(processor, 0, 0, "frequency"), 1800.f, .02f)
          && near(get(processor, 0, 0, "gain"), 8.f), "Redo lost the final graph coordinates");
    check(edits.undo(), "No-op redo fixture");
    check(edits.beginEdit(0, "No movement"), "No-op transaction rejected");
    (void)edits.setValue(0, 0, "frequency", get(processor, 0, 0, "frequency"));
    edits.endEdit();
    check(edits.canRedo() && edits.redo(), "No-op click discarded redo history");
    localSet(processor, 0, 0, "q", 2.125f);
    check(edits.undo() && near(get(processor, 0, 0, "q"), .70710678f), "Numeric edit did not form a single reversible step");
    check(edits.undo() && near(get(processor, 0, 0, "gain"), 0.f), "Numeric undo swallowed the prior graph operation");
    check(edits.undo() && edits.occupiedMask(0) == 0 && !edits.canUndo(), "Creation was not one reversible operation");
    check(edits.redo() && edits.redo() && edits.redo() && near(get(processor, 0, 0, "q"), 2.125f),
          "Create/drag/numeric redo order changed");
    check(edits.removeBand(0, 0) && !edits.isOccupied(0, 0), "Delete history fixture");
    check(edits.undo() && edits.isOccupied(0, 0) && get(processor, 0, 0, "enabled") == 1.f
          && near(get(processor, 0, 0, "q"), 2.125f), "Deletion undo did not restore occupancy, enable and tuning atomically");
    check(edits.redo() && !edits.isOccupied(0, 0) && get(processor, 0, 0, "enabled") == 0.f,
          "Deletion redo left an active hidden band");
    trace.balanced("Drag, numeric, creation/deletion undo/redo");
    std::cout << "PASS one-step graph/numeric/create/delete Undo/Redo / redo survives no-op / balanced host gestures\n";
}

void externalOwnership() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage; auto& edits = processor.eqEditing();
    EQHostTrace trace(processor);
    check(edits.addBand(0, 1000.f, 3.f) == 0, "External ownership fixture");
    auto* output = processor.parameters().getParameter("output");
    output->setValueNotifyingHost(output->convertTo0to1(-9.f));
    const float hostOutput = processor.parameters().getRawParameterValue("output")->load();
    check(near(hostOutput, -9.f), "Unrelated output automation fixture was not applied");
    // The host range may round -9 by an ULP (notably with ARM fused arithmetic).
    // Undo must preserve the actual host value exactly, without a tolerance.
    check(edits.undo() && processor.parameters().getRawParameterValue("output")->load() == hostOutput,
          "EQ undo rewound an unrelated processor parameter or non-EQ automation erased EQ history");
    check(edits.redo(), "External ownership redo fixture");
    check(edits.beginEdit(0, "Interrupted drag") && edits.setValue(0, 0, "frequency", 1750.f),
          "Interrupted drag could not start");
    std::thread automation([&] { hostSet(processor, 1, 4, "gain", -7.f); });
    automation.join();
    check(!edits.canUndo() && !edits.canRedo(), "External EQ automation became undoable or left stale local history");
    check(!edits.setValue(0, 0, "frequency", 2200.f), "An interrupted transaction kept writing after external ownership changed");
    edits.endEdit();
    check(near(get(processor, 0, 0, "frequency"), 1750.f, .02f)
          && near(get(processor, 1, 4, "gain"), -7.f), "History cancellation rewound current local/host values");
    trace.balanced("Automation-interrupted drag");
    localSet(processor, 0, 0, "gain", 5.f);
    check(edits.canUndo(), "New user operation after automation did not start fresh history");
    hostSet(processor, 0, 0, "gain", 11.f);
    check(!edits.undo() && near(get(processor, 0, 0, "gain"), 11.f), "Undo overwrote a host-owned parameter value");
    localSet(processor, 0, 0, "gain", 12.f);
    check(edits.canUndo(), "Same-value host ownership fixture has no history");
    hostSet(processor, 0, 0, "frequency", get(processor, 0, 0, "frequency"));
    check(!edits.canUndo() && !edits.canRedo(), "An unchanged-value host notification failed to establish external ownership");
    auto* notificationSource = processor.parameters().getParameter(eqID(0, 0, "frequency"));
    ReentrantHostChange reentrant(processor);
    notificationSource->addListener(&reentrant);
    check(edits.beginEdit(0, "Reentrant host edit"), "Reentrant host fixture start");
    (void)edits.setValue(0, 0, "frequency", 1775.f);
    edits.endEdit();
    notificationSource->removeListener(&reentrant);
    check(!reentrant.armed && !edits.canUndo() && !edits.canRedo() && near(get(processor, 1, 4, "gain"), -3.f),
          "A synchronous nested host write was misclassified as local history");
    trace.balanced("Synchronous host reentry");
    check(edits.beginEdit(0, "Replay interrupted by host")
          && edits.setValue(0, 0, "frequency", 1880.f) && edits.setValue(0, 0, "gain", 6.f),
          "Reentrant replay fixture start");
    edits.endEdit();
    ReentrantHostChange replayHost(processor);
    replayHost.targetBank = 0; replayHost.targetBand = 0; replayHost.targetValue = 14.f;
    notificationSource->addListener(&replayHost);
    const bool replayed = edits.undo();
    notificationSource->removeListener(&replayHost);
    check(!replayed && !replayHost.armed && !edits.canUndo() && !edits.canRedo()
          && near(get(processor, 0, 0, "gain"), 14.f),
          "Undo continued replay after a nested host write and overwrote the host's newer value");
    trace.balanced("Host-interrupted multi-field replay");
    check(edits.beginEdit(0, "Same-parameter host reply"), "Same-parameter host fixture start");
    ReentrantHostChange sameParameter(processor);
    sameParameter.targetBank = 0; sameParameter.targetBand = 0;
    sameParameter.targetField = "frequency"; sameParameter.targetValue = 999.f;
    notificationSource->addListener(&sameParameter);
    (void)edits.setValue(0, 0, "frequency", 1950.f);
    edits.endEdit();
    notificationSource->removeListener(&sameParameter);
    check(!sameParameter.armed && !edits.canUndo() && !edits.canRedo()
          && near(get(processor, 0, 0, "frequency"), 999.f, .02f),
          "A host override of the currently written parameter was lost");
    trace.balanced("Same-parameter host reply");
    ReentrantHostChange fineOverride(processor);
    fineOverride.targetBank = 0; fineOverride.targetBand = 0;
    fineOverride.targetField = "frequency"; fineOverride.targetValue = 19000.01f;
    notificationSource->addListener(&fineOverride);
    check(edits.beginEdit(0, "Fine host frequency override"), "Fine host override fixture start");
    (void)edits.setValue(0, 0, "frequency", 19000.f);
    edits.endEdit();
    notificationSource->removeListener(&fineOverride);
    check(!fineOverride.armed && !edits.canUndo() && !edits.canRedo()
          && get(processor, 0, 0, "frequency") > 19000.f,
          "Normalized-value tolerance misclassified a one-step high-frequency host override as local");
    trace.balanced("One-step high-frequency host override");
    check(edits.removeBand(0, 0), "Cancel edit fixture deletion");
    check(edits.addBand(1, 2400.f, 0.f) == 0, "Cancel fixture band creation");
    check(edits.beginEdit(1, "Canceled edit") && edits.setValue(1, 0, "gain", -2.f), "Cancel fixture start");
    edits.cancelEdit();
    check(near(get(processor, 1, 0, "gain"), -2.f) && !edits.canUndo() && !edits.canRedo(),
          "Cancel did not preserve current values as a fresh history baseline");
    trace.balanced("Explicit transaction cancellation");
    std::cout << "PASS host automation ownership / canceled active gestures / no external values in history / unrelated audio parameters preserved\n";
}

void projectAndComparisonRecall() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage; auto& edits = processor.eqEditing();
    check(edits.addBand(0, 200.f, 4.f) == 0 && edits.addBand(0, 700.f, -2.f) == 1, "A fixture creation");
    check(edits.removeBand(0, 0) && edits.addBand(1, 3000.f, 1.f) == 0, "A fixture occupancy");
    processor.copyComparison();
    check(!edits.canUndo() && !edits.canRedo(), "Global Copy A/B failed to delimit EQ history");
    processor.selectComparison(1);
    check(edits.occupiedMask(0) == 2 && edits.occupiedMask(1) == 1, "A-to-B copy lost occupancy");
    check(edits.removeBand(0, 1) && edits.addBand(0, 1700.f, 7.f) == 0
          && edits.addBand(1, 6000.f, -3.f) == 1, "B fixture creation");
    processor.selectComparison(0);
    check(edits.occupiedMask(0) == 2 && edits.occupiedMask(1) == 1
          && near(get(processor, 0, 1, "frequency"), 700.f, .02f), "Global A recall mixed B occupancy or values");
    check(!edits.canUndo() && !edits.canRedo(), "Global A/B recall leaked pre-recall history");
    localSet(processor, 0, 1, "q", 1.75f);
    const auto saved = stateTree(processor);
    check(edits.canUndo(), "Ordinary project save cleared session edit history");
    const auto currentMetadata = saved.getChildWithName("EQ_EDIT_STATE");
    check(currentMetadata.isValid() && int(currentMetadata.getProperty("schema")) == 1
          && int(currentMetadata.getProperty("toneOccupied")) == 2
          && int(currentMetadata.getProperty("finalOccupied")) == 1,
          "Current core omitted versioned occupancy metadata");
    const auto comparisons = saved.getChildWithName("COMPARISONS");
    check(comparisons.getNumChildren() == 2, "Saved project lost a global A/B slot");
    for (auto comparison : comparisons)
        check(comparison.getChildWithName("EQ_EDIT_STATE").isValid(), "Global A/B slot did not serialize its own occupancy");
    auto recalled = std::make_unique<ChimeraProcessor>();
    restore(*recalled, saved);
    check(recalled->eqEditing().occupiedMask(0) == 2 && recalled->eqEditing().occupiedMask(1) == 1
          && near(get(*recalled, 0, 1, "q"), 1.75f)
          && !recalled->eqEditing().canUndo() && !recalled->eqEditing().canRedo(),
          "Project recall lost occupied/bypassed slots or persisted local history");
    recalled->selectComparison(1);
    check(recalled->eqEditing().occupiedMask(0) == 1 && recalled->eqEditing().occupiedMask(1) == 3
          && near(get(*recalled, 0, 0, "frequency"), 1700.f, .02f)
          && near(get(*recalled, 1, 1, "frequency"), 6000.f, .02f),
          "Restored project B lost its distinct Tone/Final slot layout");
    recalled->selectComparison(0);
    check(recalled->eqEditing().occupiedMask(0) == 2 && recalled->eqEditing().occupiedMask(1) == 1,
          "Restored A/B roundtrip contaminated A");
    restore(processor, saved);
    check(!edits.canUndo() && !edits.canRedo(), "Same-values state recall failed to clear old history");
    std::cout << "PASS binary project / independent global A-B slot occupancy / save preserves session history / recall clears history\n";
}

void legacyAndMalformedMigration() {
    auto original = std::make_unique<ChimeraProcessor>();
    check(original->eqEditing().addBand(0, 875.f, -6.f) == 0, "Legacy fixture creation");
    localSet(*original, 0, 0, "enabled", 0.f);
    auto legacy = stateTree(*original);
    removeMetadata(legacy);
    for (auto comparison : legacy.getChildWithName("COMPARISONS")) removeMetadata(comparison);
    auto recalled = std::make_unique<ChimeraProcessor>();
    restore(*recalled, legacy);
    check(recalled->eqEditing().occupiedMask(0) == allSlots && recalled->eqEditing().occupiedMask(1) == allSlots
          && near(get(*recalled, 0, 0, "frequency"), 875.f, .02f)
          && near(get(*recalled, 0, 0, "gain"), -6.f) && get(*recalled, 0, 0, "enabled") == 0.f,
          "P0 project migration hid legacy disabled nodes or changed saved values");
    auto preEQ = legacy.createCopy();
    stripEQ(preEQ);
    for (auto comparison : preEQ.getChildWithName("COMPARISONS")) stripEQ(comparison);
    hostSet(*recalled, 0, -1, "bypass", 0.f); hostSet(*recalled, 0, 0, "enabled", 1.f);
    restore(*recalled, preEQ);
    check(get(*recalled, 0, -1, "bypass") == 1.f && get(*recalled, 1, -1, "bypass") == 1.f,
          "Pre-EQ migration inherited current-session global bypass");
    for (int bank = 0; bank < 2; ++bank) for (int band = 0; band < graphicalEQBands; ++band)
        check(get(*recalled, bank, band, "enabled") == 0.f
              && near(get(*recalled, bank, band, "frequency"), graphicalEQFrequencies[size_t(band)], .02f),
              "Pre-EQ migration did not use released parameter defaults");
    for (int scenario = 0; scenario < 8; ++scenario) {
        auto malformed = stateTree(*original);
        auto metadata = malformed.getChildWithName("EQ_EDIT_STATE");
        check(metadata.isValid(), "Malformed fixture has no metadata");
        if (scenario == 0) metadata.setProperty("schema", 2, nullptr);
        if (scenario == 1) metadata.setProperty("toneOccupied", -1, nullptr);
        if (scenario == 2) metadata.setProperty("finalOccupied", 4096, nullptr);
        if (scenario == 3) metadata.setProperty("toneOccupied", 1.5, nullptr);
        if (scenario == 4) metadata.setProperty("toneOccupied", "not-a-mask", nullptr);
        if (scenario == 5) metadata.removeProperty("finalOccupied", nullptr);
        if (scenario == 6) malformed.appendChild(metadata.createCopy(), nullptr);
        if (scenario == 7) metadata.setProperty("schema", "01", nullptr);
        restore(*recalled, malformed);
        check(recalled->eqEditing().occupiedMask(0) == allSlots && recalled->eqEditing().occupiedMask(1) == allSlots,
              "Malformed/future/duplicate metadata did not conservatively retain legacy slots, scenario=" + std::to_string(scenario));
    }
    auto inconsistent = stateTree(*original);
    auto metadata = inconsistent.getChildWithName("EQ_EDIT_STATE");
    metadata.setProperty("toneOccupied", 0, nullptr);
    auto enabled = inconsistent.getChildWithProperty("id", eqID(0, 0, "enabled"));
    enabled.setProperty("value", 1.f, nullptr);
    restore(*recalled, inconsistent);
    check(recalled->eqEditing().isOccupied(0, 0) && get(*recalled, 0, 0, "enabled") == 1.f,
          "Recall permitted audible processing in a hidden slot");
    std::cout << "PASS P0/pre-EQ migration / disabled tuning preservation / malformed metadata fallback / no hidden enabled band after recall\n";
}

void factoryResetAndBounds() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage; auto& edits = processor.eqEditing();
    for (int preset = 0; preset < selectablePresetCount; ++preset) {
        // Band 12 is deliberately outside the authored finishing bands. Seed
        // both its audible state and dynamic assignments before every recall.
        for (int bank = 0; bank < 2; ++bank) {
            hostSet(processor, bank, 11, "frequency", 17000.f);
            hostSet(processor, bank, 11, "gain", 7.f);
            hostSet(processor, bank, 11, "q", 4.f);
            hostSet(processor, bank, 11, "type", 0.f);
            hostSet(processor, bank, 11, "enabled", 1.f);
            hostSet(processor, bank, -1, "bypass", 0.f);
            check(edits.isOccupied(bank, 11), "Factory dirty-state Band 12 fixture");
        }
        for (int slot = 0; slot < 4; ++slot) {
            auto* target = processor.parameters().getParameter(dynamicSlotID(slot, "target"));
            check(target != nullptr, "Factory dirty-state dynamic target missing");
            target->setValueNotifyingHost(target->convertTo0to1(float((slot % 2 + 1) * graphicalEQBands)));
        }
        localSet(processor, 0, 11, "gain", 8.f);
        localSet(processor, 0, 11, "gain", 9.f);
        check(edits.undo() && edits.canUndo() && edits.canRedo(), "Factory dirty-state Undo/Redo fixture");
        processor.loadFactoryPreset(preset);
        for (int bank = 0; bank < 2; ++bank) {
            uint16_t enabledMask = 0;
            for (int band = 0; band < graphicalEQBands; ++band)
                if (get(processor, bank, band, "enabled") > .5f) enabledMask |= uint16_t(1u << band);
            check(enabledMask != 0 && edits.occupiedMask(bank) == enabledMask,
                  "Factory EQ occupancy differs from its active recipe, index=" + std::to_string(preset));
            check(get(processor, bank, -1, "bypass") == 0.f,
                  "Factory finishing EQ is bypassed, index=" + std::to_string(preset));
            check(!edits.isOccupied(bank, 11) && get(processor, bank, 11, "enabled") == 0.f,
                  "Factory recall retained a previous Band 12, index=" + std::to_string(preset));
            for (const auto* field : fields) {
                const auto* parameter = processor.parameters().getParameter(eqID(bank, 11, field));
                check(near(get(processor, bank, 11, field),
                           parameter->convertFrom0to1(parameter->getDefaultValue()),
                           std::string(field) == "frequency" ? .02f : .002f),
                      "Unused factory EQ slot retained old tuning, index=" + std::to_string(preset));
            }
        }
        std::set<int> assignedTargets;
        for (int slot = 0; slot < 4; ++slot) {
            const int target = int(processor.parameters().getRawParameterValue(dynamicSlotID(slot, "target"))->load());
            if (slot >= 2) {
                check(target == 0, "Unused factory dynamic slot retained an assignment, index=" + std::to_string(preset));
                continue;
            }
            check(target > 0 && target <= 2 * graphicalEQBands && assignedTargets.insert(target).second,
                  "Factory dynamic targets are missing, invalid or duplicated, index=" + std::to_string(preset));
            const int bank = (target - 1) / graphicalEQBands, band = (target - 1) % graphicalEQBands;
            check(band != 11 && get(processor, bank, band, "enabled") == 1.f
                  && get(processor, bank, band, "type") <= 2.f,
                  "Factory dynamic target retained a stale or inactive band, index=" + std::to_string(preset));
        }
        check(!edits.canUndo() && !edits.canRedo() && !edits.undo() && !edits.redo(),
              "Factory preset inherited the previous EQ history, index=" + std::to_string(preset));
    }
    // The bounded editing history is independent of a factory recipe's bands.
    auto historyStorage = std::make_unique<ChimeraProcessor>();
    auto& historyProcessor = *historyStorage; auto& historyEdits = historyProcessor.eqEditing();
    check(historyEdits.addBand(0, 1000.f, 0.f) == 0, "History-capacity fixture");
    for (int index = 0; index < 80; ++index) localSet(historyProcessor, 0, 0, "gain", index % 2 == 0 ? 5.f : -5.f);
    int undone = 0;
    while (historyEdits.undo()) {
        check(++undone <= 64, "EQ edit history exceeded its bounded session capacity");
    }
    check(undone == 64, "EQ session history did not retain the most recent 64 operations");
    check(historyEdits.canRedo(), "Bounded history lost redo after undoing retained entries");
    std::cout << "PASS all " << selectablePresetCount << " factory active-band occupancy / stale Band 12 and dynamic-target reset / EQ history barrier / bounded 64-operation session history\n";
}

void fill(juce::AudioBuffer<float>& buffer, int offset, double rate, double frequency) {
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        const float value = .03f * float(std::sin(2. * juce::MathConstants<double>::pi * frequency * (offset + sample) / rate));
        buffer.setSample(0, sample, value);
        if (buffer.getNumChannels() > 1) buffer.setSample(1, sample, -.7f * value);
    }
}

void deletedAutomationAudio() {
    auto storage = std::make_unique<ChimeraProcessor>();
    auto reference = std::make_unique<ChimeraProcessor>();
    auto& processor = *storage; auto& edits = processor.eqEditing();
    check(edits.addBand(0, 1000.f, 12.f) == 0, "Deleted automation fixture");
    hostSet(processor, 0, -1, "bypass", 0.f);
    processor.graphicalEQ(0).prepare(48000.);
    juce::AudioBuffer<float> audio(2, 128), dry(2, 128), expected(2, 128);
    double inputEnergy = 0., outputEnergy = 0.;
    for (int block = 0; block < 160; ++block) {
        fill(audio, block * 128, 48000., 1000.);
        if (block >= 80) for (int sample = 0; sample < 128; ++sample) inputEnergy += audio.getSample(0, sample) * audio.getSample(0, sample);
        processor.graphicalEQ(0).process(audio);
        if (block >= 80) for (int sample = 0; sample < 128; ++sample) outputEnergy += audio.getSample(0, sample) * audio.getSample(0, sample);
    }
    check(std::abs(10. * std::log10(outputEnergy / inputEnergy) - 12.) < .05, "Audio fixture did not exercise an audible EQ band");
    check(edits.removeBand(0, 0), "Delete audible band failed");
    for (int block = 0; block < 40; ++block) { fill(audio, block * 128, 48000., 1000.); processor.graphicalEQ(0).process(audio); }
    fill(audio, 0, 48000., 1000.); dry.makeCopyOf(audio, true); processor.graphicalEQ(0).process(audio);
    for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 128; ++sample)
        check(audio.getSample(channel, sample) == dry.getSample(channel, sample), "Deleted band was not exact dry after its existing bypass smoothing settled");
    std::thread automation([&] {
        hostSet(processor, 0, 0, "frequency", 2300.f);
        hostSet(processor, 0, 0, "gain", -8.f);
        hostSet(processor, 0, 0, "q", 1.2f);
    });
    automation.join();
    check(!edits.isOccupied(0, 0) && get(processor, 0, 0, "enabled") == 0.f,
          "Automation of deleted tuning values silently created a band");
    hostSet(processor, 0, 0, "enabled", 1.f);
    check(edits.isOccupied(0, 0) && near(get(processor, 0, 0, "frequency"), 2300.f, .02f)
          && near(get(processor, 0, 0, "gain"), -8.f), "Host enable did not restore the same stable slot and retained tuning");
    hostSet(*reference, 0, -1, "bypass", 0.f);
    for (const auto* field : fields) hostSet(*reference, 0, 0, field, get(processor, 0, 0, field));
    for (double rate : { 44100., 48000., 96000., 192000. }) {
        processor.graphicalEQ(0).prepare(rate); reference->graphicalEQ(0).prepare(rate);
        for (int block = 0; block < 64; ++block) {
            fill(audio, block * 128, rate, 2300.); expected.makeCopyOf(audio, true);
            processor.graphicalEQ(0).process(audio); reference->graphicalEQ(0).process(expected);
            for (int channel = 0; channel < 2; ++channel) for (int sample = 0; sample < 128; ++sample)
                check(std::isfinite(audio.getSample(channel, sample)) && audio.getSample(channel, sample) == expected.getSample(channel, sample),
                      "Host-reoccupied band changed the existing EQ transfer");
        }
    }
    check(!edits.canUndo() && !edits.canRedo(), "Host reoccupation entered user edit history");
    std::cout << "PASS audible delete settles exact dry / tuning automation stays hidden / host enable reoccupies / four-rate exact EQ transfer\n";
}
}

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        hostParameterContract();
        occupancyAndSlotReuse();
        transactionGrouping();
        externalOwnership();
        projectAndComparisonRecall();
        legacyAndMalformedMigration();
        factoryResetAndBounds();
        deletedAutomationAudio();
        std::cout << "PASS October 23 EQ editing backend regression contract\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}

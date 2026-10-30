// 1.10 tracking checks on the synthetic corpus (docs/TRACKING_1_10.md):
// every in-range note meets the acceptance targets, out-of-range notes and
// the power chord are measured and reported, hum and hiss open no gate, and
// the tool's label file and offline reference agree with the exact labels.
#include "TrackingEvaluation.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace spectralforge::tracking;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }

void corpusMeetsTargets() {
    const Targets targets;std::vector<Report> reports;int inScope = 0, met = 0;
    for (const auto& p : corpus()) reports.push_back(evaluate(p.name, analyse(p.audio, p.rate), p.labels, p.outOfScope));
    std::cout << markdown(reports, targets);
    std::string failures;
    for (const auto& r : reports) {
        require(r.falseGateSeconds <= targets.falseGateSeconds, r.name + ": gate open outside notes");
        if (r.outOfScope) continue;
        for (const auto& n : r.notes) { ++inScope;if (meets(n, targets, !n.label.picked)) ++met;else failures += " " + r.name + " / " + n.label.name + ";"; }
    }
    std::cout << "MEASURE tracking corpus: " << met << " of " << inScope << " in-range notes meet the targets\n";
    require(failures.empty(), "Notes below the tracking targets:" + failures);
    std::cout << "PASS synthetic corpus: in-range notes meet the targets; no false triggers in hum and hiss\n";
}

void labelsAndReference() {
    const auto labels = readLabels("# start,end,pitch,name\n0.2,1.2,40,E2\n1.5,2.0,110.0,A2 in Hz\nbad line\n");
    require(labels.size() == 2 && std::abs(labels[0].pitchAt(.5) - noteHz(40)) < 1e-9 && std::abs(labels[1].pitchAt(1.6) - 110) < 1e-9 && labels[1].name == "A2 in Hz", "Label file not read");
    // The offline reference finds the steady notes of a performance.
    double worst = 0;int found = 0, expected = 0;
    for (const auto& p : corpus()) {
        if (p.outOfScope || p.name.find("bend") != std::string::npos || p.name.find("vibrato") != std::string::npos) continue;
        const auto reference = referenceLabels(p.audio, p.rate);
        for (const auto& truth : p.labels) {
            ++expected;
            for (const auto& r : reference)
                if (r.start < truth.end && r.end > truth.start && std::abs(cents(r.pitchAt(r.start), truth.pitchAt(truth.start))) < 50) {
                    ++found;worst = std::max(worst, std::abs(cents(r.pitchAt(r.start), truth.pitchAt(truth.start))));break;
                }
        }
    }
    std::cout << "MEASURE offline reference: found " << found << " of " << expected << " steady notes, worst " << worst << " cents\n";
    // The string model's inharmonicity reads low notes 6-9 cents sharp in both
    // trackers (0.1-0.2 cents without it), so the bound is 15 cents.
    require(found >= expected * 9 / 10 && worst < 15, "The offline reference misses steady notes");
    std::cout << "PASS label files, and the offline reference finds the steady notes\n";
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try { corpusMeetsTargets();labelsAndReference(); }
    catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << "\n";return 1; }
    return 0;
}

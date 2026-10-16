#pragma once
#include "PluginProcessor.h"
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace chimeraTest {
inline bool sameFloatBits(float a, float b) noexcept {
    return std::memcmp(&a, &b, sizeof(float)) == 0;
}

// A decimal grid point is not necessarily the literal float on every target:
// NormalisableRange::snapToLegalValue may contract start + interval * step.
// Require the EXACT released parameter roundtrip, as well as the pre-existing
// intent tolerance. Do not change parameter ranges, snapping or host ownership.
inline void requireParameterRoundtrip(ChimeraProcessor& processor, const juce::String& id,
                                      float requested, float intentTolerance) {
    auto* parameter = processor.parameters().getParameter(id);
    auto* raw = processor.parameters().getRawParameterValue(id);
    if (!parameter || !raw) throw std::runtime_error("Missing roundtrip parameter");
    const float expected = parameter->convertFrom0to1(parameter->convertTo0to1(requested));
    const float actual = raw->load();
    std::cout << "PARAMETER_ROUNDTRIP " << id << std::setprecision(9)
              << " requested=" << requested << " expected=" << expected << " actual=" << actual
              << " literal_equal=" << (actual == requested)
              << " canonical_bits_equal=" << sameFloatBits(actual, expected) << '\n';
    if (!std::isfinite(actual) || !std::isfinite(expected)
        || std::abs(actual - requested) > intentTolerance || !sameFloatBits(actual, expected))
        throw std::runtime_error("Parameter failed exact canonical roundtrip: " + id.toStdString());
}
}

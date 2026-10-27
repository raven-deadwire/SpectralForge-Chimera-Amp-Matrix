#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace spectralforge::standalone {
// Installed only by Source/ChimeraStandaloneApp.cpp; null in plug-ins and tests.
// Message thread. Describes the Standalone audio device for the diagnostic
// snapshot (Issue #113): type, rate, buffer, driver latency, xruns. Never the
// device name, which the snapshot deliberately excludes.
inline juce::var (*audioDeviceDiagnostics)() = nullptr;

inline void addReportedLatency(juce::var& device,int pluginLatencySamples) {
    auto* d=device.getDynamicObject();
    if(d==nullptr || !(bool)d->getProperty("open"))return;
    const double rate=d->getProperty("sample_rate");
    const double input=d->getProperty("driver_input_latency_samples");
    const double output=d->getProperty("driver_output_latency_samples");
    // An unavailable/invalid latency must not become a credible round-trip
    // estimate. Sum in double to avoid overflow on malformed driver values.
    if(d->hasProperty("driver_input_latency_samples") && d->hasProperty("driver_output_latency_samples")
       && std::isfinite(rate) && rate>0 && std::isfinite(input) && input>=0
       && std::isfinite(output) && output>=0 && pluginLatencySamples>=0)
        d->setProperty("driver_plus_plugin_latency_ms",1000.*(input+output+pluginLatencySamples)/rate);
}
}

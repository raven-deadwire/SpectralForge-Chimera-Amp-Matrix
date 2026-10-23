#pragma once
#include <juce_core/juce_core.h>

namespace spectralforge::standalone {
// Installed only by Source/ChimeraStandaloneApp.cpp; null in plug-ins and tests.
// Message thread. Describes the Standalone audio device for the diagnostic
// snapshot (Issue #113): type, rate, buffer, driver latency, xruns. Never the
// device name, which the snapshot deliberately excludes.
inline juce::var (*audioDeviceDiagnostics)() = nullptr;
}

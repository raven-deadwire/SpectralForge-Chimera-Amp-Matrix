#pragma once
#include <functional>

namespace spectralforge::standalone {
// Installed only by Source/ChimeraStandaloneApp.cpp; null in plug-ins and tests.
// Message thread. The Standalone starts with its audio input muted so a
// laptop's speakers cannot feed back into its microphone; the editors show a
// beginner guide instead of JUCE's notification bar while it stays muted.
struct InputMute {
    std::function<bool()> muted;
    std::function<void()> unmute,openAudioSettings;
};
inline InputMute* inputMute = nullptr;
}

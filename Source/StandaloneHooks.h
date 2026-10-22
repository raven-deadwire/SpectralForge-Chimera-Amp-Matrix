#pragma once

namespace spectralforge::standalone {
// Installed only by the Standalone application target (StandaloneAudioDefaults.cpp);
// null in VST3/AU and the test executables. Message thread only.
// Selects a low-latency ASIO driver on the very first Standalone run, before
// any audio device setup has been saved. Returns true when it changed the device.
inline bool (*preferLowLatencyDriverOnFirstRun)() = nullptr;
}

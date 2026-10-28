#pragma once
// Public, non-production fixtures only. None is in the shipped allowlist.
#include <vector>
#include <juce_core/juce_core.h>
namespace testercode {
inline constexpr const char* alphaCode = "CHT1-ALPHA-0A1B2C";
inline constexpr const char* betaCode = "CHT1-BETA-3D4E5F";
inline constexpr const char* gammaCode = "CHT1-GAMMA-6G7H10";
inline constexpr const char* shortestCode = "CHT1-AB-ABC123";
inline constexpr const char* longestCode = "CHT1-XXXXXXXXXXXXXXXX-XYZ789";
inline std::vector<juce::String> allowlist() { return {
    "730870ed9c7594197e1f6ca44adbf775363003edc2497522090abea68adaea04",
    "bdac63f09b225dfaee3b1a3d3ff595cfc2aa19fcfa706cab8de49a63d451e16b",
    "c92eff2a9b55347c695cde2236b2c99e7c111a9070c30284c9a98b2755828a65",
    "ede2e79b9640db1663f273bbf95e2ce6f2a7f57b3f266918a996a87ba86385f3",
    "6314dc2f4aeaf94af7514f54d362a81f4329d11f089e7a25855b1ac078ddeffc",
}; }
} // namespace testercode

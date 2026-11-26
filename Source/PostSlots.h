#pragma once
#include <juce_core/juce_core.h>

// POST second slots (docs/POST_SECOND_SLOTS.md). Every POST section (POST
// dynamics, POST preamp, POST EQ, modulation, delay, CREATIVE, reverb) has a
// second, fully independent instance. Slot 1 keeps every released parameter
// ID; slot 2 uses the same IDs with the "slot2_" prefix, appended at the end
// of the parameter list at version hint 35, after the Metro Clean TT-800
// bank (hint 34).
namespace spectralforge {
// Slots 3 and 4 (docs/POST_FOUR_SLOTS.md) follow the same scheme with the
// "slot3_" and "slot4_" prefixes, appended at version hint 37.
inline constexpr int postSlotCount = 4;
inline constexpr int postSecondSlotParameterVersion = 35;
inline constexpr int postExtraSlotParameterVersion = 37;
inline juce::String postSlotPrefix(int slot) { return slot <= 0 ? juce::String() : "slot" + juce::String(slot + 1) + "_"; }
// Component-ID suffix ("", "_2", "_3", "_4") and title suffix ("", " 2", ...).
inline juce::String postSlotSuffix(int slot) { return slot <= 0 ? juce::String() : "_" + juce::String(slot + 1); }
inline juce::String postSlotNumber(int slot) { return slot <= 0 ? juce::String() : " " + juce::String(slot + 1); }
// The parameter ID of `id` in POST slot `slot` (0 = slot 1, the released ID).
inline juce::String postSlotID(int slot, const juce::String& id) { return postSlotPrefix(slot) + id; }
// Host-visible names: slot 2 parameters read "... (slot 2)".
inline juce::String postSlotName(int slot, const juce::String& name) { return slot <= 0 ? name : name + " (slot " + juce::String(slot + 1) + ")"; }
} // namespace spectralforge

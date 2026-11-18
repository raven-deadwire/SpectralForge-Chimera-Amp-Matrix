#pragma once
#include <juce_core/juce_core.h>

// POST second slots (docs/POST_SECOND_SLOTS.md). Every POST section (POST
// dynamics, POST preamp, POST EQ, modulation, delay, CREATIVE, reverb) has a
// second, fully independent instance. Slot 1 keeps every released parameter
// ID; slot 2 uses the same IDs with the "slot2_" prefix, appended at the end
// of the parameter list at version hint 35 (33 was the highest released hint;
// 34 stays free for a parallel change).
namespace spectralforge {
inline constexpr int postSlotCount = 2;
inline constexpr int postSecondSlotParameterVersion = 35;
inline juce::String postSlotPrefix(int slot) { return slot <= 0 ? juce::String() : juce::String("slot2_"); }
// The parameter ID of `id` in POST slot `slot` (0 = slot 1, the released ID).
inline juce::String postSlotID(int slot, const juce::String& id) { return postSlotPrefix(slot) + id; }
// Host-visible names: slot 2 parameters read "... (slot 2)".
inline juce::String postSlotName(int slot, const juce::String& name) { return slot <= 0 ? name : name + " (slot 2)"; }
} // namespace spectralforge

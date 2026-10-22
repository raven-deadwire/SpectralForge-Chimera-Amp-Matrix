#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ui {
// One rule for every rotary control (Issue #113): drag up/right from the
// current value, never jump to the pointer, Shift for fine adjustment as on
// the amp heads, and no wheel edits so scrolling a panel cannot change sound.
// Text-box entry and parameter attachments are untouched.
constexpr int rotaryDragPixels=300;
inline juce::String knobHint() {return "Drag to adjust; Shift for fine adjustment; click the value to type it.";}
inline void configureRotaryKnob(juce::Slider& slider) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setSliderSnapsToMousePosition(false);
    slider.setMouseDragSensitivity(rotaryDragPixels);
    slider.setVelocityModeParameters(.2,1,0.,true,juce::ModifierKeys::shiftModifier);
    slider.setScrollWheelEnabled(false);
}
}

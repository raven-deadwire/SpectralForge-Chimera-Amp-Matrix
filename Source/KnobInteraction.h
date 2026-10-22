#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ui {
// One mouse/trackpad policy for every editor slider (Issue #113). The amp head
// knobs already used it; every other slider now follows the same rules.
// Presentation only: parameter IDs, ranges, attachments and gestures are untouched.
//  - Drag travel is measured in physical pixels, so 75% and 150% windows feel
//    like 100% (JUCE measures drag distance in the slider's transformed space).
//  - Shift gives fine adjustment, matching the amp head controls.
//  - A click on a linear track does not jump the value; dragging moves it.
//  - Mouse wheel/trackpad scrolling does not change a value while passing over
//    a control unless a panel explicitly opts in with the "wheelAllowed" property.
class KnobInteraction final : private juce::MouseListener {
public:
    static constexpr double physicalDragPixels=250.0; // JUCE default at 100%.

    // Message thread only. Each open editor holds one reference; the global
    // listener also covers dialogs and controls that panels create lazily.
    static void attach() {if(instance().editors++==0)juce::Desktop::getInstance().addGlobalMouseListener(&instance());}
    static void detach() {if(--instance().editors==0)juce::Desktop::getInstance().removeGlobalMouseListener(&instance());}

    // Configure every slider below root now. Safe to call repeatedly.
    static void apply(juce::Component& root) {
        if(auto* slider=dynamic_cast<juce::Slider*>(&root))instance().configure(*slider);
        for(auto* child:root.getChildren())apply(*child);
    }

    static int dragSensitivityFor(const juce::Component& slider) {
        const float scale=juce::Component::getApproximateScaleFactorForComponent(&slider);
        return juce::jmax(1,juce::roundToInt(physicalDragPixels/juce::jmax(.25f,scale)));
    }

private:
    int editors{};
    static KnobInteraction& instance() {static KnobInteraction policy;return policy;}

    static juce::Slider* sliderFor(juce::Component* component) {
        // The pointer may be over a slider's own value label.
        for(int depth=0;component!=nullptr && depth<2;++depth,component=component->getParentComponent())
            if(auto* slider=dynamic_cast<juce::Slider*>(component))return slider;
        return nullptr;
    }
    void configure(juce::Slider& slider) {
        auto& properties=slider.getProperties();
        if(!(bool)properties["knobPolicy"]) {
            properties.set("knobPolicy",true);
            // Amp head controls own an identical, already-released configuration.
            if(!(bool)properties["headControl"]) {
                slider.setVelocityModeParameters(.2,1,0.,true,juce::ModifierKeys::shiftModifier);
                if(!(bool)properties["wheelAllowed"])slider.setScrollWheelEnabled(false);
            }
            slider.setSliderSnapsToMousePosition(false);
        }
        slider.setMouseDragSensitivity(dragSensitivityFor(slider));
    }
    // A pointer always hovers before it presses, so a slider is configured
    // before its first click. Mouse-down refreshes the scale-dependent travel;
    // drag deltas are read only after it.
    void mouseEnter(const juce::MouseEvent& event) override {if(auto* slider=sliderFor(event.eventComponent))configure(*slider);}
    void mouseDown(const juce::MouseEvent& event) override {if(auto* slider=sliderFor(event.eventComponent))configure(*slider);}
};
}

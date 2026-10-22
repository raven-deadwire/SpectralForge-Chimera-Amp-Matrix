#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ui {
// Editor geometry shared by the scale menu, the host editor and the
// Standalone window (Issue #113). The canvas keeps its fixed aspect ratio;
// its scrollable window is independently bounded by the available work area.
constexpr int editorBaseWidth=1180,editorBaseHeight=780;
constexpr float minimumEditorScale=.75f,maximumEditorScale=1.5f;

// Largest supported scale whose editor plus window chrome fits `area`.
// It never goes below 75%: smaller text is not an acceptable fit.
inline float fittedEditorScale(juce::Rectangle<int> area,juce::Point<int> chrome) {
    const float width=float(area.getWidth()-chrome.x)/float(editorBaseWidth);
    const float height=float(area.getHeight()-chrome.y)/float(editorBaseHeight);
    return juce::jlimit(minimumEditorScale,maximumEditorScale,juce::jmin(width,height));
}
inline juce::Point<int> editorSizeForScale(float scale) {
    return {juce::roundToInt(float(editorBaseWidth)*scale),juce::roundToInt(float(editorBaseHeight)*scale)};
}
inline juce::Point<int> boundedEditorSize(juce::Point<int> requested,juce::Rectangle<int> area,juce::Point<int> chrome) {
    return {juce::jmax(1,juce::jmin(requested.x,area.getWidth()-juce::jmax(0,chrome.x)-8)),
            juce::jmax(1,juce::jmin(requested.y,area.getHeight()-juce::jmax(0,chrome.y)-8))};
}
inline juce::Point<int> fittedEditorSize(juce::Rectangle<int> area,juce::Point<int> chrome) {
    return boundedEditorSize(editorSizeForScale(fittedEditorScale(area.reduced(4),chrome)),area,chrome);
}
inline juce::Point<int> editorWindowChrome(juce::Component& editor) {
    auto* top=editor.getTopLevelComponent();
    juce::Point<int> chrome{juce::jmax(0,top->getWidth()-editor.getWidth()),juce::jmax(0,top->getHeight()-editor.getHeight())};
    if(auto* peer=top->getPeer())if(const auto frame=peer->getFrameSizeIfPresent()) {
        chrome+=juce::Point<int>{frame->getLeftAndRight(),frame->getTopAndBottom()};
        return chrome;
    }
    return chrome+juce::Point<int>{16,48};
}
// Centre `window` in `area`; if it is larger, keep its top-left (title bar
// and menu) on screen rather than centring it off the display.
inline juce::Point<int> centredWindowPosition(juce::Rectangle<int> area,juce::Point<int> windowSize) {
    return {juce::jmax(area.getX(),area.getCentreX()-windowSize.x/2),juce::jmax(area.getY(),area.getCentreY()-windowSize.y/2)};
}
}

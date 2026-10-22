#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ui {
// Editor geometry shared by the scale menu, the host editor and the
// Standalone window (Issue #113). The canvas keeps its fixed aspect ratio.
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
// Centre `window` in `area`; if it is larger, keep its top-left (title bar
// and menu) on screen rather than centring it off the display.
inline juce::Point<int> centredWindowPosition(juce::Rectangle<int> area,juce::Point<int> windowSize) {
    return {juce::jmax(area.getX(),area.getCentreX()-windowSize.x/2),juce::jmax(area.getY(),area.getCentreY()-windowSize.y/2)};
}
}

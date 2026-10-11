#pragma once
#include <juce_graphics/juce_graphics.h>

namespace spectralforge::cabHornArt {
// Presentation only: compact original waveguide, not a measured APT-150.
// Reference construction: Eminence APT-150 ABS flange/rectangular flare and
// round throat (https://eminence.com/products/apt_150), behind the continuous
// metal grille as on Eden D410XST (https://www.edenamps.com/product/d410xst/).
// The original cabinet crop supplies the flange and its four mounting bolts.
inline void mouth(juce::Graphics& g,juce::Rectangle<float> plate) {
    const juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(plate.getWidth()/128.f,plate.getHeight()/88.f)
        .translated(plate.getX(),plate.getY()));
    const juce::Rectangle<float> opening(10.f,12.f,108.f,64.f);
    juce::Path aperture;aperture.addRoundedRectangle(opening,5.f);
    g.reduceClipRegion(aperture);
    g.setColour(juce::Colour(0xff101214));g.fillPath(aperture);
    // Curved flare walls replace the sharply faceted decorative pyramid.
    // Low-contrast ABS highlights belong to the walls, not a bright centre
    // dome: the compression driver sits behind the narrow throat.
    const auto wall=[&](juce::Path path,juce::Colour colour,juce::Point<float> light) {
        g.setGradientFill({colour,light,juce::Colour(0xff080a0c),{64.f,44.f},false});g.fillPath(path);
    };
    juce::Path top;top.startNewSubPath(10.f,12.f);top.lineTo(118.f,12.f);
    top.cubicTo(101.f,25.f,85.f,30.f,73.f,33.f);top.quadraticTo(64.f,29.f,55.f,33.f);
    top.cubicTo(42.f,30.f,25.f,25.f,10.f,12.f);top.closeSubPath();
    wall(top,juce::Colour(0xff373b3e),{64.f,12.f});
    juce::Path bottom;bottom.startNewSubPath(10.f,76.f);bottom.lineTo(118.f,76.f);
    bottom.cubicTo(101.f,63.f,85.f,58.f,73.f,55.f);bottom.quadraticTo(64.f,59.f,55.f,55.f);
    bottom.cubicTo(42.f,58.f,25.f,63.f,10.f,76.f);bottom.closeSubPath();
    wall(bottom,juce::Colour(0xff26292c),{64.f,76.f});
    juce::Path left;left.startNewSubPath(10.f,12.f);left.lineTo(10.f,76.f);
    left.cubicTo(25.f,63.f,42.f,58.f,55.f,55.f);left.quadraticTo(51.f,44.f,55.f,33.f);
    left.cubicTo(42.f,30.f,25.f,25.f,10.f,12.f);left.closeSubPath();
    wall(left,juce::Colour(0xff424649),{10.f,36.f});
    juce::Path right;right.startNewSubPath(118.f,12.f);right.lineTo(118.f,76.f);
    right.cubicTo(101.f,63.f,85.f,58.f,73.f,55.f);right.quadraticTo(77.f,44.f,73.f,33.f);
    right.cubicTo(85.f,30.f,101.f,25.f,118.f,12.f);right.closeSubPath();
    wall(right,juce::Colour(0xff2b2f32),{118.f,36.f});
    // A shallow rim rolls into a recessed circular throat, never a dust cap.
    g.setGradientFill({juce::Colour(0xff34393b),53.f,32.f,juce::Colour(0xff07090b),75.f,56.f,false});
    g.fillEllipse(51.f,31.f,26.f,26.f);
    g.setColour(juce::Colour(0xff050709));g.fillEllipse(55.f,35.f,18.f,18.f);
    g.setColour(juce::Colour(0xff1b2022));g.drawEllipse(55.f,35.f,18.f,18.f,.65f);
    g.setColour(juce::Colour(0xff353a3d));g.strokePath(aperture,juce::PathStrokeType(1.3f));
}
}

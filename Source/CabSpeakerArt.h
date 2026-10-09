#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "CabExpansionModel.h"
#include <array>
#include <cmath>

namespace spectralforge::cabSpeakerArt {
// The assembly is a true front elevation. Its outer diameter is set by the
// caller in physical scene units; none of these styles changes the view angle.
inline constexpr float coneRadiusRatio = .87f;

namespace detail {
enum class Cap { paper, cloth, mesh, alloy, flat, recessed };
struct Style {
    const char* key;
    juce::uint32 frame, cone, cap;
    int ribs, rolls, bolts, facets;
    float capRadius, surroundWidth, ribWeight;
    Cap capType;
    bool woven, polished;
};
// Original front-face designs, in the same stable order as cabExpansion::drivers.
// Cone ribs, surround construction, cap profile and mounting pattern distinguish
// the models even in monochrome; colour is only an additional material cue.
inline constexpr std::array<Style, 14> styles {{
    {"ember-30-front",    0xff494039,0xff49423a,0xff30302d, 4,2,4, 0,.285f,.100f,.010f,Cap::paper,   false,false},
    {"steel-75-front",    0xff515963,0xff383c42,0xff34383c, 7,1,8, 0,.330f,.078f,.006f,Cap::cloth,   true, false},
    {"verdant-25-front",  0xff424c3b,0xff4b4d40,0xff38382e, 3,3,4, 0,.255f,.125f,.014f,Cap::paper,   false,false},
    {"granite-55-front",  0xff555452,0xff48494a,0xff292b2c, 6,2,8, 0,.285f,.096f,.011f,Cap::cloth,   false,false},
    {"silver-12-front",   0xff818990,0xff79786e,0xffb0b6b8, 2,2,6, 0,.295f,.083f,.004f,Cap::alloy,   false,true },
    {"carnivore-12-front",0xff3c403a,0xff333832,0xff232923,10,1,8, 0,.395f,.108f,.007f,Cap::mesh,    false,false},
    {"raven-100-front",   0xff424a51,0xff3c4146,0xff2e343a, 4,2,8,16,.355f,.086f,.015f,Cap::flat,    false,false},
    {"ruin-12-front",     0xff514740,0xff423c37,0xff24282b, 2,3,6,12,.325f,.104f,.010f,Cap::recessed,false,false},
    {"foundry-10-front",  0xff515149,0xff4b4a3f,0xff3a3a31, 3,3,4, 0,.410f,.122f,.009f,Cap::cloth,   false,false},
    {"vector-10-front",   0xff4c5a61,0xff3f4950,0xff344047, 1,1,6, 8,.250f,.114f,.010f,Cap::flat,    true, false},
    {"clarity-12-front",  0xff565d61,0xff4d5457,0xff323a3d, 0,2,8, 0,.275f,.098f,.006f,Cap::paper,   true, false},
    {"alloy-10-front",    0xff79848d,0xffadb3b6,0xff41474b, 5,1,8, 0,.310f,.126f,.004f,Cap::alloy,   false,true },
    {"monolith-15-front", 0xff55514b,0xff48473f,0xff35352f, 2,1,8, 0,.380f,.159f,.018f,Cap::paper,   false,false},
    {"depth-12-front",    0xff445661,0xff374951,0xff27393f, 3,2,6,18,.335f,.132f,.009f,Cap::recessed,true, false}
}};
inline constexpr std::array<Style, 2> legacyStyles {{
    {"legacy-guitar-front",0xff514c44,0xff484740,0xff33342f,4,2,4,0,.300f,.105f,.010f,Cap::paper,false,false},
    {"legacy-bass-front",  0xff505750,0xff464b42,0xff30372e,3,2,8,0,.370f,.125f,.009f,Cap::cloth,false,false}
}};
static_assert(styles.size() == cabExpansion::drivers.size(), "Every speaker needs its own front-face design");

inline const Style& style(int driver, int legacyDesign) {
    return driver > 0 && driver <= int(styles.size()) ? styles[size_t(driver - 1)]
        : legacyStyles[size_t(legacyDesign == 1)];
}
inline juce::Rectangle<float> circle(float radius) {
    return {-radius, -radius, radius * 2.f, radius * 2.f};
}
inline juce::Point<float> polar(float radius, float angle) {
    return {radius * std::cos(angle), radius * std::sin(angle)};
}
inline void ring(juce::Graphics& g, float radius, juce::Colour colour, float width) {
    g.setColour(colour);
    g.drawEllipse(circle(radius), width);
}
inline void radialFill(juce::Graphics& g, float radius, juce::Colour inner,
                       juce::Colour middle, juce::Colour edge, float middleAt = .65f) {
    juce::ColourGradient gradient(inner, 0.f, 0.f, edge, radius, 0.f, true);
    gradient.addColour(middleAt, middle);
    g.setGradientFill(gradient);
    g.fillEllipse(circle(radius));
}
inline void texture(juce::Graphics& g, float radius, bool woven, bool polished) {
    const juce::Graphics::ScopedSaveState saved(g);
    juce::Path clip;
    clip.addEllipse(circle(radius));
    g.reduceClipRegion(clip);
    if (woven) {
        // A quiet fabric weave, rather than a repeated logo or a colour overlay.
        g.setColour(juce::Colours::white.withAlpha(.055f));
        for (int i = -27; i <= 27; ++i) {
            const float offset = float(i) * .036f;
            g.drawLine(-radius, offset, radius, offset, .0045f);
            g.drawLine(offset, -radius, offset, radius, .0045f);
        }
    } else if (polished) {
        // Fine concentric tooling marks on the exposed aluminium diaphragm.
        for (int i = 8; i < 56; ++i)
            ring(g, radius * float(i) / 56.f,
                 juce::Colours::white.withAlpha(i % 3 == 0 ? .085f : .028f), .003f);
    } else {
        // Deterministic short paper fibres stay radial and cannot distort the
        // circular silhouette, unlike the previous side-on raster artwork.
        for (int i = 0; i < 92; ++i) {
            const float angle = float(i) * 2.39996323f;
            const float r = radius * (.29f + .66f * float((i * 37) % 91) / 91.f);
            const auto p = polar(r, angle), q = polar(r + .025f, angle + .016f);
            g.setColour((i % 3 ? juce::Colours::white : juce::Colours::black).withAlpha(.065f));
            g.drawLine(p.x, p.y, q.x, q.y, .0038f);
        }
    }
}
inline void cap(juce::Graphics& g, const Style& s) {
    const float r = s.capRadius;
    const auto material = juce::Colour(s.cap);
    ring(g, r + .012f, juce::Colours::black.withAlpha(.72f), .022f);
    if (s.capType == Cap::flat || s.capType == Cap::recessed) {
        radialFill(g, r, material.darker(.38f), material, material.brighter(.12f), .8f);
        ring(g, r - .016f, material.brighter(.25f), .012f);
        if (s.capType == Cap::recessed) {
            radialFill(g, r * .81f, material.darker(.25f), material.darker(.10f),
                       juce::Colours::black.withAlpha(.95f), .86f);
            ring(g, r * .84f, material.brighter(.22f), .016f);
        }
    } else {
        juce::ColourGradient dome(material.brighter(s.capType == Cap::alloy ? .75f : .25f),
                                  -r * .30f, -r * .36f, material.darker(.63f), r * .78f, r * .80f, true);
        dome.addColour(.52, material.brighter(s.capType == Cap::alloy ? .20f : .02f));
        dome.addColour(.87, material.darker(.30f));
        g.setGradientFill(dome);
        g.fillEllipse(circle(r));
    }
    if (s.capType == Cap::cloth || s.capType == Cap::mesh || s.capType == Cap::recessed) {
        const juce::Graphics::ScopedSaveState saved(g);
        juce::Path clip;
        clip.addEllipse(circle(r * (s.capType == Cap::recessed ? .78f : .94f)));
        g.reduceClipRegion(clip);
        const float step = s.capType == Cap::mesh ? .035f : .028f;
        for (int row = -15; row <= 15; ++row)
            for (int column = -15; column <= 15; ++column) {
                const float x = float(column) * step + (row % 2 ? step * .5f : 0.f);
                const float y = float(row) * step;
                g.setColour(juce::Colours::black.withAlpha(s.capType == Cap::cloth ? .18f : .45f));
                g.fillEllipse(x, y, step * .43f, step * .43f);
                if (s.capType == Cap::mesh) {
                    g.setColour(juce::Colours::white.withAlpha(.13f));
                    g.drawLine(x, y + step * .43f, x + step * .40f, y + step * .43f, .004f);
                }
            }
    } else if (s.capType == Cap::alloy) {
        for (int i = 3; i <= 17; ++i)
            ring(g, r * float(i) / 19.f, juce::Colours::white.withAlpha(.07f), .0028f);
    } else if (s.capType == Cap::flat) {
        texture(g, r * .93f, true, false);
        ring(g, r * .82f, material.brighter(.16f), .007f);
    } else {
        texture(g, r * .91f, false, false);
    }
    ring(g, r, material.brighter(.16f).withAlpha(.8f), .007f);
}
} // namespace detail

inline const char* styleKey(int driver, int legacyDesign = 0) {
    return detail::style(driver, legacyDesign).key;
}

inline void paint(juce::Graphics& g, juce::Rectangle<float> area, int driver,
                  int legacyDesign = 0, float coneRatio = coneRadiusRatio) {
    if (area.isEmpty()) return;
    const juce::Graphics::ScopedSaveState saved(g);
    const auto& s = detail::style(driver, legacyDesign);
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * .5f;
    g.addTransform(juce::AffineTransform::scale(radius).translated(area.getCentreX(), area.getCentreY()));
    const auto frame = juce::Colour(s.frame), cone = juce::Colour(s.cone);
    const float diaphragm = juce::jlimit(.78f, .90f, coneRatio);
    const float coneEdge = diaphragm - s.surroundWidth;

    // A flush, circular mounting flange. No rear basket, side wall, skew or
    // elliptical projection is present in this detail view.
    detail::radialFill(g, 1.f, juce::Colour(0xff171a1b), frame, juce::Colour(0xff090b0c), .955f);
    detail::ring(g, .985f, frame.brighter(.24f), .012f);
    detail::ring(g, .958f, juce::Colours::black.withAlpha(.68f), .015f);
    detail::ring(g, (diaphragm + .946f) * .5f, frame.darker(.16f), .025f);

    // The surround and cone remain concentric, sharing the DSP pickup centre.
    detail::radialFill(g, diaphragm, juce::Colour(0xff141617), juce::Colour(0xff373b3d),
                       juce::Colour(0xff111315), coneEdge / diaphragm);
    const float rollWidth = s.surroundWidth / float(s.rolls);
    for (int roll = 0; roll < s.rolls; ++roll) {
        const float mid = coneEdge + rollWidth * (float(roll) + .48f);
        detail::ring(g, mid, juce::Colour(0xff45484a), rollWidth * .26f);
        detail::ring(g, mid + rollWidth * .25f, juce::Colours::black.withAlpha(.56f), rollWidth * .18f);
        detail::ring(g, mid - rollWidth * .19f, juce::Colours::white.withAlpha(.085f), .006f);
    }
    detail::ring(g, diaphragm, juce::Colour(0xff080a0b), .013f);

    juce::ColourGradient coneGradient(cone.darker(.52f), 0.f, 0.f,
                                     cone.darker(.36f), coneEdge, 0.f, true);
    coneGradient.addColour(.44, cone.darker(.22f));
    coneGradient.addColour(.68, cone.brighter(s.polished ? .34f : .06f));
    coneGradient.addColour(.88, cone.brighter(s.polished ? .05f : .14f));
    g.setGradientFill(coneGradient);
    g.fillEllipse(detail::circle(coneEdge));

    if (s.facets > 0) {
        for (int facet = 0; facet < s.facets; ++facet) {
            const float angle = juce::MathConstants<float>::twoPi * float(facet) / float(s.facets);
            const auto a = detail::polar(s.capRadius * .96f, angle - .021f);
            const auto b = detail::polar(coneEdge * .97f, angle - .010f);
            const auto c = detail::polar(coneEdge * .97f, angle + .010f);
            const auto d = detail::polar(s.capRadius * .96f, angle + .021f);
            juce::Path crease;
            crease.startNewSubPath(a); crease.lineTo(b); crease.lineTo(c); crease.lineTo(d); crease.closeSubPath();
            g.setColour(juce::Colours::black.withAlpha(.22f)); g.fillPath(crease);
            g.setColour(juce::Colours::white.withAlpha(.11f));
            g.drawLine(d.x, d.y, c.x, c.y, .0045f);
        }
    }
    detail::texture(g, coneEdge * .98f, s.woven, s.polished);
    for (int rib = 0; rib < s.ribs; ++rib) {
        const float start = s.capRadius + .048f;
        const float r = start + (coneEdge - start - .036f) * float(rib + 1) / float(s.ribs + 1);
        detail::ring(g, r + s.ribWeight * .58f, juce::Colours::black.withAlpha(.40f), s.ribWeight);
        detail::ring(g, r - s.ribWeight * .50f, cone.brighter(.26f).withAlpha(.66f), s.ribWeight * .68f);
    }
    detail::ring(g, coneEdge, juce::Colours::black.withAlpha(.65f), .012f);
    detail::cap(g, s);

    // Fasteners live entirely inside the flange, with stable mounting centres.
    // Four, six and eight-bolt patterns also identify the front-face designs.
    const float boltRadius = (diaphragm + .97f) * .5f;
    for (int bolt = 0; bolt < s.bolts; ++bolt) {
        const float angle = juce::MathConstants<float>::twoPi * (float(bolt) + .5f) / float(s.bolts);
        const auto p = detail::polar(boltRadius, angle);
        const float seat = juce::jmin(.039f, (.985f - diaphragm) * .38f);
        g.setColour(frame.brighter(.12f));
        g.fillEllipse(p.x - seat, p.y - seat, seat * 2.f, seat * 2.f);
        g.setColour(juce::Colour(0xff101314));
        g.fillEllipse(p.x - seat * .76f, p.y - seat * .76f, seat * 1.52f, seat * 1.52f);
        g.setColour(juce::Colour(s.polished ? 0xffc8cdd0 : 0xff919794));
        g.drawEllipse(p.x - seat * .55f, p.y - seat * .55f, seat * 1.1f, seat * 1.1f, .006f);
        g.setColour(juce::Colour(0xff565b5e));
        g.drawLine(p.x - seat * .33f, p.y, p.x + seat * .33f, p.y, .008f);
        g.drawLine(p.x, p.y - seat * .33f, p.x, p.y + seat * .33f, .008f);
    }
}
} // namespace spectralforge::cabSpeakerArt

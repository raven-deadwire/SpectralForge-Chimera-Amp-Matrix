#pragma once
#include "PluginProcessor.h"
#include "SignalOrder.h"
#include <functional>

// POST rack page drag-to-reorder (docs/SIGNAL_ORDER_1_4.md, "Drag on the POST
// page"). The rack rows follow the POST order of their slot 1 blocks; CREATIVE
// keeps its strip above the rows. Each row (and the strip) shows slot 1 or
// slot 2 of its section (docs/POST_SECOND_SLOTS.md); dragging moves the block
// that is shown, and dropping takes the place of the block shown there. Each row's left strip (and the CREATIVE strip's left edge) is a
// grip. Dragging a grip more than 6 pixels highlights the block under the
// pointer; releasing there is one placement command (the ROUTING graph's
// command, so one undo step and the same 5 ms order crossfade). Hovering writes
// nothing. Escape, releasing over the own block or outside every block cancels.
namespace spectralforge::postRack {
inline constexpr int rowCount = 6;
// Rack row r shows this POST module.
inline constexpr std::array<int, rowCount> rowModules{postDynamicsModule, postPreampModule, postEQModule, postModulationModule, postDelayModule, postReverbModule};
inline int rowOfModule(int module) noexcept { for (int r = 0; r < rowCount; ++r) if (rowModules[size_t(r)] == module) return r; return -1; }
// Display slot (0-5) of each row: the rows keep the POST order among themselves.
inline std::array<int, rowCount> rowSlots(const PostOrder& order) noexcept {
    std::array<int, rowCount> slots{};
    for (int r = 0; r < rowCount; ++r) {
        const int p = order.position(rowModules[size_t(r)]);int s = 0;
        for (int k = 0; k < rowCount; ++k) if (order.position(rowModules[size_t(k)]) < p) ++s;
        slots[size_t(r)] = s;
    }
    return slots;
}
inline juce::String blockCaption(int block) { return moduleName(OrderRegion::post, juce::jlimit(0, postBlockCount - 1, block)).toUpperCase(); }
// "AFTER DELAY / BEFORE REVERB" for the released order (CREATIVE in `slot`).
inline juce::String creativePlacement(const PostOrder& order, int slot = 0) {
    const int p = order.position(postBlock(postCreativeModule, slot));
    if (p <= 0) return "FIRST IN POST / BEFORE " + blockCaption(order.slots[1]);
    if (p >= postBlockCount - 1) return "AFTER " + blockCaption(order.slots[size_t(p - 1)]) + " / LAST IN POST";
    return "AFTER " + blockCaption(order.slots[size_t(p - 1)]) + " / BEFORE " + blockCaption(order.slots[size_t(p + 1)]);
}

// A row's drag handle: grip dots, hand cursor, Escape cancels.
class Grip final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit Grip(int moduleIndex) : module(moduleIndex) {
        setComponentID("postGrip" + juce::String(module));setWantsKeyboardFocus(true);setMouseClickGrabsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        setTooltip("Drag to move " + blockCaption(module) + " in the POST order");
        setTitle(blockCaption(module) + " drag handle");
    }
    const int module;
    juce::Point<int> dots{14, 40};   // where the dots sit, in local coordinates
    std::function<void(const juce::MouseEvent&)> down, drag, up;
    std::function<bool()> escape;
    void mouseDown(const juce::MouseEvent& e) override { if (!e.mods.isPopupMenu() && down) down(e); }
    void mouseDrag(const juce::MouseEvent& e) override { if (drag) drag(e); }
    void mouseUp(const juce::MouseEvent& e) override { if (up) up(e); }
    bool keyPressed(const juce::KeyPress& key) override { return key == juce::KeyPress::escapeKey && escape && escape(); }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xff9b968c).withAlpha(isMouseOverOrDragging() ? 1.f : .7f));
        for (int row = 0; row < 3; ++row) for (int column = 0; column < 2; ++column)
            g.fillEllipse(float(dots.x + column * 5), float(dots.y + row * 5), 2.5f, 2.5f);
    }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
};

// Owns the grips and paints the drop feedback above the rack. Coordinates are
// the parent's (the editor canvas). Only the grips take clicks.
class Overlay final : public juce::Component {
public:
    explicit Overlay(ChimeraProcessor& owner) : processor(owner) {
        setComponentID("postRackDrag");setInterceptsMouseClicks(false, true);
        for (int m = 0; m < postBlockCount; ++m) {
            auto& grip = *grips.emplace_back(std::make_unique<Grip>(m));addChildComponent(grip);
            grip.down = [this, m](const juce::MouseEvent& e) { begin(m, e); };
            grip.drag = [this](const juce::MouseEvent& e) { move(e); };
            grip.up = [this](const juce::MouseEvent& e) { finish(e); };
            grip.escape = [this] { return cancel(); };
        }
    }
    // The block (whole row or strip) and its grip, in parent coordinates.
    void setBlock(int module, juce::Rectangle<int> block, juce::Rectangle<int> grip, juce::Point<int> dots = {14, 40}) {
        blocks[size_t(module)] = block;
        auto& g = *grips[size_t(module)];g.setBounds(grip - getPosition());g.dots = dots;g.setVisible(!block.isEmpty());
    }
    Grip& grip(int module) noexcept { return *grips[size_t(module)]; }
    bool isDragging() const noexcept { return dragging; }
    int draggedModule() const noexcept { return dragging ? source : -1; }
    // The block under the pointer, or -1 (outside every block).
    int dropModule() const noexcept { return dragging ? target : -1; }
    // The POST position the drop would take (0-based), or -1.
    int dropPosition() const { return dragging && target >= 0 ? processor.requestedOrder().post.position(target) : -1; }
    juce::String caption() const {
        if (!dragging) return {};
        if (target < 0) return blockCaption(source) + " > RELEASE TO CANCEL";
        if (target == source) return blockCaption(source) + " > KEEP POSITION";
        return blockCaption(source) + " > POSITION " + juce::String(dropPosition() + 1);
    }
    juce::String lastMessage;   // "Moved ...", "Move cancelled."
    std::function<void()> changed;   // after a drop moved the order

    void paint(juce::Graphics& g) override {
        if (!dragging) return;
        const auto origin = getPosition();
        for (int m = 0; m < postBlockCount; ++m) {
            const auto box = (blocks[size_t(m)] - origin).toFloat().reduced(1.f);
            if (box.isEmpty()) continue;
            if (m == source) { g.setColour(juce::Colours::black.withAlpha(.35f));g.fillRoundedRectangle(box, 4); }
            g.setColour(juce::Colour(0xffc4a678).withAlpha(m == target ? .95f : .35f));
            g.drawRoundedRectangle(box, 4, m == target ? 2.f : 1.f);
            if (m == target && m != source) { g.setColour(juce::Colour(0xffc4a678).withAlpha(.12f));g.fillRoundedRectangle(box, 4); }
        }
        const auto text = caption();
        juce::Font font(juce::FontOptions(10.f, juce::Font::bold));
        const float width = juce::GlyphArrangement::getStringWidth(font, text) + 20.f;
        const auto local = point - origin;
        const juce::Rectangle<float> tag(juce::jlimit(0.f, juce::jmax(0.f, getWidth() - width), float(local.x) + 14.f),
                                         juce::jlimit(0.f, float(getHeight() - 22), float(local.y) - 26.f), width, 22.f);
        g.setColour(juce::Colour(0xff0d1010).withAlpha(.94f));g.fillRoundedRectangle(tag, 4);
        g.setColour(juce::Colour(0xffc4a678));g.drawRoundedRectangle(tag, 4, 1.f);
        g.setColour(juce::Colour(0xffe1dbce));g.setFont(font);g.drawText(text, tag, juce::Justification::centred, false);
    }

private:
    ChimeraProcessor& processor;
    std::vector<std::unique_ptr<Grip>> grips;
    std::array<juce::Rectangle<int>, postBlockCount> blocks{};
    bool pressed{}, dragging{};int source{-1}, target{-1};juce::Point<int> point;
    juce::Point<int> inParent(const juce::MouseEvent& e) const { return getParentComponent() != nullptr ? e.getEventRelativeTo(getParentComponent()).getPosition() : e.getEventRelativeTo(const_cast<Overlay*>(this)).getPosition() + getPosition(); }
    int blockAt(juce::Point<int> p) const noexcept { for (int m = 0; m < postBlockCount; ++m) if (blocks[size_t(m)].contains(p)) return m; return -1; }
    void begin(int module, const juce::MouseEvent&) { pressed = true;dragging = false;source = module;target = module; }
    void move(const juce::MouseEvent& e) {
        if (!pressed) return;   // released, or cancelled with Escape
        if (!dragging && e.getDistanceFromDragStart() < 6) return;
        dragging = true;point = inParent(e);target = blockAt(point);repaint();
    }
    void finish(const juce::MouseEvent& e) {
        const bool wasDragging = dragging && pressed;
        if (wasDragging) { point = inParent(e);target = blockAt(point); }
        const int from = source, to = target;
        pressed = dragging = false;source = target = -1;repaint();
        if (!wasDragging) return;   // a click: nothing moves
        if (to < 0 || to == from) { lastMessage = "Move cancelled.";return; }
        const int position = processor.requestedOrder().post.position(to);
        if (processor.placeModule(OrderRegion::post, from, position)) {
            lastMessage = "Moved " + blockCaption(from) + " to POST position " + juce::String(position + 1) + ".";
            if (changed) changed();
        }
    }
    bool cancel() {
        if (!pressed) return false;
        const bool was = dragging;pressed = dragging = false;source = target = -1;
        if (was) lastMessage = "Move cancelled.";
        repaint();return true;
    }
};
// The POST page geometry (editor canvas coordinates): rows of 64 pixels every
// 69 from y 330 in display-slot order, CREATIVE's strip at y 302.
inline int rowY(int slot) noexcept { return 330 + juce::jlimit(0, rowCount - 1, slot) * 69; }
inline juce::Rectangle<int> rowBounds(int slot) noexcept { return {20, rowY(slot), 1140, 64}; }
inline const juce::Rectangle<int> creativeStripBounds{310, 302, 850, 24};
// `shown`: the slot (0 or 1) each section's row or strip shows; the other
// slot's block has no grip and takes no drop on this page.
inline void layout(Overlay& overlay, const std::array<int, rowCount>& slots, const std::array<int, postModuleCount>& shown = {}) {
    overlay.setBounds(20, 298, 1140, 443);
    for (int row = 0; row < rowCount; ++row) {
        const int section = rowModules[size_t(row)];const auto box = rowBounds(slots[size_t(row)]);
        for (int slot = 0; slot < 2; ++slot) overlay.setBlock(postBlock(section, slot), slot == shown[size_t(section)] ? box : juce::Rectangle<int>(), box.withWidth(26));
    }
    for (int slot = 0; slot < 2; ++slot)
        overlay.setBlock(postBlock(postCreativeModule, slot), slot == shown[size_t(postCreativeModule)] ? creativeStripBounds.expanded(4, 3) : juce::Rectangle<int>(), creativeStripBounds.withWidth(14), {4, 7});
}
} // namespace spectralforge::postRack

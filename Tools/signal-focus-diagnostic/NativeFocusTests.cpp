#include <juce_gui_basics/juce_gui_basics.h>
#include "NativeMessageLoop.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const juce::String& message)
{
    if (!condition)
        throw std::runtime_error(message.toStdString());
}

juce::String currentFocusName()
{
    if (auto* current = juce::Component::getCurrentlyFocusedComponent())
        return current->getName();
    return "<none>";
}

void dispatch(int milliseconds)
{
    require(chimeraTest::dispatchFor(milliseconds), "Native focus message loop exited");
}

template<class Predicate>
void waitForSetup(Predicate predicate, const juce::String& description)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 2000.0;
    while (!predicate() && juce::Time::getMillisecondCounterHiRes() < deadline)
        dispatch(10);
    require(predicate(), "Native focus setup failed: " + description
                         + "; current=" + currentFocusName());
}

void requireFocus(juce::Component& expected, const juce::String& description)
{
    auto* peer = expected.getPeer();
    require(peer != nullptr && peer->isFocused() && expected.hasKeyboardFocus(true),
            description + "; expected=" + expected.getName()
                        + "; current=" + currentFocusName()
                        + "; native=" + juce::String(int(peer != nullptr && peer->isFocused())));
}

struct FocusSlider final : juce::Slider {
    int focusGains = 0;
    void focusGained(FocusChangeType cause) override
    {
        ++focusGains;
        juce::Slider::focusGained(cause);
    }
};

struct Panel final : juce::Component {
    FocusSlider old, frequency;
    Panel()
    {
        setName("owner-panel");
        old.setName("old-control");
        frequency.setName("frequency-control");
        for (auto* slider : {&old, &frequency}) {
            slider->setWantsKeyboardFocus(true);
            addAndMakeVisible(slider);
        }
        old.setBounds(10, 10, 250, 40);
        frequency.setBounds(10, 60, 250, 40);
        setSize(280, 130);
    }
};

struct Window final : juce::DocumentWindow {
    explicit Window(const char* name)
        : juce::DocumentWindow(name, juce::Colours::darkgrey,
                               juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
    }
    void closeButtonPressed() override {}
};

struct Fixture {
    Panel panel;
    juce::TextButton graphButton{"graph-button"};
    std::unique_ptr<Window> owner;
    Window graph{"graph"};
    juce::Component* top = nullptr;
    const juce::String label;

    explicit Fixture(bool temporary) : label(temporary ? "temporary owner" : "managed owner")
    {
        if (temporary) {
            panel.addToDesktop(juce::ComponentPeer::windowIsTemporary);
            panel.setVisible(true);
        } else {
            owner = std::make_unique<Window>("owner");
            owner->setContentNonOwned(&panel, true);
            owner->setVisible(true);
        }
        top = panel.getTopLevelComponent();
        top->setTopLeftPosition(60, 60);
        graphButton.setName("graph-button");
        graphButton.setSize(220, 100);
        graph.setContentNonOwned(&graphButton, true);
        graph.setTopLeftPosition(450, 60);
        graph.setVisible(true);
        dispatch(100);
        require(top->getPeer() != nullptr && graph.getPeer() != nullptr
                    && panel.isShowing() && graphButton.isShowing(),
                label + ": native windows were not created and shown");
        activateForSetup(*top);
    }

    ~Fixture()
    {
        graph.clearContentComponent();
        if (owner)
            owner->clearContentComponent();
        else
            panel.removeFromDesktop();
    }

    void activateForSetup(juce::Component& window)
    {
        auto* peer = window.getPeer();
        require(peer != nullptr, label + ": missing native peer during setup");
        // One native request. Poll only readiness, never repeat a focus operation.
        // Avoid queuing an additional window-manager activation request here.
        peer->grabFocus();
        waitForSetup([&] { return peer->isFocused() && window.hasKeyboardFocus(true); },
                     label + ": activate " + window.getName());
        dispatch(30);
        requireFocus(window, label + ": native activation did not remain settled");
    }

    void selectOldForSetup()
    {
        activateForSetup(*top);
        panel.old.grabKeyboardFocus();
        requireFocus(panel.old, label + ": could not seed the old focus target");
        dispatch(30);
        requireFocus(panel.old, label + ": old focus target was not settled");
    }

    void cacheOldAndActivateGraph()
    {
        selectOldForSetup();
        top->getPeer()->handleFocusLoss();
        activateForSetup(graph);
        require(top->getPeer()->getLastFocusedSubcomponent() == &panel.old,
                label + ": old focus target was not cached");
    }
};

void controlledLateFocus(bool temporary)
{
    Fixture fixture(temporary);
    auto& panel = fixture.panel;
    auto* ownerPeer = fixture.top->getPeer();
    // Control the cached target and deliver the delayed native notification
    // explicitly. The requested child still uses the real public focus API.
    fixture.cacheOldAndActivateGraph();
    panel.frequency.grabKeyboardFocus();
    requireFocus(panel.frequency, fixture.label + ": immediate cross-peer focus failed");
    const auto gainsBeforeNotification = panel.frequency.focusGains;
    ownerPeer->handleFocusGain();
    requireFocus(panel.frequency, fixture.label + ": delayed FocusIn replaced the requested target");
    require(panel.frequency.focusGains > gainsBeforeNotification,
            fixture.label + ": native focus-gain notification was suppressed");
    dispatch(100);
    requireFocus(panel.frequency, fixture.label + ": requested focus did not persist");

    panel.old.grabKeyboardFocus();
    requireFocus(panel.old, fixture.label + ": superseding focus request failed");
    ownerPeer->handleFocusGain();
    requireFocus(panel.old, fixture.label + ": stale target replaced a superseding request");
    dispatch(100);
    requireFocus(panel.old, fixture.label + ": superseding focus did not persist");

    ownerPeer->handleFocusLoss();
    fixture.activateForSetup(fixture.graph);
    require(ownerPeer->getLastFocusedSubcomponent() == &panel.old,
            fixture.label + ": ordinary restoration lost its cached target");
    ownerPeer->grabFocus();
    require(ownerPeer->isFocused(), fixture.label + ": native reactivation failed");
    ownerPeer->handleFocusGain();
    requireFocus(panel.old, fixture.label + ": ordinary cached focus was not restored");
    dispatch(100);
    requireFocus(panel.old, fixture.label + ": restored focus did not persist");

    fixture.graphButton.grabKeyboardFocus();
    requireFocus(fixture.graphButton, fixture.label + ": focus could not leave the owner");
    dispatch(100);
    requireFocus(fixture.graphButton, fixture.label + ": owner stole focus back from the graph");
    std::cout << "PASS controlled late FocusIn, superseding request, restoration and transfer: "
              << fixture.label << std::endl;

    const auto invalidatedRequest = [&](juce::Component& target, const char* reason,
                                        auto invalidate, auto restore) {
        fixture.cacheOldAndActivateGraph();
        target.grabKeyboardFocus();
        requireFocus(target, fixture.label + ": invalidation setup could not focus target");
        invalidate();
        ownerPeer->handleFocusGain();
        requireFocus(panel.old, fixture.label + ": failed to restore after target became " + reason);
        dispatch(60);
        requireFocus(panel.old, fixture.label + ": invalidated target regained focus: " + reason);
        restore();
    };
    invalidatedRequest(panel.frequency, "hidden",
                       [&] { panel.frequency.setVisible(false); },
                       [&] { panel.frequency.setVisible(true); });
    invalidatedRequest(panel.frequency, "non-focusable",
                       [&] { panel.frequency.setWantsKeyboardFocus(false); },
                       [&] { panel.frequency.setWantsKeyboardFocus(true); });
    invalidatedRequest(panel.frequency, "disabled",
                       [&] { panel.frequency.setEnabled(false); },
                       [&] { panel.frequency.setEnabled(true); });
    auto removed = std::make_unique<FocusSlider>();
    removed->setName("deleted-control");
    removed->setWantsKeyboardFocus(true);
    removed->setBounds(10, 105, 250, 20);
    panel.addAndMakeVisible(*removed);
    invalidatedRequest(*removed, "deleted", [&] { removed.reset(); }, [] {});
    std::cout << "PASS hidden, non-focusable, disabled and deleted target fallback: "
              << fixture.label << std::endl;
}

void dispatchedNativeFocus()
{
    Fixture fixture(false);
    auto& panel = fixture.panel;
    fixture.selectOldForSetup();
    // This entire case uses actual native focus delivery, with no calls to
    // handleFocusGain/handleFocusLoss and no synthetic component pointer events.
    fixture.activateForSetup(fixture.graph);
    require(fixture.top->getPeer()->getLastFocusedSubcomponent() == &panel.old,
            "Native transfer setup did not cache the old owner control");
    panel.frequency.grabKeyboardFocus();
    requireFocus(panel.frequency, "Native transfer failed before event dispatch");
    const auto gainsAfterRequest = panel.frequency.focusGains;
    dispatch(150);
    requireFocus(panel.frequency, "Dispatched native FocusIn replaced the requested control");
    require(panel.frequency.focusGains > gainsAfterRequest,
            "Native transfer did not deliver the owner's focus-gain notification");
    std::cout << "PASS native cross-peer transfer through regular event dispatch: managed owner"
              << std::endl;
}
}

int main()
{
    try {
        juce::ScopedJuceInitialiser_GUI init;
        controlledLateFocus(false);
        dispatch(30);
        controlledLateFocus(true);
        dispatch(30);
        dispatchedNativeFocus();
        std::cout << "Native focus regressions PASS" << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native focus regression FAILED: " << error.what() << std::endl;
        return 1;
    }
}

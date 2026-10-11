#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <memory>
// Diagnostic only: observed failures are printed, never re-run or treated as
// release test evidence. The production Signal test remains unchanged.
static void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }
struct Panel : juce::Component {
    juce::Slider old, frequency;
    Panel() {
        old.setName("old"); frequency.setName("frequency");
        for (auto* s : {&old, &frequency}) { s->setWantsKeyboardFocus(true); addAndMakeVisible(s); }
        old.setBounds(10,10,250,40); frequency.setBounds(10,60,250,40); setSize(280,130);
    }
};
struct Window : juce::DocumentWindow {
    Window(const char* name) : juce::DocumentWindow(name,juce::Colours::darkgrey,juce::DocumentWindow::closeButton) {setUsingNativeTitleBar(true);}
    void closeButtonPressed() override {}
};
static juce::String focused() {auto* c=juce::Component::getCurrentlyFocusedComponent(); return c ? c->getName() : "null";}
static void pointerClick(juce::Button& button) {
    const auto centre=button.getLocalBounds().getCentre().toFloat();
    juce::Desktop::setMousePosition(button.localPointToGlobal(centre).roundToInt());pump(40);
    const auto time=juce::Time::getCurrentTime();
    const auto event=[&](bool down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),centre,juce::ModifierKeys(down?juce::ModifierKeys::leftButtonModifier:0),1,0,0,0,0,&button,&button,time,centre,time,1,false);};
    static_cast<juce::Component&>(button).mouseDown(event(true));
    static_cast<juce::Component&>(button).mouseUp(event(false));
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    std::cout<<"JUCE="<<juce::SystemStats::getJUCEVersion()<<" variant="<<FOCUS_DIAGNOSTIC_VARIANT<<std::endl;
    for (bool temporary : {true,false}) {
        Panel panel; std::unique_ptr<Window> owner;
        if(temporary) {panel.addToDesktop(juce::ComponentPeer::windowIsTemporary); panel.setVisible(true);}
        else {owner=std::make_unique<Window>("owner");owner->setContentNonOwned(&panel,true);owner->setVisible(true);}
        auto* top=panel.getTopLevelComponent(); top->setTopLeftPosition(60,60);
        Window graph("graph");juce::TextButton graphButton("graph-button");graphButton.setName("graph-button");graphButton.setSize(220,100);graph.setContentNonOwned(&graphButton,true);graph.setTopLeftPosition(450,60);graph.setVisible(true);pump(100);
        for(bool nativeFront : {false,true}) for(bool ownerFront : {false,true}) {
            int immediateFails=0, lateFails=0, clicks=0;
            for(int n=0;n<12;++n) {
                top->toFront(true);pump(30);panel.old.grabKeyboardFocus();pump(20);
                if(nativeFront) graph.getPeer()->toFront(true); else graph.toFront(true);
                juce::String immediate;
                graphButton.onClick=[&]{
                    ++clicks;
                    std::cout<<"REQUEST temporary="<<temporary<<" graph-peer-front="<<nativeFront<<" owner-front="<<ownerFront<<" n="<<n<<" current="<<focused()<<" native-owner="<<top->getPeer()->isFocused()<<" cached="<<top->getPeer()->getLastFocusedSubcomponent()->getName()<<"\n";
                    if(ownerFront)top->toFront(true);
                    panel.frequency.grabKeyboardFocus();
                    immediate=focused();
                    if(!panel.frequency.hasKeyboardFocus(true))++immediateFails;
                    std::cout<<"IMMEDIATE current="<<immediate<<" native-owner="<<top->getPeer()->isFocused()<<" cached="<<top->getPeer()->getLastFocusedSubcomponent()->getName()<<"\n";
                };
                pointerClick(graphButton);pump(80);
                if(!panel.frequency.hasKeyboardFocus(true))++lateFails;
                std::cout<<"SETTLED current="<<focused()<<" native-owner="<<top->getPeer()->isFocused()<<" cached="<<top->getPeer()->getLastFocusedSubcomponent()->getName()<<"\n";
            }
            std::cout<<"ACTUAL temporary="<<temporary<<" graph-peer-front="<<nativeFront<<" owner-front="<<ownerFront<<" immediate-fails="<<immediateFails<<" late-fails="<<lateFails<<" clicks="<<clicks<<" /12"<<std::endl;
            graphButton.onClick=nullptr;
        }
        top->toFront(true);pump(40);panel.old.grabKeyboardFocus();pump(30);
        top->getPeer()->handleFocusLoss();
        graph.toFront(true);pump(40);
        panel.frequency.grabKeyboardFocus();
        std::cout<<"DETERMINISTIC temporary="<<temporary<<" before-FocusIn="<<focused();
        top->getPeer()->handleFocusGain();
        std::cout<<" after-FocusIn="<<focused()<<std::endl;
        panel.old.grabKeyboardFocus();
        top->getPeer()->handleFocusGain();
        std::cout<<"SUPERSEDED temporary="<<temporary<<" expected=old actual="<<focused()<<std::endl;
        top->getPeer()->handleFocusLoss();
        graph.toFront(true);pump(40);
        top->getPeer()->grabFocus();
        top->getPeer()->handleFocusGain();
        std::cout<<"RESTORE temporary="<<temporary<<" expected=old actual="<<focused()<<std::endl;
        graph.clearContentComponent();if(owner)owner->clearContentComponent();else panel.removeFromDesktop();pump(30);
    }
}

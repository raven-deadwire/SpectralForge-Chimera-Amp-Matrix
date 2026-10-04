#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ui {
// Message-thread only. No parameter listeners or work added to processBlock.
template<class T> struct Changed {
    T value{};
    bool dirty{true};
    bool update(const T& next) {
        if(!dirty && value==next)return false;
        value=next;dirty=false;return true;
    }
};

inline bool visible(const juce::Component& component) {
    for(auto* c=&component;c;c=c->getParentComponent()) {
        // A peer-less root is also used by the offline snapshot tests. Honour
        // child visibility there; real peers additionally honour hide/minimise.
        if((c->getParentComponent() || c->getPeer()) && !c->isVisible())return false;
        if(auto* peer=c->getPeer();peer && peer->isMinimised())return false;
    }
    return true;
}
inline int tenth(float value) {return std::isfinite(value)?juce::roundToInt(value*10.f):0;}
}

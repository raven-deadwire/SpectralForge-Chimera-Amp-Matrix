#pragma once
#include <juce_events/juce_events.h>

namespace chimeraTest {
// Test-only event pump. Shipping plug-ins keep their host's normal event loop.
#if JUCE_MAC
bool dispatchFor(int milliseconds);
#else
inline bool dispatchFor(int milliseconds) {
    return juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
#endif
}

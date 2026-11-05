#pragma once
#include <juce_core/juce_core.h>

namespace spectralforge::irState {
inline constexpr size_t maxAssetBytes=4u*1024u*1024u;

// MemoryBlock allocates the size prefix before reading any payload. Bound it
// before calling JUCE: a short corrupt string can otherwise request gigabytes
// (or SIZE_MAX for a negative prefix). Keep JUCE's legacy extra-dot tolerance.
inline bool decodeAsset(const juce::String& encoded,juce::MemoryBlock& bytes) {
    if(encoded.length()>6*1024*1024)return false;
    auto p=encoded.getCharPointer();
    size_t size=0;
    bool digitSeen=false;
    for(;;) {
        const auto c=p.getAndAdvance();
        if(c=='.')break;
        if(c<'0' || c>'9')return false;
        const auto digit=size_t(c-'0');
        if(size>(maxAssetBytes-digit)/10)return false;
        size=size*10+digit;digitSeen=true;
    }
    return digitSeen && bytes.fromBase64Encoding(encoded);
}
}

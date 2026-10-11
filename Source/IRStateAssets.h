#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>

namespace spectralforge::irState {
inline constexpr size_t maxStateBytes=64u*1024u*1024u;
inline constexpr size_t maxAssetBytes=4u*1024u*1024u;
// Only USER_IRS payloads are pooled. Names, metadata and lane/slot bindings stay
// in their own snapshots; identity is the SHA-256 of the original file bytes.
inline bool pool(juce::ValueTree tree,juce::ValueTree assets) {
    if(tree.hasType("USER_IRS")) for(auto ir:tree) {
        juce::MemoryBlock bytes;
        const auto encoded=ir.getProperty("data").toString();
        if(encoded.length()>6*1024*1024 || !bytes.fromBase64Encoding(encoded)
            || bytes.getSize()==0 || bytes.getSize()>maxAssetBytes) return false;
        const auto hash=juce::SHA256(bytes).toHexString();
        auto asset=assets.getChildWithProperty("sha256",hash);
        if(!asset.isValid()) {
            asset=juce::ValueTree("ASSET");asset.setProperty("sha256",hash,nullptr);
            asset.setProperty("data",bytes.toBase64Encoding(),nullptr);assets.appendChild(asset,nullptr);
        }
        ir.removeProperty("data",nullptr);ir.setProperty("asset",hash,nullptr);
    }
    else for(auto child:tree) if(!pool(child,assets))return false;
    return true;
}
// Validate the complete table and every reference before callers mutate state.
// Old inline USER_IRS children remain supported through the existing decoder.
inline bool expand(juce::ValueTree tree,const juce::ValueTree& assets) {
    if(tree.hasType("USER_IRS")) for(auto ir:tree) if(ir.hasProperty("asset")) {
        if(ir.hasProperty("data"))return false;
        const auto hash=ir.getProperty("asset").toString();
        auto asset=assets.getChildWithProperty("sha256",hash);
        if(!asset.isValid())return false;
        ir.setProperty("data",asset.getProperty("data"),nullptr);ir.removeProperty("asset",nullptr);
    }
    if(!tree.hasType("USER_IRS"))for(auto child:tree)if(!expand(child,assets))return false;
    return true;
}
inline bool unpack(juce::ValueTree tree) {
    auto assets=tree.getChildWithName("IR_ASSETS");
    juce::StringArray hashes;
    for(auto asset:assets) {
        const auto hash=asset.getProperty("sha256").toString();
        const auto encoded=asset.getProperty("data").toString();juce::MemoryBlock bytes;
        if(!asset.hasType("ASSET") || hash.length()!=64 || hashes.contains(hash)
            || encoded.length()>6*1024*1024 || !bytes.fromBase64Encoding(encoded)
            || bytes.getSize()==0 || bytes.getSize()>maxAssetBytes
            || juce::SHA256(bytes).toHexString()!=hash)return false;
        hashes.add(hash);
    }
    tree.removeChild(assets,nullptr);
    return expand(tree,assets);
}
inline bool serialize(juce::ValueTree tree,juce::MemoryBlock& destination) {
    tree=tree.createCopy();tree.removeChild(tree.getChildWithName("IR_ASSETS"),nullptr);
    juce::ValueTree assets("IR_ASSETS");if(!pool(tree,assets))return false;
    tree.appendChild(assets,nullptr);tree.setProperty("schemaVersion",11,nullptr);
    auto xml=tree.createXml();juce::MemoryBlock candidate;
    juce::AudioProcessor::copyXmlToBinary(*xml,candidate);
    // Includes JUCE's binary header, UTF-8 XML and terminator, not an estimate.
    if(candidate.getSize()>maxStateBytes)return false;
    destination.swapWith(candidate);return true;
}
}

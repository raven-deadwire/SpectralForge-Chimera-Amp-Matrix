#pragma once
#include "OriginalAmpDefinition.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectralforge::original {
constexpr int contextCount=6;
struct Context { bool enabled=false; State state; };
using Banks=std::array<Context,contextCount>;
inline juce::String parameterPrefix(int context) { return "originalAmp_c"+juce::String(context)+"_nastrond_"; }
inline juce::String parameterID(int context,std::size_t control) { return parameterPrefix(context)+controls[control].id; }
// Integration must call this AFTER every released parameter, including Range.
// This development module is not yet invoked by the released processor layout.
inline void appendParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    for(int context=0;context<contextCount;++context) {
        const auto prefix=parameterPrefix(context);
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{prefix+"enabled",3},"Náströnd enabled "+juce::String(context),false));
        for(std::size_t i=0;i<controlCount;++i) {
            const auto& k=controls[i];
            auto range=juce::NormalisableRange<float>(k.minimum,k.maximum,.001f);
            if(i==std::size_t(Control::midFrequency))range.setSkewForCentre(850);
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{parameterID(context,i),3},"Náströnd "+juce::String(context)+" "+k.label,range,k.initial));
        }
    }
}
// Reusable non-RT state codec. Unknown future versions are rejected without
// partially changing the destination. A missing subtree is an old session.
inline juce::ValueTree save(const Banks& banks) {
    juce::ValueTree root("ORIGINAL_AMPS");root.setProperty("version",2,nullptr);
    for(int i=0;i<contextCount;++i) {
        juce::ValueTree node("CONTEXT");node.setProperty("index",i,nullptr);node.setProperty("model","original.nastrond.v1",nullptr);
        node.setProperty("enabled",banks[std::size_t(i)].enabled,nullptr);
        auto state=banks[std::size_t(i)].state;state.sanitise();
        node.setProperty("channel",state.channel,nullptr);node.setProperty("modern",state.modern,nullptr);
        for(std::size_t c=0;c<controlCount;++c)node.setProperty(controls[c].id,state.values[c],nullptr);
        root.appendChild(node,nullptr);
    }
    return root;
}
inline bool restore(const juce::ValueTree& root,Banks& destination) {
    if(!root.isValid()){destination={};return true;}
    if(!root.hasType("ORIGINAL_AMPS") || (int(root.getProperty("version",0))<1 || int(root.getProperty("version",0))>2) || root.getNumChildren()!=contextCount)return false;
    Banks next;std::array<bool,contextCount> seen{};
    for(const auto& node:root) {
        const int index=int(node.getProperty("index",-1));
        if(!node.hasType("CONTEXT") || index<0 || index>=contextCount || seen[std::size_t(index)] || node.getProperty("model").toString()!="original.nastrond.v1")return false;
        seen[std::size_t(index)]=true;auto& b=next[std::size_t(index)];
        b.enabled=bool(node.getProperty("enabled",false));
        b.state.channel=int(node.getProperty("channel",0));b.state.modern=bool(node.getProperty("modern",false));
        for(std::size_t c=0;c<controlCount;++c) {
            const auto value=node.getProperty(controls[c].id);
            if(!value.isDouble()&&!value.isInt()&&!value.isInt64())return false;
            b.state.values[c]=float(value);
        }
        b.state.sanitise();
    }
    destination=next;return true;
}
}

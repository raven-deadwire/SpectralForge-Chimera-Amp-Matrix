#pragma once
#include "OriginalCabModel.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace spectralforge {
constexpr int originalCabParameterCount=39;
inline juce::String originalCabID(int lane,const char* suffix) {return "ocab"+juce::String(lane+1)+"_"+suffix;}
inline void appendOriginalCabParameters(juce::AudioProcessorValueTreeState::ParameterLayout& p) {
    for(int lane=0;lane<3;++lane) {
        auto choice=[&](const char* id,const char* label,juce::StringArray names,int initial=0) {
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{originalCabID(lane,id),7},juce::String(label)+" "+juce::String(lane+1),names,initial));
        };
        choice("design","Original CAB design",{"Guitar 4x12 / v1","Bass 4x10 / v1"});
        choice("rear","Original CAB rear",{"Closed","Open"});
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{originalCabID(lane,"tweeter"),7},"Original tweeter "+juce::String(lane+1),juce::NormalisableRange<float>{0,1,.01f},0));
        for(const auto* slot:{"A","B"}) {
            const auto prefix=juce::String(slot);
            p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{originalCabID(lane,(prefix+"on").toRawUTF8()),7},"Modeled Mic "+prefix+" "+juce::String(lane+1),false));
            choice((prefix+"mic").toRawUTF8(),("Modeled Mic "+prefix).toRawUTF8(),{"Attack dynamic / v1","Body ribbon / v1","Detail condenser / v1"});
            choice((prefix+"unit").toRawUTF8(),("Speaker unit "+prefix).toRawUTF8(),{"1 / upper left","2 / upper right","3 / lower left","4 / lower right"});
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{originalCabID(lane,(prefix+"position").toRawUTF8()),7},"Position "+prefix+" "+juce::String(lane+1),juce::NormalisableRange<float>{0,1,.001f},.25f));
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{originalCabID(lane,(prefix+"distance").toRawUTF8()),7},"Distance cm "+prefix+" "+juce::String(lane+1),juce::NormalisableRange<float>{2,60,.1f},10));
        }
    }
}
struct OriginalCabParameters {
    std::array<std::array<std::atomic<float>*,13>,3> values{};
    void bind(juce::AudioProcessorValueTreeState& state) {
        constexpr std::array<const char*,13> names{"design","rear","tweeter","Aon","Amic","Aunit","Aposition","Adistance","Bon","Bmic","Bunit","Bposition","Bdistance"};
        for(int lane=0;lane<3;++lane)for(size_t n=0;n<names.size();++n)values[lane][n]=state.getRawParameterValue(originalCabID(lane,names[n]));
    }
    uint64_t read(int lane,int slot) const noexcept {
        const auto& v=values[lane];const int offset=slot ? 8 : 3;
        if(v[offset]->load()<.5f)return 0;
        return originalCab::key({true,int(v[0]->load()),int(v[1]->load()),int(v[offset+1]->load()),int(v[offset+2]->load()),
            v[2]->load(),v[offset+3]->load(),v[offset+4]->load()});
    }
};
}

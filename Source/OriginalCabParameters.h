#pragma once
#include "OriginalCabModel.h"
#include "CabLayoutModel.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace spectralforge {
constexpr int originalCabParameterCount=39;
constexpr int cabExpansionParameterCount=12;
constexpr int cabLayoutParameterCount=9;
inline juce::String cabLayoutID(int lane,const char* suffix) {return "lcab"+juce::String(lane+1)+"_"+suffix;}
inline juce::StringArray cabLayoutNames() {
    // Choice positions are part of saved projects. Describe their usable
    // layouts without exposing the implementation's parameter generations.
    juce::StringArray names{"Guitar 4x12 / Bass 4x10"};
    for(const auto& layout:cabLayout::layouts)names.add(layout.name);
    return names;
}
inline void appendCabLayoutParameters(juce::AudioProcessorValueTreeState::ParameterLayout& p) {
    for(int lane=0;lane<3;++lane) {
        const auto add=[&](const char* id,const char* name,const juce::StringArray& items) {
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{cabLayoutID(lane,id),9},juce::String(name)+" "+juce::String(lane+1),items,0));
        };
        add("layout","Cabinet layout",cabLayoutNames());
        add("Aunit","Cabinet unit A",{"Unit 1","Unit 2","Unit 3","Unit 4","Unit 5","Unit 6","Unit 7","Unit 8"});
        add("Bunit","Cabinet unit B",{"Unit 1","Unit 2","Unit 3","Unit 4","Unit 5","Unit 6","Unit 7","Unit 8"});
    }
}
inline juce::String originalCabID(int lane,const char* suffix) {return "ocab"+juce::String(lane+1)+"_"+suffix;}
inline juce::String cabExpansionID(int lane,const char* suffix) {return "xcab"+juce::String(lane+1)+"_"+suffix;}
inline juce::StringArray expandedDriverNames() {
    juce::StringArray names{"Chimera speaker"};
    for(const auto& d:cabExpansion::drivers)names.add(d.name);
    return names;
}
inline juce::StringArray expandedMicNames() {
    juce::StringArray names{"Chimera microphone"};
    for(const auto& m:cabExpansion::microphones)names.add(m.name);
    return names;
}
inline void appendCabExpansionParameters(juce::AudioProcessorValueTreeState::ParameterLayout& p) {
    for(int lane=0;lane<3;++lane) {
        const auto add=[&](const char* id,const char* label,const juce::StringArray& names) {
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{cabExpansionID(lane,id),8},
                juce::String(label)+" "+juce::String(lane+1),names,0));
        };
        add("driver","Speaker design",expandedDriverNames());
        add("Amic","Mic A design",expandedMicNames());
        add("Bmic","Mic B design",expandedMicNames());
        add("tweeter","Tweeter design",{"Chimera HF","Silk HF","Metal HF","Air HF"});
    }
}
inline void appendOriginalCabParameters(juce::AudioProcessorValueTreeState::ParameterLayout& p) {
    for(int lane=0;lane<3;++lane) {
        auto choice=[&](const char* id,const char* label,juce::StringArray names,int initial=0) {
            p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{originalCabID(lane,id),7},juce::String(label)+" "+juce::String(lane+1),names,initial));
        };
        choice("design","Chimera speaker family",{"Chimera Guitar 12","Chimera Bass 10"});
        choice("rear","Cabinet rear",{"Closed","Open"});
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{originalCabID(lane,"tweeter"),7},"Tweeter level "+juce::String(lane+1),juce::NormalisableRange<float>{0,1,.01f},0));
        for(const auto* slot:{"A","B"}) {
            const auto prefix=juce::String(slot);
            p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{originalCabID(lane,(prefix+"on").toRawUTF8()),7},"Modeled Mic "+prefix+" "+juce::String(lane+1),false));
            choice((prefix+"mic").toRawUTF8(),("Modeled Mic "+prefix).toRawUTF8(),{"Attack Dynamic","Body Ribbon","Detail Condenser"});
            choice((prefix+"unit").toRawUTF8(),("Speaker unit "+prefix).toRawUTF8(),{"1 / upper left","2 / upper right","3 / lower left","4 / lower right"});
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{originalCabID(lane,(prefix+"position").toRawUTF8()),7},"Position "+prefix+" "+juce::String(lane+1),juce::NormalisableRange<float>{0,1,.001f},.25f));
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{originalCabID(lane,(prefix+"distance").toRawUTF8()),7},"Distance cm "+prefix+" "+juce::String(lane+1),juce::NormalisableRange<float>{2,60,.1f},10));
        }
    }
}
struct OriginalCabParameters {
    std::array<std::array<std::atomic<float>*,13>,3> values{};
    std::array<std::array<std::atomic<float>*,4>,3> expansion{};
    std::array<std::array<std::atomic<float>*,3>,3> layouts{};
    void bind(juce::AudioProcessorValueTreeState& state) {
        constexpr std::array<const char*,13> names{"design","rear","tweeter","Aon","Amic","Aunit","Aposition","Adistance","Bon","Bmic","Bunit","Bposition","Bdistance"};
        for(int lane=0;lane<3;++lane)for(size_t n=0;n<names.size();++n)values[lane][n]=state.getRawParameterValue(originalCabID(lane,names[n]));
        constexpr std::array<const char*,4> extra{"driver","Amic","Bmic","tweeter"};
        for(int lane=0;lane<3;++lane)for(size_t n=0;n<extra.size();++n)expansion[lane][n]=state.getRawParameterValue(cabExpansionID(lane,extra[n]));
        for(int lane=0;lane<3;++lane) {
            layouts[lane][0]=state.getRawParameterValue(cabLayoutID(lane,"layout"));
            layouts[lane][1]=state.getRawParameterValue(cabLayoutID(lane,"Aunit"));
            layouts[lane][2]=state.getRawParameterValue(cabLayoutID(lane,"Bunit"));
        }
    }
    uint64_t read(int lane,int slot) const noexcept {
        const auto& v=values[lane];const int offset=slot ? 8 : 3;
        if(v[offset]->load()<.5f)return 0;
        const auto& x=expansion[lane];
        return cabLayout::key({{{true,int(v[0]->load()),int(v[1]->load()),int(v[offset+1]->load()),int(v[offset+2]->load()),
            v[2]->load(),v[offset+3]->load(),v[offset+4]->load()},int(x[0]->load()),int(x[1+slot]->load()),int(x[3]->load())},int(layouts[lane][0]->load()),int(layouts[lane][1+slot]->load())});
    }
};
}

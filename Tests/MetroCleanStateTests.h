#pragma once
// Metro Clean (model 11) TT-800 remodel: a project saved with the released
// single-channel D-800+ panel must keep loading. Its fourteen host parameters
// keep their IDs and values; the D-800+ state recalls the MODERN channel; the
// appended TT-800 controls (version 34) open at their defaults even when the
// receiving instance had moved them. docs/METRO_CLEAN_TT800.md.
#include "PluginProcessor.h"
#include "AmpNativeDSP.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace metroCleanStateTests {
inline void require(bool ok,const std::string& message) {if(!ok)throw std::runtime_error(message);}
inline juce::RangedAudioParameter& parameter(ChimeraProcessor& p,const juce::String& id) {
    auto* result=p.parameters().getParameter(id);require(result!=nullptr,"Missing parameter "+id.toStdString());return *result;
}
inline void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto& target=parameter(p,id);target.setValueNotifyingHost(target.convertTo0to1(value));
}
inline float get(ChimeraProcessor& p,const juce::String& id) {return parameter(p,id).convertFrom0to1(parameter(p,id).getValue());}
inline std::vector<float> render(const spectralforge::AmpNativeState& state) {
    spectralforge::AmpNativeDSP dsp;dsp.prepare(48000);dsp.set(state);dsp.reset();std::vector<float> out(9600);
    for(size_t n=0;n<out.size();++n)out[n]=dsp.tick(float(.2*std::sin(2*3.14159265358979323846*55*double(n)/48000.)+.05*std::sin(2*3.14159265358979323846*880*double(n)/48000.)),0);
    return out;
}
inline void run() {
    using namespace spectralforge;
    constexpr int model=metroCleanAmpModel;
    const auto& panel=ampNativePanel(model);
    // Released D-800+ positions, in released order (VOICING / HI MID retired).
    constexpr float stored[]{.62f,.30f,.71f,.58f,.40f,.36f,.64f,.67f,.55f,.45f,1,1,0,1};
    static_assert(std::size(stored)==std::size(metroCleanReleasedControls));
    const auto value=[&](int context,size_t i){const auto& r=metroCleanReleasedControls[i];return r.knob?stored[i]-.02f*float(context):stored[i];};
    auto source=std::make_unique<ChimeraProcessor>();auto& p=*source;
    int kept=0,retired=0;
    for(int context=0;context<ampNativeContextCount;++context) {
        for(size_t i=0;i<std::size(metroCleanReleasedControls);++i) {
            const auto& released=metroCleanReleasedControls[i];
            const auto id=metroCleanRetiredID(context,released.key);
            auto& host=parameter(p,id);
            require(host.getVersionHint()==1 && std::abs(host.convertFrom0to1(host.getDefaultValue())-released.initial)<1.e-6f
                && std::abs(host.getNormalisableRange().end-released.maximum)<1.e-6f,"Released Metro Clean parameter changed its version, range or default: "+id.toStdString());
            const int control=ampNativeControlIndex(model,released.key);
            if(control>=0){require(id==ampNativeControlID(context,model,control),"A kept D-800+ key no longer addresses its TT-800 control");++kept;}
            else ++retired;
            set(p,id,value(context,i));
        }
        set(p,ampNativeModelID(context),float(model));set(p,ampNativeChannelID(context,model),0);
    }
    require(kept==6*11 && retired==6*3,"Expected eleven kept and three retired D-800+ controls per context");
    // A released session has no version-34 entries.
    auto older=p.parameters().copyState();int removed=0;
    for(int i=older.getNumChildren()-1;i>=0;--i) {
        auto* host=p.parameters().getParameter(older.getChild(i).getProperty("id").toString());
        if(host!=nullptr && host->getVersionHint()==metroCleanTT800ParameterVersion){older.removeChild(i,nullptr);++removed;}
    }
    require(removed==metroCleanTT800ParameterCount(),"The TT-800 appended bank is not exactly the version-34 parameters");
    // The receiving instance had CLASSIC selected and every TT-800 control moved.
    auto target=std::make_unique<ChimeraProcessor>();auto& r=*target;
    for(int context=0;context<ampNativeContextCount;++context) {
        set(r,ampNativeChannelID(context,model),1);
        for(size_t c=0;c<panel.controls.size();++c)if(!metroCleanReleasedKey(panel.controls[c].key))
            set(r,ampNativeControlID(context,model,int(c)),panel.controls[c].initial>.5f?panel.controls[c].minimum:panel.controls[c].maximum);
    }
    juce::MemoryBlock bytes;const auto xml=older.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
    r.setStateInformation(bytes.getData(),int(bytes.getSize()));
    AmpNativeParameterCache cache;cache.bind(r.parameters());
    for(int context=0;context<ampNativeContextCount;++context) {
        require(std::lround(get(r,ampNativeChannelID(context,model)))==0,"A D-800+ session did not recall the MODERN channel");
        for(size_t i=0;i<std::size(metroCleanReleasedControls);++i)
            require(std::abs(get(r,metroCleanRetiredID(context,metroCleanReleasedControls[i].key))-value(context,i))<.0011f,"A stored D-800+ value changed on recall");
        const auto state=cache.read(context);
        require(state.model==model && state.channel==0,"The DSP cache does not run the recalled MODERN channel");
        auto expected=defaultAmpNativeState(model);expected.enabled=state.enabled;expected.soloEnabled=state.soloEnabled;
        expected.inputTrimDb=state.inputTrimDb;expected.outputLevelDb=state.outputLevelDb;expected.inputRoute=state.inputRoute;
        for(size_t c=0;c<panel.controls.size();++c) {
            const auto& k=panel.controls[c];
            if(metroCleanReleasedKey(k.key)) {
                size_t i=0;while(std::string_view(metroCleanReleasedControls[i].key)!=k.key)++i;
                // The recalled D-800+ parameter (checked above against the stored value).
                expected.values[c]=r.parameters().getRawParameterValue(metroCleanRetiredID(context,k.key))->load();
                require(i<std::size(stored),"Released key lookup");
            } else require(std::abs(state.values[c]-k.initial)<.0011f,std::string("A TT-800 control inherited a non-default value: ")+k.key);
            require(std::abs(state.values[c]-expected.values[c])<.0011f,std::string("Recalled Metro Clean control differs: ")+k.key);
        }
        // Sound: the recalled state renders exactly as its D-800+ values mapped
        // to MODERN, and the retired VOICING / HI MID values have no effect.
        const auto recalled=render(state);
        require(recalled==render(expected),"The recalled D-800+ session does not render as its mapped MODERN state");
        if(context==0) {
            for(const char* key:{"hw.voicing","hw.hi_mid_frequency","hw.hi_mid"})set(r,metroCleanRetiredID(0,key),.05f);
            require(render(cache.read(0))==recalled,"A retired D-800+ parameter still reaches the engine");
        }
    }
    std::cout<<"PASS Metro Clean D-800+ sessions: 14 released IDs per context kept (11 mapped to TT-800 MODERN + shared, 3 retired), MODERN recalled, "
             <<metroCleanTT800ParameterCount()<<" appended TT-800 parameters at defaults\n";
}
} // namespace metroCleanStateTests

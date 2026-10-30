#include "Amplifier.h"
#include "NiflheimrPresets.h"
#include <functional>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Internal production tool. The target is the released Amp::setNative path,
// including its 4x IIR oversampling and delay compensation. No FX, cab or mix.
namespace {
constexpr int rate=48000, blockSize=256;
using namespace spectralforge;
juce::var object() { return juce::var(new juce::DynamicObject); }
void property(juce::var& v,const char* k,const juce::var& x) { v.getDynamicObject()->setProperty(k,x); }
double numeric(const juce::var& v) {
    if(!(v.isDouble() || v.isInt() || v.isInt64()) || !std::isfinite(double(v)))
        throw std::runtime_error("Expected finite numeric value");
    return double(v);
}
// The amp a job renders. Náströnd captures use each channel's own defaults.
// Niflheimr captures use the factory rig voicing of each channel (see voicing()).
struct ControlInfo { const char* id;const char* label;float minimum,maximum,initial; };
struct Spec {
    const char* key;const char* display;int model;size_t controlCount;int channelCount;
    const char* const* channelNames;
    std::function<ControlInfo(size_t)> control;
    std::function<std::vector<float>(int)> defaults;
};
// Channel -> (rig preset, lane) whose voice is that channel's main voice in
// NiflheimrPresets.h. Hrímfaxi and Hel lead Classic rigs; Garmr, Nidavellir and
// Ymir are the driven lane of Death Grind, Heavy Pulse and Slam Punch.
constexpr int niflheimrVoicing[niflheimr::channelCount][2]{{0,0},{1,1},{2,1},{3,1},{4,0}};
const Spec& spec(const juce::String& key) {
    static const Spec nastrond{"nastrond","Náströnd",firstOriginalAmpModel,original::controlCount,original::channelCount,original::channelNames,
        [](size_t i){const auto& c=original::controls[i];return ControlInfo{c.id,c.label,c.minimum,c.maximum,c.initial};},
        [](int ch){auto s=original::channelState(ch);return std::vector<float>(s.values.begin(),s.values.end());}};
    static const Spec nifl{"niflheimr","Niflheimr",niflheimrAmpModel,niflheimr::controlCount,niflheimr::channelCount,niflheimr::channelNames,
        [](size_t i){const auto& c=niflheimr::controls[i];return ControlInfo{c.id,c.label,c.minimum,c.maximum,c.initial};},
        [](int ch){const auto v=niflheimrRigVoice(niflheimrVoicing[ch][0],niflheimrVoicing[ch][1]);
            if(v.channel!=ch)throw std::runtime_error("Niflheimr voicing table names the wrong channel");
            return std::vector<float>(v.values.begin(),v.values.end());}};
    if(key.isEmpty() || key=="nastrond")return nastrond;
    if(key=="niflheimr")return nifl;
    throw std::runtime_error("Unknown amp: "+key.toStdString());
}
void apply(const Spec& amp,AmpNativeState& state,const juce::var& job) {
    if(!job.isObject())throw std::runtime_error("State must be an object");
    if(job.hasProperty("channel")) {
        const auto n=numeric(job["channel"]);
        if(n!=std::floor(n) || n<0 || n>=amp.channelCount)throw std::runtime_error("Invalid channel");
        state.channel=int(n);
    }
    if(job.hasProperty("controls")) {
        const auto* values=job["controls"].getDynamicObject();
        if(!values)throw std::runtime_error("controls must be an object");
        for(const auto& kv:values->getProperties()) {
            const auto key=kv.name.toString().toStdString();
            const auto idx=ampNativeControlIndex(amp.model,key);
            if(idx<0)throw std::runtime_error("Unknown control: "+key);
            const auto x=numeric(kv.value);const auto c=amp.control(size_t(idx));
            if(x<c.minimum || x>c.maximum)throw std::runtime_error("Control out of range: "+key);
            state.values[size_t(idx)]=float(x);
        }
    }
}
AmpNativeState initial(const Spec& amp,const juce::var& job) {
    auto s=defaultAmpNativeState(amp.model);
    apply(amp,s,job);
    const auto defaults=amp.defaults(s.channel);
    for(size_t i=0;i<amp.controlCount;++i)s.values[i]=defaults[i];
    apply(amp,s,job);s.enabled=true;s.originalModern=true;s.inputTrimDb=0;s.outputLevelDb=0;
    return s;
}
juce::var catalogue(const Spec& amp) {
    auto out=object();if(amp.model!=firstOriginalAmpModel)property(out,"amp",amp.key);property(out,"model",juce::String::fromUTF8(amp.display));property(out,"model_index",amp.model);
    property(out,"sample_rate",rate);property(out,"block_size",blockSize);property(out,"oversampling",4);
    juce::Array<juce::var> params, mapping;
    for(size_t i=0;i<amp.controlCount;++i) {
        const auto c=amp.control(i);const bool hz=std::string(c.id)=="mid_frequency";
        auto p=object();property(p,"name",c.label);property(p,"type","continuous");
        property(p,"min",hz?c.minimum:0.f);property(p,"max",hz?c.maximum:10.f);
        property(p,"default",amp.defaults(0)[i]*(hz?1.f:10.f));params.add(p);
        auto m=p; m=m.clone();property(m,"source_key",c.id);property(m,"source_scale",hz?1.0:.1);mapping.add(m);
    }
    auto ch=object();property(ch,"name","CHANNEL");property(ch,"type","switch");
    property(ch,"min",0);property(ch,"max",amp.channelCount-1);property(ch,"default",0);
    juce::Array<juce::var> names;for(int n=0;n<amp.channelCount;++n)names.add(juce::String::fromUTF8(amp.channelNames[n]));property(ch,"enum_names",names);
    params.add(ch);property(out,"params",params);property(out,"mapping",mapping);
    juce::Array<juce::var> defaults;
    for(int chn=0;chn<amp.channelCount;++chn){const auto v=amp.defaults(chn);juce::Array<juce::var> p;
      for(size_t i=0;i<amp.controlCount;++i)p.add(v[i]*(std::string(amp.control(i).id)=="mid_frequency"?1.f:10.f));p.add(chn);defaults.add(p);}
    property(out,"channel_defaults",defaults);
    if(amp.model==niflheimrAmpModel){juce::Array<juce::var> src;
      for(int chn=0;chn<amp.channelCount;++chn){auto r=object();property(r,"preset",juce::String(niflheimrRigPresets[size_t(niflheimrVoicing[chn][0])].id));property(r,"lane",niflheimrVoicing[chn][1]);src.add(r);}
      property(out,"channel_default_source",src);}
    return out;
}
}
int main(int argc,char** argv) try {
    if((argc==2 || argc==3) && std::string(argv[1])=="--catalog") {std::cout<<juce::JSON::toString(catalogue(spec(argc==3?argv[2]:""))).toStdString()<<'\n';return 0;}
    if(argc!=4){std::cerr<<"Usage: ChimeraNamRender input.f32 output.f32 job.json | --catalog [nastrond|niflheimr]\n";return 1;}
    const auto job=juce::JSON::parse(juce::File(argv[3]));const auto& target=spec(job["amp"].toString());auto state=initial(target,job);
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open input");
    const auto bytes=in.tellg();if(bytes<=0 || bytes%4!=0 || bytes>rate*600LL*4)throw std::runtime_error("Invalid f32 length");
    std::vector<float> samples(size_t(bytes)/4);in.seekg(0);if(!in.read(reinterpret_cast<char*>(samples.data()),bytes))throw std::runtime_error("Cannot read input samples");
    for(float x:samples)if(!std::isfinite(x))throw std::runtime_error("Nonfinite input");
    Amp amp;amp.prepare({rate,blockSize,1});amp.setNative(state);amp.setOversampling(2);amp.reset();
    // Establish the same static state before capture. No sample-dependent alignment.
    juce::AudioBuffer<float> warm(1,blockSize);
    for(int p=0;p<rate;p+=blockSize){warm.clear();amp.process(warm,false,true);}
    const auto* events=job["events"].getArray();int eventIndex=0, previous=-1;
    if(events)for(const auto& e:*events){const double n=numeric(e["sample"]);
      if(n!=std::floor(n) || n<0 || n>=samples.size() || int(n)%blockSize || n<=previous)
        throw std::runtime_error("Events must be unique ordered block-aligned sample indices");
      auto check=state;apply(target,check,e);previous=int(n);}
    for(size_t p=0;p<samples.size();p+=blockSize) {
        if(events && eventIndex<events->size() && int((*events)[eventIndex]["sample"])==int(p)){
            apply(target,state,(*events)[eventIndex++]);amp.setNative(state);
        }
        float* ptr=samples.data()+p;juce::AudioBuffer<float> buffer(&ptr,1,int(std::min(size_t(blockSize),samples.size()-p)));
        amp.process(buffer,false,true);
    }
    double sum=0,peak=0;for(float x:samples){if(!std::isfinite(x))throw std::runtime_error("Nonfinite output");sum+=double(x)*x;peak=std::max(peak,std::abs(double(x)));}
    std::ofstream out(argv[2],std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(samples.data()),bytes);
    if(!out)throw std::runtime_error("Cannot write output");
    auto meta=object();property(meta,"samples",juce::int64(samples.size()));property(meta,"latency",amp.latency());
    property(meta,"peak",peak);property(meta,"rms",std::sqrt(sum/samples.size()));property(meta,"sample_rate",rate);
    property(meta,"oversampling",4);property(meta,"block_size",blockSize);property(meta,"events",eventIndex);
    std::cout<<juce::JSON::toString(meta,true).toStdString()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}

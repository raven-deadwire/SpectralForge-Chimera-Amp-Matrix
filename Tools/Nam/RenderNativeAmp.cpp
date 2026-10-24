#include "Amplifier.h"
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
void apply(AmpNativeState& state,const juce::var& job) {
    if(!job.isObject())throw std::runtime_error("State must be an object");
    if(job.hasProperty("channel")) {
        const auto n=numeric(job["channel"]);
        if(n!=std::floor(n) || n<0 || n>=original::channelCount)throw std::runtime_error("Invalid channel");
        state.channel=int(n);
    }
    if(job.hasProperty("controls")) {
        const auto* values=job["controls"].getDynamicObject();
        if(!values)throw std::runtime_error("controls must be an object");
        for(const auto& kv:values->getProperties()) {
            const auto key=kv.name.toString().toStdString();
            const auto idx=ampNativeControlIndex(firstOriginalAmpModel,key);
            if(idx<0)throw std::runtime_error("Unknown control: "+key);
            const auto x=numeric(kv.value);const auto& c=original::controls[size_t(idx)];
            if(x<c.minimum || x>c.maximum)throw std::runtime_error("Control out of range: "+key);
            state.values[size_t(idx)]=float(x);
        }
    }
}
AmpNativeState initial(const juce::var& job) {
    auto s=defaultAmpNativeState(firstOriginalAmpModel);
    apply(s,job);
    const auto defaults=original::channelState(s.channel);
    for(size_t i=0;i<original::controlCount;++i)s.values[i]=defaults.values[i];
    apply(s,job);s.enabled=true;s.originalModern=true;s.inputTrimDb=0;s.outputLevelDb=0;
    return s;
}
juce::var catalogue() {
    auto out=object();property(out,"model",juce::String::fromUTF8("Náströnd"));property(out,"model_index",firstOriginalAmpModel);
    property(out,"sample_rate",rate);property(out,"block_size",blockSize);property(out,"oversampling",4);
    juce::Array<juce::var> params, mapping;
    for(size_t i=0;i<original::controlCount;++i) {
        const auto& c=original::controls[i];const bool hz=std::string(c.id)=="mid_frequency";
        auto p=object();property(p,"name",c.label);property(p,"type","continuous");
        property(p,"min",hz?c.minimum:0.f);property(p,"max",hz?c.maximum:10.f);
        property(p,"default",original::channelState(0).values[i]*(hz?1.f:10.f));params.add(p);
        auto m=p; m=m.clone();property(m,"source_key",c.id);property(m,"source_scale",hz?1.0:.1);mapping.add(m);
    }
    auto ch=object();property(ch,"name","CHANNEL");property(ch,"type","switch");
    property(ch,"min",0);property(ch,"max",4);property(ch,"default",0);
    juce::Array<juce::var> names;for(auto n:original::channelNames)names.add(juce::String::fromUTF8(n));property(ch,"enum_names",names);
    params.add(ch);property(out,"params",params);property(out,"mapping",mapping);
    juce::Array<juce::var> defaults;
    for(int chn=0;chn<5;++chn){auto s=original::channelState(chn);juce::Array<juce::var> p;
      for(size_t i=0;i<original::controlCount;++i)p.add(s.values[i]*(i==4?1.f:10.f));p.add(chn);defaults.add(p);}
    property(out,"channel_defaults",defaults);return out;
}
}
int main(int argc,char** argv) try {
    if(argc==2 && std::string(argv[1])=="--catalog") {std::cout<<juce::JSON::toString(catalogue()).toStdString()<<'\n';return 0;}
    if(argc!=4){std::cerr<<"Usage: ChimeraNamRender input.f32 output.f32 job.json | --catalog\n";return 1;}
    const auto job=juce::JSON::parse(juce::File(argv[3]));auto state=initial(job);
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open input");
    const auto bytes=in.tellg();if(bytes<=0 || bytes%4!=0 || bytes>rate*600LL*4)throw std::runtime_error("Invalid f32 length");
    std::vector<float> samples(size_t(bytes)/4);in.seekg(0);in.read(reinterpret_cast<char*>(samples.data()),bytes);
    for(float x:samples)if(!std::isfinite(x))throw std::runtime_error("Nonfinite input");
    Amp amp;amp.prepare({rate,blockSize,1});amp.setNative(state);amp.setOversampling(2);amp.reset();
    // Establish the same static state before capture. No sample-dependent alignment.
    juce::AudioBuffer<float> warm(1,blockSize);
    for(int p=0;p<rate;p+=blockSize){warm.clear();amp.process(warm,false,true);}
    const auto* events=job["events"].getArray();int eventIndex=0, previous=-1;
    if(events)for(const auto& e:*events){const double n=numeric(e["sample"]);
      if(n!=std::floor(n) || n<0 || n>=samples.size() || int(n)%blockSize || n<=previous)
        throw std::runtime_error("Events must be unique ordered block-aligned sample indices");
      auto check=state;apply(check,e);previous=int(n);}
    for(size_t p=0;p<samples.size();p+=blockSize) {
        if(events && eventIndex<events->size() && int((*events)[eventIndex]["sample"])==int(p)){
            apply(state,(*events)[eventIndex++]);amp.setNative(state);
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

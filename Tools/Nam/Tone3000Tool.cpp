// Test the pinned, unmodified TONE3000 engine / EQ / preset writer offline.
#include "NamEngine.h"
#include "BlockEq.h"
#include "PresetFile.h"
#include "NAM/get_dsp.h"
#include "NAM/wavenet/a2_fast.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <numeric>
using Json = nlohmann::json;
namespace {
Json readJson(const char* path) { std::ifstream in(path); Json j; in >> j; return j; }
juce::String js(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }
void set(juce::ValueTree& t,const char* k,const juce::var& v) { t.setProperty(k,v,nullptr); }
void configure(BlockEq& eq,const Json& job) {
  if (!job.contains("eq")) {eq.setEnabled(false);return;}
  const auto e=juce::JSON::parse(js(job.at("eq").dump()));
  const auto* bands=e["bands"].getArray();
  if(!bands || bands->size()!=6)throw std::runtime_error("Exactly six EQ bands required");
  for(int i=0;i<6;++i) if(!eq.setBandFromVar(i,(*bands)[i]))throw std::runtime_error("Invalid EQ band");
  eq.setEnabled(bool(e["enabled"]));eq.setPre(bool(e["pre"]));eq.prepare(48000);
}
Json inspectPreset(const juce::File& file) {
  auto p=t3k::presetfile::read(file);auto header=t3k::presetfile::readHeader(file);
  if(!header.valid || !p.hasType("T3KPreset"))throw std::runtime_error("Invalid preset framing");
  auto blocks=p.getChildWithName("ChainSnapshot").getChildWithName("ChainBlocks");
  Json results=Json::array();
  for(int i=0;i<blocks.getNumChildren();++i){auto b=blocks.getChild(i);if(b["type"].toString()!="nam")continue;
    auto cache=b.getChildWithName("ModelCache");auto data=cache.getChild(0)["data"];
    const auto* bytes=data.getBinaryData();if(!bytes)throw std::runtime_error("Missing embedded NAM");
    auto model=Json::parse(static_cast<const char*>(bytes->getData()),static_cast<const char*>(bytes->getData())+bytes->getSize());
    int channels=0;if(model.value("architecture","")!="WaveNet" || !nam::wavenet::a2_fast::is_a2_shape(model.at("config"),&channels))throw std::runtime_error("Embedded model fails TONE3000 A2 gate");
    BlockEq eq;eq.restoreFromValueTree(b.getChildWithName("Eq"));
    results.push_back({{"embedded_bytes",bytes->getSize()},{"a2_channels",channels},{"normalize",bool(b["normalize"])},
      {"input_gain_normalized",double(b["inputGain"])},{"output_gain_normalized",double(b["outputGain"])},
      {"eq",Json::parse(juce::JSON::toString(eq.toVar()).toStdString())}});
  }
  if(results.size()!=1)throw std::runtime_error("Expected one amp block");
  return {{"name",header.name.toStdString()},{"id",header.id.toStdString()},{"v2",!header.legacy},{"blocks",results}};
}
void preset(const char* modelPath,const char* outputPath,const Json& job) {
  const auto name=js(job.at("name").get<std::string>());const auto uid=js(job.at("id").get<std::string>());
  juce::MemoryBlock bytes; if(!juce::File(modelPath).loadFileAsData(bytes))throw std::runtime_error("Cannot read model bytes");
  juce::ValueTree p("T3KPreset");set(p,"schemaVersion",1);set(p,"id",uid);set(p,"name",name);
  juce::ValueTree snap("ChainSnapshot");set(snap,"stereoEnabled",false);set(snap,"branchSide","left");set(snap,"branchAfterBlockId","");
  juce::ValueTree left("ChainBlocks"),right("RightChainBlocks"),block("ChainBlock");
  set(block,"id",uid+"-amp");set(block,"type","nam");set(block,"enabled",true);set(block,"normalize",job.value("normalize",true));
  set(block,"slimSize",1.0);set(block,"inputGain",0.5);set(block,"outputGain",0.5);set(block,"mix",1.0);set(block,"toneId",0);set(block,"activeModelId",1);
  Json tone={{"id",0},{"local",true},{"title",name.toStdString()},{"format","nam"},{"gear","amp"},
    {"models",Json::array({{{"id",1},{"name",name.toStdString()},{"model_url",""},{"gear","amp"}}})}};
  set(block,"toneJson",js(tone.dump()));BlockEq eq;configure(eq,job);block.appendChild(eq.toValueTree(),nullptr);
  juce::ValueTree cache("ModelCache"),cm("CachedModel");set(cm,"modelId",1);set(cm,"data",juce::var(bytes));cache.appendChild(cm,nullptr);block.appendChild(cache,nullptr);left.appendChild(block,nullptr);
  snap.appendChild(left,nullptr);snap.appendChild(right,nullptr);p.appendChild(snap,nullptr);
  juce::ValueTree params("Params");
  const Json values={{"inputLevel",0.5},{"outputLevel",0.5},{"outputBalance",0.5},{"toneBass",5.0},{"toneMid",5.0},{"toneTreble",5.0},
    {"gateEnabled",0.0},{"toneEqEnabled",0.0},{"pitchEnabled",0.0},{"spreadEnabled",0.0},{"alignEnabled",0.0},{"chainInvertLeft",0.0},{"chainInvertRight",0.0}};
  for(auto it=values.begin();it!=values.end();++it){juce::ValueTree param("Param");set(param,"id",js(it.key()));set(param,"value",it.value().get<double>());params.appendChild(param,nullptr);}
  p.appendChild(params,nullptr);
  if(!t3k::presetfile::write(juce::File(outputPath),p))throw std::runtime_error("Preset write failed");
  const auto reread=t3k::presetfile::read(juce::File(outputPath));
  if(!p.isEquivalentTo(reread))throw std::runtime_error("Preset roundtrip mismatch");
  std::cout<<inspectPreset(juce::File(outputPath)).dump()<<'\n';
}
void render(const char* modelPath,const char* inputPath,const char* outputPath,const Json& job) {
  const auto model=readJson(modelPath);int channels=0;
  if(model.value("architecture","")!="WaveNet" || !nam::wavenet::a2_fast::is_a2_shape(model.at("config"),&channels))throw std::runtime_error("TONE3000 rejects non-A2 model");
  const int block=job.value("block_size",64);if(block<1 || block>4096)throw std::runtime_error("Invalid block size");
  std::vector<std::unique_ptr<nam::DSP>> instances;instances.push_back(nam::get_dsp(std::filesystem::path(modelPath)));
  NamEngine engine(std::move(instances),1);engine.setSlimmableSize(1);engine.prepare(block);
  BlockEq eq;configure(eq,job);
  std::ifstream in(inputPath,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Input open failed");const auto size=in.tellg();
  if(size<=0 || size%4)throw std::runtime_error("Invalid float32 length");std::vector<float> x(static_cast<size_t>(size)/4);in.seekg(0);in.read(reinterpret_cast<char*>(x.data()),size);
  double gain=1.;if(job.value("normalize",false)){
    double loud=engine.hasLoudness()?engine.getLoudness():-18.;
    if(!std::isfinite(loud)||loud < -100 || loud > 0)loud=-18.;
    gain=std::pow(10.,std::clamp(-18.-loud,-12.,12.)/20.);
  }
  std::vector<double> times;size_t misses=0;
  for(size_t s=0;s<x.size();s+=block){const auto n=int(std::min<size_t>(block,x.size()-s));float* p=x.data()+s;juce::AudioBuffer<float> b(&p,1,n);
    const auto t=std::chrono::steady_clock::now();if(eq.isActive()&&eq.isPre())eq.process(b);engine.process(b);b.applyGain(float(gain));if(eq.isActive()&&!eq.isPre())eq.process(b);
    const double dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();times.push_back(dt);if(dt>double(n)/48000)++misses;}
  double power=0,peak=0;for(const auto v:x){if(!std::isfinite(v))throw std::runtime_error("Nonfinite output");power+=double(v)*v;peak=std::max(peak,std::abs(double(v)));}
  std::ofstream out(outputPath,std::ios::binary);out.write(reinterpret_cast<const char*>(x.data()),size);if(!out)throw std::runtime_error("Output write failed");
  std::sort(times.begin(),times.end());const double p99=times[size_t((times.size()-1)*.99)];
  std::cout<<Json({{"a2_gate",true},{"a2_channels",channels},{"sample_rate",engine.getModelSampleRate()},{"samples",x.size()},{"peak",peak},{"rms",std::sqrt(power/x.size())},
    {"normalize",job.value("normalize",false)},{"normalization_gain",gain},{"callback_p99_seconds",p99},{"p99_budget_ratio",p99*48000/block},{"deadline_misses",misses},
    {"eq",Json::parse(juce::JSON::toString(eq.toVar()).toStdString())}}).dump()<<'\n';
}
}
int main(int argc,char** argv) try {
  if(argc==3 && std::string(argv[1])=="inspect"){std::cout<<inspectPreset(juce::File(argv[2])).dump()<<'\n';return 0;}
  if(argc==5 && std::string(argv[1])=="preset"){preset(argv[2],argv[3],readJson(argv[4]));return 0;}
  if(argc==6 && std::string(argv[1])=="render"){render(argv[2],argv[3],argv[4],readJson(argv[5]));return 0;}
  throw std::runtime_error("Usage: tool render model input output job | preset model output job | inspect preset");
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

#include "IRLibrary.h"
#include "ChimeraIRData.h"

namespace spectralforge {
IRLibrary::IRLibrary(std::array<Cab*,3> cabinets) : Thread("Chimera IR preparation"), cabs(cabinets)
{
    juce::String error;
    factory[0]=decode(juce::MemoryBlock(ChimeraIRData::guitar_v30_sm57_wav,ChimeraIRData::guitar_v30_sm57_wavSize),"V30 / SM57",error);
    factory[1]=decode(juce::MemoryBlock(ChimeraIRData::guitar_jensen_sm57_wav,ChimeraIRData::guitar_jensen_sm57_wavSize),"Jensen / SM57",error);
    jassert(factory[0] && factory[1]);
    for(int i=0;i<2;++i)if(factory[i]) {
        auto& m=factory[i]->metadata;m.values[0]=i==0 ? "Celestion Vintage 30" : "Jensen (model unspecified)";
        m.values[1]=i==0 ? "ENGL (configuration unspecified)" : "Unspecified";m.values[3]="Shure SM57";m.values[4]="Center";
        m.values[8]="jesterdyne";m.values[9]=i==0 ? "https://freesound.org/s/116735/" : "https://freesound.org/s/116743/";m.values[10]="CC BY 4.0";m.values[11]="Factory IR. Distance, angle and speaker diameter are not documented by this asset.";
    }
}
IRLibrary::~IRLibrary() { stop(); }
void IRLibrary::stop() { signalThreadShouldExit(); notify(); stopThread(-1); }
std::shared_ptr<IRLibrary::Asset> IRLibrary::decode(const juce::MemoryBlock& bytes, const juce::String& name, juce::String& error)
{
    error.clear();
    if(bytes.getSize()<44 || bytes.getSize()>4*1024*1024) { error="IR must be a WAV/AIFF file under 4 MB."; return {}; }
    juce::AudioFormatManager formats;
    formats.registerFormat(new juce::WavAudioFormat(),true);
    formats.registerFormat(new juce::AiffAudioFormat(),false);
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(std::make_unique<juce::MemoryInputStream>(bytes,false)));
    if(!reader) { error="Cannot decode this WAV/AIFF file. Previous IR kept."; return {}; }
    if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>384000 ||
       reader->lengthInSamples<8 || reader->lengthInSamples>juce::int64(reader->sampleRate))
    { error="Use a mono/stereo IR, 8-384 kHz, up to 1 second."; return {}; }
    auto asset=std::make_shared<Asset>();
    asset->samples.setSize((int)reader->numChannels,(int)reader->lengthInSamples);
    if(!reader->read(&asset->samples,0,asset->samples.getNumSamples(),0,true,true))
    { error="IR data is incomplete. Previous IR kept."; return {}; }
    double energy=0;
    for(int c=0;c<asset->samples.getNumChannels();++c)
        for(int n=0;n<asset->samples.getNumSamples();++n)
        {
            const auto x=asset->samples.getSample(c,n);
            if(!std::isfinite(x) || std::abs(x)>32.f) { error="IR contains invalid samples."; return {}; }
            energy+=double(x)*x;
        }
    if(energy<1e-12) { error="IR is silent. Previous IR kept."; return {}; }
    asset->encoded=bytes; asset->rate=reader->sampleRate; asset->name=name;
    asset->metadata=IRMetadata::filenameHints(name);
    return asset;
}
juce::Result IRLibrary::importFile(int lane, const juce::File& file)
{
    if(lane<0 || lane>2) return juce::Result::fail("Invalid lane");
    juce::MemoryBlock bytes;
    juce::String error;
    std::shared_ptr<Asset> asset;
    if(file.getSize()>4*1024*1024 || !file.loadFileAsData(bytes)) error="Cannot read IR (maximum 4 MB).";
    else asset=decode(bytes,file.getFileName(),error);
    if(asset) {const auto sidecar=juce::File(file.getFullPathName()+".json");
        if(sidecar.existsAsFile() && sidecar.getSize()<=16384) {const auto json=juce::JSON::parse(sidecar);if(json.isObject())asset->metadata=IRMetadata::fromJSON(json);}}
    { std::lock_guard<std::mutex> lock(mutex);
      errors[(size_t)lane]=error;
      if(asset) { users[(size_t)lane]=std::move(asset); ++generations[(size_t)lane]; }
    }
    if(error.isNotEmpty()) return juce::Result::fail(error);
    notify(); return juce::Result::ok();
}
std::unique_ptr<Cab::Kernel> IRLibrary::build(int lane,int source,unsigned generation)
{
    std::shared_ptr<Asset> asset;
    { std::lock_guard<std::mutex> lock(mutex);
      if(source==1 || source==2) asset=factory[(size_t)source-1];
      else if(source==3) asset=users[(size_t)lane];
    }
    juce::AudioBuffer<float> samples;
    double rate=spec.sampleRate;
    if(asset) { samples.makeCopyOf(asset->samples); rate=asset->rate; }
    else { samples.setSize(1,1); samples.setSample(0,0,1.f); }
    auto kernel=std::make_unique<Cab::Kernel>(std::move(samples),rate,spec,source,generation);
    kernel->hasIR = asset != nullptr;
    return kernel;
}
void IRLibrary::prepare(const juce::dsp::ProcessSpec& settings,const std::array<int,3>& sources)
{
    stop(); spec=settings;
    for(int i=0;i<3;++i)
    {
        cabs[i]->requestedSource.store(sources[i]);
        unsigned generation;
        { std::lock_guard<std::mutex> lock(mutex); generation=generations[i]; }
        cabs[i]->install(build(i,sources[i],generation));
    }
    startThread();
}
void IRLibrary::run()
{
    std::array<int,3> built;
    std::array<unsigned,3> versions;
    for(int i=0;i<3;++i) { built[i]=cabs[i]->activeSource.load(); versions[i]=cabs[i]->activeGeneration.load(); }
    while(!threadShouldExit())
    {
        for(int i=0;i<3 && !threadShouldExit();++i)
        {
            cabs[i]->collect();
            const int source=cabs[i]->requestedSource.load();
            unsigned generation;
            { std::lock_guard<std::mutex> lock(mutex); generation=generations[i]; }
            if(source==built[i] && (source!=3 || generation==versions[i])) continue;
            try { cabs[i]->publish(build(i,source,generation)); }
            catch(const std::exception&) { std::lock_guard<std::mutex> lock(mutex); errors[i]="IR preparation failed. Previous IR kept."; }
            built[i]=source; versions[i]=generation;
        }
        wait(20);
    }
    for(auto* cab:cabs) cab->collect();
}
juce::String IRLibrary::status(int lane) const
{
    std::lock_guard<std::mutex> lock(mutex);
    if(errors[lane].isNotEmpty()) return errors[lane];
    const int source=cabs[lane]->requestedSource.load();
    if(source==3 && !users[lane]) return "No user IR loaded. Filters only.";
    const bool loading=cabs[lane]->activeSource.load()!=source ||
        (source==3 && cabs[lane]->activeGeneration.load()!=generations[lane]);
    if(loading) return source==3 && users[lane] ? users[lane]->name+" | Preparing IR..." : "Preparing IR...";
    if(source==0) return "Filters only | no speaker IR";
    const auto asset=source==3 ? users[lane] : factory[(size_t)source-1];
    if(!asset) return "Factory IR unavailable";
    return asset->name + " | " + juce::String(juce::roundToInt(1000.0*asset->samples.getNumSamples()/asset->rate))+" ms";
}
juce::ValueTree IRLibrary::save() const
{
    juce::ValueTree tree("USER_IRS");
    std::lock_guard<std::mutex> lock(mutex);
    for(int i=0;i<3;++i) if(users[i])
    {
        juce::ValueTree child("IR");
        child.setProperty("lane",i,nullptr); child.setProperty("name",users[i]->name,nullptr);
        child.setProperty("data",users[i]->encoded.toBase64Encoding(),nullptr);
        child.setProperty("metadata",juce::JSON::toString(users[i]->metadata.json(),true),nullptr);
        tree.appendChild(child,nullptr);
    }
    return tree;
}
void IRLibrary::restore(const juce::ValueTree& tree)
{
    std::array<std::shared_ptr<Asset>,3> restored;
    std::array<juce::String,3> messages;
    for(auto child:tree)
    {
        const int lane=(int)child.getProperty("lane",-1);
        if(lane<0 || lane>2) continue;
        const auto encoded=child.getProperty("data").toString();
        juce::MemoryBlock bytes;
        if(encoded.length()>6*1024*1024 || !bytes.fromBase64Encoding(encoded)) messages[lane]="Saved IR is damaged. Filters only.";
        else restored[lane]=decode(bytes,child.getProperty("name").toString(),messages[lane]);
        if(restored[lane] && child.hasProperty("metadata"))restored[lane]->metadata=IRMetadata::fromJSON(juce::JSON::parse(child.getProperty("metadata").toString()));
    }
    { std::lock_guard<std::mutex> lock(mutex);
      users=std::move(restored); errors=std::move(messages);
      for(auto& generation:generations) ++generation;
    }
    notify();
}
IRMetadata IRLibrary::metadata(int lane,int source) const
{
    std::lock_guard<std::mutex> lock(mutex);if(lane<0 || lane>2)return {};
    auto asset=source==3 ? users[(size_t)lane] : source==1 || source==2 ? factory[(size_t)source-1] : nullptr;
    return asset ? asset->metadata : IRMetadata{};
}
void IRLibrary::setMetadata(int lane,const IRMetadata& metadata)
{
    std::lock_guard<std::mutex> lock(mutex);if(lane>=0 && lane<3 && users[(size_t)lane])users[(size_t)lane]->metadata=IRMetadata::fromJSON(metadata.json());
}
}

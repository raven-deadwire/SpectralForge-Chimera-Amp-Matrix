#include "IRLibrary.h"
#include "IRUserPreferences.h"
#include "ChimeraIRData.h"
#include "CabLayoutModel.h"

namespace spectralforge {
IRLibrary::IRLibrary(std::array<Cab*,3> cabinets) : Thread("Chimera IR preparation"), cabs{cabinets[0],cabinets[1],cabinets[2],cabinets[0]->secondMic(),cabinets[1]->secondMic(),cabinets[2]->secondMic()}
{
    juce::String error;
    factory[0]=decode(juce::MemoryBlock(ChimeraIRData::guitar_v30_sm57_wav,ChimeraIRData::guitar_v30_sm57_wavSize),IRMetadata::factoryFilename(0),error);
    factory[1]=decode(juce::MemoryBlock(ChimeraIRData::guitar_jensen_sm57_wav,ChimeraIRData::guitar_jensen_sm57_wavSize),IRMetadata::factoryFilename(1),error);
    jassert(factory[0] && factory[1]);
    for(int i=0;i<2;++i)if(factory[i])factory[i]->metadata=IRMetadata::factory(i);
}
IRLibrary::~IRLibrary() { stop(); }
void IRLibrary::requestStop()
{
    lifecycle::write("ir.stop.requested", this);
    signalThreadShouldExit(); notify();
}
void IRLibrary::stop()
{
    const lifecycle::Scope trace("ir.stop", this);
    requestStop(); stopThread(-1);
}
bool IRLibrary::resourcesReleased() const noexcept
{
    if (isThreadRunning()) return false;
    for (const auto* cab : cabs) if (cab->hasResources()) return false;
    return true;
}
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
    asset->encoded=bytes; asset->rate=reader->sampleRate; asset->name=IRMetadata::leafName(name);
    asset->metadata=IRMetadata::filenameHints(asset->name);
    return asset;
}
juce::Result IRLibrary::importFile(int lane, const juce::File& file)
{
    if(lane<0 || lane>5) return juce::Result::fail("Invalid lane");
    juce::MemoryBlock bytes;
    juce::String error;
    std::shared_ptr<Asset> asset;
    if(file.getSize()>4*1024*1024 || !file.loadFileAsData(bytes)) error="Cannot read IR (maximum 4 MB).";
    else asset=decode(bytes,file.getFileName(),error);
    if(asset) {const auto sidecar=juce::File(file.getFullPathName()+".json");
        if(sidecar.existsAsFile() && sidecar.getSize()<=16384) {const auto json=juce::JSON::parse(sidecar);if(json.isObject())asset->metadata=IRMetadata::fromJSON(json);}}
    if(asset)IRUserPreferences::apply(IRUserPreferences::read(),file,asset->metadata);
    { std::lock_guard<std::mutex> lock(mutex);
      errors[(size_t)lane]=error;
      if(asset) { users[(size_t)lane]=std::move(asset); ++generations[(size_t)lane]; }
      ++displayGeneration[(size_t)lane];
    }
    if(error.isNotEmpty()) return juce::Result::fail(error);
    notify(); return juce::Result::ok();
}
std::unique_ptr<Cab::Kernel> IRLibrary::build(int lane,int source,unsigned generation,uint64_t model)
{
    SF_CAB_PROFILE_SCOPE(WorkerBuild);
    const lifecycle::Scope trace("ir.build", this);
    std::shared_ptr<Asset> asset;
    { std::lock_guard<std::mutex> lock(mutex);
      if(source==1 || source==2) asset=factory[(size_t)source-1];
      else if(source==3) asset=users[(size_t)lane];
    }
    juce::AudioBuffer<float> samples;
    std::shared_ptr<const ModeledCabConvolution::Prepared> prepared;
    double rate=spec.sampleRate;
    if(model) {
        auto found=std::find_if(modelCache.begin(),modelCache.end(),[&](const auto& item){return item.key==model;});
        if(found==modelCache.end()) {
            SF_CAB_PROFILE_SCOPE(ResponseGenerate);
            auto response=cabLayout::generate(model,rate);
            juce::AudioBuffer<float> generated(1,int(response.size()));
            generated.copyFrom(0,0,response.data(),int(response.size()));
            if(modelCache.size()==8)modelCache.pop_front();
            modelCache.push_back({model,std::move(generated),{}});found=std::prev(modelCache.end());
        }
        // Reuse only immutable spectra. The cache is cleared on every prepare,
        // so its model key is also bound to the current sample rate and block
        // size. Every published microphone still owns private input/history.
        if(Cab::Kernel::supportsPreparedModel(spec)) {
            if(!found->prepared)found->prepared=std::make_shared<const ModeledCabConvolution::Prepared>(found->samples,int(spec.maximumBlockSize),spec.numChannels==1);
            prepared=found->prepared;
        }
        samples.makeCopyOf(found->samples);
    }
    else if(asset) { samples.makeCopyOf(asset->samples); rate=asset->rate; }
    else { samples.setSize(1,1); samples.setSample(0,0,1.f); }
    auto kernel=std::make_unique<Cab::Kernel>(std::move(samples),rate,spec,source,generation,model,std::move(prepared));
    kernel->hasIR = model != 0 || asset != nullptr;
    return kernel;
}
void IRLibrary::prepare(const juce::dsp::ProcessSpec& settings,const std::array<int,3>& sources)
{
    const lifecycle::Scope trace("ir.prepare", this);
    stop(); spec=settings; modelCache.clear();
    for(int i=0;i<6;++i)
    {
        cabs[i]->requestedSource.store((i<3 ? sources[i] : cabs[i]->requestedSource.load()));
        unsigned generation;
        { std::lock_guard<std::mutex> lock(mutex); generation=generations[i]; }
        cabs[i]->install(build(i,(i<3 ? sources[i] : cabs[i]->requestedSource.load()),generation,cabs[i]->requestedModel.load()));
    }
    startThread();
}
void IRLibrary::run()
{
#if defined(CHIMERA_CAB_PROFILE) && CHIMERA_CAB_PROFILE
    cabProfile::ThreadCapture profileCapture(profileSink);
#endif
    const lifecycle::Scope trace("ir.worker", this);
    std::array<int,6> built;
    std::array<unsigned,6> versions, rejections{};
    std::array<uint64_t,6> models{};
    for(int i=0;i<6;++i) { built[i]=cabs[i]->activeSource.load(); versions[i]=cabs[i]->activeGeneration.load(); models[i]=cabs[i]->activeModel.load(); rejections[i]=cabs[i]->rejectedModels.load(); }
    while(!threadShouldExit())
    {
        for(int i=0;i<6 && !threadShouldExit();++i)
        {
            cabs[i]->collect();
            const int source=cabs[i]->requestedSource.load();
            unsigned generation;
            { std::lock_guard<std::mutex> lock(mutex); generation=generations[i]; }
            const auto model=cabs[i]->requestedModel.load();
            const auto rejected=cabs[i]->rejectedModels.load();
            if(rejected==rejections[i] && source==built[i] && model==models[i] && (source!=3 || generation==versions[i])) continue;
            try {
                auto kernel=build(i,source,generation,model);
                if(threadShouldExit())break;
                // Coalesce rapid automation; no stale response is published.
                if(source!=cabs[i]->requestedSource.load() || model!=cabs[i]->requestedModel.load())continue;
                { std::lock_guard<std::mutex> lock(mutex); if(generation!=generations[i])continue; errors[i].clear(); }
                cabs[i]->publish(std::move(kernel));
                built[i]=source; versions[i]=generation; models[i]=model; rejections[i]=rejected;
            }
            catch(const std::exception&) { std::lock_guard<std::mutex> lock(mutex); errors[i]="IR preparation failed. Previous IR kept."; ++displayGeneration[(size_t)i]; }
        }
        wait(20);
    }
    for(auto* cab:cabs) cab->collect();
}
juce::String IRLibrary::userName(int lane) const
{
    std::lock_guard<std::mutex> lock(mutex);
    return users[(size_t)lane] ? users[(size_t)lane]->name : juce::String{};
}
juce::String IRLibrary::status(int lane) const
{
    std::lock_guard<std::mutex> lock(mutex);
    if(errors[lane].isNotEmpty()) return errors[lane];
    const auto model=cabs[lane]->requestedModel.load();
    if(model) {
        const auto p=originalCab::settings(model);
        if(model&cabLayout::versionBit) {
            const auto p3=cabLayout::settings(model);const auto* d=cabExpansion::driver(p3.voice);const auto* mic=cabExpansion::microphone(p3.voice);
            return juce::String(d ? d->name : p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12")+" | "+juce::String(cabLayout::count(p3.layout))+"x"+juce::String(cabExpansion::diameter(p3.voice))
                +" | "+(mic ? mic->name : p.mic==0 ? "Attack Dynamic" : p.mic==1 ? "Body Ribbon" : "Detail Condenser")+" | Unit "+juce::String(cabLayout::effectiveUnit(p3)+1)
                +(model!=cabs[lane]->activeModel.load() ? " | Preparing..." : " | Ready");
        }
        if(model&cabExpansion::versionBit) {
            const auto x=cabExpansion::settings(model);const auto* d=cabExpansion::driver(x);const auto* m=cabExpansion::microphone(x);
            return juce::String(d ? d->name : p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12")
                +" | "+(m ? m->name : p.mic==0 ? "Attack Dynamic" : p.mic==1 ? "Body Ribbon" : "Detail Condenser")+" | Unit "+juce::String(p.unit+1)
                +(model!=cabs[lane]->activeModel.load() ? " | Preparing..." : " | Ready");
        }
        return juce::String(p.cabinet ? "Bass 4x10" : "Guitar 4x12")
            +" | Modeled / unit "+juce::String(p.unit+1)
            +(model!=cabs[lane]->activeModel.load() ? " | Preparing..." : " | Ready");
    }
    const int source=cabs[lane]->requestedSource.load();
    if(source==3 && !users[lane]) return "No user IR loaded. Filters only.";
    const bool loading=cabs[lane]->activeSource.load()!=source ||
        (source==3 && cabs[lane]->activeGeneration.load()!=generations[lane]);
    if(loading) return source==3 && users[lane] ? users[lane]->metadata.shortLabel(users[lane]->name)+" | Preparing IR..." : "Preparing IR...";
    if(source==0) return "Filters only | no speaker IR";
    const auto asset=source==3 ? users[lane] : factory[(size_t)source-1];
    if(!asset) return "Factory IR unavailable";
    return asset->metadata.shortLabel(asset->name) + " | " + juce::String(juce::roundToInt(1000.0*asset->samples.getNumSamples()/asset->rate))+" ms";
}
juce::ValueTree IRLibrary::save() const
{
    juce::ValueTree tree("USER_IRS");
    std::lock_guard<std::mutex> lock(mutex);
    for(int i=0;i<6;++i) if(users[i])
    {
        juce::ValueTree child("IR");
        child.setProperty("lane",i%3,nullptr); child.setProperty("slot",i/3,nullptr); child.setProperty("name",users[i]->name,nullptr);
        child.setProperty("data",users[i]->encoded.toBase64Encoding(),nullptr);
        child.setProperty("metadata",juce::JSON::toString(users[i]->metadata.json(),true),nullptr);
        tree.appendChild(child,nullptr);
    }
    return tree;
}
void IRLibrary::restore(const juce::ValueTree& tree)
{
    std::array<std::shared_ptr<Asset>,6> restored;
    std::array<juce::String,6> messages;
    for(auto child:tree)
    {
        const int base=(int)child.getProperty("lane",-1), slot=(int)child.getProperty("slot",0);
        if(base<0 || base>2 || slot<0 || slot>1) continue;
        const int lane=base+3*slot;
        if(lane<0 || lane>5) continue;
        const auto encoded=child.getProperty("data").toString();
        juce::MemoryBlock bytes;
        if(encoded.length()>6*1024*1024 || !bytes.fromBase64Encoding(encoded)) messages[lane]="Saved IR is damaged. Filters only.";
        else restored[lane]=decode(bytes,child.getProperty("name").toString(),messages[lane]);
        if(restored[lane] && child.hasProperty("metadata"))restored[lane]->metadata=IRMetadata::fromJSON(juce::JSON::parse(child.getProperty("metadata").toString()));
    }
    { std::lock_guard<std::mutex> lock(mutex);
      users=std::move(restored); errors=std::move(messages);
      for(auto& generation:generations) ++generation;
      for(auto& generation:displayGeneration) ++generation;
    }
    notify();
}
IRMetadata IRLibrary::metadata(int lane,int source,bool includeModeled) const
{
    std::lock_guard<std::mutex> lock(mutex);if(lane<0 || lane>5)return {};
    if(const auto model=includeModeled ? cabs[lane]->requestedModel.load() : 0) {
        const auto p=originalCab::settings(model);IRMetadata m;
        if(model&cabLayout::versionBit) {
            const auto p3=cabLayout::settings(model);const auto g=cabLayout::geometry(p3);
            const auto* d=cabExpansion::driver(p3.voice);const auto* mic=cabExpansion::microphone(p3.voice);
            m.instrument=cabExpansion::isBass(p3.voice) ? IRMetadata::Instrument::bass : IRMetadata::Instrument::guitar;
            m.displayLabel=juce::String(d ? d->name : p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12")+" "+juce::String(g.count)+"x"+juce::String(cabExpansion::diameter(p3.voice));
            m.values[0]=d ? d->name : p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12";m.values[1]=m.displayLabel;
            m.values[3]=mic ? mic->name : p.mic==0 ? "Attack Dynamic" : p.mic==1 ? "Body Ribbon" : "Detail Condenser";
            m.values[4]="Modeled unit "+juce::String(cabLayout::effectiveUnit(p3)+1)+", radius "+juce::String(p.position,3);
            m.values[8]="Chimera original design";
            m.values[11]="Independent linear array model; "+juce::String(g.box.volume*1000,1)+" L net volume; "
                +juce::String(p.distanceCm,1)+" cm. No measured hardware matching, ports or room reverb.";return m;
        }
        if(model&cabExpansion::versionBit) {
            const auto x=cabExpansion::settings(model);const auto* d=cabExpansion::driver(x);const auto* mic=cabExpansion::microphone(x);
            m.instrument=cabExpansion::isBass(x) ? IRMetadata::Instrument::bass : IRMetadata::Instrument::guitar;
            m.displayLabel=juce::String(d ? d->name : p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12")+" 4x"+juce::String(cabExpansion::diameter(x));
            m.values[0]=d ? d->name : p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12";m.values[1]=m.displayLabel;
            m.values[3]=mic ? mic->name : p.mic==0 ? "Attack Dynamic" : p.mic==1 ? "Body Ribbon" : "Detail Condenser";
            m.values[4]="Modeled unit "+juce::String(p.unit+1)+", radius "+juce::String(p.position,3);
            m.values[8]="Chimera original design";
            m.values[11]="Independent linear model; not a measured hardware clone. Distance "+juce::String(p.distanceCm,1)+" cm. Four identical drivers; no third-party IR fitting.";
            return m;
        }
        m.instrument=p.cabinet ? IRMetadata::Instrument::bass : IRMetadata::Instrument::guitar;
        m.displayLabel=p.cabinet ? "Bass 4x10" : "Guitar 4x12";
        m.values[0]=p.cabinet ? "Chimera Bass 10" : "Chimera Guitar 12";m.values[1]=m.displayLabel;
        m.values[3]=p.mic==0 ? "Attack Dynamic" : p.mic==1 ? "Body Ribbon" : "Detail Condenser";
        m.values[4]="Modeled unit "+juce::String(p.unit+1)+", radius "+juce::String(p.position,3);
        m.values[8]="Chimera original design";
        m.values[11]="Authored linear acoustic approximation, not a hardware measurement. Distance "+juce::String(p.distanceCm,1)+" cm from cone plane. No third-party IR fitting.";
        return m;
    }
    auto asset=source==3 ? users[(size_t)lane] : source==1 || source==2 ? factory[(size_t)source-1] : nullptr;
    return asset ? asset->metadata : IRMetadata{};
}
void IRLibrary::setMetadata(int lane,const IRMetadata& metadata)
{
    std::lock_guard<std::mutex> lock(mutex);if(lane>=0 && lane<6 && users[(size_t)lane]){users[(size_t)lane]->metadata=IRMetadata::fromJSON(metadata.json());++displayGeneration[(size_t)lane];}
}
}

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
    for(auto& cached:kernelCache)cached.clear();
}
bool IRLibrary::resourcesReleased() const noexcept
{
    if (isThreadRunning()) return false;
    for(const auto& cached:kernelCache)if(!cached.empty())return false;
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
      if(asset) { users[(size_t)lane]=std::move(asset); ++generations[(size_t)lane];cabs[(size_t)lane]->requestedGeneration.store(generations[(size_t)lane]); }
      ++displayGeneration[(size_t)lane];
    }
    if(error.isNotEmpty()) return juce::Result::fail(error);
    notify(); return juce::Result::ok();
}
bool IRLibrary::requestIsCurrent(int lane,int source,uint64_t model) const noexcept
{
    return source==cabs[lane]->requestedSource.load() && model==cabs[lane]->requestedModel.load();
}
std::unique_ptr<Cab::Kernel> IRLibrary::build(int lane,int source,unsigned generation,uint64_t model,
                                           bool cancelObsolete)
{
    SF_CAB_PROFILE_SCOPE(WorkerBuild);
    const lifecycle::Scope trace("ir.build", this);
    struct Request {const IRLibrary* library;int lane,source;uint64_t model;};
    const Request request{this,lane,source,model};
    // prepare() synchronously builds the saved initial state after stop(). It
    // deliberately has no cancellation token. Only the running worker abandons
    // requests using the same source/model predicate as its publication check.
    const originalCab::GenerationCancellation cancellation{&request,cancelObsolete
        ? +[](const void* context) {
            const auto& expected=*static_cast<const Request*>(context);
            // A modeled response does not depend on the captured-source slot.
            // Preserve completed model work when recall writes source after model;
            // the final publication check still requires the full exact identity.
            return expected.library->threadShouldExit()
                || (expected.model!=0
                    ? expected.library->cabs[expected.lane]->requestedModel.load()!=expected.model
                    : !expected.library->requestIsCurrent(expected.lane,expected.source,expected.model));
        } : nullptr};
    cancellation.checkpoint();
    std::shared_ptr<Asset> asset;
    { std::lock_guard<std::mutex> lock(mutex);
      if(source==1 || source==2) asset=factory[(size_t)source-1];
      else if(source==3) asset=users[(size_t)lane];
    }
    juce::AudioBuffer<float> samples;
    std::shared_ptr<const ModeledCabConvolution::Prepared> prepared;
    CachedModel generated{model,{},{}};
    double rate=spec.sampleRate;
    if(model) {
        bool found=false;
        { std::lock_guard<std::mutex> lock(modelCacheMutex);
          const auto item=std::find_if(modelCache.begin(),modelCache.end(),[&](const auto& entry){return entry.key==model;});
          if(item!=modelCache.end()) {
              generated.samples.makeCopyOf(item->samples);generated.prepared=item->prepared;found=true;
          }
        }
        if(!found) {
            SF_CAB_PROFILE_SCOPE(ResponseGenerate);
            auto response=cabLayout::generate(model,rate,cancellation);
            generated.samples.setSize(1,int(response.size()));
            generated.samples.copyFrom(0,0,response.data(),int(response.size()));
        }
        cancellation.checkpoint();
        // Reuse only immutable spectra. The cache is cleared on every prepare,
        // so its model key is also bound to the current sample rate and block
        // size. Every published microphone still owns private input/history.
        if(Cab::Kernel::supportsPreparedModel(spec)) {
            prepared=generated.prepared;
            if(!prepared)prepared=std::make_shared<const ModeledCabConvolution::Prepared>(generated.samples,int(spec.maximumBlockSize),spec.numChannels==1);
        }
        cancellation.checkpoint();
        samples.makeCopyOf(generated.samples);
    }
    else if(asset) { samples.makeCopyOf(asset->samples); rate=asset->rate; }
    else { samples.setSize(1,1); samples.setSample(0,0,1.f); }
    auto kernel=std::make_unique<Cab::Kernel>(std::move(samples),rate,spec,source,generation,model,prepared);
    kernel->hasIR = model != 0 || asset != nullptr;
    cancellation.checkpoint();
    if(model) {
        generated.prepared=std::move(prepared);
        std::lock_guard<std::mutex> lock(modelCacheMutex);
        const auto found=std::find_if(modelCache.begin(),modelCache.end(),[&](const auto& item){return item.key==model;});
        if(found!=modelCache.end())modelCache.erase(found);
        modelCache.push_back(std::move(generated));
        // Two complete six-mic A/B states; rate/block/channel changes clear it.
        const size_t capacity=(preparationPolicy==PreparationPolicy::audibleFirst
            || preparationPolicy==PreparationPolicy::audibleCached) ? 12 : 8;
        if(modelCache.size()>capacity)modelCache.pop_front();
    }
    return kernel;
}
void IRLibrary::prepare(const juce::dsp::ProcessSpec& settings,const std::array<int,3>& sources)
{
    const lifecycle::Scope trace("ir.prepare", this);
    stop(); spec=settings; modelCache.clear();cancelledBuilds.store(0,std::memory_order_relaxed);cacheHits=0;scheduling={};
    for(int i=0;i<6;++i)
    {
        cabs[i]->requestedSource.store((i<3 ? sources[i] : cabs[i]->requestedSource.load()));
        unsigned generation;
        { std::lock_guard<std::mutex> lock(mutex); generation=generations[i];cabs[i]->requestedGeneration=generation; }
        cabs[i]->install(build(i,(i<3 ? sources[i] : cabs[i]->requestedSource.load()),generation,cabs[i]->requestedModel.load()));
    }
    // IR generation/preparation is background work. Let the host's processing
    // thread preempt it; do not change the caller/process priority or affinity.
    // JUCE maps low to Windows LOWEST and macOS UTILITY. Linux ignores this
    // non-realtime priority request, so it must not be credited there as a fix.
    scheduling.startSucceeded=startThread(Priority::low);
}
void IRLibrary::run()
{
    scheduling.observedPriority=int(getPriority());
    scheduling.enteredRun=true;
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
        std::array<int,6> order{0,1,2,3,4,5};
        std::array<bool,6> audible{};
        for(size_t i=0;i<audible.size();++i)audible[i]=cabs[i]->audiblePriority.load();
        if(preparationPolicy!=PreparationPolicy::serial)
            std::sort(order.begin(),order.end(),[&](int a,int b){return audible[a]!=audible[b] ? audible[a] : a<b;});
        for(int i=0;i<6;++i) {
            if(auto old=cabs[i]->takeRetired()) {
                if(preparationPolicy==PreparationPolicy::audibleCached && old->modelKey!=0) {
                    auto& cache=kernelCache[i];cache.push_back(std::move(old));
                    while(cache.size()>2)cache.pop_front(); // destruction is worker-only
                }
            }
        }
        const auto needsWork=[&](int i) {
            unsigned generation;
            {std::lock_guard<std::mutex> lock(mutex);generation=generations[i];}
            return cabs[i]->requestedSource.load()!=built[i] || cabs[i]->requestedModel.load()!=models[i]
                || cabs[i]->rejectedModels.load()!=rejections[i]
                || (cabs[i]->requestedSource.load()==3 && generation!=versions[i]);
        };
        size_t workCount=0;
        for(int i:order)if(needsWork(i))order[workCount++]=i;
        const auto service=[&](int i) {
            const int source=cabs[i]->requestedSource.load();
            unsigned generation;
            { std::lock_guard<std::mutex> lock(mutex); generation=generations[i]; }
            const auto model=cabs[i]->requestedModel.load();
            const auto rejected=cabs[i]->rejectedModels.load();
            if(rejected==rejections[i] && source==built[i] && model==models[i] && (source!=3 || generation==versions[i])) return;
            try {
                std::unique_ptr<Cab::Kernel> kernel;
                if(preparationPolicy==PreparationPolicy::audibleCached) {
                    auto& cache=kernelCache[i];
                    const auto found=std::find_if(cache.begin(),cache.end(),[&](const auto& k){
                        return k->source==source && k->modelKey==model
                            && (model!=0 || source!=3 || k->generation==generation);
                    });
                    if(found!=cache.end()) {
                        kernel=std::move(*found);cache.erase(found);
                        kernel->reset();kernel->generation=generation;++cacheHits;
                    }
                }
                if(!kernel)kernel=build(i,source,generation,model,true);
                if(threadShouldExit() || !requestIsCurrent(i,source,model))return;
                { std::lock_guard<std::mutex> lock(mutex); if(generation!=generations[i])return; errors[i].clear(); }
                cabs[i]->publish(std::move(kernel));
                built[i]=source;versions[i]=generation;models[i]=model;rejections[i]=rejected;
            }
            catch(const originalCab::GenerationCancelled&) {cancelledBuilds.fetch_add(1,std::memory_order_relaxed);}
            catch(const std::exception&) {std::lock_guard<std::mutex> lock(mutex);errors[i]="IR preparation failed. Recall remains muted until a valid cabinet is ready.";++displayGeneration[(size_t)i];}
        };
        for(size_t n=0;n<workCount && !threadShouldExit();) {
            const int i=order[n++];
            // Experimental only: at most two outstanding preparations, joined
            // off the audio thread. No concurrent work on the same mic/cache.
            if(preparationPolicy==PreparationPolicy::parallel && n<workCount) {
                const int j=order[n++];
                auto second=std::async(std::launch::async,[&service,j]{service(j);});
                service(i);second.get();
            } else service(i);
        }
        wait(preparationPolicy==PreparationPolicy::serial ? 20 : 2); // bounded poll; atomic requests originate on the audio thread
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
    if(source==3 && !users[lane]) return "No user IR loaded. Recall requires a valid IR.";
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
        if(encoded.length()>6*1024*1024 || !bytes.fromBase64Encoding(encoded)) messages[lane]="Saved IR is damaged. Recall requires a valid IR.";
        else restored[lane]=decode(bytes,child.getProperty("name").toString(),messages[lane]);
        if(restored[lane] && child.hasProperty("metadata"))restored[lane]->metadata=IRMetadata::fromJSON(juce::JSON::parse(child.getProperty("metadata").toString()));
    }
    { std::lock_guard<std::mutex> lock(mutex);
      users=std::move(restored); errors=std::move(messages);
      for(size_t i=0;i<generations.size();++i) {++generations[i];cabs[i]->requestedGeneration.store(generations[i]);}
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

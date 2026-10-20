#include "PluginProcessor.h"
#include "IRStateAssets.h"
#include "FullGraphAdapter.h"
#include "AllocationProbe.h"
#include "FullGraphBuild.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <thread>

namespace {
void require(bool b, const juce::String& why) { if (!b) throw std::runtime_error(why.toStdString()); }
int integer(const char* s, int lo, int hi) {
    std::size_t used = 0; const auto text = std::string(s); const int n = std::stoi(text, &used);
    require(used == text.size() && n >= lo && n <= hi, "Invalid integer argument"); return n;
}
void save(const juce::File& file, const juce::var& value) {
    require(file.replaceWithText(juce::JSON::toString(value)), "Cannot write JSON");
}
float value(ChimeraProcessor& p, const juce::String& id) {
    auto* v = p.parameters().getRawParameterValue(id); require(v != nullptr, "Unknown parameter "+id); return v->load();
}
void set(ChimeraProcessor& p, const juce::String& id, float v) {
    auto* parameter = p.parameters().getParameter(id); require(parameter != nullptr, "Unknown parameter "+id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(v));
}
juce::String parameterHash(ChimeraProcessor& p) {
    juce::MemoryBlock bytes;
    for (auto* parameter : p.getParameters()) {const float v=parameter->getValue();bytes.append(&v,sizeof(v));}
    return juce::SHA256(bytes).toHexString();
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        // Explicit fixture authoring, never silently replace a supplied preset.
        if (argc == 6 && juce::String(argv[1]) == "--make-state") {
            auto fixture=std::make_unique<ChimeraProcessor>();
            auto& p=*fixture;
            const int mode = integer(argv[3],0,2), shift = integer(argv[4],-2,0);
            const int crossover = integer(argv[5],0,1);
            set(p,"mode",float(mode));set(p,"dualtype",float(crossover));
            set(p,"oversampling",2);set(p,"transpose",float(shift));set(p,"transposeon",shift != 0 ? 1.f : 0.f);
            set(p,"tuneron",0);set(p,"tunermute",0);set(p,"gateon",0);
            for(int lane=1;lane<=mode+1;++lane) {
                set(p,"ampon"+juce::String(lane),1);set(p,"cab"+juce::String(lane),1);
                set(p,"mute"+juce::String(lane),0);set(p,"solo"+juce::String(lane),0);
            }
            juce::MemoryBlock state;p.getStateInformation(state);
            const juce::File file(argv[2]);require(!file.exists(),"State output already exists");
            require(file.replaceWithData(state.getData(),state.getSize()),"Cannot save fixture state");return 0;
        }
        require(argc == 9, "Usage: ChimeraFullGraphHost DI.wav state.chimera common-ir.wav NEW-output instances(1/2/4) block(128/256) seconds(1..1800) warmup(1..60)");
        require(juce::String(CHIMERA_PERF_CONFIG) == "Release", "Use a Release build");
        require(juce::SystemStats::getEnvironmentVariable("CHIMERA_LIFECYCLE_TRACE",{}).isEmpty(), "Disable lifecycle trace for timing");
        const juce::File inputFile(argv[1]), stateFile(argv[2]), irFile(argv[3]), out(argv[4]);
        const int instances=integer(argv[5],1,4), block=integer(argv[6],128,256);
        require(instances==1 || instances==2 || instances==4,"Instance count must be 1/2/4");
        require(block==128 || block==256,"Block must be 128/256");
        const int seconds=integer(argv[7],1,1800), warmup=integer(argv[8],1,60);
        require(inputFile.existsAsFile() && stateFile.existsAsFile() && irFile.existsAsFile() && !out.exists(),"Missing input/state/IR or existing output");
        const auto inputHash=juce::SHA256(inputFile).toHexString(), stateHash=juce::SHA256(stateFile).toHexString(), irHash=juce::SHA256(irFile).toHexString();
        juce::WavAudioFormat wav;
        auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(inputFile.createInputStream().release(),true));
        require(reader && reader->sampleRate==48000 && reader->lengthInSamples>0 && reader->lengthInSamples<=48000*600
                && (reader->numChannels==1 || reader->numChannels==2),"Use a finite mono/stereo 48 kHz WAV <=600s");
        juce::AudioBuffer<float> input(2,int(reader->lengthInSamples));
        require(reader->read(&input,0,input.getNumSamples(),0,true,reader->numChannels==2),"Cannot read input");
        if(reader->numChannels==1)input.copyFrom(1,0,input,0,0,input.getNumSamples());
        double inputEnergy=0;
        for(int c=0;c<2;++c)for(int n=0;n<input.getNumSamples();++n) {
            const float x=input.getSample(c,n);require(std::isfinite(x),"Nonfinite input");inputEnergy+=double(x)*x;
        }
        require(inputEnergy>0,"Silent input");
        juce::MemoryBlock state;require(stateFile.loadFileAsData(state),"Cannot read state");
        const auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
        require(state.getSize()<=64*1024*1024 && xml != nullptr && xml->hasTagName("PARAMS"),"Invalid Chimera state root");
        auto decoded=juce::ValueTree::fromXml(*xml);
        require(spectralforge::irState::unpack(decoded),"Invalid packed IR state");
        std::vector<std::unique_ptr<ChimeraProcessor>> owned;
        std::vector<ChimeraProcessor*> nodes;
        std::vector<juce::String> hashes;
        require(out.createDirectory().wasOk(),"Cannot create output");
        juce::Array<juce::var> states;
        int mode=0, shift=0, crossover=0;
        for(int i=0;i<instances;++i) {
            auto p=std::make_unique<ChimeraProcessor>();p->setStateInformation(state.getData(),int(state.getSize()));
            // Require a complete same-version state and read it back. A valid
            // XML root alone must not silently measure constructor defaults.
            for(auto* parameter:p->getParameters()) {
                auto* named=dynamic_cast<juce::RangedAudioParameter*>(parameter);
                require(named!=nullptr,"Unidentified parameter");
                const auto item=decoded.getChildWithProperty("id",named->paramID);
                require(item.isValid() && item.hasProperty("value"),"Incomplete state: "+named->paramID);
                const float expected=float(item["value"]);
                require(std::isfinite(expected) && std::abs(value(*p,named->paramID)-expected)<1e-4f,
                        "State readback mismatch: "+named->paramID);
            }
            require(p->betaProcessingPermitted(),"Product beta policy does not permit processing");
            mode=int(value(*p,"mode"));shift=int(value(*p,"transpose"));crossover=int(value(*p,"dualtype"));
            require(value(*p,"oversampling")==2 && value(*p,"tunermute")==0,"State must select 4x and unmuted tuner");
            require((shift==0 && value(*p,"transposeon")==0) || (shift==-2 && value(*p,"transposeon")==1),"Use transpose bypass or -2");
            for(int lane=0;lane<=mode;++lane) {
                const auto suffix=juce::String(lane+1);
                require(value(*p,"ampon"+suffix)==1 && value(*p,"cab"+suffix)==1 &&
                        value(*p,"mute"+suffix)==0 && value(*p,"solo"+suffix)==0,"Active amp/CAB paths required");
                // A declared common-IR workload: disable modeled mic recipes,
                // replace A capture on each active lane and set B mix to zero.
                const auto result=p->loadIR(lane,irFile);require(result.wasOk(),result.getErrorMessage());
                set(*p,spectralforge::originalCabID(lane,"Bon"),0);
                set(*p,"cabBtype"+suffix,0);set(*p,"cabblend"+suffix,0);
            }
            p->setRateAndBufferSizeDetails(48000,block);p->prepareToPlay(48000,block);
            hashes.push_back(parameterHash(*p));
            juce::MemoryBlock resolved;require(p->tryGetStateInformation(resolved),"Cannot serialize resolved state");
            const auto resolvedXml=juce::AudioProcessor::getXmlFromBinary(resolved.getData(),int(resolved.getSize()));
            require(resolvedXml!=nullptr,"Invalid resolved state");
            auto actual=juce::ValueTree::fromXml(*resolvedXml);
            require(spectralforge::irState::unpack(actual),"Invalid resolved IR table");
            for(int lane=0;lane<=mode;++lane) {
                bool found=false;
                for(auto capture:actual.getChildWithName("USER_IRS"))
                    if(int(capture["lane"])==lane && int(capture["slot"])==0) {
                        juce::MemoryBlock bytes;
                        require(bytes.fromBase64Encoding(capture["data"].toString())
                                && juce::SHA256(bytes).toHexString()==irHash,"Loaded IR bytes mismatch");
                        found=true;
                    }
                require(found,"Loaded IR binding missing");
            }
            const auto saved=out.getChildFile("resolved-"+juce::String(i)+".chimera");
            require(saved.replaceWithData(resolved.getData(),resolved.getSize()),"Cannot save resolved state");
            states.add(juce::SHA256(saved).toHexString());nodes.push_back(p.get());owned.push_back(std::move(p));
        }
        chimera_perf::FullGraphAdapter<ChimeraProcessor> graph(nodes,input,block);
        const int count=int(std::ceil(seconds*48000.0/block)), warmCount=int(std::ceil(warmup*48000.0/block));
        chimera_perf::CallbackRecorder recorder(std::size_t(count)+1), empty(10000), clocks(10000);
        // Probe self-test prevents a silently ineffective replacement new.
        chimera_perf::allocationScope=true;
        void* probe=::operator new(32);::operator delete(probe);
        chimera_perf::allocationScope=false;
        require(chimera_perf::allocationCount==1,"Allocation probe self-test failed");
        chimera_perf::allocationCount=0;
        for(int i=0;i<10000;++i) {
            chimera_perf::allocationScope=true;
            const auto a=clocks.begin();clocks.end(a,block);
            const auto b=empty.begin();graph.process(true);empty.end(b,block);
            chimera_perf::allocationScope=false;
        }
        require(chimera_perf::allocationCount==0,"Adapter/control allocated");
        // Normal-priority synthetic scheduler. Fixed epoch includes late wakeups.
        auto epoch=chimera_perf::wallNs();
        for(int i=0;i<warmCount;++i) {
            std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(epoch+std::int64_t(i)*block*1000000000LL/48000)));
            graph.process();
        }
        std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(epoch+std::int64_t(warmCount)*block*1000000000LL/48000)));
        const auto warmElapsed=(chimera_perf::wallNs()-epoch)/1e9;
        // Handshake lets the Python sampler capture the same PID before timing.
        require(out.getChildFile("ready").replaceWithText("ready"),"Cannot write ready");
        const auto timeout=chimera_perf::wallNs()+30000000000LL;
        while(!out.getChildFile("go").existsAsFile()) {
            require(chimera_perf::wallNs()<timeout,"Sampler handshake timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        epoch=chimera_perf::wallNs();
        for(int i=0;i<count;++i) {
            const auto start=epoch+std::int64_t(i)*block*1000000000LL/48000;
            const auto deadline=epoch+std::int64_t(i+1)*block*1000000000LL/48000;
            std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(start)));
            chimera_perf::allocationScope=true;
            const auto stamp=recorder.begin();graph.process();recorder.end(stamp,block,deadline);
            chimera_perf::allocationScope=false;
            graph.observe();
        }
        const auto elapsed=(chimera_perf::wallNs()-epoch)/1e9;
        recorder.writeCsv(out.getChildFile("callbacks.csv").getFullPathName().toRawUTF8());
        empty.writeCsv(out.getChildFile("empty-graph.csv").getFullPathName().toRawUTF8());
        clocks.writeCsv(out.getChildFile("clock-only.csv").getFullPathName().toRawUTF8());
        auto report=std::make_unique<juce::DynamicObject>();
        report->setProperty("schema","chimera.full-graph.v1");report->setProperty("synthetic",true);
        report->setProperty("requirements_certified",false);report->setProperty("publication_allowed",false);
        report->setProperty("source_sha",CHIMERA_PERF_SOURCE);report->setProperty("source_fingerprint",CHIMERA_PERF_FINGERPRINT);
        report->setProperty("build_config",CHIMERA_PERF_CONFIG);report->setProperty("version",CHIMERA_PERF_VERSION);
        report->setProperty("binary_sha256",juce::SHA256(juce::File::getSpecialLocation(juce::File::currentExecutableFile)).toHexString());
        report->setProperty("input_sha256",inputHash);report->setProperty("state_sha256",stateHash);report->setProperty("ir_sha256",irHash);
        report->setProperty("resolved_state_sha256",states);report->setProperty("instances",instances);
        report->setProperty("mode",mode);report->setProperty("transpose",shift);report->setProperty("dualtype",crossover);
        report->setProperty("block",block);report->setProperty("sample_rate",48000);report->setProperty("oversampling",4);
        report->setProperty("warmup_audio_seconds",warmCount*block/48000.0);
        report->setProperty("warmup_wall_seconds",warmElapsed);report->setProperty("elapsed_seconds",elapsed);
        report->setProperty("callback_count",juce::int64(recorder.count()));
        report->setProperty("dropped_rows",juce::int64(recorder.droppedRows()));
        report->setProperty("callback_cpp_allocations",juce::int64(chimera_perf::allocationCount));
        report->setProperty("finite",graph.finite);
        bool stable=true;juce::Array<juce::var> energies,calls;
        for(int i=0;i<instances;++i) {
            stable=stable && hashes[std::size_t(i)]==parameterHash(*nodes[std::size_t(i)]);
            energies.add(graph.energy[std::size_t(i)]);calls.add(juce::int64(graph.calls[std::size_t(i)]));
            nodes[std::size_t(i)]->releaseResources();
        }
        report->setProperty("parameters_unchanged",stable);report->setProperty("instance_energy",energies);report->setProperty("instance_calls",calls);
        save(out.getChildFile("host.json"),juce::var(report.release()));
        require(inputHash==juce::SHA256(inputFile).toHexString() && stateHash==juce::SHA256(stateFile).toHexString() && irHash==juce::SHA256(irFile).toHexString(),"Input files changed");
        require(stable && graph.finite && recorder.droppedRows()==0 && chimera_perf::allocationCount==0,"Invalid run; retained raw evidence");
        for(auto energy:graph.energy)require(energy>0,"Silent instance; retained raw evidence");
        return 0;
    } catch(const std::exception& e) {chimera_perf::allocationScope=false;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

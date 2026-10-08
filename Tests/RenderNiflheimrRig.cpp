// Reuse the established WAV writer and integrity helpers; the head renderer
// remains unchanged and is invoked separately by the audition coordinator.
#define main niflheimrHeadRendererEntry
#include "RenderNiflheimr.cpp"
#undef main
#include "PluginProcessor.h"
#include "NiflheimrPresets.h"

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    try {
        require(argc == 8, "Usage: ChimeraNiflheimrRigRender input.wav NEW-directory common-ir.wav head-cab|preset-cab 1|2|4|8 controls.json");
        const juce::File input(argv[1]), destination(argv[2]), ir(argv[3]), controls(argv[7]);
        const juce::String mode(argv[4]);
        require(mode=="head-cab" || mode=="preset-cab", "Unknown comparison mode");
        const auto rawFactor=number(argv[5],"oversampling");
        const int factor=int(rawFactor);
        require(rawFactor==factor,"Oversampling must be integral");
        const auto rawBlock=number(argv[6],"block-size");
        const int block=int(rawBlock);
        require(rawBlock==block,"Block size must be integral");
        require(factor==1 || factor==2 || factor==4 || factor==8,"Invalid oversampling");
        require(block>=1 && block<=8192,"Invalid block size");
        require(input.existsAsFile() && ir.existsAsFile() && !destination.exists(),"Missing input/IR or existing output");
        const auto config=configuration(controls);
        juce::WavAudioFormat format;
        auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(input.createInputStream().release(),true));
        require(reader && (reader->numChannels==1 || reader->numChannels==2) && reader->lengthInSamples>0
                && reader->lengthInSamples<=reader->sampleRate*600,"Invalid input WAV");
        require(destination.createDirectory().wasOk(),"Cannot create output directory");
        juce::Array<juce::var> rows;
        const auto set=[](ChimeraProcessor& p,const juce::String& id,float value){
            auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Unknown parameter: "+id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        const auto inputHash=juce::SHA256(input).toHexString(), irHash=juce::SHA256(ir).toHexString();
        for(int channel=0;channel<5;++channel) {
            auto processor=std::make_unique<ChimeraProcessor>();
            // Full rig uses authored PRE/amp/POST/gate. Neutral head-cab uses
            // a reset parameter tree, no PRE/POST/gate, and identical knobs.
            if(mode=="preset-cab")processor->loadFactoryPreset(spectralforge::niflheimrPresetStart+channel);
            else {
                for(auto* raw:processor->getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw))p->setValueNotifyingHost(p->getDefaultValue());
                set(*processor,"mode",0);set(*processor,"boardEnabled",0);set(*processor,"gateon",0);
                set(*processor,"output",0);
                processor->setAmpModel(0,spectralforge::niflheimrAmpModel);
                processor->setAmpChannel(0,channel);processor->activateNativeAmp(0);
                const auto native=stateFor(channel,config);
                for(std::size_t c=0;c<spectralforge::niflheimr::controlCount;++c)
                    set(*processor,spectralforge::ampNativeControlID(0,spectralforge::niflheimrAmpModel,int(c),channel),native.values[c]);
                set(*processor,spectralforge::ampNativeInputTrimID(0),0);
                set(*processor,spectralforge::ampNativeOutputLevelID(0),0);
            }
            set(*processor,"input",0);set(*processor,"inputmode",0);set(*processor,"tuneron",0);
            set(*processor,"oversampling",factor==1?0.f:factor==2?1.f:factor==4?2.f:3.f);
            // Replace all cabinet lanes with the same IR and the same cuts.
            // This is a controlled cabinet variant, not the filters-only original.
            for(int lane=0;lane<3;++lane){
                require(processor->loadIR(lane,ir).wasOk(),"Cannot import common IR");
                set(*processor,"cab"+juce::String(lane+1),1);
                set(*processor,"cablow"+juce::String(lane+1),20);
                set(*processor,"cabhigh"+juce::String(lane+1),20000);
            }
            processor->prepareToPlay(reader->sampleRate,block); // synchronously installs kernels
            const int frames=int(reader->lengthInSamples+std::llround(reader->sampleRate*2));
            const auto filename="CH"+juce::String(channel+1)+".wav";
            const auto writing=destination.getChildFile(filename+".writing");
            FloatWav wav(writing,2,int(reader->sampleRate),frames,block);
            juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer midi;
            for(int offset=0;offset<frames;offset+=block){
                const int count=std::min(block,frames-offset);buffer.setSize(2,count,false,false,true);buffer.clear();
                const int available=int(std::max<juce::int64>(0,std::min<juce::int64>(count,reader->lengthInSamples-offset)));
                if(available>0){require(reader->read(&buffer,0,available,offset,true,reader->numChannels==2),"Input read failed");
                    if(reader->numChannels==1)buffer.copyFrom(1,0,buffer,0,0,available);}
                for(int c=0;c<2;++c)for(int n=0;n<count;++n)require(std::isfinite(buffer.getSample(c,n)),"Nonfinite input");
                processor->processBlock(buffer,midi);
                for(int c=0;c<2;++c)for(int n=0;n<count;++n)require(std::isfinite(buffer.getSample(c,n)),"Nonfinite output");
                wav.write(buffer);
            }
            wav.finish();
            const auto observed=observeFile(writing,"post_flush",filename);
            verifyObservation(observed,58+juce::int64(frames)*8);
            require(writing.moveFileTo(destination.getChildFile(filename)),"Cannot publish WAV");
            verifyObservation(observeFile(destination.getChildFile(filename),"post_publish"),58+juce::int64(frames)*8,observed["sha256"].toString());
            auto row=std::make_unique<juce::DynamicObject>();row->setProperty("file",filename);
            row->setProperty("sha256",observed["sha256"]);row->setProperty("channel_index",channel);
            row->setProperty("latency_samples",processor->getLatencySamples());
            row->setProperty("diagnostics",juce::JSON::parse(processor->diagnosticReport()));
            juce::MemoryBlock state;processor->getStateInformation(state);
            const auto stateFile=destination.getChildFile("CH"+juce::String(channel+1)+".chimera");
            require(stateFile.replaceWithData(state.getData(),state.getSize()),"Cannot save resolved rig state");
            row->setProperty("state_sha256",juce::SHA256(stateFile).toHexString());
            rows.add(juce::var(row.release()));processor->releaseResources();
        }
        require(juce::SHA256(input).toHexString()==inputHash && juce::SHA256(ir).toHexString()==irHash,"Input/IR changed");
        auto manifest=std::make_unique<juce::DynamicObject>();
        manifest->setProperty("schema","spectralforge.niflheimr.rig-render.v1");
        manifest->setProperty("mode",mode);manifest->setProperty("configured_git_head",CHIMERA_NIFLHEIMR_GIT_HEAD);
        manifest->setProperty("build",juce::JSON::parse(CHIMERA_NIFLHEIMR_BUILD_JSON));
        manifest->setProperty("rig_renderer_sha256",CHIMERA_NIFLHEIMR_RIG_RENDERER_SHA256);
        manifest->setProperty("processor_sha256",CHIMERA_NIFLHEIMR_PROCESSOR_SHA256);
        manifest->setProperty("input_sha256",inputHash);manifest->setProperty("ir_sha256",irHash);
        manifest->setProperty("cabinet_policy","production Cab; JUCE normalization; common 20 Hz / 20 kHz cuts; no IR trim");
        manifest->setProperty("release_approved",false);manifest->setProperty("musical_acceptance","PENDING");
        manifest->setProperty("outputs",rows);
        require(destination.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(juce::var(manifest.release()),false)),"Cannot write manifest");
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

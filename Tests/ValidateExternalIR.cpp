// CLI validation for user-owned IRs. No external audio is embedded or copied
// into the repository. Usage: ChimeraValidateExternalIR output-directory IR...
#include "ChimeraDSP.h"
#include "IRLibrary.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const juce::String& message) {if(!value)throw std::runtime_error(message.toStdString());}
juce::var object() {return juce::var(new juce::DynamicObject);}
void put(juce::var& value,const juce::Identifier& key,const juce::var& content) {value.getDynamicObject()->setProperty(key,content);}
struct Render {std::vector<float> left;float peak{},otherPeak{};double energy{};int first{-1},last{-1};};
void capture(Render& result,const juce::AudioBuffer<float>& audio,int offset) {
    for(int n=0;n<audio.getNumSamples();++n) {
        const float value=audio.getSample(0,n);
        require(std::isfinite(value) && std::abs(value)<32.f,"Non-finite/unbounded convolution output");
        result.left.push_back(value);result.peak=juce::jmax(result.peak,std::abs(value));result.energy+=double(value)*value;
        if(std::abs(value)>1e-8f) {if(result.first<0)result.first=offset+n;result.last=offset+n;}
        if(audio.getNumChannels()>1)result.otherPeak=juce::jmax(result.otherPeak,std::abs(audio.getSample(1,n)));
    }
}
juce::var metrics(const Render& output) {
    auto result=object();put(result,"peak",output.peak);put(result,"energy",output.energy);
    put(result,"first_above_1e_minus_8",output.first);put(result,"last_above_1e_minus_8",output.last);
    put(result,"inactive_right_peak",output.otherPeak);
    put(result,"render_float32_sha256",juce::SHA256(output.left.data(),output.left.size()*sizeof(float)).toHexString());
    return result;
}
Render renderEngine(spectralforge::Engine& engine,const juce::dsp::ProcessSpec& spec) {
    std::array<spectralforge::LaneState,3> lanes{};
    for(auto& lane:lanes) {lane.ampEnabled=false;lane.cab=true;lane.cabLow=70;lane.cabHigh=9000;}
    juce::AudioBuffer<float> audio((int)spec.numChannels,(int)spec.maximumBlockSize);
    for(int n=0;n<int(spec.sampleRate*.1);n+=audio.getNumSamples()) {
        audio.clear();engine.process(audio,spectralforge::RoutingMode::classic,150,1200,lanes);
    }
    engine.reset();Render output;
    for(int offset=0;offset<int(spec.sampleRate*.4);offset+=audio.getNumSamples()) {
        audio.clear();if(offset==0)audio.setSample(0,0,.25f);
        engine.process(audio,spectralforge::RoutingMode::classic,150,1200,lanes);capture(output,audio,offset);
    }
    require(output.energy>1e-9 && output.last-output.first>8,"Engine did not produce a nontrivial cabinet response");
    require(output.otherPeak<1e-7f,"Cabinet leaked left-only input into the right channel");
    return output;
}
Render renderKernel(const spectralforge::IRLibrary::Asset& asset,const juce::dsp::ProcessSpec& spec) {
    spectralforge::Cab::Kernel kernel(juce::AudioBuffer<float>(asset.samples),asset.rate,spec,3,1);
    juce::AudioBuffer<float> audio((int)spec.numChannels,(int)spec.maximumBlockSize);Render output;
    for(int offset=0;offset<int(spec.sampleRate*.4);offset+=audio.getNumSamples()) {
        audio.clear();if(offset==0)audio.setSample(0,0,.25f);kernel.process(audio);capture(output,audio,offset);
    }
    require(output.energy>1e-9 && output.last-output.first>8,"Kernel did not convolve the imported response");
    require(output.otherPeak<1e-7f,"Kernel leaked left-only input into the right channel");
    return output;
}
juce::var validateCase(const juce::File& source,const juce::MemoryBlock& original,const spectralforge::IRLibrary::Asset& asset,double rate,int channels,int blockSize) {
    const juce::dsp::ProcessSpec spec{rate,(juce::uint32)blockSize,(juce::uint32)channels};
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-owned-IR-probe",{},false);
    struct Cleanup {juce::File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{folder};
    require(folder.createDirectory().wasOk(),"Cannot create private temporary fixture");
    const auto temporary=folder.getChildFile(source.getFileName());require(temporary.replaceWithData(original.getData(),original.getSize()),"Cannot create temporary IR copy");
    spectralforge::Engine engine;engine.prepare(spec);
    spectralforge::IRLibrary library({&engine.cabinet(0),&engine.cabinet(1),&engine.cabinet(2)});
    const auto imported=library.importFile(0,temporary);require(imported.wasOk(),imported.getErrorMessage());
    library.prepare(spec,{3,0,0});library.stop();
    require(engine.cabinet(0).activeSource.load()==3,"Imported user IR did not become active");
    require(engine.cabinet(1).activeSource.load()==0 && engine.cabinet(2).activeSource.load()==0,"Import changed another lane");
    const auto before=renderEngine(engine,spec),direct=renderKernel(asset,spec);
    spectralforge::Engine filtersOnly;filtersOnly.prepare(spec);const auto control=renderEngine(filtersOnly,spec);
    float loadedDifference=0;for(size_t n=0;n<before.left.size();++n)loadedDifference=juce::jmax(loadedDifference,std::abs(before.left[n]-control.left[n]));
    require(loadedDifference>1e-5f,"Imported IR has no measurable effect versus filters-only engine");
    const auto saved=library.save();require(saved.getNumChildren()==1,"Embedded IR state contains wrong lane count");
    juce::MemoryBlock embedded;require(embedded.fromBase64Encoding(saved.getChild(0).getProperty("data").toString()) && embedded==original,"Saved state changed original encoded WAV bytes");
    juce::MemoryOutputStream serialized;saved.writeToStream(serialized);
    const auto decodedState=juce::ValueTree::readFromData(serialized.getData(),serialized.getDataSize());
    require(decodedState.isValid(),"Cannot restore serialized IR state");
    require(temporary.deleteFile() && !temporary.exists(),"Temporary source was not deleted before restore");
    spectralforge::Engine restored;restored.prepare(spec);
    spectralforge::IRLibrary restoredLibrary({&restored.cabinet(0),&restored.cabinet(1),&restored.cabinet(2)});
    restoredLibrary.restore(decodedState);restoredLibrary.prepare(spec,{3,0,0});restoredLibrary.stop();
    require(restored.cabinet(0).activeSource.load()==3,"Embedded IR did not restore without source file");
    const auto after=renderEngine(restored,spec);require(before.left.size()==after.left.size(),"Restored response length changed");
    float difference=0;for(size_t n=0;n<before.left.size();++n)difference=juce::jmax(difference,std::abs(before.left[n]-after.left[n]));
    require(difference<1e-7f,"Restored cabinet response differs from imported response");
    juce::MemoryBlock recalled;require(recalled.fromBase64Encoding(restoredLibrary.save().getChild(0).getProperty("data").toString()) && recalled==original,"Restored state did not preserve original WAV bytes");
    auto result=object();put(result,"sample_rate",rate);put(result,"host_channels",channels);put(result,"block_size",blockSize);
    put(result,"engine_reported_latency_samples",engine.latency());put(result,"kernel",metrics(direct));put(result,"engine_classic_amp_bypassed",metrics(before));
    put(result,"maximum_difference_from_filters_only",loadedDifference);
    put(result,"restore_maximum_sample_difference",difference);put(result,"restored_response",metrics(after));
    put(result,"embedded_original_bytes_equal",true);put(result,"temporary_source_deleted_before_restore",true);put(result,"other_lanes_unchanged",true);put(result,"verdict","PASS");
    return result;
}
}
int main(int argc,char** argv) {
    if(argc<3) {std::cerr<<"Usage: ChimeraValidateExternalIR output-directory IR...\n";return 2;}
    juce::Array<juce::var> results;auto report=object();const juce::File output=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    try {
        require(output.createDirectory().wasOk(),"Cannot create evidence directory");
        for(int index=2;index<argc;++index) {
            const juce::File source=juce::File::getCurrentWorkingDirectory().getChildFile(argv[index]);juce::MemoryBlock bytes;
            require(source.loadFileAsData(bytes),"Cannot read input IR");const auto originalHash=juce::SHA256(bytes).toHexString();juce::String error;
            const auto asset=spectralforge::IRLibrary::decode(bytes,source.getFileName(),error);require(asset!=nullptr,error);
            auto file=object();put(file,"filename",source.getFileName());put(file,"input_sha256",originalHash);put(file,"encoded_bytes",(juce::int64)bytes.getSize());
            put(file,"source_sample_rate",asset->rate);put(file,"source_channels",asset->samples.getNumChannels());put(file,"source_frames",asset->samples.getNumSamples());
            juce::Array<juce::var> cases;
            for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})for(int block:{64,257})cases.add(validateCase(source,bytes,*asset,rate,channels,block));
            put(file,"cases",cases);put(file,"source_file_unmodified",juce::SHA256(source).toHexString()==originalHash);
            require((bool)file.getProperty("source_file_unmodified",false),"Source file changed during validation");results.add(file);
            std::cout<<"PASS "<<source.getFileName()<<": 12 import/kernel/engine/embedded restore cases; SHA256 "<<originalHash<<'\n';
        }
        put(report,"schema_version",1);put(report,"scope","Private local synthetic IR load/convolution/embedded-state validation. No Windows DAW or listening validation.");
        put(report,"cabinet_processing","JUCE Convolution Normalise::yes, Trim::no; Engine cabinet filters 70 Hz / 9 kHz; source bytes preserved independently.");
        put(report,"juce_version",juce::SystemStats::getJUCEVersion());put(report,"files",results);put(report,"verdict","PASS");
        require(output.getChildFile("results.json").replaceWithText(juce::JSON::toString(report)),"Cannot save result JSON");return 0;
    } catch(const std::exception& error) {
        put(report,"verdict","FAIL");put(report,"error",juce::String(error.what()));put(report,"completed_files",results);
        output.getChildFile("results.json").replaceWithText(juce::JSON::toString(report));std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}

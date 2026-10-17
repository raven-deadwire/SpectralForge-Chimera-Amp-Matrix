// Compiled against the immutable beta.2 SOURCE in CI. This fixture is not a
// user DAW project and is not described as installed-host acceptance.
#include "PluginProcessor.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void set(ChimeraProcessor& p,const char* id,float value) {
    auto* parameter=p.parameters().getParameter(id);check(parameter!=nullptr,"Missing historical parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
juce::var parameters(ChimeraProcessor& p) {
    juce::Array<juce::var> rows;
    for(auto* raw:p.getParameters()) {
        auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(raw);check(parameter!=nullptr,"Missing host ID");
        auto* row=new juce::DynamicObject();row->setProperty("id",parameter->paramID);row->setProperty("ordinal",parameter->getParameterIndex());
        row->setProperty("version_hint",parameter->getVersionHint());row->setProperty("value",double(parameter->getValue()));rows.add(row);
    }
    return rows;
}
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        check(argc==2,"Expected output directory");const juce::File directory(argv[1]);check(directory.createDirectory().wasOk(),"Cannot create fixture directory");
        auto p=std::make_unique<ChimeraProcessor>();
        juce::AudioBuffer<float> impulse(1,128);impulse.clear();impulse.setSample(0,0,.75f);impulse.setSample(0,32,.12f);
        const auto file=directory.getChildFile("beta2-personal.wav");
        {
            juce::WavAudioFormat format;auto stream=file.createOutputStream();check(stream!=nullptr,"Cannot create IR");
            auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),48000,1,24,{},0));
            check(writer!=nullptr,"Cannot create IR writer");stream.release();check(writer->writeFromAudioSampleBuffer(impulse,0,128),"Cannot encode IR");
        }
        juce::MemoryBlock ir;check(file.loadFileAsData(ir),"Cannot read IR");check(p->loadIR(0,file).wasOk(),"Cannot load old IR");
        set(*p,"mode",2);set(*p,"drive1",.23f);set(*p,"lowcomp",.42f);set(*p,"gateRangeDb",18);set(*p,"output",-3);
        p->setAmpModel(0,25);p->setAmpChannel(0,2);
        p->copyComparison();const auto a=parameters(*p);
        p->selectComparison(1);set(*p,"drive1",.79f);set(*p,"output",-7);p->setAmpChannel(0,4);const auto b=parameters(*p);p->selectComparison(0);
        juce::MemoryBlock bytes;p->getStateInformation(bytes);const auto state=directory.getChildFile("beta2-released-source.chimera");
        check(state.replaceWithData(bytes.getData(),bytes.getSize()),"Cannot write historical state");
        auto* report=new juce::DynamicObject();report->setProperty("source_sha","6033236cb81820a43679cbbd34be020804f9658d");
        report->setProperty("origin","reconstructed-from-released-beta2-source-not-a-user-DAW-project");
        report->setProperty("state_sha256",juce::SHA256(bytes).toHexString());report->setProperty("ir_sha256",juce::SHA256(ir).toHexString());
        report->setProperty("A",a);report->setProperty("B",b);
        check(directory.getChildFile("beta2-fixture.json").replaceWithText(juce::JSON::toString(juce::var(report),true)),"Cannot write fixture identity");
        check(file.deleteFile(),"Cannot remove source IR to exercise embedded restoration");
        std::cout<<"PASS: fixture saved by immutable beta.2 source 6033236c; includes A/B and embedded personal IR\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

// Renders FabFilter black-box kit signals through Chimera's production EQ and
// clean compressor (Source/GraphicalEQ.h, Source/CommonDynamics.h) so the same
// analysis compares a FabFilter render with Chimera. Offline tool; it never
// loads, inspects or links any FabFilter code. Driven by Tools/fabfilter_kit.py.
//
//   ChimeraKitProbe eq   <rate> <type> <frequency> <gain> <q> <in.f32> <out.f32>
//   ChimeraKitProbe comp <rate> <threshold> <ratio> <attack> <release> <knee> <rms|peak> <in.f32> <out.f32>
//
// Audio files are raw little-endian float32, interleaved stereo.
#include "CommonDynamics.h"
#include "GraphicalEQ.h"
#include <fstream>
#include <iostream>

namespace {
class Host final : public juce::AudioProcessor {
public:
    Host() : state(*this,nullptr,"KIT",layout()) {}
    juce::AudioProcessorValueTreeState state;
    void set(const juce::String& id,float value) {
        auto* parameter=state.getParameter(id);
        if(parameter==nullptr)throw std::runtime_error(("unknown parameter "+id).toStdString());
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    const juce::String getName() const override {return "ChimeraKitProbe";}
    void prepareToPlay(double,int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override {return 0;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    juce::AudioProcessorEditor* createEditor() override {return nullptr;}
    bool hasEditor() const override {return false;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*,int) override {}
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout parameters;
        spectralforge::appendCommonDynamicsParameters(parameters);
        spectralforge::appendGraphicalEQParameters(parameters);
        return parameters;
    }
};

juce::AudioBuffer<float> load(const char* path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error(std::string("cannot read ")+path);
    const auto bytes=size_t(file.tellg());file.seekg(0);
    std::vector<float> data(bytes/sizeof(float));file.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size()*sizeof(float)));
    juce::AudioBuffer<float> audio(2,int(data.size()/2));
    for(int n=0;n<audio.getNumSamples();++n)for(int c=0;c<2;++c)audio.setSample(c,n,data[size_t(n*2+c)]);
    return audio;
}
void save(const juce::AudioBuffer<float>& audio,const char* path) {
    std::vector<float> data(size_t(audio.getNumSamples())*2);
    for(int n=0;n<audio.getNumSamples();++n)for(int c=0;c<2;++c)data[size_t(n*2+c)]=audio.getSample(c,n);
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()*sizeof(float)));
    if(!file)throw std::runtime_error(std::string("cannot write ")+path);
}
// Process in host-sized blocks, as a DAW render does.
template<class Process> void render(juce::AudioBuffer<float>& audio,Process&& process) {
    constexpr int block=256;
    for(int offset=0;offset<audio.getNumSamples();offset+=block) {
        const int n=std::min(block,audio.getNumSamples()-offset);
        juce::AudioBuffer<float> view(audio.getArrayOfWritePointers(),2,offset,n);
        process(view);
    }
}
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    try {
        const juce::String mode=argc>1?argv[1]:"";
        Host host;
        if(mode=="eq" && argc==9) {
            const double rate=std::atof(argv[2]);
            host.set(spectralforge::eqID(0,-1,"bypass"),0.f);
            host.set(spectralforge::eqID(0,0,"enabled"),1.f);
            host.set(spectralforge::eqID(0,0,"type"),float(std::atoi(argv[3])));
            host.set(spectralforge::eqID(0,0,"frequency"),float(std::atof(argv[4])));
            host.set(spectralforge::eqID(0,0,"gain"),float(std::atof(argv[5])));
            host.set(spectralforge::eqID(0,0,"q"),float(std::atof(argv[6])));
            spectralforge::GraphicalEQ eq;eq.bind(host.state,0);eq.prepare(rate);
            auto audio=load(argv[7]);render(audio,[&](auto& view){eq.process(view);});save(audio,argv[8]);return 0;
        }
        if(mode=="comp" && argc==11) {
            const double rate=std::atof(argv[2]);
            host.set("cleanCompOn",1.f);
            host.set("cleanCompThreshold",float(std::atof(argv[3])));
            host.set("cleanCompRatio",float(std::atof(argv[4])));
            host.set("cleanCompAttack",float(std::atof(argv[5])));
            host.set("cleanCompRelease",float(std::atof(argv[6])));
            host.set("cleanCompKnee",float(std::atof(argv[7])));
            host.set("cleanCompMakeup",0.f);host.set("cleanCompMix",1.f);host.set("cleanCompHP",20.f);
            host.set("cleanCompDetector",juce::String(argv[8])=="rms"?1.f:0.f);
            spectralforge::CommonDynamics dynamics;dynamics.bind(host.state);dynamics.prepare(rate);
            auto audio=load(argv[9]);render(audio,[&](auto& view){dynamics.compress(view);});save(audio,argv[10]);return 0;
        }
        std::cerr<<"usage: ChimeraKitProbe eq <rate> <type> <frequency> <gain> <q> <in.f32> <out.f32>\n"
                   "       ChimeraKitProbe comp <rate> <threshold> <ratio> <attack> <release> <knee> <rms|peak> <in.f32> <out.f32>\n";
        return 2;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

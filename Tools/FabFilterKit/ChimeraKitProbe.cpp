// Renders FabFilter black-box kit signals through Chimera's production EQ, clean
// compressor and live limiter, four-band dynamics, de-esser and noise gate
// (Source/GraphicalEQ.h, Source/CommonDynamics.h, Source/MultibandDynamicsCore.h,
// Source/GlobalDSP.h) so the same
// analysis compares a FabFilter render with Chimera. Offline tool; it never
// loads, inspects or links any FabFilter code. Driven by Tools/fabfilter_kit.py.
//
//   ChimeraKitProbe eq   <rate> <type> <frequency> <gain> <q> <in.f32> <out.f32>
//   ChimeraKitProbe comp <rate> <threshold> <ratio> <attack> <release> <knee> <rms|peak> <in.f32> <out.f32>
//   ChimeraKitProbe mb   <rate> <x1> <x2> <x3> <band|-1> <threshold> <ratio> <attack> <release> <knee> <range> <rms|peak> <in.f32> <out.f32>
//     band -1 keeps every band neutral (ratio 1, range 0); otherwise only that
//     zero-based band compresses downward and the others stay neutral.
//   ChimeraKitProbe lim  <rate> <ceiling> <release> <in.f32> <out.f32>
//   ChimeraKitProbe ds   <rate> <frequency> <threshold> <range> <attack> <release> <ratio> <knee> <in.f32> <out.f32>
//   ChimeraKitProbe gate <rate> <threshold> <range> <attack> <hold> <release> <gate|expander> <ratio> <in.f32> <out.f32>
//
// Audio files are raw little-endian float32, interleaved stereo.
#include "CommonDynamics.h"
#include "GraphicalEQ.h"
#include "MultibandDynamicsCore.h"
#include "GlobalDSP.h"
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
        if(mode=="mb" && argc==16) {
            spectralforge::MultibandSettings settings;settings.on=true;settings.mix=1.f;
            settings.detector=juce::String(argv[13])=="rms"?1:0;
            for(int i=0;i<3;++i)settings.crossovers[size_t(i)]=float(std::atof(argv[3+i]));
            const int band=std::atoi(argv[6]);
            for(int i=0;i<spectralforge::multibandBandCount;++i) {
                auto& b=settings.bands[size_t(i)];b.mode=0;b.makeup=0;b.ratio=1.f;b.range=0.f;
                if(i!=band)continue;
                b.threshold=float(std::atof(argv[7]));b.ratio=float(std::atof(argv[8]));b.attack=float(std::atof(argv[9]));
                b.release=float(std::atof(argv[10]));b.knee=float(std::atof(argv[11]));b.range=float(std::atof(argv[12]));
            }
            spectralforge::MultibandDynamicsCore core;core.setSettings(settings);core.prepare(std::atof(argv[2]));
            auto audio=load(argv[14]);render(audio,[&](auto& view){core.process(view.getArrayOfWritePointers(),2,view.getNumSamples());});save(audio,argv[15]);return 0;
        }
        if(mode=="lim" && argc==7) {
            host.set("liveLimiterOn",1.f);host.set("liveLimiterCeiling",float(std::atof(argv[3])));host.set("liveLimiterRelease",float(std::atof(argv[4])));
            spectralforge::CommonDynamics dynamics;dynamics.bind(host.state);dynamics.prepare(std::atof(argv[2]));
            auto audio=load(argv[5]);render(audio,[&](auto& view){dynamics.limit(view);});save(audio,argv[6]);return 0;
        }
        if(mode=="ds" && argc==12) {
            spectralforge::DeEsserSettings settings;settings.on=true;
            settings.frequency=float(std::atof(argv[3]));settings.threshold=float(std::atof(argv[4]));settings.range=float(std::atof(argv[5]));
            settings.attack=float(std::atof(argv[6]));settings.release=float(std::atof(argv[7]));settings.ratio=float(std::atof(argv[8]));settings.knee=float(std::atof(argv[9]));
            spectralforge::DeEsserCore core;core.prepare(std::atof(argv[2]));core.setSettings(settings);core.reset();
            auto audio=load(argv[10]);render(audio,[&](auto& view){core.process(view.getArrayOfWritePointers(),2,view.getNumSamples());});save(audio,argv[11]);return 0;
        }
        if(mode=="gate" && argc==12) {
            spectralforge::NoiseGate gate;gate.prepare(std::atof(argv[2]));
            gate.setMode(juce::String(argv[8])=="expander",float(std::atof(argv[9])),float(std::atof(argv[5])));
            const float threshold=float(std::atof(argv[3])),range=float(std::atof(argv[4])),hold=float(std::atof(argv[6])),release=float(std::atof(argv[7]));
            auto audio=load(argv[10]);render(audio,[&](auto& view){gate.process(view,true,threshold,release,hold,range);});save(audio,argv[11]);return 0;
        }
        std::cerr<<"usage: ChimeraKitProbe eq <rate> <type> <frequency> <gain> <q> <in.f32> <out.f32>\n"
                   "       ChimeraKitProbe comp <rate> <threshold> <ratio> <attack> <release> <knee> <rms|peak> <in.f32> <out.f32>\n"
                   "       ChimeraKitProbe lim <rate> <ceiling> <release> <in.f32> <out.f32>\n"
                   "       ChimeraKitProbe ds <rate> <frequency> <threshold> <range> <attack> <release> <ratio> <knee> <in.f32> <out.f32>\n"
                   "       ChimeraKitProbe gate <rate> <threshold> <range> <attack> <hold> <release> <gate|expander> <ratio> <in.f32> <out.f32>\n"
                   "       ChimeraKitProbe mb <rate> <x1> <x2> <x3> <band|-1> <threshold> <ratio> <attack> <release> <knee> <range> <rms|peak> <in.f32> <out.f32>\n";
        return 2;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

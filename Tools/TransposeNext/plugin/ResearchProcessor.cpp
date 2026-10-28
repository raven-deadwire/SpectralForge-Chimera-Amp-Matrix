// Research-only VST3/AU for T6 DAW acceptance of the Transpose candidate.
// Not the product plug-in: no amp, cab, presets or release state.
#include <juce_audio_utils/juce_audio_utils.h>
#include "ResearchEngine.h"

namespace transposeResearchPlugin {
class Processor final : public juce::AudioProcessor {
public:
    Processor()
        : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                          .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
          state(*this,nullptr,"TransposeResearch",{
              std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"shift",1},"Shift (semitones)",-2,2,-2),
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"engine",1},"Engine",juce::StringArray{"Product baseline","corr40-split-v2","corr40-split-v3"},2),
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"latency",1},"Latency report",juce::StringArray{"Live (0)","Fixed PDC"},0),
              std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"lowUpshiftFloor",1},"Low upshift floor (v4)",false)}) {
        shift=state.getRawParameterValue("shift");engine=state.getRawParameterValue("engine");latencyMode=state.getRawParameterValue("latency");
        lowFloor=state.getRawParameterValue("lowUpshiftFloor");
    }
    const juce::String getName() const override {return "Chimera Transpose Research";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    double getTailLengthSeconds() const override {return .05;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override {return new juce::GenericAudioProcessorEditor(*this);}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        const auto o=l.getMainOutputChannelSet();
        return (o==juce::AudioChannelSet::mono() || o==juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet()==o;
    }
    void prepareToPlay(double rate,int maxBlock) override {
        channels=getTotalNumOutputChannels();capacity=std::max(1,maxBlock);
        sampleRate=rate;dsp.prepare(rate,capacity,channels);
        scratch.setSize(channels,capacity);
        // Bypass must be delayed by the reported latency or a bypassed track
        // moves in time. Sized for the largest report (baseline FFT window).
        const int maxReport=std::max(dsp.latency(Engine::baseline,true,rate),dsp.latency(Engine::v3,true,rate));
        bypassLine.setSize(channels,maxReport+1);bypassLine.clear();bypassWrite=0;
        applied=current();active=engineNow();fixed=fixedNow();dsp.setSemitones(applied);
        setLatencySamples(dsp.latency(active,fixed,sampleRate));
    }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        if(const int s=current();s!=applied){applied=s;dsp.setSemitones(s);}
        if(const auto e=engineNow();e!=active){
            // Switching engines restarts the newly selected one; a click is
            // expected. Research comparison only, not a product transition.
            active=e;dsp.reset();setLatencySamples(dsp.latency(active,fixed,sampleRate));
        }
        if(const bool f=fixedNow();f!=fixed){fixed=f;setLatencySamples(dsp.latency(active,fixed,sampleRate));}
        pushBypassHistory(buffer);
        // Hosts may exceed the prepared size; never allocate on this thread.
        for(int off=0;off<buffer.getNumSamples();off+=capacity){
            const int n=std::min(capacity,buffer.getNumSamples()-off);
            juce::AudioBuffer<float> in(buffer.getArrayOfWritePointers(),channels,off,n);
            scratch.setSize(channels,n,false,false,true);
            dsp.process(active,in,scratch);
            for(int c=0;c<channels;++c)in.copyFrom(c,0,scratch,c,0,n);
        }
    }
    // Host bypass: the dry signal delayed by exactly the reported latency.
    void processBlockBypassed(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {
        const int d=getLatencySamples(),size=bypassLine.getNumSamples();
        for(int i=0;i<buffer.getNumSamples();++i) {
            for(int c=0;c<channels;++c) {
                const float x=buffer.getSample(c,i);
                bypassLine.setSample(c,bypassWrite,x);
                buffer.setSample(c,i,bypassLine.getSample(c,(bypassWrite-d+size)%size));
            }
            bypassWrite=(bypassWrite+1)%size;
        }
    }
    void getStateInformation(juce::MemoryBlock& d) override {
        if(auto x=state.copyState().createXml())copyXmlToBinary(*x,d);
    }
    void setStateInformation(const void* d,int size) override {
        if(auto x=getXmlFromBinary(d,size); x && x->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*x));
    }
private:
    int current() const {return int(std::lround(shift->load()));}
    bool fixedNow() const {return latencyMode->load()>.5f;}
    // Keep the bypass line filled while processing so a switch to bypass is
    // continuous with the input the host compensates against.
    void pushBypassHistory(const juce::AudioBuffer<float>& b) {
        const int size=bypassLine.getNumSamples();
        for(int i=0;i<b.getNumSamples();++i){for(int c=0;c<channels;++c)bypassLine.setSample(c,bypassWrite,b.getSample(c,i));bypassWrite=(bypassWrite+1)%size;}
    }
    Engine engineNow() const {
        const auto selected=Engine(juce::jlimit(0,2,int(std::lround(engine->load()))));
        return selected==Engine::v3 && lowFloor->load()>.5f ? Engine::v4 : selected;
    }
    juce::AudioProcessorValueTreeState state;
    std::atomic<float>* shift{};std::atomic<float>* engine{};std::atomic<float>* latencyMode{};std::atomic<float>* lowFloor{};
    ResearchEngine dsp;juce::AudioBuffer<float> scratch;
    juce::AudioBuffer<float> bypassLine;int bypassWrite{};double sampleRate{48000};bool fixed{};
    int channels{2},capacity{1},applied{0};Engine active{Engine::v3};
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new transposeResearchPlugin::Processor();}

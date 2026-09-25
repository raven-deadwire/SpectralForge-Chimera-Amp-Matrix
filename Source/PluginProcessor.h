#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChimeraDSP.h"
#include "FXParameters.h"
#include "IRLibrary.h"
#include "PerformanceUtilities.h"

class ChimeraProcessor : public juce::AudioProcessor {
public:
    ChimeraProcessor();
    ~ChimeraProcessor() override;
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Chimera Amp Matrix"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override {
        double tail=1.1;
        if(fxParameters[4] && fxParameters[4]->load()>.5f) tail+=(fxParameters[48]->load()>.5f ? 1.5 : fxParameters[5]->load()*.001)*std::log(.001)/std::log(juce::jlimit(.0001f,.85f,fxParameters[6]->load()));
        if(fxParameters[8] && fxParameters[8]->load()>.5f) tail+=12.0;
        return tail;
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState& parameters() { return state; }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::Result loadIR(int lane,const juce::File& file);
    juce::String userIRName(int lane) const {return library.userName(lane);}
    float preCompressorReduction() const {return preReduction.load();}
    float postCompressorReduction() const {return postReduction.load();}
    float postModuleLevel(int position) const {return postPeaks[(size_t)juce::jlimit(0,5,position)].load();}
    juce::String cabStatus(int lane) const { return library.status(lane); }
    spectralforge::IRMetadata cabMetadata(int lane) const { return library.metadata(lane,(int)state.getRawParameterValue("cabtype"+juce::String(lane+1))->load()); }
    void setCabMetadata(int lane,const spectralforge::IRMetadata& metadata) { library.setMetadata(lane,metadata); }
    float inputMeter() const { return inputPeak.load(); }
    float outputMeter() const { return outputPeak.load(); }
    float cpuLoad() const { return cpuAverage.load(); }
    float cpuPeakLoad() const { return cpuPeak.load(); }
    float lowCompMeter() const {return lowCompGain.load();}
    float gateMeter() const { return gateGain.load(); }
    float tuningFrequency() const { return tuner.frequency(); }
    float tuningConfidence() const { return tuner.confidence(); }
    void selectComparison(int slot);
    void copyComparison();
    int comparisonSlot() const {return selectedComparison.load();}
    void loadFactoryPreset(int index);
    void learnMidi(const juce::String& parameterId);
    void clearMidi();
    bool learningMidi() const {return midiLearn.load()>=0;}
    float currentTempo() const {return tempoMeter.load();}
    void tapTempo();
    int pitchLatency() const { return preFX.transpose.latency(); }
private:
    void process(juce::AudioBuffer<float>&);
    juce::ValueTree captureCore();
    void restoreCore(juce::ValueTree);
    std::array<juce::ValueTree,2> comparisons;
    std::mutex comparisonMutex;
    std::atomic<int> selectedComparison{0};
    std::atomic<bool> resetPending{false};
    juce::AudioProcessorValueTreeState state{*this,nullptr,"PARAMS",layout()};
    spectralforge::Engine engine;
    spectralforge::IRLibrary library{{&engine.cabinet(0),&engine.cabinet(1),&engine.cabinet(2)}};
    spectralforge::PreFXChain preFX;
    spectralforge::PostFXChain postFX;
    spectralforge::Tuner tuner;
    spectralforge::PerformanceUtilities utilities;
    std::array<std::atomic<int>,128> midiMap;
    std::atomic<int> midiLearn{-1};
    std::atomic<bool> restartClick{false};
    std::atomic<float> tempoMeter{120};
    double lastTap{};std::array<double,4> tapIntervals{};int tapCount{};
    enum Extra {dualType,dualBlend,dualFrequency,inputMode,doublerOn,doublerTime,tempo,hostTempo,metronome,extraCount};
    std::array<std::atomic<float>*,extraCount> extras{};
    std::array<std::atomic<float>*,spectralforge::fxSpecs.size()> fxParameters{};
    std::array<std::atomic<float>*,11> modelParameters{};
    std::atomic<float>* lowCompParameter{}, * lowAmpMixParameter{}, * preOrderParameter{};
    enum Global { mode,x1,x2,input,output,gateOn,threshold,release,hold,pitchOn,semitones,os,tunerOn,tunerMute,globalCount };
    std::array<std::atomic<float>*,globalCount> globals{};
    std::array<std::array<std::atomic<float>*,18>,3> laneParameters{};
    juce::SmoothedValue<float> inputGain, outputGain, tuningMute;
    std::atomic<float> inputPeak{0},outputPeak{0},gateGain{1},lowCompGain{0};
    std::atomic<float> preReduction{0},postReduction{0};
    std::array<std::atomic<float>,6> postPeaks{};
    std::atomic<float> cpuAverage{0},cpuPeak{0};
    int maximumBlock{512};
    double rate{48000};
};

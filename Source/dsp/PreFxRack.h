#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
namespace spectralforge {
enum class DriveModel:int { greenDrive=0, piFuzz=1 };
class PreFxRack {
public:
 void prepare(const juce::dsp::ProcessSpec&); void reset();
 void setGate(float,bool); void setCompressor(float,float,float,float,bool); void setBoost(float,bool);
 void setDrive(DriveModel,float,float,float,float,bool); void setEq(float,float,float,bool);
 void process(juce::AudioBuffer<float>&);
private:
 using IIR=juce::dsp::IIR::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>>;
 double sampleRate{48000.0}; float gateThreshold{.001f},boostGain{1.f},drive{.35f},tone{.5f},driveLevel{1.f},driveMix{1.f};
 bool gateOn{},compOn{},boostOn{},driveOn{},eqOn{}; DriveModel driveModel{DriveModel::greenDrive};
 juce::dsp::Compressor<float> compressor; IIR drivePre,driveTone,low,mid,high;
};}

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "CabModule.h"
namespace spectralforge {
enum class AmpModel : int { glass=0, tight515=1, ironTube=2, solidPunch=3 };
class LaneProcessor {
public:
 void prepare(const juce::dsp::ProcessSpec& s){ level.reset(s.sampleRate,0.02); level.setCurrentAndTargetValue(1.0f); cab.prepare(s); sampleRate=s.sampleRate; bassFilter.prepare(s);midFilter.prepare(s);trebleFilter.prepare(s);presenceFilter.prepare(s);resonanceFilter.prepare(s);setTone(0,0,0,0,0); oversampling=std::make_unique<juce::dsp::Oversampling<float>>(s.numChannels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);oversampling->initProcessing(s.maximumBlockSize); }
 void reset(){cab.reset();bassFilter.reset();midFilter.reset();trebleFilter.reset();presenceFilter.reset();resonanceFilter.reset();if(oversampling)oversampling->reset();}
 void setModel(AmpModel m){ model=m; } void setDrive(float d){ drive=juce::jlimit(0.f,1.f,d); } void setTone(float b,float m,float t,float presence,float resonance);
 void setLevelDb(float d){ level.setTargetValue(juce::Decibels::decibelsToGain(d)); } void setMuted(bool m){ muted=m; }
 void setCabEnabled(bool e){cab.setEnabled(e);} void setCabCuts(float lo,float hi){cab.setLowCut(lo);cab.setHighCut(hi);}
 void process(juce::AudioBuffer<float>&);
private: AmpModel model{AmpModel::glass}; float drive{0.35f}; bool muted{}; juce::SmoothedValue<float> level{1.f}; CabModule cab; double sampleRate{48000.0}; float bassDb{},midDb{},trebleDb{},presenceDb{},resonanceDb{}; juce::dsp::IIR::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,juce::dsp::IIR::Coefficients<float>> bassFilter,midFilter,trebleFilter,presenceFilter,resonanceFilter; std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
}; }

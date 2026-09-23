#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "CabModule.h"
namespace spectralforge {
enum class AmpModel : int { clean=0, tightDrive=1, bassSaturator=2 };
class LaneProcessor {
public:
 void prepare(const juce::dsp::ProcessSpec& s){ level.reset(s.sampleRate,0.02); level.setCurrentAndTargetValue(1.0f); cab.prepare(s); }
 void reset(){cab.reset();}
 void setModel(AmpModel m){ model=m; } void setDrive(float d){ drive=juce::jlimit(0.f,1.f,d); }
 void setLevelDb(float d){ level.setTargetValue(juce::Decibels::decibelsToGain(d)); } void setMuted(bool m){ muted=m; }
 void setCabEnabled(bool e){cab.setEnabled(e);} void setCabCuts(float lo,float hi){cab.setLowCut(lo);cab.setHighCut(hi);}
 void process(juce::AudioBuffer<float>&);
private: AmpModel model{AmpModel::clean}; float drive{0.35f}; bool muted{}; juce::SmoothedValue<float> level{1.f}; CabModule cab;
}; }

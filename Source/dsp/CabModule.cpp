#include "CabModule.h"
namespace spectralforge {
void CabModule::prepare(const juce::dsp::ProcessSpec&s){sampleRate=s.sampleRate;hp.prepare(s);lp.prepare(s);setLowCut(lowCutHz);setHighCut(highCutHz);}
void CabModule::reset(){hp.reset();lp.reset();}
void CabModule::setLowCut(float hz){lowCutHz=juce::jlimit(20.f,500.f,hz);*hp.state=*juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate,lowCutHz);}
void CabModule::setHighCut(float hz){highCutHz=juce::jlimit(1500.f,20000.f,hz);*lp.state=*juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate,highCutHz);}
void CabModule::process(juce::AudioBuffer<float>&b){if(!enabled)return;juce::dsp::AudioBlock<float>bl(b);juce::dsp::ProcessContextReplacing<float>ctx(bl);hp.process(ctx);lp.process(ctx);}
}

#include "PluginProcessor.h"
#include "PluginEditor.h"
ChimeraAmpMatrixAudioProcessor::ChimeraAmpMatrixAudioProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"STATE",createParameters()){}
void ChimeraAmpMatrixAudioProcessor::prepareToPlay(double sr,int bs){engine.prepare({sr,(juce::uint32)bs,(juce::uint32)getTotalNumOutputChannels()});}
bool ChimeraAmpMatrixAudioProcessor::isBusesLayoutSupported(const BusesLayout&l)const{auto o=l.getMainOutputChannelSet();return(o==juce::AudioChannelSet::mono()||o==juce::AudioChannelSet::stereo())&&l.getMainInputChannelSet()==o;}
void ChimeraAmpMatrixAudioProcessor::processBlock(juce::AudioBuffer<float>&b,juce::MidiBuffer&){juce::ScopedNoDenormals n;auto f=[this](const char*i){return state.getRawParameterValue(i)->load();};auto q=[&](const char*i){return f(i)>.5f;};
 std::array<float,3>lv{f("l1"),f("l2"),f("l3")};std::array<int,3>mo{(int)f("m1"),(int)f("m2"),(int)f("m3")};std::array<bool,3>mu{q("u1"),q("u2"),q("u3")},so{q("s1"),q("s2"),q("s3")};
 engine.process(b,(spectralforge::RoutingMode)(int)f("mode"),f("x1"),f("x2"),lv,mo,mu,so);}
juce::AudioProcessorValueTreeState::ParameterLayout ChimeraAmpMatrixAudioProcessor::createParameters(){std::vector<std::unique_ptr<juce::RangedAudioParameter>>p;
 p.push_back(std::make_unique<juce::AudioParameterChoice>("mode","Routing",juce::StringArray{"Classic","Dual","Matrix"},0));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("x1","Low Mid",juce::NormalisableRange<float>(60,800,1,.45f),150));p.push_back(std::make_unique<juce::AudioParameterFloat>("x2","Mid High",juce::NormalisableRange<float>(500,6000,1,.45f),1200));
 for(int i=1;i<=3;++i){auto n=juce::String(i);p.push_back(std::make_unique<juce::AudioParameterFloat>("l"+n,"Lane Level",-24,12,0));p.push_back(std::make_unique<juce::AudioParameterChoice>("m"+n,"Lane Amp",juce::StringArray{"Clean","Tight Drive","Bass Saturator"},i-1));p.push_back(std::make_unique<juce::AudioParameterBool>("u"+n,"Mute",false));p.push_back(std::make_unique<juce::AudioParameterBool>("s"+n,"Solo",false));}return{p.begin(),p.end()};}
void ChimeraAmpMatrixAudioProcessor::getStateInformation(juce::MemoryBlock&d){if(auto x=state.copyState().createXml())copyXmlToBinary(*x,d);}
void ChimeraAmpMatrixAudioProcessor::setStateInformation(const void*d,int z){if(auto x=getXmlFromBinary(d,z))if(x->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*x));}
juce::AudioProcessor*JUCE_CALLTYPE createPluginFilter(){return new ChimeraAmpMatrixAudioProcessor();}

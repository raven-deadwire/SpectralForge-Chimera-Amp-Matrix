#include "PluginProcessor.h"
#include "PluginEditor.h"
ChimeraProcessor::ChimeraProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)){}
void ChimeraProcessor::prepareToPlay(double sr,int bs){engine.prepare({sr,(juce::uint32)bs,(juce::uint32)getTotalNumOutputChannels()});}
bool ChimeraProcessor::isBusesLayoutSupported(const BusesLayout&l)const{return(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo())&&l.getMainInputChannelSet()==l.getMainOutputChannelSet();}
void ChimeraProcessor::processBlock(juce::AudioBuffer<float>&b,juce::MidiBuffer&){juce::ScopedNoDenormals nd;auto f=[this](const juce::String&id){return state.getRawParameterValue(id)->load();};std::array<spectralforge::LaneState,3>s{};for(int i=0;i<3;++i){auto n=juce::String(i+1);s[i].amp=(int)f("amp"+n);s[i].drive=f("drive"+n);s[i].levelDb=f("level"+n);s[i].bass=f("bass"+n);s[i].lowMid=f("lowmid"+n);s[i].highMid=f("highmid"+n);s[i].treble=f("treble"+n);s[i].presence=f("presence"+n);s[i].resonance=f("resonance"+n);s[i].bandTone=f("bandtone"+n);s[i].mute=f("mute"+n)>.5f;s[i].solo=f("solo"+n)>.5f;s[i].polarity=f("polarity"+n)>.5f;s[i].cab=f("cab"+n)>.5f;s[i].cabLow=f("cablow"+n);s[i].cabHigh=f("cabhigh"+n);}engine.process(b,(spectralforge::RoutingMode)(int)f("mode"),f("x1"),f("x2"),s);}
juce::AudioProcessorValueTreeState::ParameterLayout ChimeraProcessor::layout(){juce::AudioProcessorValueTreeState::ParameterLayout p;p.add(std::make_unique<juce::AudioParameterChoice>("mode","Routing",juce::StringArray{"Classic","Dual","Matrix"},0));p.add(std::make_unique<juce::AudioParameterFloat>("x1","X1",60.f,350.f,150.f));p.add(std::make_unique<juce::AudioParameterFloat>("x2","X2",500.f,4000.f,1200.f));for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterChoice>("amp"+n,"Amp "+n,juce::StringArray{"Glass","Brit Edge","Tight 515","Wide Rect","Liquid Lead","Iron Tube","Solid Punch","Modern Bass"},i==0?0:2));p.add(std::make_unique<juce::AudioParameterFloat>("drive"+n,"Drive "+n,0.f,1.f,.35f));p.add(std::make_unique<juce::AudioParameterFloat>("level"+n,"Level "+n,-24.f,12.f,0.f));for(auto id:{"bass","lowmid","highmid","treble"})p.add(std::make_unique<juce::AudioParameterFloat>(juce::String(id)+n,juce::String(id)+" "+n,-12.f,12.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("presence"+n,"Presence "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("resonance"+n,"Resonance "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterBool>("mute"+n,"Mute "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("solo"+n,"Solo "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("polarity"+n,"Polarity "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("cab"+n,"Cab "+n,true));p.add(std::make_unique<juce::AudioParameterFloat>("cablow"+n,"Cab Low "+n,20.f,500.f,70.f));p.add(std::make_unique<juce::AudioParameterFloat>("cabhigh"+n,"Cab High "+n,1500.f,20000.f,9000.f));}for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterFloat>("bandtone"+n,"Matrix Band Tone "+n,-12.f,12.f,0.f));}return p;}
void ChimeraProcessor::getStateInformation(juce::MemoryBlock&d){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,d);}void ChimeraProcessor::setStateInformation(const void*d,int n){if(auto xml=getXmlFromBinary(d,n))if(xml->hasTagName(state.state.getType())){
    auto restored = juce::ValueTree::fromXml(*xml);
    // Older test builds have no Matrix tone parameters: recall them at neutral,
    // even if the currently open session previously used a nonzero band tone.
    for (int i = 1; i <= 3; ++i)
    {
        const auto id = "bandtone" + juce::String(i);
        if (!restored.getChildWithProperty("id", id).isValid())
        {
            juce::ValueTree value("PARAM");
            value.setProperty("id", id, nullptr);
            value.setProperty("value", 0.0f, nullptr);
            restored.appendChild(value, nullptr);
        }
    }
    state.replaceState(restored);
}}
juce::AudioProcessorEditor*ChimeraProcessor::createEditor(){return new ChimeraEditor(*this);}juce::AudioProcessor*JUCE_CALLTYPE createPluginFilter(){return new ChimeraProcessor();}

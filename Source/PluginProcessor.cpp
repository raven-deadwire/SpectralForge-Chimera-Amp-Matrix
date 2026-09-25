#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

ChimeraProcessor::ChimeraProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true))
{
    clearMidi();
    const std::array<const char*,extraCount> extraIds{"dualtype","dualblend","dualcross","inputmode","doubleron","doublertime","tempo","temposync","metronome"};
    for(size_t i=0;i<extraIds.size();++i)extras[i]=state.getRawParameterValue(extraIds[i]);
    lowCompParameter=state.getRawParameterValue("lowcomp");lowAmpMixParameter=state.getRawParameterValue("lowampmix");
    preOrderParameter=state.getRawParameterValue("preorder");
    gainOrderParameter=state.getRawParameterValue("gainorder");
    const std::array<const char*,globalCount> ids{"mode","x1","x2","input","output","gateon","gatethreshold","gaterelease","gatehold","transposeon","transpose","oversampling","tuneron","tunermute"};
    for(size_t i=0;i<ids.size();++i) globals[i]=state.getRawParameterValue(ids[i]);
    for(size_t i=0;i<spectralforge::fxSpecs.size();++i) fxParameters[i]=state.getRawParameterValue(spectralforge::fxSpecs[i].id);
    for(size_t i=0;i<modelParameters.size();++i)modelParameters[i]=state.getRawParameterValue(spectralforge::modelFamilies[i].parameter);
    const std::array<const char*,18> laneIds{"amp","drive","level","bass","lowmid","highmid","treble","presence","resonance","bandtone","mute","solo","polarity","cab","cablow","cabhigh","cabtype","ampon"};
    for(int i=0;i<3;++i) for(size_t k=0;k<laneIds.size();++k)
        laneParameters[i][k]=state.getRawParameterValue(juce::String(laneIds[k])+juce::String(i+1));
}
ChimeraProcessor::~ChimeraProcessor() { releaseResources(); }
void ChimeraProcessor::releaseResources() { library.stop(); tuner.stop(); }
void ChimeraProcessor::prepareToPlay(double sr,int block)
{
    library.stop(); tuner.stop(); cpuAverage.store(0);cpuPeak.store(0);rate=sr; maximumBlock=juce::jmax(1,block);
    const juce::dsp::ProcessSpec spec{sr,(juce::uint32)maximumBlock,(juce::uint32)getTotalNumOutputChannels()};
    engine.prepare(spec); preFX.prepare(spec); postFX.prepare(spec);utilities.prepare(spec); tuner.prepare(sr);
    preReduction.store(0);postReduction.store(0);for(auto& meter:postPeaks)meter.store(0);
    std::array<int,3> sources{};
    for(int i=0;i<3;++i) sources[i]=(int)laneParameters[i][16]->load();
    library.prepare(spec,sources);
    inputGain.reset(sr,.020); outputGain.reset(sr,.020); tuningMute.reset(sr,.005);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[input]->load()));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[output]->load()));
    tuningMute.setCurrentAndTargetValue(1);
    engine.setOversampling((int)globals[os]->load());
    setLatencySamples(postFX.latency()+engine.latency()+preFX.latency(globals[pitchOn]->load()>.5f));
}
bool ChimeraProcessor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    return (buses.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()) &&
            buses.getMainInputChannelSet()==buses.getMainOutputChannelSet();
}
void ChimeraProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)
{
    const auto started=juce::Time::getHighResolutionTicks();
    juce::ScopedNoDenormals noDenormals;
    for(const auto metadata:midi) {
        const auto message=metadata.getMessage();if(!message.isController())continue;
        const int cc=message.getControllerNumber(),learn=midiLearn.exchange(-1);
        if(learn>=0) midiMap[(size_t)cc].store(learn);
        const int target=midiMap[(size_t)cc].load();
        if(target>=0 && target<getParameters().size()) getParameters()[target]->setValueNotifyingHost(message.getControllerValue()/127.f);
    }
    float bpm=extras[tempo]->load();
    if(extras[hostTempo]->load()>.5f) if(auto* playhead=getPlayHead()) if(const auto position=playhead->getPosition()) if(const auto hostBpm=position->getBpm()) bpm=juce::jlimit(40.f,240.f,float(*hostBpm));
    tempoMeter.store(bpm);
    // Hosts may deliver blocks larger than prepareToPlay's hint. All DSP and
    // convolution buffers remain bounded to the prepared capacity.
    for(int offset=0;offset<buffer.getNumSamples();offset+=maximumBlock)
    {
        const int count=juce::jmin(maximumBlock,buffer.getNumSamples()-offset);
        std::array<float*,2> channels{};
        for(int c=0;c<buffer.getNumChannels();++c) channels[(size_t)c]=buffer.getWritePointer(c,offset);
        juce::AudioBuffer<float> part(channels.data(),buffer.getNumChannels(),count);
        process(part);
    }
    if(buffer.getNumSamples()>0 && rate>0) {
        const double budget=buffer.getNumSamples()/rate;
        const float load=float(100.0*juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-started)/budget);
        const float decay=float(std::exp(-budget));
        cpuAverage.store(decay*cpuAverage.load()+(1-decay)*load);
        cpuPeak.store(juce::jmax(load,cpuPeak.load()*decay));
    }
}
void ChimeraProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if(resetPending.exchange(false)) {engine.reset();preFX.reset();postFX.reset();utilities.reset();}
    if(extras[inputMode]->load()>.5f && buffer.getNumChannels()==2) buffer.copyFrom(1,0,buffer,0,0,buffer.getNumSamples());
    const auto value=[this](Global id){return globals[id]->load();};
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(value(input)));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(value(output)));
    float peak=0;
    for(int n=0;n<buffer.getNumSamples();++n)
    {
        const float gain=inputGain.getNextValue();
        for(int c=0;c<buffer.getNumChannels();++c)
        {
            const float x=buffer.getSample(c,n)*gain;
            buffer.setSample(c,n,std::isfinite(x) ? x : 0.f);
            peak=juce::jmax(peak,std::abs(buffer.getSample(c,n)));
        }
    }
    const float meterDecay=float(std::exp(-buffer.getNumSamples()/(rate*.4)));
    inputPeak.store(juce::jmax(peak,inputPeak.load()*meterDecay));
    tuner.push(buffer,value(tunerOn)>.5f);
    auto fx=spectralforge::readFX(fxParameters);fx.envelopeFirst=preOrderParameter->load()>.5f;fx.boostAfterDrive=gainOrderParameter->load()>.5f;for(size_t i=0;i<modelParameters.size();++i)fx.models[i]=(int)modelParameters[i]->load();if(fx.delaySync)fx.delayMs=60000.f/tempoMeter.load();
    const bool pitching=value(pitchOn)>.5f;
    preFX.process(buffer,value(gateOn)>.5f,value(threshold),value(release),value(hold),pitching,(int)value(semitones),fx);
    preReduction.store(preFX.compressor.reduction());
    gateGain.store(preFX.gate.reduction());
    const int latency=postFX.latency()+engine.latency()+preFX.latency(pitching);
    if(getLatencySamples()!=latency) setLatencySamples(latency);
    engine.setOversampling((int)value(os));
    std::array<spectralforge::LaneState,3> lanes{};
    for(int i=0;i<3;++i)
    {
        auto f=[this,i](int k){return laneParameters[i][(size_t)k]->load();};
        auto& lane=lanes[i];
        lane.amp=(int)f(0); lane.drive=f(1); lane.levelDb=f(2); lane.bass=f(3); lane.lowMid=f(4);
        lane.highMid=f(5); lane.treble=f(6); lane.presence=f(7); lane.resonance=f(8); lane.bandTone=f(9);
        lane.mute=f(10)>.5f; lane.solo=f(11)>.5f; lane.polarity=f(12)>.5f; lane.cab=f(13)>.5f;
        lane.cabLow=f(14); lane.cabHigh=f(15); lane.ampEnabled=f(17)>.5f;
        engine.cabinet(i).requestedSource.store((int)f(16));
    }
    lanes[0].lowComp=lowCompParameter->load();lanes[0].lowAmpMix=lowAmpMixParameter->load();
    const bool dualCross=value(mode)==1 && extras[dualType]->load()>.5f;
    engine.process(buffer,(spectralforge::RoutingMode)(int)value(mode),dualCross ? extras[dualFrequency]->load() : value(x1),value(x2),lanes,&preFX.cleanOutput(),dualCross,extras[dualBlend]->load());
    lowCompGain.store(engine.lowReduction());
    postFX.process(buffer,fx);
    postReduction.store(postFX.compressorReduction());
    for(size_t i=0;i<postPeaks.size();++i)
        postPeaks[i].store(juce::jmax(postFX.stagePeaks[i],postPeaks[i].load()*meterDecay));
    utilities.process(buffer,tempoMeter.load(),extras[doublerOn]->load()>.5f,extras[doublerTime]->load(),extras[metronome]->load()>.5f,restartClick.exchange(false));
    tuningMute.setTargetValue(value(tunerOn)>.5f && value(tunerMute)>.5f ? 0.f : 1.f);
    peak=0;
    for(int n=0;n<buffer.getNumSamples();++n)
    {
        const float gain=outputGain.getNextValue()*tuningMute.getNextValue();
        for(int c=0;c<buffer.getNumChannels();++c)
        {
            const float x=buffer.getSample(c,n)*gain;
            buffer.setSample(c,n,std::isfinite(x) ? x : 0.f);
            peak=juce::jmax(peak,std::abs(buffer.getSample(c,n)));
        }
    }
    outputPeak.store(juce::jmax(peak,outputPeak.load()*meterDecay));
}
juce::Result ChimeraProcessor::loadIR(int lane,const juce::File& file)
{
    const auto result=library.importFile(lane,file);
    if(result.wasOk())
    {
        auto* parameter=state.getParameter("cabtype"+juce::String(lane+1));
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(3)); parameter->endChangeGesture();
        engine.cabinet(lane).requestedSource.store(3);
    }
    return result;
}
juce::AudioProcessorValueTreeState::ParameterLayout ChimeraProcessor::layout(){juce::AudioProcessorValueTreeState::ParameterLayout p;p.add(std::make_unique<juce::AudioParameterChoice>("mode","Routing",juce::StringArray{"Classic","Dual","Matrix"},0));p.add(std::make_unique<juce::AudioParameterFloat>("x1","X1",60.f,350.f,150.f));p.add(std::make_unique<juce::AudioParameterFloat>("x2","X2",500.f,4000.f,1200.f));for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterChoice>("amp"+n,"Amp "+n,spectralforge::ampNames(),2));p.add(std::make_unique<juce::AudioParameterFloat>("drive"+n,"Drive "+n,0.f,1.f,.35f));p.add(std::make_unique<juce::AudioParameterFloat>("level"+n,"Level "+n,-24.f,12.f,0.f));for(auto id:{"bass","lowmid","highmid","treble"})p.add(std::make_unique<juce::AudioParameterFloat>(juce::String(id)+n,juce::String(id)+" "+n,-12.f,12.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("presence"+n,"Presence "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("resonance"+n,"Resonance "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterBool>("mute"+n,"Mute "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("solo"+n,"Solo "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("polarity"+n,"Polarity "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("cab"+n,"Cab "+n,true));p.add(std::make_unique<juce::AudioParameterFloat>("cablow"+n,"Cab Low "+n,20.f,500.f,70.f));p.add(std::make_unique<juce::AudioParameterFloat>("cabhigh"+n,"Cab High "+n,1500.f,20000.f,9000.f));}for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterFloat>("bandtone"+n,"Matrix Band Tone "+n,-12.f,12.f,0.f));}
    for(int i=1;i<=3;++i) p.add(std::make_unique<juce::AudioParameterChoice>("cabtype"+juce::String(i),"Cabinet source "+juce::String(i),juce::StringArray{"Filters only","V30 / SM57","Jensen / SM57","User IR"},1));
    auto number=[&](const char* id,const char* name,float low,float high,float value,float interval=0.01f) { p.add(std::make_unique<juce::AudioParameterFloat>(id,name,juce::NormalisableRange<float>{low,high,interval},value)); };
    auto toggle=[&](const char* id,const char* name,bool value) { p.add(std::make_unique<juce::AudioParameterBool>(id,name,value)); };
    number("input","Input gain",-24,24,0); number("output","Output gain",-36,12,-6);
    toggle("gateon","Noise gate",true); number("gatethreshold","Gate threshold",-90,-15,-65);
    number("gaterelease","Gate release",5,500,80); number("gatehold","Gate hold",0,150,20);
    toggle("transposeon","Transpose",false);
    p.add(std::make_unique<juce::AudioParameterInt>("transpose","Transpose semitones",-12,12,0));
    p.add(std::make_unique<juce::AudioParameterChoice>("oversampling","Oversampling",juce::StringArray{"1x","2x","4x","8x"},2));
    toggle("tuneron","Tuner",false); toggle("tunermute","Mute while tuning",true);
    number("tunerref","Tuner A4 reference",430,450,440);
    toggle("preon","Pre drive",false);number("predrive","Pre drive amount",0,1,.3f);number("pretone","Pre drive tone",1000,10000,4000);number("prelevel","Pre drive level",-18,12,0);
    toggle("delayon","Post delay",false);number("delaytime","Delay time",20,1000,250);number("delayfeedback","Delay feedback",0,.85f,.25f);number("delaymix","Delay mix",0,.6f,.2f);
    toggle("reverbon","Post reverb",false);number("reverbsize","Reverb room size",0,1,.35f);number("reverbdamping","Reverb damping",0,1,.55f);number("reverbmix","Reverb mix",0,.6f,.15f);
    for(int i=1;i<=3;++i) p.add(std::make_unique<juce::AudioParameterBool>("ampon"+juce::String(i),"Amplifier enabled "+juce::String(i),true));
    number("lowcomp","Matrix LOW compression",0,1,.35f);
    number("lowampmix","Matrix LOW DI to amp blend",0,1,0);
    for(size_t i=12;i<spectralforge::fxSpecs.size();++i) {const auto& spec=spectralforge::fxSpecs[i];if(spec.toggle)toggle(spec.id,spec.id,spec.initial>.5f);else number(spec.id,spec.id,spec.minimum,spec.maximum,spec.initial,spec.interval);}
    p.add(std::make_unique<juce::AudioParameterChoice>("dualtype","Dual routing",juce::StringArray{"Blend","Crossover"},0));
    number("dualblend","Dual blend",0,1,.5f);number("dualcross","Dual crossover",60,4000,350);
    p.add(std::make_unique<juce::AudioParameterChoice>("inputmode","Input mode",juce::StringArray{"Stereo","Mono L"},0));
    toggle("doubleron","Doubler",false);number("doublertime","Doubler spread",1,20,6);
    number("tempo","Tempo",40,240,120);toggle("temposync","Follow host tempo",false);toggle("metronome","Metronome",false);
    for(size_t i=0;i<spectralforge::modelFamilies.size();++i) {const auto& family=spectralforge::modelFamilies[i];p.add(std::make_unique<juce::AudioParameterChoice>(family.parameter,juce::String(family.category)+" model",spectralforge::modelNames((int)i),0));}
    p.add(std::make_unique<juce::AudioParameterChoice>("preorder","Pedal detector order",juce::StringArray{"Compressor first","Envelope first"},1));
    // Append new parameters so existing host parameter indices remain stable.
    p.add(std::make_unique<juce::AudioParameterChoice>("gainorder","Pedal gain order",juce::StringArray{"Fuzz > Boost > Drive","Fuzz > Drive > Boost"},0));
return p;
}

juce::ValueTree ChimeraProcessor::captureCore()
{
    auto saved=state.copyState();
    saved.removeChild(saved.getChildWithName("USER_IRS"),nullptr);
    saved.removeChild(saved.getChildWithName("COMPARISONS"),nullptr);
    saved.appendChild(library.save(),nullptr); saved.setProperty("schemaVersion",6,nullptr);
    return saved;
}
void ChimeraProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto saved=captureCore();
    juce::ValueTree slots("COMPARISONS");
    { std::lock_guard<std::mutex> lock(comparisonMutex);
      const int active=selectedComparison.load();comparisons[(size_t)active]=saved.createCopy();
      slots.setProperty("active",active,nullptr);
      for(int i=0;i<2;++i) if(comparisons[(size_t)i].isValid()) {auto slot=comparisons[(size_t)i].createCopy();slot.setProperty("slot",i,nullptr);slots.appendChild(slot,nullptr);}
    }
    saved.appendChild(slots,nullptr);
    juce::ValueTree midi("MIDI_MAP");for(int cc=0;cc<128;++cc) {const int index=midiMap[(size_t)cc].load();if(index<0 || index>=getParameters().size())continue;if(auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(getParameters()[index])) {juce::ValueTree item("CC");item.setProperty("cc",cc,nullptr);item.setProperty("id",parameter->paramID,nullptr);midi.appendChild(item,nullptr);}}
    saved.appendChild(midi,nullptr);auto xml=saved.createXml();copyXmlToBinary(*xml,data);
}
void ChimeraProcessor::restoreCore(juce::ValueTree restored)
{
    if(!restored.isValid() || !restored.hasType("PARAMS")) return;
    const auto defaults=state.copyState();
    for(auto child:defaults)
    {
        const auto id=child.getProperty("id").toString();
        if(id.isEmpty() || restored.getChildWithProperty("id",id).isValid()) continue;
        auto* parameter=state.getParameter(id);if(!parameter) continue;
        float value=parameter->convertFrom0to1(parameter->getDefaultValue());
        if(id.startsWith("cabtype") || id=="gateon" || id=="output" || id=="lowcomp" || id=="preorder" || id=="gainorder") value=0;
        juce::ValueTree item("PARAM");item.setProperty("id",id,nullptr);item.setProperty("value",value,nullptr);restored.appendChild(item,nullptr);
    }
    library.restore(restored.getChildWithName("USER_IRS"));restored.removeChild(restored.getChildWithName("USER_IRS"),nullptr);
    restored.removeChild(restored.getChildWithName("COMPARISONS"),nullptr);state.replaceState(restored);resetPending.store(true);
    for(int i=0;i<3;++i) engine.cabinet(i).requestedSource.store((int)laneParameters[i][16]->load());
}
void ChimeraProcessor::setStateInformation(const void* data,int size)
{
    // Three bounded snapshots, each containing up to three <=4 MB IR assets.
    if(size<=0 || size>64*1024*1024) return;
    auto xml=getXmlFromBinary(data,size);if(!xml || !xml->hasTagName("PARAMS")) return;
    auto restored=juce::ValueTree::fromXml(*xml);const auto saved=restored.getChildWithName("COMPARISONS");
    { std::lock_guard<std::mutex> lock(comparisonMutex);
      comparisons={};selectedComparison.store(juce::jlimit(0,1,(int)saved.getProperty("active",0)));
      for(auto child:saved) {const int slot=(int)child.getProperty("slot",-1);if(slot>=0 && slot<2) comparisons[(size_t)slot]=child.createCopy();}
    }
    clearMidi();for(auto item:restored.getChildWithName("MIDI_MAP")) {const int cc=(int)item.getProperty("cc",-1);auto* parameter=state.getParameter(item.getProperty("id").toString());if(cc>=0 && cc<128 && parameter)midiMap[(size_t)cc].store(parameter->getParameterIndex());}
    restored.removeChild(restored.getChildWithName("MIDI_MAP"),nullptr);restoreCore(restored);
}
void ChimeraProcessor::selectComparison(int slot)
{
    slot=juce::jlimit(0,1,slot);if(slot==selectedComparison.load()) return;
    auto current=captureCore();juce::ValueTree next;
    { std::lock_guard<std::mutex> lock(comparisonMutex);
      comparisons[(size_t)selectedComparison.load()]=current.createCopy();
      if(!comparisons[(size_t)slot].isValid()) comparisons[(size_t)slot]=current.createCopy();
      next=comparisons[(size_t)slot].createCopy();selectedComparison.store(slot);
    }
    restoreCore(next);
}
void ChimeraProcessor::copyComparison()
{
    auto current=captureCore();std::lock_guard<std::mutex> lock(comparisonMutex);
    comparisons[(size_t)(1-selectedComparison.load())]=current.createCopy();
}
juce::AudioProcessorEditor* ChimeraProcessor::createEditor() { return new ChimeraEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChimeraProcessor(); }

void ChimeraProcessor::clearMidi() {for(auto& target:midiMap)target.store(-1);midiLearn.store(-1);}
void ChimeraProcessor::learnMidi(const juce::String& id) {if(auto* parameter=state.getParameter(id))midiLearn.store(parameter->getParameterIndex());}
void ChimeraProcessor::tapTempo() {
    const double now=juce::Time::getMillisecondCounterHiRes(),interval=now-lastTap;lastTap=now;
    if(interval<250 || interval>1500) {tapCount=0;restartClick.store(true);return;}
    tapIntervals[(size_t)(tapCount++%4)]=interval;double sum=0;const int count=juce::jmin(4,tapCount);for(int i=0;i<count;++i)sum+=tapIntervals[(size_t)i];
    auto* parameter=state.getParameter("tempo");parameter->setValueNotifyingHost(parameter->convertTo0to1(float(60000/(sum/count))));state.getParameter("temposync")->setValueNotifyingHost(0);restartClick.store(true);
}
void ChimeraProcessor::loadFactoryPreset(int index) {
    // The shared catalog resets every sound parameter, then applies the preset.
    // Performance controls, MIDI mappings and user-owned IR assets are retained.
    // An invalid preset index deliberately leaves the current sound unchanged.
    if (spectralforge::applyFactoryPreset(index, [this](const char* id, float value) {
        if (auto* parameter = state.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    })) resetPending.store(true);
}

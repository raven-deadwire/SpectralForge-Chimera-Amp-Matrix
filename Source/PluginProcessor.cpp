#include "PluginProcessor.h"
#include "PluginEditor.h"

ChimeraProcessor::ChimeraProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true))
{
    lowCompParameter=state.getRawParameterValue("lowcomp");
    const std::array<const char*,globalCount> ids{"mode","x1","x2","input","output","gateon","gatethreshold","gaterelease","gatehold","transposeon","transpose","oversampling","tuneron","tunermute"};
    for(size_t i=0;i<ids.size();++i) globals[i]=state.getRawParameterValue(ids[i]);
    const std::array<const char*,12> fxIds{"preon","predrive","pretone","prelevel","delayon","delaytime","delayfeedback","delaymix","reverbon","reverbsize","reverbdamping","reverbmix"};
    for(size_t i=0;i<fxIds.size();++i) fxParameters[i]=state.getRawParameterValue(fxIds[i]);
    const std::array<const char*,18> laneIds{"amp","drive","level","bass","lowmid","highmid","treble","presence","resonance","bandtone","mute","solo","polarity","cab","cablow","cabhigh","cabtype","ampon"};
    for(int i=0;i<3;++i) for(size_t k=0;k<laneIds.size();++k)
        laneParameters[i][k]=state.getRawParameterValue(juce::String(laneIds[k])+juce::String(i+1));
}
ChimeraProcessor::~ChimeraProcessor() { releaseResources(); }
void ChimeraProcessor::releaseResources() { library.stop(); tuner.stop(); }
void ChimeraProcessor::prepareToPlay(double sr,int block)
{
    library.stop(); tuner.stop(); rate=sr; maximumBlock=juce::jmax(1,block);
    const juce::dsp::ProcessSpec spec{sr,(juce::uint32)maximumBlock,(juce::uint32)getTotalNumOutputChannels()};
    engine.prepare(spec); preFX.prepare(spec); postFX.prepare(spec); tuner.prepare(sr);
    std::array<int,3> sources{};
    for(int i=0;i<3;++i) sources[i]=(int)laneParameters[i][16]->load();
    library.prepare(spec,sources);
    inputGain.reset(sr,.020); outputGain.reset(sr,.020); tuningMute.reset(sr,.005);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[input]->load()));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[output]->load()));
    tuningMute.setCurrentAndTargetValue(1);
    engine.setOversampling((int)globals[os]->load());
    setLatencySamples(engine.latency()+preFX.latency(globals[pitchOn]->load()>.5f));
}
bool ChimeraProcessor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    return (buses.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()) &&
            buses.getMainInputChannelSet()==buses.getMainOutputChannelSet();
}
void ChimeraProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
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
}
void ChimeraProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if(resetPending.exchange(false)) {engine.reset();preFX.reset();postFX.reset();}
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
    const auto fxValue=[this](size_t i){return fxParameters[i]->load();};
    spectralforge::FXState fx;
    fx.driveOn=fxValue(0)>.5f;fx.drive=fxValue(1);fx.tone=fxValue(2);fx.driveLevel=fxValue(3);
    fx.delayOn=fxValue(4)>.5f;fx.delayMs=fxValue(5);fx.feedback=fxValue(6);fx.delayMix=fxValue(7);
    fx.reverbOn=fxValue(8)>.5f;fx.room=fxValue(9);fx.damping=fxValue(10);fx.reverbMix=fxValue(11);
    const bool pitching=value(pitchOn)>.5f;
    preFX.process(buffer,value(gateOn)>.5f,value(threshold),value(release),value(hold),pitching,(int)value(semitones),fx);
    gateGain.store(preFX.gate.reduction());
    const int latency=engine.latency()+preFX.latency(pitching);
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
    lanes[0].lowComp=lowCompParameter->load();
    engine.process(buffer,(spectralforge::RoutingMode)(int)value(mode),value(x1),value(x2),lanes,&preFX.drive.cleanOutput());
    lowCompGain.store(engine.lowReduction());
    postFX.process(buffer,fx);
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
juce::AudioProcessorValueTreeState::ParameterLayout ChimeraProcessor::layout(){juce::AudioProcessorValueTreeState::ParameterLayout p;p.add(std::make_unique<juce::AudioParameterChoice>("mode","Routing",juce::StringArray{"Classic","Dual","Matrix"},0));p.add(std::make_unique<juce::AudioParameterFloat>("x1","X1",60.f,350.f,150.f));p.add(std::make_unique<juce::AudioParameterFloat>("x2","X2",500.f,4000.f,1200.f));for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterChoice>("amp"+n,"Amp "+n,juce::StringArray{"Glass","Brit Edge","Tight 515","Wide Rect","Liquid Lead","Iron Tube","Solid Punch","Modern Bass"},2));p.add(std::make_unique<juce::AudioParameterFloat>("drive"+n,"Drive "+n,0.f,1.f,.35f));p.add(std::make_unique<juce::AudioParameterFloat>("level"+n,"Level "+n,-24.f,12.f,0.f));for(auto id:{"bass","lowmid","highmid","treble"})p.add(std::make_unique<juce::AudioParameterFloat>(juce::String(id)+n,juce::String(id)+" "+n,-12.f,12.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("presence"+n,"Presence "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("resonance"+n,"Resonance "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterBool>("mute"+n,"Mute "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("solo"+n,"Solo "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("polarity"+n,"Polarity "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("cab"+n,"Cab "+n,true));p.add(std::make_unique<juce::AudioParameterFloat>("cablow"+n,"Cab Low "+n,20.f,500.f,70.f));p.add(std::make_unique<juce::AudioParameterFloat>("cabhigh"+n,"Cab High "+n,1500.f,20000.f,9000.f));}for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterFloat>("bandtone"+n,"Matrix Band Tone "+n,-12.f,12.f,0.f));}
    for(int i=1;i<=3;++i) p.add(std::make_unique<juce::AudioParameterChoice>("cabtype"+juce::String(i),"Cabinet source "+juce::String(i),juce::StringArray{"Filters only","V30 / SM57","Jensen / SM57","User IR"},1));
    auto number=[&](const char* id,const char* name,float low,float high,float value) { p.add(std::make_unique<juce::AudioParameterFloat>(id,name,low,high,value)); };
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
    number("lowcomp","Matrix LOW DI compression",0,1,.35f);
    return p;
}

juce::ValueTree ChimeraProcessor::captureCore()
{
    auto saved=state.copyState();
    saved.removeChild(saved.getChildWithName("USER_IRS"),nullptr);
    saved.removeChild(saved.getChildWithName("COMPARISONS"),nullptr);
    saved.appendChild(library.save(),nullptr); saved.setProperty("schemaVersion",3,nullptr);
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
    saved.appendChild(slots,nullptr);auto xml=saved.createXml();copyXmlToBinary(*xml,data);
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
        if(id.startsWith("cabtype") || id=="gateon" || id=="output" || id=="lowcomp") value=0;
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
    restoreCore(restored);
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

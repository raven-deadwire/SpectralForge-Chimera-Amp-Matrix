#include "GuitarSignaturePresets.h"
#include "FactoryNativeVoicing.h"
#include "PluginProcessor.h"
#include <algorithm>
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "ReleaseInfo.h"

namespace {
std::pair<int,int> nativeSelectionFromLegacy(int model,int channel) noexcept {
    const auto& panel=spectralforge::ampNativePanel(model);
    // The first appended release represented Model T input routing as channels,
    // and offered only the SLO overdrive circuit. Keep those meanings on recall.
    if(model==19)return {0,juce::jlimit(0,(int)panel.routes.size()-1,channel)};
    if(model==21)return {1,0};
    return {model>=spectralforge::legacyAmpModelCount?juce::jlimit(0,(int)panel.channels.size()-1,channel):panel.defaultChannel,0};
}
}

ChimeraProcessor::ChimeraProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true))
{
    clearMidi();
    boardParameters.bind(state);
    ampSelection.bind(state);
    nativeAmps.bind(state);nativePost.bind(state);
    gateAfterRig=state.getRawParameterValue("gateAfterRig");
    gateRangeDb=state.getRawParameterValue("gateRangeDb");
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
ChimeraProcessor::~ChimeraProcessor()
{
    const spectralforge::lifecycle::Scope trace("processor.destroy", this);
    releaseResources();
}
void ChimeraProcessor::releaseResources()
{
    const spectralforge::lifecycle::Scope trace("processor.release", this);
    // The host has stopped processing. Signal both workers before joining
    // either, then reclaim ALL IR kernels while the plugin is still loaded.
    // Each juce::dsp::Convolution owns a further background loader: merely
    // stopping IRLibrary left those threads alive until member destruction.
    library.requestStop(); tuner.requestStop();
    library.stop(); tuner.stop();
    for (int lane=0; lane<3; ++lane) engine.cabinet(lane).clear();
    jassert(backgroundResourcesReleased());
}
void ChimeraProcessor::prepareToPlay(double sr,int block)
{
    const spectralforge::lifecycle::Scope trace("processor.prepare", this);
    releaseResources(); cpuAverage.store(0);cpuPeak.store(0);rate=sr; maximumBlock=juce::jmax(1,block);
    const juce::dsp::ProcessSpec spec{sr,(juce::uint32)maximumBlock,(juce::uint32)getTotalNumOutputChannels()};
    engine.prepare(spec); preFX.prepare(spec); pedalBoard.prepare(spec); postFX.prepare(spec);utilities.prepare(spec); tuner.prepare(sr);
    audioBoard=boardParameters.read();
    postRigGate.prepare(spec,juce::jmax(preFX.latency(true),pedalBoard.maximumLatency()+preFX.transpose.latency())+engine.latency());
    preReduction.store(0);postReduction.store(0);for(auto& meter:postPeaks)meter.store(0);
    for(auto& meter:nativePostMeters)meter.store(0);
    std::array<int,3> sources{};
    for(int i=0;i<3;++i) sources[i]=(int)laneParameters[i][16]->load();
    library.prepare(spec,sources);
    inputGain.reset(sr,.020); outputGain.reset(sr,.020); tuningMute.reset(sr,.005);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[input]->load()));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(globals[output]->load()));
    tuningMute.setCurrentAndTargetValue(1);
    engine.setOversampling((int)globals[os]->load());
    const bool pitchActive=globals[pitchOn]->load()>.5f && int(globals[semitones]->load())!=0;
    setLatencySamples(postFX.latency()+engine.latency()+(audioBoard.enabled ? pedalBoard.latency(audioBoard)+(pitchActive ? preFX.transpose.latency() : 0) : preFX.latency(pitchActive)));
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
    if(resetPending.exchange(false)) {engine.reset();preFX.reset();pedalBoard.reset();postRigGate.reset();postFX.reset();utilities.reset();}
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
    measureStage(0,buffer);
    tuner.push(buffer,value(tunerOn)>.5f);
    auto fx=spectralforge::readFX(fxParameters);fx.envelopeFirst=preOrderParameter->load()>.5f;fx.boostAfterDrive=gainOrderParameter->load()>.5f;for(size_t i=0;i<modelParameters.size();++i)fx.models[i]=(int)modelParameters[i]->load();if(fx.delaySync)fx.delayMs=60000.f/tempoMeter.load();
    fx.postNative=nativePost.read();
    const bool pitching=value(pitchOn)>.5f && int(value(semitones))!=0;
    const auto before=boardEditSequence.load(std::memory_order_acquire);
    if((before&1u)==0) {
        const auto candidate=boardParameters.read();
        if(before==boardEditSequence.load(std::memory_order_acquire)) audioBoard=candidate;
    }
    const bool gateAtOutput=gateAfterRig->load()>.5f;
    const float gateRange=gateRangeDb->load();
    const int preLatency=audioBoard.enabled ? pedalBoard.latency(audioBoard)+(pitching ? preFX.transpose.latency() : 0) : preFX.latency(pitching);
    postRigGate.detect(buffer,value(gateOn)>.5f,value(threshold),value(release),value(hold),preLatency+engine.latency(),gateRange);
    if(audioBoard.enabled) {
        preFX.gate.process(buffer,!gateAtOutput && value(gateOn)>.5f,value(threshold),value(release),value(hold),gateRange);
        preFX.transpose.process(buffer,pitching,(int)value(semitones));
        pedalBoard.process(buffer,audioBoard);
    } else preFX.process(buffer,!gateAtOutput && value(gateOn)>.5f,value(threshold),value(release),value(hold),pitching,(int)value(semitones),fx,gateRange);
    boardReduction.store(audioBoard.enabled ? pedalBoard.compressorReduction() : 0.f);
    preReduction.store(audioBoard.enabled ? pedalBoard.compressorReduction() : preFX.compressor.reduction());
    measureStage(1,buffer);
    const int latency=postFX.latency()+engine.latency()+(audioBoard.enabled ? pedalBoard.latency(audioBoard)+(pitching ? preFX.transpose.latency() : 0) : preFX.latency(pitching));
    if(getLatencySamples()!=latency) setLatencySamples(latency);
    engine.setOversampling((int)value(os));
    std::array<spectralforge::LaneState,3> lanes{};
    for(int i=0;i<3;++i)
    {
        auto f=[this,i](int k){return laneParameters[i][(size_t)k]->load();};
        auto& lane=lanes[i];
        lane.amp=ampSelection.model(i); lane.ampChannel=ampSelection.channel(i,lane.amp);
        lane.drive=f(1); lane.levelDb=f(2); lane.bass=f(3); lane.lowMid=f(4);
        lane.native=nativeAmps.read(spectralforge::ampNativeContext((int)value(mode),i));
        if(lane.native.enabled) {lane.amp=lane.native.model;lane.ampChannel=lane.native.channel;lane.levelDb=0.f;}
        lane.highMid=f(5); lane.treble=f(6); lane.presence=f(7); lane.resonance=f(8); lane.bandTone=f(9);
        lane.mute=f(10)>.5f; lane.solo=f(11)>.5f; lane.polarity=f(12)>.5f; lane.cab=f(13)>.5f;
        lane.cabLow=f(14); lane.cabHigh=f(15); lane.ampEnabled=f(17)>.5f;
        engine.cabinet(i).requestedSource.store((int)f(16));
    }
    lanes[0].lowComp=lowCompParameter->load();lanes[0].lowAmpMix=lowAmpMixParameter->load();
    const bool dualCross=value(mode)==1 && extras[dualType]->load()>.5f;
    engine.process(buffer,(spectralforge::RoutingMode)(int)value(mode),dualCross ? extras[dualFrequency]->load() : value(x1),value(x2),lanes,audioBoard.enabled ? &pedalBoard.cleanOutput() : &preFX.cleanOutput(),dualCross,extras[dualBlend]->load());
    lowCompGain.store(engine.lowReduction());
    if(gateAtOutput)postRigGate.apply(buffer);
    gateGain.store(gateAtOutput ? postRigGate.reduction() : preFX.gate.reduction());
    measureStage(2,buffer);
    postFX.process(buffer,fx);
    measureStage(3,buffer);
    postReduction.store(postFX.compressorReduction());
    for(int section=0;section<3;++section)nativePostMeters[(size_t)section].store(postFX.nativeMeter(section));
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
    measureStage(4,buffer);
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
juce::AudioProcessorValueTreeState::ParameterLayout ChimeraProcessor::layout(){juce::AudioProcessorValueTreeState::ParameterLayout p;p.add(std::make_unique<juce::AudioParameterChoice>("mode","Routing",juce::StringArray{"Classic","Dual","Matrix"},0));p.add(std::make_unique<juce::AudioParameterFloat>("x1","X1",60.f,350.f,150.f));p.add(std::make_unique<juce::AudioParameterFloat>("x2","X2",500.f,4000.f,1200.f));for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterChoice>("amp"+n,"Amp "+n,spectralforge::legacyAmpNames(),2));p.add(std::make_unique<juce::AudioParameterFloat>("drive"+n,"Drive "+n,0.f,1.f,.35f));p.add(std::make_unique<juce::AudioParameterFloat>("level"+n,"Level "+n,-24.f,12.f,0.f));for(auto id:{"bass","lowmid","highmid","treble"})p.add(std::make_unique<juce::AudioParameterFloat>(juce::String(id)+n,juce::String(id)+" "+n,-12.f,12.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("presence"+n,"Presence "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterFloat>("resonance"+n,"Resonance "+n,0.f,10.f,0.f));p.add(std::make_unique<juce::AudioParameterBool>("mute"+n,"Mute "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("solo"+n,"Solo "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("polarity"+n,"Polarity "+n,false));p.add(std::make_unique<juce::AudioParameterBool>("cab"+n,"Cab "+n,true));p.add(std::make_unique<juce::AudioParameterFloat>("cablow"+n,"Cab Low "+n,20.f,500.f,70.f));p.add(std::make_unique<juce::AudioParameterFloat>("cabhigh"+n,"Cab High "+n,1500.f,20000.f,9000.f));}for(int i=1;i<=3;++i){auto n=juce::String(i);p.add(std::make_unique<juce::AudioParameterFloat>("bandtone"+n,"Matrix Band Tone "+n,-12.f,12.f,0.f));}
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
    spectralforge::addPedalBoardParameters(p);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"gateAfterRig",1},"Gate after rig (input detector)",false));
    spectralforge::addAmpSelectionParameters(p);
    spectralforge::addAmpNativeParameters(p);
    spectralforge::addPostNativeParameters(p);
    spectralforge::appendNewAmpNativeParameters(p);
    // Append after ALL released parameters, including native model banks.
    // Version hint 2 also keeps this after the released AU hint-1 parameters.
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateRangeDb",2},"Gate range",
        juce::NormalisableRange<float>{0.f,spectralforge::NoiseGate::fullRangeDb,.1f},spectralforge::NoiseGate::fullRangeDb,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v,int) {return v>=spectralforge::NoiseGate::fullRangeDb ? juce::String("Full") : juce::String(v,1)+" dB";})
            .withValueFromStringFunction([](const juce::String& text) {return text.trim().equalsIgnoreCase("Full") ? spectralforge::NoiseGate::fullRangeDb : text.getFloatValue();})));
    spectralforge::appendNewAmpNativeParameters(p,spectralforge::firstOriginalAmpModel,spectralforge::ampModelCount);
return p;
}

juce::ValueTree ChimeraProcessor::captureCore()
{
    auto saved=state.copyState();
    saved.removeChild(saved.getChildWithName("USER_IRS"),nullptr);
    saved.removeChild(saved.getChildWithName("COMPARISONS"),nullptr);
    saved.appendChild(library.save(),nullptr); saved.setProperty("schemaVersion",8,nullptr);
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
    boardUndo.clear();boardRedo.clear();midiLearn.store(-1);
    const bool missingNative=!restored.getChildWithProperty("id",spectralforge::ampNativeModelID(0)).isValid();
    const bool missingPost=!restored.getChildWithProperty("id",spectralforge::postNativeModelID(0)).isValid();
    const bool missingBoard=!restored.getChildWithProperty("id",spectralforge::pedalModelID(0)).isValid();
    const auto defaults=state.copyState();
    for(auto child:defaults)
    {
        const auto id=child.getProperty("id").toString();
        if(id.isEmpty() || restored.getChildWithProperty("id",id).isValid()) continue;
        auto* parameter=state.getParameter(id);if(!parameter) continue;
        float value=parameter->convertFrom0to1(parameter->getDefaultValue());
        // Missing gateRangeDb uses its Full default, including old A/B slots.
        // Never inherit the current session's finite floor during migration.
        if(id.startsWith("cabtype") || id=="gateon" || id=="output" || id=="lowcomp" || id=="preorder" || id=="gainorder" || id=="boardEnabled") value=0;
        if((id.startsWith("nativeAmp_")&&id.endsWith("_enabled")) || (id.startsWith("pn_")&&id.endsWith("_native")))value=0;
        juce::ValueTree item("PARAM");item.setProperty("id",id,nullptr);item.setProperty("value",value,nullptr);restored.appendChild(item,nullptr);
    }
    library.restore(restored.getChildWithName("USER_IRS"));restored.removeChild(restored.getChildWithName("USER_IRS"),nullptr);
    restored.removeChild(restored.getChildWithName("COMPARISONS"),nullptr);state.replaceState(restored);resetPending.store(true);
    if(missingNative || missingPost || missingBoard)seedNativeSelections(missingBoard,missingNative,missingPost);
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
    if(index<0 || index>=spectralforge::selectablePresetCount)return;
    {
        const auto snapshot=spectralforge::isOriginalPreset(index)
            ? spectralforge::originalPresetSnapshot(state,index-spectralforge::originalPresetStart)
            : spectralforge::isGuitarSignature(index)
            ? spectralforge::guitarSignatureSnapshot(state,index-spectralforge::factoryPresetCount)
            : spectralforge::factoryNativeSnapshot(state,index);
        state.replaceState(snapshot);
        // Canonicalize host values as well as APVTS values. Bool/choice adapters
        // may already cache the snapped value of a noncanonical host write and
        // suppress an otherwise identical replaceState update.
        for(auto* raw:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
            const auto node=snapshot.getChildWithProperty("id",parameter->paramID);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(node.getProperty("value"))));
        }
        boardUndo.clear();boardRedo.clear();
        if(spectralforge::isSignaturePreset(index))applyPresetIRTargets(index);
        resetPending.store(true);
    }
}

void ChimeraProcessor::applyPresetIRTargets(int index) {
    if(!spectralforge::isSignaturePreset(index))return;
    const auto entries=spectralforge::IRCollection::scan(spectralforge::IRCollection::roots(),true);
    for(int lane=0;lane<3;++lane) {
        const auto target=juce::String::fromUTF8(spectralforge::presetIRTarget(index,lane));
        const auto expectedHash=juce::String::fromUTF8(spectralforge::presetIRTargetHash(index,lane));
        if(target.isEmpty())continue;
        const bool tagged=target.startsWith("tag:");
        const auto terms=tagged ? juce::StringArray::fromTokens(target.substring(4),"|","") : juce::StringArray{};
        const bool catalogTarget=!tagged && std::any_of(entries.begin(),entries.end(),[&](const auto& entry){return entry.reference && entry.name==target;});
        const spectralforge::IRCollection::Entry* match=nullptr;
        for(const auto& entry:entries) {
            if(!entry.ready())continue;
            bool matches=false;
            if(tagged) {
                const auto haystack=entry.name+" "+entry.displayName()+" "+juce::JSON::toString(entry.tags.json(),true);
                matches=true;for(const auto& term:terms)if(!haystack.containsIgnoreCase(term.trim())){matches=false;break;}
            } else matches=entry.name==target && (!catalogTarget || entry.reference);
            if(matches && expectedHash.isNotEmpty() && entry.factorySource==0)
                matches=spectralforge::IRCollection::matchesExpectedHash(entry.file,expectedHash);
            if(!matches)continue;
            match=&entry;
            if(entry.reference)break;
        }
        if(match && match->factorySource!=0) setRawParameter("cabtype"+juce::String(lane+1),float(match->factorySource));
        else if(match && match->file.existsAsFile()) loadIR(lane,match->file);
        else setRawParameter("cabtype"+juce::String(lane+1),0.f);
    }
}

int ChimeraProcessor::ampContext(int lane) const noexcept {
    return spectralforge::ampNativeContext((int)globals[mode]->load(),lane);
}
int ChimeraProcessor::selectedAmpModel(int lane) const noexcept {
    const auto native=nativeAmps.read(ampContext(lane));
    return native.enabled?native.model:ampSelection.model(lane);
}
int ChimeraProcessor::selectedAmpChannel(int lane) const noexcept {
    const auto native=nativeAmps.read(ampContext(lane));
    if(native.enabled)return native.channel;
    const int model=selectedAmpModel(lane);
    return nativeSelectionFromLegacy(model,ampSelection.channel(lane,model)).first;
}
int ChimeraProcessor::selectedAmpNativeRoute(int lane) const noexcept {
    const int context=ampContext(lane);const auto native=nativeAmps.read(context);
    if(native.enabled)return native.inputRoute;
    const int model=ampSelection.model(lane);
    if(model==19)return nativeSelectionFromLegacy(model,ampSelection.channel(lane,model)).second;
    return juce::jlimit(0,(int)spectralforge::ampNativePanel(model).routes.size()-1,
        juce::roundToInt(spectralforge::AmpNativeParameterCache::value(nativeAmps.routes[(size_t)context][(size_t)model],0.f)));
}
void ChimeraProcessor::activateNativeAmp(int lane) {
    if(lane<0 || lane>=3)return;
    const int context=ampContext(lane);const auto native=nativeAmps.read(context);
    if(native.enabled) {
        if(spectralforge::ampRequiresNative(native.model))setRawParameter(spectralforge::ampNativeEnabledID(context),1.f);
        return;
    }
    const int model=ampSelection.model(lane);const auto selection=nativeSelectionFromLegacy(model,ampSelection.channel(lane,model));
    setRawParameter(spectralforge::ampNativeModelID(context),float(model));
    setRawParameter(spectralforge::ampNativeChannelID(context,model),float(selection.first));
    if(model>=spectralforge::legacyAmpModelCount)setRawParameter(spectralforge::ampNativeRouteID(context,model),float(selection.second));
    setRawParameter(spectralforge::ampNativeEnabledID(context),1.f);
}
void ChimeraProcessor::activateNativePost(int section) {
    if(section>=0 && section<3)setRawParameter(spectralforge::postNativeModeID(section),1.f);
}
void ChimeraProcessor::seedNativeSelections(bool seedBoard,bool seedAmps,bool seedPost) {
    // Populate the new presentation from released selections. Old projects keep
    // their audio until an explicit edit; native banks remain independently saved.
    for(int context=0;seedAmps && context<spectralforge::ampNativeContextCount;++context) {
        const int lane=context==0?0:context<3?context-1:context-3;
        const int model=ampSelection.model(lane);
        setRawParameter(spectralforge::ampNativeEnabledID(context),0.f);
        setRawParameter(spectralforge::ampNativeModelID(context),float(model));
        const auto selection=nativeSelectionFromLegacy(model,ampSelection.channel(lane,model));
        setRawParameter(spectralforge::ampNativeChannelID(context,model),float(selection.first));
        setRawParameter(spectralforge::ampNativeRouteID(context,model),float(selection.second));
    }
    constexpr const char* postOn[]{"buscompon","preampon","eqon"};
    for(int s=0;seedPost && s<3;++s) {
        const int model=juce::jlimit(0,2,(int)modelParameters[(size_t)(7+s)]->load());
        setRawParameter(spectralforge::postNativeModeID(s),0.f);
        setRawParameter(spectralforge::postNativeModelID(s),float(model));
        setRawParameter(spectralforge::postNativeBypassID(s,model),state.getRawParameterValue(postOn[s])->load()>.5f?0.f:1.f);
    }
    if(!seedBoard)return;
    constexpr int families[]{3,4,5,6,0},first[]{6,11,16,21,1};
    constexpr const char* on[]{"precompon","filteron","fuzzon","booston","preon"};
    for(int owner=0;owner<5;++owner) {
        const int model=first[owner]+juce::jlimit(0,4,(int)modelParameters[(size_t)families[owner]]->load());
        setRawParameter(spectralforge::pedalModelID(owner),float(model));
        setRawParameter(spectralforge::pedalBypassID(owner,model),state.getRawParameterValue(on[owner])->load()>.5f?0.f:1.f);
        setRawParameter(spectralforge::pedalOrderID(owner),float(owner));
    }
    if(preOrderParameter->load()>.5f) {setRawParameter(spectralforge::pedalOrderID(0),1);setRawParameter(spectralforge::pedalOrderID(1),0);}
    if(gainOrderParameter->load()>.5f) {setRawParameter(spectralforge::pedalOrderID(3),4);setRawParameter(spectralforge::pedalOrderID(4),3);}
}
void ChimeraProcessor::setAmpModel(int lane,int model) {
    if(lane<0 || lane>=3 || model<0 || model>=spectralforge::ampModelCount)return;
    setRawParameter(spectralforge::ampNativeModelID(ampContext(lane)),float(model));
    setRawParameter(spectralforge::ampNativeEnabledID(ampContext(lane)),1.f);
    if(model<spectralforge::legacyAmpModelCount) {
        // Set the legacy value before deactivating the extension bank. An old
        // host automation write alone never deactivates an explicit new model.
        setRawParameter("amp"+juce::String(lane+1),float(model));
        setRawParameter(spectralforge::ampExtensionID(lane),0.f);
    } else if(model<spectralforge::releasedNativeAmpModelCount) {
        setRawParameter(spectralforge::ampExtensionID(lane),float(model-spectralforge::legacyAmpModelCount+1));
    }
}
void ChimeraProcessor::setAmpChannel(int lane,int channel) {
    if(lane<0 || lane>=3)return;
    const int model=selectedAmpModel(lane);
    if(channel<0 || channel>=(int)spectralforge::ampNativePanel(model).channels.size())return;
    activateNativeAmp(lane);
    setRawParameter(spectralforge::ampNativeChannelID(ampContext(lane),model),float(channel));
    if(model==19)setRawParameter(spectralforge::ampChannelID(lane,model),float(selectedAmpNativeRoute(lane)));
    else if(model==21) {if(channel==1)setRawParameter(spectralforge::ampChannelID(lane,model),0.f);}
    else if(spectralforge::isNewAmpModel(model) && channel<spectralforge::newAmpChannelCount(model))setRawParameter(spectralforge::ampChannelID(lane,model),float(channel));
}
void ChimeraProcessor::setAmpNativeRoute(int lane,int route) {
    if(lane<0 || lane>=3)return;
    const int model=selectedAmpModel(lane);
    if(route<0 || route>=(int)spectralforge::ampNativePanel(model).routes.size())return;
    activateNativeAmp(lane);setRawParameter(spectralforge::ampNativeRouteID(ampContext(lane),model),float(route));
    if(model==19)setRawParameter(spectralforge::ampChannelID(lane,model),float(route));
}
void ChimeraProcessor::resetAmpSelection() {
    for(int lane=0;lane<3;++lane) {
        setRawParameter(spectralforge::ampExtensionID(lane),0.f);
        for(int bank=0;bank<spectralforge::extendedAmpBankCount;++bank) {
            const int model=spectralforge::legacyAmpModelCount+bank;
            setRawParameter(spectralforge::ampChannelID(lane,model),float(spectralforge::newAmpDefaultChannel(model)));
        }
    }
}
void ChimeraProcessor::setRawParameter(const juce::String& id,float value) {
    if(auto* parameter=state.getParameter(id)) {
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(value));parameter->endChangeGesture();
    }
}
void ChimeraProcessor::rememberPedalEdit() {
    juce::ValueTree snapshot("BOARD_EDIT");
    for(auto child:state.copyState()) if(child.getProperty("id").toString().startsWith("board"))snapshot.appendChild(child.createCopy(),nullptr);
    boardUndo.push_back(snapshot);if(boardUndo.size()>32)boardUndo.erase(boardUndo.begin());boardRedo.clear();
}
void ChimeraProcessor::setPedalBoardEnabled(bool enabled) {
    if(boardParameters.read().enabled==enabled)return;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    setRawParameter("boardEnabled",enabled?1.f:0.f);
    boardEditSequence.fetch_add(1,std::memory_order_release);resetPending.store(true);
}
void ChimeraProcessor::setPedalModel(int owner,int model) {
    if(owner<0 || owner>=5 || model<0 || model>=spectralforge::pedalModelCount || !spectralforge::pedalModel(model).implemented)return;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    midiLearn.store(-1);
    // Retire controller bindings for this owner when an explicit replacement is made.
    const auto prefix="board"+juce::String(owner)+"_";
    for(auto& binding:midiMap) {
        const auto index=binding.load();
        if(index>=0 && index<getParameters().size()) if(auto* p=dynamic_cast<juce::AudioProcessorParameterWithID*>(getParameters()[index]))
            if(p->paramID.startsWith(prefix))binding.store(-1);
    }
    setRawParameter(spectralforge::pedalModelID(owner),float(model));
    const auto usedKey="boardBankUsed_"+juce::String(owner)+"_"+juce::String(model);
    const bool used=(bool)state.state.getProperty(usedKey,false);
    if(model)state.state.setProperty(usedKey,true,nullptr);
    if(model)setRawParameter(spectralforge::pedalBypassID(owner,model),0);
    for(int c=0;!used && c<spectralforge::pedalModel(model).controlCount;++c)
        setRawParameter(spectralforge::pedalControlID(owner,model,c),spectralforge::pedalModel(model).controls[(size_t)c].initial);
    setRawParameter("boardEnabled",1);
    boardEditSequence.fetch_add(1,std::memory_order_release);
}
void ChimeraProcessor::movePedal(int owner,int direction) {
    const auto board=boardParameters.read();int position=-1;
    for(int i=0;i<5;++i)if(board.order[(size_t)i]==owner)position=i;
    const int destination=position+direction;
    if(position<0 || destination<0 || destination>=5)return;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    setRawParameter(spectralforge::pedalOrderID(position),float(board.order[(size_t)destination]));
    setRawParameter(spectralforge::pedalOrderID(destination),float(owner));
    setRawParameter("boardEnabled",1);
    boardEditSequence.fetch_add(1,std::memory_order_release);
}
bool ChimeraProcessor::duplicatePedal(int owner) {
    if(owner<0 || owner>=5)return false;
    const auto board=boardParameters.read();const auto source=board.instances[(size_t)owner];if(!source.model)return false;
    int target=-1;for(int i:board.order)if(!board.instances[(size_t)i].model) {target=i;break;}
    if(target<0)return false;
    rememberPedalEdit();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    midiLearn.store(-1);
    const auto prefix="board"+juce::String(target)+"_";
    for(auto& binding:midiMap) {const int index=binding.load();if(index>=0&&index<getParameters().size())
        if(auto* p=dynamic_cast<juce::AudioProcessorParameterWithID*>(getParameters()[index]))if(p->paramID.startsWith(prefix))binding.store(-1);}
    setRawParameter(spectralforge::pedalModelID(target),float(source.model));
    state.state.setProperty("boardBankUsed_"+juce::String(target)+"_"+juce::String(source.model),true,nullptr);
    setRawParameter(spectralforge::pedalBypassID(target,source.model),source.bypass?1.f:0.f);
    for(int c=0;c<spectralforge::pedalModel(source.model).controlCount;++c)
        setRawParameter(spectralforge::pedalControlID(target,source.model,c),source.controls[(size_t)c]);
    setRawParameter("boardEnabled",1);
    boardEditSequence.fetch_add(1,std::memory_order_release);return true;
}
void ChimeraProcessor::undoPedalEdit(bool redo) {
    auto& from=redo ? boardRedo : boardUndo;auto& to=redo ? boardUndo : boardRedo;if(from.empty())return;
    juce::ValueTree current("BOARD_EDIT");
    for(auto child:state.copyState())if(child.getProperty("id").toString().startsWith("board"))current.appendChild(child.createCopy(),nullptr);
    to.push_back(current);const auto next=from.back();from.pop_back();boardEditSequence.fetch_add(1,std::memory_order_acq_rel);
    for(auto child:next)setRawParameter(child.getProperty("id").toString(),float(child.getProperty("value")));
    boardEditSequence.fetch_add(1,std::memory_order_release);
}

void ChimeraProcessor::measureStage(int index,const juce::AudioBuffer<float>& buffer) {
    double energy=0;float peak=0;const int count=buffer.getNumSamples()*buffer.getNumChannels();
    for(int c=0;c<buffer.getNumChannels();++c)for(int n=0;n<buffer.getNumSamples();++n) {
        const float v=buffer.getSample(c,n);energy+=double(v)*v;peak=juce::jmax(peak,std::abs(v));
    }
    stageRms[(size_t)index].store(count>0 ? float(std::sqrt(energy/count)) : 0.f);stagePeaks[(size_t)index].store(peak);
}
juce::String ChimeraProcessor::diagnosticReport() const {
    auto object=std::make_unique<juce::DynamicObject>();
    object->setProperty("schema",1);object->setProperty("version",spectralforge::release::version);
    object->setProperty("build_revision",spectralforge::release::revision);
    object->setProperty("sample_rate",getSampleRate());object->setProperty("host_block_hint",getBlockSize());
    object->setProperty("channels",getTotalNumOutputChannels());object->setProperty("reported_latency_samples",getLatencySamples());
    object->setProperty("mode",int(globals[mode]->load()));object->setProperty("universal_board",boardParameters.enabled->load()>.5f);
    object->setProperty("transpose_enabled",globals[pitchOn]->load()>.5f);object->setProperty("semitones",int(globals[semitones]->load()));
    object->setProperty("oversampling_index",int(globals[os]->load()));object->setProperty("output_trim_db",globals[output]->load());
    object->setProperty("post_rig_gate",gateAfterRig->load()>.5f);object->setProperty("callback_average_percent",cpuAverage.load());
    object->setProperty("callback_peak_percent",cpuPeak.load());object->setProperty("measurements","Last processed block, unweighted RMS/peak; not LUFS or whole-PC CPU. No audio is included.");
    juce::Array<juce::var> stages;const char* names[]{"INPUT_TRIM","PRE","RIG_GATE","POST","OUTPUT"};
    for(int i=0;i<5;++i) {auto stage=std::make_unique<juce::DynamicObject>();stage->setProperty("stage",names[i]);
        stage->setProperty("rms_dbfs",juce::Decibels::gainToDecibels(stageRms[(size_t)i].load(),-120.f));
        stage->setProperty("peak_dbfs",juce::Decibels::gainToDecibels(stagePeaks[(size_t)i].load(),-120.f));stages.add(juce::var(stage.release()));}
    object->setProperty("stages",stages);return juce::JSON::toString(juce::var(object.release()),false);
}

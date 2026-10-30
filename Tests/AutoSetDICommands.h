#pragma once

// Existing processor test seam; checks the real cabinet worker before capture.
struct ChimeraCabExportTestAccess {
    static bool settled(ChimeraProcessor& p){return p.engine.audibleCabinetsSettled();}
    static int toneLatency(ChimeraProcessor& p){return p.getLatencySamples()-p.postFX.latency();}
};

// Offline entry points in the existing native Auto Set test executable. These
// use the production capture, controller and GraphicalEQ, never a Python DSP copy.
namespace autoSetDI {
void setRaw(ChimeraProcessor& p,const juce::String& id,float v) {
    auto* q=p.parameters().getParameter(id);check(q!=nullptr,"Unknown parameter "+id.toStdString());
    check(std::isfinite(v) && v>=q->getNormalisableRange().start && v<=q->getNormalisableRange().end,"Parameter out of range");
    q->setValueNotifyingHost(q->convertTo0to1(v));
}
struct Audio {double rate;juce::AudioBuffer<float> buffer;};
Audio read(const juce::File& file) {
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));
    check(reader && reader->numChannels==1,"Expected mono WAV");
    check(reader->sampleRate==44100 || reader->sampleRate==48000 || reader->sampleRate==96000,"Expected native 44.1/48/96 kHz");
    check(reader->lengthInSamples>=6*reader->sampleRate && reader->lengthInSamples<=120*reader->sampleRate,"WAV length must be 6..120 s");
    Audio a{reader->sampleRate,juce::AudioBuffer<float>(1,int(reader->lengthInSamples))};
    check(reader->read(&a.buffer,0,a.buffer.getNumSamples(),0,true,false),"WAV read failed");
    for(int n=0;n<a.buffer.getNumSamples();++n)check(std::isfinite(a.buffer.getSample(0,n)),"Nonfinite WAV sample");
    return a;
}
void write(const juce::File& file,const juce::AudioBuffer<float>& b,double rate) {
    check(!file.exists(),"Output already exists: "+file.getFullPathName().toStdString());
    for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)
        check(std::isfinite(b.getSample(c,n)) && std::abs(b.getSample(c,n))<.999f,"Refusing clipped/nonfinite capture; revise and freeze a gain-safe profile before testing");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(file.createOutputStream().release(),rate,unsigned(b.getNumChannels()),32,{},0));
    check(writer && writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples()),"WAV write failed");
}
void json(const juce::File& file,juce::DynamicObject* object) {
    check(!file.exists(),"Report already exists");check(file.replaceWithText(juce::JSON::toString(juce::var(object))),"JSON write failed");
}
juce::DynamicObject* parameters(ChimeraProcessor& p,bool eqOnly=false) {
    auto* out=new juce::DynamicObject;
    for(auto* q:p.getParameters())if(auto* named=dynamic_cast<juce::RangedAudioParameter*>(q))
        if(!eqOnly || named->paramID.startsWith("toneEQ_") || named->paramID.startsWith("dynamicEQ"))out->setProperty(named->paramID,named->convertFrom0to1(q->getValue()));
    return out;
}
void capture(const juce::File& input,const juce::File& profileFile,const juce::File& dir) {
    auto a=read(input);const auto profile=juce::JSON::parse(profileFile);
    check(profile.isObject() && profile["parameters"].isObject(),"Profile needs a parameters object");
    check(dir.createDirectory().wasOk(),"Output directory");
    auto p=std::make_unique<ChimeraProcessor>();
    for(const auto& property:profile["parameters"].getDynamicObject()->getProperties())
        setRaw(*p,property.name.toString(),float(property.value));
    if(profile.hasProperty("amp_model")) {
        const int model=int(profile["amp_model"]);check(model>=0 && model<spectralforge::ampModelCount,"Invalid model");p->setAmpModel(0,model);
    }
    if(profile["ir"].isString()) {
        auto ir=profileFile.getParentDirectory().getChildFile(profile["ir"].toString());
        check(p->loadIR(0,ir).wasOk(),"Cannot load IR");
    }
    // This narrowly scoped renderer is for a fixed mono Classic amp/cab chain.
    check(get(*p,"mode")==0 && get(*p,"gateon")<.5f && get(*p,"gate2On")<.5f && get(*p,"expOn")<.5f
          && get(*p,"transposeon")<.5f && get(*p,"preon")<.5f,"Profile must isolate Classic amp/cab without gates, transpose or pre drive");
    for(const auto* id:{"boardEnabled","precompon","filteron","booston","fuzzon","doubleron"})
        check(get(*p,id)<.5f,"Profile must bypass all other PRE processing");
    p->setRateAndBufferSizeDetails(a.rate,256);p->prepareToPlay(a.rate,256);
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
    // Allow the real worker to finish, then settle filters with silence. Bounded
    // wait, failure is an error rather than silently capturing fallback audio.
    bool ready=false;
    for(int k=0;k<1000;++k){block.clear();p->processBlock(block,midi);if(k>=int(a.rate/256) && ChimeraCabExportTestAccess::settled(*p)){ready=true;break;}juce::Thread::sleep(1);}
    check(ready,"Cabinet worker did not settle");
    std::vector<float> mono;p->autoSetCapture().listen(Tap::tone);
    for(int offset=0;offset<a.buffer.getNumSamples();offset+=256) {
        const int count=std::min(256,a.buffer.getNumSamples()-offset);block.setSize(2,count,false,false,true);
        for(int channel=0;channel<2;++channel)block.copyFrom(channel,0,a.buffer,0,offset,count);
        p->processBlock(block,midi);p->autoSetCapture().drainSamples(mono);
    }
    p->autoSetCapture().listen(std::nullopt);check(int(mono.size())==a.buffer.getNumSamples(),"Capture dropped samples");
    juce::AudioBuffer<float> captured(1,int(mono.size()));captured.copyFrom(0,0,mono.data(),int(mono.size()));
    write(dir.getChildFile("pre-tone.wav"),captured,a.rate);
    auto* report=new juce::DynamicObject;
    report->setProperty("sample_rate",a.rate);report->setProperty("samples",int(mono.size()));report->setProperty("reported_latency_samples",p->getLatencySamples());
    report->setProperty("tone_tap_latency_samples",ChimeraCabExportTestAccess::toneLatency(*p));
    report->setProperty("cabinet_settled",ready);report->setProperty("tap","post-amp/cab, pre-Tone EQ, channel average");
    report->setProperty("parameters",juce::var(parameters(*p)));json(dir.getChildFile("capture.json"),report);p->releaseResources();
}
void analyse(const juce::File& input,bool bass,Style style,float strength,const juce::File& dir) {
    auto a=read(input);check(a.buffer.getNumSamples()==int(6*a.rate),"Analysis window must be exactly 6 seconds");
    check(dir.createDirectory().wasOk(),"Output directory");
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(a.rate,256);p->prepareToPlay(a.rate,256);
    Controller controller(*p);
    controller.setOptions({bass?2:1,false,style,false,false,false,strength,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,true,false});
    controller.start();double now=0;
    std::vector<float> signal(a.buffer.getReadPointer(0),a.buffer.getReadPointer(0)+a.buffer.getNumSamples());
    feedCapture(*p,controller,signal,now);
    check(controller.current().phase==Controller::Phase::done,"Controller did not complete");
    auto* report=new juce::DynamicObject;
    report->setProperty("sample_rate",a.rate);report->setProperty("instrument",bass?"bass":"guitar");
    report->setProperty("style",styleName(style));report->setProperty("strength",strength);
    report->setProperty("listening_completed",false);report->setProperty("result",controller.current().text);
    report->setProperty("playing_detected",controller.lastDynamicAnalysis().playingDetected);
    juce::Array<juce::var> nodes;
    for(const auto& n:controller.lastDynamicAnalysis().nodes) {
        auto* node=new juce::DynamicObject;
        node->setProperty("frequency",n.frequency);node->setProperty("threshold_db",n.thresholdDb);
        node->setProperty("range_db",n.rangeDb*strength);node->setProperty("ratio",n.ratio);nodes.add(juce::var(node));
    }
    report->setProperty("nodes",nodes);report->setProperty("applied_parameters",juce::var(parameters(*p,true)));
    auto& eq=p->graphicalEQ(0);eq.reset();auto output=a.buffer;float maximum=0;
    for(int offset=0;offset<output.getNumSamples();offset+=256) {
        float* channel[]{output.getWritePointer(0,offset)};juce::AudioBuffer<float> part(channel,1,std::min(256,output.getNumSamples()-offset));
        eq.process(part);for(int b=0;b<12;++b)maximum=std::max(maximum,eq.dynamicReduction(b));
    }
    report->setProperty("maximum_reduction_db",maximum);
    write(dir.getChildFile("before.wav"),a.buffer,a.rate);write(dir.getChildFile("after.wav"),output,a.rate);
    controller.undo();report->setProperty("undo_parameters",juce::var(parameters(*p,true)));
    json(dir.getChildFile("analysis.json"),report);p->releaseResources();
}
}

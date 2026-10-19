#pragma once
#include <fstream>

namespace presetFinishingComparison {
using namespace spectralforge;

// main ec6fd639 initializes these banks to their registered defaults in all
// four snapshot builders. Only voiceFactoryFinishing now writes them. Restoring
// those defaults reconstructs the prior recipe while using identical DSP.
// Gate, amp, CAB, PRE, POST, routing, levels and metadata must remain untouched.
inline bool finishingParameter(const juce::String& id) {
    return id.startsWith("toneEQ_") || id.startsWith("finalEQ_")
        || id.startsWith("dynamicEQ") || id.startsWith("cleanComp")
        || id.startsWith("liveLimiter");
}
inline void restoreOldRecipe(ChimeraProcessor& p) {
    for(auto* raw:p.getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw))
        if(finishingParameter(q->paramID))q->setValueNotifyingHost(q->getDefaultValue());
    spectralforge::EQEditState::RestoreScope resetOccupancy(p.eqEditing(),p.parameters().copyState(),true);
}
inline void noCompressionOverlap(ChimeraProcessor& p) {
    const auto get=[&](const juce::String& id){return p.parameters().getRawParameterValue(id)->load();};
    if(get("cleanCompOn")<.5f)return;
    const auto board=p.pedalBoardState();
    if(board.enabled) {
        for(const auto& pedal:board.instances)
            require(pedal.bypass || pedal.model<6 || pedal.model>10,
                    "Common compressor duplicates an active PRE compressor");
    } else require(get("precompon")==0,"Common compressor duplicates legacy PRE compression");
    require(get("mode")!=2 || get("lowcomp")==0,"Common compressor duplicates Matrix LOW compression");
    const int model=int(get(postNativeModelID(0)));
    const bool postOn=get(postNativeModeID(0))>.5f
        ? get(postNativeBypassID(0,model))<.5f : get("buscompon")>.5f;
    require(!postOn,"Common compressor duplicates an active POST compressor");
}
struct Stats {double rms{},peak{},attackPeak{};};
inline Stats stats(const std::vector<float>& samples) {
    Stats result;double energy=0;
    for(size_t i=0;i<samples.size();++i) {
        const double value=std::abs(double(samples[i]));energy+=value*value;
        result.peak=std::max(result.peak,value);
        // render() stores L128,R128 per block. Inspect first 20 ms of each
        // known pluck, including unchanged host latency; not an onset estimator.
        const size_t frame=(i/256)*128+i%128;
        if(frame%9600<960)result.attackPeak=std::max(result.attackPeak,value);
    }
    result.rms=std::sqrt(energy/double(samples.size()));return result;
}
inline double db(double x){return 20*std::log10(std::max(1.e-12,x));}
inline void saveWav(const juce::File& file,const std::vector<float>& samples,float gain=1) {
    juce::AudioBuffer<float> b(2,int(samples.size()/2));
    for(size_t i=0;i<samples.size();++i)b.setSample(int(i%256/128),int(i/256*128+i%128),samples[i]*gain);
    auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot create audition WAV");
    stream->setPosition(0);stream->truncate();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));
    require(writer && writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples()),"Cannot write audition WAV");
}
inline void automation(const juce::File& directory) {
    std::ofstream csv(directory.getChildFile("automation.csv").getFullPathName().toStdString());
    require(bool(csv),"Cannot create automation CSV");
    csv<<"index,rate,channels,max_restore_error,max_peak,latency_samples,gestures\n";
    struct Listener final:juce::AudioProcessorParameter::Listener {
        int starts{},ends{},values{};
        void parameterValueChanged(int,float) override{++values;}
        void parameterGestureChanged(int,bool starting) override{starting?++starts:++ends;}
    };
    constexpr const char* ids[]{"toneEQ_b1_gain","toneEQ_b1_frequency","dynamicEQ1_threshold",
        "dynamicEQ1_range","cleanCompMix","liveLimiterCeiling"};
    for(int index:{0,5,9,42,47})for(double rate:{44100.,48000.,96000.})for(int channels:{1,2}) {
        auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        auto buses=a->getBusesLayout();
        buses.inputBuses.set(0,channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        buses.outputBuses.set(0,buses.inputBuses[0]);
        require(a->setBusesLayout(buses) && b->setBusesLayout(buses),"Automation bus configuration refused");
        a->loadFactoryPreset(index);juce::MemoryBlock bytes;a->getStateInformation(bytes);
        b->setStateInformation(bytes.getData(),int(bytes.getSize()));
        Listener listener;for(const char* id:ids)a->parameters().getParameter(id)->addListener(&listener);
        a->prepareToPlay(rate,64);b->prepareToPlay(rate,64);const int latency=a->getLatencySamples();
        juce::AudioBuffer<float> x(channels,64),y(channels,64);juce::MidiBuffer midi;double error=0,peak=0;
        for(int block=0;block<120;++block) {
            const float phase=float(block%17)/16.f;
            const float values[]{-1+2*phase,160+240*phase,-24+12*phase,phase*2,phase,-2+phase};
            for(size_t i=0;i<std::size(ids);++i)for(auto* p:{a.get(),b.get()}) {
                auto* q=p->parameters().getParameter(ids[i]);require(q->isAutomatable(),"Automatable finishing parameter lost its contract");
                q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(values[i]));q->endChangeGesture();
            }
            for(int n=0;n<64;++n)for(int c=0;c<channels;++c)
                x.setSample(c,n,float(.16*std::sin((block*64+n)*2*juce::MathConstants<double>::pi*110/rate))*(c?-.7f:1.f));
            y.makeCopyOf(x,true);a->processBlock(x,midi);b->processBlock(y,midi);
            for(int n=0;n<64;++n)for(int c=0;c<channels;++c) {
                require(std::isfinite(x.getSample(c,n)) && std::isfinite(y.getSample(c,n)),"Automation produced nonfinite audio");
                error=std::max(error,std::abs(double(x.getSample(c,n))-y.getSample(c,n)));
                peak=std::max(peak,std::abs(double(x.getSample(c,n))));
            }
        }
        for(const char* id:ids)a->parameters().getParameter(id)->removeListener(&listener);
        require(listener.starts==720 && listener.ends==720 && listener.values>0,"Unbalanced or missing automation gestures");
        require(a->getLatencySamples()==latency && b->getLatencySamples()==latency,"Automation changed latency");
        require(error<1.e-6 && peak<=std::pow(10.,-.05)+2.e-7,"Automated state recall or limiter bound failed");
        csv<<index<<','<<rate<<','<<channels<<','<<error<<','<<peak<<','<<latency<<','<<listener.starts<<'\n';
        a->releaseResources();b->releaseResources();
    }
    std::cout<<"PASS 30 preset automation routes: 44.1/48/96 kHz, mono/stereo, matched restored output and balanced host gestures\n";
}
inline void run(const juce::File& directory) {
    require(directory.createDirectory().wasOk(),"Cannot create comparison directory");
    std::ofstream csv(directory.getChildFile("comparison.csv").getFullPathName().toStdString());
    require(bool(csv),"Cannot create comparison CSV");
    csv<<"index,name,before_rms_dbfs,after_rms_dbfs,rms_delta_db,match_gain_db,before_peak_dbfs,after_peak_dbfs,crest_delta_db,matched_attack_delta_db,restore_max_error,latency_samples\n";
    csv<<std::setprecision(10);
    std::ofstream audit(directory.getChildFile("signal-path-audit.csv").getFullPathName().toStdString());
    require(bool(audit),"Cannot create preset signal-path audit");
    audit<<"index,name,mode,active_pre_compressors,post_compressor_on,post_eq_on,post_eq_model,matrix_low_comp,common_comp_on,gate_mode,gate_release_ms\n";
    auto baseline=std::make_unique<ChimeraProcessor>(),candidate=std::make_unique<ChimeraProcessor>(),restored=std::make_unique<ChimeraProcessor>();
    for(int index=0;index<selectablePresetCount;++index) {
        candidate->loadFactoryPreset(index);baseline->loadFactoryPreset(index);
        noCompressionOverlap(*candidate);restoreOldRecipe(*baseline);
        const auto get=[&](const juce::String& id){return baseline->parameters().getRawParameterValue(id)->load();};
        const auto board=baseline->pedalBoardState();int preCompressors=0;
        if(board.enabled)for(const auto& pedal:board.instances)preCompressors+=!pedal.bypass && pedal.model>=6 && pedal.model<=10;
        else preCompressors=get("precompon")>.5f?1:0;
        const auto postOn=[&](int section,const char* legacy){return get(postNativeModeID(section))>.5f
            ?get(postNativeBypassID(section,int(get(postNativeModelID(section)))))<.5f:get(legacy)>.5f;};
        audit<<index<<','<<selectablePresetName(index)<<','<<get("mode")<<','<<preCompressors<<','<<postOn(0,"buscompon")
             <<','<<postOn(2,"eqon")<<','<<get(postNativeModelID(2))<<','<<(get("mode")==2?get("lowcomp"):0)
             <<','<<candidate->parameters().getRawParameterValue("cleanCompOn")->load()<<','<<get("gateMode")<<','<<get("gaterelease")<<'\n';
        for(auto* raw:candidate->getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
            require(q->getParameterIndex()==baseline->parameters().getParameter(q->paramID)->getParameterIndex(),"Parameter ordinal changed");
            if(!finishingParameter(q->paramID))
                require(q->getValue()==baseline->parameters().getParameter(q->paramID)->getValue(),"Finishing changed an unrelated parameter");
        }
        juce::MemoryBlock current,old;candidate->getStateInformation(current);baseline->getStateInformation(old);
        restored->setStateInformation(current.getData(),int(current.getSize()));
        const bool bass=isNiflheimrPreset(index)||(index<factoryPresetCount&&juce::String(factoryPresets[size_t(index)].instrument).contains("Bass"));
        const auto before=render(*baseline,bass,450,index),after=render(*candidate,bass,450,index),again=render(*restored,bass,450,index);
        require(before.size()==after.size() && after.size()==again.size(),"Comparison size mismatch");
        double residual=0;for(size_t i=0;i<after.size();++i)residual=std::max(residual,std::abs(double(after[i])-again[i]));
        require(residual<1.e-6,"Preset binary restore changed rendered audio");
        require(candidate->getLatencySamples()==baseline->getLatencySamples(),"Finishing changed reported latency");
        const auto a=stats(before),b=stats(after);require(a.rms>1.e-6 && b.rms>1.e-6,"Silent comparison");
        const double rmsDelta=db(b.rms/a.rms),crestDelta=db(b.peak/b.rms)-db(a.peak/a.rms);
        const double attackDelta=db((b.attackPeak/b.rms)/(a.attackPeak/a.rms));
        csv<<index<<','<<selectablePresetName(index)<<','<<db(a.rms)<<','<<db(b.rms)<<','<<rmsDelta<<','<<-rmsDelta
           <<','<<db(a.peak)<<','<<db(b.peak)<<','<<crestDelta<<','<<attackDelta<<','<<residual<<','<<candidate->getLatencySamples()<<'\n';csv.flush();
        require(std::abs(rmsDelta)<=1.5,"Preset level drift exceeds 1.5 dB");
        require(std::abs(crestDelta)<=1.5 && std::abs(attackDelta)<=1.5,"Level-matched transient change exceeds 1.5 dB");
        require(b.peak<=std::pow(10.,-.05)+2.e-7,"Preset exceeds -1 dBFS sample peak ceiling");
        // Loading a saved pre-change user sound must not inject the new recipe.
        restored->setStateInformation(old.getData(),int(old.getSize()));
        for(auto* raw:baseline->getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw))
            require(std::abs(q->getValue()-restored->parameters().getParameter(q->paramID)->getValue())<1.e-6,
                    "Saved pre-change sound was rewritten by factory finishing");
        // Representative level-matched synthetic listening pairs, not real DI.
        if(index==0 || index==9 || index==10 || index==14 || index==33 || index==36 || index==42 || index==47) {
            const auto prefix=juce::String(index).paddedLeft('0',2);
            const double match=a.rms/b.rms,pad=std::min(1.,std::pow(10.,-.05)/std::max(a.peak,b.peak*match));
            saveWav(directory.getChildFile(prefix+"-before.wav"),before,float(pad));
            saveWav(directory.getChildFile(prefix+"-after-matched.wav"),after,float(match*pad));
        }
        std::cout<<"COMPARE "<<index<<" rms_delta="<<rmsDelta<<" crest_delta="<<crestDelta<<" matched_attack_delta="<<attackDelta<<" restore_error="<<residual<<'\n';
    }
    std::cout<<"PASS 48 before/after renders, level-matched transients, binary audio recall, saved-old-state preservation and no PRE/POST/LOW compressor duplication\n";
    automation(directory);
}
}

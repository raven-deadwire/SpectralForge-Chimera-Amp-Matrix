#pragma once
// Phase 13: the Dual blend and crossover. Included by AutoSetTests.cpp after its helpers.

std::vector<float> linkwitzRiley(const std::vector<float>& x,double rate,float hz,bool high) {
    juce::dsp::LinkwitzRileyFilter<float> filter;filter.prepare({rate,512,1});filter.setCutoffFrequency(hz);
    filter.setType(high?juce::dsp::LinkwitzRileyFilterType::highpass:juce::dsp::LinkwitzRileyFilterType::lowpass);
    std::vector<float> y(x.size());for(size_t n=0;n<x.size();++n)y[n]=filter.processSample(0,x[n]);
    return y;
}
void lowShelf(std::vector<float>& x,double frequency,double gain,double rate) {
    const auto c=spectralforge::eqCoefficients(1,frequency,gain,.707,rate);double z1=0,z2=0;
    for(auto& v:x){const double y=c.b0*v+z1;z1=c.b1*v-c.a1*y+z2;z2=c.b2*v-c.a2*y;v=float(y);}
}
std::vector<float> mixOf(const std::vector<float>& a,const std::vector<float>& b,float blend) {
    std::vector<float> y(a.size());for(size_t n=0;n<y.size();++n)y[n]=(1-blend)*a[n]+blend*b[n];return y;
}
void dualRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{130};
        // Blend: rig 1 dark, rig 2 on the target. Mostly rig 1 now, so the blend rises.
        auto dark=targetNoise(false,rate,6,rng);shelve(dark,3000,-10,rate);
        const auto even=targetNoise(false,rate,6,rng);
        const auto up=analyseDual(dark,even,rate,false,.2f,false);
        check(up.playingDetected && up.rig1Heard && up.rig2Heard && !up.crossover,"Blend analysis did not hear both rigs");
        check(up.changed() && up.proposed>up.before && up.toneErrorAfterDb<up.toneErrorBeforeDb-dualMinGainDb,"A dark rig 1 did not raise the blend toward rig 2");
        check(up.proposed<=up.before+dualBlendMaxMove+1e-6f && std::abs(up.proposed*100-std::round(up.proposed*100))<1e-3f,"Blend moved past the limit or off the 1% grid");
        // The prediction is the mix itself.
        check(near(up.toneErrorAfterDb,ampToneErrorDb(measureSpectrum(mixOf(dark,even,up.proposed),rate,false),Style::rock),1e-4f),"Blend prediction differs from the mix");
        // And the other way: rig 2 bright, mostly rig 2 now, so the blend falls.
        auto bright=targetNoise(false,rate,6,rng);shelve(bright,3000,10,rate);
        const auto down=analyseDual(even,bright,rate,false,.8f,false);
        check(down.changed() && down.proposed<down.before,"A bright rig 2 did not lower the blend");
        // On target already: kept.
        const auto other=targetNoise(false,rate,6,rng);
        check(!analyseDual(even,other,rate,false,.5f,false).changed(),"Two on-target rigs changed the blend");
        // A silent rig, and silence.
        const std::vector<float> silence(size_t(rate*6));
        const auto lonely=analyseDual(even,silence,rate,false,.5f,false);
        check(lonely.playingDetected && lonely.rig1Heard && !lonely.rig2Heard && !lonely.changed(),"A silent rig 2 was blended");
        check(!analyseDual(silence,silence,rate,false,.5f).playingDetected,"Silence reported as playing");
        // Polarity: rig 2 an inverted, quieter copy of rig 1 cancels at the blend.
        auto inverted=even;scale(inverted,-.6f);
        const auto cancel=analyseDual(even,inverted,rate,false,.5f,false);
        check(cancel.playingDetected && cancel.cancellationDb>dualCancellationDb,"Cancellation between the rigs was not reported ("+std::to_string(cancel.cancellationDb)+" dB)");
        check(analyseDual(even,other,rate,false,.5f,false).cancellationDb<1.5f,"Independent rigs reported as cancelling");
        // Crossover: the band scaling predicts a split at another frequency
        // for rigs that add in power (independent sources).
        const auto x1=targetNoise(false,rate,6,rng),x2=targetNoise(false,rate,6,rng);
        for(const float to:{250.f,700.f}) {
            const auto low=measureSpectrum(linkwitzRiley(x1,rate,400,false),rate,false),high=measureSpectrum(linkwitzRiley(x2,rate,400,true),rate,false);
            const auto predicted=dualPredictCrossover(low.bandsDb,high.bandsDb,400,to);
            std::vector<float> actual(x1.size());
            const auto l=linkwitzRiley(x1,rate,to,false),h=linkwitzRiley(x2,rate,to,true);
            for(size_t n=0;n<actual.size();++n)actual[n]=l[n]+h[n];
            const auto heard=measureSpectrum(actual,rate,false);
            float worst=0;
            for(int b=0;b<toneBandCount;++b)if(cabFitBand(b))worst=std::max(worst,std::abs(predicted[size_t(b)]-heard.bandsDb[size_t(b)]));
            check(worst<1.5f,"Crossover prediction off by "+std::to_string(worst)+" dB for a split at "+std::to_string(to)+" Hz");
        }
        // A low rig 8 dB louder than the high rig: the split comes down, within an octave, on the 5 Hz grid.
        auto boomy=linkwitzRiley(x1,rate,800,false);scale(boomy,2.5f); // the low rig 8 dB too loud
        const auto split=analyseDual(boomy,linkwitzRiley(x2,rate,800,true),rate,true,800,false);
        check(split.crossover && split.changed() && split.proposed<800 && split.proposed>=400-1e-3f && std::fmod(split.proposed,5.f)==0
              && split.toneErrorAfterDb<split.toneErrorBeforeDb-dualMinGainDb,"A loud low rig did not lower the crossover ("+std::to_string(split.proposed)+" Hz)");
        if(rate==48000.)std::cout<<"MEASURE dual rules 48 kHz: blend "<<up.before<<" > "<<up.proposed<<" (error "<<up.toneErrorBeforeDb<<" > "<<up.toneErrorAfterDb<<" dB, level "<<up.levelChangeDb
                                <<" dB); bright rig 2 "<<down.before<<" > "<<down.proposed<<"; cancellation "<<cancel.cancellationDb<<" dB; crossover 800 > "<<split.proposed
                                <<" Hz (error "<<split.toneErrorBeforeDb<<" > "<<split.toneErrorAfterDb<<" dB)\n";
    }
    std::cout<<"PASS auto set dual rules at 44.1/48/96 kHz: blend toward the target both ways, exact mix, on-target kept, silent rig, silence, cancellation, crossover prediction, loud low rig lowers the split\n";
}
// The engine captures each Dual rig after its level and before the blend:
// the mix it plays is exactly (1 - blend) * rig 1 + blend * rig 2.
void dualLaneCapture() {
    spectralforge::Engine engine;engine.prepare({48000,256,1});engine.setLaneCapture(true);
    std::array<spectralforge::LaneState,3> lanes{};lanes[0].amp=0;lanes[1].amp=1;lanes[1].levelDb=-4;lanes[1].polarity=true;
    std::mt19937 rng{131};const auto x=targetNoise(false,48000,1,rng);
    juce::AudioBuffer<float> buffer(1,256);float worst=0,energy=0;
    for(int block=0;block<40;++block) {
        buffer.copyFrom(0,0,x.data()+size_t(block)*256,256);
        engine.process(buffer,spectralforge::RoutingMode::dual,350,1200,lanes,nullptr,false,.3f);
        if(block<20)continue; // past the blend and level smoothing
        for(int n=0;n<256;++n) {
            const float expected=.7f*engine.laneOutput(0).getSample(0,n)+.3f*engine.laneOutput(1).getSample(0,n);
            worst=std::max(worst,std::abs(buffer.getSample(0,n)-expected));energy=std::max(energy,std::abs(buffer.getSample(0,n)));
        }
    }
    check(energy>1e-3f && worst<1e-5f*std::max(1.f,energy),"Captured Dual rigs do not add up to the blended output");
    // The processor feeds the lane taps only in Dual routing, while they are listened to.
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    const auto capture=[&](int mode) {
        setRaw("mode",float(mode));p->autoSetCapture().listen({Tap::lane1,Tap::lane2});
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        for(int block=0;block<20;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block)*256,256);p->processBlock(io,midi);}
        std::vector<float> first,second;p->autoSetCapture().drainSamples(first);p->autoSetCapture().drainAmpSamples(second);
        p->autoSetCapture().listen(std::nullopt);
        return std::pair{first.size(),second.size()};
    };
    const auto dual=capture(1),classic=capture(0);
    check(dual.first>=8*256 && dual.first==dual.second && dual.first%256==0 && classic.first==0 && classic.second==0,"Lane taps not fed in Dual only ("+std::to_string(dual.first)+", "+std::to_string(classic.first)+")");
    p->releaseResources();
    std::cout<<"PASS auto set dual capture: rigs after level and polarity, before the blend, sum to the engine output; taps fed in Dual routing only\n";
}
struct DualRun {std::vector<float> before,values,undone;Controller::Status status;DualResult analysis;};
// Feeds the two rigs to the lane taps (and the playing to the window taps for
// a compressor pass) of a real processor in Dual routing.
// during: called once, after the first tick (an edit while the pass listens).
DualRun runDual(const Controller::Options& options,const std::vector<float>& rig1,const std::vector<float>& rig2,const std::function<void(ChimeraProcessor&)>& prepare,
                const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    prepare(*p);
    Controller c(*p);c.setOptions(options);
    DualRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=rig1.size();offset+=256) {
        const float* a[]{rig1.data()+offset};const float* b[]{rig2.data()+offset};
        p->autoSetCapture().push(Tap::lane1,a,1,256);p->autoSetCapture().push(Tap::lane2,b,1,256);
        for(auto tap:{Tap::input,Tap::compressor})p->autoSetCapture().push(tap,a,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Dual run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastDualAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerDual() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{132};
    auto dark=targetNoise(false,48000,8,rng);shelve(dark,3000,-10,48000);
    const auto even=targetNoise(false,48000,8,rng);
    const auto blendRig=[](float blend){return [blend](ChimeraProcessor& p) {
        for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"mode",1},{"dualtype",0},{"dualblend",blend}})
            p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    const Controller::Options dualOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,true,false,false,false};
    const auto full=runDual(dualOnly,dark,even,blendRig(.2f));
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::dual);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::dual} && step && step->changed && step->result.contains("BLEND"),"Dual pass did not run alone or report");
    check(a.playingDetected && a.changed() && a.proposed>a.before && near(value(full.values,"dualblend"),a.proposed,1e-4f),"Applied blend does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the blend");
    auto halfOptions=dualOnly;halfOptions.strength=.5f;
    const auto half=runDual(halfOptions,dark,even,blendRig(.2f));
    check(near(value(half.values,"dualblend"),.2f+.5f*(half.analysis.proposed-.2f),1e-4f),"Strength did not scale the blend change");
    auto lockedOptions=dualOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockDual;
    const auto locked=runDual(lockedOptions,dark,even,blendRig(.2f));
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::dual) && stepOf(locked.status,P::dual)->result=="kept (locked)"
          && value(locked.values,"dualblend")==value(locked.before,"dualblend"),"Dual lock did not keep the blend");
    // Classic routing: no pass, the row says why.
    auto classicOptions=dualOnly;classicOptions.locks=Controller::lockInput|Controller::lockGate;
    const auto classic=runDual(classicOptions,dark,even,[](ChimeraProcessor& p){p.parameters().getParameter("mode")->setValueNotifyingHost(0);});
    check(classic.status.passes==std::vector<P>{P::compressor} && stepOf(classic.status,P::dual)->result.contains("Classic or Matrix routing")
          && value(classic.values,"dualblend")==value(classic.before,"dualblend"),"Classic routing ran or changed the Dual blend");
    // The Dual type or the routing changing while the pass listens keeps everything.
    for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"dualtype",1},{"mode",2}}) {
        const auto edited=runDual(dualOnly,dark,even,blendRig(.2f),[id,v](ChimeraProcessor& p){p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));});
        check(stepOf(edited.status,P::dual)->result.contains("changed while listening") && !stepOf(edited.status,P::dual)->changed
              && value(edited.values,"dualblend")==value(edited.before,"dualblend") && value(edited.values,"dualcross")==value(edited.before,"dualcross"),
              std::string("An edit of ")+id+" while listening did not void the Dual pass");
    }
    // A silent rig 2 keeps the blend.
    const auto silent=runDual(dualOnly,even,std::vector<float>(even.size()),blendRig(.5f));
    check(stepOf(silent.status,P::dual)->result.contains("rig 2 is silent") && silent.values==silent.before,"A silent rig 2 changed the blend");
    // Crossover: a low rig 8 dB louder than the high rig brings the split down.
    auto x1=targetNoise(false,48000,8,rng);const auto x2=targetNoise(false,48000,8,rng);
    auto boomy=linkwitzRiley(x1,48000,800,false);scale(boomy,2.5f);
    const auto cross=runDual(dualOnly,boomy,linkwitzRiley(x2,48000,800,true),[](ChimeraProcessor& p) {
        for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"mode",1},{"dualtype",1},{"dualcross",800}})
            p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));});
    const auto* crossStep=stepOf(cross.status,P::dual);
    check(crossStep && crossStep->changed && crossStep->result.contains("CROSSOVER") && cross.analysis.crossover && cross.analysis.proposed<800
          && near(value(cross.values,"dualcross"),cross.analysis.proposed,.5f) && value(cross.values,"dualblend")==value(cross.before,"dualblend"),"Crossover was not lowered as analysed");
    std::cout<<"MEASURE auto set dual controller: "<<step->result<<"; crossover: "<<crossStep->result<<'\n';
    std::cout<<"PASS auto set dual controller: blend pass alone, strength, lock, Classic routing kept without a pass, Dual type or routing edited while listening, silent rig, loud low rig lowers the crossover, undo\n";
}

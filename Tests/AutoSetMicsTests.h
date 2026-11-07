#pragma once
// Phase 18: the Classic cabinet's two blended mics. Included by AutoSetTests.cpp after AutoSetDualTests.h.

// Two mics on one source: mic B a little later (and duller) than mic A.
struct MicPair {std::vector<float> a,b;};
MicPair micPair(double rate,double seconds,std::mt19937& rng,float lateMs,float sign=1.f) {
    const auto x=targetNoise(false,rate,seconds,rng);
    auto b=micsShift(x,double(lateMs)*rate*.001);shelve(b,4000,-4,rate);
    for(auto& v:b)v*=sign;
    return {x,b};
}
void micsRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{180};
        // Mic B 0.6 ms late: mic A is delayed by the offset, and the comb at the blend eases.
        const auto late=micPair(rate,5,rng,.6f);
        const auto r=analyseMics(late.a,late.b,rate,{0,0},false,.5f,false);
        check(r.playingDetected && r.aHeard && r.bHeard && r.coherent && r.correlation>.8f,"Mic analysis did not hear two coherent mics");
        check(std::abs(r.lagMs-.6f)<.03f,"Mic offset off: "+std::to_string(r.lagMs)+" ms for 0.6 ms");
        check(r.aligned() && near(r.delayAfterMs[0],r.lagMs,1e-4f) && r.delayAfterMs[1]==0 && !r.invertAfter
              && r.cancellationAfterDb<=r.cancellationBeforeDb-micsAlignGainDb,"A late mic B did not delay mic A");
        // Applying the alignment to the source mics removes the offset: a second run keeps them.
        const auto aligned=micsShift(late.a,double(r.delayAfterMs[0])*rate*.001);
        const auto again=analyseMics(aligned,late.b,rate,{r.delayAfterMs[0],0},false,r.blendAfter,false);
        check(again.coherent && std::abs(again.lagMs)<.03f && !again.aligned(),"Aligned mics were moved again ("+std::to_string(again.lagMs)+" ms)");
        // Mic B already delayed 2 ms and still late: its delay comes down instead of mic A's going up.
        const auto reduce=analyseMics(late.a,late.b,rate,{0,2},false,.5f,false);
        check(reduce.aligned() && reduce.delayAfterMs[0]==0 && near(reduce.delayAfterMs[1],2-reduce.lagMs,1e-4f),"A delayed mic B was not brought forward");
        // Mic B opposed and late: its polarity flips as well.
        const auto opposed=micPair(rate,5,rng,.4f,-1.f);
        const auto flip=analyseMics(opposed.a,opposed.b,rate,{0,0},false,.5f,false);
        check(flip.correlation<-.8f && flip.aligned() && flip.invertAfter && std::abs(flip.lagMs-.4f)<.03f
              && flip.cancellationAfterDb<=flip.cancellationBeforeDb-micsAlignGainDb,"An opposed mic B was not flipped and aligned");
        // Two unrelated mics: too different to align.
        const auto other=targetNoise(false,rate,5,rng);
        const auto unrelated=analyseMics(late.a,other,rate,{0,0},false,.5f,false);
        check(!unrelated.coherent && !unrelated.aligned(),"Unrelated mics were aligned");
        // Blend: mic A dark, aligned mics; the blend moves toward mic B, within 5-95%.
        auto dark=late.a;shelve(dark,3000,-10,rate);
        auto onTarget=micsShift(late.a,0);
        const auto toward=analyseMics(dark,onTarget,rate,{0,0},false,.3f,false);
        check(!toward.aligned() && toward.blendAfter>toward.blendBefore && toward.blendAfter<=micsBlendMax && toward.toneErrorAfterDb<toward.toneErrorBeforeDb-micsMinGainDb,
              "A dark mic A did not move the blend toward mic B");
        // A silent mic B, and silence.
        const std::vector<float> silence(late.a.size());
        const auto lonely=analyseMics(late.a,silence,rate,{0,0},false,.5f,false);
        check(lonely.playingDetected && !lonely.bHeard && !lonely.changed(),"A silent mic B was changed");
        check(!analyseMics(silence,silence,rate,{0,0},false,.5f).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE mics rules 48 kHz: late 0.6 ms > lag "<<r.lagMs<<" ms, correlation "<<r.correlation<<", mic A DELAY "<<r.delayAfterMs[0]<<" ms, cancellation "
                                <<r.cancellationBeforeDb<<" > "<<r.cancellationAfterDb<<" dB, blend "<<r.blendBefore<<" > "<<r.blendAfter<<"; opposed: lag "<<flip.lagMs<<" ms, correlation "<<flip.correlation
                                <<", cancellation "<<flip.cancellationBeforeDb<<" > "<<flip.cancellationAfterDb<<" dB; unrelated correlation "<<unrelated.correlation<<"; dark mic A blend "<<toward.blendBefore<<" > "<<toward.blendAfter<<'\n';
    }
    std::cout<<"PASS auto set mics rules at 44.1/48/96 kHz: offset found, late mic aligned, settles, delayed mic brought forward, opposed mic flipped, unrelated kept, blend toward the target, silent mic, silence\n";
}
// The cabinet captures mic A after its level, polarity and delay, and mic B,
// before the blend: the cabinet output is (1 - blend) * A + blend * B.
void micsCapture() {
    spectralforge::Cab cab;cab.blend=.4f;cab.gainDb=-3;cab.delayMs=.5f;
    cab.secondMic()->gainDb=2;cab.secondMic()->invert=true;
    cab.prepare({48000,256,1});cab.setMicCapture(true);
    std::mt19937 rng{181};const auto x=targetNoise(false,48000,1,rng);
    juce::AudioBuffer<float> buffer(1,256);float worst=0,energy=0;int filled=0;
    for(int block=0;block<40;++block) {
        buffer.copyFrom(0,0,x.data()+size_t(block)*256,256);
        cab.process(buffer);
        if(block<10)continue;
        filled+=cab.micsCaptured();
        for(int n=0;n<256;++n) {
            const float expected=.6f*cab.micOutput(0).getSample(0,n)+.4f*cab.micOutput(1).getSample(0,n);
            worst=std::max(worst,std::abs(buffer.getSample(0,n)-expected));energy=std::max(energy,std::abs(buffer.getSample(0,n)));
        }
    }
    check(filled==30 && energy>1e-3f && worst<1e-5f*std::max(1.f,energy),"Captured cabinet mics do not add up to the cabinet output");
    // The processor feeds the mic taps in Classic routing with both mics heard, while they are listened to.
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    const auto capture=[&](int mode,float blend,bool fed) {
        setRaw("mode",float(mode));setRaw("cabblend1",blend);setRaw("cabBtype1",1);
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        const auto run=[&](int blocks){for(int block=0;block<blocks;++block){for(int c=0;c<2;++c)io.copyFrom(c,0,x.data()+size_t(block%20)*256,256);p->processBlock(io,midi);}};
        const auto listen20=[&] {
            p->autoSetCapture().listen({Tap::micA,Tap::micB});run(20);
            std::vector<float> first,second;p->autoSetCapture().drainSamples(first);p->autoSetCapture().drainLaneSamples(second);
            p->autoSetCapture().listen(std::nullopt);
            return std::pair{first.size(),second.size()};
        };
        // Settle first: routing switches and the blend fade. Mic B keeps rendering
        // (and its tap stays fed) while its IR load is pending, and the IR worker
        // runs in wall-clock time while these blocks run faster than real time, so
        // the expected answer is waited for, up to 5 s; the last reading is kept.
        run(40);
        auto last=listen20();
        for(int round=0;round<50 && (fed?last.first!=size_t(20*256):last.first!=0);++round){juce::Thread::sleep(100);run(20);last=listen20();}
        return last;
    };
    const auto both=capture(0,.5f,true),oneMic=capture(0,0,false),dual=capture(1,.5f,false);
    check(both.first==20*256 && both.first==both.second && oneMic.first==0 && dual.first==0,"Mic taps not fed for a Classic two-mic cabinet only ("+std::to_string(both.first)+", "+std::to_string(oneMic.first)+", "+std::to_string(dual.first)+")");
    p->releaseResources();
    std::cout<<"PASS auto set mics capture: mic A after level, polarity and delay plus mic B sum to the cabinet output; taps fed in Classic with both mics heard only\n";
}
struct MicsRun {std::vector<float> before,values,undone;Controller::Status status;MicsResult analysis;};
MicsRun runMics(const Controller::Options& options,const MicPair& mics,const std::function<void(ChimeraProcessor&)>& prepare,const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    prepare(*p);
    Controller c(*p);c.setOptions(options);
    MicsRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=mics.a.size();offset+=256) {
        const float* a[]{mics.a.data()+offset};const float* b[]{mics.b.data()+offset};
        p->autoSetCapture().push(Tap::micA,a,1,256);p->autoSetCapture().push(Tap::micB,b,1,256);
        for(auto tap:{Tap::input,Tap::compressor})p->autoSetCapture().push(tap,a,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Mics run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastMicsAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerMics() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{182};
    const auto late=micPair(48000,8,rng,.6f,-1.f); // mic B late and opposed
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list)p.parameters().getParameter(id)->setValueNotifyingHost(p.parameters().getParameter(id)->convertTo0to1(v));};};
    const auto twoMics=setter({{"mode",0},{"cabblend1",.5f},{"cabBtype1",1}});
    const Controller::Options micsOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,false,false,false,true,false,false,false};
    const auto full=runMics(micsOnly,late,twoMics);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::mics);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::mics} && step && step->changed && step->result.contains("mic A DELAY") && step->result.contains("POLARITY inverted"),"Mics pass did not run alone or report");
    check(a.aligned() && near(value(full.values,"cabAdelay1"),a.delayAfterMs[0],.01f) && value(full.values,"cabBinvert1")>.5f && value(full.values,"cabBdelay1")==value(full.before,"cabBdelay1"),"Applied alignment does not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the mics");
    // Strength scales the blend, never the alignment (a part-way delay still combs).
    auto dark=late.a;shelve(dark,3000,-10,48000);
    const MicPair blendPair{dark,micsShift(late.a,0)};
    auto halfOptions=micsOnly;halfOptions.strength=.5f;
    const auto half=runMics(halfOptions,blendPair,setter({{"mode",0},{"cabblend1",.3f},{"cabBtype1",1}}));
    check(half.analysis.blendAfter>.3f && near(value(half.values,"cabblend1"),.3f+.5f*(half.analysis.blendAfter-.3f),1e-3f),"Strength did not scale the blend change");
    const auto halfAlign=runMics(halfOptions,late,twoMics);
    check(near(value(halfAlign.values,"cabAdelay1"),halfAlign.analysis.delayAfterMs[0],.01f),"Strength scaled the alignment");
    auto lockedOptions=micsOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockMics;
    const auto locked=runMics(lockedOptions,late,twoMics);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::mics)->result=="kept (locked)" && value(locked.values,"cabAdelay1")==value(locked.before,"cabAdelay1"),"Mics lock did not keep them");
    // One mic heard, and Dual routing: no pass, with the reason.
    auto openOptions=micsOnly;openOptions.locks=Controller::lockInput|Controller::lockGate;
    const auto one=runMics(openOptions,late,setter({{"mode",0},{"cabblend1",0},{"cabBtype1",1}}));
    check(one.status.passes==std::vector<P>{P::compressor} && stepOf(one.status,P::mics)->result.contains("one mic heard (mic A)"),"A single mic ran the mics pass");
    const auto dual=runMics(openOptions,late,setter({{"mode",1},{"cabblend1",.5f},{"cabBtype1",1}}));
    check(stepOf(dual.status,P::mics)->result.contains("Dual and Matrix routing") && value(dual.values,"cabAdelay1")==value(dual.before,"cabAdelay1"),"Dual routing ran the mics pass");
    // The blend going to one mic while listening voids the pass; a delay edited while listening is kept.
    const auto edited=runMics(micsOnly,late,twoMics,setter({{"cabblend1",0}}));
    check(stepOf(edited.status,P::mics)->result.contains("changed while listening") && value(edited.values,"cabAdelay1")==value(edited.before,"cabAdelay1"),"A blend edit while listening did not void the mics pass");
    const auto manual=runMics(micsOnly,late,twoMics,setter({{"cabAdelay1",3}}));
    check(near(value(manual.values,"cabAdelay1"),3,.01f),"A mic delay edited while listening was overwritten");
    std::cout<<"MEASURE auto set mics controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set mics controller: mics pass alone, delay and polarity applied whole, strength scales the blend only, lock, one mic and Dual kept without a pass, blend edit voids, manual delay kept, undo\n";
}

#pragma once
// Phase 15: the cabinet LOW and HIGH cuts. Included by AutoSetTests.cpp after its helpers.

// The cab's cuts, as Cab::setCuts makes them (second-order high and low pass).
std::vector<float> withCuts(std::vector<float> x,double rate,float low,float high) {
    for(const bool lowCut:{true,false}) {
        const auto c=lowCut?juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(rate,low):juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(rate,high);
        // ArrayCoefficients: b0 b1 b2 a0 a1 a2, normalised to a0.
        const double b0=c[0]/c[3],b1=c[1]/c[3],b2=c[2]/c[3],a1=c[4]/c[3],a2=c[5]/c[3];double z1=0,z2=0;
        for(auto& v:x){const double y=b0*v+z1;z1=b1*v-a1*y+z2;z2=b2*v-a2*y;v=float(y);}
    }
    return x;
}
void cutsRules() {
    const std::array<float,toneBandCount> none{};
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{150};
        const auto source=targetNoise(false,rate,6,rng);
        // The band model predicts the rendered cuts.
        const auto heard=measureSpectrum(withCuts(source,rate,70,9000),rate,false);
        for(const auto& [low,high]:std::initializer_list<std::pair<float,float>>{{120,6000},{40,14000}}) {
            const auto actual=measureSpectrum(withCuts(source,rate,low,high),rate,false);
            float worst=0;
            for(int b=0;b<toneBandCount;++b)if(toneBandCentres[size_t(b)]<rate*.4)
                worst=std::max(worst,std::abs(heard.bandsDb[size_t(b)]+cutBandChangeDb(b,70,low,true,rate)+cutBandChangeDb(b,9000,high,false,rate)-actual.bandsDb[size_t(b)]));
            check(worst<1.f,"Cut band model off by "+std::to_string(worst)+" dB");
        }
        // Boom below the guitar with a low LOW cut: it rises, never past the limit.
        auto boomy=source;lowShelf(boomy,100,12,rate);
        const auto boom=analyseCuts(measureSpectrum(withCuts(boomy,rate,30,9000),rate,false),rate,30,9000,none,Style::rock);
        check(boom.playingDetected && boom.lowChanged() && boom.lowAfter>30 && boom.lowAfter<=cutLowLimit(false,Style::rock) && boom.lowErrorAfterDb<boom.lowErrorBeforeDb-cutMinGainDb,
              "Boom did not raise the LOW cut ("+std::to_string(boom.lowAfter)+" Hz)");
        // Each run moves at most an octave; re-run at its own result, it settles within a few runs.
        float settled=boom.lowAfter;int runs=1;
        for(;runs<6;++runs) {
            const auto again=analyseCuts(measureSpectrum(withCuts(boomy,rate,settled,9000),rate,false),rate,settled,9000,none,Style::rock);
            if(!again.lowChanged())break;
            check(again.lowAfter>settled,"A re-run reversed the LOW cut");settled=again.lowAfter;
        }
        check(runs<6 && settled<=cutLowLimit(false,Style::rock),"The LOW cut did not settle");
        // A thin low end (cut at 160 Hz): it falls.
        const auto thin=analyseCuts(measureSpectrum(withCuts(source,rate,160,9000),rate,false),rate,160,9000,none,Style::rock);
        check(thin.lowChanged() && thin.lowAfter<160 && thin.lowAfter>=80-1e-3f,"A thin low end did not lower the LOW cut ("+std::to_string(thin.lowAfter)+" Hz, error "+std::to_string(thin.lowErrorBeforeDb)+" > "+std::to_string(thin.lowErrorAfterDb)+")");
        // Fizz: the HIGH cut falls; a dull top: it rises. Within an octave, on a 100 Hz grid.
        auto fizzy=source;shelve(fizzy,5000,12,rate);
        const auto fizz=analyseCuts(measureSpectrum(withCuts(fizzy,rate,70,14000),rate,false),rate,70,14000,none,Style::rock);
        check(fizz.highChanged() && fizz.highAfter<14000 && fizz.highAfter>=7000-1e-3f && std::fmod(fizz.highAfter,100.f)==0,"Fizz did not lower the HIGH cut");
        const auto dull=analyseCuts(measureSpectrum(withCuts(source,rate,70,3000),rate,false),rate,70,3000,none,Style::rock);
        check(dull.highChanged() && dull.highAfter>3000 && dull.highAfter<=6000+1e-3f,"A dull top did not raise the HIGH cut ("+std::to_string(dull.highAfter)+" Hz, error "+std::to_string(dull.highErrorBeforeDb)+" > "+std::to_string(dull.highErrorAfterDb)+")");
        // Metal allows a tighter LOW cut than rock; bass keeps the low end open.
        const auto metal=analyseCuts(measureSpectrum(withCuts(boomy,rate,60,9000),rate,false),rate,60,9000,none,Style::metal);
        check(metal.lowAfter<=cutLowLimit(false,Style::metal)+1e-3f,"Metal LOW cut past its limit");
        auto bassBoom=targetNoise(true,rate,6,rng);lowShelf(bassBoom,60,12,rate);
        const auto bass=analyseCuts(measureSpectrum(withCuts(bassBoom,rate,20,9000),rate,true),rate,20,9000,none,Style::rock);
        check(bass.bass && bass.lowAfter<=cutLowLimit(true,Style::rock)+1e-3f,"Bass LOW cut past 40 Hz");
        // The mic move phase 9 applies is counted: a boost the move adds below 160 Hz raises the cut further.
        std::array<float,toneBandCount> lift{};for(int b=0;b<toneBandCount;++b)lift[size_t(b)]=toneBandCentres[size_t(b)]<=125?6.f:0.f;
        const auto plain=analyseCuts(measureSpectrum(withCuts(source,rate,30,9000),rate,false),rate,30,9000,none,Style::rock);
        const auto lifted=analyseCuts(measureSpectrum(withCuts(source,rate,30,9000),rate,false),rate,30,9000,lift,Style::rock);
        check(lifted.lowAfter>=plain.lowAfter && lifted.lowErrorBeforeDb>plain.lowErrorBeforeDb,"The mic move was not counted");
        check(!analyseCuts(measureSpectrum(std::vector<float>(size_t(rate*6)),rate),rate,70,9000,none).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE cuts rules 48 kHz: boom LOW 30 > "<<boom.lowAfter<<" Hz (low end "<<boom.lowErrorBeforeDb<<" > "<<boom.lowErrorAfterDb<<" dB, settled at "<<settled<<" Hz after "<<runs<<" runs); thin 160 > "<<thin.lowAfter
                                <<" Hz; fizz HIGH 14 kHz > "<<fizz.highAfter<<" Hz (top "<<fizz.highErrorBeforeDb<<" > "<<fizz.highErrorAfterDb<<" dB); dull 3 kHz > "<<dull.highAfter<<" Hz; bass LOW "<<bass.lowAfter<<" Hz\n";
    }
    std::cout<<"PASS auto set cuts rules at 44.1/48/96 kHz: band model matches rendered cuts, boom raises LOW within the limit and settles, thin lowers it, fizz lowers HIGH, dull raises it, metal and bass limits, mic move counted, silence\n";
}
struct CutsRun {std::vector<float> before,values,undone;Controller::Status status;CutResult analysis;};
CutsRun runCuts(const Controller::Options& options,const std::vector<float>& post,float low,float high,const std::function<void(ChimeraProcessor&)>& prepare={},
                const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    setRaw("mode",0);setRaw("cab1",1);setRaw("cablow1",low);setRaw("cabhigh1",high);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    CutsRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=post.size();offset+=256) {
        const float* out[]{post.data()+offset};
        for(auto tap:{Tap::cab,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,out,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Cuts run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastCutsAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerCuts() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{151};
    auto boomy=targetNoise(false,48000,8,rng);lowShelf(boomy,100,12,48000);shelve(boomy,5000,12,48000);
    const auto post=withCuts(boomy,48000,30,14000);
    const Controller::Options cutsOnly{1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,false,false,false,false,false,false,true};
    const auto full=runCuts(cutsOnly,post,30,14000);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::cuts);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::cab} && !stepOf(full.status,P::cab) && step && step->changed && step->result.contains("LOW") && step->result.contains("HIGH"),
          "Cuts step did not run alone in the cabinet pass or report");
    check(a.lowAfter>30 && a.highAfter<14000 && near(value(full.values,"cablow1"),a.lowAfter,.01f) && near(value(full.values,"cabhigh1"),a.highAfter,1.f),"Applied cuts do not match the analysis");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the cuts");
    auto halfOptions=cutsOnly;halfOptions.strength=.5f;
    const auto half=runCuts(halfOptions,post,30,14000);
    check(near(value(half.values,"cablow1"),30+.5f*(half.analysis.lowAfter-30),.01f) && near(value(half.values,"cabhigh1"),14000+.5f*(half.analysis.highAfter-14000),1.f),"Strength did not scale the cuts");
    auto lockedOptions=cutsOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockCuts;
    const auto locked=runCuts(lockedOptions,post,30,14000);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::cuts)->result=="kept (locked)" && value(locked.values,"cablow1")==value(locked.before,"cablow1"),"Cuts lock did not keep them");
    // The mic lock alone keeps the pass for the cuts.
    auto both=cutsOnly;both.cab=true;both.locks|=Controller::lockCab;
    const auto micLocked=runCuts(both,post,30,14000);
    check(micLocked.status.passes==std::vector<P>{P::cab} && stepOf(micLocked.status,P::cab)->result=="kept (locked)" && stepOf(micLocked.status,P::cuts)->changed,"The mic lock stopped the cuts step");
    // A cut edited while listening, Dual routing and a switched-off cabinet keep the cuts.
    const auto edited=runCuts(cutsOnly,post,30,14000,{},[](ChimeraProcessor& p){auto* q=p.parameters().getParameter("cablow1");q->setValueNotifyingHost(q->convertTo0to1(55));});
    check(stepOf(edited.status,P::cuts)->result.contains("edited while listening") && near(value(edited.values,"cablow1"),55,.01f) && value(edited.values,"cabhigh1")==value(edited.before,"cabhigh1"),"A cut edited while listening was overwritten");
    const auto dual=runCuts(cutsOnly,post,30,14000,[](ChimeraProcessor& p){p.parameters().getParameter("mode")->setValueNotifyingHost(p.parameters().getParameter("mode")->convertTo0to1(1));});
    check(stepOf(dual.status,P::cuts)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing cuts were changed");
    const auto off=runCuts(cutsOnly,post,30,14000,[](ChimeraProcessor& p){p.parameters().getParameter("cab1")->setValueNotifyingHost(0);});
    check(stepOf(off.status,P::cuts)->result.contains("switched off") && off.values==off.before,"Cuts changed with the cabinet off");
    std::cout<<"MEASURE auto set cuts controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set cuts controller: cuts step alone in the cabinet pass, strength, own lock, mic lock, edit while listening, Dual and cabinet off kept, undo\n";
}

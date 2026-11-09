#pragma once
// Phase 23: the heard mic's factory IR. Included by AutoSetTests.cpp after its helpers.

// The factory IR rendered as the cabinet loads it: juce::dsp::Convolution, Normalise::yes, resampled to the rate.
std::vector<float> renderFactoryIR(const std::vector<float>& x,double rate,int index) {
    const auto ir=spectralforge::IRLibrary::factoryIR(index);
    check(ir!=nullptr,"Factory IR missing");
    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> copy;copy.makeCopyOf(ir->samples);
    convolution.loadImpulseResponse(std::move(copy),ir->rate,juce::dsp::Convolution::Stereo::yes,juce::dsp::Convolution::Trim::no,juce::dsp::Convolution::Normalise::yes);
    convolution.prepare({rate,512,1});
    std::vector<float> out(x.size());juce::AudioBuffer<float> block(1,512);
    for(size_t offset=0;offset<x.size();offset+=512) {
        const int n=int(std::min<size_t>(512,x.size()-offset));
        block.clear();block.copyFrom(0,0,x.data()+offset,n);
        juce::dsp::AudioBlock<float> audio(block);convolution.process(juce::dsp::ProcessContextReplacing<float>(audio));
        std::copy(block.getReadPointer(0),block.getReadPointer(0)+n,out.begin()+std::ptrdiff_t(offset));
    }
    return out;
}
// Target-shaped noise divided by one factory IR's third-octave response, so
// that IR brings it back to the style's target: the amp sound that IR suits.
// Smooth like an amp, not shaped to the IR's fine structure.
std::vector<float> irSuitedNoise(double rate,double seconds,std::mt19937& random,int index,Style style) {
    const auto ir=spectralforge::IRLibrary::factoryIR(index);
    const auto bands=cabIRBandsDb(ir->samples,ir->rate);
    const auto magnitudeAt=[&](double f) {
        const double x=std::log2(f);int b=0;
        while(b+2<toneBandCount && std::log2(double(toneBandCentres[size_t(b+1)]))<x)++b;
        const double x0=std::log2(double(toneBandCentres[size_t(b)])),x1=std::log2(double(toneBandCentres[size_t(b+1)]));
        const double t=std::clamp((x-x0)/(x1-x0),0.,1.);
        const auto db=[&](int i){return std::max(double(bands[size_t(i)]),-60.);};
        return std::pow(10.,((1-t)*db(b)+t*db(b+1))/20.);
    };
    constexpr int order=15,size=1<<order;
    juce::dsp::FFT fft(order);std::vector<float> spectrum(size_t(2*size));
    std::uniform_real_distribution<double> phase(0,2*juce::MathConstants<double>::pi);
    for(int k=1;k<size/2;++k) {
        const double f=k*rate/size;
        const double magnitude=f<35 || f>ir->rate*.45?0.:std::sqrt(std::pow(10.,toneTargetDb(false,float(std::clamp(f,20.,20000.)),style)/10.)/f)/magnitudeAt(f);
        const double ph=phase(random);spectrum[size_t(2*k)]=float(magnitude*std::cos(ph));spectrum[size_t(2*k+1)]=float(magnitude*std::sin(ph));
    }
    fft.performRealOnlyInverseTransform(spectrum.data());
    float top=0;for(int n=0;n<size;++n)top=std::max(top,std::abs(spectrum[size_t(n)]));
    std::vector<float> out(size_t(seconds*rate));
    for(size_t n=0;n<out.size();++n)out[n]=.25f*spectrum[n%size_t(size)]/top;
    return out;
}
std::array<std::array<float,toneBandCount>,2> factoryIRBands() {
    std::array<std::array<float,toneBandCount>,2> bands{};
    for(int i=0;i<2;++i){const auto ir=spectralforge::IRLibrary::factoryIR(i);bands[size_t(i)]=cabIRBandsDb(ir->samples,ir->rate);}
    return bands;
}
void cabIRRules() {
    const auto bands=factoryIRBands();
    check(!spectralforge::IRLibrary::factoryIR(2) && !spectralforge::IRLibrary::factoryIR(-1),"factoryIR served an index past the factory IRs");
    float worstPrediction=0,worstLevel=0;int switches=0;
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{230};
        // Prediction: the heard V30 bands plus the IR difference against the rendered Jensen.
        const auto raw=targetNoise(false,rate,6,rng);
        const auto v30=renderFactoryIR(raw,rate,0),jensen=renderFactoryIR(raw,rate,1);
        const auto heardV30=measureSpectrum(v30,rate,false),heardJensen=measureSpectrum(jensen,rate,false);
        double before=0,after=0;
        for(int b=0;b<toneBandCount;++b) {
            before+=std::pow(10.,heardV30.bandsDb[size_t(b)]/10.);after+=std::pow(10.,heardJensen.bandsDb[size_t(b)]/10.);
            if(!cabFitBand(b))continue;
            worstPrediction=std::max(worstPrediction,std::abs(heardV30.bandsDb[size_t(b)]+bands[1][size_t(b)]-bands[0][size_t(b)]-heardJensen.bandsDb[size_t(b)]));
        }
        for(const Style style:{Style::rock,Style::clean,Style::metal}) {
            // Playing that suits one IR: from the other one, the step switches to it; from it, it stays.
            for(int suited=0;suited<2;++suited) {
                std::mt19937 noise{uint32_t(231+suited)};
                const auto amp=irSuitedNoise(rate,6,noise,suited,style);
                const int other=1-suited;
                const auto heardOther=renderFactoryIR(amp,rate,other),heardSuited=renderFactoryIR(amp,rate,suited);
                const auto from=analyseCabIR(heardOther,rate,other+1,bands,std::optional<bool>(false),style);
                check(from.playingDetected && !from.bass && from.changed() && from.proposed==suited+1 && from.toneErrorAfterDb<from.toneErrorBeforeDb-cabIRMinGainDb,
                      std::string("No switch to the suited IR ")+cabIRName(suited+1)+" ("+std::to_string(from.toneErrorBeforeDb)+" > "+std::to_string(from.toneErrorAfterDb)+" dB)");
                // The predicted error matches the rendered switch.
                const float actual=ampToneErrorDb(measureSpectrum(heardSuited,rate,false),style);
                check(std::abs(actual-from.toneErrorAfterDb)<.5f,"Predicted IR switch error "+std::to_string(from.toneErrorAfterDb)+" dB, rendered "+std::to_string(actual)+" dB");
                // The reported level change matches the rendered one.
                const auto power=[](const Spectrum& s){double p=0;for(int b=0;b<toneBandCount;++b)p+=std::pow(10.,s.bandsDb[size_t(b)]/10.);return 10*std::log10(p);};
                worstLevel=std::max(worstLevel,float(std::abs(power(measureSpectrum(heardSuited,rate,false))-power(measureSpectrum(heardOther,rate,false))-from.levelChangeDb)));
                const auto stay=analyseCabIR(heardSuited,rate,suited+1,bands,std::optional<bool>(false),style);
                check(stay.playingDetected && !stay.changed() && stay.toneErrorAfterDb==stay.toneErrorBeforeDb,"The suited IR was switched away");
                ++switches;
                if(rate==48000. && style==Style::rock)std::cout<<"MEASURE cab IR rules 48 kHz rock: "<<cabIRName(other+1)<<" > "<<cabIRName(suited+1)<<", balance "<<from.toneErrorBeforeDb<<" > "<<from.toneErrorAfterDb<<" dB (rendered "<<actual<<"), level "<<from.levelChangeDb<<" dB\n";
            }
        }
        // Bass, other sources and silence keep the source.
        std::mt19937 bassRng{232};
        const auto bassHeard=renderFactoryIR(targetNoise(true,rate,6,bassRng),rate,0);
        const auto bass=analyseCabIR(bassHeard,rate,1,bands,std::optional<bool>(true),Style::rock);
        check(bass.playingDetected && bass.bass && !bass.changed(),"Bass switched a guitar IR");
        for(const int source:{0,3})check(!analyseCabIR(v30,rate,source,bands).changed() && !analyseCabIR(v30,rate,source,bands).playingDetected,"A non-factory source was judged");
        check(!analyseCabIR(std::vector<float>(size_t(rate*6)),rate,1,bands).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE cab IR rules 48 kHz: rendered level V30 > Jensen "<<10*std::log10(after/before)<<" dB\n";
    }
    check(worstPrediction<1.f,"IR band prediction off by "+std::to_string(worstPrediction)+" dB");
    check(worstLevel<.3f,"IR level change off by "+std::to_string(worstLevel)+" dB");
    std::cout<<"MEASURE cab IR rules: worst band prediction "<<worstPrediction<<" dB (80 Hz-6.3 kHz), worst level "<<worstLevel<<" dB, "<<switches<<" switch cases\n";
    std::cout<<"PASS auto set cab IR rules at 44.1/48/96 kHz: IR difference predicts the rendered switch, the suited IR is chosen from the other and kept, bass, filters/user IR and silence kept\n";
}
// IR-01: the switch is judged from 80 Hz, so the 50 and 63 Hz bands (a
// seven-string's A1 in Drop A, 55 Hz) are kept apart: a switch that would cut
// them by more than 3 dB, where the playing has them, is not made.
void cabIRLowEnd() {
    const auto bands=factoryIRBands();
    std::array<CabIRLow,2> fine;for(int i=0;i<2;++i){const auto ir=spectralforge::IRLibrary::factoryIR(i);fine[size_t(i)]=cabIRLowResponse(ir->samples,ir->rate);}
    for(const double hz:{46.2,55.,61.7})std::cout<<"MEASURE cab IR low end at "<<hz<<" Hz: Jensen - V30 (as loaded) "<<10*std::log10(cabIRLowAt(fine[1],hz)/cabIRLowAt(fine[0],hz))<<" dB\n";
    const auto bandOf=[](float hz){int b=0;while(b+1<toneBandCount && toneBandCentres[size_t(b)]<hz-1)++b;return b;};
    const int b50=bandOf(50),b63=bandOf(63),b80=bandOf(80);
    std::cout<<"MEASURE cab IR low end: Jensen - V30 (as loaded) "<<bands[1][size_t(b50)]-bands[0][size_t(b50)]<<" dB at 50 Hz, "<<bands[1][size_t(b63)]-bands[0][size_t(b63)]<<" dB at 63 Hz, "
             <<bands[1][size_t(b80)]-bands[0][size_t(b80)]<<" dB at 80 Hz\n";
    float worstLow=0;
    for(double rate:{44100.,48000.,96000.})for(int suited=0;suited<2;++suited) {
        // Drop A guitar: the amp sound that suits one IR, with an A1 note (55 Hz, eight harmonics) on top.
        std::mt19937 noise{uint32_t(240+suited)};
        auto amp=irSuitedNoise(rate,6,noise,suited,Style::rock);
        std::vector<float> ones;for(int h=1;h<=8;++h)ones.push_back(1.f/float(h));
        const auto note=harmonicTone(rate,6,55,ones);
        for(size_t i=0;i<amp.size();++i)amp[i]+=.5f*note[i];
        const int other=1-suited;
        const auto heardOther=renderFactoryIR(amp,rate,other),heardSuited=renderFactoryIR(amp,rate,suited);
        const auto real=analyseCabIR(heardOther,rate,other+1,bands,std::optional<bool>(false),Style::rock,&fine);
        // The predicted 50-63 Hz change matches the rendered switch.
        const auto low=[&](const Spectrum& s){double p=0;for(const int b:{b50,b63})p+=std::pow(10.,s.bandsDb[size_t(b)]/10.);return 10*std::log10(p);};
        const auto o=measureSpectrum(heardOther,rate,false),n=measureSpectrum(heardSuited,rate,false);
        const float rendered=float(low(n)-low(o));
        // The fundamental itself, measured independently (projection on a 55 Hz sine), against the prediction.
        const float f0Rendered=projectionDb(heardSuited,rate,55)-projectionDb(heardOther,rate,55);
        worstLow=std::max(worstLow,std::abs(f0Rendered-real.lowEndChangeDb));
        check(real.lowEndChangeDb!=0.f,"The A1 note's low end was not counted");
        // Playing whose low end sits at the target (no extra note), and the other IR with its 50 and 63 Hz
        // bands 8 dB lower: the balance still favours it, but it would leave 50-63 Hz under the target: kept.
        std::mt19937 at{uint32_t(240+suited)};
        const auto onTarget=renderFactoryIR(irSuitedNoise(rate,6,at,suited,Style::rock),rate,other);
        auto cut=bands;cut[size_t(suited)][size_t(b50)]-=8.f;cut[size_t(suited)][size_t(b63)]-=8.f;
        const auto held=analyseCabIR(onTarget,rate,other+1,cut,std::optional<bool>(false),Style::rock);
        check(held.playingDetected && !held.changed() && held.lowEndHeld && held.lowEndChangeDb<-cabIRMaxLowLossDb && held.lowEndAfterDb<-cabIRMaxLowLossDb
              && held.toneErrorAfterDb<held.toneErrorBeforeDb-cabIRMinGainDb && held.otherSource==suited+1,
              "IR-01: a switch that leaves 50-63 Hz under the target was made ("+std::to_string(held.lowEndChangeDb)+" dB, "+std::to_string(held.lowEndAfterDb)+" dB against the target)");
        // The same through the fine responses (the controller's path): the IR the playing suits, 8 dB lower at 40-75 Hz.
        auto fineCut=fine;for(auto& v:fineCut[size_t(suited)])v*=.15849f;
        const auto heldFine=analyseCabIR(onTarget,rate,other+1,bands,std::optional<bool>(false),Style::rock,&fineCut);
        const float base=analyseCabIR(onTarget,rate,other+1,bands,std::optional<bool>(false),Style::rock,&fine).lowEndChangeDb;
        check(!heldFine.changed() && heldFine.lowEndHeld && std::abs(heldFine.lowEndChangeDb-(base-8.f))<.5f,
              "IR-01: the fine path did not keep a switch that cuts 40-75 Hz by 8 dB more ("+std::to_string(base)+" > "+std::to_string(heldFine.lowEndChangeDb)+" dB)");
        // The same cut with the A1 note on top: the low end stays above the target, so the balance decides.
        const auto strong=analyseCabIR(heardOther,rate,other+1,cut,std::optional<bool>(false),Style::rock);
        check(!strong.lowEndHeld || strong.lowEndAfterDb<-cabIRMaxLowLossDb,"Low end above the target was held");
        // Without the A1 note (no playing below 71 Hz within 30 dB): the low end does not decide.
        std::mt19937 again{uint32_t(240+suited)};
        auto plain=irSuitedNoise(rate,6,again,suited,Style::rock);
        juce::dsp::IIR::Filter<float> hp;hp.coefficients=juce::dsp::IIR::Coefficients<float>::makeHighPass(rate,160.);
        for(int pass=0;pass<4;++pass){hp.reset();for(auto& v:plain)v=hp.processSample(v);}
        const auto quiet=analyseCabIR(renderFactoryIR(plain,rate,other),rate,other+1,cut,std::optional<bool>(false),Style::rock);
        check(!quiet.lowEndHeld && (quiet.lowEndChangeDb==0.f || quiet.lowEndChangeDb>=-cabIRMaxLowLossDb),"The low end decided for playing without it ("+std::to_string(quiet.lowEndChangeDb)+" dB)");
        if(rate==48000.)std::cout<<"MEASURE cab IR low end 48 kHz, A1 note: "<<cabIRName(other+1)<<" > "<<cabIRName(suited+1)<<" 50-63 Hz "<<real.lowEndChangeDb<<" dB predicted, A1 "<<f0Rendered<<" dB rendered (third-octave bands "<<rendered
                                <<" dB), "<<real.lowEndAfterDb<<" dB against the target, switch "<<(real.changed()?"made":"kept")<<"; on-target playing, 50/63 Hz 8 dB lower: "<<held.lowEndChangeDb<<" dB, "<<held.lowEndAfterDb<<" dB against the target, kept\n";
    }
    check(worstLow<1.f,"The A1 prediction is off by "+std::to_string(worstLow)+" dB");
    std::cout<<"PASS auto set cab IR low end at 44.1/48/96 kHz: A1 change predicted within "<<worstLow<<" dB of the rendered note, a switch that cuts it by more than 3 dB to more than 3 dB under the target is kept, playing without low end not affected\n";
}
struct CabIRRun {std::vector<float> before,values,undone;Controller::Status status;CabIRResult analysis;};
CabIRRun runCabIR(const Controller::Options& options,const std::vector<float>& post,int sourceA,const std::function<void(ChimeraProcessor&)>& prepare={},
                  const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    setRaw("mode",0);setRaw("cab1",1);setRaw("ocab1_Aon",0);setRaw("ocab1_Bon",0);setRaw("cabblend1",0);setRaw("cabtype1",float(sourceA));setRaw("cabBtype1",0);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    CabIRRun result;result.before=values(*p);
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=post.size();offset+=256) {
        const float* out[]{post.data()+offset};
        for(auto tap:{Tap::cab,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,out,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Cab IR run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastCabIRAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
void controllerCabIR() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    std::mt19937 rng{233};
    // Playing that suits the Jensen, heard through the V30.
    const auto post=renderFactoryIR(irSuitedNoise(48000,8,rng,1,Style::rock),48000,0);
    Controller::Options irOnly;
    irOnly.instrument=1;irOnly.style=Style::rock;irOnly.strength=1.f;irOnly.locks=Controller::lockInput|Controller::lockGate|Controller::lockCompressor;
    irOnly.tone=irOnly.finish=irOnly.multiband=irOnly.gates=irOnly.dynamicEQ=irOnly.level=irOnly.amp=irOnly.cab=irOnly.boost=irOnly.space=irOnly.power=irOnly.dual=irOnly.width=irOnly.cuts=irOnly.bus=irOnly.matrix=irOnly.mics=irOnly.rigs=irOnly.channel=irOnly.polarity=irOnly.sync=irOnly.trim=false;
    irOnly.cabIR=true;
    const auto full=runCabIR(irOnly,post,1);
    const auto* step=stepOf(full.status,P::cabIR);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::cab} && !stepOf(full.status,P::cab) && step && step->changed
          && step->result.contains("mic A IR V30 / SM57 > Jensen / SM57") && step->result.contains("level"),
          "Cab IR step did not run alone in the cabinet pass or report"+(step?(": "+step->result).toStdString():std::string()));
    const auto source=[&](const std::vector<float>& v,const juce::String& id){return juce::roundToInt(value(v,id));};
    check(source(full.before,"cabtype1")==1 && source(full.values,"cabtype1")==2 && full.analysis.proposed==2,"cabtype1 not Jensen");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the IR");
    // 50% strength still switches whole.
    auto halfOptions=irOnly;halfOptions.strength=.5f;
    check(stepOf(runCabIR(halfOptions,post,1).status,P::cabIR)->changed,"Strength stopped the IR choice");
    // Already on the suited IR: kept.
    const auto suited=runCabIR(irOnly,renderFactoryIR(irSuitedNoise(48000,8,rng,1,Style::rock),48000,1),2);
    check(!stepOf(suited.status,P::cabIR)->changed && stepOf(suited.status,P::cabIR)->result.contains("already closest with Jensen") && suited.values==suited.before,"The suited IR was switched");
    // Mic B heard: cabBtype1 moves, cabtype1 stays.
    const auto micB=runCabIR(irOnly,post,0,[](ChimeraProcessor& p){
        for(const auto& [id,v]:std::initializer_list<std::pair<const char*,float>>{{"cabblend1",1},{"cabBtype1",1}}){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}});
    check(stepOf(micB.status,P::cabIR)->result.contains("mic B IR V30 / SM57 > Jensen / SM57") && value(micB.values,"cabtype1")==value(micB.before,"cabtype1") && source(micB.values,"cabBtype1")==2,"Mic B IR not chosen");
    // Lock: no pass; with the cuts step on, the pass runs and the row says locked.
    auto lockedOptions=irOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockCabIR;
    const auto locked=runCabIR(lockedOptions,post,1);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::cabIR)->result=="kept (locked)" && value(locked.values,"cabtype1")==value(locked.before,"cabtype1"),"Cab IR lock did not keep it");
    auto withCutsStep=lockedOptions;withCutsStep.cuts=true;withCutsStep.locks|=Controller::lockCompressor;
    const auto lockedWithCuts=runCabIR(withCutsStep,post,1);
    check(lockedWithCuts.status.passes==std::vector<P>{P::cab} && stepOf(lockedWithCuts.status,P::cabIR)->result=="kept (locked)","The cab IR lock row missing in the cabinet pass");
    // The cuts step after it counts the switch.
    auto cutsToo=irOnly;cutsToo.cuts=true;
    const auto both=runCabIR(cutsToo,post,1);
    const auto* cutsRow=stepOf(both.status,P::cuts);const auto* irRow=stepOf(both.status,P::cabIR);
    check(irRow && irRow->changed && cutsRow,"Cab IR and cuts rows missing");
    size_t irIndex=0,cutsIndex=0;for(size_t i=0;i<both.status.steps.size();++i){if(both.status.steps[i].phase==P::cabIR)irIndex=i;if(both.status.steps[i].phase==P::cuts)cutsIndex=i;}
    check(irIndex<cutsIndex,"The cab IR row is not before the cuts row");
    // Kept: an IR edited while listening, a modeled mic, filters only, a user IR, Dual routing, the cabinet off, two mics blended.
    const auto edited=runCabIR(irOnly,post,1,{},[](ChimeraProcessor& p){auto* q=p.parameters().getParameter("cabtype1");q->setValueNotifyingHost(q->convertTo0to1(2));});
    check(stepOf(edited.status,P::cabIR)->result.contains("changed while listening") && source(edited.values,"cabtype1")==2,"An IR edited while listening was overwritten");
    const auto keptCase=[&](const char* why,int source,const std::function<void(ChimeraProcessor&)>& prepare) {
        const auto r=runCabIR(irOnly,post,source,prepare);
        check(stepOf(r.status,P::cabIR) && stepOf(r.status,P::cabIR)->result.contains(why) && !stepOf(r.status,P::cabIR)->changed && value(r.values,"cabtype1")==value(r.before,"cabtype1"),
              std::string("Cab IR not kept for: ")+why+(stepOf(r.status,P::cabIR)?" ("+stepOf(r.status,P::cabIR)->result.toStdString()+")":std::string()));
    };
    const auto put=[](ChimeraProcessor& p,const char* id,float v){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    keptCase("modeled mic",1,[&](ChimeraProcessor& p){put(p,"ocab1_Aon",1);});
    keptCase("filters only",0,{});
    keptCase("user IR",3,{});
    keptCase("Dual and Matrix",1,[&](ChimeraProcessor& p){put(p,"mode",1);});
    keptCase("switched off",1,[&](ChimeraProcessor& p){put(p,"cab1",0);});
    keptCase("two mics are blended",1,[&](ChimeraProcessor& p){put(p,"cabBtype1",2);put(p,"cabblend1",.5f);});
    std::cout<<"MEASURE auto set cab IR controller: "<<step->result<<'\n';
    std::cout<<"PASS auto set cab IR controller: factory IR choice alone in the cabinet pass, strength, suited kept, mic B, lock, before the cuts, edit while listening, modeled mic, filters, user IR, Dual, cabinet off and two mics kept, undo\n";
}

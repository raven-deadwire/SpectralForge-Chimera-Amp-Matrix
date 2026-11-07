#pragma once
// Phase 24: the amp head's INPUT TRIM. Included by AutoSetTests.cpp after the amp and channel tests.

// syntheticAmp's drive with GAIN fixed at `gain`, the trim scaling the input.
TrimRenderer syntheticTrim(const std::vector<float>& input,double rate,float gain) {
    return [input,rate,gain](float trimDb) {
        auto x=input;const float g=juce::Decibels::decibelsToGain(trimDb);for(auto& v:x)v*=g;
        return syntheticAmp(std::move(x),rate)({gain,5,5,5});
    };
}
void trimRules() {
    for(double rate:{44100.,48000.,96000.}) {
        std::mt19937 rng{240};const auto di=playedNotes(rate,ampRenderSeconds,rng);
        const auto at=[&](float gain,float current,Style style){const auto render=syntheticTrim(di,rate,gain);return std::pair{analyseTrim(di,render(current),rate,current,render,false,style),render};};
        // GAIN low for rock: the trim rises toward the 45% target, on the 0.5 dB grid, within 12 dB.
        const auto [up,upRender]=at(2,0,Style::rock);
        check(up.playingDetected && up.rendered && !up.bass && near(up.target,.45f,1e-6f) && up.driveBefore<.45f-ampCompressionTolerance,"Trim rules: the low-GAIN fixture already reaches the drive");
        check(up.changed() && up.proposed>0 && up.proposed<=trimMaxMoveDb && std::fmod(up.proposed,trimGridDb)==0 && std::abs(up.driveAfter-up.target)<std::abs(up.driveBefore-up.target)-trimMinGain,
              "Trim did not rise toward the drive ("+std::to_string(up.proposed)+" dB, drive "+std::to_string(up.driveBefore)+" > "+std::to_string(up.driveAfter)+")");
        // The reported drive and loudness are the rendered ones.
        check(near(up.driveAfter,ampCompression(di,upRender(up.proposed),rate),1e-6f)
              && near(up.levelChangeDb,trimLoudnessDb(upRender(up.proposed),rate)-trimLoudnessDb(upRender(0),rate),1e-4f),"Trim drive or loudness is not the rendered one");
        // GAIN high for clean: the trim falls.
        const auto [down,downRender]=at(6,0,Style::clean);
        check(down.changed() && down.proposed<0 && down.proposed>=-trimMaxMoveDb && down.driveAfter<down.driveBefore,"Trim did not fall for a clean drive ("+std::to_string(down.proposed)+" dB)");
        // Within the tolerance: kept.
        const float reachedTrim=up.proposed;
        const auto [stay,stayRender]=at(2,reachedTrim,Style::rock);
        check(!up.reachable || (stay.reachable && !stay.changed() && near(stay.driveBefore,up.driveAfter,1e-6f)),"A trim within the drive tolerance was moved");
        // The control's range bounds the move: from +20 dB it stops at +24.
        const auto [top,topRender]=at(0,20,Style::metal);
        check(top.proposed<=trimLimitDb && (top.changed()?top.proposed>20:true),"Trim left its range");
        // GAIN at zero for metal: 12 dB is not enough; the move is kept with reachable=false, or kept when no closer.
        const auto [short_,shortRender]=at(0,0,Style::metal);
        check(!short_.reachable && short_.proposed<=trimMaxMoveDb,"Metal from GAIN 0 reported as reached");
        // A silent render and silence.
        const auto mute=analyseTrim(di,upRender(0),rate,0,[&](float){return std::vector<float>(di.size());},false,Style::rock);
        check(mute.playingDetected && !mute.rendered && !mute.changed(),"A silent head was trimmed");
        const std::vector<float> silence(di.size());
        check(!analyseTrim(silence,silence,rate,0,upRender,false).playingDetected,"Silence reported as playing");
        if(rate==48000.)std::cout<<"MEASURE trim rules 48 kHz: rock GAIN 2 trim 0 > "<<up.proposed<<" dB (drive "<<up.driveBefore<<" > "<<up.driveAfter<<", loudness "<<up.levelChangeDb<<" dB); clean GAIN 6 trim 0 > "<<down.proposed
                                <<" dB (drive "<<down.driveBefore<<" > "<<down.driveAfter<<"); metal GAIN 0 > "<<short_.proposed<<" dB (drive "<<short_.driveAfter<<", reached "<<short_.reachable<<")\n";
    }
    std::cout<<"PASS auto set trim rules at 44.1/48/96 kHz: rises for a low drive, falls for a clean one, 0.5 dB grid within 12 dB and +-24 dB, rendered drive and loudness, within tolerance kept, short of the target reported, silent head and silence\n";
}
struct TrimRun {std::vector<float> before,values,undone;Controller::Status status;TrimResult analysis;std::unique_ptr<ChimeraProcessor> processor;};
// The model 2 head on RHYTHM in Classic routing with GAIN at `gainFraction` of its range.
TrimRun runTrim(const Controller::Options& options,float gainFraction,const std::function<void(ChimeraProcessor&)>& prepare={},
                const std::function<void(ChimeraProcessor&)>& during={}) {
    constexpr int model=2,rhythm=0;
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto setRaw=[&](const juce::String& id,float v){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
    setRaw("mode",0);setRaw("ampon1",1);setRaw(spectralforge::ampNativeEnabledID(0),1);setRaw(spectralforge::ampNativeModelID(0),float(model));
    setRaw(spectralforge::ampNativeChannelID(0,model),float(rhythm));
    const auto gainID=spectralforge::ampNativeControlID(0,model,roleControls(model,rhythm,0)[ampGain],rhythm);
    {auto* q=p->parameters().getParameter(gainID);const auto range=q->getNormalisableRange();setRaw(gainID,range.start+gainFraction*(range.end-range.start));}
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    TrimRun result;result.before=values(*p);
    std::mt19937 rng{241};const auto di=playedNotes(48000,8,rng);
    const auto post=nativeAmpRenderer(p->autoSetAmpState(0),{-1,-1,-1,-1},di,48000)({});
    double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=di.size();offset+=256) {
        const float* in[]{di.data()+offset};const float* out[]{post.data()+offset};
        p->autoSetCapture().push(Tap::amp,in,1,256);
        for(auto tap:{Tap::cab,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,tap==Tap::cab?out:in,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==0)during(*p);
    }
    check(!c.busy(),"Trim run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastTrimAnalysis();
    c.undo();result.undone=values(*p);
    result.processor=std::move(p);
    return result;
}
void controllerTrim() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto trimID=spectralforge::ampNativeInputTrimID(0),outputID=spectralforge::ampNativeOutputLevelID(0);
    Controller::Options trimOnly;
    trimOnly.instrument=1;trimOnly.style=Style::rock;trimOnly.strength=1.f;trimOnly.locks=Controller::lockInput|Controller::lockGate|Controller::lockCompressor;
    trimOnly.tone=trimOnly.finish=trimOnly.multiband=trimOnly.gates=trimOnly.dynamicEQ=trimOnly.level=trimOnly.amp=trimOnly.cab=trimOnly.boost=trimOnly.space=trimOnly.power=trimOnly.dual=trimOnly.width=trimOnly.cuts=trimOnly.bus=trimOnly.matrix=trimOnly.mics=trimOnly.rigs=trimOnly.channel=trimOnly.polarity=trimOnly.sync=trimOnly.cabIR=false;
    trimOnly.trim=true;
    // RHYTHM with GAIN at its bottom, rock: the trim rises, OUTPUT LEVEL holds the loudness.
    auto full=runTrim(trimOnly,0);
    const auto& a=full.analysis;const auto* step=stepOf(full.status,P::trim);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::amp} && step && step->changed && step->result.contains("INPUT TRIM +0.0 dB > +") && step->result.contains("OUTPUT LEVEL") && !stepOf(full.status,P::amp),
          "Trim step did not run alone in the amp pass or report"+(step?(": "+step->result).toStdString():std::string()));
    check(a.playingDetected && a.rendered && a.proposed>0 && near(value(full.values,trimID),a.proposed,.011f) && value(full.values,outputID)<0,"Applied trim does not match the analysis");
    // Rendered on the head as now set: the drive is the analysed one and the loudness held.
    {
        std::mt19937 rng{241};const auto di=playedNotes(48000,ampRenderSeconds,rng);
        auto before=full.processor->autoSetAmpState(0);before.inputTrimDb=0;before.outputLevelDb=0;
        auto after=before;after.inputTrimDb=value(full.values,trimID);after.outputLevelDb=value(full.values,outputID);
        const auto renderBefore=nativeAmpRenderer(before,{-1,-1,-1,-1},di,48000)({}),renderAfter=nativeAmpRenderer(after,{-1,-1,-1,-1},di,48000)({});
        check(near(ampCompression(di,renderAfter,48000),a.driveAfter,.005f),"The drive at the applied trim is not the analysed one");
        const float held=trimLoudnessDb(renderAfter,48000)-trimLoudnessDb(renderBefore,48000);
        check(std::abs(held)<.1f,"OUTPUT LEVEL did not hold the loudness ("+std::to_string(held)+" dB)");
        std::cout<<"MEASURE auto set trim controller: "<<step->result<<"; loudness after - before "<<held<<" dB\n";
    }
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the trim and level");
    // 50% strength: half the trim, the level held for it.
    auto halfOptions=trimOnly;halfOptions.strength=.5f;
    const auto half=runTrim(halfOptions,0);
    check(near(value(half.values,trimID),.5f*half.analysis.proposed,.011f) && value(half.values,outputID)<0 && value(half.values,outputID)>value(full.values,outputID),"Strength did not scale the trim");
    // With the amp step: GAIN reaches the rock drive on RHYTHM, so the trim stays; amp, trim, presence rows in order.
    auto withAmp=trimOnly;withAmp.amp=true;withAmp.power=true;
    const auto both=runTrim(withAmp,0);
    const auto& steps=both.status.steps;
    const auto row=[&](P phase){return std::find_if(steps.begin(),steps.end(),[phase](const auto& s){return s.phase==phase;});};
    check(row(P::amp)!=steps.end() && row(P::trim)!=steps.end() && row(P::power)!=steps.end() && row(P::amp)<row(P::trim) && row(P::trim)<row(P::power),"Amp, trim and presence rows are out of order");
    check(!row(P::trim)->changed && row(P::trim)->result.contains("drive") && value(both.values,trimID)==value(both.before,trimID),"The trim moved although GAIN reaches the drive: "+row(P::trim)->result.toStdString());
    // The amp lock alone: the trim does the work.
    auto ampLocked=withAmp;ampLocked.locks|=Controller::lockAmp;ampLocked.power=false;
    const auto lockedAmp=runTrim(ampLocked,0);
    check(stepOf(lockedAmp.status,P::amp)->result=="kept (locked)" && stepOf(lockedAmp.status,P::trim)->changed,"With GAIN locked the trim did not move");
    // Lock: no pass.
    auto lockedOptions=trimOnly;lockedOptions.locks=Controller::lockInput|Controller::lockGate|Controller::lockTrim;
    const auto locked=runTrim(lockedOptions,0);
    check(locked.status.passes==std::vector<P>{P::compressor} && stepOf(locked.status,P::trim)->result=="kept (locked)" && value(locked.values,trimID)==value(locked.before,trimID),"Trim lock did not keep it");
    // Kept: the trim edited while listening, Dual routing.
    const auto edited=runTrim(trimOnly,0,{},[&](ChimeraProcessor& p){auto* q=p.parameters().getParameter(trimID);q->setValueNotifyingHost(q->convertTo0to1(3));});
    check(stepOf(edited.status,P::trim)->result.contains("edited while listening") && near(value(edited.values,trimID),3,.011f) && value(edited.values,outputID)==value(edited.before,outputID),"A trim edited while listening was overwritten");
    const auto dual=runTrim(trimOnly,0,[](ChimeraProcessor& p){p.parameters().getParameter("mode")->setValueNotifyingHost(p.parameters().getParameter("mode")->convertTo0to1(1));});
    check(stepOf(dual.status,P::trim)->result.contains("Dual and Matrix") && dual.values==dual.before,"Dual routing trim was changed");
    std::cout<<"PASS auto set trim controller: trim alone in the amp pass rises for rock with GAIN at its bottom, OUTPUT LEVEL holds the loudness, rendered drive, strength, kept after an amp step that reaches the drive, GAIN locked, lock, edit while listening, Dual, undo\n";
}

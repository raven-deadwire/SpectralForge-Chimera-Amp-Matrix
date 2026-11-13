// Auto Set phase 26: the Room Core room (docs/AUTO_SET_PHASE26.md).
void roomRules() {
    check(!analyseRoom(windowsOf(std::vector<float>(48000*6)),false,Style::rock).feel.measured,"Silence reported as playing");
    struct Case {const char* name;std::vector<float> x;bool bass;Style style;int size;float absorption,returnDb;bool dense,sparse;};
    const Case cases[]{
        {"sparse clean guitar",pluckedEvery(1000,6),false,Style::clean,roomSizeLarge,40.f,-8.f,false,true},
        {"steady rock guitar",pluckedEvery(300,6,48000,12),false,Style::rock,roomSizeSmall,55.f,-14.f,false,false},
        {"steady clean guitar",pluckedEvery(300,6,48000,12),false,Style::clean,roomSizeMedium,40.f,-10.f,false,false},
        {"dense metal guitar",pluckedEvery(100,6,48000,40),false,Style::metal,roomSizeSmall,85.f,-24.f,true,false},
        {"dense clean guitar",pluckedEvery(100,6,48000,40),false,Style::clean,roomSizeSmall,55.f,-14.f,true,false},
        {"steady rock bass",pluckedEvery(300,6,48000,12),true,Style::rock,roomSizeSmall,65.f,-20.f,false,false},
        {"sparse clean bass",pluckedEvery(1000,6),true,Style::clean,roomSizeMedium,50.f,-14.f,false,true},
    };
    for(const auto& c:cases) {
        const auto r=analyseRoom(windowsOf(c.x),c.bass,c.style);
        check(r.feel.measured && r.dense==c.dense && r.sparse==c.sparse && r.room.size==c.size && near(r.room.absorption,c.absorption,1e-4f) && near(r.room.returnDb,c.returnDb,1e-4f),
              std::string("Room rule for ")+c.name+": size "+std::to_string(r.room.size)+", absorption "+std::to_string(r.room.absorption)+", return "+std::to_string(r.room.returnDb)+", "+std::to_string(r.feel.onsetsPerSecond)+" notes/s");
    }
    // A new room at a lower strength is quieter, as a mix: 20 log10(strength), to 0.5 dB.
    check(near(roomNewReturn(-14.f,1.f),-14.f,1e-6f) && near(roomNewReturn(-14.f,.5f),-20.f,1e-6f) && near(roomNewReturn(-8.f,.25f),-20.f,1e-6f) && near(roomNewReturn(-10.f,.75f),-12.5f,1e-6f),"New room return does not scale with strength");
    check(roomNewReturn(-24.f,.25f)<roomReturnMinDb && roomNewReturn(-8.f,0.f)<roomReturnMinDb,"A room below the return's range is opened");
    check(near(roomReturnRounded(-13.76f),-14.f,1e-6f) && near(roomReturnRounded(-50.f),-36.f,1e-6f) && near(roomReturnRounded(9.f),6.f,1e-6f),"Return rounding or range");
    std::cout<<"PASS auto set room rules: silence; sparse/steady/dense guitar and bass by style; new-room strength scaling and the return's range\n";
}
struct RoomRun {std::vector<float> before,values,undone;Controller::Status status;RoomResult analysis;};
Controller::Options roomOnlyOptions(Style style=Style::clean) {
    Controller::Options o;
    o.instrument=1;o.tone=false;o.style=style;o.finish=false;o.multiband=false;o.gates=false;o.locks=Controller::lockCompressor;
    o.dynamicEQ=false;o.level=false;o.amp=false;o.cab=false;o.boost=false;o.space=false;o.power=false;o.dual=false;o.width=false;o.cuts=false;o.bus=false;
    o.matrix=false;o.mics=false;o.rigs=false;o.channel=false;o.polarity=false;o.sync=false;o.cabIR=false;o.trim=false;o.crossover=false;o.room=true;
    return o;
}
// Sparse notes (one a second) on the input and compressor taps of a real processor.
RoomRun runRoom(const Controller::Options& options,const std::function<void(ChimeraProcessor&)>& prepare={},const std::function<void(ChimeraProcessor&)>& during={}) {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    if(prepare)prepare(*p);
    Controller c(*p);c.setOptions(options);
    RoomRun result;result.before=values(*p);
    const auto notes=pluckedEvery(1000,8);double ms=juce::Time::getMillisecondCounterHiRes();c.start();
    for(size_t offset=0;c.busy() && offset+256<=notes.size();offset+=256) {
        const float* in[]{notes.data()+offset};
        for(auto tap:{Tap::input,Tap::compressor})p->autoSetCapture().push(tap,in,1,256);
        ms+=1000.*256/48000;c.tick(ms);
        if(during && offset==256*10)during(*p);
    }
    check(!c.busy(),"Room run did not finish");
    result.values=values(*p);result.status=c.current();result.analysis=c.lastRoomAnalysis();
    c.undo();result.undone=values(*p);p->releaseResources();
    return result;
}
// The room's wet return relative to the dry rig, at the given room settings:
// the same notes through two processors, room off and on; the difference is the room.
float roomWetToDryDb(int size,float absorption,float returnDb) {
    const auto x=pluckedEvery(500,4);
    std::array<std::vector<float>,2> out;
    for(int on=0;on<2;++on) {
        auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
        for(const auto& [id,v]:std::initializer_list<std::pair<juce::String,float>>{{"roomOn",float(on)},{"roomSize",float(size)},{"roomAbsorption",absorption},{"roomReturn",returnDb}})
            q->parameters().getParameter(id)->setValueNotifyingHost(q->parameters().getParameter(id)->convertTo0to1(v));
        juce::AudioBuffer<float> io(2,256);juce::MidiBuffer midi;
        for(size_t offset=0;offset+256<=x.size();offset+=256) {
            for(int ch=0;ch<2;++ch)io.copyFrom(ch,0,x.data()+offset,256);
            q->processBlock(io,midi);
            out[size_t(on)].insert(out[size_t(on)].end(),io.getReadPointer(0),io.getReadPointer(0)+256);
        }
        q->releaseResources();
    }
    double dry=0,wet=0;
    for(size_t n=out[0].size()/4;n<out[0].size();++n){dry+=double(out[0][n])*out[0][n];const double d=double(out[1][n])-out[0][n];wet+=d*d;}
    return float(10*std::log10((wet+1e-20)/(dry+1e-20)));
}
void controllerRoom() {
    using P=Controller::Phase;
    const auto value=[](const std::vector<float>& v,const juce::String& id){return valueOf(LockRun{v,{},{}},id);};
    const auto setter=[](std::initializer_list<std::pair<juce::String,float>> pairs){
        std::vector<std::pair<juce::String,float>> list(pairs);
        return [list](ChimeraProcessor& p){for(const auto& [id,v]:list){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}};
    };
    check(!Controller::Options{}.room && !spectralforge::CommonDynamicsPanel::AutoSetOptions{}.room,"Room ambience is on by default");
    // A room that is off is opened with the rule (sparse clean guitar: Large, 40%, -8 dB). The sends (50%) stay.
    const auto full=runRoom(roomOnlyOptions());
    const auto* step=stepOf(full.status,P::room);
    check(full.status.phase==P::done && full.status.passes==std::vector<P>{P::input} && step && step->changed && step->result.startsWith("room on: Large, absorption 40%, return -8.0 dB"),
          "Room step did not run or report: "+(step?step->result.toStdString():std::string("no step")));
    check(value(full.values,"roomOn")>.5f && juce::roundToInt(value(full.values,"roomSize"))==roomSizeLarge && near(value(full.values,"roomAbsorption"),40.f,.01f) && near(value(full.values,"roomReturn"),-8.f,.01f)
          && near(value(full.values,"roomSend1"),value(full.before,"roomSend1"),1e-4f),"Applied room does not match the rule");
    bool undone=true;for(size_t i=0;i<full.before.size();++i)undone&=near(full.undone[i],full.before[i],1e-6f);
    check(undone,"UNDO AUTO did not restore the room");
    // Strength: a new room is quieter (50%: -6 dB); 25% on sparse metal bass (-24 dB) drops below the range and keeps it off.
    auto half=roomOnlyOptions();half.strength=.5f;
    const auto halved=runRoom(half);
    check(value(halved.values,"roomOn")>.5f && near(value(halved.values,"roomReturn"),-14.f,.01f),"Strength did not lower a new room's return");
    auto quarterMetal=roomOnlyOptions(Style::metal);quarterMetal.instrument=2;quarterMetal.strength=.25f;
    const auto quiet=runRoom(quarterMetal);
    check(value(quiet.values,"roomOn")<.5f && stepOf(quiet.status,P::room) && stepOf(quiet.status,P::room)->result=="kept (strength too low to open the room)","A room below the return's range was opened");
    // Every send of the routing at 0: they are opened (Classic: send 1; Dual: sends 1 and 2), the others stay.
    const auto closedClassic=runRoom(roomOnlyOptions(),setter({{"roomSend1",0},{"roomSend2",0},{"roomSend3",0}}));
    check(near(value(closedClassic.values,"roomSend1"),roomOpenSend,.01f) && near(value(closedClassic.values,"roomSend2"),0.f,.01f) && near(value(closedClassic.values,"roomSend3"),0.f,.01f)
          && stepOf(closedClassic.status,P::room)->result.contains("sends 50%"),"Classic sends at 0 were not opened");
    const auto closedDual=runRoom(roomOnlyOptions(),setter({{"mode",1},{"roomSend1",0},{"roomSend2",0},{"roomSend3",0}}));
    check(near(value(closedDual.values,"roomSend1"),roomOpenSend,.01f) && near(value(closedDual.values,"roomSend2"),roomOpenSend,.01f) && near(value(closedDual.values,"roomSend3"),0.f,.01f),"Dual sends at 0 were not opened");
    const auto oneOpen=runRoom(roomOnlyOptions(),setter({{"mode",1},{"roomSend1",0},{"roomSend2",30},{"roomSend3",0}}));
    check(near(value(oneOpen.values,"roomSend1"),0.f,.01f) && near(value(oneOpen.values,"roomSend2"),30.f,.01f),"A send the player set was changed");
    // A room the player has on keeps its size, absorption and sends; only its return moves (50%: halfway).
    const auto own=runRoom(half,setter({{"roomOn",1},{"roomSize",0},{"roomAbsorption",90},{"roomReturn",0},{"roomSend1",80}}));
    check(juce::roundToInt(value(own.values,"roomSize"))==0 && near(value(own.values,"roomAbsorption"),90.f,.01f) && near(value(own.values,"roomSend1"),80.f,.01f) && near(value(own.values,"roomReturn"),-4.f,.01f)
          && stepOf(own.status,P::room)->result.startsWith("room return 0.0 dB > -4.0 dB"),
          "The player's room was changed beyond its return: size "+std::to_string(value(own.values,"roomSize"))+", absorption "+std::to_string(value(own.values,"roomAbsorption"))
          +", send 1 "+std::to_string(value(own.values,"roomSend1"))+", return "+std::to_string(value(own.before,"roomReturn"))+" > "+std::to_string(value(own.values,"roomReturn"))
          +"; row: "+stepOf(own.status,P::room)->result.toStdString());
    // A declared space keeps its geometry: size and absorption are not written.
    const auto declared=runRoom(roomOnlyOptions(),setter({{"roomSpace",1},{"roomSize",0},{"roomAbsorption",70}}));
    check(value(declared.values,"roomOn")>.5f && juce::roundToInt(value(declared.values,"roomSize"))==0 && near(value(declared.values,"roomAbsorption"),70.f,.01f)
          && stepOf(declared.status,P::room)->result.contains("declared space kept"),"A declared space's size or absorption was written");
    // Room switched on and off by hand during the run: kept as it is.
    const auto edited=runRoom(roomOnlyOptions(),{},setter({{"roomOn",1},{"roomOn",0}}));
    check(value(edited.values,"roomOn")<.5f && stepOf(edited.status,P::room)->result=="kept (Room switched by hand during the run)","A room switched by hand was overwritten");
    // The lock keeps the room; without the input pass there is no playing to judge.
    auto lockedOptions=roomOnlyOptions();lockedOptions.locks|=Controller::lockRoom;
    const auto locked=runRoom(lockedOptions);
    check(stepOf(locked.status,P::room) && stepOf(locked.status,P::room)->result=="kept (locked)" && value(locked.values,"roomOn")<.5f,"Room lock did not keep the room");
    auto noInput=roomOnlyOptions();noInput.locks=Controller::lockInput|Controller::lockGate;noInput.space=true;
    const auto skipped=runRoom(noInput);
    check(stepOf(skipped.status,P::room) && stepOf(skipped.status,P::room)->result.contains("input pass did not run") && value(skipped.values,"roomOn")<.5f,"Missing input pass not reported");
    // Signal order: the room step comes before the delay + reverb step.
    auto withSpace=roomOnlyOptions();withSpace.space=true;
    const auto both=runRoom(withSpace);
    int roomAt=-1,spaceAt=-1;for(size_t i=0;i<both.status.steps.size();++i){if(both.status.steps[i].phase==P::room)roomAt=int(i);if(both.status.steps[i].phase==P::space)spaceAt=int(i);}
    check(roomAt>=0 && spaceAt>roomAt,"The room step is not reported before the delay + reverb step");
    // The rooms the rules choose, heard through the real processor (measured; the wet relative to the dry rig).
    const float clean=roomWetToDryDb(roomSizeLarge,40.f,-8.f),rock=roomWetToDryDb(roomSizeSmall,55.f,-14.f),metal=roomWetToDryDb(roomSizeSmall,85.f,-24.f);
    check(std::isfinite(clean) && clean>rock && rock>metal && metal>-60.f,"The rule's rooms are not ordered clean > rock > metal in level");
    std::cout<<"MEASURE auto set room controller: "<<step->result<<'\n';
    std::cout<<"MEASURE room wet/dry through the processor: sparse clean (Large, 40%, -8 dB) "<<clean<<" dB; steady rock (Small, 55%, -14 dB) "<<rock<<" dB; dense metal (Small, 85%, -24 dB) "<<metal<<" dB\n";
    std::cout<<"PASS auto set room controller: off by default; new room by rule; strength; sends opened only when all closed; the player's room keeps all but its return; declared space; hand edit; lock; no input pass; order; undo\n";
}

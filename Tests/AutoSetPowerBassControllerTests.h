#pragma once
#include <map>
// Phase 12 low-bass controller contract. Included after AutoSetPowerTests.h.
// These are synthetic A0/B0 fixtures through the native DSP, fed to the real
// controller's capture taps. The fixed shelves below are a deliberately known
// post-amp contour, not a recorded cabinet or a listening-test claim.

namespace powerBassControllerTest {
using Phase=Controller::Phase;
enum class Edit {none,duringCapture,afterRunRoundTrip,newUndoLock};
struct Run {
    std::vector<float> before,applied,beforeUndo,undone;
    Controller::Status status;
    PowerResult analysis;
    std::array<juce::String,2> ids;
    std::array<int,2> indices{};
    std::array<float,2> appliedRaw{};
    juce::String gainID;
    int gainIndex{};
    bool edited{};
};

Controller::Options options(float strength=1.f) {
    Controller::Options o;
    o.instrument=2;o.style=Style::rock;o.strength=strength;
    o.tone=o.finish=o.multiband=o.gates=o.dynamicEQ=o.level=false;
    o.amp=o.cab=o.boost=o.space=false;o.power=true;
    o.locks=Controller::lockInput|Controller::lockGate|Controller::lockCompressor;
    return o;
}

const std::vector<float>& signal(double rate,double fundamental) {
    // Strength/lock/ownership cases must hear identical input. Cache only the
    // deterministic source; each run still renders a fresh native amp state.
    static std::map<std::pair<double,double>,std::vector<float>> fixtures;
    auto [it,inserted]=fixtures.try_emplace({rate,fundamental});
    if(inserted)it->second=notes(rate,8,{fundamental,fundamental,fundamental},96);
    return it->second;
}

Run run(double rate,double fundamental,int model,const Controller::Options& o,Edit edit=Edit::none) {
    auto p=std::make_unique<ChimeraProcessor>();
    p->setRateAndBufferSizeDetails(rate,256);p->prepareToPlay(rate,256);
    const auto setRaw=[&](const juce::String& id,float v) {
        auto* q=p->parameters().getParameter(id);
        check(q!=nullptr,"Bass power controller parameter is missing: "+id.toStdString());
        q->setValueNotifyingHost(q->convertTo0to1(v));
    };
    setRaw("mode",0);setRaw("ampon1",1);
    setRaw(spectralforge::ampNativeEnabledID(0),1);
    setRaw(spectralforge::ampNativeModelID(0),float(model));
    const int channel=p->selectedAmpChannel(0),route=p->selectedAmpNativeRoute(0);
    Run result;
    const std::array<int,2> controls{powerControl(model,channel,powerPresence),powerControl(model,channel,powerResonance)};
    for(int role=0;role<2;++role) {
        check(controls[size_t(role)]>=0,"Bass controller fixture needs both power knobs");
        result.ids[size_t(role)]=spectralforge::ampNativeControlID(0,model,controls[size_t(role)],channel);
        auto* q=p->parameters().getParameter(result.ids[size_t(role)]);
        q->setValueNotifyingHost(.9f);
        result.indices[size_t(role)]=q->getParameterIndex();
    }
    const int gain=roleControls(model,channel,route)[ampGain];
    check(gain>=0,"Bass controller fixture needs a GAIN knob");
    result.gainID=spectralforge::ampNativeControlID(0,model,gain,channel);
    auto* gainParameter=p->parameters().getParameter(result.gainID);
    gainParameter->setValueNotifyingHost(.35f);result.gainIndex=gainParameter->getParameterIndex();

    // Keep the exact open-string fundamental and its lower harmonics in every
    // note; 96 harmonics also exercise the bass PRESENCE analysis band.
    const auto& input=signal(rate,fundamental);
    auto post=nativePowerRenderer(p->autoSetAmpState(0),{},controls,input,rate)({.9f,.9f});
    lowShelve(post,120,9,rate);shelve(post,2200,9,rate);
    Controller c(*p);c.setOptions(o);result.before=values(*p);c.start();
    double ms=juce::Time::getMillisecondCounterHiRes();
    for(size_t offset=0;c.busy() && offset+256<=input.size();offset+=256) {
        if(edit==Edit::duringCapture && !result.edited && offset>=size_t(rate)) {
            // A real edit history survives even if automation returns to the
            // original value before Auto Set tries to apply its proposal.
            for(const auto& id:result.ids) {
                auto* q=p->parameters().getParameter(id);const float saved=q->getValue();
                q->setValueNotifyingHost(.37f);q->setValueNotifyingHost(saved);
            }
            result.edited=true;
        }
        const float* in[]{input.data()+offset};const float* out[]{post.data()+offset};
        p->autoSetCapture().push(Tap::amp,in,1,256);
        for(auto tap:{Tap::tone,Tap::input,Tap::compressor})p->autoSetCapture().push(tap,tap==Tap::tone?out:in,1,256);
        ms+=1000.*256/rate;c.tick(ms);
    }
    check(!c.busy(),"Low-bass power controller did not finish at "+std::to_string(int(rate)));
    result.status=c.current();result.analysis=c.lastPowerAnalysis();result.applied=values(*p);
    for(int role=0;role<2;++role)result.appliedRaw[size_t(role)]=get(*p,result.ids[size_t(role)]);
    if(edit==Edit::afterRunRoundTrip) {
        for(const auto& id:result.ids) {
            auto* q=p->parameters().getParameter(id);const float saved=q->getValue();
            q->setValueNotifyingHost(.37f);q->setValueNotifyingHost(saved);
        }
        result.edited=true;
    }
    if(edit==Edit::newUndoLock) {auto locked=o;locked.locks|=Controller::lockPower;c.setOptions(locked);result.edited=true;}
    result.beforeUndo=values(*p);c.undo();result.undone=values(*p);
    p->releaseResources();return result;
}

void checkRestored(const Run& r) {
    check(r.before.size()==r.undone.size(),"Bass power undo changed the parameter inventory");
    for(size_t i=0;i<r.before.size();++i)
        check(near(r.before[i],r.undone[i],1e-6f),"UNDO AUTO did not restore the low-bass controller snapshot");
}
} // namespace powerBassControllerTest

void controllerPowerBassRegression() {
    using namespace powerBassControllerTest;
    constexpr double b0=30.86770632850775;
    int runs=0;std::array<int,2> moved{};
    // Tight 515 retains the original fixture's control topology; Niflheimr is
    // the native bass head, previously absent from the Phase 12 controller tests.
    for(double rate:{44100.,48000.,96000.})for(int model:{2,spectralforge::niflheimrAmpModel})for(double f0:{27.5,b0}) {
        std::array<float,2> fullProposal{};
        for(float strength:{1.f,.5f,.25f}) {
            const auto r=run(rate,f0,model,options(strength));++runs;
            check(r.status.phase==Phase::done && r.status.passes==std::vector<Phase>{Phase::amp}
                  && !stepOf(r.status,Phase::amp) && stepOf(r.status,Phase::power),"Bass-only power step did not run in one amp pass");
            check(r.analysis.playingDetected && r.analysis.bass,"A0/B0 controller fixture did not use bass analysis");
            for(int role=0;role<2;++role) {
                const auto& before=r.analysis.before[size_t(role)];
                const float proposed=r.analysis.proposed[size_t(role)].value;
                if(strength==1.f){fullProposal[size_t(role)]=proposed;moved[size_t(role)]+=r.analysis.changed(role);}
                else check(near(proposed,fullProposal[size_t(role)],1e-6f),"Strength changed the unscaled bass power proposal");
                const float expected=before.value+strength*(proposed-before.value);
                check(near(r.appliedRaw[size_t(role)],expected,.011f*(before.maximum-before.minimum)),"Strength did not scale an A0/B0 power knob");
                check(std::abs(proposed-before.value)<=powerMaxMove*(before.maximum-before.minimum)+1e-5f,"Bass power proposal exceeded the movement limit");
            }
            check(r.applied[size_t(r.gainIndex)]==r.before[size_t(r.gainIndex)],"Power-only step changed GAIN");
            checkRestored(r);
            std::cout<<"MEASURE bass power controller: "<<spectralforge::ampInfo(model).name<<" rate="<<rate<<" f0="<<f0<<" strength="<<strength
                     <<" presence="<<r.analysis.before[0].value<<" > "<<r.appliedRaw[0]<<" resonance="<<r.analysis.before[1].value<<" > "<<r.appliedRaw[1]<<'\n';
        }
    }
    check(moved[powerPresence]>0 && moved[powerResonance]>0,"Bass strength/undo coverage needs actual moves of both controls");
    for(double rate:{44100.,48000.,96000.})for(double f0:{27.5,b0}) {
        auto locked=options();locked.locks|=Controller::lockPower;locked.locks&=~unsigned(Controller::lockCompressor);
        const auto own=run(rate,f0,spectralforge::niflheimrAmpModel,locked);++runs;
        check(own.status.phase==Phase::done && own.status.passes==std::vector<Phase>{Phase::compressor}
              && stepOf(own.status,Phase::power) && stepOf(own.status,Phase::power)->result=="kept (locked)","Power lock did not skip the bass amp pass");
        for(int index:own.indices)check(own.applied[size_t(index)]==own.before[size_t(index)],"Power lock changed a bass power knob");
        checkRestored(own);

        auto ampLocked=options();ampLocked.amp=true;ampLocked.locks|=Controller::lockAmp;
        const auto separate=run(rate,f0,spectralforge::niflheimrAmpModel,ampLocked);++runs;
        check(separate.status.phase==Phase::done && separate.status.passes==std::vector<Phase>{Phase::amp}
              && stepOf(separate.status,Phase::amp) && stepOf(separate.status,Phase::amp)->result=="kept (locked)"
              && separate.analysis.playingDetected,"Amp lock suppressed bass power analysis");
        check(separate.applied[size_t(separate.gainIndex)]==separate.before[size_t(separate.gainIndex)],"Amp lock changed GAIN on bass");
        checkRestored(separate);
    }
    // Ownership is independent of the note name; check the round-trip edit and
    // newly selected undo lock on A0 at every sample rate.
    for(double rate:{44100.,48000.,96000.})for(auto edit:{Edit::duringCapture,Edit::afterRunRoundTrip,Edit::newUndoLock}) {
        const auto r=run(rate,27.5,spectralforge::niflheimrAmpModel,options(),edit);++runs;
        check(r.edited && r.status.phase==Phase::done && r.analysis.playingDetected,"Bass manual-edit fixture did not execute");
        for(int index:r.indices) {
            if(edit==Edit::duringCapture)check(r.applied[size_t(index)]==r.before[size_t(index)],"Auto Set overwrote a bass power edit during capture");
            check(r.undone[size_t(index)]==r.beforeUndo[size_t(index)],"UNDO AUTO overwrote a newer bass power edit or new lock");
        }
    }
    std::cout<<"PASS bass power controller: "<<runs<<" synthetic cases; A0/B0, 44.1/48/96 kHz, Tight 515/Niflheimr, 25/50/100% strength, power/amp locks, undo and edit ownership\n";
}

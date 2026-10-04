#pragma once
#include "Amplifier.h"
#include "PedalBoardDSP.h"

// Compare resolved preset PRE -> native AMP paths, before cabinet, rack and
// output trims. Normalized harmonics and input/output growth cannot be passed
// merely by turning up OUTPUT. Synthetic regression, not listening acceptance.
namespace presetGainTests {
using namespace spectralforge;
struct Metrics { double harmonics{}, rms{}; };
inline Metrics measure(AmpNativeState state, const PedalBoardState& pedals, float inputDb, double hz) {
    auto amp=std::make_unique<Amp>();auto board=std::make_unique<PedalBoardDSP>();
    const juce::dsp::ProcessSpec spec{48000,128,2};
    amp->prepare(spec);state.enabled=true;state.outputLevelDb=0;board->prepare(spec);
    amp->setNative(state);amp->setOversampling(2);
    juce::AudioBuffer<float> block(2,128);
    constexpr int warm=48000, count=4800, total=warm+count;
    std::array<double,16> real{},imag{};double energy=0;
    const float amplitude=juce::Decibels::decibelsToGain(inputDb);
    for(int base=0;base<total;base+=128) {
        const int size=std::min(128,total-base);block.setSize(2,size,false,false,true);
        for(int n=0;n<size;++n) {
            const float x=amplitude*float(std::sin(juce::MathConstants<double>::twoPi*hz*(base+n)/48000));
            block.setSample(0,n,x);block.setSample(1,n,x);
        }
        board->process(block,pedals);amp->process(block);
        for(int n=0;n<size;++n)if(base+n>=warm) {
            const double y=block.getSample(0,n),phase=juce::MathConstants<double>::twoPi*hz*(base+n)/48000;
            require(std::isfinite(y),"Nonfinite preset gain probe");energy+=y*y;
            for(size_t h=0;h<real.size();++h){real[h]+=y*std::cos((h+1)*phase);imag[h]+=y*std::sin((h+1)*phase);}
        }
    }
    double upper=0;for(size_t h=1;h<real.size();++h)upper+=real[h]*real[h]+imag[h]*imag[h];
    return {std::sqrt(upper/std::max(1.e-20,real[0]*real[0]+imag[0]*imag[0])),std::sqrt(energy/count)};
}
struct Profile { double harmonics{}, saturation{},growth{}; };
inline Profile profile(AmpNativeState amp,const PedalBoardState& board) {
    Profile result;
    auto gainBoard=board;
    // An envelope filter changes its EQ with input level. Bypass that sweep
    // only in the isolated gain probe so it cannot masquerade as compression
    // or harmonic loss. Full-preset rendering still exercises the real filter.
    for(auto& pedal:gainBoard.instances)if(pedal.model>=11&&pedal.model<=15)pedal.bypass=true;
    for(double hz:{150.,300.,600.}) {
        const auto weak=measure(amp,gainBoard,-54,hz),strong=measure(amp,gainBoard,-24,hz);
        result.harmonics+=std::log(std::max(weak.harmonics,1.e-12))/3;
        result.saturation+=std::log(std::max(weak.harmonics/strong.harmonics,1.e-12))/3;
        result.growth+=20*std::log10(strong.rms/weak.rms)/3;
    }
    result.harmonics=std::exp(result.harmonics);result.saturation=std::exp(result.saturation);return result;
}
inline void run(bool measureOnly) {
    // Owner's 2026-10-04 screenshots. Controls not shown retain native defaults.
    auto reference=defaultAmpNativeState(2);reference.channel=1;reference.inputRoute=0;
    const auto knob=[&](const char* key,float value){reference.values[size_t(ampNativeControlIndex(2,key))]=value;};
    knob("hw.low",1);knob("hw.lead.pre_gain",.86f);knob("hw.lead.post_gain",.40f);
    PedalBoardState board;board.enabled=true;
    for(auto& p:board.instances)p=defaultPedalInstance(0);
    board.instances[0]=defaultPedalInstance(10); // Variable Mu: visible controls at noon, 0.6 s, COMPRESS
    board.instances[1]=defaultPedalInstance(22); // Treble Lift 5.0
    board.instances[2]=defaultPedalInstance(1);  // Green Drive 3.0 / 6.0 / 5.0
    board.instances[2].controls[0]=.30f;board.instances[2].controls[1]=.60f;board.instances[2].controls[2]=.50f;
    const auto floor=profile(reference,board);
    std::cout<<"GAIN_REFERENCE,"<<floor.harmonics<<","<<floor.saturation<<","<<floor.growth<<'\n';
    auto rhythm=defaultAmpNativeState(2);rhythm.channel=0;rhythm.inputRoute=0;
    for(const auto& [key,value]:std::initializer_list<std::pair<const char*,float>>{
        {"hw.rhythm.pre_gain",.76f},{"hw.rhythm.post_gain",.63f},
        {"hw.rhythm.bright",1},{"hw.rhythm.crunch",1},
        // Offscreen in the owner's shot: keep the existing Tight Rhythm EQ.
        {"hw.low",.36f},{"hw.mid",.57f},{"hw.high",.57f},
        {"hw.presence",.58f},{"hw.resonance",.48f}})
        rhythm.values[size_t(ampNativeControlIndex(2,key))]=value;
    auto rhythmBoard=board;rhythmBoard.instances[0]=defaultPedalInstance(8);
    rhythmBoard.instances[0].controls={.5f,1.f,.5f,1.f,1.f};
    rhythmBoard.instances[1].bypass=true;rhythmBoard.instances[2].controls={1.f,1.f,1.f};
    const auto rhythmFloor=profile(rhythm,rhythmBoard);
    std::cout<<"GAIN_TIGHT_REFERENCE,"<<rhythmFloor.harmonics<<","<<rhythmFloor.saturation<<","<<rhythmFloor.growth<<'\n';
    // Later Dual example has a deliberately lower-drive 0.4/6.8/4.5 pedal.
    // Its amp gain knobs are offscreen: reconstruct visible settings and keep
    // the RELEASED 1.1.2 values for unseen knobs. These are an explicit lower
    // bound, not a claim that the screenshot revealed hidden amp settings.
    auto dualBoard=board;dualBoard.instances[2].controls={.04f,.68f,.45f};
    auto dual515=defaultAmpNativeState(2);dual515.channel=1;
    for(const auto& [key,value]:std::initializer_list<std::pair<const char*,float>>{
        {"hw.lead.pre_gain",.74f},{"hw.lead.post_gain",.5f},
        {"hw.low",.36f-1.5f/48},{"hw.mid",.58f+1.f/96},{"hw.high",.56f},
        {"hw.presence",.57f},{"hw.resonance",.48f}})
        dual515.values[size_t(ampNativeControlIndex(2,key))]=value;
    auto dualRect=defaultAmpNativeState(3);dualRect.channel=2;dualRect.soloEnabled=true;
    for(const auto& [key,value]:std::initializer_list<std::pair<const char*,float>>{
        {"hw.ch3.gain",.70f},{"hw.ch3.mode",2},{"hw.ch3.bass",.36f-2.f/48},
        {"hw.ch3.mid",.58f},{"hw.ch3.treble",.56f},{"hw.ch3.presence",.57f}})
        dualRect.values[size_t(ampNativeControlIndex(3,key))]=value;
    const std::array<Profile,2> dualFloor{{profile(dual515,dualBoard),profile(dualRect,dualBoard)}};
    for(size_t lane=0;lane<dualFloor.size();++lane)std::cout<<"GAIN_DUAL_REFERENCE,"<<lane<<","<<dualFloor[lane].harmonics<<","<<dualFloor[lane].saturation<<","<<dualFloor[lane].growth<<'\n';
    bool passed=true;
    for(int index:{1,3,4,8,9,10,20,29,34,35,36,37}) {
        auto processor=std::make_unique<ChimeraProcessor>();processor->loadFactoryPreset(index);
        if(measureOnly)processor->parameters().replaceState(isGuitarSignature(index)
            ?guitarSignatureSnapshot(processor->parameters(),index-factoryPresetCount)
            :factoryNativeSnapshot(processor->parameters(),index));
        AmpNativeParameterCache cache;cache.bind(processor->parameters());
        const int mode=int(processor->parameters().getRawParameterValue("mode")->load());
        const int first=mode==2?1:0,lanes=mode==0?1:mode==1?2:3;
        for(int lane=first;lane<lanes;++lane) {
            const auto amp=cache.read(ampNativeContext(mode,lane));
            const auto measured=profile(amp,processor->pedalBoardState());
            // Normalize weak-note harmonics to the SAME path at strong input:
            // raw THD across different tone stacks confuses EQ with drive.
            // The owner's later Tight Rhythm example deliberately uses RHYTHM
            // with BRIGHT/CRUNCH rather than the Melodic Death LEAD channel.
            const auto target=index==1?rhythmFloor:index==20?dualFloor[size_t(lane)]:floor;
            const bool ok=measured.saturation>=target.saturation*.90 && measured.growth<=target.growth+1.5;
            passed=passed&&ok;
            std::cout<<"GAIN_PRESET,"<<index<<","<<lane<<","<<amp.model<<","<<measured.harmonics<<","<<measured.saturation<<","<<measured.growth<<","<<(ok?"PASS":"LOW")<<'\n';
        }
    }
    if(!measureOnly)require(passed,"Lead/high-gain preset below screenshot saturation reference");
}
}

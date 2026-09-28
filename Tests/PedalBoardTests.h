#pragma once
#include "PedalBoardDSP.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace pedalBoardTests {
inline void require(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
inline float difference(const std::vector<float>& a,const std::vector<float>& b) {
    require(a.size()==b.size(),"PRE render size mismatch");float peak=0;for(size_t i=0;i<a.size();++i)peak=juce::jmax(peak,std::abs(a[i]-b[i]));return peak;
}
inline std::vector<float> render(spectralforge::PedalBoardDSP& board,spectralforge::PedalBoardState state,double rate,int block=256,int blocks=70) {
    juce::AudioBuffer<float> b(2,block);std::vector<float> result;
    for(int k=0;k<blocks;++k) {
        for(int n=0;n<block;++n) {
            const double t=(k*block+n)/rate;
            const float x=float(.1*std::sin(juce::MathConstants<double>::twoPi*311*t)+.07*std::sin(juce::MathConstants<double>::twoPi*2177*t));
            b.setSample(0,n,x);b.setSample(1,n,x*.7f);
        }
        board.process(b,state);
        for(int c=0;c<2;++c)for(int n=0;n<block;++n)require(std::isfinite(b.getSample(c,n)) && std::abs(b.getSample(c,n))<100,"PRE board non-finite/unstable output");
        if(k>=blocks-10)result.insert(result.end(),b.getReadPointer(0),b.getReadPointer(0)+block);
    }
    return result;
}
inline void run() {
    using namespace spectralforge;
    for(const double rate:{44100.,48000.,96000.}) {
        PedalBoardDSP board;board.prepare({rate,256,2});PedalBoardState state;state.enabled=true;
        // Empty slots execute only the fixed transport delay. LOW and full range
        // have identical impulse positions for every legal tap location.
        for(int tap=0;tap<=5;++tap) {
            board.reset();state.lowTap=tap;juce::AudioBuffer<float> b(2,256);b.clear();b.setSample(0,0,1);b.setSample(1,0,1);board.process(b,state);
            require(board.latency()<256,"PRE delay exceeds impulse test block");
            for(int n=0;n<256;++n) {
                require(std::abs(b.getSample(0,n)-(n==board.latency()?1.f:0.f))<1e-6f,"Empty PRE board delay is not sample-exact");
                require(std::abs(b.getSample(0,n)-board.cleanOutput().getSample(0,n))<1e-6f,"PRE LOW tap is not latency aligned");
            }
        }
        for(int model=1;model<38;++model) {
            state={};state.enabled=true;state.instances[0]=defaultPedalInstance(model);board.reset();const auto processed=render(board,state,rate);
            state.instances[0].bypass=true;board.reset();const auto dry=render(board,state,rate);
            // Flat graphic EQ is intentionally transparent at its default.
            if(model!=31)require(difference(processed,dry)>1e-5f,"Available PRE model produces no audio response");
        }
        state={};state.instances[0]=defaultPedalInstance(38);board.reset();const auto monoPending=render(board,state,rate);
        state.instances[0]=defaultPedalInstance(39);board.reset();const auto polyPending=render(board,state,rate);
        state.instances[0]=defaultPedalInstance(0);board.reset();const auto empty=render(board,state,rate);
        require(difference(monoPending,empty)<1e-6f && difference(polyPending,empty)<1e-6f,"Unimplemented octaver was activated");
        std::cout<<"PASS PRE board "<<rate<<" Hz: 37 available DSPs, Empty/LOW latency, pending octavers inert\n";
    }
    PedalBoardDSP first,second;first.prepare({48000,256,2});second.prepare({48000,256,2});
    PedalBoardState s;s.enabled=true;s.lowTap=0;
    s.instances[0]=defaultPedalInstance(26);s.instances[0].controls[0]=.15f;
    s.instances[1]=defaultPedalInstance(26);s.instances[1].controls[0]=.85f;
    auto copy=s;copy.instances[1].bypass=true;
    require(difference(render(first,s,48000),render(second,copy,48000))>.001f,"Duplicate SD-1 owner does not process independently");
    require(s.instances[0].controls[0]==.15f && s.instances[1].controls[0]==.85f,"Duplicate SD-1 control state aliased");
    s.instances[1]=defaultPedalInstance(31);s.instances[1].controls[5]=12;
    copy=s;std::swap(copy.order[0],copy.order[1]);first.reset();second.reset();
    require(difference(render(first,s,48000),render(second,copy,48000))>.001f,"PRE reorder does not change audible chain order");
    require(copy.instances[0].model==26 && copy.instances[1].model==31,"Reorder moved owner parameter identity");
    s={};s.lowTap=0;s.instances[0]=defaultPedalInstance(26);
    first.reset();juce::AudioBuffer<float> signal(2,256);for(int n=0;n<256;++n)for(int c=0;c<2;++c)signal.setSample(c,n,.2f);
    for(int k=0;k<12;++k){for(int n=0;n<256;++n)for(int c=0;c<2;++c)signal.setSample(c,n,.2f);first.process(signal,s);}
    require(std::abs(first.cleanOutput().getSample(0,200)-.2f)<1e-6f,"Post-tap drive contaminated LOW source");
    s.lowTap=1;first.process(signal,s);
    require(std::abs(first.cleanOutput().getSample(0,200)-.2f)>.001f,"Pre-tap drive was silently omitted from LOW source");
    for(const int model:{26,27,28,29,30,31,34,6,9}) {
        s={};s.instances[0]=defaultPedalInstance(model);copy=s;
        const int control=model==31?5:model==9?3:model==6?3:model==29?6:0;
        copy.instances[0].controls[(size_t)control]=pedalModel(model).controls[(size_t)control].maximum;
        first.reset();second.reset();require(difference(render(first,s,48000),render(second,copy,48000))>1e-5f,"PRE dedicated control is not connected to DSP");
    }
    // Every JB-2 route stays one owner and changes the actual circuit topology.
    s={};s.instances[0]=defaultPedalInstance(29);s.instances[0].controls[0]=.2f;s.instances[0].controls[3]=.85f;
    for(int mode=1;mode<=5;++mode) {copy=s;copy.instances[0].controls[6]=float(mode);if(mode==2)copy.instances[0].controls[7]=1;first.reset();second.reset();require(difference(render(first,s,48000),render(second,copy,48000))>1e-5f,"JB-2 route is only a label");}
    // The new Cali DRY=0 path must not inherit the old FET's hidden 12% dry.
    // The old default argument must retain exactly the previous mix equation.
    {
        DynamicsModule legacy,explicitLegacy,wetOnly;for(auto* module:{&legacy,&explicitLegacy,&wetOnly})module->prepare({48000,256,2});
        juce::AudioBuffer<float> a(2,256),b(2,256),c(2,256);
        for(int k=0;k<80;++k) {
            for(int ch=0;ch<2;++ch)for(int n=0;n<256;++n)a.setSample(ch,n,.5f);
            b.makeCopyOf(a);c.makeCopyOf(a);
            legacy.process(a,true,-30,20,1,100,0,3);
            explicitLegacy.process(b,true,-30,20,1,100,0,3,true);
            wetOnly.process(c,true,-30,20,1,100,0,3,false);
        }
        require(std::abs(a.getSample(0,255)-b.getSample(0,255))<1e-7f,"Legacy FET default mix changed");
        require(std::abs(a.getSample(0,255)-(.12f*.5f+.88f*c.getSample(0,255)))<1e-5f,"FET parallel blend compatibility equation changed");
        require(wetOnly.reduction()>legacy.reduction(),"Cali wet-only GR still contains hidden dry blend");
    }
    // Five owners only; corrupt host order cannot double-process one owner.
    s.order={4,4,-1,100,0};sanitisePedalBoard(s);std::array<bool,5> used{};for(int owner:s.order){require(owner>=0 && owner<5 && !used[(size_t)owner],"PRE invalid order was not made a permutation");used[(size_t)owner]=true;}
    std::cout<<"PASS PRE board: duplicate owners, reorder identity, LOW membership, dedicated controls, JB-2 routes, capacity invariant\n";
}
} // namespace pedalBoardTests

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
inline std::vector<float> render(spectralforge::PedalBoardDSP& board,spectralforge::PedalBoardState state,double rate,int block=256,int blocks=70,bool transient=false) {
    juce::AudioBuffer<float> b(2,block);std::vector<float> result;
    for(int k=0;k<blocks;++k) {
        for(int n=0;n<block;++n) {
            const double t=(k*block+n)/rate;
            const float envelope=transient?float(.025+.975*std::exp(-std::fmod(t,.08)*100)):1.f;
            const float x=envelope*float(.1*std::sin(juce::MathConstants<double>::twoPi*311*t)+.07*std::sin(juce::MathConstants<double>::twoPi*2177*t));
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
    // Exercise every connected control, not just one representative per family.
    // Conditional controls are tested in the route where they actually operate.
    int audited=0,unavailable=0,unresponsive=0;
    for(int model=1;model<pedalModelCount;++model) {
        const auto& spec=pedalModel(model);
        for(int control=0;control<spec.controlCount;++control) {
            if(!spec.controls[(size_t)control].connected){++unavailable;continue;}
            PedalBoardDSP a,b;for(auto* dsp:{&a,&b})dsp->prepare({48000,256,2});
            PedalBoardState low;low.instances[0]=defaultPedalInstance(model);auto& instance=low.instances[0];
            if(model==4){instance.controls[3]=.8f;instance.controls[4]=.8f;}
            if(model==8)instance.controls[1]=.8f;
            if(model==29)instance.controls[6]=control==7?2.f:control<3?1.f:0.f;
            auto high=low;instance.controls[(size_t)control]=spec.controls[(size_t)control].minimum;
            high.instances[0].controls[(size_t)control]=spec.controls[(size_t)control].maximum;
            const float delta=difference(render(a,low,48000,256,70,true),render(b,high,48000,256,70,true));
            std::cout<<"AUDIT PRE model="<<model<<" control="<<spec.controls[(size_t)control].id<<" delta="<<delta<<'\n';
            if(delta<=1e-6f)++unresponsive;++audited;
        }
    }
    require(unresponsive==0,"Connected PRE control has no response in its active route");
    std::cout<<"PASS PRE controls="<<audited<<" explicitly_unavailable="<<unavailable<<'\n';
    {
        PedalBoardDSP a,b;for(auto* dsp:{&a,&b})dsp->prepare({48000,256,2});
        PedalBoardState flat;flat.instances[0]=defaultPedalInstance(4);flat.instances[0].controls[1]=0;
        auto eq=flat;eq.instances[0].controls[3]=1;
        require(difference(render(a,flat,48000),render(b,eq,48000))>.01f,"BDDI Blend=0 incorrectly bypasses active EQ");
    }
    {
        PedalBoardDSP a,b;for(auto* dsp:{&a,&b})dsp->prepare({48000,256,2});
        PedalBoardState state;state.instances[0]=defaultPedalInstance(9);state.instances[0].controls[1]=0;state.instances[0].controls[2]=1;
        auto dry=state;dry.instances[0].bypass=true;
        const auto output=render(a,state,48000),reference=render(b,dry,48000);
        const float boost=juce::Decibels::decibelsToGain(9.f);
        for(size_t i=0;i<output.size();++i)require(std::abs(output[i]-boost*reference[i])<1e-4f,"Cali76 OUT=0 removed or attenuated independent DRY path");
    }
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
        for(int model=1;model<pedalModelCount;++model) {
            state={};state.enabled=true;state.instances[0]=defaultPedalInstance(model);board.reset();const auto processed=render(board,state,rate);
            state.instances[0].bypass=true;board.reset();const auto dry=render(board,state,rate);
            // Flat graphic EQ is intentionally transparent at its default.
            if(model!=31)require(difference(processed,dry)>1e-5f,"Available PRE model produces no audio response");
        }
        // A selected spectral octave owner adds its real frame delay, including
        // bypass; other slots retain only their small base transport delay.
        for(int position:{0,2,4})for(int tap=0;tap<=5;++tap)for(bool bypass:{false,true}) {
            PedalBoardDSP aligned;aligned.prepare({rate,256,2});state={};state.lowTap=tap;
            state.instances[(size_t)position]=defaultPedalInstance(39);auto& octave=state.instances[(size_t)position];octave.controls[0]=1;octave.controls[1]=octave.controls[2]=0;octave.bypass=bypass;
            const int total=aligned.latency(state);require(total>1000 && total<10000,"Selected octave latency is absent or imposed on all slots");
            juce::AudioBuffer<float> b(2,256);
            for(int block=0;block<(total+1024+255)/256;++block) {
                b.clear();if(block==0){b.setSample(0,0,1);b.setSample(1,0,1);}aligned.process(b,state);
                require(aligned.latency()==total,"Board state latency and processing latency disagree");
                for(int n=0;n<256;++n) {
                    const float expected=block*256+n==total?1.f:0.f;
                    require(std::abs(b.getSample(0,n)-expected)<1e-6f,"Poly dry/bypass latency is not sample-exact in the board");
                    require(std::abs(b.getSample(0,n)-aligned.cleanOutput().getSample(0,n))<1e-6f,"Poly before/after LOW tap is misaligned");
                }
            }
        }
        std::cout<<"PASS PRE board "<<rate<<" Hz: 39 available DSPs, Empty/LOW latency, selected poly latency and bypass alignment\n";
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

#pragma once
// Research harness only. No product/standard v3/v4 defaults are changed.
#include "TransientSplitPitch.h"
#include <string>
#include <stdexcept>
namespace transposeNext {
struct CorrelationDiagnostics {
    struct Before { double age; bool fading; };
    static Before before(const CorrelationPitch& p) {
        return {p.active?p.delay:(p.delta>0?p.minimum:p.maximum),p.fading};
    }
    static double tap(const CorrelationPitch& p,double age) {
        double pos=double(p.write)-1-age;
        while(pos<0)pos+=p.capacity;
        const int i=int(pos);const float f=float(pos-i);
        const auto& r=p.audio[0];return r[size_t(i)]+f*(r[size_t((i+1)%p.capacity)]-r[size_t(i)]);
    }
    template<class Stream> static void emit(Stream& s,const CorrelationPitch& p,Before b) {
        const bool mixed=p.fading||b.fading;
        const double next=p.nextDelay-p.delta;
        const float weight=mixed?p.activeWeights[p.fadeIndex-1]:0.f;
        const float old=float(tap(p,b.age)),fresh=mixed?float(tap(p,next)):old;
        const float reconstructed=mixed?old+weight*(fresh-old):old;
        s<<','<<b.age<<','<<next<<','<<weight<<','<<old<<','<<fresh<<','<<reconstructed
         <<','<<p.onsetAge<<','<<p.onsets<<','<<p.spliceCount<<','<<p.fadeIndex<<','<<p.activeFade;
    }
};
struct SplitDiagnostics {
    static void configure(TransientSplitPitch& p,const std::string& engine,int shift) {
        if(engine=="corr40-split-v3"||engine=="corr40-split-v4")return;
        if(engine=="env-floor075")p.body.setUpshiftMinimum(.75);
        else if(engine=="env-floor100")p.body.setUpshiftMinimum(1.);
        else if(engine=="env-floor125")p.body.setUpshiftMinimum(1.25);
        else if(engine=="env-floor150")p.body.setUpshiftMinimum(1.5);
        else if(engine=="env-fade4"){if(shift>0)p.body.setSpliceFade(4);}
        else if(engine=="env-fade8"){if(shift>0)p.body.setSpliceFade(8);}
        else throw std::runtime_error("unknown envelope research variant");
    }
    static CorrelationDiagnostics::Before before(const TransientSplitPitch& p) {return CorrelationDiagnostics::before(p.body);}
    template<class Stream> static void emit(Stream& s,const TransientSplitPitch& p,CorrelationDiagnostics::Before b) {
        CorrelationDiagnostics::emit(s,p.body,b);
        s<<','<<p.bodyIn.getSample(0,0)<<','<<p.bodyOut.getSample(0,0)<<','<<p.highOut.getSample(0,0)
         <<','<<p.lowResets<<','<<p.transient.onsetResets();
    }
};
}

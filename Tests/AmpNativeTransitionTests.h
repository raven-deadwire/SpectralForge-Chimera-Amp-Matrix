#pragma once
#include "AmpNativeDSP.h"
#include <iostream>
#include <cmath>
#include <string_view>
namespace ampNativeTransitionTests {
using namespace spectralforge;
inline void put(AmpNativeState& s,std::string_view key,float value){s.values[size_t(ampNativeControlIndex(s.model,key))]=value;}
inline float wave(int n,double rate,float level=.08f){return level*float(std::sin(6.283185307179586*173*n/rate)+.27*std::sin(6.283185307179586*1997*n/rate));}
inline int run(){
// Trim precedes detector as well as the audio path: compare with externally
// applying the same trim to the source before entering an untrimmed instance.
float gateResidual=0;
for(double rate:{44100.,48000.,96000.,384000.}){auto a=defaultAmpNativeState(15);a.channel=0;put(a,"ch1.gate",1);put(a,"gate_threshold",.5f);a.inputTrimDb=12;auto b=a;b.inputTrimDb=0;AmpNativeDSP da,db;da.prepare(rate);db.prepare(rate);da.set(a);db.set(b);da.reset();db.reset();const float gain=std::pow(10.f,.6f);
for(int n=0;n<int(rate*.5);++n){const float x=wave(n,rate,.0008f);gateResidual=std::max(gateResidual,std::abs(da.tick(x,0)-db.tick(x*gain,0)));}}
if(gateResidual>1e-7f){std::cerr<<"FAIL trim detector equivalence "<<gateResidual<<"\n";return 1;}
// Turning reverb down must keep its tank advancing. Both instances receive
// identical input, but one hides its reverb for an interval before restoring it.
double tailDifference=0,tailReference=0;
for(int model:{0,4,8,9,16}){auto s=defaultAmpNativeState(model);if(model==0)s.channel=1;for(size_t i=0;i<ampNativePanel(model).controls.size();++i){const auto key=std::string_view(ampNativePanel(model).controls[i].key);if(key.find("intensity")!=key.npos||key.find("depth")!=key.npos)s.values[i]=0;}
const auto reverb=ampNativeControlIndex(model,model==0?"hw.vibrato.reverb":model==8?"hw.reverb_level":"hw.reverb");s.values[size_t(reverb)]=.7f;AmpNativeDSP a,b;a.prepare(48000);b.prepare(48000);a.set(s);b.set(s);a.reset();b.reset();for(int n=0;n<4800;++n){const float x=wave(n,48000);a.tick(x,0);b.tick(x,0);}auto quiet=s;quiet.values[size_t(reverb)]=0;a.set(quiet);for(int n=0;n<96000;++n){a.tick(0,0);b.tick(0,0);}a.set(s);for(int n=0;n<12000;++n){const float ya=a.tick(0,0),yb=b.tick(0,0);if(n>6000){tailDifference=std::max(tailDifference,double(std::abs(ya-yb)));tailReference=std::max(tailReference,double(std::abs(yb)));}}}
if(tailDifference>1e-6){std::cerr<<"FAIL hidden tank resurrects "<<tailDifference<<"\n";return 2;}
// An EQ edit starts at the previous response for the first 15 samples and then
// transitions. Sweep two mid frequencies repeatedly at 8x sample rate and
// toggle a filter's insertion/removal to exercise identity ramps.
float immediateResidual=0;double editedDifference=0;float transitionPeak=0;
for(double rate:{48000.,384000.}){auto base=defaultAmpNativeState(11);put(base,"hw.lo_mid",.9f);put(base,"hw.hi_mid",.1f);AmpNativeDSP a,b;a.prepare(rate);b.prepare(rate);a.set(base);b.set(base);a.reset();b.reset();for(int n=0;n<2048;++n){a.tick(wave(n,rate),0);b.tick(wave(n,rate),0);}auto edited=base;put(edited,"hw.lo_mid_frequency",1);put(edited,"hw.hi_mid_frequency",0);a.set(edited);for(int n=0;n<int(rate*.04);++n){const float ya=a.tick(wave(n+2048,rate),0),yb=b.tick(wave(n+2048,rate),0);if(n<15)immediateResidual=std::max(immediateResidual,std::abs(ya-yb));if(n>rate*.022)editedDifference+=double(ya-yb)*(ya-yb);transitionPeak=std::max(transitionPeak,std::abs(ya));if(!std::isfinite(ya))return 3;}
for(int step=0;step<80;++step){put(edited,"hw.bright",step%2);put(edited,"hw.deep",(step/2)%2);put(edited,"hw.lo_mid_frequency",step%2);a.set(edited);for(int n=0;n<257;++n){const float y=a.tick(wave(step*257+n,rate),0);if(!std::isfinite(y))return 4;transitionPeak=std::max(transitionPeak,std::abs(y));}}}
if(immediateResidual>1e-7f||editedDifference<1e-5||transitionPeak>4){std::cerr<<"FAIL filter transition "<<immediateResidual<<" "<<editedDifference<<" "<<transitionPeak<<"\n";return 5;}
std::cout<<"PASS trim_gate_residual="<<gateResidual<<" hidden_tank_difference="<<tailDifference<<" hidden_tank_reference="<<tailReference<<" first15_coeff_edit_residual="<<immediateResidual<<" edited_difference_energy="<<editedDifference<<" transition_peak="<<transitionPeak<<"\n";
return 0;
}

} // namespace ampNativeTransitionTests

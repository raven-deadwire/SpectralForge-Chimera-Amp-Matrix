// 1.5 six-band multiband expansion (docs/SIX_BAND_1_5.md): four bands ignore
// the new settings; six bands reconstruct flat and match the LR4 band sum;
// equal upper gains reproduce four bands; independent band cuts, solo and the
// sidechain key per band; automation stress without solver failures; the
// band-count switch fades; parameters, state and the processor; CPU cost.
#include "PluginProcessor.h"
#include "MultibandDynamics.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {thread_local bool countAllocations=false;thread_local size_t allocations=0;}
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using namespace spectralforge;
using C=std::complex<double>;
constexpr double pi=3.14159265358979323846;
using Audio=std::array<std::vector<float>,2>;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
double db(double v){return 20*std::log10(std::max(v,1.e-30));}

void run(MultibandDynamicsCore& core,Audio& audio,int channels,const std::vector<int>& blocks={137},const Audio* key=nullptr) {
    size_t offset=0,index=0;
    while(offset<audio[0].size()) {
        const int n=std::min(int(audio[0].size()-offset),blocks[index++%blocks.size()]);
        float* a[2]{audio[0].data()+offset,audio[1].data()+offset};
        const float* k[2]{key?(*key)[0].data()+offset:nullptr,key?(*key)[1].data()+offset:nullptr};
        countAllocations=true;core.process(a,channels,n,key?k:nullptr,key?2:0);countAllocations=false;
        offset+=size_t(n);
    }
}
Audio tones(double rate,double seconds,const std::vector<double>& hz,double amplitude) {
    const size_t length=size_t(rate*seconds);Audio a{std::vector<float>(length),std::vector<float>(length)};
    for(size_t i=0;i<length;++i){double x=0;for(double f:hz)x+=amplitude*std::sin(2*pi*f*double(i)/rate);a[0][i]=float(x);a[1][i]=float(-.25*x);}
    return a;
}
double amplitude(const std::vector<float>& audio,double rate,double hz,size_t start) {
    C sum{};for(size_t n=start;n<audio.size();++n)sum+=double(audio[n])*std::polar(1.,-2*pi*hz*double(n)/rate);
    return 2*std::abs(sum)/double(audio.size()-start);
}
MultibandSettings six(std::array<float,5> x={120,700,3500,6000,11000}) {
    MultibandSettings s;s.on=true;s.bandCount=6;s.crossovers={x[0],x[1],x[2]};s.upperCrossovers={x[3],x[4]};return s;
}
// Bilinear Butterworth^2 oracle for K crossovers, independent of the SVFs.
std::vector<C> oracle(double rate,double hz,const std::vector<double>& x) {
    const C zi=std::polar(1.,-2*pi*hz/rate),q=(1.-zi)/(1.+zi);const size_t K=x.size();
    std::vector<C> lo(K),hi(K),ap(K),out(K+1);
    for(size_t i=0;i<K;++i){const C u=q/std::tan(pi*x[i]/rate);const C d=1.+std::sqrt(2.)*u+u*u;lo[i]=1./(d*d);hi[i]=u*u*u*u*lo[i];ap[i]=lo[i]+hi[i];}
    for(size_t k=0;k<=K;++k){C v=1;for(size_t i=0;i<k;++i)v*=hi[i];if(k<K)v*=lo[k];for(size_t j=k+1;j<K;++j)v*=ap[j];out[k]=v;}
    return out;
}
std::vector<double> applied(const MultibandDynamicsCore& core) {
    const auto& s=core.getSettings();return {s.crossovers[0],s.crossovers[1],s.crossovers[2],s.upperCrossovers[0],s.upperCrossovers[1]};
}
C response(const std::vector<float>& impulse,double rate,double hz,double scale) {
    C sum{};for(size_t n=0;n<impulse.size();++n)sum+=double(impulse[n])*std::polar(1.,-2*pi*hz*double(n)/rate)/scale;return sum;
}

void fourBandsIgnoreTheNewSettings() {
    MultibandSettings plain;plain.on=true;
    for(int b=0;b<4;++b){auto& v=plain.bands[size_t(b)];v.threshold=-40;v.ratio=4;v.makeup=float(3*b-4);}
    auto odd=plain;
    odd.upperCrossovers={300.f,200.f};
    for(int b=4;b<6;++b){auto& v=odd.bands[size_t(b)];v.solo=true;v.makeup=-24;v.mode=3;v.threshold=-70;}
    Audio a=tones(48000,.5,{60,400,2000,9000},.2),b=a;
    MultibandDynamicsCore one,two;one.setSettings(plain);one.prepare(48000);two.setSettings(odd);two.prepare(48000);
    run(one,a,2);run(two,b,2);
    require(a==b,"Four bands are affected by the band 5/6 settings or crossovers 4/5");
    require(one.activeBandCount()==4 && one.reduction(4)==0.f && one.reduction(5)==0.f,"Four bands report six");
    std::cout<<"PASS four bands: band 5/6 settings (even solo) and crossovers 4/5 change nothing\n";
}

void reconstruction() {
    double worst=0;
    for(double rate:{44100.,48000.,96000.,192000.}) {
        auto s=six();for(auto& b:s.bands)b.ratio=1;
        MultibandDynamicsCore core;core.setSettings(s);core.prepare(rate);
        Audio impulse{std::vector<float>(65536),std::vector<float>(65536)};impulse[0][0]=.125f;impulse[1][0]=-.03125f;
        run(core,impulse,2,{0,1,31,257,2049});
        double energy=0;for(float v:impulse[0])energy+=double(v)*v;
        require(std::abs(energy/(.125*.125)-1)<1.e-5,"Neutral six-band sum is not allpass in energy");
        for(double hz:{25.,80.,120.,500.,700.,2000.,3500.,5000.,6000.,8000.,11000.,15000.,18000.})
            {const double e=db(std::abs(response(impulse[0],rate,hz,.125)));worst=std::max(worst,std::abs(e));require(std::abs(e)<.001,"Six-band neutral sum has a crossover notch");}
        // Mix 0 and all bands bypassed reconstruct raw samples exactly.
        s.mix=0;core.setSettings(s);core.reset();auto dry=tones(rate,.25,{80,700,7000},.1),zero=dry;run(core,zero,2);
        require(zero==dry,"Six-band zero mix is not exact raw audio");
    }
    std::cout<<"PASS six-band neutral settings return the input (magnitude error "<<worst<<" dB, allpass energy); zero mix exact raw at four rates\n";
}

// Static band gains (makeup, ratio 1) against the LR4 oracle, including one
// band at 0 dB and the rest at -24 dB, and the tightest crossover spacing.
void weightedMagnitude() {
    double worst=0;unsigned rng=7;int cases=0;
    const std::array<std::array<float,5>,5> sets{{{120,700,3500,6000,11000},{40,56,78.4f,109.76f,153.664f},{80,250,800,2500,8000},{2000,2800,3920,5488,7683.2f},{40,400,4000,8000,16000}}};
    for(double rate:{44100.,48000.,96000.,192000.})for(const auto& x:sets)for(int trial=0;trial<12;++trial) {
        auto s=six(x);std::array<double,6> makeup{};
        for(size_t b=0;b<6;++b){rng=rng*1664525u+1013904223u;s.bands[b].ratio=1;makeup[b]=trial<6?double(int(rng>>24)%49-24):(int(b)==trial-6?0.:-24.);s.bands[b].makeup=float(makeup[b]);}
        MultibandDynamicsCore core;core.setSettings(s);core.prepare(rate);
        Audio impulse{std::vector<float>(65536),std::vector<float>(65536)};impulse[0][0]=.125f;
        run(core,impulse,1,{257});
        const auto xs=applied(core);
        for(double hz:{20.,31.,56.,80.,120.,250.,700.,1000.,2000.,3500.,5000.,6000.,8000.,11000.,14000.,16000.,19000.}) {
            if(hz>=rate*.45)continue;
            const auto bands=oracle(rate,hz,xs);C expected{};for(size_t b=0;b<6;++b)expected+=bands[b]*std::pow(10.,makeup[b]/20.);
            const double error=std::abs(db(std::abs(response(impulse[0],rate,hz,.125)))-db(std::abs(expected)));
            worst=std::max(worst,error);
        }
        require(core.synthesisSolverFailures()==0,"Quintic solver failed");
        ++cases;
    }
    require(worst<1.e-3,"Six-band weighted sum misses the LR4 band magnitudes (worst "+std::to_string(worst)+" dB)");
    std::cout<<"PASS weighted six-band magnitude in "<<cases<<" cases (4 rates, 5 crossover sets): worst "<<worst<<" dB\n";
}

// With bands 4-6 at one gain, six bands have the four-band magnitude, and a
// minimum-phase filter is fixed by its magnitude: the outputs agree.
void equalUpperBandsMatchFour() {
    double worst=0;
    for(double rate:{48000.,96000.}) {
        MultibandSettings four;four.on=true;
        auto s=six();
        const std::array<float,6> makeup{-6,3,-9,6,6,6};
        for(size_t b=0;b<6;++b){s.bands[b].ratio=1;s.bands[b].makeup=makeup[b];if(b<4){four.bands[b].ratio=1;four.bands[b].makeup=makeup[b];}}
        MultibandDynamicsCore a,b;a.setSettings(four);a.prepare(rate);b.setSettings(s);b.prepare(rate);
        auto x=tones(rate,.5,{55,300,1500,5000,9000,14000},.1),y=x;
        run(a,x,2);run(b,y,2);
        for(size_t n=0;n<x[0].size();++n)worst=std::max(worst,double(std::abs(x[0][n]-y[0][n])));
    }
    require(worst<2.e-6,"Equal upper bands do not reproduce four bands");
    std::cout<<"PASS bands 4-6 at one gain reproduce four bands: largest float sample difference "<<worst<<"\n";
}

// Bands narrower than an octave (3500-6000-11000 Hz) do not pass their
// centre at full level, so every tone is compared with the LR4 oracle.
void bandIsolationSoloAndKey() {
    const double rate=48000;const std::vector<double> hz{50,300,1500,4500,8000,14000};
    double worst=0;
    for(int selected=0;selected<6;++selected) {
        auto s=six();s.detector=0;
        for(auto& b:s.bands){b.bypass=true;b.knee=0;}
        auto& band=s.bands[size_t(selected)];band.bypass=false;band.threshold=-48;band.range=6;band.ratio=20;band.attack=.1f;
        MultibandDynamicsCore core;core.setSettings(s);core.prepare(rate);
        auto audio=tones(rate,1,hz,.1);run(core,audio,2);
        const auto xs=applied(core);
        for(size_t f=0;f<6;++f){
            const auto bands=oracle(rate,hz[f],xs);C expected{};for(size_t b=0;b<6;++b)expected+=bands[b]*(int(b)==selected?.5:1.);
            const double error=std::abs(db(amplitude(audio[0],rate,hz[f],24000)/.1)-db(std::abs(expected)));
            worst=std::max(worst,error);
            require(error<.1,"Band "+std::to_string(selected+1)+" cut: tone "+std::to_string(int(hz[f]))+" Hz off the oracle by "+std::to_string(error)+" dB");
        }
        require(std::abs(core.reduction(selected)-6.f)<.005f,"Band meter wrong");
        s.bands[size_t(selected)].solo=true;s.bands[size_t(selected)].bypass=true;core.setSettings(s);core.reset();
        audio=tones(rate,1,hz,.1);run(core,audio,1);
        for(size_t f=0;f<6;++f){
            const double expected=.1*std::abs(oracle(rate,hz[f],xs)[size_t(selected)]);
            require(std::abs(amplitude(audio[0],rate,hz[f],24000)-expected)<.002,"Solo band "+std::to_string(selected+1)+" does not pass exactly its band at "+std::to_string(int(hz[f]))+" Hz");
        }
    }
    // Sidechain key: a loud 8 kHz key compresses band 5 and leaves bands 1-3.
    auto s=six();
    for(auto& b:s.bands){b.threshold=-30;b.ratio=10;b.range=12;b.knee=0;}
    MultibandDynamicsCore core;core.setSettings(s);core.prepare(rate);
    auto audio=tones(rate,1,hz,.01),key=tones(rate,1,{8000},.5);run(core,audio,2,{256},&key);
    for(int f=0;f<6;++f){const double level=db(amplitude(audio[0],rate,hz[size_t(f)],24000)/.01);
        // Bands 4 and 6 also hear the key through their LR4 slopes (and
        // band 4's cut reaches the 1.5 kHz tone slightly); bands 1-3 do not.
        if(f==4)require(level<-6,"Band 5 key did not cut the 8 kHz tone");else if(f<2)require(std::abs(level)<.01,"Band 5 key moved a distant band");}
    require(std::abs(core.reduction(4)-12.f)<.01f && core.reduction(0)==0.f && core.reduction(1)==0.f && core.reduction(2)==0.f,"Key-driven band meters wrong");
    std::cout<<"PASS six independent band cuts (all tones within "<<worst<<" dB of the LR4 oracle) and meters, solo per band, band-5 sidechain key\n";
}

// Nonuniform extremes, all modes, M/S channels, solo and crossover motion at
// the tightest spacing, mix and on/off events: finite, block-independent,
// no solver failure, no allocation.
void automationStress() {
    for(double rate:{44100.,48000.,96000.}) {
        int failures=-1;
        auto render=[&](bool fragmented){
            MultibandDynamicsCore core;auto s=six({40,56,78.4f,109.76f,153.664f});
            for(size_t b=0;b<6;++b){auto& v=s.bands[b];v.makeup=b%2?24.f:-24.f;v.ratio=4;v.range=36;v.attack=.1f;v.release=5;v.mode=int(b%4);v.threshold=-48;v.channels=int(b%3);}
            s.stereo=1;s.link=.3f;
            core.setSettings(s);core.prepare(rate);
            auto audio=tones(rate,.3,{27.5,41.2,700,3500,9000},.0001);
            const int event=std::max(1,int(rate*.003));
            for(int offset=0,index=0;offset<int(audio[0].size());++index) {
                s.on=(index%7)<5;
                if(index%9==0){const bool low=index%18==0;s.crossovers=low?std::array<float,3>{40,56,78.4f}:std::array<float,3>{2000,2800,3920};s.upperCrossovers=low?std::array<float,2>{109.76f,153.664f}:std::array<float,2>{5488,7683.2f};}
                s.bands[size_t(index%6)].solo=index%5==0;
                s.mix=float(index%11)/10.f;
                core.setSettings(s);
                const int length=std::min(event,int(audio[0].size())-offset);
                for(int n=0;n<length;) {
                    const int count=fragmented?std::min(1+n%31,length-n):length;
                    float* p[]{audio[0].data()+offset+n,audio[1].data()+offset+n};
                    countAllocations=true;core.process(p,2,count);countAllocations=false;
                    n+=count;
                }
                offset+=length;
            }
            for(size_t n=0;n<audio[0].size();++n)require(std::isfinite(audio[0][n])&&std::abs(audio[0][n])<8.f&&std::isfinite(audio[1][n]),"Six-band automation became unstable");
            failures=core.synthesisSolverFailures();
            return audio;
        };
        allocations=0;
        require(render(false)==render(true),"Six-band output depends on host block size");
        require(failures==0,"Quintic solver failed under automation");
        require(allocations==0,"Six-band processing allocated");
    }
    std::cout<<"PASS six-band automation extremes (modes, M/S channels, solo, tightest crossovers, mix, on/off): finite, block-independent, no solver failure or allocation\n";
}

// 4 <-> 6 while running fades out, settles 30 ms and fades in, like the M/S
// switch; the step-free fade keeps the waveform as smooth as the input.
void bandCountSwitch() {
    const double rate=48000;
    auto s=six();s.bandCount=4;
    for(auto& b:s.bands){b.ratio=1;b.makeup=-6;}
    s.bands[4].makeup=-18;s.bands[5].makeup=-18;
    MultibandDynamicsCore core;core.setSettings(s);core.prepare(rate);
    auto audio=tones(rate,.6,{220,8000},.2);const auto input=audio;
    const int at=int(rate*.2);
    float* p[2]{audio[0].data(),audio[1].data()};core.process(p,2,at);
    s.bandCount=6;core.setSettings(s);
    int switched=-1;
    for(int n=at;n<int(audio[0].size());n+=64){
        float* q[2]{audio[0].data()+n,audio[1].data()+n};core.process(q,2,std::min(64,int(audio[0].size())-n));
        if(switched<0 && core.activeBandCount()==6)switched=n;
    }
    require(switched>=at && switched<at+int(rate*.02),"Band count did not switch after the fade-out");
    double curve=0,inputCurve=0;
    for(int n=at-200;n<at+int(rate*.1);++n){
        curve=std::max(curve,double(std::abs(audio[0][size_t(n+1)]-2*audio[0][size_t(n)]+audio[0][size_t(n-1)])));
        inputCurve=std::max(inputCurve,double(std::abs(input[0][size_t(n+1)]-2*input[0][size_t(n)]+input[0][size_t(n-1)])));
    }
    require(curve<inputCurve*1.05,"Band-count switch has a step");
    const double high=db(amplitude(audio[0],rate,8000,size_t(rate*.4))/.2),low=db(amplitude(audio[0],rate,220,size_t(rate*.4))/.2);
    const auto bands=oracle(rate,8000,applied(core));C expected{};for(size_t b=0;b<6;++b)expected+=bands[b]*std::pow(10.,s.bands[b].makeup/20.);
    require(std::abs(high-db(std::abs(expected)))<.05 && std::abs(low+6)<.05,"After the switch the bands do not apply their gains ("+std::to_string(high)+" / "+std::to_string(db(std::abs(expected)))+" dB)");
    // A 220 Hz tone alone has a small second difference, so a step would show.
    double lowCurve=0,lowInput=0;
    {
        auto t=six();t.bandCount=6;for(auto& b:t.bands){b.ratio=1;b.makeup=-6;}
        MultibandDynamicsCore one;one.setSettings(t);one.prepare(rate);
        auto x=tones(rate,.4,{220},.2);const auto in=x;
        for(int n=0,k=0;n<int(x[0].size());n+=64,++k){
            if(k==150){t.bandCount=4;one.setSettings(t);}
            float* q[2]{x[0].data()+n,x[1].data()+n};one.process(q,2,std::min(64,int(x[0].size())-n));
        }
        for(size_t n=150*64-100;n<150*64+size_t(rate*.08);++n){
            lowCurve=std::max(lowCurve,double(std::abs(x[0][n+1]-2*x[0][n]+x[0][n-1])));
            lowInput=std::max(lowInput,double(std::abs(in[0][n+1]-2*in[0][n]+in[0][n-1])));
        }
        require(lowCurve<lowInput*1.2,"6 -> 4 band switch has a step at 220 Hz");
    }
    std::cout<<"PASS 4 -> 6 band switch: swapped "<<(switched-at)*1000./rate<<" ms after the change, largest second difference "<<curve<<" (input "<<inputCurve<<"); then 8 kHz "<<high<<" dB, 220 Hz "<<low<<" dB; 6 -> 4 at 220 Hz alone: "<<lowCurve<<" (input "<<lowInput<<")\n";
}

// Review (10/30): band-count changes must never let the live crossovers
// cross (the synthesis residues are singular there) or leave the band.
double peakOf(const Audio& a){double m=0;for(const auto& c:a)for(float x:c)m=std::max(m,double(std::abs(x)));return m;}
void crossoverSafety() {
    std::array<double,4> peaks{};
    const auto noise=[](double rate,double seconds){Audio a{std::vector<float>(size_t(rate*seconds)),std::vector<float>(size_t(rate*seconds))};unsigned r=3;for(size_t i=0;i<a[0].size();++i){r=r*1664525u+1013904223u;a[0][i]=float((double(r>>8)/16777216.-.5)*.35);a[1][i]=a[0][i];}return a;};
    {   // 6 -> 4 while on, crossover 3 at 16 kHz (six bands clamp it to 8.2 kHz), +-12 dB makeup.
        auto s=six({120,700,16000,6000,11000});for(size_t b=0;b<6;++b)s.bands[b].makeup=b%2?12.f:-12.f;
        MultibandDynamicsCore core;core.setSettings(s);core.prepare(48000);auto a=noise(48000,.6);
        for(int k=0;k*256<int(a[0].size());++k){if(k==60){s.bandCount=4;core.setSettings(s);}float* p[2]{a[0].data()+k*256,a[1].data()+k*256};core.process(p,2,std::min(256,int(a[0].size())-k*256));}
        peaks[0]=peakOf(a);
    }
    {   // Four bands on, off, switch to six while off, on again.
        MultibandSettings s;s.on=true;s.crossovers={120,700,16000};for(size_t b=0;b<6;++b)s.bands[b].makeup=b%2?24.f:-24.f;
        MultibandDynamicsCore core;core.setSettings(s);core.prepare(48000);auto a=noise(48000,.8);
        for(int k=0;k*256<int(a[0].size());++k){if(k==40){s.on=false;core.setSettings(s);}if(k==60){s.bandCount=6;core.setSettings(s);}if(k==80){s.on=true;core.setSettings(s);}
            float* p[2]{a[0].data()+k*256,a[1].data()+k*256};core.process(p,2,std::min(256,int(a[0].size())-k*256));}
        peaks[1]=peakOf(a);
    }
    {   // 22.05 kHz, crossover 5 set to 12 kHz (above Nyquist), 6 -> 4.
        auto s=six({120,700,3500,6000,12000});
        MultibandDynamicsCore core;core.setSettings(s);core.prepare(22050);auto a=noise(22050,.4);
        for(int k=0;k*256<int(a[0].size());++k){if(k==10){s.bandCount=4;core.setSettings(s);}float* p[2]{a[0].data()+k*256,a[1].data()+k*256};core.process(p,2,std::min(256,int(a[0].size())-k*256));}
        peaks[2]=peakOf(a);
    }
    {   // Random band count, M/S and on/off every 3-40 ms at 8 and 48 kHz, crossovers at the limits.
        for(double rate:{8000.,48000.}){
            auto s=six({120,700,16000,16000,16000});for(size_t b=0;b<6;++b)s.bands[b].makeup=b%2?24.f:-24.f;
            MultibandDynamicsCore core;core.setSettings(s);core.prepare(rate);auto a=noise(rate,2);unsigned r=11;
            for(size_t off=0;off<a[0].size();){r=r*1664525u+1013904223u;const int n=int(rate*(.003+.037*double(r>>24)/255.));
                s.bandCount=(r>>3)&1?6:4;s.stereo=int((r>>5)&1);s.on=((r>>7)&3)!=0;core.setSettings(s);
                const int len=std::min(n,int(a[0].size()-off));float* p[2]{a[0].data()+off,a[1].data()+off};core.process(p,2,len);off+=size_t(len);}
            peaks[3]=std::max(peaks[3],peakOf(a));
        }
    }
    // +-24 dB makeup on 0.35 noise: about 6 at most; a crossing reaches 1e4 to 1e15.
    require(peaks[0]<10 && peaks[1]<10 && peaks[2]<10 && peaks[3]<20,"A band-count change let the crossovers cross ("+std::to_string(peaks[0])+", "+std::to_string(peaks[1])+", "+std::to_string(peaks[2])+", "+std::to_string(peaks[3])+")");
    std::cout<<"PASS band-count changes keep the crossovers ordered: peaks "<<peaks[0]<<" (6 -> 4 on), "<<peaks[1]<<" (changed while off), "<<peaks[2]<<" (22.05 kHz), "<<peaks[3]<<" (random toggles)\n";
}

void parametersStateAndProcessor() {
    auto owner=std::make_unique<ChimeraProcessor>();auto& processor=*owner;auto& state=processor.parameters();
    const int total=processor.getParameters().size();int seen=0;
    for(auto* raw:processor.getParameters())if(raw->getVersionHint()==multibandSixBandParameterVersion){
        require(raw->getParameterIndex()>=total-oversamplingAutoParameterCount-truePeakLimiterParameterCount-multibandSixBandParameterCount,"Six-band bank is not last");++seen;}
    require(seen==multibandSixBandParameterCount,"Six-band bank size");
    for(const char* id:{"mbBandCount","mbCrossover4","mbCrossover5","mbBand5_threshold","mbBand6_channels","mbBand6_solo"})require(state.getParameter(id)!=nullptr,std::string("Missing ")+id);
    require(state.getRawParameterValue("mbBandCount")->load()==0.f,"Default is not four bands");
    const auto set=[&](const char* id,float v){auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));};
    // Validation: in six-band mode crossover 3 must leave room for 4 and 5.
    set("mbBandCount",1);set("mbCrossover3",9000);set("mbCrossover4",9500);set("mbCrossover5",9800);
    processor.prepareToPlay(48000,512);
    const auto x=processor.multibandDynamics().allCrossovers();
    for(size_t i=1;i<x.size();++i)require(x[i]>=x[i-1]*1.399f,"Six-band crossovers are not ordered with spacing");
    require(processor.multibandDynamics().bandCount()==6,"Band count not read");
    // State: band 6 settings and the band count survive a save/restore.
    set("mbBand6_makeup",-12);set("mbCrossover5",12500);
    juce::MemoryBlock data;processor.getStateInformation(data);
    auto restoredOwner=std::make_unique<ChimeraProcessor>();auto& restored=*restoredOwner;restored.setStateInformation(data.getData(),int(data.getSize()));
    require(std::abs(restored.parameters().getRawParameterValue("mbBand6_makeup")->load()+12)<1.e-4f && restored.parameters().getRawParameterValue("mbBandCount")->load()>.5f,"Six-band state not restored");
    // Through the processor's wrapper: band 6 makeup cuts a 14 kHz tone in six-band mode only.
    for(int count:{4,6}) {
        MultibandDynamics mb;mb.bind(state);
        set("mbOn",1);set("mbBandCount",count==6?1.f:0.f);set("mbCrossover1",120);set("mbCrossover2",700);set("mbCrossover3",3500);set("mbCrossover4",6000);set("mbCrossover5",11000);
        for(int b=1;b<=6;++b){set(("mbBand"+std::to_string(b)+"_ratio").c_str(),1);}
        mb.prepare(48000);
        juce::AudioBuffer<float> buffer(2,512);std::vector<float> out;
        for(int k=0;k<120;++k){for(int n=0;n<512;++n){const float v=float(.1*std::sin(2*pi*14000*double(k*512+n)/48000));buffer.setSample(0,n,v);buffer.setSample(1,n,v);}
            mb.process(buffer);for(int n=0;n<512;++n)out.push_back(buffer.getSample(0,n));}
        const double level=db(amplitude(out,48000,14000,out.size()/2)/.1);
        double expected=0;
        if(count==6){const auto x=mb.allCrossovers();const auto bands=oracle(48000,14000,{x[0],x[1],x[2],x[3],x[4]});C sum{};for(size_t b=0;b<6;++b)sum+=bands[b]*(b==5?std::pow(10.,-12./20.):1.);expected=db(std::abs(sum));}
        require(std::abs(level-expected)<.05,"Wrapper band count wrong ("+std::to_string(level)+" dB, expected "+std::to_string(expected)+")");
        require(mb.activeBandCount()==count,"Wrapper active band count");
    }
    std::cout<<"PASS "<<seen<<" parameters at version 31, last; ordered six-band crossovers; state restore; wrapper band 6\n";
}

double cost(int count,bool linked) {
    const double rate=48000;MultibandDynamicsCore core;auto s=six();s.bandCount=count;s.link=linked?1.f:0.f;
    for(auto& v:s.bands){v.threshold=-40;v.ratio=4;v.range=24;v.attack=2;v.release=60;}
    core.setSettings(s);core.prepare(rate);
    const int n=int(rate*4);Audio a{std::vector<float>(size_t(n)),std::vector<float>(size_t(n))};unsigned r=1;
    for(int i=0;i<n;++i){r=r*1664525u+1013904223u;a[0][size_t(i)]=float((double(r>>8)/16777216.-.5)*.5*(1+std::sin(i*.0003)));r=r*1664525u+1013904223u;a[1][size_t(i)]=float((double(r>>8)/16777216.-.5)*.3);}
    juce::ScopedNoDenormals noDenormals;
    const auto start=std::chrono::steady_clock::now();
    for(int off=0;off<n;off+=256){float* p[2]{a[0].data()+off,a[1].data()+off};core.process(p,2,std::min(256,n-off));}
    return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/4.*100.;
}
void performance() {
    double four=1e9,sixBands=1e9,unlinked=1e9;
    for(int repeat=0;repeat<3;++repeat){four=std::min(four,cost(4,true));sixBands=std::min(sixBands,cost(6,true));unlinked=std::min(unlinked,cost(6,false));}
    std::cout<<"MEASURE 48 kHz stereo, compressing all bands: 4 bands "<<four<<" %, 6 bands "<<sixBands<<" % (unlinked "<<unlinked<<" %) of one core\n";
    require(sixBands<4*four,"Six bands cost more than four times four bands");
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try{fourBandsIgnoreTheNewSettings();reconstruction();weightedMagnitude();equalUpperBandsMatchFour();bandIsolationSoloAndKey();automationStress();bandCountSwitch();crossoverSafety();parametersStateAndProcessor();performance();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
    return 0;
}

#include "PrecisionTuner.h"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace spectralforge::tuning;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    double worst=0;int cases=0;std::mt19937 rng(12345);std::normal_distribution<double> noise(0,1);
    for(double rate:{22050.,24000.,24500.})for(double note:{16.352,23.125,27.5,30.8677,41.2034,55.,82.4069,110.,220.,440.,880.,1318.51,1975.53})for(double cents:{-25.,-.1,0.,.1,25.})for(int kind=0;kind<3;++kind){
        const double hz=note*std::exp2(cents/1200);if(hz>2000)continue;
        std::array<float,8192> samples{};
        for(size_t n=0;n<samples.size();++n){const double phase=6.283185307179586*hz*n/rate+.7;
            samples[n]=float(.2*std::sin(phase)+(kind? .1*std::sin(2*phase)+.07*std::sin(3*phase):0)+(kind==2? .0002*noise(rng):0));}
        const auto result=estimate(samples.data(),int(samples.size()),rate);const double error=1200*std::log2(result.first/hz);
        require(result.second>.9,"Lost tonal lock");const double limit=kind==2?.3:.1;
        if(std::abs(error)>limit){std::cerr<<"FAIL "<<rate<<" Hz rate / "<<hz<<" Hz tone / kind "<<kind<<" / "<<error<<" ct\n";return 1;}
        worst=std::max(worst,std::abs(error));++cases;
    }
    std::array<float,8192> bad{};require(estimate(bad.data(),8192,24000).first==0,"Silence lock");bad.fill(.2f);require(estimate(bad.data(),8192,24000).first==0,"DC lock");for(auto& s:bad)s=float(noise(rng)*.01);require(estimate(bad.data(),8192,24000).first==0,"Noise lock");
    Settings s;s.profile=Profile::tt8;
    for(int i=0;i<8;++i){const auto t=target(frequency(openMidi[size_t(i)],440,ttOpen[size_t(i)]),s);require(t.valid&&t.stringIndex==i&&std::abs(t.cents)<1e-9,"TT target mismatch");}
    require(target(frequency(64,440),s).offset==-1&&target(frequency(40,440),s).offset==-2,"TT E octaves collapsed");
    s.profile=Profile::feitenElectric;require(target(frequency(55,440,-2),s).cents==0,"Feiten open mismatch");s.profile=Profile::feitenElectric12;require(std::abs(target(frequency(67,440,1),s).cents)<1e-9,"Feiten intonation mismatch");
    s.profile=Profile::custom;s.reference=442;s.stringIndex=7;s.customMidi[7]=33;s.customCents[7]=-7.3;
    require(std::abs(target(frequency(33,442,-7.3),s).cents)<1e-9,"Custom target/calibration mismatch");
    require(advanceStrobe(.4,440,440,.1)==.4,"Strobe drifts in tune");require(advanceStrobe(.4,440.1,440,.1)>.4,"Sharp direction");require(advanceStrobe(.4,439.9,440,.1)<.4,"Flat direction");require(advanceStrobe(.4,440.001,440,.1)>.4,"Strobe dead band");
    std::cout<<"PASS "<<cases<<" pitch cases; worst absolute error "<<worst<<" ct; profiles, custom/calibration, silence/DC/noise, strobe direction/dead-band\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

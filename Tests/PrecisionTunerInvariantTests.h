#pragma once
#include "PrecisionTuner.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
// Byte-exact Source/PrecisionTuner.h from 52f7d19e772e6f3321d47c4b46d22b1f7f1946a9.
// Git blob d77eca22470cf8ba16844a26d8a090c472e9c7a4. Rename only at preprocessing.
#define spectralforge spectralforge_tuner52f7
#include "reference/PrecisionTuner52f7.h"
#undef spectralforge

namespace spectralforge::tuning::tests {
inline int frozenComparisons=0;
inline void compareFrozen(const std::pair<float,float>& actual,const float* x,int count,double rate){
    const auto expected=spectralforge_tuner52f7::tuning::estimate(x,count,rate);
    if(!std::isfinite(actual.first) || !std::isfinite(actual.second)
       || std::memcmp(&actual.first,&expected.first,sizeof(float))!=0
       || std::memcmp(&actual.second,&expected.second,sizeof(float))!=0){
        std::cerr<<"Frozen estimator mismatch: count="<<count<<" rate="<<rate
                 <<" expected="<<expected.first<<'/'<<expected.second
                 <<" actual="<<actual.first<<'/'<<actual.second<<'\n';
        throw std::runtime_error("Tuner frequency/confidence changed against frozen 52f7 estimator");
    }
    ++frozenComparisons;
}
inline void runAdditionalInvariantCases(){
    // Some short/low-tone fixtures cannot acquire. Equality, not new accuracy
    // acceptance, is the contract here. The original tonal gates stay separate.
    std::array<float,8192> samples{};
    for(double rate:{22050.,24000.,24500.})for(int count:{512,1024,2048,4096,8192})
    for(double hz:{16.352,27.5,30.8677,82.4069,110.,440.,1975.53})for(int kind=0;kind<4;++kind){
        for(int n=0;n<count;++n){
            const double t=n/rate,p=6.283185307179586*hz*t+.123*(kind+1);
            const double value=kind==0?.2*std::sin(p)
                :kind==1?.015*std::sin(p)+.2*std::sin(2*p)
                :kind==2?(.2*std::sin(p)+.1*std::sin(2*p))*std::exp(-6*t)+.17
                :.13*std::sin(p)+.04*std::sin(3*p)+(n<32?.15*std::exp(-n/8.):0.);
            samples[size_t(n)]=float(value);
        }
        compareFrozen(estimate(samples.data(),count,rate),samples.data(),count,rate);
    }
    // Exercise the published-float and 16/2000-Hz range boundaries explicitly.
    const double floatMid440=(double(440.f)+double(std::nextafter(440.f,441.f)))*.5;
    const double floatMidBass=(double(27.5f)+double(std::nextafter(27.5f,28.f)))*.5;
    for(double rate:{22050.,24000.,24500.})
    for(double hz:{std::nextafter(16.,0.),16.,std::nextafter(16.,32.),double(std::nextafter(16.f,17.f)),
                   1999.9999,2000.,std::nextafter(2000.,3000.),floatMid440,floatMidBass})
    for(double phase:{0.,.7,2.1}){
        for(int n=0;n<4096;++n)samples[size_t(n)]=float(.2*std::sin(6.283185307179586*hz*n/rate+phase));
        compareFrozen(estimate(samples.data(),4096,rate),samples.data(),4096,rate);
    }
    const auto check=[&](const float* p,int count,double rate){compareFrozen(estimate(p,count,rate),p,count,rate);};
    check(nullptr,4096,24000);check(samples.data(),63,24000);check(samples.data(),8193,24000);
    check(samples.data(),4096,3999);check(samples.data(),4096,std::numeric_limits<double>::quiet_NaN());
    check(samples.data(),4096,std::numeric_limits<double>::infinity());
    samples.fill(0);check(samples.data(),4096,24000);
    samples.fill(.2f);check(samples.data(),4096,24000);
    samples[100]=std::numeric_limits<float>::quiet_NaN();check(samples.data(),4096,24000);
    samples[100]=std::numeric_limits<float>::infinity();check(samples.data(),4096,24000);
    std::cout<<"PASS "<<frozenComparisons<<" exact frequency/confidence comparisons against frozen 52f7 estimator\n";
}
}

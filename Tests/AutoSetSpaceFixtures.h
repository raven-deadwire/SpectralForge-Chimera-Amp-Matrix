#pragma once
#include "AutoSetSpace.h"
#include <string>

// Deterministic mono synthesis only; these are not recorded instrument DIs.
// The score BPM and event list are independent ground truth, never inferred
// from the analyser's output. All fixtures last six seconds.
namespace spaceFixtures {
struct Note {double time, amplitude, duration;};
struct Fixture {std::string name;double bpm;std::vector<Note> notes;};
inline std::vector<Fixture> scores() {
    std::vector<Fixture> out;
    for(double bpm:{90.,120.,150.}) {
        const double beat=60./bpm;
        for(const std::string kind:{"eighths","sixteenths","triplets","shuffle","syncopation","long-rests","uneven-picking","sustain"}) {
            Fixture f{kind,bpm,{}};
            for(int bar=0;bar<5;++bar) {
                std::vector<double> positions;
                if(kind=="syncopation")positions={0,.75,1.5,2.5,3.25};
                else if(kind=="sustain")positions={0,2};
                else if(kind=="shuffle")positions={0,2./3,1,1+2./3,2,2+2./3,3,3+2./3};
                else {const double step=kind=="triplets"?1./3:(kind=="sixteenths" || kind=="long-rests"?.25:.5);
                    for(int k=0;double(k)*step<4.-1e-8;++k)positions.push_back(k*step);}
                for(double pos:positions) {
                    const double t=.1+(4*bar+pos)*beat;
                    if(t>=5.85 || (kind=="long-rests" && t>1.25 && t<4.75))continue;
                    const size_t n=f.notes.size();
                    const double amp=kind=="uneven-picking"?std::array<double,5>{.3,.035,.12,.06,.22}[n%5]:.25;
                    f.notes.push_back({t,amp,kind=="sustain"?.9:.07});
                }
            }
            out.push_back(std::move(f));
        }
    }
    return out;
}
inline std::vector<float> render(const Fixture& f,double rate) {
    std::vector<float> x(size_t(6*rate));
    // Explicit integer PRNG avoids implementation-dependent distributions.
    uint32_t seed=239;
    for(auto& sample:x){seed=1664525u*seed+1013904223u;sample=float((double(seed)/4294967296.-.5)*.000002);}
    for(const auto& note:f.notes) {
        const size_t start=size_t(std::llround(note.time*rate));
        for(size_t i=0;start+i<x.size() && double(i)/rate<note.duration;++i) {
            const double t=double(i)/rate;
            const double envelope=std::min(1.,t/.001)*std::exp(-5.*t/note.duration);
            constexpr double pi=3.14159265358979323846;
            x[start+i]+=float(note.amplitude*envelope*(std::sin(2*pi*196*t)+.2*std::sin(2*pi*392*t)));
        }
    }
    return x;
}
inline std::vector<spectralforge::autoset::Window> capture(const std::vector<float>& x,double rate,int block=256) {
    using namespace spectralforge::autoset;
    auto c=std::make_unique<Capture>();c->prepare(rate);c->listen(Tap::input);
    std::vector<Window> w;
    for(size_t n=0;n<x.size();n+=size_t(block)) {
        const float* p[]{x.data()+n};c->push(Tap::input,p,1,int(std::min(size_t(block),x.size()-n)));c->drain(Tap::input,w);
    }
    return w;
}
}

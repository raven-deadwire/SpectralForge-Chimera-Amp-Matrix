// Offline measurement adapter for the production header. No alternate DSP.
// Binary output: little/native-endian float samples (runner records platform).
#include "MultibandDynamicsCore.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace spectralforge;
int main(int argc, char** argv) {
    if (argc < 3) return 2;
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    const std::string mode = argv[1];
    const double sr = std::stod(argv[2]);
    MultibandSettings s; s.on = true;
    for (auto& b : s.bands) b.ratio = 1.f;
    MultibandDynamicsCore core;
    const auto output = [](const std::vector<float>& v) {
        std::cout.write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size()*sizeof(float)));
    };
    if (mode == "impulse" && argc == 11) {
        for (int i=0;i<3;++i) s.crossovers[size_t(i)] = std::stof(argv[3+i]);
        s.mix = std::stof(argv[6]);
        for (int i=0;i<4;++i) s.bands[size_t(i)].makeup = std::stof(argv[7+i]);
        core.setSettings(s); core.prepare(sr);
        std::vector<float> x(65536); x[0]=1.f;
        float* p[]{x.data()}; core.process(p,1,int(x.size())); output(x);
    } else if ((mode == "off" || mode == "on" || mode == "band_off") && argc == 4) {
        const double hz = std::stod(argv[3]);
        const int warm = int(sr * .5), length = int(sr * .04);
        // Two independent mono runs in quadrature expose the instantaneous
        // envelope of the *linear neutral* time-varying bypass transition.
        for (int quadrature=0;quadrature<2;++quadrature) {
            s.on = mode != "on";
            if(mode == "band_off") for(auto& b:s.bands) {b.makeup=-6; b.bypass=false;}
            core.setSettings(s);core.prepare(sr);
            std::vector<float> x(size_t(warm+length));
            for (int n=0;n<warm+length;++n) x[size_t(n)] = float(.25 * std::sin(2.*multiband_detail::pi*hz*n/sr + quadrature*multiband_detail::pi*.5));
            float* p[]{x.data()};core.process(p,1,warm);
            if(mode == "band_off") for(auto& b:s.bands)b.bypass=true;
            else s.on = mode == "on";
            core.setSettings(s);p[0]+=warm;core.process(p,1,length);
            output(std::vector<float>(x.begin()+warm,x.end()));
        }
    } else if (mode == "timing" && argc == 4) {
        s.detector=std::stoi(argv[3]);s.crossovers={80,400,1200};
        for(auto& b:s.bands)b.bypass=true;
        auto& b=s.bands[3];b.bypass=false;b.threshold=-24;b.ratio=4;
        b.range=36;b.knee=0;b.attack=10;b.release=120;
        core.setSettings(s);core.prepare(sr);
        std::vector<float> gain(size_t(sr*2.5));
        for(size_t n=0;n<gain.size();++n) {
            const double db = (n>=size_t(sr*.5) && n<size_t(sr*1.5)) ? -6. : -42.;
            float x=float(std::pow(10.,db/20)*std::sin(2.*multiband_detail::pi*6000*n/sr));
            float* p[]{&x};core.process(p,1,1);gain[n]=core.reduction(3);
        }
        output(gain);
    } else return 2;
    return std::cout.good()?0:3;
}

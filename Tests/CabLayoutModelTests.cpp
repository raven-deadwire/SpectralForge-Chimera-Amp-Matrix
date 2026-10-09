#include "CabLayoutModel.h"
#include <iostream>
#include <set>
namespace l=spectralforge::cabLayout;
namespace x=spectralforge::cabExpansion;
namespace v=spectralforge::originalCab;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double energy(const std::vector<float>& a){double e=0;for(float f:a){require(std::isfinite(f),"finite kernel");e+=double(f)*f;}return e;}
int main(){try {
    l::Settings p{{{true,0,0,0,0,0,.63,10},1,20,0},0,7};
    for(int driver:{0,1,14})for(double rate:{44100.,48000.,96000.}) {
        p.voice.driver=driver;require(l::key(p)==x::key(p.voice),"legacy request changed");
        require(l::generate(l::key(p),rate)==x::generate(x::key(p.voice),rate),"v1/v2 kernel changed");
    }
    const std::array<int,9> counts{1,2,4,1,2,4,8,1,2};
    const std::array<int,9> drivers{1,1,1,13,9,9,9,11,14};
    double largest=0;size_t cases=0;std::set<uint64_t> keys;
    for(int layout=1;layout<=9;++layout) {
        p.layout=layout;p.voice.driver=drivers[size_t(layout-1)];const auto g=l::geometry(p);
        require(g.count==counts[size_t(layout-1)] && g.box.volume>0,"authored count / volume");
        require(g.box.volume<g.box.width*g.box.height*g.box.depth,"net volume exceeds exterior");
        for(int unit=0;unit<g.count;++unit) {
            p.unit=unit;require(keys.insert(l::key(p)).second,"request collision");
            require(l::settings(l::key(p)).unit==unit && l::key(l::settings(l::key(p)))==l::key(p),"array index roundtrip");
            const auto c=g.centres[size_t(unit)];
            require(std::abs(c.x)+g.radius<g.box.width/2 && std::abs(c.y)+g.radius<g.box.height/2,"cone outside box");
            for(int j=0;j<unit;++j)require(std::hypot(c.x-g.centres[size_t(j)].x,c.y-g.centres[size_t(j)].y)>2*g.radius,"overlapping cones");
        }
        p.unit=7;require(l::settings(l::key(p)).unit==g.count-1,"invalid target not clamped");
        p.unit=0;
        // Production field equals the coherent ray sum, not incoherent magnitudes.
        const auto* d=x::driver(p.voice);const auto* m=x::microphone(p.voice);
        const v::Point pickup{g.centres[0].x+p.voice.base.position*g.radius,g.centres[0].y};
        double minimumCoherence=1;
        for(double hz:{180.,400.,800.,1600.,3200.,6400.}) {
            v::Complex field{};double incoherent=0;
            for(int n=0;n<g.count;++n){auto ray=x::coneField(hz,d->speaker,g.centres[size_t(n)],pickup,.1,d->coherenceHz,m);field+=ray;incoherent+=std::abs(ray);}
            const v::Complex s{0,2*v::pi*hz};
            auto expected=.45*l::driverTransfer(p,hz,g)*field*v::microphoneResponse(s,m->response,.1)*(1.+m->presenceAmount*v::mode(s,m->presenceHz,m->presenceQ));
            require(std::abs(expected-l::response(p,hz))<1e-12,"array field mismatch");
            minimumCoherence=std::min(minimumCoherence,std::abs(field)/incoherent);
        }
        if(g.count>1)require(minimumCoherence<.98,"multi-driver phase interference absent");
        auto bigger=g;bigger.box.volume*=2;
        require(std::abs(l::driverTransfer(p,65,g)-l::driverTransfer(p,65,bigger))>.01,"net volume does not load speaker");
        p.voice.base.rear=1;require(l::driverTransfer(p,65,g)==l::driverTransfer(p,65,bigger),"open rear retains sealed compliance");p.voice.base.rear=0;
        for(double rate:{44100.,48000.,96000.}) {
            const auto wave=l::generate(l::key(p),rate);const double total=energy(wave);
            require(wave.size()==size_t(std::ceil(rate*.085)) && total>1e-6,"kernel length / energy");
            double tail=0;for(size_t n=wave.size()*9/10;n<wave.size();++n)tail+=double(wave[n])*wave[n];
            require(tail/total<.001,"v3 tail gate");
            auto farther=p;farther.voice.base.distanceCm=60;require(energy(l::generate(l::key(farther),rate))<total,"distance attenuation");
        }
    }
    for(int layout=1;layout<=9;++layout)for(int d=1;d<=14;++d)for(int mic=1;mic<=20;++mic)
    for(int unit=0;unit<l::count(layout);++unit)for(int rear=0;rear<2;++rear)for(double position:{0.,1.})for(double distance:{2.,60.}) {
        p={{{true,0,rear,0,0,1,position,distance},d,mic,3},layout,unit};
        for(double hz:{30.,300.,3000.,15000.}) {
            auto r=l::response(p,hz);require(std::isfinite(r.real()) && std::isfinite(r.imag()),"finite array boundary");largest=std::max(largest,std::abs(r));
        }++cases;
    }
    require(largest<8,"array magnitude gate");
    double positionStep=0,distanceStep=0;
    for(int layout=1;layout<=9;++layout)for(int mic=1;mic<=20;++mic)for(double distance:{2.,10.,59.9}) {
        p={{{true,0,0,0,0,.5,.63,distance},drivers[size_t(layout-1)],mic,2},layout,l::count(layout)-1};
        auto a=p,b=p;a.voice.base.position+=.001;b.voice.base.distanceCm+=.1;
        for(double hz:{80.,400.,1500.,5000.,9000.}) {
            positionStep=std::max(positionStep,std::abs(l::response(p,hz)-l::response(a,hz)));
            distanceStep=std::max(distanceStep,std::abs(l::response(p,hz)-l::response(b,hz)));
        }
    }
    require(positionStep<.025 && distanceStep<.15,"array control continuity gate");
    std::cout<<"PASS layouts=9 all_driver_mic_unit_boundaries="<<cases<<" max_magnitude="<<largest<<" position_tick="<<positionStep<<" distance_tick="<<distanceStep<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

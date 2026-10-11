#include "OriginalCabModel.h"
#include "CabQuadratureReference.h"
#include <iostream>
#include <chrono>
using namespace spectralforge::originalCab;
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
double energy(const std::vector<float>& a){double e=0;for(auto v:a)e+=v*v;return e;}
double delta(const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(double(a[n]-b[n])));return d;}
double peakTime(const std::vector<float>& a,double sr){return std::distance(a.begin(),std::max_element(a.begin(),a.end(),[](float x,float y){return std::abs(x)<std::abs(y);}))/sr;}
void quadratureReference() {
    namespace reference=spectralforge::cabQuadratureReference;
    size_t geometry=0,cases=0;
    for(int ring=0;ring<4;++ring)for(int sector=0;sector<12;++sector) {
        const double theta=2*pi*(sector+.5*(ring%2))/12;
        const auto& point=coneQuadraturePoints()[size_t(ring*12+sector)];
        require(reference::sameBits(point.radiusScale,std::sqrt((ring+.5)/4)),"cached cone radius bits differ");
        require(reference::sameBits(point.cosTheta,std::cos(theta)),"cached cone cosine bits differ");
        require(reference::sameBits(point.sinTheta,std::sin(theta)),"cached cone sine bits differ");
        geometry+=3;
    }
    for(size_t cabinet=0;cabinet<speakers.size();++cabinet) {
        const auto& speaker=speakers[cabinet];const auto points=centres(enclosures[cabinet]);
        for(const auto source:points)for(int unit:{0,3})for(double position:{0.,.63,1.})
        for(double z:{.02,.10,.60})for(double hz:{20.,80.,1000.,1700.,6000.,20000.}) {
            const Point pickup{points[size_t(unit)].x+position*speaker.radius,points[size_t(unit)].y};
            require(reference::sameBits(coneField(hz,speaker,source,pickup,z),
                reference::originalConeField(hz,speaker,source,pickup,z)),"cached v1 cone differs from original math");
            ++cases;
        }
    }
    std::cout<<"PASS cone_quadrature_reference_v1="<<cases<<" geometry_doubles="<<geometry<<" exact_double_bits\n";
}
int main(){try {
    quadratureReference();
    // Frozen authored v1 transfer anchors (not physical measurements).
    const std::array<Complex,6> golden{{{-0.428362432676646,0.201748237235966},{-0.199687868229497,-0.266552891708711},{-0.0374053842063739,-0.149089915074266},{-0.376149775579828,0.711919460466929},{-0.129113878143088,-0.235379002422618},{-0.020334227310253,-0.120790512406904}}};
    int anchor=0;for(int cab=0;cab<2;++cab)for(double hz:{80.,1000.,6000.}) {
        Settings p{true,cab,0,0,0,0,.25,10};require(std::abs(response(p,hz)-golden[size_t(anchor++)])<1e-10,"frozen generator v1 changed");
    }
    size_t cases=0;double largest=0;
    for(int cab=0;cab<2;++cab)for(int mic=0;mic<3;++mic)for(int unit=0;unit<4;++unit)
    for(int rear=0;rear<2;++rear)for(double pos:{0.,.5,1.})for(double distance:{2.,10.,60.})for(double tweeter:{0.,1.}) {
        Settings p{true,cab,rear,mic,unit,tweeter,pos,distance};require(key(settings(key(p)))==key(p),"key reproducibility");
        for(double hz:{20.,31.,60.,120.,300.,1000.,3000.,6000.,12000.,20000.}) {
            const auto h=response(p,hz);require(std::isfinite(h.real()) && std::isfinite(h.imag()),"finite boundary response");
            largest=std::max(largest,std::abs(h));require(std::abs(h)<8,"bounded amplitude");
        }
        ++cases;
    }
    for(double sr:{44100.,48000.,96000.})for(int cab=0;cab<2;++cab) {
        Settings p{true,cab,0,0,0,0,.25,10};const auto started=std::chrono::steady_clock::now();
        auto reference=generate(p,sr);require(energy(reference)>1e-6,"non-silent kernel");
        require(reference==generate(p,sr),"deterministic generation");
        double tail=0;for(size_t n=reference.size()*9/10;n<reference.size();++n)tail+=reference[n]*reference[n];
        require(tail/energy(reference)<1e-3,"85ms tail decay");
        p.position=1;require(delta(reference,generate(p,sr))>1e-4,"position changes waveform");
        p.position=.25;p.distanceCm=60;auto far=generate(p,sr);
        require(energy(far)<energy(reference),"natural distance attenuation survives generation");
        require(peakTime(far,sr)-peakTime(reference,sr)>.0008,"distance changes arrival time, not only EQ");
        p.distanceCm=10;p.rear=1;require(delta(reference,generate(p,sr))>1e-4,"rear radiation changes waveform");
        p.rear=0;p.tweeter=1;require(delta(reference,generate(p,sr))>1e-4,"independent tweeter");
        p.tweeter=0;p.unit=1;require(delta(reference,generate(p,sr))>1e-5,"unit geometry changes waveform");
        p.unit=0;for(int mic=1;mic<3;++mic){p.mic=mic;require(delta(reference,generate(p,sr))>1e-4,"distinct microphone");}
        std::cout<<"PASS rate="<<sr<<" cabinet="<<cab<<" energy="<<energy(reference)<<" far_ratio="<<energy(far)/energy(reference)
                 <<" tail_ratio="<<tail/energy(reference)<<" generation_suite_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';
    }
    // Continuity thresholds declared on complex response; test every control tick.
    Settings p{true,0,0,1,0,0,.25,10};double worst=0;
    for(int tick=1;tick<=1000;++tick)for(double hz:{80.,400.,1500.,5000.,9000.}) {
        p.position=(tick-1)*.001;const auto a=response(p,hz);p.position=tick*.001;const auto b=response(p,hz);
        worst=std::max(worst,std::abs(a-b));
    }
    require(worst<.025,"continuous position response");
    double distanceWorst=0;p.position=.25;
    for(int tick=21;tick<=600;++tick)for(double hz:{80.,400.,1500.,5000.,9000.}) {
        p.distanceCm=(tick-1)*.1;const auto a=response(p,hz);p.distanceCm=tick*.1;const auto b=response(p,hz);distanceWorst=std::max(distanceWorst,std::abs(a-b));
    }
    require(distanceWorst<.15,"continuous distance response");
    std::cout<<"PASS distance_tick_delta="<<distanceWorst<<'\n';
    std::cout<<"PASS boundary_cases="<<cases<<" maximum_magnitude="<<largest<<" position_tick_delta="<<worst<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

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
    for(int family:{0,1})for(int driver:{0,1,9,10,12})for(double rate:{44100.,48000.,96000.}) {
        p.voice.base.cabinet=family;p.voice.driver=driver;
        if(!l::isDriverCompatible(p))continue;
        require(l::key(p)==x::key(p.voice),"valid four-unit request changed");
        require(l::generate(l::key(p),rate)==x::generate(x::key(p.voice),rate),"v1/v2 kernel changed");
    }
    for(int family:{0,1})for(int driver=0;driver<=14;++driver) {
        p.voice.base.cabinet=family;p.voice.driver=driver;const auto effective=l::effectiveSettings(p);
        const int fallback=family ? 9 : 1,expectedInches=family ? 10 : 12;
        const bool compatible=!driver || (x::drivers[size_t(driver-1)].bass==(family!=0) && x::drivers[size_t(driver-1)].inches==expectedInches);
        require(l::isDriverCompatible(p)==compatible && l::isDriverCompatible(effective),"four-unit family / diameter mismatch accepted");
        require(effective.voice.driver==(compatible ? driver : fallback),"four-unit wrong fallback");
        require(l::isBass(effective)==(family!=0) && l::nominalInches(effective)==expectedInches,"four-unit driver changed cabinet family");
        const auto g=l::geometry(p);const auto& authored=v::enclosures[size_t(family)];
        require(g.box.width==authored.width && g.box.height==authored.height && g.box.depth==authored.depth && g.box.volume==authored.volume,"four-unit mismatch resized cabinet");
        require(l::key(p)==x::key(effective.voice) && l::settings(x::key(p.voice)).voice.driver==effective.voice.driver,"four-unit mismatch bypasses key resolution");
        for(double hz:{65.,1500.,6500.})require(l::response(p,hz)==l::response(effective,hz),"four-unit automation changed cabinet family");
        if(!compatible && (driver==1 || driver==9 || driver==13))
            require(l::generate(x::key(p.voice),48000)==x::generate(x::key(effective.voice),48000),"restored four-unit kernel retains incompatible driver");
    }
    p.voice.base.cabinet=0;
    const std::array<int,9> counts{1,2,4,1,2,4,6,1,2};
    const std::array<int,9> drivers{1,1,1,13,9,9,9,11,14};
    double largest=0;size_t cases=0,compatibilityCases=0;std::set<uint64_t> keys;
    for(int layout=1;layout<=9;++layout) {
        p.layout=layout;p.voice.driver=drivers[size_t(layout-1)];const auto g=l::geometry(p);
        const auto& authored=l::layouts[size_t(layout-1)];
        require(l::isDriverCompatible(p) && l::nominalInches(p)==authored.inches && l::isBass(p)==authored.bass,"authored driver classification");
        require(g.count==counts[size_t(layout-1)] && g.box.volume>0,"authored count / volume");
        require(g.box.width==authored.width && g.box.height==authored.height && g.box.depth==authored.depth && g.box.volume==authored.volume,"authored dimensions changed");
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
    // The 1.3 large bass cabinet reuses the preview driver model while removing
    // one row. Retain per-driver sealed loading, but use six actual sources and
    // the shorter cabinet's axial mode, not a relabelled eight-driver response.
    p={{{true,1,0,0,0,0,.63,10},9,20,0},7,0};
    const auto six=l::geometry(p);
    require(six.count==6 && std::string(l::layouts[6].id)=="b610-v3", "6x10 layout identity");
    require(l::layouts[6].columns==2 && l::layouts[6].rows==3, "6x10 baffle rows");
    require(six.box.width==.63 && six.box.height==.94 && six.box.depth==.40, "6x10 enclosure dimensions");
    auto eight=six;eight.count=8;eight.box.height=1.22;eight.box.volume=.238;
    require(std::abs(six.box.volume-eight.box.volume*6./8.)<1e-12, "6x10 per-driver volume inheritance");
    require(std::abs(six.count*six.box.vasPerDriver/six.box.volume-eight.count*eight.box.vasPerDriver/eight.box.volume)<1e-12,
        "6x10 sealed compliance changed without a new driver model");
    const double pitch=10*.0254*1.10;
    for(int row=0;row<4;++row)for(int col=0;col<2;++col)
        eight.centres[size_t(row*2+col)]={(col-.5)*pitch,(1.5-row)*pitch};
    const auto* inheritedDriver=x::driver(p.voice);const auto* inheritedMic=x::microphone(p.voice);
    double sourceChange=0,modeChange=0;
    for(double hz:{80.,180.,400.,800.,1600.,3200.}) {
        const v::Point pickup{six.centres[0].x+p.voice.base.position*six.radius,six.centres[0].y};
        v::Complex sixField{},eightField{};
        for(int n=0;n<six.count;++n)sixField+=x::coneField(hz,inheritedDriver->speaker,six.centres[size_t(n)],pickup,.1,inheritedDriver->coherenceHz,inheritedMic);
        for(int n=0;n<eight.count;++n)eightField+=x::coneField(hz,inheritedDriver->speaker,eight.centres[size_t(n)],pickup,.1,inheritedDriver->coherenceHz,inheritedMic);
        sourceChange=std::max(sourceChange,std::abs(sixField-eightField));
        modeChange=std::max(modeChange,std::abs(l::driverTransfer(p,hz,six)-l::driverTransfer(p,hz,eight)));
    }
    require(sourceChange>.01 && modeChange>.001, "6x10 retained eight-driver field or tall enclosure modes");
    for(int rawUnit=0;rawUnit<8;++rawUnit) {
        p.unit=rawUnit;const int expectedUnit=rawUnit<6 ? rawUnit : rawUnit-2;
        const auto before=p;auto resolved=p;resolved.unit=expectedUnit;
        require(l::effectiveUnit(p)==expectedUnit && l::settings(l::key(p)).unit==expectedUnit, "6x10 preview target migration");
        require(p.unit==before.unit, "6x10 migration wrote raw state");
        require(l::key(p)==l::key(resolved), "6x10 visual and DSP target keys differ");
        for(double hz:{65.,1500.,6500.})require(l::response(p,hz)==l::response(resolved,hz), "6x10 migrated microphone changes column");
    }
    // Saved values and automation can bypass the UI's compatible-driver menu.
    // Every such request resolves to a matching driver while keeping raw settings
    // untouched, fixed cabinet dimensions, and one canonical rendering/DSP key.
    for(int layout=1;layout<=9;++layout)for(int family=0;family<2;++family)for(int driver=0;driver<=14;++driver) {
        p={{{true,family,0,0,0,.4,.63,10},driver,20,2},layout,0};
        const auto stored=p,effective=l::effectiveSettings(p);const auto& authored=l::layouts[size_t(layout-1)];
        require(p.voice.driver==stored.voice.driver && p.voice.base.cabinet==stored.voice.base.cabinet,"compatibility resolution mutates stored choice");
        require(l::isDriverCompatible(effective),"mismatch has no compatible fallback");
        const bool expected=(driver ? x::drivers[size_t(driver-1)].bass : family!=0)==authored.bass
            && (driver ? x::drivers[size_t(driver-1)].inches : family ? 10 : 12)==authored.inches;
        require(l::isDriverCompatible(p)==expected,"family / diameter acceptance wrong");
        if(expected)require(effective.voice.driver==driver,"compatible selected driver replaced");
        else require(effective.voice.driver==(authored.bass ? authored.inches==15 ? 13 : authored.inches==12 ? 11 : 9 : 1),"fallback is not deterministic");
        const auto g=l::geometry(p),resolved=l::geometry(effective);
        require(g.box.width==authored.width && g.box.height==authored.height && g.box.depth==authored.depth && g.box.volume==authored.volume,"driver resized cabinet");
        require(g.radius==resolved.radius && l::key(p)==l::key(effective),"effective visual and DSP driver disagree");
        const auto rawKey=x::key(p.voice)|l::versionBit|(uint64_t(layout)<<47);
        require(l::settings(rawKey).voice.driver==effective.voice.driver,"restored request bypasses compatibility");
        for(double hz:{65.,1500.,6500.})require(l::response(p,hz)==l::response(effective,hz),"automated mismatch bypasses DSP compatibility");
        ++compatibilityCases;
    }
    for(int layout=1;layout<=9;++layout)for(int d=1;d<=14;++d)for(int mic=1;mic<=20;++mic)
    for(int unit=0;unit<l::count(layout);++unit)for(int rear=0;rear<2;++rear)for(double position:{0.,1.})for(double distance:{2.,60.}) {
        p={{{true,0,rear,0,0,1,position,distance},d,mic,3},layout,unit};
        if(!l::isDriverCompatible(p))continue;
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
    std::cout<<"PASS layouts=9 compatibility_requests="<<compatibilityCases<<" compatible_driver_mic_unit_boundaries="<<cases<<" max_magnitude="<<largest<<" position_tick="<<positionStep<<" distance_tick="<<distanceStep<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

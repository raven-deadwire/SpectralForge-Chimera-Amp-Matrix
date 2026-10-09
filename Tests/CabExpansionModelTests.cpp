#include "CabExpansionModel.h"
#include <iostream>
#include <set>
#include <string>
namespace x=spectralforge::cabExpansion;
namespace v1=spectralforge::originalCab;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
double energy(const std::vector<float>& a){double e=0;for(auto y:a)e+=double(y)*y;return e;}
double difference(const std::vector<float>& a,const std::vector<float>& b){double e=0;for(size_t n=0;n<a.size();++n)e=std::max(e,std::abs(double(a[n]-b[n])));return e;}
std::array<double,12> signature(x::Settings p) {
    std::array<double,12> s{};int i=0;
    for(double f:{40.,80.,160.,320.,640.,1250.,2500.,4000.,6000.,8000.,12000.,18000.})
        s[size_t(i++)]=20*std::log10(std::max(1e-12,std::abs(x::response(p,f))));
    const auto level=s[5];for(auto& v:s)v-=level;return s;
}
template<size_t N> void distinct(const std::array<std::array<double,12>,N>& signatures,const char* why) {
    double smallest=100;
    for(size_t a=0;a<N;++a)for(size_t b=0;b<a;++b) {
        double d=0;for(size_t n=0;n<12;++n)d+=std::pow(signatures[a][n]-signatures[b][n],2);
        smallest=std::min(smallest,std::sqrt(d/12));
    }
    require(smallest>.15,why);std::cout<<why<<" minimum_level_matched_spectral_rms_db="<<smallest<<'\n';
}
int main(){try {
    std::set<std::string> ids;int guitar=0,bass=0;std::array<int,3> kinds{};
    for(const auto& d:x::drivers){require(ids.insert(d.id).second,"duplicate driver ID");(d.bass?bass:guitar)++;}
    for(const auto& m:x::microphones){require(ids.insert(m.id).second,"duplicate mic ID");++kinds[size_t(m.kind)];}
    require(guitar==8 && bass==6 && kinds==std::array<int,3>{9,3,8},"expanded inventory");
    x::Settings p{{true,0,0,0,0,0,.25,10},0,0,0};
    for(double rate:{44100.,48000.,96000.}) {
        require(x::key(p)==v1::key(p.base),"legacy packed key changed");
        require(x::generate(x::key(p),rate)==v1::generate(p.base,rate),"legacy kernel must be bit-identical");
    }
    p.driver=1;p.mic=1;
    std::array<std::array<double,12>,14> driverSignatures{};
    for(int i=1;i<=14;++i){p.driver=i;driverSignatures[size_t(i-1)]=signature(p);}
    distinct(driverSignatures,"distinct drivers");p.driver=1;
    std::array<std::array<double,12>,20> micSignatures{};
    for(int i=1;i<=20;++i){p.mic=i;micSignatures[size_t(i-1)]=signature(p);}
    distinct(micSignatures,"distinct microphones");
    size_t cases=0;double largest=0,positionStep=0,distanceStep=0;
    for(int d=1;d<=14;++d)for(int m=1;m<=20;++m)for(int rear=0;rear<2;++rear)
    for(int unit=0;unit<4;++unit)for(double position:{0.,1.})for(double distance:{2.,60.}) {
        p={{true,0,rear,0,unit,1,position,distance},d,m,3};
        require(x::key(x::settings(x::key(p)))==x::key(p),"request roundtrip");
        for(double f:{30.,80.,300.,1000.,3000.,6000.,12000.,20000.}) {
            const auto h=x::response(p,f);require(std::isfinite(h.real()) && std::isfinite(h.imag()),"finite boundaries");
            largest=std::max(largest,std::abs(h));
        }++cases;
    }
    require(largest<8,"bounded expansion response");
    for(int m=1;m<=20;++m)for(int d:{1,6,7,8,9,11,13,14})for(double position:{0.,.25,.75,.999})for(double distance:{2.,10.,59.9}) {
        p={{true,0,0,0,0,.5,position,distance},d,m,2};auto moved=p;moved.base.position+=.001;auto farther=p;farther.base.distanceCm+=.1;
        for(double f:{80.,400.,1500.,5000.,9000.}) {
            positionStep=std::max(positionStep,std::abs(x::response(p,f)-x::response(moved,f)));
            distanceStep=std::max(distanceStep,std::abs(x::response(p,f)-x::response(farther,f)));
        }
    }
    require(positionStep<.025 && distanceStep<.15,"control continuity");
    for(double rate:{44100.,48000.,96000.})for(int d:{1,8,9,13,14}) {
        p={{true,0,0,0,0,.3,.25,10},d,20,1};const auto a=x::generate(x::key(p),rate);
        require(a==x::generate(x::key(p),rate),"deterministic expanded kernel");
        require(energy(a)>1e-6,"non-silent expansion");
        double tail=0;for(size_t n=a.size()*9/10;n<a.size();++n)tail+=double(a[n])*a[n];
        require(tail/energy(a)<.001,"expanded tail decay");
        p.base.distanceCm=60;const auto far=x::generate(x::key(p),rate);require(energy(far)<energy(a),"natural distance attenuation");
        p.base.distanceCm=10;p.base.rear=1;require(difference(a,x::generate(x::key(p),rate))>1e-5,"rear changes expanded audio");
        p.base.rear=0;p.tweeter=3;require(difference(a,x::generate(x::key(p),rate))>1e-5,"tweeter design changes audio");
    }
    // Every selectable mic and driver produces a kernel, not just catalog metadata.
    p={{true,0,0,0,0,0,.25,10},1,1,0};const auto anchor=x::generate(x::key(p),48000);
    for(int d=2;d<=14;++d){p.driver=d;require(difference(anchor,x::generate(x::key(p),48000))>1e-5,"driver kernel duplicates anchor");}
    p.driver=1;for(int m=2;m<=20;++m){p.mic=m;require(difference(anchor,x::generate(x::key(p),48000))>1e-5,"mic kernel duplicates anchor");}
    std::cout<<"PASS drivers="<<guitar<<'+'<<bass<<" mics="<<kinds[0]<<'+'<<kinds[1]<<'+'<<kinds[2]
        <<" boundary_cases="<<cases<<" magnitude="<<largest<<" position_tick="<<positionStep<<" distance_tick="<<distanceStep<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

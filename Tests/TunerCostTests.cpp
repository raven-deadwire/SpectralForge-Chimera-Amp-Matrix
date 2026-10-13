#include "PrecisionTuner.h"
#include "Fixtures/PrecisionTunerPR67.h"
#include "TunerDiagnostics.h"
#include <iostream>
#include <iomanip>
int main(){
    std::cout<<std::setprecision(12)<<"repeat,hz,engine,wall_us,cpu_us,cycles,error_cents\n";int failed=0;
    for(int repeat=0;repeat<10;++repeat)for(double hz:{16.352,27.5,30.867706,82.406889,440.,1318.5102}){
        std::array<float,4096>x{};
        for(size_t i=0;i<x.size();++i){const double phase=6.283185307179586*hz*double(i)/24000+.73;x[i]=float((.2*std::sin(phase)+.1*std::sin(2*phase)+.07*std::sin(3*phase))*std::exp(-double(i)/24000*3));}
        for(int pair=0;pair<2;++pair){const bool candidate=(pair==0)==(repeat%2==0);const auto cpu=tunerDiagnostics::readClock();const double start=tunerDiagnostics::nowUs();
            const auto result=candidate?spectralforge::tuning::estimate(x.data(),4096,24000):baselineTuningPR67::estimate(x.data(),4096,24000);
            const double wall=tunerDiagnostics::nowUs()-start;const auto end=tunerDiagnostics::readClock();const cabTiming::Sample sample{0,wall,cpu,end};
            const double cents=result.first>0?1200*std::log2(result.first/hz):99999;failed+=std::abs(cents)>.1;
            std::cout<<repeat<<','<<hz<<','<<(candidate?"candidate":"pr67")<<','<<wall<<','<<sample.cpuUs()<<','<<sample.cycles()<<','<<cents<<'\n';
        }
    }
    return failed?1:0;
}

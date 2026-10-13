#include "TunerDiagnostics.h"
#include <iostream>
#include <thread>
int main(){
    std::cout<<"kind,index,wall_us,cpu_us,cycles\n";
    for(int i=0;i<8;++i)for(int kind=0;kind<3;++kind){
        const auto cpu=tunerDiagnostics::readClock();const double start=tunerDiagnostics::nowUs();
        if(kind==1)std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if(kind==2){volatile double x=0;while(tunerDiagnostics::nowUs()-start<20000)x=x+1;}
        const double wall=tunerDiagnostics::nowUs()-start;const auto end=tunerDiagnostics::readClock();
        const cabTiming::Sample s{size_t(i),wall,cpu,end};std::cout<<kind<<','<<i<<','<<wall<<','<<s.cpuUs()<<','<<s.cycles()<<'\n';
    }
}

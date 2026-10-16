#include "GlobalDSP.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using Clock=std::chrono::steady_clock;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
double error(double measured,double expected){return measured>0?1200*std::log2(measured/expected):99999;}
float tone(double hz,double time,int){double p=6.283185307179586*hz*time+.73;return float(.2*std::sin(p)+.1*std::sin(2*p)+.07*std::sin(3*p));}
#include "TunerGenerationTests.h"
int main(int argc,char** argv){try{generationTransition(argc>1 && std::string(argv[1])=="resume");return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

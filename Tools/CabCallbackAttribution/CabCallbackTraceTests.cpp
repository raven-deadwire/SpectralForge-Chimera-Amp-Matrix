// Diagnostic trace contract, active in Release builds. No product timing gate.
#include "CabCallbackTrace.h"
#include <stdexcept>
#include <iostream>
using namespace spectralforge::cabTrace;
void require(bool value) {if(!value)throw std::runtime_error("CAB trace contract failed");}
int main() {try {
    const auto clocks=clockSample();
    require(clocks.wallValid && clocks.cpuValid);
    Buffer zero{};
    {CallbackCapture callback(&zero,7);{Scope child(Stage::FFTForward);}callback.stop();require(current==nullptr);}
    require(zero.used==0 && zero.dropped==2 && zero.nestingErrors==0);
    Event events[3]; Buffer bounded{events,3};
    WorkerProgress worker;
    const auto initial=worker.snapshot();require(initial.valid && initial.sequence==0 && initial.lane==-1);
    worker.transition(WorkerState::Build,2,1);
    const auto built=worker.snapshot();require(built.sequence==1 && built.state==WorkerState::Build && built.lane==2 && built.mic==1);
    {
        CallbackCapture callback(&bounded,9,&worker);
        {ContextScope context(Context{2,1,1234,42,Role::Fading});Scope convolution(Stage::FadingConvolution);
            {Scope fft(Stage::FFTForward);{Scope overflow(Stage::SpectrumCopy);}}}
        worker.transition(WorkerState::Wait);
        callback.stop();require(current==nullptr);
        require(callback.workerEntry.sequence==1 && callback.workerExit.sequence==2);
    }
    require(bounded.used==3 && bounded.dropped==1 && bounded.nestingErrors==0 && bounded.clockErrors==0);
    for(std::size_t i=0;i<bounded.used;++i) {
        const auto& e=events[i];require(e.eventId==i+1 && e.complete && e.callbackId==9);
        require(e.end.wallNs>=e.start.wallNs && e.end.cpuNs>=e.start.cpuNs);
        if(i) {const auto& p=events[e.parentId-1];require(e.depth==p.depth+1);require(e.start.wallNs>=p.start.wallNs && e.end.wallNs<=p.end.wallNs);require(e.modelKey==1234 && e.generation==42 && e.role==Role::Fading);}
    }
    Event workerEvents[4];Buffer workerBuffer{workerEvents,4};
    {ThreadCapture capture(&workerBuffer,noCallback,&worker);{ContextScope context(Context{1,0,5678,11,Role::Worker});WorkerScope build(Stage::WorkerBuild,WorkerState::Build);require(worker.snapshot().state==WorkerState::Build);}}
    require(current==nullptr && workerBuffer.used==1 && workerEvents[0].callbackId==noCallback);
    require(worker.snapshot().state==WorkerState::Wait && workerEvents[0].modelKey==5678);
    Event waitEvents[1];Buffer waitBuffer{waitEvents,1};
    {ThreadCapture capture(&waitBuffer);WorkerScope wait(Stage::WorkerWait,WorkerState::Wait);wait.waitResult(20,false);}
    require(waitEvents[0].complete && waitEvents[0].waitRequestedNs==20000000 && waitEvents[0].waitNotified==0);
    std::cout << "TRACE_CONTRACT PASS zero-capacity bounded-overflow parent-depth endpoint-nesting kernel-context worker-packed-state tls-stop sizeof_event=" << sizeof(Event) << '\n';
    return 0;
} catch(const std::exception& error) {std::cerr << error.what() << "\n"; return 1;}
}

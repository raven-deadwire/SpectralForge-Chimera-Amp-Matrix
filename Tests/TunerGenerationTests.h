#pragma once

// Controlled real-worker transitions. No scheduler luck or threshold changes:
// a fresh full window is published while the consumer is deliberately paused.
void generationTransition(bool expiredGap)
{
#if CHIMERA_TUNER_TEST_HOOKS
    spectralforge::Tuner tuner;
    tuner.prepare(48000);
    juce::AudioBuffer<float> window(1,8192);
    const auto fill=[&](double hz) {
        for(int n=0;n<window.getNumSamples();++n)
            window.setSample(0,n,tone(hz,n/48000.,1));
    };
    fill(110);tuner.push(window,true);
    auto deadline=Clock::now()+std::chrono::seconds(1);
    while(tuner.frequency()==0 && Clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(std::abs(error(tuner.frequency(),110))<.1,"Generation fixture did not acquire");
    tuner.pauseWorkerForTest(expiredGap?450:120);
    deadline=Clock::now()+std::chrono::seconds(1);
    while(!tuner.workerPausedForTest() && Clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(tuner.workerPausedForTest(),"Generation worker pause not reached");
    if(expiredGap) {
        std::this_thread::sleep_for(std::chrono::milliseconds(280));
        require(tuner.frequency()==0,"Expired pre-gap pitch remained visible");
    } else {
        tuner.push(window,false);
        require(tuner.frequency()==0,"Disable exposed previous pitch");
    }
    fill(220);
    const auto start=Clock::now();
    tuner.push(window,true);
    require(tuner.frequency()==0,"Generation transition leaked previous pitch");
    double acquired=-1;
    // No further input: consuming this complete fresh window must suffice.
    while(Clock::now()-start<std::chrono::milliseconds(290)) {
        if(std::abs(error(tuner.frequency(),220))<.1 && tuner.confidence()>.9f) {
            acquired=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    tuner.stop();
    std::cerr<<"GENERATION_TRANSITION,"<<(expiredGap?"resumed_after_expiry":"reenable")
             <<",acquire_ms="<<acquired<<'\n';
    require(acquired>=0 && acquired<290,"Fresh complete generation was discarded");
#endif
}

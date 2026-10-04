#include "GlobalDSP.h"
#include "PerformanceUtilities.h"
#include "PostRigGate.h"
#include "SpatialModules.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
float signal(int sample,int channel,double rate) {
    return float((channel==0 ? .2 : -.13)*std::sin(juce::MathConstants<double>::twoPi*220*sample/rate));
}
void pitchDryPaths()
{
    for(double rate:{44100.,48000.,96000.}) for(int channels:{1,2}) for(int blockSize:{1,64,257,2048}) {
        spectralforge::Transposer pitch;pitch.prepare({rate,(juce::uint32)blockSize,(juce::uint32)channels});
        juce::AudioBuffer<float> audio(channels,blockSize);
        for(int enabled:{0,1}) {
            pitch.reset();
            for(int offset=0;offset<pitch.latency()*3;offset+=blockSize) {
                for(int c=0;c<channels;++c)for(int n=0;n<blockSize;++n)audio.setSample(c,n,signal(offset+n,c,rate));
                pitch.process(audio,enabled!=0,enabled ? 0 : -12);
                for(int c=0;c<channels;++c)for(int n=0;n<blockSize;++n) {
                    const int original=offset+n-(enabled ? pitch.latency() : 0);
                    require(std::abs(audio.getSample(c,n)-(original>=0 ? signal(original,c,rate) : 0.f))<1e-7f,"Bypass/zero-semitone path changed samples or delay");
                }
            }
            require(pitch.processedPitchFrames()==0,"Bypass/zero-semitone path still runs the FFT");
        }
    }
    std::cout<<"PASS: exact bypass and latency-matched zero semitones; 3 rates x 2 layouts x 4 block sizes, no FFT frames\n";
}
void pitchTransitions()
{
    for(double rate:{44100.,48000.,96000.}) for(int shift:{-12,-5,7,12}) {
        spectralforge::Transposer pitch;pitch.prepare({rate,256,2});
        juce::AudioBuffer<float> audio(2,256);int offset=0;
        auto process=[&](bool enabled,int semitones,int blocks) {
            double energy=0;
            for(int block=0;block<blocks;++block) {
                for(int c=0;c<2;++c)for(int n=0;n<256;++n)audio.setSample(c,n,signal(offset+n,c,rate));
                pitch.process(audio,enabled,semitones);
                for(int c=0;c<2;++c)for(int n=0;n<256;++n) {
                    const float x=audio.getSample(c,n);
                    require(std::isfinite(x) && std::abs(x)<1.f,"Pitch transition produces invalid/unbounded audio");
                    if(block>blocks/2) energy+=double(x)*x;
                }
                offset+=256;
            }
            return energy;
        };
        process(false,shift,50);
        require(pitch.processedPitchFrames()==0,"Disabled nonzero pitch runs the FFT");
        require(process(true,shift,100)>1.,"Pitch engagement does not produce sustained audio");
        require(pitch.processedPitchFrames()>0,"Active pitch never runs the FFT");
        process(true,0,50);const auto zeroFrames=pitch.processedPitchFrames();
        process(true,0,50);require(pitch.processedPitchFrames()==zeroFrames,"Zero-semitone settled fade did not idle");
        require(process(true,shift,100)>1.,"Pitch re-engagement exposes stale/empty synthesis");
        process(false,shift,50);const auto bypassFrames=pitch.processedPitchFrames();
        process(false,shift,50);require(pitch.processedPitchFrames()==bypassFrames,"Bypassed settled fade did not idle");
    }
    std::cout<<"PASS: bypass/zero/shift/re-engagement transitions across 3 rates and 4 intervals\n";
}
void pitchOutgoingRatio()
{
    for(double rate:{44100.,48000.,96000.})for(int shift:{-12,-2,2,12}) {
        spectralforge::Transposer a,b;a.prepare({rate,128,2});b.prepare({rate,128,2});
        juce::AudioBuffer<float> left(2,128),right(2,128);
        for(int block=0;block<120;++block) {
            for(int c=0;c<2;++c)for(int n=0;n<128;++n)left.setSample(c,n,signal(block*128+n,c,rate));
            right.makeCopyOf(left);a.process(left,block<90,shift);b.process(right,block<90,block<90?shift:0);
            for(int c=0;c<2;++c)for(int n=0;n<128;++n)require(left.getSample(c,n)==right.getSample(c,n),"Bypass request retuned outgoing wet OLA tail");
        }
    }
    std::cout<<"PASS outgoing ratio retained during bypass fade: 3 rates x 4 shifts\n";
}
void tunerLifetime()
{
    const auto started=juce::Time::getMillisecondCounterHiRes();
    for(int iteration=0;iteration<30;++iteration) {
        spectralforge::Tuner tuner;tuner.prepare(48000);
        juce::AudioBuffer<float> audio(2,256);
        for(int block=0;block<40;++block) {
            for(int c=0;c<2;++c)for(int n=0;n<256;++n)audio.setSample(c,n,signal(block*256+n,c,48000));
            tuner.push(audio,true);
        }
        tuner.stop();tuner.stop();
    }
    std::cout<<"PASS: 30 active tuner prepare/stop/destruct cycles, "<<juce::Time::getMillisecondCounterHiRes()-started<<" ms\n";
}
void postRigGate()
{
    for(int delay:{0,37,2048}) {
        spectralforge::PostRigGate gate;gate.prepare({48000,256,2},4096);
        spectralforge::NoiseGate reference;reference.prepare(48000);
        juce::AudioBuffer<float> input(2,256),referenceAudio(2,256),rigOutput(2,256);
        std::vector<float> expected;
        for(int block=0;block<200;++block) {
            // Initial silence closes the detector; the burst must reopen at
            // exactly the matching audio latency, including across blocks.
            const float detectorSample=(block>=100 && block<120) ? .2f : 0.f;
            for(int n=0;n<256;++n) {
                input.setSample(0,n,detectorSample);input.setSample(1,n,-detectorSample);
                referenceAudio.setSample(0,n,detectorSample);referenceAudio.setSample(1,n,-detectorSample);
                rigOutput.setSample(0,n,.01f);rigOutput.setSample(1,n,-.007f);
            }
            std::array<float,256> gains{};
            reference.detect(referenceAudio,gains.data(),true,-60,5,0);
            expected.insert(expected.end(),gains.begin(),gains.end());
            gate.detect(input,true,-60,5,0,delay);gate.apply(rigOutput);
            for(int n=0;n<256;++n) {
                require(input.getSample(0,n)==detectorSample && input.getSample(1,n)==-detectorSample,"Post-rig detector changed clean input");
                const int index=block*256+n-delay;
                const float expectedGain=index>=0 ? expected[(size_t)index] : 0.f;
                require(std::abs(rigOutput.getSample(0,n)-.01f*expectedGain)<1e-7f,"Post-rig gate envelope latency is misaligned");
                require(std::abs(rigOutput.getSample(1,n)+.007f*expectedGain)<1e-7f,"Post-rig gate is not stereo linked");
            }
        }
        require(rigOutput.getMagnitude(0,256)<1e-8f,"Clean-input gate cannot suppress post-rig residual noise during silence");
    }
    // This comparison locks the legacy input-gate path against the newly
    // factored detector, including hysteresis, hold and release.
    spectralforge::NoiseGate legacy,split;legacy.prepare(48000);split.prepare(48000);
    juce::AudioBuffer<float> audio(2,256),original(2,256);std::array<float,256> gains{};
    for(int block=0;block<100;++block) {
        for(int c=0;c<2;++c)for(int n=0;n<256;++n)audio.setSample(c,n,block<20 ? signal(block*256+n,c,48000) : 1e-7f);
        original.makeCopyOf(audio);legacy.process(audio,true,-60,25,10);split.detect(original,gains.data(),true,-60,25,10);
        for(int c=0;c<2;++c)for(int n=0;n<256;++n)require(audio.getSample(c,n)==original.getSample(c,n)*gains[(size_t)n],"Legacy gate behaviour changed while adding detector output");
    }
    spectralforge::PostRigGate tailGate;tailGate.prepare({48000,256,2});
    spectralforge::EchoModule echo;echo.prepare({48000,256,2});
    double tailEnergy=0;
    for(int block=0;block<120;++block) {
        for(int c=0;c<2;++c)for(int n=0;n<256;++n)audio.setSample(c,n,block<15 ? signal(block*256+n,c,48000) : 0.f);
        tailGate.detect(audio,true,-60,5,0,0);tailGate.apply(audio);
        echo.process(audio,true,0,160,.55f,.7f);
        if(block>40 && tailGate.reduction()<1e-5f)
            for(int n=0;n<256;++n)tailEnergy+=double(audio.getSample(0,n))*audio.getSample(0,n);
    }
    require(tailEnergy>.01,"Gate before POST did not preserve delayed tails while detector was closed");
    std::cout<<"PASS: optional post-rig gate clean detector, stereo gain, residual-noise attenuation, exact 0/37/2048-sample alignment and POST delay tails; legacy gate equivalence\n";
}
void pitchCost()
{
    for(double rate:{44100.,48000.,96000.}) {
        auto elapsed=[&](bool enabled,int shift) {
            spectralforge::Transposer pitch;pitch.prepare({rate,256,2});juce::AudioBuffer<float> audio(2,256);
            const auto started=juce::Time::getHighResolutionTicks();
            for(int block=0;block<400;++block) {
                for(int c=0;c<2;++c)for(int n=0;n<256;++n)audio.setSample(c,n,signal(block*256+n,c,rate));
                pitch.process(audio,enabled,shift);
            }
            return juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-started)*1000.;
        };
        const auto bypass=elapsed(false,-5),zero=elapsed(true,0),active=elapsed(true,-5);
        std::cout<<"MEASURE pitch 400 stereo blocks/256 samples at "<<rate<<" Hz: bypass "<<bypass
                 <<" ms, zero "<<zero<<" ms, active -5 "<<active<<" ms; algorithm delay "
                 <<(rate>48000 ? 4096 : 2048)<<" samples (CPU timings are local, not a Windows DAW benchmark)\n";
    }
}
}
int main()
{
    try {pitchDryPaths();pitchTransitions();pitchOutgoingRatio();tunerLifetime();postRigGate();pitchCost();return 0;}
    catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}

#pragma once
#include "GlobalDSP.h"
#include "IRLibrary.h"
#include "ChimeraIRData.h"
#include "AmpVoiceTests.h"
#include <complex>

namespace featureTests {
inline void expect(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
inline double rms(const std::vector<float>& samples) { double sum=0; for(auto x:samples) sum+=double(x)*x; return std::sqrt(sum/samples.size()); }
inline std::vector<float> amplifier(int model,int oversampling,float amplitude,double hz=997.0)
{
    spectralforge::Amp amp; amp.prepare({48000,256,1}); amp.set((spectralforge::AmpModel)model,1); amp.setOversampling(oversampling);
    juce::AudioBuffer<float> buffer(1,256); std::vector<float> result;
    for(int block=0;block<192;++block)
    {
        for(int n=0;n<256;++n) buffer.setSample(0,n,amplitude*float(std::sin(juce::MathConstants<double>::twoPi*hz*(block*256+n)/48000)));
        amp.process(buffer,false);
        if(block>=64) result.insert(result.end(),buffer.getReadPointer(0),buffer.getReadPointer(0)+256);
    }
    return result;
}
inline double component(const std::vector<float>& audio,double hz)
{
    std::complex<double> sum{};
    for(size_t i=0;i<audio.size();++i) sum+=double(audio[i])*std::polar(1.0,-juce::MathConstants<double>::twoPi*hz*double(i)/48000);
    return std::abs(sum)/audio.size();
}
inline void amps()
{
    std::array<std::vector<float>,spectralforge::ampModelCount> renders;
    for(int model=0;model<spectralforge::ampModelCount;++model)
    {
        renders[(size_t)model]=amplifier(model,2,.3f);
        double mean=0;
        for(auto x:renders[(size_t)model]) { expect(std::isfinite(x) && std::abs(x)<3,"Unstable amplifier output"); mean+=x; }
        mean/=renders[(size_t)model].size();
        expect(std::abs(mean)<.001,"Amplifier introduces persistent DC");
        const auto quiet=amplifier(model,2,.015f);
        const double ratio=rms(renders[(size_t)model])/rms(quiet);
        expect(ratio>1.05 && ratio<21,"Amplifier dynamics are inverted or expanding");
        for(int other=0;other<model;++other)
        {
            double difference=0;
            for(size_t n=0;n<quiet.size();++n) difference+=std::pow(renders[(size_t)model][n]-renders[(size_t)other][n],2);
            expect(std::sqrt(difference/quiet.size())>.005,"Two amp voices still share the same response");
        }
    }
    const double hz=48000.0*6831/32768.0;
    const double folded=5*hz-48000;
    std::array<double,4> ratios{};
    for(int factor=0;factor<4;++factor)
    {
        const auto audio=amplifier(2,factor,.8f,hz);
        ratios[(size_t)factor]=20*std::log10(component(audio,folded)/component(audio,hz));
        std::cout<<"MEASURE oversampling "<<(1<<factor)<<"x: fifth-harmonic foldback = "<<ratios[(size_t)factor]<<" dBc\n";
    }
    expect(ratios[2]<ratios[0]-12,"4x oversampling did not suppress foldback by 12 dB");
    expect(ratios[3]<ratios[0]-12,"8x oversampling did not suppress foldback by 12 dB");
}
inline void gateAndTuner()
{
    spectralforge::NoiseGate gate; gate.prepare(48000);
    juce::AudioBuffer<float> buffer(2,256);
    for(int block=0;block<200;++block)
    {
        for(int n=0;n<256;++n) {buffer.setSample(0,n,1e-5f);buffer.setSample(1,n,-1e-5f);}
        gate.process(buffer,true,-60,50,20);
    }
    expect(buffer.getMagnitude(0,256)<1e-9f,"Gate does not close on noise");
    for(int n=0;n<256;++n) {buffer.setSample(0,n,.1f);buffer.setSample(1,n,-.05f);}
    gate.process(buffer,true,-60,50,20);
    expect(std::abs(buffer.getSample(0,255))>.099f,"Gate attack loses the onset");
    expect(std::abs(buffer.getSample(0,255)+2*buffer.getSample(1,255))<1e-7f,"Gate is not stereo linked");
    for(int block=0;block<300;++block)
    {
        for(int n=0;n<256;++n) { const float x=.01f*float(std::sin(juce::MathConstants<double>::twoPi*30.87*(block*256+n)/48000));buffer.setSample(0,n,x); buffer.setSample(1,n,x); }
        gate.process(buffer,true,-60,50,20);
        expect(gate.reduction()>.99f,"Gate chatters on a low bass note");
    }
    for(double hz:{25.96,30.87,41.203,55.0,82.407,110.0,220.0,440.0,880.0,1200.0,1390.0})
    {
        std::array<float,2048> data{};
        for(size_t i=0;i<data.size();++i) {const double phase=juce::MathConstants<double>::twoPi*hz*double(i)/12000;data[i]=float(.2*std::sin(phase)+.10*std::sin(2*phase)+.04*std::sin(3*phase));}
        const auto estimate=spectralforge::Tuner::estimate(data.data(),2048,12000);
        const double cents=1200*std::log2(estimate.first/hz);
        std::cout<<"MEASURE tuner "<<hz<<" Hz: "<<cents<<" cents\n";
        expect(estimate.second>.85 && std::abs(cents)<5,"Tuner inaccurate or octave tracking failed");
    }
    std::array<float,2048> silence{};
    expect(spectralforge::Tuner::estimate(silence.data(),2048,12000).first==0,"Tuner displays a note for silence");
}
inline void pitch()
{
    for(double rate:{44100.0,48000.0,96000.0})
    {
        spectralforge::Transposer transposer; transposer.prepare({rate,256,2});
        juce::AudioBuffer<float> buffer(2,256);
        std::vector<float> original;
        for(int block=0;block<60;++block)
        {
            for(int n=0;n<256;++n) {const float x=.1f*float(std::sin(juce::MathConstants<double>::twoPi*110*(block*256+n)/rate));buffer.setSample(0,n,x);buffer.setSample(1,n,-x);original.push_back(x);}
            transposer.process(buffer,true,0);
            for(int n=0;n<256;++n)
            {
                const int inputIndex=block*256+n-transposer.latency();
                if(inputIndex>=0) expect(std::abs(buffer.getSample(0,n)-original[(size_t)inputIndex])<1e-6f,"Zero transpose is not a latency-matched dry path");
                expect(std::abs(buffer.getSample(0,n)+buffer.getSample(1,n))<1e-6f,"Pitch dry path changed stereo relationship");
            }
        }
        std::cout<<"MEASURE pitch latency at "<<rate<<" Hz = "<<transposer.latency()<<" samples\n";
    }
    for(int shift:{-12,-5,7,12})
    {
        spectralforge::Transposer transposer; transposer.prepare({48000,256,1}); juce::AudioBuffer<float> buffer(1,256);
        std::vector<float> audio;
        for(int block=0;block<240;++block)
        {
            for(int n=0;n<256;++n) buffer.setSample(0,n,.2f*float(std::sin(juce::MathConstants<double>::twoPi*220*(block*256+n)/48000)));
            transposer.process(buffer,true,shift);
            if(block>=112) audio.insert(audio.end(),buffer.getReadPointer(0),buffer.getReadPointer(0)+256);
        }
        const double expected=220*std::pow(2.0,shift/12.0);
        const double wanted=component(audio,expected),unshifted=component(audio,220);
        std::cout<<"MEASURE transpose "<<shift<<" st: target "<<expected<<" Hz, dry leakage "<<20*std::log10(unshifted/wanted)<<" dB; target amplitude "<<wanted<<"; RMS "<<rms(audio)<<"\n";
        const auto detected=spectralforge::Tuner::estimate(audio.data()+audio.size()-2048,2048,48000);
        const double cents=1200*std::log2(detected.first/expected);
        std::cout<<"MEASURE pitch accuracy: "<<cents<<" cents\n";
        expect(std::abs(cents)<5,"Transpose is detuned");
        expect(wanted>.015 && wanted>unshifted*10,"Pitch interval/dry rejection is incorrect");
    }
}
inline void cabinets()
{
    using namespace spectralforge;
    juce::String error;
    const auto source=juce::MemoryBlock(ChimeraIRData::guitar_v30_sm57_wav,ChimeraIRData::guitar_v30_sm57_wavSize);
    auto decoded=IRLibrary::decode(source,"Factory",error);
    expect(decoded && decoded->rate==44100 && decoded->samples.getNumSamples()>12000,"Factory IR is invalid");
    juce::MemoryBlock junk("not an impulse",14);
    expect(!IRLibrary::decode(junk,"bad",error),"Invalid IR accepted");
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("chimera-test-ir",".wav");
    expect(file.replaceWithData(source.getData(),source.getSize()),"Cannot create temporary IR");
    std::array<Cab,3> cabs;
    for(auto& cab:cabs) cab.prepare({48000,256,2});
    IRLibrary library({&cabs[0],&cabs[1],&cabs[2]}); library.prepare({48000,256,2},{0,0,0});
    expect(library.importFile(1,file).wasOk(),"User IR import failed");
    auto saved=library.save(); expect(file.deleteFile(),"Cannot remove original IR");
    library.restore(saved); cabs[1].requestedSource.store(3);
    juce::AudioBuffer<float> block(2,256);
    for(int i=0;i<150;++i) {block.clear();cabs[1].process(block);juce::Thread::sleep(2);}
    expect(cabs[1].activeSource.load()==3,"Restored embedded IR did not activate without original file");
    expect(cabs[0].activeSource.load()==0 && cabs[2].activeSource.load()==0,"Loading IR changed another lane");
    expect(library.importFile(1,file).failed(),"Missing IR accepted");
    expect(library.save().getChild(0).getProperty("data")==saved.getChild(0).getProperty("data"),"Failed load discarded previous IR");
    library.stop();
    Cab::Kernel kernel(juce::AudioBuffer<float>(decoded->samples),decoded->rate,{48000,256,2},1,1);
    block.clear(); block.setSample(0,0,1);
    kernel.process(block);
    expect(block.getMagnitude(0,0,256)>0.001f,"Convolution did not produce audio");
    expect(block.getMagnitude(1,0,256)<1e-8f,"Cabinet leaks left input into right channel");
}
inline void run() { amps();ampVoiceTests::run();gateAndTuner();pitch();cabinets();std::cout<<"PASS: amplifier signatures/DC/dynamics, anti-aliasing, gate, tuner, transpose, embedded IR and lane isolation\n"; }
}

#pragma once
#include "IRLibrary.h"
#include "ChimeraIRData.h"

namespace referenceTests {
inline void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
inline juce::AudioBuffer<float> fixture()
{
    // Versioned synthetic excitation, not a guitar recording or hardware capture.
    juce::AudioBuffer<float> audio(1,48000*5);audio.clear();unsigned seed=0x4348494d;
    for(int n=24000;n<48000*4;++n) {
        const int segment=(n-24000)/24000;const double t=((n-24000)%24000)/48000.0;
        const double amplitude=segment%3==0 ? .04 : segment%3==1 ? .12 : .32;
        const double fundamental=segment<3 ? 61.7354 : 82.4069;
        double x=0;for(int harmonic=1;harmonic<=6;++harmonic) x+=std::sin(juce::MathConstants<double>::twoPi*fundamental*harmonic*t)/double(harmonic*harmonic);
        if(segment>=3) x+=.45*std::sin(juce::MathConstants<double>::twoPi*123.4708*t)+.25*std::sin(juce::MathConstants<double>::twoPi*164.8138*t);
        seed=seed*1664525u+1013904223u;
        x=x*std::exp(-t*5)*(1-std::exp(-t*800))+.1*(double(seed>>8)/8388608.0-1)*std::exp(-t*400);
        audio.setSample(0,n,float(x*amplitude));
    }
    return audio;
}
inline juce::AudioBuffer<float> render(const juce::AudioBuffer<float>& dry,float drive)
{
    spectralforge::Engine engine;const juce::dsp::ProcessSpec spec{48000,256,1};engine.prepare(spec);engine.setOversampling(2);
    juce::String error;const auto asset=spectralforge::IRLibrary::decode(juce::MemoryBlock(ChimeraIRData::guitar_v30_sm57_wav,ChimeraIRData::guitar_v30_sm57_wavSize),"V30",error);
    require(asset!=nullptr,"Reference IR did not decode");
    engine.cabinet(0).install(std::make_unique<spectralforge::Cab::Kernel>(juce::AudioBuffer<float>(asset->samples),asset->rate,spec,1,1));
    std::array<spectralforge::LaneState,3> lanes{};lanes[0].amp=2;lanes[0].drive=drive;lanes[0].levelDb=-12;
    juce::AudioBuffer<float> output(1,dry.getNumSamples()),block(1,256);
    for(int offset=0;offset<dry.getNumSamples();offset+=256) {
        const int count=juce::jmin(256,dry.getNumSamples()-offset);block.setSize(1,count,false,false,true);block.copyFrom(0,0,dry,0,offset,count);
        engine.process(block,spectralforge::RoutingMode::classic,150,1200,lanes);output.copyFrom(0,offset,block,0,0,count);
    }
    return output;
}
inline void save(const juce::File& folder,const char* name,const juce::AudioBuffer<float>& audio)
{
    const auto file=folder.getChildFile(name);file.deleteFile();auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot create reference audio");
    juce::WavAudioFormat format;auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),48000,1,24,{},0));
    require(writer!=nullptr,"Cannot encode reference audio");stream.release();require(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()),"Reference WAV write failed");
}
inline void run()
{
    const auto dry=fixture(),a=render(dry,.35f),repeat=render(dry,.35f);auto b=render(dry,.65f);
    double maximum=0,difference=0;for(int n=0;n<a.getNumSamples();++n) maximum=juce::jmax(maximum,std::abs(double(a.getSample(0,n)-repeat.getSample(0,n))));
    const float rmsA=a.getRMSLevel(0,24000,168000),rmsB=b.getRMSLevel(0,24000,168000),match=rmsA/rmsB;b.applyGain(match);
    for(int n=24000;n<192000;++n) difference+=std::pow(a.getSample(0,n)-b.getSample(0,n),2);
    const double residual=std::sqrt(difference/168000),level=20*std::log10(b.getRMSLevel(0,24000,168000)/rmsA);
    require(maximum<1e-7,"A/A reference render is not repeatable");require(std::abs(level)<.001,"A/B renders are not RMS matched");require(residual>1e-4,"Drive reference lost its level-matched response difference");
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("reference-audio");require(folder.createDirectory().wasOk(),"Cannot create reference directory");
    save(folder,"DI-synthetic-v1.wav",dry);save(folder,"A-tight035-v30.wav",a);save(folder,"B-tight065-v30-RMS-matched.wav",b);
    juce::String report="fixture,DI-synthetic-v1; not a real instrument or hardware reference\nsample_rate,48000\namp,Tight515\nIR,V30-SM57 factory\noversampling,4x\nA_drive,0.35\nB_drive,0.65\nA_A_null_peak,"+juce::String(maximum,10)+"\nB_match_gain_db,"+juce::String(juce::Decibels::gainToDecibels(match),6)+"\nmatched_level_error_db,"+juce::String(level,9)+"\nmatched_residual_rms,"+juce::String(residual,9)+"\n";
    require(folder.getChildFile("metrics.csv").replaceWithText(report),"Cannot save reference metrics");
    std::cout<<"MEASURE fixed reference: A/A null "<<maximum<<", matched A/B level "<<level<<" dB, residual RMS "<<residual<<"\n";
}
}

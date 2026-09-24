#include "Amplifier.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
// Offline reference renderer. No cabinet, gain matching or automatic normalisation.
int main(int argc,char** argv) {
    if(argc!=5){std::cerr<<"Usage: chimera-render input.wav output.wav amp-index drive\n";return 1;}
    juce::AudioFormatManager formats;formats.registerFormat(new juce::WavAudioFormat(),true);
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(argv[1])));
    if(!reader || reader->numChannels!=1 || reader->lengthInSamples>48000*600)return 2;
    juce::AudioBuffer<float> audio(1,(int)reader->lengthInSamples);reader->read(&audio,0,audio.getNumSamples(),0,true,false);
    spectralforge::Amp amp;amp.prepare({reader->sampleRate,256,1});amp.set(static_cast<spectralforge::AmpModel>(juce::jlimit(0,7,std::atoi(argv[3]))),(float)std::atof(argv[4]));amp.tone(0,0,0,0,0,0);
    for(int pos=0;pos<audio.getNumSamples();pos+=256){float* data=audio.getWritePointer(0,pos);juce::AudioBuffer<float> block(&data,1,juce::jmin(256,audio.getNumSamples()-pos));amp.process(block);}
    juce::WavAudioFormat format;auto out=juce::File(argv[2]).createOutputStream();if(!out)return 3;
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(out.get(),reader->sampleRate,1,32,{},0));if(!writer)return 4;out.release();
    if(!writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()))return 5;
    std::cout<<"amp="<<argv[3]<<" drive="<<argv[4]<<" oversampling=4x latency="<<amp.latency()<<" samples\n";
}

#include "Amplifier.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
// Offline reference renderer. No cabinet, gain matching or automatic normalisation.
int main(int argc,char** argv) {
    if(argc!=5&&argc!=6){std::cerr<<"Usage: chimera-render input.wav output.wav amp-index drive [native-state.json]\n";return 1;}
    juce::AudioFormatManager formats;formats.registerFormat(new juce::WavAudioFormat(),true);
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(argv[1])));
    if(!reader || reader->numChannels!=1 || reader->lengthInSamples>48000*600)return 2;
    juce::AudioBuffer<float> audio(1,(int)reader->lengthInSamples);reader->read(&audio,0,audio.getNumSamples(),0,true,false);
    spectralforge::Amp amp;amp.prepare({reader->sampleRate,256,1});amp.set(static_cast<spectralforge::AmpModel>(juce::jlimit(0,spectralforge::ampModelCount-1,std::atoi(argv[3]))),(float)std::atof(argv[4]));amp.tone(0,0,0,0,0,0);
    if(argc==6) {
        const auto json=juce::JSON::parse(juce::File(argv[5]));
        if(!json.isObject()){std::cerr<<"Invalid native state JSON\n";return 6;}
        auto state=spectralforge::defaultAmpNativeState(std::atoi(argv[3]));
        if(json.hasProperty("channel"))state.channel=int(json["channel"]);
        if(json.hasProperty("input_route"))state.inputRoute=int(json["input_route"]);
        if(json.hasProperty("input_trim_db"))state.inputTrimDb=float(json["input_trim_db"]);
        if(json.hasProperty("output_level_db"))state.outputLevelDb=float(json["output_level_db"]);
        if(auto* controls=json["controls"].getDynamicObject())for(const auto& item:controls->getProperties()) {
            const auto key=item.name.toString();const int i=spectralforge::ampNativeControlIndex(state.model,key.toStdString());
            if(i<0){std::cerr<<"Unknown native control: "<<key<<'\n';return 6;}
            state.values[size_t(i)]=float(item.value);
        }
        spectralforge::sanitiseAmpNativeState(state);amp.setNative(state);amp.reset();
    }
    for(int pos=0;pos<audio.getNumSamples();pos+=256){float* data=audio.getWritePointer(0,pos);juce::AudioBuffer<float> block(&data,1,juce::jmin(256,audio.getNumSamples()-pos));amp.process(block);}
    juce::WavAudioFormat format;auto out=juce::File(argv[2]).createOutputStream();if(!out || !out->setPosition(0) || out->truncate().failed())return 3;
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(out.get(),reader->sampleRate,1,32,{},0));if(!writer)return 4;out.release();
    if(!writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()))return 5;
    std::cout<<"amp="<<argv[3]<<" engine="<<(argc==6?"native":"legacy")<<" drive="<<argv[4]<<" oversampling=4x latency="<<amp.latency()<<" samples\n";
}

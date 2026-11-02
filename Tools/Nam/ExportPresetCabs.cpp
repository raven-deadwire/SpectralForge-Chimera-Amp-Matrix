// Exports the cabinet IR of one factory rig preset lane, exactly as the
// plugin's own "Export cabinet IR" does, for building head + cab NAM targets.
// Usage: ChimeraPresetCabs <nastrond|niflheimr> <rig preset> <lane> <out.wav>
#include "PluginProcessor.h"
#include "GuitarSignaturePresets.h"
#include <iostream>

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc!=5){std::cerr<<"usage: ChimeraPresetCabs <nastrond|niflheimr> <preset> <lane> <out.wav>\n";return 2;}
    const juce::String amp(argv[1]);
    const int preset=juce::String(argv[2]).getIntValue(),lane=juce::String(argv[3]).getIntValue();
    const int start=amp=="niflheimr"?spectralforge::niflheimrPresetStart:spectralforge::originalPresetStart;
    const int index=start+preset;
    ChimeraProcessor p;
    constexpr double rate=48000.;constexpr int block=256;
    p.setRateAndBufferSizeDetails(rate,block);p.prepareToPlay(rate,block);
    p.loadFactoryPreset(index);
    juce::AudioBuffer<float> silence(2,block);juce::MidiBuffer midi;
    for(int i=0;i<8;++i){silence.clear();p.processBlock(silence,midi);}
    const auto ir=p.cabinetImpulseResponse(lane,rate);
    double diff=0;for(int n=0;n<ir.getNumSamples();++n)diff=std::max(diff,double(std::abs(ir.getSample(0,n)-ir.getSample(1,n))));
    const auto result=p.exportCabinetIR(lane,juce::File::getCurrentWorkingDirectory().getChildFile(argv[4]));
    std::cout<<"{\"amp\":\""<<amp.toStdString()<<"\",\"preset\":"<<preset<<",\"preset_index\":"<<index<<",\"preset_name\":\""<<juce::String::fromUTF8(spectralforge::selectablePresetName(index)).toStdString()
             <<"\",\"lane\":"<<lane<<",\"samples\":"<<ir.getNumSamples()<<",\"lr_max_diff\":"<<diff
             <<",\"ok\":"<<(result.wasOk()?"true":"false")<<"}\n";
    if(!result.wasOk()){std::cerr<<result.getErrorMessage()<<'\n';return 1;}
    return 0;
}

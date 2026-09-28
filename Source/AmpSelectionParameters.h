#pragma once
#include "AmpCatalog.h"
#include "NewAmpDSP.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace spectralforge {
// Never extend amp1/2/3: their original 15 choices map automation to index/14.
// This separate explicit bank selects the eight appended DSP models. Changes
// to an old amp automation lane keep updating its legacy value but cannot turn
// this bank off. Structural model/channel selection is not host-automatable.
inline constexpr int extendedAmpBankCount=8;
inline juce::String ampExtensionID(int lane) { return "ampext"+juce::String(lane+1); }
inline juce::String ampChannelID(int lane,int model) {
    return "ampchannel"+juce::String(lane+1)+"_m"+juce::String(model);
}
inline void addAmpSelectionParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
    const auto structural=juce::AudioParameterIntAttributes().withAutomatable(false);
    for(int lane=0;lane<3;++lane) {
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{ampExtensionID(lane),1},"Amp "+juce::String(lane+1)+" extension bank",
            0,extendedAmpBankCount,0,structural));
        for(int bank=0;bank<extendedAmpBankCount;++bank) {
            const int model=legacyAmpModelCount+bank;
            // Reserve four raw positions permanently, including one-channel
            // models. Only the DSP's declared channels are offered in the UI.
            layout.add(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID{ampChannelID(lane,model),1},
                "Amp "+juce::String(lane+1)+" "+ampInfo(model).name+" channel",
                0,3,newAmpDefaultChannel(model),structural));
        }
    }
}
struct AmpSelectionParameterCache {
    std::array<std::atomic<float>*,3> legacy{},extension{};
    std::array<std::array<std::atomic<float>*,extendedAmpBankCount>,3> channels{};
    void bind(juce::AudioProcessorValueTreeState& state) {
        for(int lane=0;lane<3;++lane) {
            const auto i=static_cast<size_t>(lane);
            legacy[i]=state.getRawParameterValue("amp"+juce::String(lane+1));
            extension[i]=state.getRawParameterValue(ampExtensionID(lane));
            for(int bank=0;bank<extendedAmpBankCount;++bank)
                channels[i][static_cast<size_t>(bank)]=state.getRawParameterValue(ampChannelID(lane,legacyAmpModelCount+bank));
        }
    }
    static int integer(const std::atomic<float>* parameter,int fallback) noexcept {
        if(!parameter)return fallback;
        const float value=parameter->load();
        return std::isfinite(value) && value>=-1024.f && value<=1024.f ? juce::roundToInt(value) : fallback;
    }
    int model(int lane) const noexcept {
        if(lane<0 || lane>=3)return 0;
        const auto i=static_cast<size_t>(lane);
        const int bank=integer(extension[i],0);
        if(bank>=1 && bank<=extendedAmpBankCount)return legacyAmpModelCount+bank-1;
        return juce::jlimit(0,legacyAmpModelCount-1,integer(legacy[i],2));
    }
    int channel(int lane,int selectedModel) const noexcept {
        if(lane<0 || lane>=3 || selectedModel<legacyAmpModelCount || selectedModel>=legacyAmpModelCount+extendedAmpBankCount)return 0;
        const int fallback=newAmpDefaultChannel(selectedModel);
        return juce::jlimit(0,newAmpChannelCount(selectedModel)-1,
            integer(channels[static_cast<size_t>(lane)][static_cast<size_t>(selectedModel-legacyAmpModelCount)],fallback));
    }
};
} // namespace spectralforge

#pragma once
#include "GuitarSignaturePresets.h"

namespace spectralforge {
// Display order is independent of the append-only saved preset identities.
inline constexpr std::array<const char*,12> presetCategoryOrder{{
    "SIGNATURE / Deadwire","SIGNATURE / Raven Guitar",
    "Guitar / Clean & Ambient","Guitar / Edge & Rock","Guitar / High Gain","Guitar / Lead & Texture",
    "Bass / Clean & Dynamics","Bass / Drive & Texture",
    "Dual / Blend","Dual / Crossover","Matrix / Bass","Matrix / Experimental"
}};
inline juce::String selectablePresetCategory(int index) {
    return isGuitarSignature(index)?presetCategoryOrder[1]:index>=0&&index<factoryPresetCount?factoryPresets[size_t(index)].category:"";
}
inline const std::array<int,selectablePresetCount>& presetDisplayOrder() {
    static const auto order=[] {
        std::array<int,selectablePresetCount> result{};int count=0;
        for(const auto* category:presetCategoryOrder)
            for(int index=0;index<selectablePresetCount;++index)
                if(selectablePresetCategory(index)==category)result[size_t(count++)]=index;
        jassert(count==selectablePresetCount);return result;
    }();
    return order;
}
inline int adjacentPreset(int current,int direction) {
    const auto& order=presetDisplayOrder();
    const auto found=std::find(order.begin(),order.end(),current);
    if(found==order.end())return direction<0?order.back():order.front();
    const int position=int(found-order.begin());
    return order[size_t((position+(direction<0?-1:1)+selectablePresetCount)%selectablePresetCount)];
}
}

#pragma once
#include "AmpNativeCatalog.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectralforge::ampHead {
// Anchors are relative to cabHead::Geometry::front, after alpha cropping.
// Presentation order is deliberately independent of host parameter order.
struct Knob {const char* key; const char* label; float x,y;};
inline constexpr Knob brit[]{
    {"hw.presence","PRESENCE",.14f,.75f}, {"hw.bass","BASS",.25f,.75f},
    {"hw.middle","MIDDLE",.36f,.75f}, {"hw.treble","TREBLE",.47f,.75f},
    {"hw.high_treble","LOUDNESS I",.58f,.75f}, {"hw.normal","LOUDNESS II",.69f,.75f}};
inline constexpr int nifOrder[]{0,1,2,4,3,5,6,8,9,10,11,12,13,7};
inline bool supported(int model) {return model==int(AmpModel::britEdge) || model==niflheimrAmpModel;}
inline std::vector<Knob> knobs(int model) {
    if(model==int(AmpModel::britEdge))return {std::begin(brit),std::end(brit)};
    std::vector<Knob> result;
    if(model==niflheimrAmpModel)for(int i=0;i<14;++i) {
        const auto& spec=ampNativePanel(model).controls[size_t(nifOrder[i])];
        result.push_back({spec.key,spec.label,.256f+float(i%7)*.069f,i<7?.704f:.837f});
    }
    return result;
}
inline int controlIndex(int model,const char* key) {
    const auto& controls=ampNativePanel(model).controls;
    for(size_t i=0;i<controls.size();++i)if(std::string_view(controls[i].key)==key)return int(i);
    return -1;
}
}

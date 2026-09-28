#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModelCatalog.h"
#include "PedalBoardCatalog.h"

namespace spectralforge {
// Product names are presentation data. Hardware references and stable DSP /
// automation identifiers remain in their original catalogs and saved state.
enum class PedalMenuKind { empty, compressor, boost, overdrive, distortion, fuzz,
                           envelope, wah, equalizer, modulation, pitch };
struct PedalMenuEntry { const char* name; PedalMenuKind kind; };
inline constexpr std::array<PedalMenuEntry,pedalModelCount> pedalMenuEntries{{
    {"Empty / +",PedalMenuKind::empty},
    {"Green Drive",PedalMenuKind::overdrive},
    {"Gold Drive",PedalMenuKind::overdrive},
    {"Rodent",PedalMenuKind::distortion},
    {"Bass DI",PedalMenuKind::overdrive},
    {"Micro Bass",PedalMenuKind::overdrive},
    {"Studio VCA",PedalMenuKind::compressor},
    {"Red OTA",PedalMenuKind::compressor},
    {"Optical",PedalMenuKind::compressor},
    {"Studio FET",PedalMenuKind::compressor},
    {"Variable Mu",PedalMenuKind::compressor},
    {"Q Sweep",PedalMenuKind::envelope},
    {"Band Sweep",PedalMenuKind::envelope},
    {"Reverse Sweep",PedalMenuKind::envelope},
    {"Bass Envelope",PedalMenuKind::envelope},
    {"Dynamic Wah",PedalMenuKind::wah},
    {"Big Sustain",PedalMenuKind::fuzz},
    {"Round Fuzz",PedalMenuKind::fuzz},
    {"Vintage Fuzz",PedalMenuKind::fuzz},
    {"Wool Bass",PedalMenuKind::fuzz},
    {"Gated Fuzz",PedalMenuKind::fuzz},
    {"Clean Lift",PedalMenuKind::boost},
    {"Treble Lift",PedalMenuKind::boost},
    {"Micro Lift",PedalMenuKind::boost},
    {"Echo Lift",PedalMenuKind::boost},
    {"Linear Power",PedalMenuKind::boost},
    {"Yellow Asym",PedalMenuKind::overdrive},
    {"Obsession",PedalMenuKind::overdrive},
    {"Plus Drive",PedalMenuKind::distortion},
    {"Dual Circuit",PedalMenuKind::overdrive},
    {"Manual Wah",PedalMenuKind::wah},
    {"Graphic EQ",PedalMenuKind::equalizer},
    {"Classic Chorus",PedalMenuKind::modulation},
    {"Spatial Chorus",PedalMenuKind::modulation},
    {"Stone Phase",PedalMenuKind::modulation},
    {"Tidal Flange",PedalMenuKind::modulation},
    {"Drift Vibrato",PedalMenuKind::modulation},
    {"Pulse Tremolo",PedalMenuKind::modulation},
    {"Mono Octaver",PedalMenuKind::pitch},
    {"Spectral Octaver",PedalMenuKind::pitch}
}};
struct PedalMenuGroup { PedalMenuKind kind; const char* label; };
inline constexpr std::array<PedalMenuGroup,10> pedalMenuGroups{{
    {PedalMenuKind::compressor,"Compressor"}, {PedalMenuKind::boost,"Boost"},
    {PedalMenuKind::overdrive,"Overdrive"}, {PedalMenuKind::distortion,"Distortion"},
    {PedalMenuKind::fuzz,"Fuzz"}, {PedalMenuKind::envelope,"Envelope Filter"},
    {PedalMenuKind::wah,"Wah"}, {PedalMenuKind::equalizer,"Equalizer"},
    {PedalMenuKind::modulation,"Modulation"}, {PedalMenuKind::pitch,"Pitch / Octave"}
}};
inline const char* pedalMenuName(int model) {
    return pedalMenuEntries[(size_t)juce::jlimit(0,pedalModelCount-1,model)].name;
}
inline const char* effectFamilyMenuName(int family,int model) {
    constexpr std::array<int,11> firstPedal{1,0,0,6,11,16,21,0,0,0,32};
    if(family<0 || family>=int(modelFamilies.size()))return "";
    const auto& f=modelFamilies[(size_t)family];model=juce::jlimit(0,f.count-1,model);
    if(firstPedal[(size_t)family]>0)return pedalMenuName(firstPedal[(size_t)family]+model);
    // Rack algorithms have independent aliases; never concatenate reference
    // names into the selector, including when a native panel is selected.
    constexpr std::array<const char*,3> delay{"Precision Delay","Tape Echo","Analog Echo"};
    constexpr std::array<const char*,3> reverb{"Studio Plate","Concert Hall","Spring Tank"};
    constexpr std::array<const char*,3> compressor{"Console VCA","Studio FET","Opto Level"};
    constexpr std::array<const char*,3> preamp{"Console Colour","Pure DI","Transformer Blue"};
    constexpr std::array<const char*,3> equalizer{"Console EQ","Colour Shelves","Passive Tube"};
    switch(family) {
        case 1:return delay[(size_t)model];case 2:return reverb[(size_t)model];
        case 7:return compressor[(size_t)model];case 8:return preamp[(size_t)model];
        case 9:return equalizer[(size_t)model];default:return f.models[(size_t)model].name;
    }
}
inline juce::StringArray effectFamilyMenuNames(int family) {
    juce::StringArray names;
    if(family>=0 && family<int(modelFamilies.size()))
        for(int model=0;model<modelFamilies[(size_t)family].count;++model)names.add(effectFamilyMenuName(family,model));
    return names;
}
}

class StableEffectComboBox : public juce::ComboBox {
    int lastSyncedId{};bool synced{};
public:
    void acceptSelection() {lastSyncedId=getSelectedId();synced=true;}
    void resetSyncExplicit(int id) {setSelectedId(id,juce::dontSendNotification);acceptSelection();}
    void syncSelectedId(int id) {
        if(synced && getSelectedId()!=lastSyncedId)return;
        resetSyncExplicit(id);
    }
};

class PedalSelector final : public StableEffectComboBox {
public:
    PedalSelector() {
        for(int model=0;model<spectralforge::pedalModelCount;++model)
            addItem(spectralforge::pedalMenuName(model),model+1);
    }
    juce::PopupMenu browsingMenu() const {
        using namespace spectralforge;
        juce::PopupMenu menu;menu.addItem(1,pedalMenuName(0),true,getSelectedId()==1);
        menu.addSeparator();
        for(const auto& category:pedalMenuGroups) {
            juce::PopupMenu group;
            for(int model=1;model<pedalModelCount;++model)
                if(pedalMenuEntries[(size_t)model].kind==category.kind)
                    group.addItem(model+1,pedalMenuName(model),pedalModel(model).implemented,getSelectedId()==model+1);
            menu.addSubMenu(category.label,group);
        }
        return menu;
    }
    bool selectMenuResult(int id) {
        if(id<1 || id>spectralforge::pedalModelCount || !spectralforge::pedalModel(id-1).implemented)return false;
        setSelectedId(id,juce::sendNotificationSync);return true;
    }
    void showPopup() override {
        if(!isEnabled())return;
        const juce::Component::SafePointer<PedalSelector> safe(this);
        browsingMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe](int id) {
            if(!safe)return;
            safe->hidePopup();
            if(safe && id>0)safe->selectMenuResult(id);
        });
    }
};

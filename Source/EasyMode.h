#pragma once
#include "AmpNativeCatalog.h"
#include <juce_core/juce_core.h>
#include <array>
#include <string_view>

namespace spectralforge::easy {
// Easy view is presentation only. Every knob edits an existing parameter of
// the full editor; it owns no host parameter and no saved sound state.
enum class Role {gain,bass,middle,treble};
inline constexpr int roleCount=4;
inline constexpr std::array<const char*,roleCount> roleNames{"GAIN","BASS","MIDDLE","TREBLE"};

// Front-panel names differ between heads. The first listed name the current
// channel shows wins, so GAIN is preferred over a non-master VOLUME.
// Catalog labels may append a hardware alias ("LOUDNESS I / HIGH TREBLE").
inline std::span<const char* const> roleLabels(Role role) noexcept {
    static constexpr const char* gain[]{"GAIN","DRIVE","PRE GAIN","PREAMP","CLEAN GAIN","LEAD GAIN","GAIN 1","GAIN 2",
                                        "LOUDNESS I","NORMAL","INPUT","VOLUME"};
    static constexpr const char* bass[]{"BASS","LOW","LO"};
    static constexpr const char* middle[]{"MIDDLE","MID","MIDRANGE","MID LEVEL","LO MID","LO MIDS"};
    static constexpr const char* treble[]{"TREBLE","HIGH","HI"};
    switch(role) {
        case Role::gain:return gain;
        case Role::bass:return bass;
        case Role::middle:return middle;
        case Role::treble:return treble;
    }
    return {};
}
// Heads whose active knob depends on the input jack, or whose panel holds two
// sections, are mapped explicitly. Easy view must edit the control the
// selected input path actually plays through (checked against the DSP).
inline int explicitAmpControl(int model,int route,Role role) noexcept {
    const auto key=[model](std::string_view k){return ampNativeControlIndex(model,k);};
    switch(model) {
        case 1: case 27: // Brit Edge / Bluestack 100: HIGH TREBLE jacks 0-1, NORMAL jacks 2-3, JUMPED 4 plays both.
            if(role==Role::gain)return route==2 || route==3 ? key("hw.normal") : key("hw.high_treble");
            break;
        case 19: // Monolith: NORMAL 0, BRILLIANT 1, JUMPED 2 plays both.
            if(role==Role::gain)return route==0 ? key("hw.normal") : key("hw.brilliant");
            break;
        case 28: // Eightyfire: Pre-amp Volume is the gain; Master stays in the full editor.
            if(role==Role::gain)return key("hw.preamp");
            break;
        case 29: // Fortress 103: NORMAL jacks 0-1, BRIGHT jacks 2-3, JUMPED 4 plays both.
            if(role==Role::gain)return route<2 ? key("hw.normal") : key("hw.bright");
            break;
        case 30: // Northlight has no gain stage: GAIN edits its Master.
            if(role==Role::gain)return key("hw.master");
            break;
        case 7: // Modern Bass: the knobs shape the amplifier section, not the drive section.
            switch(role) {
                case Role::gain:return key("hw.db751.gain");
                case Role::bass:return key("hw.db751.bass");
                case Role::middle:return key("hw.db751.mid");
                case Role::treble:return key("hw.db751.treble");
            }
            break;
        default:break;
    }
    return -2;
}
// Index of the head knob playing this role on the given channel and input
// route, or -1. With JUMPED inputs GAIN edits the bright input's volume
// (HIGH TREBLE / BRILLIANT); the other volume stays in the full editor.
inline int ampControl(int model,int channel,int route,Role role) noexcept {
    const auto& panel=ampNativePanel(model);
    if(const int explicitControl=explicitAmpControl(model,route,role);explicitControl!=-2)
        return explicitControl>=0 && ampNativeControlVisible(model,explicitControl,channel) ? explicitControl : -1;
    for(const auto* wanted:roleLabels(role))
        for(int c=0;c<(int)panel.controls.size();++c) {
            const auto& control=panel.controls[(size_t)c];
            if(control.kind==AmpNativeControlKind::knob && ampNativeControlVisible(model,c,channel)
               && juce::String::fromUTF8(control.label).upToFirstOccurrenceOf(" / ",false,false)==wanted)return c;
        }
    return -1;
}
// Legacy-engine projects (native panel off) keep the original lane knobs.
// The legacy EQ has two mid bands; MIDDLE edits the low mid.
inline juce::String legacyControlID(int lane,Role role) {
    constexpr std::array<const char*,roleCount> ids{"drive","bass","lowmid","treble"};
    return ids[(size_t)role]+juce::String(lane+1);
}
// Matrix LOW is a compressed DI; its MID rig carries the main amp voice.
inline int primaryLane(int mode) noexcept {return mode==2?1:0;}

// First start opens Easy view; afterwards the last chosen view is kept for
// this computer. It is never written into a project or reference file.
struct Preference {
    static juce::File file() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SpectralForge/Chimera/interface.json");
    }
    static bool startsEasy(const juce::File& storage=file()) {
        if(!storage.existsAsFile() || storage.getSize()>64*1024)return true;
        const auto settings=juce::JSON::parse(storage);
        return !settings.isObject() || (bool)settings.getProperty("easyView",true);
    }
    static bool remember(bool easyView,const juce::File& storage=file()) {
        auto settings=storage.existsAsFile() && storage.getSize()<=64*1024 ? juce::JSON::parse(storage) : juce::var{};
        if(!settings.isObject())settings=new juce::DynamicObject();
        settings.getDynamicObject()->setProperty("easyView",easyView);
        return storage.getParentDirectory().createDirectory().wasOk()
            && storage.replaceWithText(juce::JSON::toString(settings));
    }
};
} // namespace spectralforge::easy

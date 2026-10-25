#pragma once
#include "AmpNativeCatalog.h"
#include <juce_core/juce_core.h>
#include <array>

namespace spectralforge::easy {
// Easy view is presentation only. Every knob edits an existing parameter of
// the full editor; it owns no host parameter and no saved sound state.
enum class Role {gain,bass,middle,treble};
inline constexpr int roleCount=4;
inline constexpr std::array<const char*,roleCount> roleNames{"GAIN","BASS","MIDDLE","TREBLE"};

// Front-panel names differ between heads. The first listed name the current
// channel shows wins, so GAIN is preferred over a non-master VOLUME.
inline std::span<const char* const> roleLabels(Role role) noexcept {
    static constexpr const char* gain[]{"GAIN","DRIVE","PRE GAIN","PREAMP","LOUDNESS I","VOLUME"};
    static constexpr const char* bass[]{"BASS","LOW"};
    static constexpr const char* middle[]{"MIDDLE","MID","MIDRANGE","MID LEVEL"};
    static constexpr const char* treble[]{"TREBLE","HIGH"};
    switch(role) {
        case Role::gain:return gain;
        case Role::bass:return bass;
        case Role::middle:return middle;
        case Role::treble:return treble;
    }
    return {};
}
// Index of the head knob playing this role on the given channel, or -1.
inline int ampControl(int model,int channel,Role role) noexcept {
    const auto& panel=ampNativePanel(model);
    for(const auto* wanted:roleLabels(role))
        for(int c=0;c<(int)panel.controls.size();++c) {
            const auto& control=panel.controls[(size_t)c];
            if(control.kind==AmpNativeControlKind::knob && ampNativeControlVisible(model,c,channel)
               && juce::String::fromUTF8(control.label)==wanted)return c;
        }
    return -1;
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

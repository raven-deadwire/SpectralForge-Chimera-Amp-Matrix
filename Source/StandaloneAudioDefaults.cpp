// Standalone application target only (see CMakeLists.txt). Issue #113.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "StandaloneHooks.h"

namespace {
bool preferAsioOnFirstRun()
{
    auto* holder=juce::StandalonePluginHolder::getInstance();
    if(holder==nullptr || holder->settings==nullptr)return false;
    // JUCE writes "audioSetup" when the settings dialog closes or the app quits.
    // Its absence means the user has never chosen a device: never override a choice.
    if(holder->settings->containsKey("audioSetup"))return false;
    auto& devices=holder->deviceManager;
    const auto current=devices.getCurrentAudioDeviceType();
    if(current=="ASIO")return false;
    for(auto* type:devices.getAvailableDeviceTypes()) {
        if(type->getTypeName()!="ASIO")continue;
        type->scanForDevices();
        if(type->getDeviceNames().isEmpty())return false;
        devices.setCurrentAudioDeviceType("ASIO",true);
        if(devices.getCurrentAudioDevice()!=nullptr)return true;
        // The driver could not open (in use or disconnected): keep Windows audio.
        devices.setCurrentAudioDeviceType(current,true);
        return false;
    }
    return false;
}
[[maybe_unused]] const bool installed=[]{spectralforge::standalone::preferLowLatencyDriverOnFirstRun=&preferAsioOnFirstRun;return true;}();
}

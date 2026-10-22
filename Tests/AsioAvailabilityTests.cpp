// Windows Standalone ASIO contract (Issue #113): the product's JUCE build must
// offer the ASIO device type. Opening a driver needs real hardware and is
// outside CI; listing installed drivers is reported, not required.
#include <juce_audio_devices/juce_audio_devices.h>
#include <iostream>

int main() {
    juce::ScopedJuceInitialiser_GUI juce;
   #if ! JUCE_ASIO
    std::cerr << "FAIL: JUCE_ASIO is not enabled in this build\n";
    return 1;
   #else
    std::unique_ptr<juce::AudioIODeviceType> asio(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
    if(asio == nullptr || asio->getTypeName() != "ASIO") {std::cerr << "FAIL: ASIO device type unavailable\n";return 1;}
    juce::AudioDeviceManager manager;
    bool listed = false;
    for(auto* type : manager.getAvailableDeviceTypes()) listed = listed || type->getTypeName() == "ASIO";
    if(!listed) {std::cerr << "FAIL: AudioDeviceManager does not offer ASIO\n";return 1;}
    asio->scanForDevices();
    std::cout << "PASS ASIO device type available; installed drivers: " << asio->getDeviceNames().size() << "\n";
    return 0;
   #endif
}

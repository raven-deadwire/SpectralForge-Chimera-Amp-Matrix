#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

namespace spectralforge::standalone {

// Message-thread only. Keep this reader independent of the Standalone holder so
// tests can drive the real JUCE manager with deterministic device backends.
inline juce::var deviceSnapshot(juce::AudioDeviceManager& devices, bool inputMuted) {
    auto object=std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> types;
    for(auto* type:devices.getAvailableDeviceTypes())types.add(type->getTypeName());
    object->setProperty("type",devices.getCurrentAudioDeviceType());
    object->setProperty("available_types",types);
    object->setProperty("input_muted_for_feedback",inputMuted);
    if(auto* device=devices.getCurrentAudioDevice();device!=nullptr && device->isOpen()) {
        object->setProperty("open",true);
        object->setProperty("running",device->isPlaying());
        object->setProperty("sample_rate",device->getCurrentSampleRate());
        object->setProperty("buffer_samples",device->getCurrentBufferSizeSamples());
        object->setProperty("driver_input_latency_samples",device->getInputLatencyInSamples());
        object->setProperty("driver_output_latency_samples",device->getOutputLatencyInSamples());
        object->setProperty("active_inputs",device->getActiveInputChannels().countNumberOfSetBits());
        object->setProperty("active_outputs",device->getActiveOutputChannels().countNumberOfSetBits());
        const int driverXruns=device->getXRunCount();
        object->setProperty("xruns",driverXruns); // -1 means the driver does not report this.
        // JUCE's manager adds measured callback deadline overruns to the driver
        // counter. Keep the two separate: they may describe the same dropout.
        object->setProperty("callback_overruns",juce::jmax(0,devices.getXRunCount()-juce::jmax(0,driverXruns)));
        object->setProperty("device_cpu_percent",devices.getCpuUsage()*100.);
    } else object->setProperty("open",false);
    return juce::var(object.release());
}

inline void preferAsioOnFirstRun(juce::AudioDeviceManager& devices, const juce::PropertySet* settings) {
    if(settings==nullptr || settings->containsKey("audioSetup"))return;
    const auto current=devices.getCurrentAudioDeviceType();
    const auto previous=devices.getAudioDeviceSetup();
    const auto isRunning=[&] {
        auto* device=devices.getCurrentAudioDevice();
        return device!=nullptr && device->isOpen() && device->isPlaying();
    };
    if(current=="ASIO" && isRunning())return;
    for(auto* type:devices.getAvailableDeviceTypes()) {
        if(type->getTypeName()!="ASIO")continue;
        type->scanForDevices();
        auto names=type->getDeviceNames();
        if(names.isEmpty())return;
        // JUCE enumerates installed ASIO drivers, including disconnected
        // interfaces. Try the backend's default first, then remaining drivers.
        names.move(type->getDefaultDeviceIndex(false),0);
        if(current!="ASIO") {
            devices.setCurrentAudioDeviceType("ASIO",true);
            if(isRunning())return;
            names.remove(0); // The default has already failed during the switch.
        }
        for(const auto& name:names) {
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.inputDeviceName=setup.outputDeviceName=name;
            // Zero rate/buffer asks JUCE for this driver's supported defaults.
            const auto error=devices.setAudioDeviceSetup(setup,true);
            if(error.isEmpty() && isRunning())return;
        }
        devices.setCurrentAudioDeviceType(current,true);
        // Preserve the exact previous rate, buffer and channel selection.
        devices.setAudioDeviceSetup(previous,true);
        return;
    }
}
}

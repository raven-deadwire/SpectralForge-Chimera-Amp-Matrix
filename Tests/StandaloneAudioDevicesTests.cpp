// Real JUCE AudioDeviceManager, deterministic device backends. No hardware or
// driver DLL is opened. These contracts do not measure physical round trips.
#include "StandaloneAudioDevices.h"
#include "StandaloneDiagnostics.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace spectralforge::standalone;
namespace {
void check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
const juce::String privateName="PRIVATE_DEVICE_SERIAL_USER_PATH";
struct Device final : juce::AudioIODevice {
    Device(juce::String name,juce::String type):AudioIODevice(name,type) {}
    ~Device() override {stop();}
    juce::StringArray getOutputChannelNames() override {return {privateName,privateName};}
    juce::StringArray getInputChannelNames() override {return {privateName,privateName};}
    juce::Array<double> getAvailableSampleRates() override {return {44100.,48000.,96000.};}
    juce::Array<int> getAvailableBufferSizes() override {return {64,128,256};}
    int getDefaultBufferSize() override {return 128;}
    juce::String open(const juce::BigInteger& in,const juce::BigInteger& out,double sr,int bs) override {
        if(getName().startsWith("busy"))return "Device busy";
        inputs=in;outputs=out;rate=sr;buffer=bs;opened=true;return {};
    }
    void close() override {stop();opened=false;}
    bool isOpen() override {return opened;}
    void start(juce::AudioIODeviceCallback* c) override {callback=c;if(c)c->audioDeviceAboutToStart(this);}
    void stop() override {auto* c=callback;callback=nullptr;if(c)c->audioDeviceStopped();}
    bool isPlaying() override {return callback!=nullptr;}
    juce::String getLastError() override {return {};}
    int getCurrentBufferSizeSamples() override {return buffer;}
    double getCurrentSampleRate() override {return rate;}
    int getCurrentBitDepth() override {return 24;}
    juce::BigInteger getActiveOutputChannels() const override {return outputs;}
    juce::BigInteger getActiveInputChannels() const override {return inputs;}
    int getOutputLatencyInSamples() override {return 160;}
    int getInputLatencyInSamples() override {return 96;}
    int getXRunCount() const noexcept override {return xruns;}
    void tick() {
        juce::AudioBuffer<float> data(2,buffer);data.clear();
        callback->audioDeviceIOCallbackWithContext(data.getArrayOfReadPointers(),2,data.getArrayOfWritePointers(),2,buffer,{});
    }
    bool opened=false;
    int buffer=128,xruns=3;
    double rate=48000.;
    juce::BigInteger inputs,outputs;
    juce::AudioIODeviceCallback* callback=nullptr;
};
struct Type final : juce::AudioIODeviceType {
    Type(juce::String kind,juce::StringArray list):AudioIODeviceType(kind),names(list) {}
    void scanForDevices() override {++scans;}
    juce::StringArray getDeviceNames(bool =false) const override {return names;}
    int getDefaultDeviceIndex(bool) const override {return 0;}
    int getIndexOfDevice(juce::AudioIODevice* d,bool) const override {return d?names.indexOf(d->getName()):-1;}
    bool hasSeparateInputsAndOutputs() const override {return false;}
    juce::AudioIODevice* createDevice(const juce::String& out,const juce::String& in) override {
        ++creates;const auto name=out.isNotEmpty()?out:in;
        if(name.startsWith("absent"))return nullptr;
        return new Device(name,getTypeName());
    }
    juce::StringArray names;
    int scans=0,creates=0;
};
struct Fixture {
    Fixture(juce::StringArray asio={privateName},bool installAsio=true) {
        manager.addAudioDeviceType(std::make_unique<Type>("WASAPI",juce::StringArray{privateName}));
        if(installAsio){auto t=std::make_unique<Type>("ASIO",asio);asioType=t.get();manager.addAudioDeviceType(std::move(t));}
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName=setup.outputDeviceName=privateName;setup.sampleRate=48000.;setup.bufferSize=256;
        check(manager.initialise(2,2,nullptr,false,{},&setup).isEmpty(),"fixture initialise failed");
    }
    Device& device(){return *dynamic_cast<Device*>(manager.getCurrentAudioDevice());}
    juce::AudioDeviceManager manager;
    juce::PropertySet settings;
    Type* asioType=nullptr;
};
void selectionTests() {
    {Fixture f;preferAsioOnFirstRun(f.manager,&f.settings);
     check(f.manager.getCurrentAudioDeviceType()=="ASIO" && f.device().isOpen(),"first run did not select ASIO");
     check(!f.settings.containsKey("audioSetup"),"preference wrote settings directly");
     auto xml=f.manager.createStateXml();check(xml!=nullptr,"ASIO selection not persisted by JUCE");
     f.settings.setValue("audioSetup",xml.get());
     const auto saved=f.settings.getValue("audioSetup");
     check(f.manager.initialise(2,2,xml.get(),true).isEmpty(),"saved ASIO reload failed");
     const int creates=f.asioType->creates;preferAsioOnFirstRun(f.manager,&f.settings);
     check(f.asioType->creates==creates && f.settings.getValue("audioSetup")==saved,"saved ASIO was changed");}
    {Fixture f;auto setup=f.manager.getAudioDeviceSetup();setup.sampleRate=96000;setup.bufferSize=64;
     setup.useDefaultInputChannels=false;setup.inputChannels.clear();setup.inputChannels.setBit(1);
     check(f.manager.setAudioDeviceSetup(setup,true).isEmpty(),"saved setup failed");
     auto xml=f.manager.createStateXml();f.settings.setValue("audioSetup",xml.get());
     const auto saved=f.settings.getValue("audioSetup");
     check(f.manager.initialise(2,2,xml.get(),true).isEmpty(),"saved WASAPI reload failed");
     const auto before=f.manager.getAudioDeviceSetup();const int scans=f.asioType->scans;
     preferAsioOnFirstRun(f.manager,&f.settings);
     check(f.manager.getCurrentAudioDeviceType()=="WASAPI" && f.manager.getAudioDeviceSetup()==before,
           "saved non-ASIO selection/rate/buffer/channels was changed");
     check(f.asioType->scans==scans && f.settings.getValue("audioSetup")==saved,"saved configuration scanned ASIO or changed XML");}
    for(const auto& value:juce::StringArray{"","invalid xml"}) {
        Fixture f;f.settings.setValue("audioSetup",value);preferAsioOnFirstRun(f.manager,&f.settings);
        check(f.manager.getCurrentAudioDeviceType()=="WASAPI","existing settings key was overridden");
    }
    {Fixture f;preferAsioOnFirstRun(f.manager,nullptr);check(f.manager.getCurrentAudioDeviceType()=="WASAPI","null settings changed device");}
    {Fixture f({},false);preferAsioOnFirstRun(f.manager,&f.settings);check(f.device().isOpen(),"no-ASIO build lost device");}
    {Fixture f(juce::StringArray{});preferAsioOnFirstRun(f.manager,&f.settings);check(f.manager.getCurrentAudioDeviceType()=="WASAPI" && f.device().isOpen(),"empty ASIO list lost device");}
    {Fixture f({"busy driver"});const auto before=f.manager.getAudioDeviceSetup();preferAsioOnFirstRun(f.manager,&f.settings);
     check(f.manager.getCurrentAudioDeviceType()=="WASAPI" && f.device().isOpen() && f.manager.getAudioDeviceSetup()==before,"busy driver did not restore exact prior setup");}
    {Fixture f({"absent driver",privateName});preferAsioOnFirstRun(f.manager,&f.settings);
     check(f.manager.getCurrentAudioDeviceType()=="ASIO" && f.manager.getCurrentAudioDevice()!=nullptr,
           "disconnected default ASIO hid a working second interface");}
    {Fixture f({"busy driver",privateName});preferAsioOnFirstRun(f.manager,&f.settings);
     check(f.manager.getCurrentAudioDeviceType()=="ASIO" && f.manager.getCurrentAudioDevice()!=nullptr,
           "busy default ASIO hid a working second interface");}
    std::cout<<"PASS first run, saved ASIO/WASAPI reload, null/invalid settings, absent/busy/empty ASIO and exact fallback\n";
}
struct SlowCallback final:juce::AudioIODeviceCallback {
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override {juce::Thread::sleep(12);}
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
};
void latencyTests() {
    Fixture f;
    for(double rate:{44100.,48000.,96000.}) {
        auto report=deviceSnapshot(f.manager,false);report.getDynamicObject()->setProperty("sample_rate",rate);
        addReportedLatency(report,2048);
        check(std::abs(double(report["driver_plus_plugin_latency_ms"])-1000.*(96+160+2048)/rate)<1e-9,"latency units/rate/plugin delay mismatch");
    }
    for(double rate:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        auto report=deviceSnapshot(f.manager,false);report.getDynamicObject()->setProperty("sample_rate",rate);
        addReportedLatency(report,2048);check(!report.hasProperty("driver_plus_plugin_latency_ms"),"invalid sample rate produced latency");
    }
    for(const auto* key:{"driver_input_latency_samples","driver_output_latency_samples"}) {
        auto report=deviceSnapshot(f.manager,false);report.getDynamicObject()->setProperty(key,-1);
        addReportedLatency(report,2048);check(!report.hasProperty("driver_plus_plugin_latency_ms"),"unknown driver latency produced an estimate");
        report.getDynamicObject()->removeProperty(key);addReportedLatency(report,2048);
        check(!report.hasProperty("driver_plus_plugin_latency_ms"),"missing driver latency produced an estimate");
    }
    juce::var missing;addReportedLatency(missing,2048);check(missing.isVoid(),"missing device gained latency");
    auto closed=deviceSnapshot(f.manager,false);closed.getDynamicObject()->setProperty("open",false);
    addReportedLatency(closed,2048);check(!closed.hasProperty("driver_plus_plugin_latency_ms"),"closed device gained latency");
    std::cout<<"PASS reported latency arithmetic at 44.1/48/96 kHz and missing/invalid values\n";
}
void snapshotTests() {
    Fixture f;
    check(audioDeviceDiagnostics==nullptr,"plugin/test hook unexpectedly active");
    auto report=deviceSnapshot(f.manager,true);
    check(bool(report["open"]) && double(report["sample_rate"])==48000. && int(report["buffer_samples"])==256,"rate/buffer snapshot mismatch");
    check(int(report["driver_input_latency_samples"])==96 && int(report["driver_output_latency_samples"])==160,"driver latency mismatch");
    check(int(report["active_inputs"])==2 && int(report["active_outputs"])==2 && int(report["xruns"])==3,"channel/driver xrun mismatch");
    check(bool(report["input_muted_for_feedback"]) && report["available_types"].getArray()->size()==2,"mute/types mismatch");
    check(!juce::JSON::toString(report).contains(privateName),"device/channel names leaked");
    auto setup=f.manager.getAudioDeviceSetup();setup.sampleRate=96000;setup.bufferSize=64;
    setup.useDefaultInputChannels=false;setup.inputChannels.clear();setup.inputChannels.setBit(1);
    check(f.manager.setAudioDeviceSetup(setup,true).isEmpty(),"reconfiguration failed");
    report=deviceSnapshot(f.manager,false);
    check(double(report["sample_rate"])==96000. && int(report["buffer_samples"])==64 && int(report["active_inputs"])==1 && !bool(report["input_muted_for_feedback"]),"snapshot stale after rate/buffer/channel/mute change");
    SlowCallback slow;f.manager.addAudioCallback(&slow);
    struct RemoveCallback {juce::AudioDeviceManager& manager;SlowCallback& callback;
        ~RemoveCallback(){manager.removeAudioCallback(&callback);}} remove{f.manager,slow};
    f.device().tick();
    report=deviceSnapshot(f.manager,false);
    check(double(report["device_cpu_percent"])>0 && std::abs(double(report["device_cpu_percent"])-100.*f.manager.getCpuUsage())<1e-9,"device CPU percent not measured/converted");
    check(report.hasProperty("callback_overruns") && int(report["callback_overruns"])>0,"callback deadline miss absent from diagnostics");
    check(int(report["xruns"])==3,"callback overrun conflated with driver xrun");
    f.device().xruns=-1;report=deviceSnapshot(f.manager,false);
    check(int(report["xruns"])==-1 && int(report["callback_overruns"])>0,"unsupported driver xruns hid callback overruns");
    f.manager.removeAudioCallback(&slow);
    f.device().close();report=deviceSnapshot(f.manager,false);
    check(!bool(report["open"]) && !report.hasProperty("sample_rate"),"closed device reported open with stale metrics");
    f.manager.closeAudioDevice();report=deviceSnapshot(f.manager,true);
    check(!bool(report["open"]) && !report.hasProperty("xruns") && !report.hasProperty("device_cpu_percent"),"missing device invented metrics");
    std::cout<<"PASS live snapshot, reconfiguration, privacy, driver xruns vs callback overruns, CPU, closed/missing device\n";
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    int failures=0;
    for(auto test:{selectionTests,snapshotTests,latencyTests})try{test();}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';++failures;}
    return failures?1:0;
}

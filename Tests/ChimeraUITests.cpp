#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message)
{ if (!ok) throw std::runtime_error(message); }
void set(ChimeraProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.parameters().getParameter(id);
    require(parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void checkControls(ChimeraEditor& editor, int mode)
{
    int sliders = 0, toneLabels = 0;
    for (auto* child : editor.getChildren())
    {
        if (!child->isVisible()) continue;
        require(editor.getLocalBounds().contains(child->getBounds()), "Visible control outside editor bounds");
        if (dynamic_cast<juce::Slider*>(child) != nullptr) ++sliders;
        if (auto* label = dynamic_cast<juce::Label*>(child))
        {
            const auto text = label->getText();
            for (auto character : text)
                require(character < 128, "Non-ASCII UI text reintroduced");
            if (text == "BAND TONE") ++toneLabels;
            if (mode == 2)
                require(text != "BASS" && text != "LOW MID" && text != "HIGH MID" &&
                        text != "TREBLE" && text != "PRESENCE" && text != "RESONANCE",
                        "Full-range EQ labels visible in Matrix");
        }
    }
    require(sliders == (mode == 0 ? 8 : mode == 1 ? 16 : 11), "Wrong controls for routing mode");
    require(toneLabels == (mode == 2 ? 3 : 0), "Wrong band tone visibility");
}
void saveSnapshot(ChimeraEditor& editor, const juce::File& directory,
                  const juce::String& name, float scale = 1.0f)
{
    const auto file = directory.getChildFile(name + ".png");
    auto output = file.createOutputStream();
    require(output != nullptr, "Cannot create UI snapshot");
    juce::PNGImageFormat format;
    require(format.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,scale),*output),
            "Cannot write UI snapshot");
}
void checkState()
{
    ChimeraProcessor source;
    set(source,"mode",2); set(source,"bass1",7); set(source,"treble2",-5);
    set(source,"bandtone1",9); set(source,"bandtone2",-6); set(source,"x1",220);
    juce::MemoryBlock data;
    source.getStateInformation(data);
    ChimeraProcessor restored;
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    for (const auto* id : {"mode","bass1","treble2","bandtone1","bandtone2","x1"})
        require(std::abs(source.parameters().getRawParameterValue(id)->load() -
                         restored.parameters().getRawParameterValue(id)->load()) < 0.0001f,
                "State recall lost an EQ or Matrix parameter");
    auto legacy = source.parameters().copyState();
    for (int i=1;i<=3;++i)
        legacy.removeChild(legacy.getChildWithProperty("id","bandtone"+juce::String(i)),nullptr);
    auto xml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml,data);
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    for (int i=1;i<=3;++i)
        require(restored.parameters().getRawParameterValue("bandtone"+juce::String(i))->load() == 0.0f,
                "Legacy state inherited a non-neutral Matrix tone");
}
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseGUI;
    try
    {
        const auto directory = argc > 1 ? juce::File(argv[1])
                                       : juce::File::getCurrentWorkingDirectory().getChildFile("ui-snapshots");
        require(directory.createDirectory().wasOk(), "Cannot create snapshot directory");
        checkState();
        ChimeraProcessor processor;
        processor.setRateAndBufferSizeDetails(48000,256);
        for (int mode=0;mode<3;++mode)
        {
            set(processor,"mode",static_cast<float>(mode));
            ChimeraEditor editor(processor);
            checkControls(editor,mode);
            const juce::String name = mode == 0 ? "Classic" : mode == 1 ? "Dual" : "Matrix";
            saveSnapshot(editor,directory,name);
            if (mode == 2)
            {
                saveSnapshot(editor,directory,"Matrix-150pct",1.5f);
                set(processor,"x1",350); set(processor,"x2",4000);
                juce::Thread::sleep(80);
                juce::Timer::callPendingTimersSynchronously();
                bool lowUpdated = false, highUpdated = false;
                for (auto* child : editor.getChildren())
                    if (auto* label = dynamic_cast<juce::Label*>(child))
                    {
                        lowUpdated = lowUpdated || label->getText() == "INPUT: below 350 Hz";
                        highUpdated = highUpdated || label->getText() == "INPUT: above 4.00 kHz";
                    }
                require(lowUpdated && highUpdated,"Automated crossover labels are stale");
                saveSnapshot(editor,directory,"Matrix-crossovers");
                // Host-driven mode changes must immediately remove hidden controls.
                set(processor,"mode",0);
                juce::Thread::sleep(80);
                juce::Timer::callPendingTimersSynchronously();
                checkControls(editor,0);
                require(processor.parameters().getRawParameterValue("x1")->load()==350,
                        "Switching modes reset saved crossover settings");
            }
        }
        std::cout << "PASS: mode controls, text, automation, state recall, legacy recall; PNGs in "
                  << directory.getFullPathName() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}

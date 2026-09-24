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
    for (auto* child : editor.findChildWithID("surface")->getChildren())
    {
        if (!child->isVisible()) continue;
        require(editor.getLocalBounds().contains(editor.getLocalArea(child,child->getLocalBounds())), "Visible control outside editor bounds");
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
    if (sliders != (mode == 0 ? 18 : mode == 1 ? 29 : 23))
        throw std::runtime_error("Wrong controls for mode " + std::to_string(mode) +
                                 ": found " + std::to_string(sliders));
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
    set(source,"lowcomp",.65f);set(source,"preon",1);set(source,"delayon",1);set(source,"reverbon",1);
    set(source,"mode",2); set(source,"bass1",7); set(source,"treble2",-5);
    set(source,"input",4); set(source,"output",-9); set(source,"gatehold",35); set(source,"gaterelease",140); set(source,"gatethreshold",-57); set(source,"transposeon",1); set(source,"transpose",-5); set(source,"oversampling",3); set(source,"tunerref",442);
    set(source,"bandtone1",9); set(source,"bandtone2",-6); set(source,"x1",220);
    juce::MemoryBlock data;
    source.getStateInformation(data);
    ChimeraProcessor restored;
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    for (const auto* id : {"lowcomp","preon","delayon","reverbon","mode","bass1","treble2","bandtone1","bandtone2","x1","input","output","gatehold","gaterelease","gatethreshold","transposeon","transpose","oversampling","tunerref"})
        require(std::abs(source.parameters().getRawParameterValue(id)->load() -
                         restored.parameters().getRawParameterValue(id)->load()) < 0.0001f,
                "State recall lost an EQ or Matrix parameter");
    auto legacy = source.parameters().copyState();
    for (int i=1;i<=3;++i)
        legacy.removeChild(legacy.getChildWithProperty("id","bandtone"+juce::String(i)),nullptr);
    for(const auto* id:{"lowcomp","cabtype1","cabtype2","cabtype3","input","output","gateon","transposeon","transpose","oversampling","tuneron","tunerref"})
        legacy.removeChild(legacy.getChildWithProperty("id",id),nullptr);
    auto xml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml,data);
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    require(restored.parameters().getRawParameterValue("lowcomp")->load()==0 &&
            restored.parameters().getRawParameterValue("cabtype1")->load()==0 &&
            restored.parameters().getRawParameterValue("gateon")->load()==0 &&
            restored.parameters().getRawParameterValue("output")->load()==0 &&
            restored.parameters().getRawParameterValue("transposeon")->load()==0 &&
            restored.parameters().getRawParameterValue("oversampling")->load()==2,
            "Legacy state inherited new DSP settings");
    for (int i=1;i<=3;++i)
        require(restored.parameters().getRawParameterValue("bandtone"+juce::String(i))->load() == 0.0f,
                "Legacy state inherited a non-neutral Matrix tone");
}
void checkProcessor(const juce::File& directory)
{
    ChimeraProcessor source;
    source.setRateAndBufferSizeDetails(48000,256); source.prepareToPlay(48000,256);
    set(source,"cab1",0); set(source,"gateon",0); set(source,"amp1",0); set(source,"drive1",0); set(source,"output",0);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2,511); // Deliberately larger than the prepared hint.
    const auto level=[&]() {
        double power=0;
        for(int block=0;block<40;++block) {
            for(int n=0;n<511;++n) for(int c=0;c<2;++c)
                buffer.setSample(c,n,.001f*float(std::sin(juce::MathConstants<double>::twoPi*220*(block*511+n)/48000)));
            source.processBlock(buffer,midi);
            if(block>20) for(int n=0;n<511;++n) {const float x=buffer.getSample(0,n);require(std::isfinite(x),"Processor output is non-finite");power+=x*x;}
        }
        return power;
    };
    const auto baseline=level(); set(source,"input",6); const auto input=level();
    require(std::abs(10*std::log10(input/baseline)-6)<.2,"Global input gain is not connected");
    set(source,"input",0); set(source,"output",-12); const auto output=level();
    require(std::abs(10*std::log10(output/baseline)+12)<.2,"Global output gain is not connected");
    const int latency=source.getLatencySamples();
    for(int os=0;os<4;++os) {set(source,"oversampling",float(os));level();require(source.getLatencySamples()==latency,"Oversampling changed host latency");}
    set(source,"transposeon",1); set(source,"transpose",-5); level();
    require(source.getLatencySamples()==latency+source.pitchLatency(),"Pitch latency is not reported to host");
    set(source,"tuneron",1); set(source,"tunermute",1);
    require(level()<1e-12,"Tuner auto-mute is not connected");
    set(source,"tuneron",0); require(level()>1e-9,"Tuner mute did not release");

    // Exercise actual processor serialization with a self-contained user IR.
    const auto irFile=directory.getChildFile("temporary-user-ir.wav");
    juce::AudioBuffer<float> impulse(1,128); impulse.clear(); impulse.setSample(0,0,1); impulse.setSample(0,32,.3f);
    {
        juce::WavAudioFormat format;
        auto stream=irFile.createOutputStream(); require(stream!=nullptr,"Cannot write user IR test file");
        auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),48000,1,24,{},0));
        require(writer!=nullptr,"Cannot create IR writer"); stream.release();
        require(writer->writeFromAudioSampleBuffer(impulse,0,128),"Cannot encode IR");
    }
    require(source.loadIR(0,irFile).wasOk(),"Processor IR load failed");
    set(source,"lowcomp",.42f);set(source,"drive1",.21f);source.copyComparison();source.selectComparison(1);
    set(source,"drive1",.79f);set(source,"cabtype1",2);set(source,"lowcomp",.68f);
    source.selectComparison(0);
    require(std::abs(source.parameters().getRawParameterValue("drive1")->load()-.21f)<1e-5f,"A/B failed to restore amp controls");
    require(source.parameters().getRawParameterValue("cabtype1")->load()==3,"A/B failed to restore user IR selection");
    juce::MemoryBlock state; source.getStateInformation(state); require(irFile.deleteFile(),"Cannot delete source IR");
    ChimeraProcessor restored; restored.setStateInformation(state.getData(),(int)state.getSize());
    restored.setRateAndBufferSizeDetails(48000,256); restored.prepareToPlay(48000,256);
    require(restored.cabStatus(0).contains("temporary-user-ir.wav"),"Project did not restore embedded IR");
    require(restored.parameters().getRawParameterValue("cabtype1")->load()==3,"IR source selection not recalled");
    restored.selectComparison(1);
    require(std::abs(restored.parameters().getRawParameterValue("drive1")->load()-.79f)<1e-5f && restored.parameters().getRawParameterValue("cabtype1")->load()==2,"Saved B slot did not survive project recall");
    restored.selectComparison(0);
    require(restored.cabStatus(0).contains("temporary-user-ir.wav") && std::abs(restored.parameters().getRawParameterValue("lowcomp")->load()-.42f)<1e-5f,"A slot IR or COMP was lost after project recall");
    const auto reference=directory.getChildFile("roundtrip.chimera");require(reference.replaceWithData(state.getData(),state.getSize()),"Could not save reference fixture");
    juce::MemoryBlock fromDisk;require(reference.loadFileAsData(fromDisk) && fromDisk==state,"Reference export/import bytes changed");
    std::cout<<"PASS: global gain, oversized blocks, host latency, tuner mute, embedded IR, A/B and reference recall\n";
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
        checkProcessor(directory);
        ChimeraProcessor processor;
        processor.setRateAndBufferSizeDetails(48000,256);
        processor.prepareToPlay(48000,256);
        processor.learnMidi("output");juce::MidiBuffer cc;cc.addEvent(juce::MidiMessage::controllerEvent(1,40,0),0);juce::AudioBuffer<float> silence(2,256);silence.clear();processor.processBlock(silence,cc);
        require(processor.parameters().getRawParameterValue("output")->load()==-36 && !processor.learningMidi(),"MIDI learn did not map the next CC");
        juce::MemoryBlock midiState;processor.getStateInformation(midiState);ChimeraProcessor recalled;recalled.setStateInformation(midiState.getData(),(int)midiState.getSize());recalled.prepareToPlay(48000,256);cc.clear();cc.addEvent(juce::MidiMessage::controllerEvent(1,40,127),0);recalled.processBlock(silence,cc);require(recalled.parameters().getRawParameterValue("output")->load()==12,"MIDI map was not restored");
        set(processor,"output",-6);
        for (int mode=0;mode<3;++mode)
        {
            set(processor,"mode",static_cast<float>(mode));
            ChimeraEditor editor(processor);
            // Exercise the real asynchronous UI event loop. Sleeping the message
            // thread can prevent parameter notifications and timers from settling.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
            checkControls(editor,mode);
            const juce::String name = mode == 0 ? "Classic" : mode == 1 ? "Dual" : "Matrix";
            saveSnapshot(editor,directory,name);
            if(mode==1) {
                for(auto* child:editor.findChildWithID("surface")->getChildren()) if(auto* slider=dynamic_cast<juce::Slider*>(child);slider && slider->getName()=="dualblend")for(auto* text:slider->getChildren())if(auto* label=dynamic_cast<juce::Label*>(text))require(label->getText()=="50:50","Initial blend readout is not a rig ratio");
                set(processor,"dualtype",1);set(processor,"dualcross",700);juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                int count=0;for(auto* child:editor.findChildWithID("surface")->getChildren())if(child->isVisible() && dynamic_cast<juce::Slider*>(child))++count;
                require(count==19,"Dual crossover has incorrect band controls");saveSnapshot(editor,directory,"Dual-crossover");set(processor,"dualtype",0);
            }
            if (mode == 2)
            {
                saveSnapshot(editor,directory,"Matrix-150pct",1.5f);
                editor.setSize(885,585);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                checkControls(editor,2);
                for(auto* child:editor.findChildWithID("surface")->getChildren()) if(auto* box=dynamic_cast<juce::ComboBox*>(child);box && box->getName()=="Interface size")require(box->getText()=="75%","Size selector did not follow window resizing");
                saveSnapshot(editor,directory,"Matrix-75pct");
                editor.setSize(1180,780);
                set(processor,"x1",350); set(processor,"x2",4000);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                bool lowUpdated = false, highUpdated = false;
                for (auto* child : editor.findChildWithID("surface")->getChildren())
                    if (auto* label = dynamic_cast<juce::Label*>(child))
                    {
                        lowUpdated = lowUpdated || label->getText() == "INPUT: below 350 Hz";
                        highUpdated = highUpdated || label->getText() == "INPUT: above 4.00 kHz";
                    }
                require(lowUpdated && highUpdated,"Automated crossover labels are stale");
                saveSnapshot(editor,directory,"Matrix-crossovers");
                for(const auto* tab:{"PRE","POST"}) {
                    bool clicked=false;
                    for(auto* child:editor.findChildWithID("surface")->getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()==tab) {button->triggerClick();clicked=true;}
                    require(clicked,"Missing FX page tab");juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                    int sliders=0;for(auto* child:editor.findChildWithID("surface")->getChildren()) if(child->isVisible()) {
                        require(editor.getLocalBounds().contains(editor.getLocalArea(child,child->getLocalBounds())),"FX control outside editor");
                        if(dynamic_cast<juce::Slider*>(child)) ++sliders;
                    }
                    require(sliders==(juce::String(tab)=="PRE" ? 23 : 30),"FX module controls have the wrong scope");
                    saveSnapshot(editor,directory,juce::String(tab)=="PRE" ? "Pre-pedalboard" : "Post-rack");
                }
                for(auto* child:editor.findChildWithID("surface")->getChildren()) if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()=="RIGS") button->triggerClick();
                juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                set(processor,"tuneron",1);juce::MessageManager::getInstance()->runDispatchLoopUntil(120);saveSnapshot(editor,directory,"Tuner-global");set(processor,"tuneron",0);
                // Host-driven mode changes must immediately remove hidden controls.
                set(processor,"mode",0);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
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

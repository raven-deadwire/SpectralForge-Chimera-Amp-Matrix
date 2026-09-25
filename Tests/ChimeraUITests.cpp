#include "PluginEditor.h"
#include "HardwareArtwork.h"
#include "SupportPanel.h"
#include "FactoryPresets.h"
#include <map>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void require(bool ok, const char* message)
{ if (!ok) throw std::runtime_error(message); }
void checkArtwork()
{
    using namespace spectralforge::art;
    const auto& images=RasterBank::get().images;
    require(images.size()==static_cast<size_t>(Surface::count) && images.size()==size_t(51+spectralforge::ampModelCount),
            "The complete hardware artwork inventory was not embedded");
    for(size_t i=0;i<images.size();++i) {
        const auto& asset=images[i];
        if(!asset.isValid() || asset.getWidth()<=0 || asset.getHeight()<=0)
            throw std::runtime_error("Hardware raster or matching alpha resource is missing/undecodable: surface "+std::to_string(i));
        const juce::Image::BitmapData pixels(asset,juce::Image::BitmapData::readOnly);
        bool visible=false;
        for(int y=0;y<pixels.height && !visible;++y)
            for(int x=0;x<pixels.width && !visible;++x)visible=pixels.getPixelColour(x,y).getAlpha()!=0;
        require(visible,"Embedded hardware artwork is fully transparent");
    }
    std::set<Surface> heads,pedals,racks;
    for(int model=0;model<spectralforge::ampModelCount;++model)require(heads.insert(headStyle(model).surface).second,"Two amplifier heads share an unrelated artwork surface");
    for(int family:{0,3,4,5,6}) {
        std::set<Surface> familySurfaces;
        require(spectralforge::modelFamilies[(size_t)family].count==5,"A PRE family has fewer than five selectable models");
        for(int model=0;model<5;++model) {
            const auto surface=pedalStyle(family,model).surface;
            if(family==4 && model==2) {
                require(surface==pedalStyle(4,1).surface,"Mu-Tron up/down modes should share their physical enclosure");
            } else {
                require(familySurfaces.insert(surface).second,"Distinct pedal references share one artwork surface");
                require(pedals.insert(surface).second,"Unrelated PRE families share one artwork surface");
            }
        }
        require(familySurfaces.size()==(family==4 ? 4u : 5u),"PRE artwork coverage is incomplete");
    }
    for(int family:{1,2,7,8,9,10})
        for(int model=0;model<spectralforge::modelFamilies[(size_t)family].count;++model)
            require(racks.insert(rackStyle(family,model).surface).second,"Distinct POST references share one artwork surface");
    require(heads.size()==size_t(spectralforge::ampModelCount) && pedals.size()==24 && racks.size()==21,"Model artwork mapping does not cover every reference");
    for(auto surface:pedals)require(!heads.count(surface) && !racks.count(surface),"A pedal is using amp or rack artwork");
    for(auto surface:racks)require(!heads.count(surface),"A rack is using amplifier artwork");
    std::cout<<"PASS: "<<images.size()<<" decoded visible rasters; "<<heads.size()<<" unique heads, 24 PRE enclosures (25 models), 21 unique POST surfaces\n";
    for(const auto* resource:{"chimerawordmark_png","spectralforgeemblem_png"}) {int bytes=0;const auto* data=ChimeraArtworkData::getNamedResource(resource,bytes);require(data && bytes>0 && juce::ImageFileFormat::loadFrom(data,(size_t)bytes).isValid(),"Brand image missing or undecodable");}
    int manualBytes=0;const auto* manual=ChimeraManualData::getNamedResource("MANUAL_html",manualBytes);require(manual && manualBytes>1000 && juce::String::fromUTF8(manual,manualBytes).contains("SpectralForge Chimera"),"Embedded offline manual missing");
}
void set(ChimeraProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.parameters().getParameter(id);
    require(parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void writeIRFixture(const juce::File& file, bool stereo=false)
{
    juce::AudioBuffer<float> impulse(stereo ? 2 : 1,128);impulse.clear();
    for(int c=0;c<impulse.getNumChannels();++c) {
        impulse.setSample(c,0,.8f);impulse.setSample(c,16+8*c,.2f);
    }
    juce::WavAudioFormat format;
    auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot create WAV fixture");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),48000,(unsigned int)impulse.getNumChannels(),24,{},0));
    require(writer!=nullptr,"Cannot encode WAV fixture");stream.release();
    require(writer->writeFromAudioSampleBuffer(impulse,0,impulse.getNumSamples()),"Cannot write WAV fixture samples");
}
void checkDecodedIR(const juce::File& file)
{
    juce::MemoryBlock bytes;juce::String error;
    require(file.loadFileAsData(bytes),"Cannot read discovered IR");
    const auto decoded=spectralforge::IRLibrary::decode(bytes,file.getFileName(),error);
    if(!decoded) throw std::runtime_error("Discovered IR failed application decoder: "+file.getFileName().toStdString()+" / "+error.toStdString());
    require(decoded->samples.getMagnitude(0,decoded->samples.getNumSamples())>0,"Decoded IR has no signal");
}
void checkInstalledIR(const juce::File& expected)
{
    bool found=false;
    for(const auto& entry:spectralforge::IRCollection::scan(spectralforge::IRCollection::roots(),true))
        found=found || (entry.file==expected && entry.ready());
    require(found,"Installed personal IR not discovered by the application library");
    checkDecodedIR(expected);
    ChimeraProcessor processor;CabinetSelector selector;selector.refresh();
    bool selected=false;
    selector.selected=[&](juce::File file,int source) {
        require(source==3 && file==expected,"Cabinet selector resolved the wrong installed IR");
        require(processor.loadIR(0,file).wasOk(),"Installed IR could not be loaded into the rig");selected=true;
    };
    for(int i=0;i<selector.getNumItems();++i)
        if(selector.getItemId(i)>=100 && selector.getItemId(i)<9000 && selector.fileForItemId(selector.getItemId(i))==expected) {
            selector.setSelectedId(selector.getItemId(i),juce::sendNotificationSync);break;
        }
    require(selected,"Installed IR absent from the rig cabinet menu");
    require(processor.parameters().getRawParameterValue("cabtype1")->load()==3 && processor.userIRName(0)==expected.getFileName(),"Installed IR selection was not applied to the rig");
}
void checkControls(ChimeraEditor& editor, int mode)
{
    int sliders = 0, toneLabels = 0;bool lowHeadVisible=false;
    for (auto* child : editor.findChildWithID("surface")->getChildren())
    {
        if (!child->isVisible()) continue;
        require(editor.getLocalBounds().contains(editor.getLocalArea(child,child->getLocalBounds())), "Visible control outside editor bounds");
        if (dynamic_cast<juce::Slider*>(child) != nullptr) ++sliders;
        if(auto* box=dynamic_cast<juce::ComboBox*>(child);box && box->getName()=="Amp 1")lowHeadVisible=true;
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
    if (sliders != (mode == 0 ? 18 : mode == 1 ? 29 : 26))
        throw std::runtime_error("Wrong controls for mode " + std::to_string(mode) +
                                 ": found " + std::to_string(sliders));
    require(mode!=2 || lowHeadVisible,"Matrix LOW head selector hidden");
    require(toneLabels == (mode == 2 ? 3 : 0), "Wrong band tone visibility");
}
void saveSnapshot(juce::Component& editor, const juce::File& directory,
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
    require(source.parameters().getRawParameterValue("preorder")->load()==1,"New session must put the envelope before compression");
    require(source.parameters().getRawParameterValue("gainorder")->load()==0,"New session must put boost before drive");
    set(source,"gainorder",1);
    set(source,"preorder",0);
    set(source,"lowcomp",.65f);set(source,"lowampmix",.61f);set(source,"preon",1);set(source,"delayon",1);set(source,"reverbon",1);
    set(source,"mode",2); set(source,"bass1",7); set(source,"treble2",-5);
    set(source,"input",4); set(source,"output",-9); set(source,"gatehold",35); set(source,"gaterelease",140); set(source,"gatethreshold",-57); set(source,"transposeon",1); set(source,"transpose",-5); set(source,"oversampling",3); set(source,"tunerref",442);
    set(source,"bandtone1",9); set(source,"bandtone2",-6); set(source,"x1",220);
    for(const auto& family:spectralforge::modelFamilies)set(source,family.parameter,float(family.count-1));
    juce::MemoryBlock data;
    source.getStateInformation(data);
    ChimeraProcessor restored;
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    for (const auto* id : {"gainorder","preorder","lowampmix","lowcomp","preon","delayon","reverbon","mode","bass1","treble2","bandtone1","bandtone2","x1","input","output","gatehold","gaterelease","gatethreshold","transposeon","transpose","oversampling","tunerref"})
        require(std::abs(source.parameters().getRawParameterValue(id)->load() -
                         restored.parameters().getRawParameterValue(id)->load()) < 0.0001f,
                "State recall lost an EQ or Matrix parameter");
    for(const auto& family:spectralforge::modelFamilies)require(restored.parameters().getRawParameterValue(family.parameter)->load()==family.count-1,"Selected model not recalled");
    auto legacy = source.parameters().copyState();
    for(const auto& family:spectralforge::modelFamilies)legacy.removeChild(legacy.getChildWithProperty("id",family.parameter),nullptr);
    for (int i=1;i<=3;++i)
        legacy.removeChild(legacy.getChildWithProperty("id","bandtone"+juce::String(i)),nullptr);
    for(const auto* id:{"gainorder","preorder","lowampmix","lowcomp","cabtype1","cabtype2","cabtype3","input","output","gateon","transposeon","transpose","oversampling","tuneron","tunerref"})
        legacy.removeChild(legacy.getChildWithProperty("id",id),nullptr);
    auto xml = legacy.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml,data);
    restored.setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    require(restored.parameters().getRawParameterValue("lowampmix")->load()==0 && restored.parameters().getRawParameterValue("lowcomp")->load()==0 &&
            restored.parameters().getRawParameterValue("preorder")->load()==0 &&
            restored.parameters().getRawParameterValue("gainorder")->load()==0 &&
            restored.parameters().getRawParameterValue("cabtype1")->load()==0 &&
            restored.parameters().getRawParameterValue("gateon")->load()==0 &&
            restored.parameters().getRawParameterValue("output")->load()==0 &&
            restored.parameters().getRawParameterValue("transposeon")->load()==0 &&
            restored.parameters().getRawParameterValue("oversampling")->load()==2,
            "Legacy state inherited new DSP settings");
    for(const auto& family:spectralforge::modelFamilies)require(restored.parameters().getRawParameterValue(family.parameter)->load()==0,"Legacy project inherited a model choice");
    for (int i=1;i<=3;++i)
        require(restored.parameters().getRawParameterValue("bandtone"+juce::String(i))->load() == 0.0f,
                "Legacy state inherited a non-neutral Matrix tone");
}
void checkFactoryPresets()
{
    ChimeraProcessor processor;
    // Device/performance preferences survive switching sounds.
    const std::map<std::string,float> performance{{"input",-3},{"inputmode",1},{"tempo",137},
        {"temposync",1},{"metronome",1},{"tuneron",1},{"tunermute",0},{"tunerref",442}};
    for(const auto& [id,value]:performance) set(processor,id,value);
    for(int index=0;index<spectralforge::factoryPresetCount;++index) {
        std::map<std::string,float> expected;
        require(spectralforge::applyFactoryPreset(index,[&](const char* id,float value) {
            auto* parameter=processor.parameters().getParameter(id);
            if(!parameter) throw std::runtime_error(std::string("Preset refers to an unknown host parameter: ")+id);
            const auto range=parameter->getNormalisableRange();
            if(value<range.start || value>range.end)
                throw std::runtime_error(std::string("Preset parameter is outside its host range: ")+id);
            expected[id]=value;
        }),"Factory preset metadata rejected a valid index");
        // Every module and latent control starts at an unrelated extreme;
        // processor recall must clear previous solos, polarity, FX and pitch.
        for(const auto& [id,value]:expected) {
            auto* parameter=processor.parameters().getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value)==1.f ? 0.f : 1.f);
        }
        processor.loadFactoryPreset(index);
        for(const auto& [id,value]:expected)
            if(std::abs(processor.parameters().getRawParameterValue(id)->load()-value)>juce::jmax(1e-4f,std::abs(value)*2e-6f))
                throw std::runtime_error(std::string("Factory preset host recall mismatch: ")+spectralforge::factoryPresets[(size_t)index].name+" / "+id);
        for(const auto& [id,value]:performance)
            require(std::abs(processor.parameters().getRawParameterValue(id)->load()-value)<juce::jmax(1e-4f,std::abs(value)*2e-6f),
                    "Factory preset changed an input/performance preference");
        processor.loadFactoryPreset(-1);processor.loadFactoryPreset(spectralforge::factoryPresetCount);
        for(const auto& [id,value]:expected)
            require(std::abs(processor.parameters().getRawParameterValue(id)->load()-value)<juce::jmax(1e-4f,std::abs(value)*2e-6f),
                    "Invalid preset ID reset or changed the current sound");
    }
    std::cout<<"PASS: all factory preset host IDs/ranges, complete recall, performance preservation and invalid-index isolation\n";
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
    juce::MemoryBlock originalIR;
    require(irFile.loadFileAsData(originalIR),"Cannot read original IR bytes");
    const auto sidecar=juce::File(irFile.getFullPathName()+".json");require(sidecar.replaceWithText("{\"speaker\":\"Reference V30\",\"cabinet\":\"Reference 4x12\",\"diameter_in\":\"12\",\"microphone\":\"SM57\",\"distance\":\"0.5 in\"}"),"Cannot write IR sidecar");
    require(source.loadIR(0,irFile).wasOk(),"Processor IR load failed");
    require(source.cabMetadata(0).values[2]=="12" && source.cabMetadata(0).values[5]=="0.5 in","IR sidecar was not imported");require(sidecar.deleteFile(),"Cannot remove IR sidecar fixture");
    set(source,"lowcomp",.42f);set(source,"drive1",.21f);set(source,"preorder",1);set(source,"gainorder",1);source.copyComparison();source.selectComparison(1);
    set(source,"drive1",.79f);set(source,"cabtype1",2);set(source,"lowcomp",.68f);set(source,"preorder",0);set(source,"gainorder",0);
    source.selectComparison(0);
    require(std::abs(source.parameters().getRawParameterValue("drive1")->load()-.21f)<1e-5f,"A/B failed to restore amp controls");
    require(source.parameters().getRawParameterValue("cabtype1")->load()==3,"A/B failed to restore user IR selection");
    require(source.parameters().getRawParameterValue("preorder")->load()==1,"A/B failed to restore pedal order");
    require(source.parameters().getRawParameterValue("gainorder")->load()==1,"A/B failed to restore gain order");
    juce::MemoryBlock state; source.getStateInformation(state); require(irFile.deleteFile(),"Cannot delete source IR");
    ChimeraProcessor restored; restored.setStateInformation(state.getData(),(int)state.getSize());
    restored.setRateAndBufferSizeDetails(48000,256); restored.prepareToPlay(48000,256);
    require(restored.cabMetadata(0).values[2]=="12" && restored.cabMetadata(0).values[5]=="0.5 in","Embedded IR metadata was lost");
    require(restored.userIRName(0)==irFile.getFileName(),"Project did not restore embedded IR identity");
    require(restored.cabStatus(0).contains("Reference 4x12"),"Restored IR status did not use the compact metadata label");
    juce::MemoryBlock recalledState, recalledIR;
    restored.getStateInformation(recalledState);
    const auto recalledXml=juce::AudioProcessor::getXmlFromBinary(recalledState.getData(),(int)recalledState.getSize());
    require(recalledXml!=nullptr,"Recalled project state is invalid");
    const auto savedIR=juce::ValueTree::fromXml(*recalledXml).getChildWithName("USER_IRS").getChildWithProperty("lane",0);
    require(recalledIR.fromBase64Encoding(savedIR.getProperty("data").toString()) && recalledIR==originalIR,
            "Project did not preserve embedded IR bytes after the source file was deleted");
    require(restored.parameters().getRawParameterValue("cabtype1")->load()==3,"IR source selection not recalled");
    restored.selectComparison(1);
    require(restored.parameters().getRawParameterValue("preorder")->load()==0,"Saved B pedal order did not survive project recall");
    require(restored.parameters().getRawParameterValue("gainorder")->load()==0,"Saved B gain order did not survive project recall");
    require(std::abs(restored.parameters().getRawParameterValue("drive1")->load()-.79f)<1e-5f && restored.parameters().getRawParameterValue("cabtype1")->load()==2,"Saved B slot did not survive project recall");
    restored.selectComparison(0);
    require(restored.parameters().getRawParameterValue("gainorder")->load()==1,"Saved A gain order did not survive project recall");
    require(restored.userIRName(0)==irFile.getFileName() && std::abs(restored.parameters().getRawParameterValue("lowcomp")->load()-.42f)<1e-5f,"A slot IR or COMP was lost after project recall");
    const auto reference=directory.getChildFile("roundtrip.chimera");require(reference.replaceWithData(state.getData(),state.getSize()),"Could not save reference fixture");
    juce::MemoryBlock fromDisk;require(reference.loadFileAsData(fromDisk) && fromDisk==state,"Reference export/import bytes changed");
    std::cout<<"PASS: global gain, oversized blocks, host latency, tuner mute, embedded IR, A/B and reference recall\n";
}

}
int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI initialiseGUI;
    try
    {
        if(argc==3 && juce::String(argv[1])=="--installed-ir-probe") {
            checkInstalledIR(juce::File(argv[2]));
            std::cout<<"PASS: installed personal IR discovered, audio decoded, selected in the cabinet menu and loaded into a rig\n";return 0;
        }
        if(argc==3 && juce::String(argv[1])=="--personal-ir-pack-probe") {
            struct TemporaryDirectory {
                juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-IR-pack-probe",{},false);
                ~TemporaryDirectory() {folder.deleteRecursively();}
            } temporary;
            const auto catalog=juce::JSON::parse(spectralforge::referenceIRCatalog);
            require(catalog.getArray()!=nullptr,"Reference catalog is invalid");
            const int expected=catalog.getArray()->size();const int externalCount=juce::JSON::parse(spectralforge::externalBassIRCatalog).getArray()->size();int imported=0;
            require(spectralforge::IRCollection::importPersonalPack(juce::File(argv[2]),temporary.folder,imported).wasOk(),"Personal IR ZIP failed import");
            require(imported==expected,"Personal IR ZIP is missing one or more catalog WAVs");
            const auto rows=spectralforge::IRCollection::scan({temporary.folder},true);
            require(rows.size()==(size_t)(expected+2+externalCount),"Imported IRs were duplicated or lost during catalog resolution");
            for(const auto& row:rows) {if(row.external)continue;require(row.ready(),"Imported reference remains unavailable");if(!row.factorySource)checkDecodedIR(row.file);}
            ChimeraProcessor processor;CabinetSelector selector;selector.refresh({temporary.folder});int loaded=0;
            require(selector.installedCount()==expected,"Cabinet menu did not expose the complete imported pack");
            selector.selected=[&](juce::File file,int source) {
                require(source==3 && processor.loadIR(loaded%3,file).wasOk(),"Imported IR failed to load into a rig");++loaded;
            };
            for(int i=0;i<expected;++i)selector.setSelectedId(100+i,juce::sendNotificationSync);
            require(loaded==expected,"Cabinet menu failed to select every imported IR");
            std::cout<<"PASS: "<<expected<<" personal IR WAVs imported, hash matched, decoded and loaded through the cabinet menu; "<<(expected+2)<<" available / "<<rows.size()<<" listed\n";return 0;
        }
        const auto directory = argc > 1 ? juce::File(argv[1])
                                       : juce::File::getCurrentWorkingDirectory().getChildFile("ui-snapshots");
        require(directory.createDirectory().wasOk(), "Cannot create snapshot directory");
        // A/B is two complete sound snapshots, not a stereo channel selector.
        {ChimeraProcessor ab;ab.prepareToPlay(48000,256);set(ab,"cab1",0);set(ab,"drive1",.2f);set(ab,"preampmodel",2);set(ab,"delayon",0);set(ab,"reverbon",0);set(ab,"inputmode",0);ab.copyComparison();ab.selectComparison(1);set(ab,"drive1",.8f);set(ab,"preampmodel",1);ab.selectComparison(0);
         require(ab.parameters().getRawParameterValue("preampmodel")->load()==2,"A/B lost model choice");
         const auto render=[&](bool right){juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;double active=0,other=0;for(int k=0;k<120;++k){block.clear();for(int n=0;n<256;++n)block.setSample(right ? 1 : 0,n,.1f*float(std::sin(juce::MathConstants<double>::twoPi*220*(k*256+n)/48000)));ab.processBlock(block,midi);if(k>60){active+=block.getRMSLevel(right?1:0,0,256);other+=block.getRMSLevel(right?0:1,0,256);}}return std::pair<double,double>{active,other};};
         for(int slot:{0,1,0}){ab.selectComparison(slot);const auto l=render(false),r=render(true);require(l.first>.01 && r.first>.01 && l.second<1e-5 && r.second<1e-5,"A/B changed stereo channel routing");require(std::abs(l.first-r.first)<1e-3,"A/B lost equal left/right gain");}
         require(std::isfinite(ab.cpuLoad()) && ab.cpuLoad()>0 && ab.cpuPeakLoad()>0,"CPU timing meter is inactive");std::cout<<"MEASURE CPU: "<<ab.cpuLoad()<<" percent average, "<<ab.cpuPeakLoad()<<" percent peak (this runner)\n";
        }
        checkArtwork();
        checkState();
        checkFactoryPresets();
        checkProcessor(directory);
        {const auto folder=directory.getChildFile("ir-browser-fixture");require(folder.createDirectory().wasOk(),"Cannot create IR collection fixture");const auto guitar=folder.getChildFile("TEST V30 4x12 SM57.wav"),bass=folder.getChildFile("TEST Bass 8x10 MD421.wav");writeIRFixture(guitar);writeIRFixture(bass,true);checkDecodedIR(guitar);checkDecodedIR(bass);juce::File picked;
         IRBrowserPanel browser(folder,[&](juce::File file){picked=file;});auto* size=dynamic_cast<juce::ComboBox*>(browser.findChildWithID("irdiameter"));auto* list=dynamic_cast<juce::ListBox*>(browser.findChildWithID("irlist"));auto* search=dynamic_cast<juce::TextEditor*>(browser.findChildWithID("irsearch"));require(size && list && search,"IR collection controls missing");
         size->setSelectedId(3,juce::sendNotificationSync);require(list->getListBoxModel()->getNumRows()==1,"10-inch IR filter did not isolate bass fixture");list->selectRow(0);dynamic_cast<juce::TextButton*>(browser.findChildWithID("irload"))->onClick();require(picked==bass,"IR collection loaded wrong file");checkDecodedIR(picked);
         search->setText("SM57");search->onTextChange();require(list->getListBoxModel()->getNumRows()==0,"Mic filter ignored diameter selection");search->clear();search->onTextChange();size->setSelectedId(1,juce::sendNotificationSync);saveSnapshot(browser,directory,"IR-collection");
         auto tags=spectralforge::IRMetadata::filenameHints(bass.getFileName());IRDetailsPanel details(tags,true,[](spectralforge::IRMetadata){});saveSnapshot(details,directory,"IR-details");
         CabinetSelector selector;selector.refresh({folder});require(selector.installedCount()==2,"Cabinet menu must expose both actual WAV fixtures");
         int choices=0,browses=0;selector.selected=[&](juce::File file,int source){require(source==3 && (file==guitar || file==bass),"Installed cabinet selection changed a host enum index");checkDecodedIR(file);++choices;};selector.browse=[&]{++browses;};
         for(int id=100;id<102;++id) {selector.setSelectedId(id,juce::sendNotificationAsync);selector.sync(1,{});juce::MessageManager::getInstance()->runDispatchLoopUntil(20);}
         require(choices==2,"Parameter polling erased an asynchronous cabinet selection");
         selector.setSelectedId(4,juce::sendNotificationSync);require(browses==1 && choices==2,"An empty Project IR must open the library instead of silently selecting an empty slot");
         selector.sync(3,"Embedded take.wav");selector.refresh({folder});require(selector.getText()=="Embedded take","Refreshing the cabinet list lost an embedded project IR name");
         selector.setSelectedId(9000,juce::sendNotificationSync);require(browses==2 && selector.getText()=="Embedded take","Browsing erased the currently loaded IR label");
         int stableSource=-1;selector.selected=[&](juce::File file,int source){require(file==juce::File{},"A built-in cabinet selection unexpectedly returned a path");stableSource=source;};
         selector.setSelectedId(4,juce::sendNotificationSync);require(stableSource==3 && selector.getText()=="Embedded take","Reselecting the current Project IR erased its name");
         for(int id=1;id<=3;++id) {selector.setSelectedId(id,juce::sendNotificationSync);require(stableSource==id-1,"Built-in cabinet automation indices changed");}
         const auto duplicates=folder.getChildFile("duplicates");require(duplicates.createDirectory().wasOk(),"Cannot create duplicate IR fixture");
         const auto sameName=duplicates.getChildFile(bass.getFileName());writeIRFixture(sameName);
         const auto compact=spectralforge::IRCollection::scan({folder},false);juce::StringArray compactNames;
         for(const auto& entry:compact){require(entry.displayName().length()<=56 && !entry.displayName().contains(folder.getFullPathName()),"IR display label is too long or exposes a path");require(!compactNames.contains(entry.displayName()),"Duplicate IR menu names are ambiguous");compactNames.add(entry.displayName());require(entry.details().contains(entry.file.getFileName()),"Original filename missing from capture details");}
         const auto reference=spectralforge::IRMetadata::filenameHints("1970 Bassman Cabinet CTS - SM57 Upper - Cone.wav");require(reference.shortLabel("ignored.wav")==juce::String::fromUTF8("Bassman 2x15 CTS — SM57 cone"),"Known IR compact label was not used");
         require(spectralforge::IRMetadata::leafName("C:\\private\\session\\cab.wav")=="cab.wav","IR restoration leaks Windows paths");
         require(folder.deleteRecursively(),"Cannot remove browser fixtures");}

        {
            IRBrowserPanel browser([](juce::File,int){});
            auto* kind=dynamic_cast<juce::ComboBox*>(browser.findChildWithID("irkind"));
            auto* list=dynamic_cast<juce::ListBox*>(browser.findChildWithID("irlist"));
            require(kind && list,"Reference library controls missing");
            kind->setSelectedId(2,juce::sendNotificationSync);
            require(list->getListBoxModel()->getNumRows()>=2,"Bass references invisible before import");
            list->selectRow(0);require(!dynamic_cast<juce::TextButton*>(browser.findChildWithID("irload"))->isEnabled(),"Missing IR incorrectly loadable");
            saveSnapshot(browser,directory,"IR-bass-reference-library");
            const auto all=spectralforge::IRCollection::scan({},true);
            const auto catalog=juce::JSON::parse(spectralforge::referenceIRCatalog);const auto* entries=catalog.getArray();
            require(entries && entries->size()==26,"Expected twenty-six verified personal capture references");
            const auto external=juce::JSON::parse(spectralforge::externalBassIRCatalog);require(external.getArray() && external.getArray()->size()==12,"Expected twelve external Shift Line references");
            require(all.size()==(size_t)entries->size()+2+external.getArray()->size(),"Factory and reference catalog counts disagree");
            int available=0,karnivore=0,bass=0;
            for(const auto& row:all) {available+=row.ready();karnivore+=row.tags.values[0].containsIgnoreCase("Karnivore");bass+=row.bass();}
            require(available==2 && karnivore==7 && bass==27,"Missing catalog WAVs were counted as installed or capture inventory changed");
            for(const auto& row:all) if(row.reference) require(row.tags.values[9].startsWith("https://"),"Reference source fields are shifted");
            for(const auto& row:all)if(row.external)require(!row.ready() && row.file==juce::File{},"An external bass reference falsely claims an installed WAV");
            require(browser.findChildWithID("irbassdownload")!=nullptr,"Official external bass download button missing");
            const auto invalid=directory.getChildFile("invalid-personal.zip");
            {std::array<char,128> damaged{};juce::ZipFile::Builder zip;zip.addEntry(new juce::MemoryInputStream(damaged.data(),damaged.size(),false),9,"../../DYN 421.wav",juce::Time::getCurrentTime());auto stream=invalid.createOutputStream();require(stream && zip.writeToStream(*stream,nullptr),"Cannot write invalid pack fixture");}
            int count=0;const auto target=directory.getChildFile("pack-import-destination");
            require(spectralforge::IRCollection::importPersonalPack(invalid,target,count).failed() && !target.exists(),"Invalid personal pack was extracted");
            invalid.deleteFile();
            std::cout<<"PASS: bass reference visibility, missing-file state, source metadata and invalid ZIP rejection\n";
        }

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
            if(mode==0) {
                const float original=processor.parameters().getRawParameterValue("amp1")->load();
                for(int model=0;model<spectralforge::ampModelCount;++model) {
                    set(processor,"amp1",(float)model);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
                    auto* reference=dynamic_cast<juce::Label*>(editor.findChildWithID("surface")->findChildWithID("ampreference1"));
                    require(reference && reference->isVisible() && reference->getText()==spectralforge::ampInfo(model).reference,"Amp original reference did not follow model selection");
                    saveSnapshot(editor,directory,"Head-"+juce::String(model+1));
                }
                set(processor,"amp1",original);
            }
            if(mode==1) {
                for(auto* child:editor.findChildWithID("surface")->getChildren()) if(auto* slider=dynamic_cast<juce::Slider*>(child);slider && slider->getName()=="dualblend")for(auto* text:slider->getChildren())if(auto* label=dynamic_cast<juce::Label*>(text))require(label->getText()=="50:50","Initial blend readout is not a rig ratio");
                set(processor,"dualtype",1);set(processor,"dualcross",700);juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                int count=0;for(auto* child:editor.findChildWithID("surface")->getChildren())if(child->isVisible() && dynamic_cast<juce::Slider*>(child))++count;
                require(count==19,"Dual crossover has incorrect band controls");saveSnapshot(editor,directory,"Dual-crossover");set(processor,"dualtype",0);
            }
            if (mode == 2)
            {
                auto* lowMix=dynamic_cast<juce::Slider*>(editor.findChildWithID("surface")->findChildWithID("lowampmix"));
                require(lowMix!=nullptr,"Matrix LOW blend is missing");
                const auto checkLowReadout=[&](const juce::String& expected) {
                    bool found=false;
                    for(auto* child:lowMix->getChildren()) if(auto* label=dynamic_cast<juce::Label*>(child)) {found=true;require(label->getText()==expected,"LOW blend display does not show the AMP percentage");}
                    require(found,"LOW blend value is not visible");
                };
                checkLowReadout("0% AMP");
                set(processor,"amp1",5);set(processor,"amp2",3);set(processor,"amp3",1);set(processor,"lowampmix",.5f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                checkLowReadout("50% AMP");
                saveSnapshot(editor,directory,"Matrix-bass-blend");
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
                    const bool pre=juce::String(tab)=="PRE";
                    for(int variant=1;variant<(pre ? 5 : 6);++variant) {
                        for(const auto& family:spectralforge::modelFamilies)set(processor,family.parameter,float(juce::jmin(variant,family.count-1)));
                        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                        for(const auto& family:spectralforge::modelFamilies) {
                            auto* box=dynamic_cast<juce::ComboBox*>(editor.findChildWithID("surface")->findChildWithID(family.parameter));
                            require(box && box->getNumItems()==family.count && box->getSelectedId()==juce::jmin(variant,family.count-1)+1,"Host model automation did not update every model selector");
                        }
                        saveSnapshot(editor,directory,juce::String(tab)+"-models-"+juce::String(variant+1));
                    }
                    for(const auto& family:spectralforge::modelFamilies)set(processor,family.parameter,0);
                    if(pre) {
                        auto* order=dynamic_cast<juce::ComboBox*>(editor.findChildWithID("surface")->findChildWithID("preorder"));
                        auto* gainOrder=dynamic_cast<juce::ComboBox*>(editor.findChildWithID("surface")->findChildWithID("gainorder"));
                        auto* fuzz=editor.findChildWithID("surface")->findChildWithID("fuzzmodel");auto* boost=editor.findChildWithID("surface")->findChildWithID("boostmodel");auto* drive=editor.findChildWithID("surface")->findChildWithID("drivemodel");
                        require(gainOrder && fuzz && boost && drive,"Gain-order controls missing");
                        for(int value:{1,0}){set(processor,"gainorder",(float)value);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);require(gainOrder->getSelectedId()==value+1 && fuzz->getX()<boost->getX() && fuzz->getX()<drive->getX() && (boost->getX()>drive->getX())==(value==1),"Gain pedal order did not follow automation");saveSnapshot(editor,directory,value ? "PRE-boost-after-drive" : "PRE-boost-before-drive");}
                        auto* envelope=editor.findChildWithID("surface")->findChildWithID("filtermodel");
                        auto* compressor=editor.findChildWithID("surface")->findChildWithID("compmodel");
                        require(order && envelope && compressor,"Pedal order controls are missing");
                        for(int value:{0,1}) {
                            set(processor,"preorder",(float)value);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                            require(order->getSelectedId()==value+1,"Host pedal-order automation did not reach the UI");
                            require((envelope->getX()<compressor->getX())==(value==1),"Pedal positions disagree with the selected audio order");
                            saveSnapshot(editor,directory,value==0 ? "PRE-compressor-first" : "PRE-envelope-first");
                        }
                    }
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
        {auto service=std::make_shared<spectralforge::release::ReleaseSupport>();ChimeraSupportPanel panel(service,{});saveSnapshot(panel,directory,"Support-updates");for(auto* child:panel.getChildren())require(panel.getLocalBounds().contains(child->getBounds()),"Support control outside panel");}
        std::cout << "PASS: mode controls, text, automation, state recall, legacy recall; PNGs in "
                  << directory.getFullPathName() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}

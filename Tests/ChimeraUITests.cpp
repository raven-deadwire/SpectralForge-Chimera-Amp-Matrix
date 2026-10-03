#include "PluginEditor.h"
#include "HardwareArtwork.h"
#include "SupportPanel.h"
#include "FactoryPresets.h"
#include "BoardStateUITests.h"
#include "AmpSelectorTests.h"
#include "AmpSelectionStateTests.h"
#include "CorrectionUITests.h"
#include "PedalMenuTests.h"
#include "NativeStateTests.h"
#include "NativeUITests.h"
#include "UIRefreshTests.h"
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
    const juce::SharedResourcePointer<RasterBank> bank;
    const auto& images=bank->images;
    require(images.size()==static_cast<size_t>(Surface::count) && images.size()==88,
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
    for(int model=26;model<spectralforge::pedalModelCount;++model)
        require(pedals.insert(boardPedalStyle(model).surface).second,"New pedal has no dedicated enclosure image");
    require(heads.size()==size_t(spectralforge::ampModelCount) && pedals.size()==38 && racks.size()==21,"Model artwork mapping does not cover every reference");
    std::set<juce::String> nativeHeads;
    for(int model=spectralforge::legacyAmpModelCount;model<spectralforge::ampModelCount;++model) {
        require(bank->images[(size_t)headStyle(model).surface].isValid(),"New head has no dedicated raster artwork");
        juce::Image image(juce::Image::ARGB,466,170,true);
        // Native Windows images finish their Direct2D frame when Graphics is
        // destroyed. Commit the drawing before PNG encoding or reading pixels.
        {juce::Graphics graphics(image);head(graphics,{0,0,466,170},model);}
        juce::MemoryOutputStream bytes;juce::PNGImageFormat png;require(png.writeImageToStream(image,bytes),"Native head cannot render");
        const auto digest=juce::SHA256(bytes.getData(),bytes.getDataSize()).toHexString();
        require(nativeHeads.insert(digest).second,"New heads share an identical fascia");
        require(image.getPixelAt(image.getWidth()/2,image.getHeight()/2).getAlpha()>0,"Native head artwork is empty");
    }
    require(nativeHeads.size()==8,"Missing new amp artwork");
    for(auto surface:pedals)require(!heads.count(surface) && !racks.count(surface),"A pedal is using amp or rack artwork");
    for(auto surface:racks)require(!heads.count(surface),"A rack is using amplifier artwork");
    std::cout<<"PASS: "<<images.size()<<" decoded visible rasters; "<<heads.size()<<" unique heads, 38 PRE enclosures (39 models), 21 unique POST surfaces\n";
    for(const auto* resource:{"chimerawordmark_png","spectralforgeemblem_png"}) {int bytes=0;const auto* data=ChimeraArtworkData::getNamedResource(resource,bytes);require(data && bytes>0 && juce::ImageFileFormat::loadFrom(data,(size_t)bytes).isValid(),"Brand image missing or undecodable");}
    int manualBytes=0;const auto* manual=ChimeraManualData::getNamedResource("MANUAL_html",manualBytes);require(manual && manualBytes>1000 && juce::String::fromUTF8(manual,manualBytes).contains("SpectralForge Chimera"),"Embedded offline manual missing");
}
void set(ChimeraProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.parameters().getParameter(id);
    require(parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void compatibilityAudio(ChimeraProcessor& processor)
{
    set(processor,"boardEnabled",0);
    for(int context=0;context<spectralforge::ampNativeContextCount;++context)
        set(processor,spectralforge::ampNativeEnabledID(context),0);
    for(int section=0;section<3;++section)set(processor,spectralforge::postNativeModeID(section),0);
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
    const auto processorStorage=std::make_unique<ChimeraProcessor>();auto& processor=*processorStorage;CabinetSelector selector;selector.refresh();
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
    int heads=0,panels=0;
    for (auto* child : editor.findChildWithID("surface")->getChildren())
    {
        if (!child->isVisible()) continue;
        require(editor.getLocalBounds().contains(editor.getLocalArea(child,child->getLocalBounds())), "Visible control outside editor bounds");
        if(dynamic_cast<AmpSelector*>(child))++heads;
        if(dynamic_cast<AmpNativePanel*>(child))++panels;
        if (auto* label = dynamic_cast<juce::Label*>(child))
        {
            const auto text = label->getText();
            // Compact IR labels use Unicode typography. Reject damaged text,
            // not legitimate dashes, arrows or localized names.
            for (auto character : text)
                require(character != 0xfffd && character != 0x7f &&
                        (character >= 32 || character == '\n' || character == '\t'),
                        "Invalid replacement/control character in UI text");
        }
    }
    require(heads==mode+1 && panels==mode+1,"Routing mode does not expose one native head panel per active lane");
    for(int lane=0;lane<3;++lane) {
        auto* panel=correctionUITests::find(editor,"ampNativePanel"+juce::String(lane+1));
        auto* model=correctionUITests::find(editor,"ampSelect"+juce::String(lane+1));
        require(panel && model && panel->isVisible()==(lane<=mode) && model->isVisible()==(lane<=mode),"Native amp visibility disagrees with routing mode");
    }
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
    const auto sourceStorage=std::make_unique<ChimeraProcessor>();auto& source=*sourceStorage;
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
    const auto restoredStorage=std::make_unique<ChimeraProcessor>();auto& restored=*restoredStorage;
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
    const auto processorStorage=std::make_unique<ChimeraProcessor>();auto& processor=*processorStorage;
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
    const auto sourceStorage=std::make_unique<ChimeraProcessor>();auto& source=*sourceStorage;
    compatibilityAudio(source);
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
    set(source,"transposeon",1); set(source,"transpose",0); level();
    require(source.getLatencySamples()==latency,"Zero-semitone transpose adds unnecessary latency");
    set(source,"transpose",-5); level();
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
    const auto restoredStorage=std::make_unique<ChimeraProcessor>();auto& restored=*restoredStorage; restored.setStateInformation(state.getData(),(int)state.getSize());
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

void checkProcessorResourceLifetime(const juce::File& directory)
{
    const auto fixture=directory.getChildFile("lifecycle-switch-ir.wav");writeIRFixture(fixture,true);
    auto processor=std::make_unique<ChimeraProcessor>();
    require(processor->backgroundResourcesReleased(),"New processor already owns background processing resources");
    set(*processor,"mode",2);set(*processor,"gateon",0);set(*processor,"tuneron",1);set(*processor,"tunermute",0);
    double slowestRelease=0;
    for(int cycle=0;cycle<3;++cycle) {
        const double sampleRate=cycle==0?44100.0:cycle==1?48000.0:96000.0;
        const int blockSize=128<<cycle;
        processor->prepareToPlay(sampleRate,blockSize);
        require(!processor->backgroundResourcesReleased(),"Prepare did not recreate the cabinet/tuner resources");
        juce::AudioBuffer<float> audio(2,blockSize);juce::MidiBuffer midi;float peak=0;
        for(int block=0;block<64;++block) {
            if(block%8==0)for(int lane=0;lane<3;++lane) {
                if((block/8)%3==2)require(processor->loadIR(lane,fixture).wasOk(),"Lifecycle IR replacement failed");
                else set(*processor,"cabtype"+juce::String(lane+1),float(1+(block/8+lane)%2));
            }
            for(int channel=0;channel<2;++channel)for(int sample=0;sample<blockSize;++sample)
                audio.setSample(channel,sample,.04f*float(std::sin(juce::MathConstants<double>::twoPi*110*(block*blockSize+sample)/sampleRate)));
            processor->processBlock(audio,midi);
            for(int channel=0;channel<2;++channel)for(int sample=0;sample<blockSize;++sample)
                require(std::isfinite(audio.getSample(channel,sample)),"Reprepared processor produced invalid audio");
            peak=juce::jmax(peak,audio.getMagnitude(0,blockSize));
        }
        require(peak>1.e-6f,"Release/reprepare left the processing path silent");
        // Queue fresh work just before host processing stops. Neither this
        // phase nor resource release dispatches UI messages.
        for(int lane=0;lane<3;++lane)require(processor->loadIR(lane,fixture).wasOk(),"Final lifecycle IR replacement failed");
        const auto start=juce::Time::getMillisecondCounterHiRes();
        processor->releaseResources();
        slowestRelease=juce::jmax(slowestRelease,juce::Time::getMillisecondCounterHiRes()-start);
        require(processor->backgroundResourcesReleased(),"releaseResources left tuner, IR worker or convolution kernels alive");
        processor->releaseResources();
        require(processor->backgroundResourcesReleased(),"Repeated releaseResources recreated background work");
        for(int lane=0;lane<3;++lane)require(processor->userIRName(lane)==fixture.getFileName(),"Resource release discarded the user's IR state");
    }
    require(slowestRelease<2000.0,"Processor release waited too long for background resources");
    require(processor->backgroundResourcesReleased(),"Processor deletion would inherit active background resources");
    processor.reset();require(fixture.deleteFile(),"Lifecycle IR fixture could not be removed");
    std::cout<<"PASS: active tuner/three-lane IR switching, three release/reprepare cycles at 44.1/48/96 kHz, idempotent release and no live background resources before delete; slowest release "<<slowestRelease<<" ms\n";
}

void checkArtworkLifetime()
{
    using Bank=spectralforge::art::RasterBank;
    using SharedBank=juce::SharedResourcePointer<Bank>;
    const auto current=[]()->const Bank* {
        // This temporary observation must not extend the bank's lifetime past
        // the expression being checked, especially across editor destruction.
        const auto bank=SharedBank::getSharedObjectWithoutCreating();
        return bank ? &bank->get() : nullptr;
    };
    require(current()==nullptr,"Artwork exists before any editor owns it");
    auto processorA=std::make_unique<ChimeraProcessor>();
    auto processorB=std::make_unique<ChimeraProcessor>();
    auto editorA=std::make_unique<ChimeraEditor>(*processorA);
    const auto* shared=current();require(shared!=nullptr,"Editor does not retain its artwork resource");
    auto editorB=std::make_unique<ChimeraEditor>(*processorB);
    require(current()==shared,"Simultaneous editors decoded separate artwork banks");
    for(int iteration=0;iteration<3;++iteration) {
        {const auto image=editorA->createComponentSnapshot(editorA->getLocalBounds());require(image.isValid(),"First editor artwork snapshot failed");}
        {const auto image=editorB->createComponentSnapshot(editorB->getLocalBounds());require(image.isValid(),"Second editor artwork snapshot failed");}
        require(current()==shared,"Painting replaced the editor-owned artwork bank");
    }
    editorA.reset();
    require(current()==shared,"Closing one editor released artwork still needed by another editor");
    {const auto image=editorB->createComponentSnapshot(editorB->getLocalBounds());require(image.isValid(),"Remaining editor lost its artwork");}
    require(current()==shared,"Remaining editor rebuilt its shared artwork bank");
    editorB.reset();
    require(current()==nullptr,"Last editor left native artwork alive for DLL shutdown");
    std::cout<<"PASS: two editors share one artwork bank across paints; last editor releases it synchronously before GUI/DLL shutdown\n";
}

void checkEditorLifetime()
{
    const auto started=juce::Time::getMillisecondCounterHiRes();
    for(int iteration=0;iteration<30;++iteration) {
        const auto processorStorage=std::make_unique<ChimeraProcessor>();auto& processor=*processorStorage;
        processor.prepareToPlay(48000,256);
        {
            auto editor=std::make_unique<ChimeraEditor>(processor);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(2);
            // A popup can outlive its target until the queued dismissal runs.
            // Exercise that callback after its editor and controls are gone.
            if(iteration%3==0) {
                for(auto* child:editor->findChildWithID("surface")->getChildren())
                    if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()=="SETTINGS") button->onClick();
            }
        }
        juce::PopupMenu::dismissAllActiveMenus();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(2);
        juce::AudioBuffer<float> audio(2,256);audio.clear();juce::MidiBuffer midi;
        processor.processBlock(audio,midi);
        processor.releaseResources();
    }
    std::cout<<"PASS: 30 editor/pending-popup/processor lifetime cycles in "
             <<juce::Time::getMillisecondCounterHiRes()-started<<" ms (standalone harness, not a DAW shutdown test)\n";
}

void checkHostedSupportLifetime(const juce::File& directory)
{
    const auto checkHostedPanel=[](juce::Component& panel) {
        require(panel.getComponentID()=="supportPanel","Hosted Support panel missing its stable identity");
        auto* updates=dynamic_cast<juce::TextButton*>(panel.findChildWithID("supportUpdates"));
        require(updates && updates->isVisible() && updates->isEnabled() && updates->getButtonText()=="OPEN RELEASES",
                "Hosted Support retained the network updater instead of the browser release action");
        for(const auto* id:{"supportAutomatic","supportDownload","supportReveal","supportCancel"}) {
            auto* control=panel.findChildWithID(id);
            require(control && !control->isVisible(),"Hosted Support exposes an automatic check or downloader control");
        }
        auto* status=dynamic_cast<juce::TextEditor*>(panel.findChildWithID("supportStatus"));
        require(status && status->getText().contains("standalone") && status->getText().contains("browser"),
                "Hosted Support does not explain the available release action");
        for(auto* child:panel.getChildren())if(child->isVisible()) {
            require(panel.getLocalBounds().contains(child->getBounds()),"Hosted Support control is outside its compact panel");
            for(auto* other:panel.getChildren())if(other!=child && other->isVisible())
                require(!child->getBounds().intersects(other->getBounds()),"Hosted Support controls overlap");
        }
    };

    // No service and no message pumping: panel lifetime must be independent of
    // background update checks and of a still-running host event loop.
    for(int iteration=0;iteration<24;++iteration) {
        auto panel=std::make_unique<ChimeraSupportPanel>(nullptr,spectralforge::release::Diagnostics{});
        checkHostedPanel(*panel);
        juce::Component::SafePointer<ChimeraSupportPanel> safe(panel.get());
        panel.reset();require(safe==nullptr,"No-service Support panel did not destroy synchronously");
    }

    double slowestTeardown=0;
    for(int iteration=0;iteration<6;++iteration) {
        auto processor=std::make_unique<ChimeraProcessor>();
        require(processor->wrapperType!=juce::AudioProcessor::wrapperType_Standalone,"Hosted Support fixture accidentally uses standalone mode");
        auto editor=std::make_unique<ChimeraEditor>(*processor);
        auto* canvas=editor->findChildWithID("surface");require(canvas!=nullptr,"Hosted Support editor has no surface");
        juce::TextButton* settings=nullptr;
        for(auto* child:canvas->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()=="SETTINGS")settings=button;
        require(settings && settings->onClick,"Hosted Support settings action missing");
        settings->onClick();
        auto* popup=juce::Component::getCurrentlyModalComponent();require(popup!=nullptr,"Settings did not open its popup");
        // Complete the real asynchronous popup callback with its Support item.
        // None of the browser, report, manual or download actions are clicked.
        popup->exitModalState(6);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
        juce::Component::SafePointer<juce::DialogWindow> dialog(nativeUITests::dialog("supportPanel"));
        require(dialog!=nullptr && dialog->getContentComponent(),"Settings Support action did not open the hosted panel");
        checkHostedPanel(*dialog->getContentComponent());
        if(iteration==0)saveSnapshot(*dialog->getContentComponent(),directory,"Support-hosted");

        const auto start=juce::Time::getMillisecondCounterHiRes();
        editor.reset();
        require(dialog==nullptr,"Closing a hosted editor left the Support dialog alive without a message pump");
        processor.reset();
        const double elapsed=juce::Time::getMillisecondCounterHiRes()-start;
        slowestTeardown=juce::jmax(slowestTeardown,elapsed);
        require(elapsed<2000.0,"Hosted Support teardown waited for background work");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
    std::cout<<"PASS: hosted Support browser-only layout, 24 no-service panel lifetimes and 6 real SETTINGS/Support editor teardowns without a message pump; slowest "
             <<slowestTeardown<<" ms (hosted UI harness, not a Studio One shutdown test)\n";
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
        if(argc==3 && (juce::String(argv[1])=="--personal-ir-pack-probe" || juce::String(argv[1])=="--raven-ir-pack-probe")) {
            struct TemporaryDirectory {
                juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-IR-pack-probe",{},false);
                ~TemporaryDirectory() {folder.deleteRecursively();}
            } temporary;
            const bool raven=juce::String(argv[1])=="--raven-ir-pack-probe";
            const auto catalog=juce::JSON::parse(raven ? spectralforge::ravenIRCatalog : spectralforge::referenceIRCatalog);
            require(catalog.getArray()!=nullptr,"Reference catalog is invalid");
            const int expected=catalog.getArray()->size();int listed=2,imported=0;
            for(const auto* raw:{spectralforge::referenceIRCatalog,spectralforge::externalBassIRCatalog,spectralforge::ravenIRCatalog})listed+=juce::JSON::parse(raw).getArray()->size();
            require(spectralforge::IRCollection::importPersonalPack(juce::File(argv[2]),temporary.folder,imported).wasOk(),"Personal IR ZIP failed import");
            require(imported==expected,"Personal IR ZIP is missing one or more catalog WAVs");
            const auto rows=spectralforge::IRCollection::scan({temporary.folder},true);
            require(rows.size()==(size_t)listed,"Imported IRs were duplicated or lost during catalog resolution");
            int ready=0;
            for(const auto& row:rows) {if(!row.ready())continue;++ready;if(!row.factorySource)checkDecodedIR(row.file);}
            require(ready==expected+2,"Imported reference remains unavailable or uninstalled audio was counted as ready");
            const auto processorStorage=std::make_unique<ChimeraProcessor>();auto& processor=*processorStorage;CabinetSelector selector;selector.refresh({temporary.folder});int loaded=0;
            require(selector.installedCount()==expected,"Cabinet menu did not expose the complete imported pack");
            selector.selected=[&](juce::File file,int source) {
                require(source==3 && processor.loadIR(loaded%3,file).wasOk(),"Imported IR failed to load into a rig");++loaded;
            };
            for(int i=0;i<expected;++i)selector.setSelectedId(100+i,juce::sendNotificationSync);
            require(loaded==expected,"Cabinet menu failed to select every imported IR");
            std::cout<<"PASS: "<<expected<<(raven ? " Raven pack" : " personal")<<" IR WAVs imported, hash matched, decoded and loaded through the cabinet menu; "<<(expected+2)<<" available / "<<rows.size()<<" listed\n";return 0;
        }
        const auto directory = argc > 1 ? juce::File(argv[1])
                                       : juce::File::getCurrentWorkingDirectory().getChildFile("ui-snapshots");
        require(directory.createDirectory().wasOk(), "Cannot create snapshot directory");
        if(argc>2 && juce::String(argv[2])=="--ui-refresh-only"){uiRefreshTests::run(directory);return 0;}
        int suiteFailures=0;
        const auto runSuite=[&](const char* name,auto&& run) {
            try {run();}
            catch(const std::exception& error) {++suiteFailures;std::cerr<<"FAIL suite "<<name<<": "<<error.what()<<'\n';juce::PopupMenu::dismissAllActiveMenus();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);}
        };
        runSuite("processor resource lifetime",[&]{checkProcessorResourceLifetime(directory);});
        runSuite("artwork lifetime",[]{checkArtworkLifetime();});
        runSuite("amp selectors",[]{ampSelectorTests::run();});
        runSuite("amp selection state",[]{ampSelectionStateTests::run();});
        runSuite("pedal menus and power",[]{pedalMenuTests::run();});
        runSuite("native state",[&]{nativeStateTests::run(directory);});
        runSuite("UI refresh",[&]{uiRefreshTests::run(directory);});
        runSuite("native panels",[&]{runNativeUITests(directory);});
        runSuite("correction UI",[&]{runCorrectionUITests(directory);});
        runSuite("editor lifetime",[]{checkEditorLifetime();});
        runSuite("hosted support lifetime",[&]{checkHostedSupportLifetime(directory);});
        runSuite("board state",[&]{runBoardStateTests(directory);});
        // A/B is two complete sound snapshots, not a stereo channel selector.
        {const auto abStorage=std::make_unique<ChimeraProcessor>();auto& ab=*abStorage;compatibilityAudio(ab);ab.prepareToPlay(48000,256);set(ab,"cab1",0);set(ab,"drive1",.2f);set(ab,"preampmodel",2);set(ab,"delayon",0);set(ab,"reverbon",0);set(ab,"inputmode",0);ab.copyComparison();ab.selectComparison(1);set(ab,"drive1",.8f);set(ab,"preampmodel",1);ab.selectComparison(0);
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
         auto* instrument=dynamic_cast<juce::ComboBox*>(browser.findChildWithID("irkind"));auto* availability=dynamic_cast<juce::ComboBox*>(browser.findChildWithID("iravailability"));require(instrument && availability,"Independent IR filters missing");
         instrument->setSelectedId(2,juce::sendNotificationSync);availability->setSelectedId(4,juce::sendNotificationSync);require(list->getListBoxModel()->getNumRows()==1,"Installed bass filter does not combine independently");
         availability->setSelectedId(5,juce::sendNotificationSync);require(list->getListBoxModel()->getNumRows()==0 && instrument->getSelectedId()==2,"Availability selection reset instrument or included installed files");
         availability->setSelectedId(1,juce::sendNotificationSync);instrument->setSelectedId(1,juce::sendNotificationSync);
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
            const auto raven=juce::JSON::parse(spectralforge::ravenIRCatalog);require(raven.getArray() && raven.getArray()->size()==4,"Expected three Raven microphone positions and one V30 comparison");
            require(all.size()==(size_t)entries->size()+2+external.getArray()->size()+raven.getArray()->size(),"Factory and reference catalog counts disagree");
            int available=0,karnivore=0,bass=0;
            for(const auto& row:all) {available+=row.ready();karnivore+=row.tags.values[0].containsIgnoreCase("Karnivore");bass+=row.bass();}
            require(available==2 && karnivore==7 && bass==27,"Missing catalog WAVs were counted as installed or capture inventory changed");
            for(const auto& row:all) if(row.reference) require(row.tags.values[9].startsWith("https://"),"Reference source fields are shifted");
            for(const auto& row:all)if(row.external)require(!row.ready() && row.file==juce::File{},"An external reference falsely claims an installed WAV");
            require(browser.findChildWithID("irbassdownload")!=nullptr,"Official external bass download button missing");
            kind->setSelectedId(1,juce::sendNotificationSync);
            auto* search=dynamic_cast<juce::TextEditor*>(browser.findChildWithID("irsearch"));require(search!=nullptr,"IR search missing");
            search->setText("Raven",false);search->onTextChange();
            require(list->getListBoxModel()->getNumRows()==4,"Raven library search does not expose all three positions and comparison");
            list->selectRow(0);require(!dynamic_cast<juce::TextButton*>(browser.findChildWithID("irload"))->isEnabled(),"Uninstalled Raven capture incorrectly loadable");
            saveSnapshot(browser,directory,"IR-Raven-reference-library");
            const auto invalid=directory.getChildFile("invalid-personal.zip");
            {std::array<char,128> damaged{};juce::ZipFile::Builder zip;zip.addEntry(new juce::MemoryInputStream(damaged.data(),damaged.size(),false),9,"../../DYN 421.wav",juce::Time::getCurrentTime());auto stream=invalid.createOutputStream();require(stream && zip.writeToStream(*stream,nullptr),"Cannot write invalid pack fixture");}
            int count=0;const auto target=directory.getChildFile("pack-import-destination");
            require(spectralforge::IRCollection::importPersonalPack(invalid,target,count).failed() && !target.exists(),"Invalid personal pack was extracted");
            invalid.deleteFile();
            std::cout<<"PASS: bass reference visibility, missing-file state, source metadata and invalid ZIP rejection\n";
        }

        const auto processorStorage=std::make_unique<ChimeraProcessor>();auto& processor=*processorStorage;
        processor.setRateAndBufferSizeDetails(48000,256);
        processor.prepareToPlay(48000,256);
        processor.learnMidi("output");juce::MidiBuffer cc;cc.addEvent(juce::MidiMessage::controllerEvent(1,40,0),0);juce::AudioBuffer<float> silence(2,256);silence.clear();processor.processBlock(silence,cc);
        require(processor.parameters().getRawParameterValue("output")->load()==-36 && !processor.learningMidi(),"MIDI learn did not map the next CC");
        juce::MemoryBlock midiState;processor.getStateInformation(midiState);const auto recalledStorage=std::make_unique<ChimeraProcessor>();auto& recalled=*recalledStorage;recalled.setStateInformation(midiState.getData(),(int)midiState.getSize());recalled.prepareToPlay(48000,256);cc.clear();cc.addEvent(juce::MidiMessage::controllerEvent(1,40,127),0);recalled.processBlock(silence,cc);require(recalled.parameters().getRawParameterValue("output")->load()==12,"MIDI map was not restored");
        set(processor,"output",-6);
        for (int mode=0;mode<3;++mode)
        {
            set(processor,"mode",static_cast<float>(mode));
            ChimeraEditor editor(processor);
            for(auto* child:editor.findChildWithID("surface")->getChildren())
                if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()=="RIGS")button->triggerClick();
            // Exercise the real asynchronous UI event loop. Sleeping the message
            // thread can prevent parameter notifications and timers from settling.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
            checkControls(editor,mode);
            const juce::String name = mode == 0 ? "Classic" : mode == 1 ? "Dual" : "Matrix";
            saveSnapshot(editor,directory,name);
            if(mode==0) {
                const int original=processor.selectedAmpModel(0);
                for(int model=0;model<spectralforge::ampModelCount;++model) {
                    processor.setAmpModel(0,model);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
                    auto* reference=dynamic_cast<juce::Label*>(editor.findChildWithID("surface")->findChildWithID("ampreference1"));
                    require(reference && reference->isVisible() && reference->getText()==juce::String("REFERENCE: ")+spectralforge::ampInfo(model).reference,"Selected amp reference caption did not follow model selection");
                    saveSnapshot(editor,directory,"Head-"+juce::String(model+1));
                }
                processor.setAmpModel(0,original);
            }
            if(mode==1) {
                for(auto* child:editor.findChildWithID("surface")->getChildren()) if(auto* slider=dynamic_cast<juce::Slider*>(child);slider && slider->getName()=="dualblend")for(auto* text:slider->getChildren())if(auto* label=dynamic_cast<juce::Label*>(text))require(label->getText()=="50:50","Initial blend readout is not a rig ratio");
                set(processor,"dualtype",1);set(processor,"dualcross",700);juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                checkControls(editor,1);saveSnapshot(editor,directory,"Dual-crossover");set(processor,"dualtype",0);
            }
            if (mode == 2)
            {
                auto* lowPanel=correctionUITests::find(editor,"ampNativePanel1");
                require(lowPanel!=nullptr,"Matrix LOW native panel missing");
                auto* lowMix=dynamic_cast<juce::Slider*>(correctionUITests::find(*lowPanel,"lowampmix"));
                require(lowMix!=nullptr && lowMix->isVisible(),"Matrix LOW DI/amp control is missing from the native panel");
                require(std::abs(lowMix->getValue())<1.e-5,"Initial LOW DI/amp value changed");
                processor.setAmpModel(0,5);processor.setAmpModel(1,3);processor.setAmpModel(2,1);set(processor,"lowampmix",.5f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                lowMix=dynamic_cast<juce::Slider*>(correctionUITests::find(*lowPanel,"lowampmix"));
                require(lowMix && std::abs(lowMix->getValue()-.5)<1.e-5,"LOW DI/amp native value did not follow host automation");
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
                    auto* canvas=editor.findChildWithID("surface");
                    correctionUITests::clickTab(*canvas,tab);
                    for(auto* child:canvas->getChildren())if(child->isVisible())
                        require(editor.getLocalBounds().contains(editor.getLocalArea(child,child->getLocalBounds())),"FX page control outside editor");
                    if(juce::String(tab)=="PRE") {
                        auto* board=canvas->findChildWithID("universalPedalBoard");require(board && board->isVisible(),"PRE does not display the current five-slot board");
                        for(const auto* id:{"legacyPreControls","boardEnabled","preorder","gainorder"})
                            if(auto* obsolete=canvas->findChildWithID(id))require(!obsolete->isVisible(),"Obsolete PRE control is visible");
                        for(int batch=0;batch<8;++batch) {
                            for(int owner=0;owner<5;++owner)processor.setPedalModel(owner,1+5*batch+owner<spectralforge::pedalModelCount?1+5*batch+owner:0);
                            juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
                            for(int owner=0;owner<5;++owner) {
                                const auto model=processor.pedalBoardState().instances[(size_t)owner].model;
                                auto* box=dynamic_cast<PedalSelector*>(board->findChildWithID("boardModelAt"+juce::String(owner)));
                                require(box && box->getText()==spectralforge::pedalMenuName(model),"Visible pedal selector uses original hardware names");
                            }
                            saveSnapshot(editor,directory,"PRE-types-"+juce::String(batch+1));
                        }
                    } else {
                        for(int section=0;section<6;++section) {
                            auto* button=dynamic_cast<juce::Button*>(correctionUITests::find(*canvas,"postExpand"+juce::String(section)));
                            require(button && button->isVisible(),"POST rack row has no accessible ALL button");
                            if(section<3) {
                                auto* panel=canvas->findChildWithID("postNativePanel"+juce::String(section));require(panel && panel->isVisible() && panel->getHeight()==64,"POST native modules do not share the compact rack overview");
                                auto* model=dynamic_cast<juce::ComboBox*>(correctionUITests::find(*panel,spectralforge::postNativeModelID(section)));
                                require(model && model->getNumItems()==3,"POST native section does not expose three models");
                                for(int choice=0;choice<3;++choice){model->setSelectedId(choice+1,juce::sendNotificationSync);juce::MessageManager::getInstance()->runDispatchLoopUntil(60);saveSnapshot(editor,directory,"POST-native-"+juce::String(section)+"-"+juce::String(choice));}
                            } else {
                                const int family=section==3?10:section==4?1:2;
                                auto* model=dynamic_cast<juce::ComboBox*>(canvas->findChildWithID(spectralforge::modelFamilies[(size_t)family].parameter));
                                require(model && model->isVisible() && model->getNumItems()==spectralforge::modelFamilies[(size_t)family].count,"POST model family is incomplete");
                                for(int choice=0;choice<model->getNumItems();++choice){model->setSelectedId(choice+1,juce::sendNotificationSync);juce::MessageManager::getInstance()->runDispatchLoopUntil(60);require(model->getText()==spectralforge::effectFamilyMenuName(family,choice),"POST effect list exposes original model names");saveSnapshot(editor,directory,"POST-fx-"+juce::String(section)+"-"+juce::String(choice));}
                            }
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
        {
            const auto universalStorage=std::make_unique<ChimeraProcessor>();auto& universal=*universalStorage;universal.prepareToPlay(48000,256);
            const int models[]{30,6,26,27,31};for(int i=0;i<5;++i)universal.setPedalModel(i,models[i]);set(universal,"boardEnabled",1);
            ChimeraEditor editor(universal);auto* canvas=editor.findChildWithID("surface");require(canvas,"Missing editor surface");
            for(auto* child:canvas->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child);button&&button->getButtonText()=="PRE")button->triggerClick();
            for(int mode=0;mode<3;++mode) {
                set(universal,"mode",float(mode));juce::MessageManager::getInstance()->runDispatchLoopUntil(180);
                auto* board=canvas->findChildWithID("universalPedalBoard");require(board&&board->isVisible(),"Product board view not connected");
                for(int i=0;i<5;++i) {auto* box=dynamic_cast<juce::ComboBox*>(board->findChildWithID("boardModelAt"+juce::String(i)));
                    require(box&&box->isVisible()&&box->getSelectedId()==models[i]+1,"Five model selectors do not reflect audio owners");
                    require(board->getLocalBounds().contains(box->getBounds()),"Five pedals do not fit without scrolling");}
                saveSnapshot(editor,directory,"Universal-mode-"+juce::String(mode));
            }
            auto* board=canvas->findChildWithID("universalPedalBoard");
            const auto clickBoardButton=[&](const juce::String& text) {
                for(auto* child:board->getChildren())
                    if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->isVisible() && button->isEnabled() && button->getButtonText()==text) {
                        button->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);return;
                    }
                throw std::runtime_error("Visible board button not found: "+text.toStdString());
            };
            const auto checkDetailPanel=[&](int expectedControls) {
                auto* model=board->findChildWithID("boardModelAt0");
                require(model && !model->isVisible(),"Pedal detail action did not replace the five-card view");
                int sliders=0;bool canReturn=false;
                for(auto* child:board->getChildren())if(child->isVisible()) {
                    require(board->getLocalBounds().contains(child->getBounds()),"Detail control outside the fixed board bounds");
                    require(editor.getLocalBounds().contains(editor.getLocalArea(child,child->getLocalBounds())),"Detail control outside scaled editor");
                    if(dynamic_cast<juce::Slider*>(child))++sliders;
                    if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()=="BACK TO 5 PEDALS")canReturn=button->isEnabled();
                }
                require(canReturn && sliders==expectedControls+1,"Detail panel is missing controls or its return action"); // Includes LOW TAP.
            };
            clickBoardButton("DETAIL / MIDI");checkDetailPanel(3);saveSnapshot(editor,directory,"Universal-detail");
            // Capture every newly added pitch type in the same five-slot board.
            clickBoardButton("BACK TO 5 PEDALS");
            universal.setPedalModel(0,38);universal.setPedalModel(1,39);
            {juce::AudioBuffer<float> audio(2,256);audio.clear();juce::MidiBuffer midi;universal.processBlock(audio,midi);}
            juce::MessageManager::getInstance()->runDispatchLoopUntil(160);saveSnapshot(editor,directory,"Correction-octavers");
            // The largest supported pedal panel must remain usable within the
            // original board bounds at both 100% and 75% UI sizes.
            universal.setPedalModel(0,31);juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
            clickBoardButton("ALL CONTROLS");checkDetailPanel(12);saveSnapshot(editor,directory,"Correction-EQ-detail");
            editor.setSize(885,585);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);checkDetailPanel(12);saveSnapshot(editor,directory,"Correction-EQ-detail-75pct");
            editor.setSize(1180,780);
            clickBoardButton("BACK TO 5 PEDALS");
            const int fourControlModels[]{27,21,19,15,28};
            for(int owner=0;owner<5;++owner)universal.setPedalModel(owner,fourControlModels[owner]);
            set(universal,spectralforge::pedalBypassID(2,19),1);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
            saveSnapshot(editor,directory,"Correction-PRE-four-controls");
            editor.setSize(885,585);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
            saveSnapshot(editor,directory,"Correction-PRE-four-controls-75pct");
            editor.setSize(1180,780);clickBoardButton("DETAIL / MIDI");checkDetailPanel(4);
            saveSnapshot(editor,directory,"Correction-four-knobs-detail");
            editor.setSize(885,585);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);checkDetailPanel(4);
            saveSnapshot(editor,directory,"Correction-four-knobs-detail-75pct");
            editor.setSize(1180,780);
            clickBoardButton("BACK TO 5 PEDALS");
            const int fiveControlModels[]{6,8,12,14,20};
            for(int owner=0;owner<5;++owner)universal.setPedalModel(owner,fiveControlModels[owner]);
            set(universal,spectralforge::pedalBypassID(2,12),1);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
            for(int scale=0;scale<2;++scale) {
                editor.setSize(scale?885:1180,scale?585:780);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
                for(int owner=0;owner<5;++owner)pedalMenuTests::checkFiveKnobLayout(*board,owner,fiveControlModels[owner],&editor);
                saveSnapshot(editor,directory,scale?"Correction-PRE-five-controls-75pct":"Correction-PRE-five-controls");
                clickBoardButton("DETAIL / MIDI");checkDetailPanel(5);
                pedalMenuTests::checkFiveKnobLayout(*board,0,fiveControlModels[0],&editor);
                saveSnapshot(editor,directory,scale?"Correction-five-knobs-detail-75pct":"Correction-five-knobs-detail");
                clickBoardButton("BACK TO 5 PEDALS");
            }
            editor.setSize(1180,780);
            std::cout<<"PASS: actual pedal detail/return actions and all 12 EQ controls inside the fixed board at 100/75 percent\n";
            const auto diagnostic=juce::JSON::parse(universal.diagnosticReport());require(diagnostic.isObject()&&diagnostic["stages"].getArray()&&diagnostic["stages"].getArray()->size()==5,"Diagnostic stage snapshot missing");
            require(!universal.diagnosticReport().contains(directory.getFullPathName()),"Diagnostic leaked a user path");
        }
        {auto service=std::make_shared<spectralforge::release::ReleaseSupport>();ChimeraSupportPanel panel(service,{});saveSnapshot(panel,directory,"Support-updates");for(auto* child:panel.getChildren())require(panel.getLocalBounds().contains(child->getBounds()),"Support control outside panel");}
        require(!juce::SharedResourcePointer<spectralforge::art::RasterBank>::getSharedObjectWithoutCreating(),
                "UI suites retained an artwork bank past all editor/test scopes");
        std::cout << "PASS: mode controls, text, automation, state recall, legacy recall; PNGs in "
                  << directory.getFullPathName() << '\n';
        require(suiteFailures==0,"One or more independent UI suites failed; see named failures above");
        return 0;
    }
    catch (const std::exception& error)
    { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}

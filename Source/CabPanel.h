#pragma once
#include "PluginProcessor.h"
#include "HardwareArtwork.h"
#include "IRBrowserPanel.h"

// Fixed captured IRs. Voicing labels are choices, never inferred coordinates.
class CabPanel : public juce::Component, private juce::Timer {
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    ChimeraProcessor& processor;
    int lane;
    std::vector<juce::File> roots;
    juce::File preferences;
    struct Slot {
        juce::Label title,status,reference;
        juce::ComboBox cabinet, microphone;
        juce::TextButton browse{"IR LIBRARY"}, invert{"POLARITY"};
        std::array<juce::Slider,4> sliders;
        std::array<juce::Label,4> labels;
        std::vector<std::unique_ptr<SA>> attachments;
        std::unique_ptr<BA> polarity;
    };
    std::array<Slot,2> slots;
    std::vector<spectralforge::IRCollection::Entry> entries;
    std::array<std::vector<int>,2> choices;
    juce::Slider blend;
    juce::Label heading,blendLabel;
    std::unique_ptr<SA> blendAttachment;
    juce::Component::SafePointer<juce::DialogWindow> browser;
    std::array<int,2> displayedSource{-1,-1};
    std::array<juce::String,2> displayedName;
    std::array<uint64_t,2> displayedMetadataRevision{};
    static juce::String cabinetName(const spectralforge::IRCollection::Entry& e) {
        if(e.factorySource) return e.factorySource==1 ? "Factory V30" : "Factory Jensen";
        // These filename labels describe the user's supplied pack, not hardware verification.
        for(const auto* name:{"British Straight 4x12","American Twin 2x12","British Alnico 2x12","Brown Deluxe 1x12","Magma Vintage 1x12","Modern Boutique 4x12","Tweed Combo 1x12","British Checkerboard 4x12","Lux-O-Vibe 2x10"})
            if(e.name.startsWithIgnoreCase(name)) return name;
        return e.tags.values[1].isNotEmpty() ? e.tags.values[1] : "User IR / unspecified cabinet";
    }
    static juce::String micName(const spectralforge::IRCollection::Entry& e) {
        const auto name=spectralforge::IRMetadata::leafName(e.name);
        if(const auto* model=e.tags.microphoneModel(e.name))return juce::String(model->alias)+" / "+name;
        return (e.tags.values[3].isNotEmpty() ? e.tags.values[3]+" / " : juce::String{})+name;
    }
    void setSource(int slot,int source) {
        auto* p=processor.parameters().getParameter((slot ? "cabBtype" : "cabtype")+juce::String(lane+1));
        p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(source)));p->endChangeGesture();
    }
    void populate(int slot) {
        auto& s=slots[slot];s.microphone.clear(juce::dontSendNotification);choices[slot].clear();
        const auto selected=s.cabinet.getText();
        juce::StringArray captureLabels;
        for(const auto& entry:entries)captureLabels.add(cabinetName(entry)==selected ? micName(entry) : juce::String{});
        for(int group=0;group<4;++group) {
            bool heading=false;
            for(size_t i=0;i<entries.size();++i) if(cabinetName(entries[i])==selected) {
                const auto* model=entries[i].tags.microphoneModel(entries[i].name);
                if((model ? int(model->kind) : 3)!=group)continue;
                if(!heading) {
                    if(!choices[slot].empty())s.microphone.addSeparator();
                    s.microphone.addSectionHeading(group==3 ? "OTHER / MIXED / UNSPECIFIED" :
                        juce::String(spectralforge::micCatalog::kindLabel(static_cast<spectralforge::micCatalog::Kind>(group))).toUpperCase());
                    heading=true;
                }
                auto label=captureLabels[int(i)];int total=0,ordinal=0;
                for(size_t j=0;j<entries.size();++j)if(captureLabels[int(j)]==label){++total;if(j<=i)++ordinal;}
                if(total>1)label+=" ["+juce::String(ordinal)+"]";
                choices[slot].push_back(int(i));s.microphone.addItem(label,int(choices[slot].size()));
            }
        }
        s.microphone.setText("Select captured mic / voicing",juce::dontSendNotification);
        s.reference.setText({},juce::dontSendNotification);s.microphone.setTooltip("Select an available captured IR. Each choice preserves its full capture filename.");
        if(s.cabinet.getSelectedId()==1 || s.cabinet.getSelectedId()==2) {
            setSource(slot,s.cabinet.getSelectedId()==1 ? 0 : 3);
            displayedSource[slot]=-1;timerCallback();
        }
    }
    void refresh() {
        displayedSource={-1,-1};
        entries=spectralforge::IRCollection::scan(roots,true,nullptr,preferences);
        for(int i=int(entries.size());--i>=0;)if(!entries[size_t(i)].ready())entries.erase(entries.begin()+i);
        juce::StringArray names;for(const auto& e:entries)names.addIfNotAlreadyThere(cabinetName(e));
        for(auto& slot:slots) {
            slot.cabinet.clear(juce::dontSendNotification);slot.cabinet.addItem("Filters only",1);slot.cabinet.addItem("Current project IR",2);
            for(int i=0;i<names.size();++i)slot.cabinet.addItem(names[i],i+3);
            slot.cabinet.setText("Choose cabinet",juce::dontSendNotification);
        }
    }
    void openBrowser(int slot) {
        if(browser) {browser->toFront(true);return;}
        juce::DialogWindow::LaunchOptions options;
        juce::Component::SafePointer<CabPanel> safe(this);
        options.content.setOwned(new IRBrowserPanel([safe,slot](juce::File file,int factory) {
            if(!safe)return;
            if(factory)safe->setSource(slot,factory);
            else {auto result=safe->processor.loadMicIR(safe->lane,slot,file);if(result.failed())safe->slots[slot].status.setText(result.getErrorMessage(),juce::dontSendNotification);}
            safe->refresh();
        }));
        options.dialogTitle="CAB / Mic "+juce::String(slot ? "B" : "A")+" library";
        options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.componentToCentreAround=this;
        browser=options.launchAsync();
    }
    void timerCallback() override {
        for(int i=0;i<2;++i) {
            auto& slot=slots[i];
            slot.status.setText(processor.micStatus(lane,i),juce::dontSendNotification);slot.status.setTooltip(processor.micStatus(lane,i));
            const auto name=processor.micName(lane,i);
            const int source=int(processor.parameters().getRawParameterValue((i ? "cabBtype" : "cabtype")+juce::String(lane+1))->load());
            // Asset/metadata revisions also distinguish equal basenames. Kernel
            // activation alone must not erase a cabinet the user is browsing.
            const auto metadataRevision=processor.micDisplayRevision(lane,i)[0];
            if(source!=displayedSource[i] || name!=displayedName[i] || metadataRevision!=displayedMetadataRevision[i]) {
                displayedSource[i]=source;displayedName[i]=name;displayedMetadataRevision[i]=metadataRevision;
                const auto metadata=processor.micMetadata(lane,i);
                const auto captureName=source==1 || source==2 ? spectralforge::IRMetadata::factoryFilename(source-1) : name;
                const auto* model=metadata.microphoneModel(captureName);
                slot.cabinet.setText(source==0 ? "Filters only" : source==1 ? "Factory V30" : source==2 ? "Factory Jensen" : "Current project IR",juce::dontSendNotification);
                const auto text=source==0 ? juce::String("No speaker IR") : captureName.isEmpty() ? juce::String("No IR loaded") :
                    model ? juce::String(model->alias)+(source==3 ? " / "+captureName : juce::String{}) : captureName;
                slot.microphone.setText(text,juce::dontSendNotification);
                slot.microphone.setTooltip(source==0 ? "Speaker IR bypassed; cabinet filters remain available." : metadata.details(captureName));
                slot.reference.setText(source==0 || captureName.isEmpty() ? juce::String{} : metadata.microphoneReference(captureName),juce::dontSendNotification);
                slot.reference.setTooltip(source==0 || captureName.isEmpty() ? juce::String{} : metadata.details(captureName));
            }
        }
    }
public:
    CabPanel(ChimeraProcessor& p,int rig,const std::vector<juce::File>& folders=spectralforge::IRCollection::roots(),
             const juce::File& settings=spectralforge::IRUserPreferences::file()):processor(p),lane(rig),roots(folders),preferences(settings) {
        const auto n=juce::String(lane+1);
        heading.setText("CAB PANEL / RIG "+n+"   |   Fixed IR / captured voicing",juce::dontSendNotification);addAndMakeVisible(heading);
        for(int i=0;i<2;++i) {
            auto& s=slots[i];s.title.setText(i ? "MIC B" : "MIC A",juce::dontSendNotification);
            for(juce::Component* c:std::initializer_list<juce::Component*>{&s.title,&s.status,&s.reference,&s.cabinet,&s.microphone,&s.browse,&s.invert})addAndMakeVisible(c);
            const auto componentPrefix=juce::String(i ? "cabB" : "cabA");
            s.cabinet.setComponentID(componentPrefix+"cabinet"+n);s.microphone.setComponentID(componentPrefix+"mic"+n);
            s.reference.setComponentID(componentPrefix+"reference"+n);s.reference.setFont(juce::FontOptions(11.f));
            s.reference.setColour(juce::Label::textColourId,juce::Colour(0xffa6b2bb));
            s.cabinet.onChange=[this,i]{populate(i);};
            s.microphone.onChange=[this,i] {
                const int index=slots[i].microphone.getSelectedId()-1;
                if(index<0 || index>=int(choices[i].size()))return;
                const auto& e=entries[size_t(choices[i][size_t(index)])];
                if(e.factorySource)setSource(i,e.factorySource);
                else {auto result=processor.loadMicIR(lane,i,e.file);if(result.failed())slots[i].status.setText(result.getErrorMessage(),juce::dontSendNotification);}
                displayedSource[i]=-1;timerCallback();
            };
            s.browse.onClick=[this,i]{openBrowser(i);};
            const auto prefix=juce::String(i ? "cabB" : "cabA");
            const std::array<juce::String,4> ids{prefix+"gain"+n,prefix+"delay"+n,(i ? "cabBlow" : "cablow")+n,(i ? "cabBhigh" : "cabhigh")+n};
            const std::array<const char*,4> labels{"Level (dB)","Signal delay (ms)","Low cut (Hz)","High cut (Hz)"};
            for(int k=0;k<4;++k) {
                s.labels[k].setText(labels[k],juce::dontSendNotification);addAndMakeVisible(s.labels[k]);
                auto& slider=s.sliders[k];slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,80,24);addAndMakeVisible(slider);
                s.attachments.push_back(std::make_unique<SA>(processor.parameters(),ids[k],slider));
            }
            s.invert.setComponentID(prefix+"invert"+n);s.invert.setClickingTogglesState(true);
            s.invert.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff21667a));
            s.invert.setTooltip("Invert this microphone's signal polarity");
            s.polarity=std::make_unique<BA>(processor.parameters(),prefix+"invert"+n,s.invert);
        }
        blendLabel.setText("Mic A  <  Linear blend  >  Mic B",juce::dontSendNotification);addAndMakeVisible(blendLabel);addAndMakeVisible(blend);
        blend.setSliderStyle(juce::Slider::LinearHorizontal);blend.setTextBoxStyle(juce::Slider::TextBoxRight,false,80,24);
        blendAttachment=std::make_unique<SA>(processor.parameters(),"cabblend"+n,blend);
        blend.setComponentID("cabblend"+n);
        blend.textFromValueFunction=[](double value){return juce::String(value*100.0,1)+"% B";};
        blend.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*.01;};blend.updateText();
        refresh();timerCallback();setSize(900,475);startTimerHz(10);
    }
    ~CabPanel() override {stopTimer();if(browser)delete browser.getComponent();}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff141b22));}
    void resized() override {
        heading.setBounds(16,10,getWidth()-32,30);
        const int width=(getWidth()-48)/2;
        for(int i=0;i<2;++i) {
            auto& s=slots[i];const int x=16+i*(width+16);
            s.title.setBounds(x,45,width,25);s.cabinet.setBounds(x,76,width,28);s.microphone.setBounds(x,112,width,28);
            s.reference.setBounds(x,142,width,22);
            s.browse.setBounds(x,175,130,27);s.invert.setBounds(x+145,175,110,27);
            for(int k=0;k<4;++k){s.labels[k].setBounds(x,215+k*36,130,28);s.sliders[k].setBounds(x+132,215+k*36,width-132,28);}
            s.status.setBounds(x,362,width,35);
        }
        blendLabel.setBounds(16,405,290,28);blend.setBounds(310,405,getWidth()-326,28);
    }
};

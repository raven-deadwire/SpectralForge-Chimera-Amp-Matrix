#pragma once
#include "PluginProcessor.h"
#include "OriginalCabControls.h"
#include "HardwareArtwork.h"
#include "IRBrowserPanel.h"
#include "CapturedCabArt.h"

// Fixed IR choices and an explicitly separate original modeled path.
class CabPanel : public juce::Component, private juce::Timer {
public:
    enum class View { cabinet,irLoader };
private:
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    class CabinetLook : public spectralforge::art::DialogLook {
    public:
        CabinetLook() {
            setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
            setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe4e2dc));
            setColour(juce::Slider::trackColourId,juce::Colour(0xff565b59));
            setColour(juce::Slider::thumbColourId,juce::Colour(0xffc4ad84));
            setColour(juce::Slider::backgroundColourId,juce::Colour(0xff2a2e2e));
        }
        void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,
                             float start,float end,juce::Slider& slider) override {
            const auto area=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(7.f);
            const auto centre=area.getCentre();const float radius=juce::jmin(area.getWidth(),area.getHeight())*.5f;
            const bool slotB=slider.getComponentID().contains("B");
            const auto ink=juce::Colour(slotB ? 0xffc4a678 : 0xffaec2c7).withMultipliedAlpha(slider.isEnabled() ? 1.f : .27f);
            juce::Path track;track.addCentredArc(centre.x,centre.y,radius,radius,0.f,start,end,true);
            g.setColour(juce::Colour(0xff333738));g.strokePath(track,juce::PathStrokeType(3.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            juce::Path value;const float angle=start+(end-start)*position;
            value.addCentredArc(centre.x,centre.y,radius,radius,0.f,start,angle,true);
            g.setColour(ink);g.strokePath(value,juce::PathStrokeType(2.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            const float inner=radius*.78f;
            g.setGradientFill({juce::Colour(0xff343839),centre.x,centre.y-inner,juce::Colour(0xff171a1b),centre.x,centre.y+inner,false});
            g.fillEllipse(centre.x-inner,centre.y-inner,inner*2.f,inner*2.f);
            g.setColour(juce::Colour(0xff414647));g.drawEllipse(centre.x-inner,centre.y-inner,inner*2.f,inner*2.f,1.f);
            g.setColour(ink);
            g.drawLine(centre.x+std::sin(angle)*inner*.36f,centre.y-std::cos(angle)*inner*.36f,
                centre.x+std::sin(angle)*inner*.88f,centre.y-std::cos(angle)*inner*.88f,2.f);
        }
    } look;
    std::unique_ptr<OriginalCabControls> originalControls;
    ChimeraProcessor& processor;
    int lane;
    std::vector<juce::File> roots;
    juce::File preferences;
    struct Slot {
        juce::Label title,status,reference,provenance,microphoneCaption;
        juce::ComboBox cabinet, microphone;
        spectralforge::capturedCabArt::CabinetView cabinetImage;
        spectralforge::capturedCabArt::MicrophoneView microphoneImage;
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
    juce::TextButton cabinetTab{"CABINET"},irLoaderTab{"IR LOADER"};
    View currentView{View::cabinet};
    std::unique_ptr<SA> blendAttachment;
    juce::Component::SafePointer<juce::DialogWindow> browser;
    std::array<int,2> displayedSource{-1,-1};
    std::array<int,2> displayedMode{-1,-1};
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
        auto* original=processor.parameters().getParameter(spectralforge::originalCabID(lane,slot ? "Bon" : "Aon"));
        original->beginChangeGesture();original->setValueNotifyingHost(0);original->endChangeGesture();
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
        bool artworkChanged=false;
        for(int i=0;i<2;++i) {
            auto& slot=slots[i];
            const bool modeled=processor.parameters().getRawParameterValue(spectralforge::originalCabID(lane,i ? "Bon" : "Aon"))->load()>.5f;
            slot.title.setText(juce::String(i ? "MIC B" : "MIC A")+(modeled ? " / CABINET MIC" : " / CAPTURED IR"),juce::dontSendNotification);
            slot.provenance.setText(modeled ? "Stored capture / select an IR to use it" : "Captured cabinet and microphone",juce::dontSendNotification);
            const auto status=processor.micStatus(lane,i);
            slot.status.setText(modeled ? status.replace(" v1","") : status,juce::dontSendNotification);slot.status.setTooltip(status);
            slot.cabinetImage.setActive(!modeled);slot.microphoneImage.setActive(!modeled);
            const auto name=processor.micName(lane,i);
            const int source=int(processor.parameters().getRawParameterValue((i ? "cabBtype" : "cabtype")+juce::String(lane+1))->load());
            // Asset/metadata revisions also distinguish equal basenames. Kernel
            // activation alone must not erase a cabinet the user is browsing.
            const auto metadataRevision=processor.micDisplayRevision(lane,i)[0];
            if(source!=displayedSource[i] || name!=displayedName[i] || metadataRevision!=displayedMetadataRevision[i] || int(modeled)!=displayedMode[i]) {
                artworkChanged=true;
                displayedMode[i]=int(modeled);
                displayedSource[i]=source;displayedName[i]=name;displayedMetadataRevision[i]=metadataRevision;
                const auto metadata=processor.micCaptureMetadata(lane,i);
                const auto captureName=source==1 || source==2 ? spectralforge::IRMetadata::factoryFilename(source-1) : name;
                const auto* model=metadata.microphoneModel(captureName);
                slot.cabinetImage.setCapture(metadata,source);
                slot.microphoneImage.setCapture(metadata,captureName,source);
                slot.microphoneCaption.setText(spectralforge::capturedCabArt::microphoneCaption(metadata,captureName,source),juce::dontSendNotification);
                slot.cabinet.setText(source==0 ? "Filters only" : source==1 ? "Factory V30" : source==2 ? "Factory Jensen" : "Current project IR",juce::dontSendNotification);
                const auto text=source==0 ? juce::String("No speaker IR") : captureName.isEmpty() ? juce::String("No IR loaded") :
                    model ? juce::String(model->alias)+(source==3 ? " / "+captureName : juce::String{}) : captureName;
                slot.microphone.setText(text,juce::dontSendNotification);
                slot.microphone.setTooltip(source==0 ? "Speaker IR bypassed; cabinet filters remain available." : metadata.details(captureName));
                slot.reference.setText(source==0 || captureName.isEmpty() ? juce::String{} : metadata.microphoneReference(captureName)
                    +(model && model->original ? " / CABINET MIC" : ""),juce::dontSendNotification);
                slot.reference.setTooltip(source==0 || captureName.isEmpty() ? juce::String{} : metadata.details(captureName));
            }
        }
        if(artworkChanged && currentView==View::irLoader){resized();repaint();}
    }
public:
    View getView() const noexcept {return currentView;}
    void finishInteraction() {if(originalControls)originalControls->getScene().endMicDrag();}
    void selectInternalView() {setView(View::cabinet);}
    void setView(View next) {
        currentView=next;const bool internal=next==View::cabinet;
        cabinetTab.setToggleState(internal,juce::dontSendNotification);
        irLoaderTab.setToggleState(!internal,juce::dontSendNotification);
        if(originalControls) {
            if(!internal)finishInteraction();
            originalControls->setVisible(internal);
        }
        for(auto& s:slots) {
            for(juce::Component* c:std::initializer_list<juce::Component*>{&s.reference,&s.provenance,&s.cabinet,&s.microphone,&s.browse,&s.cabinetImage,&s.microphoneImage,&s.microphoneCaption})
                c->setVisible(!internal);
            s.title.setFont(juce::FontOptions(internal ? 11.f : 13.f,juce::Font::bold));
            for(int i=0;i<4;++i) {
                const bool rotary=internal && i<2;
                auto& control=s.sliders[size_t(i)];
                control.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
                control.setTextBoxStyle(rotary ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,false,rotary ? 72 : internal ? 54 : 80,21);
                s.labels[size_t(i)].setJustificationType(rotary ? juce::Justification::centred : juce::Justification::centredLeft);
                s.labels[size_t(i)].setFont(juce::FontOptions(internal ? 10.f : 12.f));
            }
        }
        resized();repaint();
    }
    CabPanel(ChimeraProcessor& p,int rig,const std::vector<juce::File>& folders=spectralforge::IRCollection::roots(),
             const juce::File& settings=spectralforge::IRUserPreferences::file()):processor(p),lane(rig),roots(folders),preferences(settings) {
        setLookAndFeel(&look);
        const auto n=juce::String(lane+1);
        heading.setText("CABINET / RIG "+n,juce::dontSendNotification);addAndMakeVisible(heading);
        heading.setFont(juce::FontOptions(19.f,juce::Font::bold));
        for(auto* tab:{&cabinetTab,&irLoaderTab}) {
            addAndMakeVisible(*tab);tab->setClickingTogglesState(false);
            tab->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff1a1d1e));
            tab->setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff39382f));
            tab->setColour(juce::TextButton::textColourOnId,juce::Colour(0xffe4cfaa));
        }
        cabinetTab.setComponentID("cabViewCabinet");irLoaderTab.setComponentID("cabViewIRLoader");
        cabinetTab.onClick=[this]{setView(View::cabinet);};irLoaderTab.onClick=[this]{setView(View::irLoader);};
        for(int i=0;i<2;++i) {
            auto& s=slots[i];s.title.setText(i ? "MIC B" : "MIC A",juce::dontSendNotification);
            for(juce::Component* c:std::initializer_list<juce::Component*>{&s.title,&s.status,&s.reference,&s.provenance,&s.cabinet,&s.microphone,&s.browse,&s.invert,&s.microphoneCaption})addAndMakeVisible(c);
            addAndMakeVisible(s.cabinetImage);addAndMakeVisible(s.microphoneImage);
            const auto componentPrefix=juce::String(i ? "cabB" : "cabA");
            s.cabinetImage.setComponentID(componentPrefix+"cabinetImage"+n);s.microphoneImage.setComponentID(componentPrefix+"micImage"+n);
            s.microphoneCaption.setComponentID(componentPrefix+"micCaption"+n);
            s.microphoneCaption.setFont(juce::FontOptions(11.f));s.microphoneCaption.setJustificationType(juce::Justification::centred);
            s.microphoneCaption.setColour(juce::Label::textColourId,juce::Colour(0xffc1c8c1));
            s.cabinet.setComponentID(componentPrefix+"cabinet"+n);s.microphone.setComponentID(componentPrefix+"mic"+n);
            s.reference.setComponentID(componentPrefix+"reference"+n);s.reference.setFont(juce::FontOptions(11.f));
            s.title.setComponentID(componentPrefix+"title"+n);s.provenance.setComponentID(componentPrefix+"provenance"+n);
            s.status.setComponentID(componentPrefix+"status"+n);
            s.title.setFont(juce::FontOptions(13.f,juce::Font::bold));
            s.title.setColour(juce::Label::textColourId,juce::Colour(i ? 0xffc4a678 : 0xff7bb6c7));
            s.provenance.setFont(juce::FontOptions(11.f));s.status.setFont(juce::FontOptions(11.f));
            s.provenance.setColour(juce::Label::textColourId,juce::Colour(0xff99aab5));
            s.status.setColour(juce::Label::textColourId,juce::Colour(0xffb5c3cb));
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
                s.labels[k].setFont(juce::FontOptions(12.f));
                auto& slider=s.sliders[k];slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,80,24);addAndMakeVisible(slider);
                s.attachments.push_back(std::make_unique<SA>(processor.parameters(),ids[k],slider));
                slider.setComponentID(ids[k]);
                slider.textFromValueFunction=[k](double value){return juce::String(value,k<2 ? 2 : 0);};
                slider.updateText();
            }
            s.invert.setComponentID(prefix+"invert"+n);s.invert.setClickingTogglesState(true);
            s.invert.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff21667a));
            s.invert.setTooltip("Invert this microphone's signal polarity");
            s.polarity=std::make_unique<BA>(processor.parameters(),prefix+"invert"+n,s.invert);
        }
        blendLabel.setText("MIC A  /  BLEND  /  MIC B",juce::dontSendNotification);addAndMakeVisible(blendLabel);addAndMakeVisible(blend);
        blendLabel.setFont(juce::FontOptions(12.f,juce::Font::bold));
        blend.setSliderStyle(juce::Slider::LinearHorizontal);blend.setTextBoxStyle(juce::Slider::TextBoxRight,false,80,24);
        blendAttachment=std::make_unique<SA>(processor.parameters(),"cabblend"+n,blend);
        blend.setComponentID("cabblend"+n);
        blend.textFromValueFunction=[](double value){return juce::String(value*100.0,1)+"% B";};
        blend.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*.01;};blend.updateText();
        originalControls=std::make_unique<OriginalCabControls>(processor,lane);addAndMakeVisible(*originalControls);
        refresh();timerCallback();setSize(1040,748);setView(View::cabinet);startTimerHz(10);
    }
    ~CabPanel() override {stopTimer();if(browser)delete browser.getComponent();setLookAndFeel(nullptr);}
    void visibilityChanged() override {if(!isShowing())finishInteraction();}
    void paint(juce::Graphics& g) override {
        g.setGradientFill({juce::Colour(0xff1b1e1f),0,0,juce::Colour(0xff121516),0,float(getHeight()),false});g.fillAll();
        g.setColour(juce::Colour(0xffc4a678));g.fillRect(16,43,44,1);
        g.setColour(juce::Colour(0xff3a3e3c));g.drawHorizontalLine(49,16.f,float(getWidth()-16));
        if(currentView==View::irLoader) {
            const int width=(getWidth()-48)/2;
            for(int i=0;i<2;++i) {
                const auto area=juce::Rectangle<float>(float(16+i*(width+16)),75.f,float(width),float(getHeight()-137));
                g.setColour(juce::Colour(0xff1b2428));g.fillRoundedRectangle(area,8.f);
                g.setColour(juce::Colour(i ? 0xffa58961 : 0xff568fa4).withAlpha(.55f));g.drawRoundedRectangle(area.reduced(.5f),8.f,1.f);
                const auto stage=juce::Rectangle<float>(area.getX()+12.f,area.getY()+63.f,area.getWidth()-24.f,206.f);
                g.setGradientFill({juce::Colour(0xff141d20),stage.getTopLeft(),juce::Colour(0xff303e3d),stage.getBottomRight(),false});g.fillRoundedRectangle(stage,6.f);
                g.setColour(juce::Colour(0xff6d7f79).withAlpha(.18f));g.drawHorizontalLine(int(stage.getBottom()-45.f),stage.getX()+8.f,stage.getRight()-8.f);
            }
        } else {
            const int side=OriginalCabControls::sideWidth(getWidth());
            g.setColour(juce::Colour(0xff4a4e4d).withAlpha(.36f));
            g.drawVerticalLine(side+6,83.f,605.f);g.drawVerticalLine(getWidth()-side-6,83.f,605.f);
            for(const int x:{20,getWidth()-side}) {
                g.drawHorizontalLine(345,float(x),float(x+side-20));
                g.drawHorizontalLine(465,float(x),float(x+side-20));
            }
        }
        g.setColour(juce::Colour(0xff343939));g.drawHorizontalLine(getHeight()-57,20.f,float(getWidth()-20));
    }
    void resized() override {
        heading.setBounds(16,9,285,31);
        cabinetTab.setBounds(getWidth()/2-141,11,136,28);irLoaderTab.setBounds(getWidth()/2+5,11,136,28);
        if(originalControls)originalControls->setBounds(0,52,getWidth(),622);
        if(currentView==View::irLoader) {
            const int width=(getWidth()-48)/2;
            // One camera for both captures; an 8x10 must remain taller than a
            // 1x15 and a microphone must retain its own physical dimensions.
            float stageHeight=1.36f,stageWidth=1.18f;
            for(const auto& slot:slots) {
                const auto cabinet=slot.cabinetImage.physicalSize();
                stageHeight=juce::jmax(stageHeight,cabinet.height+.04f);
                stageWidth=juce::jmax(stageWidth,cabinet.width+.4f);
            }
            const float scale=juce::jmin(152.f/stageHeight,float(width-48)/stageWidth);
            for(int i=0;i<2;++i) {
                auto& s=slots[size_t(i)];const int x=16+i*(width+16),y=75;
                s.title.setBounds(x+16,y+12,width-32,24);s.provenance.setBounds(x+16,y+39,width-32,22);
                const int stageWidthPixels=width-32;
                s.cabinetImage.setBounds(x+16,y+68,stageWidthPixels,196);
                s.cabinetImage.setStageScale(scale);
                s.microphoneImage.setBounds(x+16,y+68,stageWidthPixels,165);
                const auto cabinet=s.cabinetImage.artworkBounds();
                const auto microphone=s.microphoneImage.physicalSize();
                const auto body=juce::Rectangle<float>(microphone.width*scale,microphone.height*scale)
                    .withPosition(cabinet.getRight()+.10f*scale,cabinet.getCentreY()-microphone.height*scale*.5f);
                s.microphoneImage.setStage(body,cabinet.getBottom()+.024f*scale,scale);
                const int captionWidth=juce::jmax(100,stageWidthPixels*3/10);
                s.microphoneCaption.setBounds(x+16+stageWidthPixels-captionWidth,y+231,captionWidth,31);
                s.cabinet.setBounds(x+16,y+280,width-32,28);s.microphone.setBounds(x+16,y+317,width-32,28);
                s.reference.setBounds(x+16,y+347,width-32,22);
                s.browse.setBounds(x+16,y+378,140,28);s.invert.setBounds(x+171,y+378,116,28);
                for(int k=0;k<4;++k) {
                    s.labels[size_t(k)].setBounds(x+16,y+422+k*38,118,26);
                    s.sliders[size_t(k)].setBounds(x+140,y+422+k*38,width-156,26);
                }
                s.status.setBounds(x+16,y+572,width-32,31);
            }
        } else {
            const int side=OriginalCabControls::sideWidth(getWidth());
            for(int i=0;i<2;++i) {
                auto& s=slots[size_t(i)];const int x=i ? getWidth()-side : 20,width=side-20,knob=(width-8)/2;
                s.title.setBounds(x,59,width,24);
                for(int k=0;k<2;++k) {
                    s.sliders[size_t(k)].setBounds(x-1+k*(knob+8),390,knob,91);
                    s.labels[size_t(k)].setBounds(x-1+k*(knob+8),482,knob,19);
                }
                for(int k=2;k<4;++k) {
                    s.labels[size_t(k)].setBounds(x,505+(k-2)*50,width,19);
                    s.sliders[size_t(k)].setBounds(x,525+(k-2)*50,width,26);
                }
                s.invert.setBounds(x,607,width,27);s.status.setBounds(x,642,width,42);
            }
        }
        blendLabel.setBounds(20,getHeight()-42,215,28);blend.setBounds(235,getHeight()-42,getWidth()-255,28);
    }
};

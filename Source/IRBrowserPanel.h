#pragma once
#include "IRCollection.h"

class IRBrowserPanel : public juce::Component, private juce::ListBoxModel {
    using Entry=spectralforge::IRCollection::Entry;
    std::vector<Entry> entries;
    std::vector<size_t> visible;
    std::vector<juce::File> folders;
    bool libraryMode{};
    juce::TextEditor search;
    juce::ComboBox diameter,kind;
    juce::ListBox list{"IR library",this};
    juce::TextButton load{"LOAD INTO RIG"},importPack{"IMPORT PERSONAL ZIP"},addFolder{"ADD FOLDER"},oneFile{"OPEN IR"},source{"SOURCE PAGE"};
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<void(juce::File,int)> selected;
    const Entry* selection() const {
        const int row=list.getSelectedRow();
        return row>=0 && row<(int)visible.size() ? &entries[visible[(size_t)row]] : nullptr;
    }
    int getNumRows() override { return (int)visible.size(); }
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if(row<0 || row>=(int)visible.size()) return;
        const auto& e=entries[visible[(size_t)row]];
        g.fillAll(active ? juce::Colour(0xff38352e) : juce::Colour(0xff181a1a));
        g.setColour(e.ready() ? juce::Colour(0xffbca479) : juce::Colour(0xff666c6b));
        g.fillEllipse(10,14,5,5);
        g.setFont(juce::FontOptions(12.f));
        g.drawText(e.name,24,4,width-32,22,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffa6aaa7)); g.setFont(juce::FontOptions(10.5f));
        const auto badge=e.factorySource ? "FACTORY" : e.ready() ? "INSTALLED" : "IMPORT REQUIRED";
        g.drawText(juce::String(badge)+" / "+(e.bass()?"BASS":"GUITAR / OTHER")+" / "+e.tags.summary(),24,28,width-32,height-30,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int) override {
        const auto* e=selection(); load.setEnabled(e && e->ready());
        source.setEnabled(e && e->tags.values[9].startsWith("https://")); repaint();
    }
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override { commit(); }
    void commit() {
        const auto* e=selection(); if(!e || !e->ready()) return;
        selected(e->file,e->factorySource);
        if(auto* window=findParentComponentOfClass<juce::DialogWindow>()) window->exitModalState(0);
    }
    void filter() {
        visible.clear();
        const auto query=search.getText().trim();
        const auto inches=diameter.getSelectedId()==1 ? juce::String{} : diameter.getText().upToFirstOccurrenceOf(" in",false,false);
        int available=0;
        for(size_t i=0;i<entries.size();++i) {
            const auto& e=entries[i]; if(e.ready()) ++available;
            const auto text=e.name+" "+juce::JSON::toString(e.tags.json(),true);
            if((query.isEmpty() || text.containsIgnoreCase(query)) && (inches.isEmpty() || e.tags.values[2]==inches)
                && (kind.getSelectedId()!=2 || e.bass()) && (kind.getSelectedId()!=3 || !e.bass())
                && (kind.getSelectedId()!=4 || e.ready())) visible.push_back(i);
        }
        list.deselectAllRows(); list.updateContent();
        status.setText(juce::String(available)+" available / "+juce::String(entries.size())+" listed. Missing reference IRs need your personal ZIP.",juce::dontSendNotification);
        selectedRowsChanged(-1);
    }
    void refresh() { entries=spectralforge::IRCollection::scan(folders,libraryMode); filter(); }
    void choose(int action) {
        chooser=std::make_unique<juce::FileChooser>(action==0 ? "Import Chimera personal IR ZIP" : action==1 ? "Add an IR folder" : "Open cabinet IR",juce::File{},action==0 ? "*.zip" : action==1 ? "" : "*.wav;*.aif;*.aiff");
        const juce::Component::SafePointer<IRBrowserPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|(action==1 ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),[safe,action](const juce::FileChooser& choice) {
            if(!safe || choice.getResult()==juce::File{}) return;
            const auto file=choice.getResult(); juce::Result result=juce::Result::ok();
            if(action==0) {int count=0;result=spectralforge::IRCollection::importPersonalPack(file,spectralforge::IRCollection::userRoot().getChildFile("IRs"),count);}
            else if(action==1) result=spectralforge::IRCollection::rememberFolder(file);
            else {safe->selected(file,0);if(auto* window=safe->findParentComponentOfClass<juce::DialogWindow>()) window->exitModalState(0);return;}
            if(result.failed()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"IR library",result.getErrorMessage(),"OK",safe);
            safe->folders=spectralforge::IRCollection::roots();safe->refresh();
        });
    }
    void initialise() {
        search.setComponentID("irsearch");diameter.setComponentID("irdiameter");kind.setComponentID("irkind");load.setComponentID("irload");list.setComponentID("irlist");
        search.setTextToShowWhenEmpty("Speaker, mic, cone position or creator",juce::Colours::grey);search.onTextChange=[this]{filter();};
        diameter.addItemList({"All sizes","8 in","10 in","12 in","15 in","18 in"},1);diameter.setSelectedId(1);diameter.onChange=[this]{filter();};
        kind.addItemList({"All cabinets","Bass","Guitar / other","Installed only"},1);kind.setSelectedId(1);kind.onChange=[this]{filter();};
        for(juce::Component* c:std::initializer_list<juce::Component*>{&search,&diameter,&kind,&list,&load,&status,&source}) addAndMakeVisible(c);
        if(libraryMode) for(auto* c:{&importPack,&addFolder,&oneFile}) addAndMakeVisible(c);
        importPack.onClick=[this]{choose(0);};addFolder.onClick=[this]{choose(1);};oneFile.onClick=[this]{choose(2);};
        source.onClick=[this]{if(const auto* e=selection()) if(e->tags.values[9].startsWith("https://"))juce::URL(e->tags.values[9]).launchInDefaultBrowser();};
        list.setRowHeight(59);list.setColour(juce::ListBox::backgroundColourId,juce::Colour(0xff171919));
        load.onClick=[this]{commit();};status.setColour(juce::Label::textColourId,juce::Colour(0xffa6aaa7));
        setSize(1000,590);refresh();
    }
public:
    explicit IRBrowserPanel(std::function<void(juce::File,int)> callback):folders(spectralforge::IRCollection::roots()),libraryMode(true),selected(std::move(callback)) {initialise();}
    IRBrowserPanel(const juce::File& folder,std::function<void(juce::File)> callback):folders{folder},selected([callback=std::move(callback)](juce::File file,int){callback(file);}) {initialise();}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101212));g.setColour(juce::Colour(0xffd4c7ad));g.setFont(juce::FontOptions(22.f));
        g.drawText("CABINET LIBRARY",20,14,340,32,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff252827));g.fillRoundedRectangle(722,105,258,422,5);
        if(const auto* e=selection()) {
            spectralforge::art::cabinet(g,{789,118,124,146},e->tags);
            g.setFont(juce::FontOptions(11.f));g.setColour(juce::Colour(0xffaaa89f));
            g.drawText("CABINET STYLE / NOT CAPTURE PHOTO",732,267,238,18,juce::Justification::centred);
            const auto field=[&](const char* key,const juce::String& value,int y) {g.setColour(juce::Colour(0xffaaa89f));g.drawText(key,736,y,224,17,juce::Justification::centredLeft);g.setColour(juce::Colour(0xffe2dbcc));g.drawFittedText(value.isEmpty()?"Unknown / not documented":value,736,y+18,228,30,juce::Justification::topLeft,2);};
            field("CABINET",e->tags.values[1],292);field("MICROPHONE",e->tags.values[3],350);field("CONE / UNIT POSITION",e->tags.values[4],408);
            g.setColour(juce::Colour(0xffaaa89f));g.drawFittedText("Distance: "+(e->tags.values[5].isEmpty()?juce::String("unknown"):e->tags.values[5])+"\nAngle: "+(e->tags.values[6].isEmpty()?juce::String("unknown"):e->tags.values[6]),736,468,228,48,juce::Justification::topLeft,3);
        } else {g.setFont(juce::FontOptions(13.f));g.setColour(juce::Colour(0xffb7b5ae));g.drawFittedText("Select a cabinet to inspect its speaker, microphone and capture position.",744,220,213,100,juce::Justification::centred,4);}
    }
    void resized() override {
        importPack.setBounds(472,19,193,27);addFolder.setBounds(675,19,147,27);oneFile.setBounds(832,19,148,27);
        search.setBounds(20,64,470,28);diameter.setBounds(500,64,140,28);kind.setBounds(650,64,330,28);
        list.setBounds(20,105,685,422);status.setBounds(20,541,565,28);source.setBounds(598,541,180,28);load.setBounds(790,541,190,28);
    }
};

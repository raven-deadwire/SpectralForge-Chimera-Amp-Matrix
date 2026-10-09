#pragma once
#include "IRCollection.h"

class IRBrowserPanel : public juce::Component, private juce::ListBoxModel {
    spectralforge::art::DialogLook look;
    using Entry=spectralforge::IRCollection::Entry;
    std::vector<Entry> entries;
    std::vector<size_t> visible;
    std::vector<juce::File> folders;
    bool libraryMode{};
    juce::File preferences{spectralforge::IRUserPreferences::file()};
    juce::TextEditor search;
    juce::ComboBox diameter,kind,availability,microphone,importType,selectedType;
    spectralforge::IRCollection::ScanReport scanReport;
    juce::ListBox list{"IR library",this};
    juce::TextButton load{"LOAD INTO RIG"},addFolder{"ADD FOLDER"},oneFile{"OPEN IR"},source{"SOURCE PAGE"},remove{"REMOVE FROM LIST"};
    juce::Label status,microphoneReference,microphoneSupport;
    juce::TextEditor captureDetails;
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<void(juce::File,int)> selected;
    const Entry* selection() const {
        const int row=list.getSelectedRow();
        return row>=0 && row<(int)visible.size() ? &entries[visible[(size_t)row]] : nullptr;
    }
    const spectralforge::micCatalog::Model* microphoneFilter() const {
        const int index=microphone.getSelectedId()-100;
        return index>=0 && index<int(spectralforge::micCatalog::models.size()) ? &spectralforge::micCatalog::models[size_t(index)] : nullptr;
    }
    int getNumRows() override { return (int)visible.size(); }
    juce::String getTooltipForRow(int row) override {return row>=0 && row<(int)visible.size() ? entries[visible[(size_t)row]].details() : juce::String{};}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if(row<0 || row>=(int)visible.size()) return;
        const auto& e=entries[visible[(size_t)row]];
        g.fillAll(active ? juce::Colour(0xff38352e) : juce::Colour(0xff181a1a));
        g.setColour(e.ready() ? juce::Colour(0xffbca479) : juce::Colour(0xff666c6b));
        g.fillEllipse(10,14,5,5);
        g.setColour(juce::Colour(0xffe2dbcc));
        g.setFont(juce::FontOptions(12.f));
        g.drawText(e.displayName(),24,4,width-32,22,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffa6aaa7)); g.setFont(juce::FontOptions(10.5f));
        const auto badge=e.factorySource ? "FACTORY" : e.validationError.isNotEmpty() ? "INVALID FILE" : e.ready() ? "INSTALLED" : "NOT AVAILABLE";
        g.drawText(juce::String(badge)+" / "+spectralforge::IRMetadata::instrumentLabel(e.tags.instrument)+" / "+e.tags.values[4],24,28,width-32,height-30,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int) override {
        const auto* e=selection(); load.setEnabled(e && e->ready());
        source.setEnabled(e && e->tags.values[9].startsWith("https://"));
        remove.setEnabled(e && e->removable());selectedType.setEnabled(e && e->removable());
        selectedType.setSelectedId(e ? (int)e->tags.instrument+1 : 0,juce::dontSendNotification);
        source.setButtonText("SOURCE PAGE");
        captureDetails.setText(e ? e->details() : "Select an IR to inspect the original filename and full capture details.",false);repaint();
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
        const auto* mic=microphoneFilter();
        const auto micId=mic ? juce::String(mic->id) : microphone.getSelectedId()==2 ? juce::String("other") : juce::String{};
        microphoneReference.setText(mic ? (mic->original ? "Chimera original / Condenser" : mic->reference) :
            "Filter captured IRs by microphone",juce::dontSendNotification);
        microphoneSupport.setText(mic ? spectralforge::micCatalog::responseLabel(*mic) :
            "Captured IR library / sound comes from the selected file",juce::dontSendNotification);
        int available=0;
        for(size_t i=0;i<entries.size();++i) {
            const auto& e=entries[i]; if(e.ready()) ++available;
            if(spectralforge::IRCollection::matches(e,query,inches,
                (spectralforge::IRCollection::Instrument)juce::jmax(0,kind.getSelectedId()-1),
                (spectralforge::IRCollection::Availability)juce::jmax(0,availability.getSelectedId()-1),micId)) visible.push_back(i);
        }
        list.deselectAllRows(); list.updateContent();
        auto summary=juce::String(visible.size())+" shown / "+juce::String(available)+" ready in library.";
        if(scanReport.invalidFiles>0)summary+=" "+juce::String(scanReport.invalidFiles)+" invalid.";
        summary+=scanReport.truncated ? " Scan limit: 512 files; additional files omitted. Select a smaller folder." :
            visible.empty() && mic ? " No matching capture. Import an IR or adjust the filters." : " Missing entries require original files.";
        status.setText(summary,juce::dontSendNotification);
        selectedRowsChanged(-1);
    }
    void refresh(const juce::File& preserve={}) {
        entries=spectralforge::IRCollection::scan(folders,libraryMode,&scanReport,preferences);filter();
        if(preserve!=juce::File{})for(size_t i=0;i<visible.size();++i)if(entries[visible[i]].file==preserve){list.selectRow((int)i);break;}
    }
    void showFailure(const juce::Result& result) {
        if(result.failed())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"IR library",result.getErrorMessage(),"OK",this);
    }
    void choose(bool folder) {
        chooser=std::make_unique<juce::FileChooser>(folder ? "Add an IR folder" : "Open cabinet IR",juce::File{},folder ? "" : "*.wav;*.aif;*.aiff");
        const bool keepExisting=importType.getSelectedId()==1;
        const auto instrument=importType.getSelectedId()==2 ? spectralforge::IRMetadata::Instrument::bass : importType.getSelectedId()==3 ? spectralforge::IRMetadata::Instrument::guitar : spectralforge::IRMetadata::Instrument::unspecified;
        const juce::Component::SafePointer<IRBrowserPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|(folder ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),[safe,folder,instrument,keepExisting](const juce::FileChooser& choice) {
            if(!safe || choice.getResult()==juce::File{})return;
            const auto file=choice.getResult();juce::Result result=juce::Result::ok();
            if(folder) {
                result=spectralforge::IRUserPreferences::update(file,"folders",keepExisting ? juce::String{} : spectralforge::IRMetadata::instrumentKey(instrument),false,false,safe->preferences);
                if(result.wasOk())result=spectralforge::IRCollection::rememberFolder(file);
            } else result=spectralforge::IRCollection::rememberFile(file,instrument,safe->preferences,keepExisting);
            safe->showFailure(result);
            if(result.failed())return;
            safe->folders=spectralforge::IRCollection::roots();safe->refresh(file);
            if(!folder) {
                safe->selected(file,0);
                if(auto* window=safe->findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(0);
            }
        });
    }
    void initialise() {
        setLookAndFeel(&look);
        search.setComponentID("irsearch");diameter.setComponentID("irdiameter");kind.setComponentID("irkind");availability.setComponentID("iravailability");load.setComponentID("irload");list.setComponentID("irlist");status.setComponentID("irstatus");
        microphone.setComponentID("irmicrophone");microphoneReference.setComponentID("irmicrophonereference");
        microphoneSupport.setComponentID("irmicrophonesupport");
        microphoneSupport.setFont(juce::FontOptions(11.f));
        microphoneSupport.setColour(juce::Label::textColourId,juce::Colour(0xffa6aaa7));
        addAndMakeVisible(microphoneSupport);
        microphone.addItem("All microphones",1);
        for(const auto family:{spectralforge::micCatalog::Kind::dynamic,spectralforge::micCatalog::Kind::ribbon,spectralforge::micCatalog::Kind::condenser}) {
            microphone.addSeparator();microphone.addSectionHeading(juce::String(spectralforge::micCatalog::kindLabel(family)).toUpperCase());
            for(size_t i=0;i<spectralforge::micCatalog::models.size();++i)if(spectralforge::micCatalog::models[i].kind==family)
                microphone.addItem(spectralforge::micCatalog::models[i].alias,100+int(i));
        }
        microphone.addSeparator();microphone.addItem("Other / mixed / unspecified",2);microphone.setSelectedId(1);
        microphone.onChange=[this]{filter();};
        microphone.setTooltip("Filter cabinet captures by microphone. Load an available capture to apply its sound.");
        microphoneReference.setFont(juce::FontOptions(11.f));microphoneReference.setColour(juce::Label::textColourId,juce::Colour(0xffa6aaa7));
        search.setTextToShowWhenEmpty("Speaker, mic, cone position or creator",juce::Colours::grey);search.onTextChange=[this]{filter();};
        diameter.addItemList({"All sizes","8 in","10 in","12 in","15 in","18 in"},1);diameter.setSelectedId(1);diameter.onChange=[this]{filter();};
        kind.addItemList({"All instruments","Bass","Guitar","Unspecified"},1);kind.setSelectedId(1);kind.onChange=[this]{filter();};
        availability.addItemList({"All statuses","Ready to load","Factory","Installed files"},1);
        availability.addItem("Invalid files",7);availability.setSelectedId(1);availability.onChange=[this]{filter();};
        for(juce::Component* c:std::initializer_list<juce::Component*>{&search,&diameter,&kind,&availability,&microphone,&microphoneReference,&list,&load,&status,&source}) addAndMakeVisible(c);
        addAndMakeVisible(captureDetails);captureDetails.setComponentID("ircapturedetails");captureDetails.setMultiLine(true);captureDetails.setReadOnly(true);captureDetails.setScrollbarsShown(true);captureDetails.setCaretVisible(false);
        captureDetails.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff252827));captureDetails.setColour(juce::TextEditor::textColourId,juce::Colour(0xffe2dbcc));captureDetails.setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);captureDetails.setFont(juce::FontOptions(11.f));
        if(libraryMode)for(juce::Component* c:std::initializer_list<juce::Component*>{&addFolder,&oneFile,&importType})addAndMakeVisible(c);
        importType.setComponentID("irimporttype");importType.addItemList({"Import: keep existing type","Import as: Bass","Import as: Guitar","Import as: Unspecified"},1);importType.setSelectedId(1);
        importType.setTooltip("Keep existing metadata by default; unknown files stay unspecified. Choose Bass or Guitar to classify the next file or folder. Individual files can be reclassified below.");
        addFolder.setComponentID("iraddfolder");oneFile.setComponentID("iropenfile");
        addFolder.onClick=[this]{choose(true);};oneFile.onClick=[this]{choose(false);};
        addAndMakeVisible(selectedType);selectedType.setComponentID("irselectedtype");
        selectedType.addItemList({"Type: Unspecified","Type: Bass","Type: Guitar"},1);selectedType.setTextWhenNothingSelected("Select an imported IR");
        selectedType.onChange=[this] {
            if(const auto* e=selection()) {
                const auto file=e->file;
                const auto result=spectralforge::IRCollection::classify(*e,(spectralforge::IRMetadata::Instrument)juce::jmax(0,selectedType.getSelectedId()-1),preferences);
                showFailure(result);refresh(file);
            }
        };
        addAndMakeVisible(remove);remove.setComponentID("irremove");
        remove.setTooltip("Remove this imported IR from the library and cabinet menu. The original file and IR audio already loaded in a project are kept. OPEN IR can add it again.");
        remove.onClick=[this] {
            if(const auto* e=selection()) {
                const auto result=spectralforge::IRCollection::remove(*e,preferences);showFailure(result);
                if(result.wasOk()){refresh();status.setText("Removed from list. Original file and loaded project IR kept.",juce::dontSendNotification);}
            }
        };
        source.onClick=[this]{if(const auto* e=selection()) if(e->tags.values[9].startsWith("https://"))juce::URL(e->tags.values[9]).launchInDefaultBrowser();};
        list.setRowHeight(59);list.setColour(juce::ListBox::backgroundColourId,juce::Colour(0xff171919));
        load.onClick=[this]{commit();};status.setColour(juce::Label::textColourId,juce::Colour(0xffa6aaa7));status.setFont(juce::FontOptions(11.f));
        setSize(1000,672);refresh();
    }
public:
    ~IRBrowserPanel() override { setLookAndFeel(nullptr); }
    explicit IRBrowserPanel(std::function<void(juce::File,int)> callback):folders(spectralforge::IRCollection::roots()),libraryMode(true),selected(std::move(callback)) {initialise();}
    IRBrowserPanel(const juce::File& folder,std::function<void(juce::File)> callback,const juce::File& settings=spectralforge::IRUserPreferences::file()):folders{folder},preferences(settings),selected([callback=std::move(callback)](juce::File file,int){callback(file);}) {initialise();}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101212));g.setColour(juce::Colour(0xffd4c7ad));g.setFont(juce::FontOptions(22.f));
        g.drawText("CABINET LIBRARY",20,14,255,32,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff252827));g.fillRoundedRectangle(722,147,258,462,5);
        if(const auto* e=selection()) {
            spectralforge::art::cabinet(g,{789,160,124,146},e->tags);
            g.setFont(juce::FontOptions(11.f));g.setColour(juce::Colour(0xffaaa89f));
            g.drawText("CABINET STYLE / NOT CAPTURE PHOTO",732,309,238,18,juce::Justification::centred);
        } else {g.setFont(juce::FontOptions(13.f));g.setColour(juce::Colour(0xffb7b5ae));g.drawFittedText("Select a cabinet to inspect its speaker, microphone and capture position.",744,206,213,100,juce::Justification::centred,4);}
    }
    void resized() override {
        importType.setBounds(430,19,235,27);addFolder.setBounds(675,19,147,27);oneFile.setBounds(832,19,148,27);
        search.setBounds(20,64,350,28);diameter.setBounds(380,64,110,28);kind.setBounds(500,64,190,28);availability.setBounds(700,64,280,28);
        microphone.setBounds(20,105,300,28);microphoneReference.setBounds(332,101,648,20);
        microphoneSupport.setBounds(332,122,648,20);
        list.setBounds(20,147,685,462);status.setBounds(20,617,565,44);source.setBounds(598,623,180,28);load.setBounds(790,623,190,28);
        selectedType.setBounds(732,334,238,28);captureDetails.setBounds(732,372,238,190);remove.setBounds(732,572,238,27);
    }
};

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "IRCollection.h"

// A file library is not a host parameter enumeration. Preserve cabtype's four
// stable automation values and resolve installed files on the message thread.
class CabinetSelector : public juce::ComboBox {
public:
    std::function<void(juce::File,int)> selected;
    std::function<void()> browse;
    CabinetSelector() {
        onChange=[this] {
            const int id=getSelectedId();
            if(id==9000 || (id==4 && currentName.isEmpty())) {
                restoreDisplay();
                if(browse) browse();
            }
            else if(id>=100 && id<100+(int)files.size()) {
                preferredFile=files[(size_t)(id-100)];
                restoreDisplay();
                if(selected) selected(files[(size_t)(id-100)],3);
            } else if(id>=1 && id<=4) {
                if(id==4) restoreDisplay();
                if(selected) selected({},id-1);
            }
        };
    }
    void refresh(const std::vector<juce::File>& roots=spectralforge::IRCollection::roots()) {
        clear(juce::dontSendNotification);files.clear();labels.clear();details.clear();
        addSectionHeading("BUILT IN");
        addItem("Filters only",1);addItem("V30 / SM57",2);addItem("Jensen / SM57",3);
        addItem("Project IR",4);
        const auto entries=spectralforge::IRCollection::scan(roots,true);
        for(bool bass:{true,false}) {
            bool heading=false;
            for(const auto& entry:entries) if(!entry.factorySource && entry.ready() && entry.bass()==bass) {
                if(!heading) {addSeparator();addSectionHeading(bass ? "INSTALLED / BASS" : "INSTALLED / GUITAR + OTHER");heading=true;}
                addItem(entry.displayName(),100+(int)files.size());files.push_back(entry.file);
                labels.add(entry.displayName());details.add(entry.details());
            }
        }
        addSeparator();addItem("Browse / import IRs...",9000);
        restoreDisplay();
    }
    void sync(int value,const juce::String& name) {
        // The editor polls parameters. Do not erase a pending asynchronous menu
        // selection when the host state has not actually changed.
        if(displayInitialised && value==source && name==currentName) return;
        source=value;currentName=name;
        if(source!=3 || preferredFile.getFileName()!=currentName)preferredFile=juce::File{};
        restoreDisplay();
    }
    void showPopup() override { refresh();juce::ComboBox::showPopup(); }
    int installedCount() const {return (int)files.size();}
    juce::File fileForItemId(int id) const {return id>=100 && id<100+(int)files.size() ? files[(size_t)(id-100)] : juce::File{};}
private:
    void restoreDisplay() {
        setSelectedId(source+1,juce::dontSendNotification);
        if(source==3) {
            auto text=currentName.isNotEmpty() ? spectralforge::IRMetadata::filenameHints(currentName).shortLabel(currentName) : "Project IR / select a file";
            auto tooltip=currentName.isNotEmpty() ? spectralforge::IRMetadata::filenameHints(currentName).details(currentName) : "Choose a WAV or AIFF cabinet impulse response.";
            for(size_t i=0;i<files.size();++i)if(files[i].getFileName()==currentName && (preferredFile==juce::File{} || preferredFile==files[i])) {text=labels[(int)i];tooltip=details[(int)i];break;}
            setText(text,juce::dontSendNotification);setTooltip(tooltip);
        } else setTooltip(source==0 ? "Speaker IR bypassed; cabinet filters remain available." : getText());
        displayInitialised=true;
    }
    std::vector<juce::File> files;
    juce::StringArray labels,details;
    juce::File preferredFile;
    bool displayInitialised{};
    int source{1};juce::String currentName;
};

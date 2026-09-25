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
                restoreDisplay();
                if(selected) selected(files[(size_t)(id-100)],3);
            } else if(id>=1 && id<=4) {
                if(id==4) restoreDisplay();
                if(selected) selected({},id-1);
            }
        };
    }
    void refresh(const std::vector<juce::File>& roots=spectralforge::IRCollection::roots()) {
        clear(juce::dontSendNotification);files.clear();
        addSectionHeading("BUILT IN");
        addItem("Filters only",1);addItem("V30 / SM57",2);addItem("Jensen / SM57",3);
        addItem("Project IR",4);
        const auto entries=spectralforge::IRCollection::scan(roots,true);
        for(bool bass:{true,false}) {
            bool heading=false;
            for(const auto& entry:entries) if(!entry.factorySource && entry.ready() && entry.bass()==bass) {
                if(!heading) {addSeparator();addSectionHeading(bass ? "INSTALLED / BASS" : "INSTALLED / GUITAR + OTHER");heading=true;}
                const auto name=entry.tags.values[1].isNotEmpty() ? entry.tags.values[1]+" / "+entry.tags.values[3]+" / "+entry.file.getFileNameWithoutExtension() : entry.name;
                addItem(name,100+(int)files.size());files.push_back(entry.file);
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
        restoreDisplay();
    }
    void showPopup() override { refresh();juce::ComboBox::showPopup(); }
    int installedCount() const {return (int)files.size();}
private:
    void restoreDisplay() {
        setSelectedId(source+1,juce::dontSendNotification);
        if(source==3) setText(currentName.isNotEmpty() ? currentName : "Project IR / select a file",juce::dontSendNotification);
        displayInitialised=true;
    }
    std::vector<juce::File> files;
    bool displayInitialised{};
    int source{1};juce::String currentName;
};

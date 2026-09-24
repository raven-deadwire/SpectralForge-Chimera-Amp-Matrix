#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "IRMetadata.h"
namespace spectralforge::art {
inline void screw(juce::Graphics& g,float x,float y) {
    g.setGradientFill({juce::Colour(0xffa4aaa8),x-3,y-3,juce::Colour(0xff252a2a),x+4,y+4,false});g.fillEllipse(x-4,y-4,8,8);
    g.setColour(juce::Colours::black.withAlpha(.8f));g.drawLine(x-2,y+2,x+2,y-2,1.2f);
}
inline void plate(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour c,float radius) {
    g.setColour(juce::Colours::black.withAlpha(.55f));g.fillRoundedRectangle(r.translated(0,4),radius);
    g.setGradientFill({c.brighter(.12f),r.getX(),r.getY(),c.darker(.42f),r.getX(),r.getBottom(),false});g.fillRoundedRectangle(r,radius);
    g.setColour(juce::Colours::white.withAlpha(.20f));g.drawRoundedRectangle(r.reduced(.8f),radius,1);
    g.setColour(juce::Colours::black.withAlpha(.18f));for(float y=r.getY()+5;y<r.getBottom()-4;y+=4)g.drawHorizontalLine((int)y,r.getX()+4,r.getRight()-4);
}
inline void cabinet(juce::Graphics& g,juce::Rectangle<float> r,const IRMetadata& m) {
    plate(g,r,juce::Colour(0xff292825),3);const auto face=r.reduced(5);g.setColour(juce::Colour(0xff111716));g.fillRect(face);
    int count=m.values[1].contains("8x") ? 8 : m.values[1].contains("6x") ? 6 : m.values[1].contains("4x") ? 4 : m.values[1].contains("2x") ? 2 : 1;
    const int columns=count>=4 ? 2 : 1,rows=(count+columns-1)/columns;
    const float d=juce::jmin(face.getWidth()/columns,face.getHeight()/rows)*.82f;
    for(int i=0;i<count;++i){const float x=face.getX()+(i%columns+.5f)*face.getWidth()/columns,y=face.getY()+(i/columns+.5f)*face.getHeight()/rows;
        g.setColour(juce::Colour(0xff43443c));g.fillEllipse(x-d/2,y-d/2,d,d);g.setColour(juce::Colour(0xff171c1a));g.drawEllipse(x-d*.36f,y-d*.36f,d*.72f,d*.72f,2);g.fillEllipse(x-d*.16f,y-d*.16f,d*.32f,d*.32f);}
    g.setColour(juce::Colour(0xff929382).withAlpha(.22f));for(float x=face.getX();x<face.getRight();x+=3)g.drawVerticalLine((int)x,face.getY(),face.getBottom());for(float y=face.getY();y<face.getBottom();y+=3)g.drawHorizontalLine((int)y,face.getX(),face.getRight());
}
inline juce::Colour ampColour(int model) {constexpr std::array<juce::uint32,8> c{0xff7a827c,0xffa38547,0xff393e3e,0xff747777,0xff3b4f4d,0xff665b4c,0xff445d61,0xff3e5264};return juce::Colour(c[(size_t)juce::jlimit(0,7,model)]);}
}

class IRDetailsPanel : public juce::Component {
public:
    IRDetailsPanel(spectralforge::IRMetadata value,bool canEdit,std::function<void(spectralforge::IRMetadata)> callback):metadata(std::move(value)),save(std::move(callback)) {
        for(size_t i=0;i<fields.size();++i){labels[i].setText(spectralforge::IRMetadata::labels[i],juce::dontSendNotification);labels[i].setFont(juce::FontOptions(12.f));labels[i].setColour(juce::Label::textColourId,juce::Colour(0xffd6d8d3));addAndMakeVisible(labels[i]);fields[i].setText(metadata.values[i]);fields[i].setReadOnly(!canEdit);fields[i].setTextToShowWhenEmpty("Unknown / not documented",juce::Colours::grey);addAndMakeVisible(fields[i]);}
        addAndMakeVisible(done);done.setButtonText(canEdit ? "SAVE IN PROJECT" : "CLOSE");done.onClick=[this,canEdit]{if(canEdit){for(size_t i=0;i<fields.size();++i)metadata.values[i]=fields[i].getText();save(metadata);}if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(0);};setSize(760,474);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171c1d));spectralforge::art::cabinet(g,{20,16,72,72},metadata);g.setColour(juce::Colour(0xffe6e4db));g.setFont(juce::FontOptions(18.f));g.drawText("CABINET / CAPTURE DETAILS",110,17,620,26,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffa0aaa5));g.setFont(juce::FontOptions(12.f));g.drawText("Cone position, grille distance and off-axis angle are separate measurements.",110,48,626,20,juce::Justification::centredLeft);g.drawText("Unknown fields stay blank. Tags travel with the IR in projects and A/B slots.",110,67,626,20,juce::Justification::centredLeft);
    }
    void resized() override {for(size_t i=0;i<fields.size();++i){int x=20+int(i%2)*370,y=102+int(i/2)*52;labels[i].setBounds(x,y,350,18);fields[i].setBounds(x,y+20,350,27);}done.setBounds(535,433,205,27);}
private:
    spectralforge::IRMetadata metadata;std::function<void(spectralforge::IRMetadata)> save;
    std::array<juce::Label,12> labels;std::array<juce::TextEditor,12> fields;juce::TextButton done;
};

class IRBrowserPanel : public juce::Component,private juce::ListBoxModel {
    struct Entry {juce::File file;spectralforge::IRMetadata tags;};
    std::vector<Entry> entries;std::vector<size_t> visible;
    juce::TextEditor search;juce::ComboBox diameter;juce::ListBox list{"IR collection",this};juce::TextButton load{"LOAD SELECTED IR"};juce::Label status;
    std::function<void(juce::File)> selected;
    int getNumRows() override {return (int)visible.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool active) override {
        if(row<0 || row>=(int)visible.size())return;const auto& entry=entries[visible[(size_t)row]];
        g.fillAll(active ? juce::Colour(0xff364a45) : juce::Colour(0xff1b2020));g.setColour(juce::Colour(0xffece9df));g.setFont(juce::FontOptions(13.f));g.drawText(entry.file.getFileNameWithoutExtension(),10,4,width-20,21,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffa0aaa5));g.setFont(juce::FontOptions(11.f));g.drawText(entry.tags.summary(),10,25,width-20,height-28,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int row) override {load.setEnabled(row>=0);repaint();}
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override {commit();}
    void commit(){const int row=list.getSelectedRow();if(row<0 || row>=(int)visible.size())return;selected(entries[visible[(size_t)row]].file);if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(0);}
    void filter(){visible.clear();const auto query=search.getText().trim();const auto inches=diameter.getSelectedId()==1 ? juce::String{} : diameter.getText().upToFirstOccurrenceOf(" in",false,false);
        for(size_t i=0;i<entries.size();++i){const auto& e=entries[i];const auto text=e.file.getFileName()+" "+juce::JSON::toString(e.tags.json(),true);if((query.isEmpty() || text.containsIgnoreCase(query)) && (inches.isEmpty() || e.tags.values[2]==inches))visible.push_back(i);}
        list.deselectAllRows();list.updateContent();status.setText(juce::String(visible.size())+" / "+juce::String(entries.size())+" IRs  |  double-click to load",juce::dontSendNotification);load.setEnabled(false);repaint();}
public:
    IRBrowserPanel(const juce::File& folder,std::function<void(juce::File)> callback):selected(std::move(callback)) {
        auto files=folder.findChildFiles(juce::File::findFiles,true,"*");files.sort();
        for(const auto& f:files){if(entries.size()>=512)break;if(!f.hasFileExtension("wav;aif;aiff"))continue;auto tags=spectralforge::IRMetadata::filenameHints(f.getFileName());const auto sidecar=juce::File(f.getFullPathName()+".json");if(sidecar.existsAsFile() && sidecar.getSize()<=16384){const auto json=juce::JSON::parse(sidecar);if(json.isObject())tags=spectralforge::IRMetadata::fromJSON(json);}entries.push_back({f,tags});}
        search.setComponentID("irsearch");diameter.setComponentID("irdiameter");load.setComponentID("irload");list.setComponentID("irlist");search.setTextToShowWhenEmpty("Search speaker, microphone, position or creator",juce::Colours::grey);search.onTextChange=[this]{filter();};addAndMakeVisible(search);
        diameter.addItemList({"All sizes","8 in","10 in","12 in","15 in","18 in"},1);diameter.setSelectedId(1);diameter.onChange=[this]{filter();};addAndMakeVisible(diameter);
        list.setRowHeight(49);list.setColour(juce::ListBox::backgroundColourId,juce::Colour(0xff171c1d));addAndMakeVisible(list);addAndMakeVisible(load);load.onClick=[this]{commit();};status.setColour(juce::Label::textColourId,juce::Colour(0xffa0aaa5));addAndMakeVisible(status);setSize(760,520);filter();
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff171c1d));g.setColour(juce::Colour(0xffe6e4db));g.setFont(juce::FontOptions(20.f));g.drawText("IR COLLECTION",20,15,450,30,juce::Justification::centredLeft);}
    void resized() override {search.setBounds(20,56,565,29);diameter.setBounds(601,56,139,29);list.setBounds(20,99,720,356);status.setBounds(20,470,490,28);load.setBounds(535,470,205,28);}
};

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "IRMetadata.h"
#include "RasterArtwork.h"
namespace spectralforge::art {
class DialogLook : public juce::LookAndFeel_V4 {
public:
    DialogLook() {
        const juce::Colour bg(0xff101313),face(0xff202522),ink(0xffe1dbce),edge(0xff42453f),gold(0xffc4a678);
        setColour(juce::ComboBox::backgroundColourId,bg);setColour(juce::ComboBox::textColourId,ink);setColour(juce::ComboBox::outlineColourId,edge);setColour(juce::ComboBox::arrowColourId,gold);
        setColour(juce::TextEditor::backgroundColourId,bg);setColour(juce::TextEditor::textColourId,ink);setColour(juce::TextEditor::outlineColourId,edge);setColour(juce::TextEditor::focusedOutlineColourId,gold);
        setColour(juce::TextButton::buttonColourId,face);setColour(juce::TextButton::buttonOnColourId,gold);setColour(juce::TextButton::textColourOffId,ink);
        setColour(juce::PopupMenu::backgroundColourId,bg);setColour(juce::PopupMenu::textColourId,ink);setColour(juce::PopupMenu::highlightedBackgroundColourId,face);
        setColour(juce::Label::textColourId,ink);setColour(juce::ListBox::outlineColourId,edge);
    }
};
inline void screw(juce::Graphics& g,float x,float y) {
    g.setGradientFill({juce::Colour(0xffa4aaa8),x-3,y-3,juce::Colour(0xff252a2a),x+4,y+4,false});g.fillEllipse(x-4,y-4,8,8);
    g.setColour(juce::Colours::black.withAlpha(.8f));g.drawLine(x-2,y+2,x+2,y-2,1.2f);
}
inline void plate(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour c,float radius) {
    g.setColour(juce::Colours::black.withAlpha(.55f));g.fillRoundedRectangle(r.translated(0,4),radius);
    g.setGradientFill({c.brighter(.12f),r.getX(),r.getY(),c.darker(.42f),r.getX(),r.getBottom(),false});g.fillRoundedRectangle(r,radius);
    g.setColour(juce::Colours::white.withAlpha(.20f));g.drawRoundedRectangle(r.reduced(.8f),radius,1);
    g.setColour(juce::Colours::black.withAlpha(.045f));for(float y=r.getY()+5;y<r.getBottom()-4;y+=5)g.drawHorizontalLine((int)y,r.getX()+4,r.getRight()-4);
}
inline void cabinet(juce::Graphics& g,juce::Rectangle<float> r,const IRMetadata& m) {
    const auto configuration=m.values[1].toLowerCase();
    if(configuration.contains("8x10") || configuration.contains("4x12") || configuration.contains("2x15") || configuration.contains("1x15")) {
        const auto surface=configuration.contains("8x10") ? Surface::campeg : configuration.contains("2x15") ? Surface::cbassman : configuration.contains("1x15") ? Surface::cdelta : Surface::cmesa;
        raster(g,surface,r,{0,0,1,1},true);return;
    }
    plate(g,r,juce::Colour(0xff292825),3);const auto face=r.reduced(5);g.setColour(juce::Colour(0xff111716));g.fillRect(face);
    // Unknown cabinet metadata must not manufacture a one-speaker cabinet.
    const int count=configuration.contains("8x") ? 8 : configuration.contains("6x") ? 6 : configuration.contains("4x") ? 4 : configuration.contains("2x") ? 2 : configuration.contains("1x") ? 1 : 0;
    if(count==0) {
        g.setColour(juce::Colour(0xffc4a678));g.setFont(juce::FontOptions(juce::jlimit(10.f,27.f,r.getHeight()*.27f)));
        g.drawText("IR",face.toNearestInt(),juce::Justification::centred);
        return;
    }
    const int columns=count>=4 ? 2 : 1,rows=(count+columns-1)/columns;
    const float d=juce::jmin(face.getWidth()/columns,face.getHeight()/rows)*.82f;
    for(int i=0;i<count;++i){const float x=face.getX()+(i%columns+.5f)*face.getWidth()/columns,y=face.getY()+(i/columns+.5f)*face.getHeight()/rows;
        g.setColour(juce::Colour(0xff43443c));g.fillEllipse(x-d/2,y-d/2,d,d);g.setColour(juce::Colour(0xff171c1a));g.drawEllipse(x-d*.36f,y-d*.36f,d*.72f,d*.72f,2);g.fillEllipse(x-d*.16f,y-d*.16f,d*.32f,d*.32f);}
    g.setColour(juce::Colour(0xff929382).withAlpha(.22f));for(float x=face.getX();x<face.getRight();x+=3)g.drawVerticalLine((int)x,face.getY(),face.getBottom());for(float y=face.getY();y<face.getBottom();y+=3)g.drawHorizontalLine((int)y,face.getX(),face.getRight());
}
inline juce::Colour ampColour(int model) {constexpr std::array<juce::uint32,15> c{0xff7a827c,0xffa38547,0xff393e3e,0xff747777,0xff3b4f4d,0xff665b4c,0xff445d61,0xff3e5264,0xff94703d,0xffbd672d,0xffa4a59a,0xff424c53,0xffbcae8d,0xff858b8c,0xffb0b4b4};return juce::Colour(c[(size_t)juce::jlimit(0,static_cast<int>(c.size())-1,model)]);}
}

class IRDetailsPanel : public juce::Component {
public:
    IRDetailsPanel(spectralforge::IRMetadata value,bool canEdit,std::function<void(spectralforge::IRMetadata)> callback):metadata(std::move(value)),save(std::move(callback)) {
        setLookAndFeel(&look);
        for(size_t i=0;i<fields.size();++i){labels[i].setText(spectralforge::IRMetadata::labels[i],juce::dontSendNotification);labels[i].setFont(juce::FontOptions(12.f));labels[i].setColour(juce::Label::textColourId,juce::Colour(0xffd6d8d3));addAndMakeVisible(labels[i]);fields[i].setText(metadata.values[i]);fields[i].setReadOnly(!canEdit);fields[i].setTextToShowWhenEmpty("Unknown / not documented",juce::Colours::grey);addAndMakeVisible(fields[i]);}
        addAndMakeVisible(done);done.setButtonText(canEdit ? "SAVE IN PROJECT" : "CLOSE");done.onClick=[this,canEdit]{if(canEdit){for(size_t i=0;i<fields.size();++i)metadata.values[i]=fields[i].getText();save(metadata);}if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(0);};setSize(760,474);
    }
    ~IRDetailsPanel() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171c1d));spectralforge::art::cabinet(g,{20,16,72,72},metadata);g.setColour(juce::Colour(0xffe6e4db));g.setFont(juce::FontOptions(18.f));g.drawText("CABINET / CAPTURE DETAILS",110,17,620,26,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffa0aaa5));g.setFont(juce::FontOptions(12.f));g.drawText("Cone position, grille distance and off-axis angle are separate measurements.",110,48,626,20,juce::Justification::centredLeft);g.drawText("Unknown fields stay blank. Tags travel with the IR in projects and A/B slots.",110,67,626,20,juce::Justification::centredLeft);
    }
    void resized() override {for(size_t i=0;i<fields.size();++i){int x=20+int(i%2)*370,y=102+int(i/2)*52;labels[i].setBounds(x,y,350,18);fields[i].setBounds(x,y+20,350,27);}done.setBounds(535,433,205,27);}
private:
    spectralforge::art::DialogLook look;
    spectralforge::IRMetadata metadata;std::function<void(spectralforge::IRMetadata)> save;
    std::array<juce::Label,12> labels;std::array<juce::TextEditor,12> fields;juce::TextButton done;
};

#include "IRBrowserPanel.h"

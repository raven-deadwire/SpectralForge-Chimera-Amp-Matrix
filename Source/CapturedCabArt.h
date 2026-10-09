#pragma once
#include "CabArtwork.h"
#include "IRMetadata.h"
#include <algorithm>
#include <regex>
#include <vector>

namespace spectralforge::capturedCabArt {
// Capture artwork is descriptive metadata only; it never selects a DSP model.
struct Configuration {
    std::vector<int> diameters;
    bool known() const noexcept {return !diameters.empty();}
};
inline Configuration configuration(const IRMetadata& metadata) {
    Configuration result;
    const auto text=metadata.values[1].replace(juce::String::fromUTF8("×"),"x").toStdString();
    static const std::regex pattern("(^|[^0-9])([12468])[[:space:]]*[xX][[:space:]]*(8|10|12|15|18)(?=[^0-9]|$)");
    for(auto it=std::sregex_iterator(text.begin(),text.end(),pattern);it!=std::sregex_iterator();++it) {
        const auto count=std::stoi((*it)[2]),diameter=std::stoi((*it)[3]);
        if(result.diameters.size()+size_t(count)>8)return {};
        result.diameters.insert(result.diameters.end(),size_t(count),diameter);
    }
    return result;
}
inline juce::String cabinetCaption(const IRMetadata& metadata,int source) {
    if(source==0)return "Filters only";
    const auto text=metadata.values[1].trim();
    return text.isEmpty() || text.equalsIgnoreCase("unspecified") ? "Unspecified cabinet" : text;
}
inline bool mixedMicrophones(const IRMetadata& metadata) {
    const auto text=metadata.values[3].toLowerCase();
    return text.contains("+") || text.contains(" / ") || text.contains(" & ")
        || text.contains("mix") || text.contains("blend");
}
inline juce::String microphoneCaption(const IRMetadata& metadata,const juce::String& filename,int source) {
    if(source==0)return "IR bypassed";
    if(const auto* model=metadata.microphoneModel(filename))return model->alias;
    return mixedMicrophones(metadata) ? "Mixed microphones" : "Unspecified microphone";
}

inline void cone(juce::Graphics& g,juce::Point<float> centre,float radius) {
    const auto bounds=juce::Rectangle<float>(radius*2,radius*2).withCentre(centre);
    g.setGradientFill({juce::Colour(0xff777975),centre.x,centre.y-radius,juce::Colour(0xff222726),centre.x,centre.y+radius,false});
    g.fillEllipse(bounds);g.setColour(juce::Colour(0xff090d0d));g.fillEllipse(bounds.reduced(radius*.09f));
    g.setGradientFill({juce::Colour(0xff585b53),centre.x-radius*.35f,centre.y-radius*.7f,juce::Colour(0xff171d1b),centre.x+radius*.5f,centre.y+radius*.7f,true});
    g.fillEllipse(bounds.reduced(radius*.20f));
    g.setColour(juce::Colour(0xff8d9084).withAlpha(.22f));
    for(float ring:{.27f,.32f,.55f,.59f,.66f,.72f})g.drawEllipse(bounds.reduced(radius*ring),.7f);
    g.setGradientFill({juce::Colour(0xff555d58),centre.x-radius*.2f,centre.y-radius*.3f,juce::Colour(0xff151a18),centre.x+radius*.3f,centre.y+radius*.3f,true});
    g.fillEllipse(bounds.reduced(radius*.70f));
    for(int n=0;n<8;++n) {
        const float angle=float(n)*juce::MathConstants<float>::twoPi/8.f;
        const auto bolt=centre+juce::Point<float>(std::sin(angle),std::cos(angle))*radius*.94f;
        g.setColour(juce::Colour(0xffa7aaa2));g.fillEllipse(bolt.x-1.f,bolt.y-1.f,2.f,2.f);
    }
}

class CabinetView : public juce::Component {
    IRMetadata metadata;
    Configuration layout;
    int source{};
public:
    CabinetView() {setInterceptsMouseClicks(false,false);setWantsKeyboardFocus(false);}
    void setCapture(const IRMetadata& next,int selected) {
        metadata=next;source=selected;layout=configuration(metadata);
        const auto caption=cabinetCaption(metadata,source);
        setTitle(caption);setDescription(caption);
        getProperties().set("cabCaptureUnitCount",source ? int(layout.diameters.size()) : 0);
        getProperties().set("cabCaptureKnownConfiguration",source!=0 && layout.known());
        repaint();
    }
    void setActive(bool active) {setAlpha(active ? 1.f : .34f);}
    void paint(juce::Graphics& g) override {
        auto area=getLocalBounds().toFloat();const auto caption=area.removeFromBottom(31.f);
        area=area.reduced(9.f,7.f);
        const int count=source ? int(layout.diameters.size()) : 0;
        const int columns=count>=4 || (count==2 && layout.diameters[0]<=12 && layout.diameters[0]==layout.diameters[1]) ? 2 : 1;
        const int rows=count ? (count+columns-1)/columns : 1;
        const float ratio=count ? float(columns)/float(rows)*1.04f : 1.22f;
        const float height=juce::jmin(area.getHeight()-12.f,area.getWidth()/ratio);
        const auto box=juce::Rectangle<float>(height*ratio,height).withCentre(area.getCentre()).translated(0,2.f);
        const auto shadow=box.withY(box.getBottom()-3.f).withHeight(13.f).expanded(8.f,0.f);
        g.setColour(juce::Colours::black.withAlpha(.3f));g.fillEllipse(shadow);
        for(const float side:{.18f,.82f}) {
            g.setColour(juce::Colour(0xff060909));g.fillRoundedRectangle(box.getX()+box.getWidth()*side-6.f,box.getBottom()-1.f,12.f,6.f,1.5f);
        }
        g.setGradientFill({juce::Colour(0xff454a46),box.getTopLeft(),juce::Colour(0xff171c1b),box.getBottomRight(),false});
        g.fillRoundedRectangle(box,5.f);
        g.setColour(juce::Colour(0xff72796f).withAlpha(.28f));g.drawRoundedRectangle(box.reduced(.5f),5.f,1.f);
        auto face=box.reduced(7.f);g.setColour(juce::Colour(0xff101615));g.fillRoundedRectangle(face,2.f);
        g.setColour(juce::Colour(0xffb2a58c).withAlpha(.50f));g.drawRoundedRectangle(face.expanded(1.2f),2.5f,1.2f);
        if(count) {
            const float cellWidth=face.getWidth()/float(columns),cellHeight=face.getHeight()/float(rows);
            const auto maximum=*std::max_element(layout.diameters.begin(),layout.diameters.end());
            for(int n=0;n<count;++n) {
                const auto centre=juce::Point<float>(face.getX()+(float(n%columns)+.5f)*cellWidth,face.getY()+(float(n/columns)+.5f)*cellHeight);
                cone(g,centre,juce::jmin(cellWidth,cellHeight)*.43f*float(layout.diameters[size_t(n)])/float(maximum));
            }
        } else {
            g.setGradientFill({juce::Colour(0xff343c38),face.getTopLeft(),juce::Colour(0xff171e1a),face.getBottomRight(),false});g.fillRoundedRectangle(face,2.f);
        }
        // An opaque grille on unknown captures supplies an actual cabinet image
        // without inventing a driver count, diameter or branded enclosure.
        g.setColour(juce::Colour(0xffabb5a6).withAlpha(count ? .075f : .17f));
        for(float x=face.getX()+1.f;x<face.getRight();x+=3.f)g.drawVerticalLine(int(x),face.getY(),face.getBottom());
        for(float y=face.getY()+1.f;y<face.getBottom();y+=3.f)g.drawHorizontalLine(int(y),face.getX(),face.getRight());
        const auto badge=juce::Rectangle<float>(32.f,11.f).withCentre({face.getCentreX(),face.getY()+10.f});
        g.setColour(juce::Colour(0xff171c19));g.fillRoundedRectangle(badge,1.f);
        g.setColour(juce::Colour(0xffb1baa9));g.setFont(juce::FontOptions(7.f,juce::Font::bold));g.drawText("CAPTURE",badge,juce::Justification::centred);
        for(float x:{box.getX()+4.f,box.getRight()-4.f})for(float y:{box.getY()+4.f,box.getBottom()-4.f}) {
            g.setColour(juce::Colour(0xff969d92));g.fillEllipse(x-1.f,y-1.f,2.f,2.f);
        }
        g.setColour(juce::Colour(0xffc1c8c1));g.setFont(juce::FontOptions(11.f));
        g.drawFittedText(cabinetCaption(metadata,source),caption.toNearestInt(),juce::Justification::centred,2);
    }
};

// Retains the public View asset identity used by the capture browser and tests.
// An unknown or mixed capture stays Asset::count, with a generic illustration.
class MicrophoneView : public cabArt::View {
    bool mixed{},bypassed{};
public:
    void setCapture(const IRMetadata& metadata,const juce::String& filename,int source) {
        mixed=mixedMicrophones(metadata);bypassed=source==0;
        const auto* model=bypassed ? nullptr : metadata.microphoneModel(filename);
        const auto caption=microphoneCaption(metadata,filename,source);
        setTrimArtwork(true);
        setAsset(cabArt::capturedMicrophone(model),caption,caption);
        getProperties().set("cabCaptureMixedMicrophones",!bypassed && mixed);
        repaint();
    }
    void paint(juce::Graphics& g) override {
        if(hasImage()){cabArt::View::paint(g);return;}
        const auto area=getLocalBounds().toFloat().reduced(7.f);
        const auto draw=[&](juce::Rectangle<float> destination) {
            juce::Graphics::ScopedSaveState state(g);
            const float scale=juce::jmin(destination.getWidth()/48.f,destination.getHeight()/156.f);
            g.addTransform(juce::AffineTransform::scale(scale).translated(destination.getCentreX()-24.f*scale,destination.getCentreY()-78.f*scale));
            g.setGradientFill({juce::Colour(0xff7b8988),7,0,juce::Colour(0xff27302f),40,0,false});g.fillRoundedRectangle(9,4,30,67,13.f);
            g.setColour(juce::Colour(0xff101817));for(float y=10.f;y<62.f;y+=4.f)g.drawHorizontalLine(int(y),13.f,35.f);
            for(float x=15.f;x<35.f;x+=4.f)g.drawVerticalLine(int(x),9.f,62.f);
            g.setGradientFill({juce::Colour(0xff606d6b),11,70,juce::Colour(0xff182322),35,70,false});g.fillRoundedRectangle(11,67,26,71,5.f);
            g.setColour(juce::Colour(0xff91a09a));g.fillRoundedRectangle(10,69,28,3,1.f);
            g.setColour(juce::Colour(0xff0b1312));g.fillRoundedRectangle(15,129,18,14,2.f);
            g.setColour(juce::Colour(0xff708078));g.drawRoundedRectangle(15,129,18,14,2.f,1.f);
            g.setColour(juce::Colour(0xff121b18));g.drawLine(24.f,143.f,24.f,156.f,3.f);
        };
        if(mixed && !bypassed) {draw(area.withWidth(area.getWidth()*.58f));draw(area.withWidth(area.getWidth()*.58f).withRight(area.getRight()).translated(0,7.f));}
        else draw(area);
    }
};
}

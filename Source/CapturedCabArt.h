#pragma once
#include "CabArtwork.h"
#include "IRMetadata.h"
#include "CabMicrophoneDimensions.h"
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
// These are authored enclosure envelopes, derived from the documented driver
// array. IR metadata rarely includes measured cabinet dimensions. All figures
// are metres; neither enclosure family nor manufacturer dimensions are inferred.
struct CabinetDimensions {
    float width{.64f},height{.54f};
    std::vector<juce::Point<float>> centres;
};
inline CabinetDimensions dimensions(const Configuration& configuration) {
    CabinetDimensions result;
    const int count=int(configuration.diameters.size());
    if(count==0)return result;
    const int columns=count>=4 || (count==2 && configuration.diameters[0]<=12
        && configuration.diameters[0]==configuration.diameters[1]) ? 2 : 1;
    const int rows=(count+columns-1)/columns;
    std::vector<float> widths(size_t(columns),0.f),heights(size_t(rows),0.f);
    for(int n=0;n<count;++n) {
        const float diameter=float(configuration.diameters[size_t(n)])*.0254f;
        widths[size_t(n%columns)]=juce::jmax(widths[size_t(n%columns)],diameter);
        heights[size_t(n/columns)]=juce::jmax(heights[size_t(n/columns)],diameter);
    }
    constexpr float side=.055f,top=.06f,bottom=.08f,gap=.025f;
    result.width=side*2.f+gap*float(columns-1);
    result.height=top+bottom+gap*float(rows-1);
    for(const auto width:widths)result.width+=width;
    for(const auto height:heights)result.height+=height;
    for(int n=0;n<count;++n) {
        float x=side,y=top;
        for(int c=0;c<n%columns;++c)x+=widths[size_t(c)]+gap;
        for(int r=0;r<n/columns;++r)y+=heights[size_t(r)]+gap;
        result.centres.push_back({x+widths[size_t(n%columns)]*.5f,y+heights[size_t(n/columns)]*.5f});
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
    return mixedMicrophones(metadata) ? "Mixed microphones / example image" : "Unspecified microphone / example image";
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

inline void paintCabinet(juce::Graphics& g,juce::Rectangle<float> box,const Configuration& layout,bool shadow=true) {
    const auto size=dimensions(layout);
    const int count=int(layout.diameters.size());
    const float pixelsPerMetre=box.getHeight()/size.height;
    if(shadow) {
        const auto ground=box.withY(box.getBottom()+.01f*pixelsPerMetre).withHeight(.06f*pixelsPerMetre).expanded(.03f*pixelsPerMetre,0.f);
        g.setColour(juce::Colours::black.withAlpha(.3f));g.fillEllipse(ground);
    }
    for(const float side:{.18f,.82f}) {
        g.setColour(juce::Colour(0xff060909));g.fillRoundedRectangle(box.getX()+box.getWidth()*side-.026f*pixelsPerMetre,box.getBottom()-.006f*pixelsPerMetre,.052f*pixelsPerMetre,.030f*pixelsPerMetre,1.f);
    }
    g.setGradientFill({juce::Colour(0xff454a46),box.getTopLeft(),juce::Colour(0xff171c1b),box.getBottomRight(),false});
    g.fillRoundedRectangle(box,5.f);
    g.setColour(juce::Colour(0xff72796f).withAlpha(.28f));g.drawRoundedRectangle(box.reduced(.5f),5.f,1.f);
    auto face=box.reduced(.032f*pixelsPerMetre);g.setColour(juce::Colour(0xff101615));g.fillRoundedRectangle(face,2.f);
    g.setColour(juce::Colour(0xffb2a58c).withAlpha(.50f));g.drawRoundedRectangle(face.expanded(1.2f),2.5f,1.2f);
    if(count) {
        for(int n=0;n<count;++n) {
            const auto centre=box.getTopLeft()+size.centres[size_t(n)]*pixelsPerMetre;
            cone(g,centre,float(layout.diameters[size_t(n)])*.0254f*pixelsPerMetre*.5f);
        }
    } else {
        g.setGradientFill({juce::Colour(0xff343c38),face.getTopLeft(),juce::Colour(0xff171e1a),face.getBottomRight(),false});g.fillRoundedRectangle(face,2.f);
    }
    // An opaque grille on unknown captures supplies an actual cabinet image
    // without inventing a driver count, diameter or branded enclosure.
    g.setColour(juce::Colour(0xffabb5a6).withAlpha(count ? .075f : .17f));
    for(float x=face.getX()+1.f;x<face.getRight();x+=3.f)g.drawVerticalLine(int(x),face.getY(),face.getBottom());
    for(float y=face.getY()+1.f;y<face.getBottom();y+=3.f)g.drawHorizontalLine(int(y),face.getX(),face.getRight());
    const auto badge=juce::Rectangle<float>(.14f*pixelsPerMetre,.048f*pixelsPerMetre).withCentre({face.getCentreX(),face.getY()+.035f*pixelsPerMetre});
    g.setColour(juce::Colour(0xff171c19));g.fillRoundedRectangle(badge,1.f);
    g.setColour(juce::Colour(0xffb1baa9));g.setFont(juce::FontOptions(.030f*pixelsPerMetre,juce::Font::bold));g.drawText(count ? "CAPTURE" : "EXAMPLE",badge,juce::Justification::centred);
    for(float x:{box.getX()+4.f,box.getRight()-4.f})for(float y:{box.getY()+4.f,box.getBottom()-4.f}) {
        g.setColour(juce::Colour(0xff969d92));g.fillEllipse(x-1.f,y-1.f,2.f,2.f);
    }
}

class CabinetView : public juce::Component {
    IRMetadata metadata;
    Configuration layout;
    int source{};
    float pixelsPerMetre{112.f};
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
    CabinetDimensions physicalSize() const {return dimensions(source ? layout : Configuration{});}
    float stageScale() const noexcept {return pixelsPerMetre;}
    void setStageScale(float value) {if(pixelsPerMetre!=value){pixelsPerMetre=value;repaint();}}
    juce::Rectangle<float> artworkBounds() const {
        const auto size=physicalSize();
        return {float(getWidth())*.43f-size.width*pixelsPerMetre*.5f,
            float(getHeight())-40.f-(size.height+.024f)*pixelsPerMetre,size.width*pixelsPerMetre,size.height*pixelsPerMetre};
    }
    void paint(juce::Graphics& g) override {
        auto area=getLocalBounds().toFloat();
        const auto caption=area.removeFromBottom(31.f).removeFromLeft(area.getWidth()*.70f);
        const auto box=artworkBounds();
        paintCabinet(g,box,source ? layout : Configuration{});
        g.setColour(juce::Colour(0xffc1c8c1));g.setFont(juce::FontOptions(11.f));
        const auto text=cabinetCaption(metadata,source)
            +(source && !layout.known() ? " / example image" : "");
        g.drawFittedText(text,caption.toNearestInt(),juce::Justification::centred,2);
    }
};

// Retains the public View asset identity used by the capture browser and tests.
// An unknown or mixed capture stays Asset::count, with a generic illustration.
class MicrophoneView : public cabArt::View {
    bool mixed{},bypassed{};
    cabPhysical::MicDimensions physical{.048f,.18f,true};
    juce::Rectangle<float> body;
    float floor{},pixelsPerMetre{};
public:
    cabPhysical::MicDimensions physicalSize() const noexcept {return physical;}
    juce::Rectangle<float> bodyBounds() const noexcept {return body;}
    void setStage(juce::Rectangle<float> artwork,float baseline,float scale) {
        body=artwork;floor=baseline;pixelsPerMetre=scale;setArtworkArea(body);repaint();
    }
    void setCapture(const IRMetadata& metadata,const juce::String& filename,int source) {
        mixed=mixedMicrophones(metadata);bypassed=source==0;
        const auto* model=bypassed ? nullptr : metadata.microphoneModel(filename);
        const auto caption=microphoneCaption(metadata,filename,source);
        physical={.048f,.18f,true};
        if(model)physical=cabPhysical::microphone(int(model-micCatalog::models.data()));
        if(mixed && !bypassed)physical.width=.10f;
        setPhysicalDimensions(physical.width,physical.height);
        setTrimArtwork(true);
        setAsset(cabArt::capturedMicrophone(model),caption,caption);
        getProperties().set("cabCaptureMixedMicrophones",!bypassed && mixed);
        repaint();
    }
    void paint(juce::Graphics& g) override {
        if(!bypassed && !body.isEmpty()) {
            // Both capture cards use the cabinet's physical scale and floor.
            g.setColour(juce::Colour(0xff111918));
            g.drawLine(body.getCentreX(),body.getBottom(),body.getCentreX(),floor,juce::jmax(.8f,.008f*pixelsPerMetre));
            g.setColour(juce::Colour(0xff76817d));
            g.drawLine(body.getCentreX()-.065f*pixelsPerMetre,floor,body.getCentreX()+.065f*pixelsPerMetre,floor,1.f);
        }
        if(hasImage()){cabArt::View::paint(g);return;}
        const auto area=body;
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
        if(mixed && !bypassed) {draw(area.withWidth(area.getWidth()*.58f));draw(area.withWidth(area.getWidth()*.58f).withRight(area.getRight()).translated(0,.02f*pixelsPerMetre));}
        else draw(area);
    }
};
}

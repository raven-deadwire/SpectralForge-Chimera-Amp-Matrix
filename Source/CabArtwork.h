#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ChimeraArtworkData.h"
#include "MicrophoneCatalog.h"
#include "LifecycleTrace.h"
#include "CabSpeakerArt.h"
#include <array>

namespace spectralforge::cabArt {
// Artwork identities are presentation-only. They never become automation or
// model indices, and a microphone illustration does not add a DSP response.
enum class Asset {
    guitarCabinet, bassCabinet, guitarSpeaker, bassSpeaker,
    attackDynamic, bodyRibbon, detailCondenser,
    dynamic57, dynamic421, dynamic441, dynamic906, dynamic20, dynamic7,
    dynamic201, dynamic88, dynamic112, ribbon121, ribbon160, ribbon4038,
    condenser87, condenser414, condenser184, condenser47Fet, condenser4050,
    condenser201Fet, condenser67, strike, count
};
inline constexpr size_t assetCount=static_cast<size_t>(Asset::count);
inline constexpr std::array<const char*,assetCount> keys{{
    "cab-guitar-412", "cab-bass-410", "speaker-guitar-12", "speaker-bass-10",
    "mic-attack-dynamic", "mic-body-ribbon", "mic-detail-condenser",
    "mic-dynamic-57", "mic-dynamic-421", "mic-dynamic-441", "mic-dynamic-906",
    "mic-dynamic-20", "mic-dynamic-7", "mic-dynamic-201", "mic-dynamic-88",
    "mic-dynamic-112", "mic-ribbon-121", "mic-ribbon-160", "mic-ribbon-4038",
    "mic-condenser-87", "mic-condenser-414", "mic-condenser-184",
    "mic-condenser-47-fet", "mic-condenser-4050", "mic-condenser-201-fet",
    "mic-condenser-67", "mic-chimera-strike"
}};
inline constexpr std::array<const char*,assetCount> resourceNames{{
    "cabguitar412_png", "cabbass410_png", "speakerguitar12_png", "speakerbass10_png",
    "micattackdynamic_png", "micbodyribbon_png", "micdetailcondenser_png",
    "micdynamic57_png", "micdynamic421_png", "micdynamic441_png", "micdynamic906_png",
    "micdynamic20_png", "micdynamic7_png", "micdynamic201_png", "micdynamic88_png",
    "micdynamic112_png", "micribbon121_png", "micribbon160_png", "micribbon4038_png",
    "miccondenser87_png", "miccondenser414_png", "miccondenser184_png",
    "miccondenser47fet_png", "miccondenser4050_png", "miccondenser201fet_png",
    "miccondenser67_png", "micchimerastrike_png"
}};
inline const char* key(Asset asset) {
    const auto index=static_cast<size_t>(asset);
    return index<assetCount ? keys[index] : "unspecified";
}
inline Asset cabinet(int design) {return design==1 ? Asset::bassCabinet : Asset::guitarCabinet;}
inline Asset speaker(int design) {return design==1 ? Asset::bassSpeaker : Asset::guitarSpeaker;}
inline Asset originalMicrophone(int role) {
    return role==1 ? Asset::bodyRibbon : role==2 ? Asset::detailCondenser : Asset::attackDynamic;
}
// These anchors describe the alpha-trimmed, cabinet-facing illustrations, not
// another microphone/acoustic model. The capsule is the physical pickup point;
// mounting hardware and XLR exits are independent of the image rectangle.
struct MicrophonePresentation {
    juce::Point<float> capsule,mount,cable;
    float longestSpeakerRatio;
    bool sideAddress;
};
// Anchors are fractions of each alpha-trimmed catalog illustration. End-address
// bodies rotate towards the cabinet; side-address grilles remain upright.
inline MicrophonePresentation expandedMicrophonePresentation(int index) {
    static const std::array<MicrophonePresentation,20> layouts{{
        {{.5f,.10f},{.5f,.68f},{.5f,.98f},.60f,false},
        {{.5f,.20f},{.5f,.77f},{.5f,.98f},.78f,false},
        {{.5f,.16f},{.5f,.80f},{.5f,.98f},.87f,false},
        {{.48f,.32f},{.5f,.70f},{.5f,.98f},.55f,true},
        {{.5f,.10f},{.5f,.64f},{.5f,.98f},.82f,false},
        {{.5f,.18f},{.55f,.75f},{.5f,.98f},.75f,false},
        {{.5f,.07f},{.5f,.55f},{.5f,.98f},.61f,false},
        {{.5f,.17f},{.5f,.65f},{.5f,.98f},.69f,false},
        {{.39f,.37f},{.58f,.77f},{.5f,.98f},.58f,true},
        {{.5f,.22f},{.5f,.66f},{.5f,.98f},.77f,true},
        {{.5f,.13f},{.5f,.65f},{.5f,.98f},.59f,false},
        {{.45f,.30f},{.5f,.72f},{.5f,.98f},.58f,true},
        {{.5f,.20f},{.5f,.65f},{.5f,.98f},.76f,true},
        {{.46f,.21f},{.5f,.81f},{.5f,.98f},.60f,true},
        {{.5f,.035f},{.5f,.70f},{.5f,.98f},.41f,false},
        {{.47f,.24f},{.5f,.88f},{.5f,.98f},.85f,true},
        {{.5f,.24f},{.5f,.76f},{.5f,.98f},.70f,true},
        {{.5f,.20f},{.5f,.70f},{.5f,.98f},.74f,true},
        {{.5f,.20f},{.5f,.73f},{.5f,.98f},.83f,true},
        {{.5f,.19f},{.5f,.80f},{.5f,.98f},.68f,true}
    }};
    return layouts[size_t(juce::jlimit(0,19,index))];
}
inline MicrophonePresentation microphonePresentation(int role) {
    if(role==1)return {{.665f,.255f},{.148f,.630f},{.691f,.990f},.55f,true};
    if(role==2)return {{.420f,.215f},{.946f,.739f},{.433f,.991f},.55f,true};
    return {{.076f,.190f},{.604f,.906f},{.964f,.894f},.48f,false};
}
inline Asset capturedMicrophone(const micCatalog::Model* model) {
    if(!model)return Asset::count;
    const juce::String wanted="mic-"+juce::String(model->id);
    for(size_t i=7;i<keys.size();++i)if(wanted==keys[i])return static_cast<Asset>(i);
    return Asset::count;
}

struct Bank {
    std::array<juce::Image,assetCount> images;
    std::array<juce::Rectangle<int>,assetCount> contentBounds;
    std::array<juce::Image,16> frontSpeakers;
    Bank() {
        const lifecycle::Scope trace("cab.artwork.create",this);
        // All decode work happens when a CAB UI takes ownership on the message
        // thread. Drawing and audio processing never read files or decode PNGs.
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        for(size_t i=0;i<resourceNames.size();++i) {
            int size=0;
            const auto* bytes=ChimeraArtworkData::getNamedResource(resourceNames[i],size);
            if(bytes && size>0) {
                auto decoded=juce::ImageFileFormat::loadFrom(bytes,static_cast<size_t>(size));
                const int maximum=i<2 ? 640 : i<4 ? 384 : 320;
                const int longest=juce::jmax(decoded.getWidth(),decoded.getHeight());
                if(longest>maximum) {
                    const double scale=double(maximum)/double(longest);
                    decoded=decoded.rescaled(juce::jmax(1,juce::roundToInt(decoded.getWidth()*scale)),
                        juce::jmax(1,juce::roundToInt(decoded.getHeight()*scale)),juce::Graphics::highResamplingQuality);
                }
                images[i]=std::move(decoded);
                if(images[i].isValid()) {
                    // Trim only for scene placement. The embedded file and the
                    // catalog thumbnail keep their original pixels and identity.
                    const juce::Image::BitmapData pixels(images[i],juce::Image::BitmapData::readOnly);
                    int left=pixels.width,top=pixels.height,right=0,bottom=0;
                    for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x)
                        if(pixels.getPixelColour(x,y).getAlpha()>32) {
                            left=juce::jmin(left,x);top=juce::jmin(top,y);
                            right=juce::jmax(right,x+1);bottom=juce::jmax(bottom,y+1);
                        }
                    contentBounds[i]=right>left && bottom>top ? juce::Rectangle<int>(left,top,right-left,bottom-top) : images[i].getBounds();
                }
            }
        }
        // Render front geometry once per shared UI bank, not in paint/timers.
        for(size_t i=0;i<frontSpeakers.size();++i) {
            const int driver=i<2 ? 0 : int(i)-1,legacy=i==1 ? 1 : 0;
            const cabExpansion::Settings settings{{false,legacy},driver,0,0};
            const float outer=float(cabExpansion::diameter(settings))*.0254f*.5f;
            const float cone=float(driver ? cabExpansion::drivers[size_t(driver-1)].speaker.radius : originalCab::speakers[size_t(legacy)].radius);
            // Bounded front caches keep the complete shared artwork bank
            // below 16 MiB, including the microphone and room textures.
            auto& image=frontSpeakers[i];image=juce::Image(juce::Image::ARGB,128,128,true);
            juce::Graphics graphics(image);cabSpeakerArt::paint(graphics,image.getBounds().toFloat(),driver,legacy,cone/outer);
        }
    }
    ~Bank() {
        const lifecycle::Scope trace("cab.artwork.destroy",this);
        images.fill({});
        frontSpeakers.fill({});
    }
};

inline void neutral(juce::Graphics& g,juce::Rectangle<float> area,const juce::String& text) {
    g.setColour(juce::Colour(0xff2b3840));
    g.drawRoundedRectangle(area.reduced(3.f),6.f,1.f);
    g.setColour(juce::Colour(0xff8d9ba4));
    g.setFont(juce::FontOptions(juce::jlimit(10.f,16.f,area.getWidth()*.18f)));
    g.drawFittedText(text,area.reduced(5.f).toNearestInt(),juce::Justification::centred,2);
}

class View : public juce::Component {
    // SharedResourcePointer drops every native image when the last CAB closes.
    // A process-lifetime singleton could survive the host's graphics shutdown.
    juce::SharedResourcePointer<Bank> bank;
    Asset selected{Asset::count};
    juce::String emptyText{"IR"};
    float opacity{1.f};
    bool trimArtwork{};
    float rotation{};
    juce::Rectangle<float> explicitArea;
    int speakerDriver{};
    bool isSpeaker() const noexcept {return selected==Asset::guitarSpeaker || selected==Asset::bassSpeaker;}
    juce::Rectangle<float> rotatedSourceBounds() const noexcept {
        const auto index=static_cast<size_t>(selected);
        if(index>=assetCount || !bank->images[index].isValid())return {0,0,1,1};
        const auto source=trimArtwork ? bank->contentBounds[index] : bank->images[index].getBounds();
        return juce::Rectangle<float>(float(source.getWidth()),float(source.getHeight())).transformedBy(juce::AffineTransform::rotation(rotation));
    }
    juce::AffineTransform artworkTransform() const noexcept {
        const auto source=rotatedSourceBounds(),destination=artworkBounds();
        return juce::AffineTransform::rotation(rotation).translated(-source.getX(),-source.getY())
            .scaled(destination.getWidth()/source.getWidth(),destination.getHeight()/source.getHeight())
            .translated(destination.getX(),destination.getY());
    }
public:
    View() {
        setInterceptsMouseClicks(false,false);setWantsKeyboardFocus(false);
        getProperties().set("cabArtworkKey",key(selected));
    }
    Asset asset() const noexcept {return selected;}
    void setSpeakerDesign(int driver) {
        const int next=juce::jlimit(0,14,driver);
        getProperties().set("cabSpeakerStyle",cabSpeakerArt::styleKey(next,selected==Asset::bassSpeaker ? 1 : 0));
        if(speakerDriver!=next){speakerDriver=next;repaint();}
    }
    bool hasImage() const noexcept {
        const auto index=static_cast<size_t>(selected);
        return index<assetCount && bank->images[index].isValid();
    }
    void setTrimArtwork(bool trim) {if(trimArtwork!=trim){trimArtwork=trim;repaint();}}
    void setArtworkRotation(float value) {if(rotation!=value){rotation=value;repaint();}}
    juce::Point<float> orientedAnchor(juce::Point<float> point) const noexcept {
        if(rotation==0)return point;
        const auto index=static_cast<size_t>(selected);
        if(index>=assetCount || !bank->images[index].isValid())return point;
        const auto source=trimArtwork ? bank->contentBounds[index] : bank->images[index].getBounds();
        const auto rotated=juce::Point<float>(point.x*float(source.getWidth()),point.y*float(source.getHeight())).transformedBy(juce::AffineTransform::rotation(rotation));
        const auto bounds=rotatedSourceBounds();
        return {(rotated.x-bounds.getX())/bounds.getWidth(),(rotated.y-bounds.getY())/bounds.getHeight()};
    }
    // Scene sprites can retain a subpixel capsule anchor while JUCE component
    // bounds remain integer-valued. Catalog views keep the default inset area.
    void setArtworkArea(juce::Rectangle<float> area) {
        if(explicitArea!=area){explicitArea=area;repaint();}
    }
    float artworkAspectRatio() const noexcept {
        if(isSpeaker())return 1.f;
        const auto index=static_cast<size_t>(selected);
        if(index>=assetCount || !bank->images[index].isValid())return 1.f;
        const auto source=trimArtwork ? bank->contentBounds[index] : bank->images[index].getBounds();
        if(rotation!=0){const auto bounds=rotatedSourceBounds();return bounds.getWidth()/bounds.getHeight();}
        return float(source.getWidth())/float(juce::jmax(1,source.getHeight()));
    }
    juce::Rectangle<float> artworkBounds() const noexcept {
        const auto index=static_cast<size_t>(selected);
        const auto area=explicitArea.isEmpty() ? getLocalBounds().toFloat().reduced(2.f) : explicitArea;
        if(isSpeaker()) {const float d=juce::jmin(area.getWidth(),area.getHeight());return juce::Rectangle<float>(d,d).withCentre(area.getCentre());}
        if(index>=assetCount || !bank->images[index].isValid())return area;
        const auto source=rotation!=0 ? rotatedSourceBounds() : (trimArtwork ? bank->contentBounds[index] : bank->images[index].getBounds()).toFloat();
        const float scale=juce::jmin(area.getWidth()/float(source.getWidth()),area.getHeight()/float(source.getHeight()));
        return juce::Rectangle<float>(float(source.getWidth())*scale,float(source.getHeight())*scale).withCentre(area.getCentre());
    }
    bool isArtworkPoint(juce::Point<float> point) const noexcept {
        const auto index=static_cast<size_t>(selected);
        const auto bounds=artworkBounds();
        if(!bounds.contains(point) || index>=assetCount || !bank->images[index].isValid())return false;
        const auto source=trimArtwork ? bank->contentBounds[index] : bank->images[index].getBounds();
        if(rotation!=0) {
            const auto pixel=point.transformedBy(artworkTransform().inverted());
            if(pixel.x<0 || pixel.y<0 || pixel.x>=float(source.getWidth()) || pixel.y>=float(source.getHeight()))return false;
            return bank->images[index].getPixelAt(source.getX()+int(pixel.x),source.getY()+int(pixel.y)).getAlpha()>48;
        }
        const int x=juce::jlimit(source.getX(),source.getRight()-1,
            source.getX()+int((point.x-bounds.getX())*float(source.getWidth())/bounds.getWidth()));
        const int y=juce::jlimit(source.getY(),source.getBottom()-1,
            source.getY()+int((point.y-bounds.getY())*float(source.getHeight())/bounds.getHeight()));
        return bank->images[index].getPixelAt(x,y).getAlpha()>48;
    }
    void setAsset(Asset next,const juce::String& description,const juce::String& fallback="IR") {
        setTitle(description);setDescription(description);
        if(selected==next && emptyText==fallback)return;
        selected=next;emptyText=fallback;
        getProperties().set("cabArtworkKey",key(next));
        repaint();
    }
    void setActive(bool active) {
        const float next=active ? 1.f : .34f;
        if(opacity==next)return;
        opacity=next;setAlpha(opacity);
    }
    void paint(juce::Graphics& g) override {
        if(isSpeaker()) {
            const size_t style=speakerDriver>0 ? size_t(speakerDriver+1) : size_t(selected==Asset::bassSpeaker);
            g.setColour(juce::Colours::white);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(bank->frontSpeakers[style],artworkBounds(),juce::RectanglePlacement::stretchToFit);return;
        }
        const auto index=static_cast<size_t>(selected);
        if(index>=assetCount || !bank->images[index].isValid()) {
            neutral(g,getLocalBounds().toFloat(),emptyText);return;
        }
        g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        const auto source=trimArtwork ? bank->images[index].getClippedImage(bank->contentBounds[index]) : bank->images[index];
        if(rotation!=0)g.drawImageTransformed(source,artworkTransform());
        else g.drawImage(source,artworkBounds(),juce::RectanglePlacement::stretchToFit);
    }
};
}

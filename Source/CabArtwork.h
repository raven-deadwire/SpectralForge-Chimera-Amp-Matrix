#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ChimeraArtworkData.h"
#include "MicrophoneCatalog.h"
#include "LifecycleTrace.h"
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
inline Asset capturedMicrophone(const micCatalog::Model* model) {
    if(!model)return Asset::count;
    const juce::String wanted="mic-"+juce::String(model->id);
    for(size_t i=7;i<keys.size();++i)if(wanted==keys[i])return static_cast<Asset>(i);
    return Asset::count;
}

struct Bank {
    std::array<juce::Image,assetCount> images;
    std::array<juce::Rectangle<int>,assetCount> contentBounds;
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
    }
    ~Bank() {
        const lifecycle::Scope trace("cab.artwork.destroy",this);
        images.fill({});
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
public:
    View() {
        setInterceptsMouseClicks(false,false);setWantsKeyboardFocus(false);
        getProperties().set("cabArtworkKey",key(selected));
    }
    Asset asset() const noexcept {return selected;}
    bool hasImage() const noexcept {
        const auto index=static_cast<size_t>(selected);
        return index<assetCount && bank->images[index].isValid();
    }
    void setTrimArtwork(bool trim) {if(trimArtwork!=trim){trimArtwork=trim;repaint();}}
    juce::Rectangle<float> artworkBounds() const noexcept {
        const auto index=static_cast<size_t>(selected);
        const auto area=getLocalBounds().toFloat().reduced(2.f);
        if(index>=assetCount || !bank->images[index].isValid())return area;
        const auto source=trimArtwork ? bank->contentBounds[index] : bank->images[index].getBounds();
        const float scale=juce::jmin(area.getWidth()/float(source.getWidth()),area.getHeight()/float(source.getHeight()));
        return juce::Rectangle<float>(float(source.getWidth())*scale,float(source.getHeight())*scale).withCentre(area.getCentre());
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
        const auto index=static_cast<size_t>(selected);
        if(index>=assetCount || !bank->images[index].isValid()) {
            neutral(g,getLocalBounds().toFloat(),emptyText);return;
        }
        g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        const auto source=trimArtwork ? bank->images[index].getClippedImage(bank->contentBounds[index]) : bank->images[index];
        g.drawImage(source,getLocalBounds().toFloat().reduced(2.f),juce::RectanglePlacement::centred);
    }
};
}

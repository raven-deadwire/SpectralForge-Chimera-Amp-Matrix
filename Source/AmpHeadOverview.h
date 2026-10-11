#pragma once
#include "AmpHeadKnobLook.h"
#include "AmpHeadLayout.h"
#include "CabHeadView.h"
#include "PluginProcessor.h"

namespace spectralforge::ampHeadOverview {
inline void paintChannel(juce::Graphics& g,juce::Rectangle<float> face,int model,
                         bool selected,bool over=false,bool focused=false,const juce::String& label={}) {
    const auto lit=juce::Colour(model==firstOriginalAmpModel ? 0xffc28320 : 0xff507d99);
    const auto colour=(selected?lit:juce::Colour(0xff171d20)).brighter(over?.15f:0.f);
    g.setGradientFill(juce::ColourGradient(colour.brighter(.12f),face.getTopLeft(),
                                         colour.darker(.3f),face.getBottomLeft(),false));
    g.fillRect(face);
    if(focused) {g.setColour(juce::Colour(0xffb3e1ff));g.drawRoundedRectangle(face,2.f,1.f);}
    if(label.isNotEmpty()) {
        g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(12.f,juce::Font::bold));
        g.drawText(label,face.toNearestInt(),juce::Justification::centred);
    }
}

// The cabinet/compact head is an overview, so it paints the same knob material
// without labels or hit targets. Positions always read the selected native bank;
// opening the head therefore reveals the same sound and channel, not dummy art.
class Painter final {
    ampHeadKnob::Renderer knobs;
    struct Control {
        juce::Point<float> anchor;
        const char* key{};
        const std::atomic<float>* parameter{};
        juce::NormalisableRange<float> range;
        float value{};
    };
    int model{-1},context{-1},channel{-1};
    std::vector<Control> controls;
public:
    bool refresh(ChimeraProcessor& processor,int lane) {
        const int nextModel=processor.selectedAmpModel(lane),nextChannel=processor.selectedAmpChannel(lane);
        const int mode=int(processor.parameters().getRawParameterValue("mode")->load());
        const int nextContext=ampNativeContext(mode,lane);
        bool changed=model!=nextModel || context!=nextContext || channel!=nextChannel;
        if(changed) {
            model=nextModel;context=nextContext;channel=nextChannel;controls.clear();
            const auto& panel=ampNativePanel(model);
            for(const auto& anchor:ampHead::knobs(model,channel)) {
                const int index=ampHead::controlIndex(model,anchor.key);
                if(index<0)continue;
                const auto* parameter=processor.parameters().getRawParameterValue(ampNativeControlID(context,model,index,channel));
                if(!parameter) {jassertfalse;continue;}
                const auto& spec=panel.controls[size_t(index)];
                juce::NormalisableRange<float> range(float(spec.minimum),float(spec.maximum));
                if(model==firstOriginalAmpModel && index==int(original::Control::midFrequency))range.setSkewForCentre(850.f);
                else if(model==niflheimrAmpModel && index==int(niflheimr::Control::midFrequency))range.setSkewForCentre(650.f);
                controls.push_back({{anchor.x,anchor.y},anchor.key,parameter,range,parameter->load()});
            }
        }
        // The APVTS parameter objects live for the processor lifetime. Idle
        // timer ticks only read the visible bank's atomics; they do not allocate,
        // resolve IDs, or invalidate the large shell image.
        for(auto& control:controls) {
            const float value=control.parameter->load();
            if(control.value!=value) {control.value=value;changed=true;}
        }
        return changed;
    }
    void paint(juce::Graphics& g,juce::Rectangle<float> bounds) {
        // Only the owner refreshes this snapshot and requests a full repaint.
        // An expose paint may cover just a clip; consuming a newer value here
        // could leave its knob outside that clip stale on subsequent idle ticks.
        jassert(model>=0);
        if(model<0)return;
        const auto layout=cabHead::geometry(bounds,model);
        const auto& style=ampHead::faceStyle(model);
        const float diameter=layout.front.getWidth()*style.knobDiameter;
        for(const auto& control:controls) {
            const auto centre=layout.front.getPosition()+juce::Point<float>{
                layout.front.getWidth()*control.anchor.x,layout.front.getHeight()*control.anchor.y};
            knobs.draw(g,{centre.x-diameter*.5f,centre.y-diameter*.5f,diameter,diameter},
                       control.range.convertTo0to1(control.value),juce::MathConstants<float>::pi*1.2f,
                       juce::MathConstants<float>::pi*2.8f,model,false,control.key);
        }
        const auto& windows=headShell::definition(model).channelWindows;
        for(size_t i=0;i<windows.size();++i)if(windows[i].valid())
            paintChannel(g,cabHead::pixels(windows[i]).transformedBy(layout.sourceToView),model,int(i)==channel);
    }
};
} // namespace spectralforge::ampHeadOverview

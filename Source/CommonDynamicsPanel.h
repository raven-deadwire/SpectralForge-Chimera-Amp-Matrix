#pragma once
#include "CommonDynamics.h"
#include "GraphicalEQ.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace spectralforge {
class CommonDynamicsPanel final : public juce::Component,private juce::Timer {
public:
    CommonDynamicsPanel(juce::AudioProcessorValueTreeState& s,CommonDynamics& d,GraphicalEQ& tone,GraphicalEQ& final)
        :state(s),dsp(d),eq{&tone,&final} {
        setComponentID("commonDynamics.panel");setOpaque(true);
        toggle(compOn,"COMPRESSOR","cleanCompOn");toggle(limiterOn,"LIMITER","liveLimiterOn");
        choice(detector,"cleanCompDetector",{"Peak","RMS"});choice(gateMode,"gateMode",{"Gate","Expander"});
        constexpr std::array<const char*,8> compIDs{"cleanCompThreshold","cleanCompRatio","cleanCompAttack","cleanCompRelease","cleanCompKnee","cleanCompMakeup","cleanCompMix","cleanCompHP"};
        constexpr std::array<const char*,8> compNames{"Threshold dB","Ratio","Attack ms","Release ms","Knee dB","Makeup dB","Parallel mix","Detector HP Hz"};
        for(size_t i=0;i<comp.size();++i)control(comp[i],compIDs[i],compNames[i]);
        constexpr std::array<const char*,6> gateIDs{"gatethreshold","gateRatio","gateAttack","gatehold","gaterelease","gateRangeDb"};
        constexpr std::array<const char*,6> gateNames{"Threshold dB","Ratio","Attack ms","Hold ms","Release ms","Range dB"};
        for(size_t i=0;i<gate.size();++i)control(gate[i],gateIDs[i],gateNames[i]);
        gate[1].slider.setTooltip("Downward expansion ratio; active only in Expander mode.");
        control(ceiling,"liveLimiterCeiling","Ceiling dBFS");control(limitRelease,"liveLimiterRelease","Release ms");
        gateMode.setTooltip("Uses the clean DI detector before PRE; attenuation follows the existing PRE / POST GATE placement. Enable GATE in the input controls.");
        limiterOn.setTooltip("Stereo-linked sample peak limiter after output trim. No lookahead or extra latency; intersample peaks are not measured.");
        compOn.setTooltip("Clean stereo-linked compressor after Tone EQ, before POST. Parallel mix blends aligned dry and compressed audio.");
        constexpr std::array<const char*,5> fields{"threshold","range","attack","release","ratio"};
        constexpr std::array<const char*,5> names{"Threshold dB","Max cut dB","Attack ms","Release ms","Ratio"};
        for(int i=0;i<4;++i){
            auto& slot=slots[size_t(i)];slot.target.addItemList(dynamicTargetNames(),1);slot.target.setComponentID(dynamicSlotID(i,"target"));
            slot.target.setName("Dynamic EQ "+juce::String(i+1)+" existing node");addAndMakeVisible(slot.target);
            slot.target.setTooltip("Select one existing Bell or shelf node. Enable the node and its EQ bank. Four slots are shared by Tone and Final EQ.");
            slot.targetAttachment=std::make_unique<juce::ParameterAttachment>(*state.getParameter(dynamicSlotID(i,"target")),[this,i](float v){slots[size_t(i)].target.setSelectedId(int(v)+1,juce::dontSendNotification);},nullptr);
            slot.targetAttachment->sendInitialUpdate();
            slot.target.onChange=[this,i]{
                auto& current=slots[size_t(i)];const int target=current.target.getSelectedId()-1;
                for(int other=0;other<4;++other)if(other!=i && target!=0 && assignment(other)==target){
                    current.target.setSelectedId(assignment(i)+1,juce::dontSendNotification);notice="That node already has a dynamic slot. Its settings were kept.";repaint();return;
                }
                current.targetAttachment->setValueAsCompleteGesture(float(target));notice.clear();refresh();repaint();
            };
            for(size_t f=0;f<fields.size();++f)control(slot.controls[f],dynamicSlotID(i,fields[f]),names[f]);
            slot.status.setFont(juce::FontOptions(12.f));slot.status.setComponentID("dynamicEQ"+juce::String(i+1)+".status");addAndMakeVisible(slot.status);
        }
        refresh();startTimerHz(20);
    }
    ~CommonDynamicsPanel() override {stopTimer();}
    void resized() override {
        compOn.setBounds(18,44,150,28);detector.setBounds(178,44,110,28);
        gateMode.setBounds(522,44,140,28);limiterOn.setBounds(884,44,150,28);
        for(int i=0;i<8;++i)comp[size_t(i)].bounds(18+(i%4)*120,82+(i/4)*57,114);
        for(int i=0;i<6;++i)gate[size_t(i)].bounds(522+(i%3)*114,82+(i/3)*57,108);
        ceiling.bounds(884,82,232);limitRelease.bounds(884,139,232);
        for(int s=0;s<4;++s){auto& slot=slots[size_t(s)];const int y=259+s*35;
            slot.target.setBounds(40,y,184,28);
            for(int f=0;f<5;++f){auto& c=slot.controls[size_t(f)];c.label.setVisible(false);c.slider.setBounds(236+f*150,y,140,28);}
            slot.status.setBounds(995,y,128,28);
        }
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff121a1d));g.setColour(juce::Colour(0xff273b3d));
        g.drawVerticalLine(508,42.f,198.f);g.drawVerticalLine(872,42.f,198.f);g.drawHorizontalLine(205,18.f,float(getWidth()-18));
        g.setColour(juce::Colour(0xffd9ebe3));g.setFont(juce::FontOptions(16.f,juce::Font::bold));
        g.drawText("CLEAN COMPRESSION",18,9,330,26,juce::Justification::centredLeft);
        g.drawText("DI GATE / EXPANDER",522,9,330,26,juce::Justification::centredLeft);
        g.drawText("FINAL OUTPUT",884,9,232,26,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(12.5f));g.setColour(juce::Colour(0xff9fb4ad));
        g.drawText("Before POST   /   GR "+juce::String(dsp.compReduction.load(),1)+" dB",300,49,192,20,juce::Justification::centredRight);
        g.drawText(state.getRawParameterValue("gateon")->load()>.5f?"GATE ENABLED":"GATE OFF",674,49,180,20,juce::Justification::centredRight);
        g.drawText("GR "+juce::String(dsp.limitReduction.load(),1)+" dB",1036,49,82,20,juce::Justification::centredRight);
        g.setColour(juce::Colour(0xffd9ebe3));g.setFont(juce::FontOptions(14.f,juce::Font::bold));
        g.drawText("DYNAMIC EQ  /  "+juce::String(used)+" OF 4 SHARED SLOTS",18,208,460,25,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff9fb4ad));g.setFont(juce::FontOptions(12.f));
        g.drawText(notice.isEmpty()?"Existing Bell / shelf nodes  •  Live mode  •  No added latency":notice,465,208,654,25,juce::Justification::centredRight);
        constexpr std::array<const char*,5> names{"THRESHOLD dB","MAX CUT dB","ATTACK ms","RELEASE ms","RATIO"};
        g.drawText("EXISTING EQ NODE",40,236,184,19,juce::Justification::centredLeft);
        for(int f=0;f<5;++f)g.drawText(names[size_t(f)],236+f*150,236,140,19,juce::Justification::centredLeft);
        g.drawText("STATUS / GR",995,236,128,19,juce::Justification::centredLeft);
        for(int s=0;s<4;++s)g.drawText(juce::String(s+1),18,259+s*35,18,28,juce::Justification::centredLeft);
    }
private:
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Control {juce::Label label;juce::Slider slider;std::unique_ptr<SA> attachment;void bounds(int x,int y,int width){label.setBounds(x,y,width,18);slider.setBounds(x,y+19,width,28);}};
    struct Slot {juce::ComboBox target;std::unique_ptr<juce::ParameterAttachment> targetAttachment;std::array<Control,5> controls;juce::Label status;};
    void control(Control& c,const juce::String& id,const char* title){
        c.label.setText(title,juce::dontSendNotification);c.label.setFont(juce::FontOptions(12.f));addAndMakeVisible(c.label);
        const bool sharedGateControl=id=="gatethreshold" || id=="gatehold" || id=="gaterelease" || id=="gateRangeDb";
        c.slider.setName(title);c.slider.setComponentID(sharedGateControl?"commonDynamics."+id:id);c.slider.setSliderStyle(juce::Slider::LinearHorizontal);
        c.slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,61,26);c.slider.setWantsKeyboardFocus(true);c.slider.setNumDecimalPlacesToDisplay(2);addAndMakeVisible(c.slider);
        c.attachment=std::make_unique<SA>(state,id,c.slider);
        if(id=="cleanCompMix"){
            c.slider.textFromValueFunction=[](double v){return juce::String(v*100,1)+" %";};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100.;};
        }else if(id=="gateRangeDb"){
            c.slider.textFromValueFunction=[](double v){return v>=95.95?juce::String("Full"):juce::String(v,1);};
            c.slider.valueFromTextFunction=[](const juce::String& text){return text.trim().equalsIgnoreCase("Full")?96.:text.getDoubleValue();};
        }
        c.slider.updateText();
    }
    void toggle(juce::TextButton& b,const char* title,const char* id){b.setButtonText(title);b.setClickingTogglesState(true);b.setComponentID(id);addAndMakeVisible(b);buttons.push_back(std::make_unique<BA>(state,id,b));}
    void choice(juce::ComboBox& box,const char* id,const juce::StringArray& names){box.addItemList(names,1);box.setComponentID(id);addAndMakeVisible(box);choices.push_back(std::make_unique<CA>(state,id,box));}
    int assignment(int i)const{return int(state.getRawParameterValue(dynamicSlotID(i,"target"))->load());}
    void refresh(){
        const int oldUsed=used;bool changed=false;
        gate[1].slider.setEnabled(state.getRawParameterValue("gateMode")->load()>.5f);
        used=0;
        for(int i=0;i<4;++i){auto& slot=slots[size_t(i)];const int target=assignment(i);used+=target!=0;bool duplicate=false;
            for(int j=0;j<i;++j)duplicate|=target!=0 && assignment(j)==target;
            juce::String status="Unassigned";bool enabled=false;
            if(target>0 && target<=24){const int bank=(target-1)/12,band=(target-1)%12;const auto v=eq[size_t(bank)]->values(band);
                enabled=!duplicate && v.type<=2;status=duplicate?"Duplicate / idle":v.type>2?"Bell / shelf only":!v.enabled?"Band off":eq[size_t(bank)]->isBypassed()?"EQ bypassed":juce::String(eq[size_t(bank)]->dynamicReduction(band),1)+" dB GR";
            }
            changed|=slot.status.getText()!=status;slot.status.setText(status,juce::dontSendNotification);for(auto& c:slot.controls)c.slider.setEnabled(enabled);
            for(int t=1;t<=24;++t){bool occupied=false;for(int j=0;j<4;++j)occupied|=j!=i && assignment(j)==t;slot.target.setItemEnabled(t+1,!occupied || t==target);}
        }
        const std::array<int,3> header{juce::roundToInt(dsp.compReduction.load()*10),juce::roundToInt(dsp.limitReduction.load()*10),int(state.getRawParameterValue("gateon")->load()>.5f)};
        if(changed || used!=oldUsed || header!=lastHeader){lastHeader=header;repaint();}
    }
    void timerCallback() override {if(isShowing())refresh();}
    juce::AudioProcessorValueTreeState& state;CommonDynamics& dsp;std::array<GraphicalEQ*,2> eq;
    juce::TextButton compOn,limiterOn;juce::ComboBox detector,gateMode;
    std::array<Control,8> comp;std::array<Control,6> gate;Control ceiling,limitRelease;std::array<Slot,4> slots;
    std::vector<std::unique_ptr<BA>> buttons;std::vector<std::unique_ptr<CA>> choices;int used{};juce::String notice;std::array<int,3> lastHeader{-1,-1,-1};
};
}

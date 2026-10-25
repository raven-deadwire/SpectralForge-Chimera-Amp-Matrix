#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace spectralforge {
// Presentation of an Auto Set run: what the current pass listens for, what it
// has heard and how long it still needs; then one row per finished pass.
struct AutoSetReport {
    bool listening{},finished{};
    juce::StringArray steps;int current{-1};          // pass names, current pass while listening
    double seconds{},totalSeconds{},passSeconds{},passTotalSeconds{};
    float levelDb{-120.f};
    juce::String listeningFor,heard,message;
    struct Row {juce::String name,result;bool changed{};int target{};};
    std::vector<Row> rows;                             // target: caller's id for SHOW
};

class AutoSetView final : public juce::Component {
public:
    AutoSetView() {
        setComponentID("autoSet.view");
        close.setComponentID("autoSet.close");close.onClick=[this]{if(onClose)onClose();};addChildComponent(close);
    }
    std::function<void(int)> onShow;
    std::function<void()> onClose;
    const AutoSetReport& report() const noexcept {return current;}
    void setReport(const AutoSetReport& next) {
        bool rowsChanged=next.rows.size()!=current.rows.size() || next.finished!=current.finished;
        for(size_t i=0;!rowsChanged && i<next.rows.size();++i)rowsChanged=next.rows[i].target!=current.rows[i].target || next.rows[i].name!=current.rows[i].name;
        current=next;
        if(rowsChanged) {
            show.clear();
            if(current.finished)for(size_t i=0;i<current.rows.size();++i) {
                auto& button=*show.emplace_back(std::make_unique<juce::TextButton>("SHOW"));
                button.setComponentID("autoSet.show."+juce::String(int(i)));
                button.setTooltip("Open "+current.rows[i].name);
                const int target=current.rows[i].target;button.onClick=[this,target]{if(onShow)onShow(target);};
                addAndMakeVisible(button);
            }
            close.setVisible(current.finished);resized();
        }
        repaint();
    }
    int preferredHeight() const {
        if(current.listening)return 150;
        return 44+int(current.rows.size())*26+(current.message.isNotEmpty()?22:0);
    }
    void resized() override {
        close.setBounds(getWidth()-86,8,74,24);
        for(size_t i=0;i<show.size();++i)show[i]->setBounds(getWidth()-76,40+int(i)*26,64,22);
    }
    void paint(juce::Graphics& g) override {
        const juce::Colour text(0xffe0e8e5),muted(0xff9fb4ad),accent(0xffd3b478),good(0xff8fd0a8),line(0xff2c3d40);
        g.setColour(juce::Colour(0xf0172225));g.fillRoundedRectangle(getLocalBounds().toFloat(),6.f);
        g.setColour(line);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),6.f,1.f);
        const int width=getWidth();
        const auto seconds=[](double s){return juce::String(juce::roundToInt(s))+" s";};
        if(current.listening) {
            g.setColour(text);g.setFont(juce::FontOptions(14.f,juce::Font::bold));
            g.drawText("AUTO SET  /  step "+juce::String(current.current+1)+" of "+juce::String(current.steps.size())+": "+current.steps[current.current],
                       12,8,width-24,20,juce::Justification::centredLeft);
            g.setColour(accent);g.setFont(juce::FontOptions(13.f));
            g.drawText(seconds(current.seconds)+" of about "+seconds(current.totalSeconds),width-200,8,188,20,juce::Justification::centredRight);
            // Pass dots: done, current, still to come.
            const int dotWidth=(width-24)/std::max(1,current.steps.size());
            for(int i=0;i<current.steps.size();++i) {
                const int x=12+i*dotWidth;const bool done=i<current.current,now=i==current.current;
                g.setColour(done?good:now?accent:muted.withAlpha(.5f));g.fillEllipse(float(x),36.f,9.f,9.f);
                g.setFont(juce::FontOptions(11.5f,now?juce::Font::bold:juce::Font::plain));
                g.drawText(current.steps[i],x+13,31,dotWidth-16,18,juce::Justification::centredLeft);
            }
            g.setColour(muted);g.setFont(juce::FontOptions(12.5f));
            g.drawText("Listening for: "+current.listeningFor,12,56,width-24,18,juce::Justification::centredLeft);
            g.setColour(text);g.drawText(current.heard,12,76,width-24,18,juce::Justification::centredLeft);
            // Level meter: -60..0 dBFS, the playing threshold sits at the left edge.
            const auto meter=juce::Rectangle<float>(12.f,102.f,float(width-120),8.f);
            g.setColour(line);g.fillRoundedRectangle(meter,3.f);
            const float fill=juce::jlimit(0.f,1.f,(current.levelDb+60.f)/60.f);
            g.setColour(current.levelDb>-6.f?juce::Colour(0xffe08b6f):good);g.fillRoundedRectangle(meter.withWidth(meter.getWidth()*fill),3.f);
            g.setColour(muted);g.setFont(juce::FontOptions(11.5f));
            g.drawText(current.levelDb<=-119.f?juce::String("level  --"):"level "+juce::String(current.levelDb,1)+" dBFS",width-104,97,96,18,juce::Justification::centredRight);
            // This pass's own progress.
            const auto bar=juce::Rectangle<float>(12.f,124.f,float(width-120),5.f);
            g.setColour(line);g.fillRoundedRectangle(bar,2.f);
            g.setColour(accent);g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*float(current.passSeconds/std::max(.001,current.passTotalSeconds))),2.f);
            g.setColour(muted);g.drawText("this step "+seconds(current.passSeconds)+" / "+seconds(current.passTotalSeconds),width-104,117,96,18,juce::Justification::centredRight);
            return;
        }
        g.setColour(text);g.setFont(juce::FontOptions(14.f,juce::Font::bold));
        g.drawText(current.rows.empty()?"AUTO SET":"AUTO SET RESULTS",12,8,width-110,22,juce::Justification::centredLeft);
        for(size_t i=0;i<current.rows.size();++i) {
            const auto& row=current.rows[i];const int y=40+int(i)*26;
            g.setColour(row.changed?good:muted);g.setFont(juce::FontOptions(13.f,juce::Font::bold));
            g.drawText(row.changed?juce::String::fromUTF8("\xe2\x9c\x93"):juce::String("-"),12,y,16,22,juce::Justification::centredLeft);
            g.setColour(text);g.drawText(row.name,30,y,140,22,juce::Justification::centredLeft);
            g.setColour(row.changed?text:muted);g.setFont(juce::FontOptions(12.5f));
            g.drawFittedText(row.result,172,y,width-260,22,juce::Justification::centredLeft,1);
        }
        if(current.message.isNotEmpty()) {
            g.setColour(accent);g.setFont(juce::FontOptions(12.5f));
            g.drawFittedText(current.message,12,40+int(current.rows.size())*26,width-24,20,juce::Justification::centredLeft,1);
        }
    }
private:
    AutoSetReport current;
    juce::TextButton close{"CLOSE"};
    std::vector<std::unique_ptr<juce::TextButton>> show;
};
}

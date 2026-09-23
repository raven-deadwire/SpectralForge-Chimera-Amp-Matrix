#include "PluginEditor.h"

namespace {
void styleLabel(juce::Label& l, float size=13.f) {
    l.setColour(juce::Label::textColourId, juce::Colour(0xffd9dde6));
    l.setFont(juce::FontOptions(size));
    l.setJustificationType(juce::Justification::centred);
}
}

ChimeraEditor::ChimeraEditor(ChimeraProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    title.setText("SpectralForge  |  CHIMERA AMP MATRIX  1.0 TEST",juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::FontOptions(23.f));
    addAndMakeVisible(title);

    mode.addItemList({"CLASSIC","DUAL","MATRIX"},1);
    mode.setTooltip("CLASSIC: one full-range rig | DUAL: two full-range rigs blended | MATRIX: crossover-split LOW/MID/HIGH rigs");
    addAndMakeVisible(mode);

    styleLabel(routingHelp,13.f); addAndMakeVisible(routingHelp);
    styleLabel(x1Label,12.f); addAndMakeVisible(x1Label);
    styleLabel(x2Label,12.f); addAndMakeVisible(x2Label);

    setupKnob(x1,"LOW / MID"," Hz");
    setupKnob(x2,"MID / HIGH"," Hz");
    addAndMakeVisible(x1); addAndMakeVisible(x2);

    auto& st=p.parameters();
    ma=std::make_unique<CA>(st,"mode",mode);
    a1=std::make_unique<SA>(st,"x1",x1);
    a2=std::make_unique<SA>(st,"x2",x2);

    const std::array<juce::String,8> knobNames{"DRIVE","LEVEL","BASS","LOW MID","HIGH MID","TREBLE","PRESENCE","RESONANCE"};
    for(int i=0;i<3;++i){
        auto& l=lanes[i]; auto n=juce::String(i+1);
        styleLabel(l.header,16.f); styleLabel(l.range,12.f);
        addAndMakeVisible(l.header); addAndMakeVisible(l.range);
        l.amp.addItemList({"Glass","Brit Edge","Tight 515","Wide Rect","Liquid Lead","Iron Tube","Solid Punch","Modern Bass"},1);
        l.amp.setTooltip("Amplifier model for this rig/lane");
        addAndMakeVisible(l.amp);
        l.aa=std::make_unique<CA>(st,"amp"+n,l.amp);
        std::array<juce::Slider*,8> s{&l.drive,&l.level,&l.bass,&l.lm,&l.hm,&l.treble,&l.pres,&l.res};
        std::array<juce::String,8> id{"drive"+n,"level"+n,"bass"+n,"lowmid"+n,"highmid"+n,"treble"+n,"presence"+n,"resonance"+n};
        for(size_t k=0;k<s.size();++k){
            setupKnob(*s[k],knobNames[k],k==1?" dB":(k>=2&&k<=5?" dB":""));
            styleLabel(l.knobLabels[k],11.f);
            l.knobLabels[k].setText(knobNames[k],juce::dontSendNotification);
            addAndMakeVisible(l.knobLabels[k]);
            addAndMakeVisible(*s[k]);
            l.sa[k]=std::make_unique<SA>(st,id[k],*s[k]);
        }
    }
    setSize(1180,650);
    startTimerHz(12);
    updateModeUI();
}

void ChimeraEditor::setupKnob(juce::Slider& s,const juce::String& name,const juce::String& suffix){
    s.setName(name); s.setTooltip(name);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,68,18);
    if(suffix.isNotEmpty())s.setTextValueSuffix(suffix);
}

void ChimeraEditor::timerCallback(){
    const int m=(int)processor.parameters().getRawParameterValue("mode")->load();
    if(m!=lastMode)updateModeUI();
    if(m==2){
        const int lo=(int)processor.parameters().getRawParameterValue("x1")->load();
        const int hi=(int)processor.parameters().getRawParameterValue("x2")->load();
        lanes[0].range.setText("20 Hz  –  "+juce::String(lo)+" Hz",juce::dontSendNotification);
        lanes[1].range.setText(juce::String(lo)+" Hz  –  "+juce::String(hi)+" Hz",juce::dontSendNotification);
        lanes[2].range.setText(juce::String(hi)+" Hz  –  20 kHz",juce::dontSendNotification);
    }
}

void ChimeraEditor::updateModeUI(){
    lastMode=(int)processor.parameters().getRawParameterValue("mode")->load();
    const bool matrix=lastMode==2, dual=lastMode==1;
    x1.setVisible(matrix); x2.setVisible(matrix); x1Label.setVisible(matrix); x2Label.setVisible(matrix);
    lanes[1].header.setVisible(dual||matrix); lanes[1].range.setVisible(dual||matrix); lanes[1].amp.setVisible(dual||matrix);
    lanes[2].header.setVisible(matrix); lanes[2].range.setVisible(matrix); lanes[2].amp.setVisible(matrix);
    for(int i=0;i<3;++i){
        std::array<juce::Slider*,8>s{&lanes[i].drive,&lanes[i].level,&lanes[i].bass,&lanes[i].lm,&lanes[i].hm,&lanes[i].treble,&lanes[i].pres,&lanes[i].res};
        const bool show=i==0||(dual&&i<2)||matrix;
        for(size_t k=0;k<s.size();++k){ s[k]->setVisible(show); lanes[i].knobLabels[k].setVisible(show); }
    }
    if(lastMode==0){
        routingHelp.setText("CLASSIC  •  Single full-range amp/cab rig  •  No crossover",juce::dontSendNotification);
        lanes[0].header.setText("FULL-RANGE RIG",juce::dontSendNotification);
        lanes[0].range.setText("20 Hz  –  20 kHz",juce::dontSendNotification);
    } else if(lastMode==1){
        routingHelp.setText("DUAL  •  Two independent full-range rigs mixed in parallel  •  No crossover",juce::dontSendNotification);
        lanes[0].header.setText("FULL-RANGE RIG A",juce::dontSendNotification);
        lanes[1].header.setText("FULL-RANGE RIG B",juce::dontSendNotification);
        lanes[0].range.setText("20 Hz  –  20 kHz",juce::dontSendNotification);
        lanes[1].range.setText("20 Hz  –  20 kHz",juce::dontSendNotification);
    } else {
        routingHelp.setText("MATRIX  •  LR4 (24 dB/oct) crossover  •  Each band feeds its own amp/cab rig",juce::dontSendNotification);
        lanes[0].header.setText("LOW BAND",juce::dontSendNotification);
        lanes[1].header.setText("MID BAND",juce::dontSendNotification);
        lanes[2].header.setText("HIGH BAND",juce::dontSendNotification);
        x1Label.setText("LOW / MID CROSSOVER",juce::dontSendNotification);
        x2Label.setText("MID / HIGH CROSSOVER",juce::dontSendNotification);
    }
    resized(); repaint();
}

void ChimeraEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff101217));
    g.setColour(juce::Colour(0xffe2e5ea)); g.setFont(juce::FontOptions(18.f));
    g.drawText("ROUTING MODE",25,68,170,24,juce::Justification::centredLeft);
    const int count=lastMode==0?1:lastMode==1?2:3;
    const float gap=14.f, left=20.f, width=(getWidth()-40.f-gap*(count-1))/count;
    for(int i=0;i<count;++i){
        float x=left+i*(width+gap);
        g.setColour(juce::Colour(0xff292d36)); g.fillRoundedRectangle(x,215.f,width,405.f,9.f);
        g.setColour(juce::Colour(0xff555d6c)); g.drawRoundedRectangle(x,215.f,width,405.f,9.f,1.2f);
    }
}

void ChimeraEditor::resized(){
    title.setBounds(20,12,getWidth()-40,38);
    mode.setBounds(25,98,170,30);
    routingHelp.setBounds(210,94,getWidth()-230,36);
    if(lastMode==2){
        x1Label.setBounds(225,136,180,20); x1.setBounds(260,153,110,58);
        x2Label.setBounds(405,136,180,20); x2.setBounds(440,153,110,58);
    }
    const int count=lastMode==0?1:lastMode==1?2:3;
    const int gap=14,left=20,width=(getWidth()-40-gap*(count-1))/count;
    for(int i=0;i<count;++i){
        auto& l=lanes[i]; int x=left+i*(width+gap);
        l.header.setBounds(x+10,226,width-20,24);
        l.range.setBounds(x+10,250,width-20,20);
        l.amp.setBounds(x+20,278,width-40,28);
        std::array<juce::Slider*,8>s{&l.drive,&l.level,&l.bass,&l.lm,&l.hm,&l.treble,&l.pres,&l.res};
        const int kw=juce::jmin(84,(width-30)/4);
        for(int k=0;k<8;++k){
            const int kx=x+12+(k%4)*(width-24)/4;
            const int ky=318+(k/4)*135;
            l.knobLabels[k].setBounds(kx,ky,kw,18);
            s[k]->setBounds(kx,ky+18,kw,94);
        }
    }
}
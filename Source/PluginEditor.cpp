#include "PluginEditor.h"

namespace {
const juce::Colour background(0xff101313),panel(0xff1b1f1f),line(0xff343a39),ink(0xffe6e4db),muted(0xffa0aaa5),accent(0xffe8a565);
void style(juce::Label& label,float size=13.f,juce::Justification justification=juce::Justification::centredLeft)
{
    label.setFont(juce::FontOptions(size)); label.setColour(juce::Label::textColourId,ink);
    label.setJustificationType(justification);
}
juce::String frequency(float hz) { return hz>=1000 ? juce::String(hz/1000,2)+" kHz" : juce::String(juce::roundToInt(hz))+" Hz"; }
void text(juce::Graphics& g,const juce::String& value,int x,int y,int width,int height,float size=12.f,juce::Colour colour=muted,
          juce::Justification justification=juce::Justification::centredLeft)
{
    g.setColour(colour); g.setFont(juce::FontOptions(size)); g.drawText(value,x,y,width,height,justification);
}
void meter(juce::Graphics& g,float gain,int x,int y,int width,int height)
{
    g.setColour(line); g.fillRoundedRectangle(float(x),float(y),float(width),float(height),2.f);
    const float value=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(gain,-60.f)+60.f)/60.f);
    g.setColour(gain>=1.f ? juce::Colour(0xfff26a5f) : accent);
    g.fillRoundedRectangle(float(x),float(y)+height*(1.f-value),float(width),height*value,2.f);
}
}
ChimeraLookAndFeel::ChimeraLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,ink); setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId,background); setColour(juce::Slider::textBoxHighlightColourId,accent.withAlpha(.35f));
    setColour(juce::ComboBox::backgroundColourId,background); setColour(juce::ComboBox::textColourId,ink);
    setColour(juce::ComboBox::outlineColourId,line); setColour(juce::ComboBox::arrowColourId,accent);
    setColour(juce::PopupMenu::backgroundColourId,panel); setColour(juce::PopupMenu::textColourId,ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,accent); setColour(juce::PopupMenu::highlightedTextColourId,background);
    setColour(juce::TextButton::textColourOffId,ink); setColour(juce::TextButton::textColourOnId,background);
    setColour(juce::TooltipWindow::backgroundColourId,ink); setColour(juce::TooltipWindow::textColourId,background);
}
juce::Font ChimeraLookAndFeel::getTextButtonFont(juce::TextButton&,int) { return juce::FontOptions(11.5f,juce::Font::bold); }
juce::Font ChimeraLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::FontOptions(14.f); }
void ChimeraLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool hover,bool down)
{
    auto area=button.getLocalBounds().toFloat().reduced(.5f);
    g.setColour(button.getToggleState() ? accent : (hover || down ? line.brighter(.15f) : background));
    g.fillRoundedRectangle(area,3.f); g.setColour(button.hasKeyboardFocus(true) ? accent : line); g.drawRoundedRectangle(area,3.f,1.f);
}
void ChimeraLookAndFeel::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float,float,
                                         juce::Slider::SliderStyle,juce::Slider& slider)
{
    const float mid=float(y)+height*.5f;
    g.setColour(line); g.drawLine(float(x),mid,float(x+width),mid,3.f);
    const float zero=slider.getMinimum()<0 && slider.getMaximum()>0 ? float(x)+float(width*slider.valueToProportionOfLength(0)) : float(x);
    g.setColour(slider.isEnabled() ? accent : muted); g.drawLine(zero,mid,position,mid,3.f);
    g.setColour(ink); g.fillRoundedRectangle(position-2.f,mid-7.f,4.f,14.f,1.f);
}
void ChimeraLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider&)
{
    const float radius=juce::jmin(float(width),float(height))*.39f;
    const juce::Point<float> centre(float(x)+width*.5f,float(y)+height*.5f);
    const float angle=start+value*(end-start);
    juce::Path track;track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    g.setColour(line);g.strokePath(track,juce::PathStrokeType(3));
    juce::Path fill;fill.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
    g.setColour(accent);g.strokePath(fill,juce::PathStrokeType(3));
    g.setColour(background);g.fillEllipse(centre.x-radius+7,centre.y-radius+7,(radius-7)*2,(radius-7)*2);
    const auto a=centre.getPointOnCircumference(radius*.40f,angle),b=centre.getPointOnCircumference(radius*.73f,angle);
    g.setColour(ink);g.drawLine({a,b},2.5f);
}
ChimeraEditor::ChimeraEditor(ChimeraProcessor& p) : AudioProcessorEditor(&p),processor(p)
{
    setLookAndFeel(&look); canvas.setComponentID("surface"); addAndMakeVisible(canvas);
    auto add=[this](juce::Component& component){canvas.addAndMakeVisible(component);};
    style(title,28.f); title.setText("CHIMERA",juce::dontSendNotification); add(title);
    mode.setName("Routing Mode"); mode.addItemList({"CLASSIC","DUAL","MATRIX"},1); add(mode);
    mode.setTooltip("Classic: one full-range rig. Dual: two parallel rigs. Matrix: three input bands.");
    quality.setName("Oversampling"); quality.addItemList({"1x","2x","4x","8x"},1); add(quality);
    quality.setTooltip("Anti-aliasing for amplifier distortion. Higher factors use more CPU. Host latency stays constant.");
    scale.setName("Interface size"); scale.addItemList({"75%","100%","125%","150%"},1); scale.setSelectedId(2); add(scale);
    scale.onChange=[this]{const float factor=float(scale.getSelectedId()+2)*.25f; setSize(juce::roundToInt(1180*factor),juce::roundToInt(780*factor));};
    add(info); info.setButtonText("MENU"); info.onClick=[this]{
        juce::PopupMenu menu;menu.addItem(1,"Save reference...");menu.addItem(2,"Load reference...");menu.addSeparator();menu.addItem(3,"About / IR credits");
        const juce::Component::SafePointer<ChimeraEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&info),[safe](int result){if(!safe)return;if(result==1)safe->referenceFile(true);else if(result==2)safe->referenceFile(false);else if(result==3)safe->showInfo();});
    };
    for(auto* button:{&compareA,&compareB,&copyAB,&rigsTab,&preTab,&postTab}) add(*button);
    compareA.onClick=[this]{processor.selectComparison(0);timerCallback();};compareB.onClick=[this]{processor.selectComparison(1);timerCallback();};copyAB.onClick=[this]{processor.copyComparison();};
    copyAB.setTooltip("Copy the active sound to the other slot. A/B stores all parameters and IR audio; both slots are saved with the project/reference file.");
    rigsTab.onClick=[this]{page=0;updateModeUI();};preTab.onClick=[this]{page=1;updateModeUI();};postTab.onClick=[this]{page=2;updateModeUI();};
    const std::array<const char*,5> effectHeaders{"TIGHT DRIVE","01 / STEREO DELAY","02 / ROOM REVERB","NOISE GATE","TRANSPOSE"};
    const std::array<const char*,5> effectScopes{"03 / BEFORE AMP SPLIT","GLOBAL / AFTER MERGE","GLOBAL / AFTER DELAY","01 / INPUT DYNAMICS","02 / POLYPHONIC PITCH"};
    const std::array<const char*,5> effectButtons{"preon","delayon","reverbon","gateon","transposeon"};
    const std::array<std::array<const char*,3>,5> effectIds{{{{"predrive","pretone","prelevel"}},{{"delaytime","delayfeedback","delaymix"}},{{"reverbsize","reverbdamping","reverbmix"}},{{"gatethreshold","gaterelease","gatehold"}},{{"transpose",nullptr,nullptr}}}};
    const std::array<std::array<const char*,3>,5> effectLabels{{{{"DRIVE","TONE","LEVEL"}},{{"TIME","FEEDBACK","MIX"}},{{"SIZE","DAMPING","MIX"}},{{"THRESHOLD","RELEASE","HOLD"}},{{"SEMITONES","",""}}}};
    const std::array<const char*,5> descriptions{
        "A focused boost before the amplifiers.\nMatrix LOW DI stays clean.",
        "One stereo echo after all active rigs.\nTime in milliseconds; feedback up to 85%.",
        "A shared space for the complete sound.\nSize, high-frequency damping and mix.",
        "Linked stereo detector with hold.\nShared by the DI and amplifier paths.",
        "Shift the complete input by +/-12 st.\nShared by the DI and amplifier paths."};
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];style(effect.header,18);style(effect.scope,11);style(effect.description,13);
        effect.header.setText(effectHeaders[i],juce::dontSendNotification);effect.scope.setText(effectScopes[i],juce::dontSendNotification);
        effect.description.setText(descriptions[i],juce::dontSendNotification);
        add(effect.header);add(effect.scope);add(effect.description);add(effect.enabled);effect.enabled.setClickingTogglesState(true);
        effect.enabled.setComponentID(effectButtons[i]);effect.enabled.setTooltip("Enable or bypass this module");
        effect.button=std::make_unique<BA>(p.parameters(),effectButtons[i],effect.enabled);
        for(size_t k=0;k<3;++k) {
            if(!effectIds[i][k]) continue;
            style(effect.labels[k],11.5f,juce::Justification::centred);effect.labels[k].setText(effectLabels[i][k],juce::dontSendNotification);
            const juce::String suffix=i==0 && k==1 ? " Hz" : (i==0 && k==2)||(i==3 && k==0) ? " dB" : (i==1 && k==0)||(i==3 && k>0) ? " ms" : i==4 ? " st" : "";
            setupSlider(effect.controls[k],effectLabels[i][k],suffix);
            effect.controls[k].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);effect.controls[k].setTextBoxStyle(juce::Slider::TextBoxBelow,false,94,24);
            if(i==4) effect.controls[k].setNumDecimalPlacesToDisplay(0);
            add(effect.labels[k]);add(effect.controls[k]);effect.attachments[k]=std::make_unique<SA>(p.parameters(),effectIds[i][k],effect.controls[k]);
        }
    }
    setupSlider(lowComp,"COMP");add(lowComp);lowCompAttachment=std::make_unique<SA>(p.parameters(),"lowcomp",lowComp);
    lowComp.setTooltip("One-knob VCA-style RMS compression: threshold and ratio move together. 0 = unity. Use LEVEL for makeup gain.");
    style(lowCompLabel,11.5f);lowCompLabel.setText("COMP",juce::dontSendNotification);add(lowCompLabel);
    style(diVoice,16);diVoice.setText("CLEAN DI / VCA",juce::dontSendNotification);add(diVoice);
    style(diNote,12);diNote.setText("Direct low end. No amp or cabinet.\nPre Drive is bypassed; Gate and Pitch apply.",juce::dontSendNotification);add(diNote);
    style(routingHelp); add(routingHelp); style(x1Label,11.f); style(x2Label,11.f);
    x1Label.setText("LOW / MID",juce::dontSendNotification); x2Label.setText("MID / HIGH",juce::dontSendNotification);
    add(x1Label); add(x2Label); setupSlider(x1,"LOW / MID"," Hz"); setupSlider(x2,"MID / HIGH"," Hz"); add(x1); add(x2);
    auto& state=p.parameters(); ma=std::make_unique<CA>(state,"mode",mode); qa=std::make_unique<CA>(state,"oversampling",quality);
    a1=std::make_unique<SA>(state,"x1",x1); a2=std::make_unique<SA>(state,"x2",x2);
    const std::array<const char*,7> globalIds{"input","gatethreshold","gaterelease","gatehold","transpose","tunerref","output"};
    const std::array<const char*,7> globalNames{"Input gain","Gate threshold","Gate release","Gate hold","Transpose semitones","Tuner reference","Output gain"};
    const std::array<const char*,7> suffix{" dB"," dB"," ms"," ms"," st"," Hz"," dB"};
    for(size_t i=0;i<7;++i)
    {
        setupSlider(globalSliders[i],globalNames[i],suffix[i]); add(globalSliders[i]);
        globalAttachments[i]=std::make_unique<SA>(state,globalIds[i],globalSliders[i]);
    }
    globalSliders[4].setNumDecimalPlacesToDisplay(0); globalSliders[5].setNumDecimalPlacesToDisplay(0);
    globalSliders[2].setNumDecimalPlacesToDisplay(0); globalSliders[3].setNumDecimalPlacesToDisplay(0);
    const std::array<juce::TextButton*,4> buttons{&gateOn,&pitchOn,&tunerOn,&tunerMute};
    const std::array<const char*,4> ids{"gateon","transposeon","tuneron","tunermute"};
    for(size_t i=0;i<4;++i) { buttons[i]->setClickingTogglesState(true); add(*buttons[i]); globalButtons[i]=std::make_unique<BA>(state,ids[i],*buttons[i]); }
    tunerOn.setTooltip("Tune the original input before the gate and transposer. Play one note at a time. Range: 25-1400 Hz.");
    tunerMute.setTooltip("Silence the output while the tuner is enabled.");
    pitchOn.setTooltip("Polyphonic transpose, -12 to +12 semitones. The added latency is shown below and reported to the host.");
    style(gateStatus,11.f); style(pitchStatus,11.f); add(gateStatus); add(pitchStatus);
    const std::array<juce::String,8> names{"DRIVE","LEVEL","BASS","LOW MID","HIGH MID","TREBLE","PRESENCE","RESONANCE"};
    const std::array<const char*,8> parameterIds{"drive","level","bass","lowmid","highmid","treble","presence","resonance"};
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i]; const auto n=juce::String(i+1);
        style(lane.header,15.f); style(lane.range,12.f); style(lane.tonePivot,11.f); style(lane.cabStatus,11.f);
        lane.range.setColour(juce::Label::textColourId,muted); lane.cabStatus.setColour(juce::Label::textColourId,muted);
        add(lane.header); add(lane.range); add(lane.tonePivot); add(lane.cabStatus);
        lane.amp.setName("Amp "+n); lane.amp.addItemList({"Glass","Brit Edge","Tight 515","Wide Rect","Liquid Lead","Iron Tube","Solid Punch","Modern Bass"},1); add(lane.amp);
        lane.aa=std::make_unique<CA>(state,"amp"+n,lane.amp);
        auto controls=lane.controls();
        for(size_t k=0;k<8;++k)
        {
            setupSlider(*controls[k],names[k],k==0 ? "" : " dB"); add(*controls[k]);
            style(lane.knobLabels[k],11.5f); lane.knobLabels[k].setText(names[k],juce::dontSendNotification); add(lane.knobLabels[k]);
            lane.sa[k]=std::make_unique<SA>(state,juce::String(parameterIds[k])+n,*controls[k]);
        }
        setupSlider(lane.bandTone,"BAND TONE"," dB"); add(lane.bandTone); style(lane.toneLabel,11.5f);
        lane.toneLabel.setText("BAND TONE",juce::dontSendNotification); add(lane.toneLabel);
        lane.bandTone.setTooltip("Pre-amp tilt within this band. Negative: darker. Positive: brighter. Pivot follows the crossover.");
        lane.toneAttachment=std::make_unique<SA>(state,"bandtone"+n,lane.bandTone);
        lane.cabType.setName("Cabinet "+n); lane.cabType.addItemList({"Filters only","V30 / SM57","Jensen / SM57","User IR"},1); add(lane.cabType);
        lane.ca=std::make_unique<CA>(state,"cabtype"+n,lane.cabType);
        lane.cabType.setTooltip("Factory speaker IRs by jesterdyne (CC BY 4.0). User IRs are stored in your project. See MENU for credits.");
        setupSlider(lane.cabLow,"Cab low cut"," Hz"); setupSlider(lane.cabHigh,"Cab high cut"," Hz");
        lane.cabLow.setNumDecimalPlacesToDisplay(0); lane.cabHigh.setNumDecimalPlacesToDisplay(0);
        add(lane.cabLow); add(lane.cabHigh); style(lane.lowLabel,11.f); style(lane.highLabel,11.f);
        lane.lowLabel.setText("LOW CUT",juce::dontSendNotification); lane.highLabel.setText("HIGH CUT",juce::dontSendNotification); add(lane.lowLabel); add(lane.highLabel);
        lane.lowAttachment=std::make_unique<SA>(state,"cablow"+n,lane.cabLow); lane.highAttachment=std::make_unique<SA>(state,"cabhigh"+n,lane.cabHigh);
        const std::array<juce::TextButton*,5> laneButtons{&lane.mute,&lane.solo,&lane.polarity,&lane.cabOn,&lane.ampOn};
        const std::array<const char*,5> laneIds{"mute","solo","polarity","cab","ampon"};
        for(size_t k=0;k<5;++k) { laneButtons[k]->setClickingTogglesState(true); add(*laneButtons[k]); lane.buttons[k]=std::make_unique<BA>(state,juce::String(laneIds[k])+n,*laneButtons[k]); }
        lane.mute.setTooltip("Mute this active rig"); lane.solo.setTooltip("Solo this active rig"); lane.polarity.setTooltip("Invert this rig's polarity");
        lane.cabOn.setTooltip("Enable or bypass this rig's IR and cabinet cuts");
        add(lane.load); lane.load.onClick=[this,i]{loadIR(i);}; lane.load.setTooltip("Load WAV/AIFF IR, mono/stereo, up to 1 second and 4 MB. You can also drop a file onto this rig.");
    }
    mode.onChange=[this]{updateModeUI();}; x1.onValueChange=[this]{updateBandLabels();}; x2.onValueChange=[this]{updateBandLabels();};
    setResizable(true,true); setResizeLimits(885,585,1770,1170); getConstrainer()->setFixedAspectRatio(1180.0/780.0);
    setSize(1180,780); updateModeUI(); startTimerHz(25);
}
ChimeraEditor::~ChimeraEditor() { stopTimer(); chooser.reset(); setLookAndFeel(nullptr); }
void ChimeraEditor::setupSlider(juce::Slider& slider,const juce::String& name,const juce::String& suffix)
{
    slider.setName(name); slider.setTooltip(name); slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,66,24); slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(name=="DRIVE" ? 2 : 1);
}
void ChimeraEditor::loadIR(int lane)
{
    chooser=std::make_unique<juce::FileChooser>("Load cabinet IR",juce::File{},"*.wav;*.aif;*.aiff");
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,lane](const juce::FileChooser& file)
    {
        if(!safe || !file.getResult().existsAsFile()) return;
        safe->processor.loadIR(lane,file.getResult()); safe->timerCallback();
    });
}
bool ChimeraEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size()==1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff");
}
void ChimeraEditor::filesDropped(const juce::StringArray& files,int x,int y)
{
    if(!isInterestedInFileDrag(files) || page!=0) return;
    const float factor=getWidth()/1180.f; x=juce::roundToInt(x/factor); y=juce::roundToInt(y/factor);
    const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    if(y<330 || y>742) return;
    const int width=(1140-14*(count-1))/count;
    for(int i=0;i<count;++i) if(!(lastMode==2 && i==0) && x>=20+i*(width+14) && x<20+i*(width+14)+width) { processor.loadIR(i,juce::File(files[0])); timerCallback(); break; }
}
void ChimeraEditor::showInfo()
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Chimera Amp Matrix",
        "SpectralForge | 1.0 development build\n\nAmp voices are algorithmic interpretations, not verified hardware replicas.\n\nFactory IRs: jesterdyne, CC BY 4.0\nEngl Celestion V30 SM57 center-01.wav\nhttps://freesound.org/s/116735/\nJensen Cab SM57 center.wav\nhttps://freesound.org/s/116743/\nhttps://creativecommons.org/licenses/by/4.0/\nFiles unchanged; normalised and resampled during playback.\n\nPitch: Chimera STFT / JUCE FFT\n\nComplete notices are supplied with the download.","OK",this);
}
void ChimeraEditor::timerCallback()
{
    const int current=(int)processor.parameters().getRawParameterValue("mode")->load();
    if(current!=lastMode) updateModeUI();
    if(current==2) updateBandLabels();
    compareA.setToggleState(processor.comparisonSlot()==0,juce::dontSendNotification);compareB.setToggleState(processor.comparisonSlot()==1,juce::dontSendNotification);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i];
        const bool active=processor.parameters().getRawParameterValue("cab"+juce::String(i+1))->load()>.5f;
        lane.cabStatus.setText(current==2 && i==0 ? "GAIN REDUCTION  "+juce::String(processor.lowCompMeter(),1)+" dB" : (active ? "" : "BYPASSED | ")+processor.cabStatus(i),juce::dontSendNotification);
        lane.cabStatus.setTooltip(lane.cabStatus.getText());
        lane.cabLow.setEnabled(active); lane.cabHigh.setEnabled(active);
    }
    for(auto& effect:effects) effect.enabled.setButtonText(effect.enabled.getToggleState() ? "ACTIVE" : "BYPASS");
    gateStatus.setText(gateOn.getToggleState() ? "REDUCTION  "+juce::String(-juce::Decibels::gainToDecibels(processor.gateMeter(),-90.f),1)+" dB" : "BYPASSED",juce::dontSendNotification);
    const double sr=processor.getSampleRate()>0 ? processor.getSampleRate() : 48000;
    pitchStatus.setText(pitchOn.getToggleState() ? "+ "+juce::String(1000.0*processor.pitchLatency()/sr,1)+" ms latency" : "BYPASSED | zero added latency",juce::dontSendNotification);
    repaint();
}
void ChimeraEditor::updateBandLabels()
{
    if(lastMode!=2) return;
    const float low=processor.parameters().getRawParameterValue("x1")->load(), high=processor.parameters().getRawParameterValue("x2")->load();
    lanes[0].range.setText("INPUT: below "+frequency(low),juce::dontSendNotification);
    lanes[1].range.setText("INPUT: "+frequency(low)+" - "+frequency(high),juce::dontSendNotification);
    lanes[2].range.setText("INPUT: above "+frequency(high),juce::dontSendNotification);
    const double sr=processor.getSampleRate()>0 ? processor.getSampleRate() : 48000;
    for(int i=0;i<3;++i) lanes[i].tonePivot.setText("DARK < TONE > BRIGHT  |  PIVOT "+frequency(spectralforge::matrixTonePivot(i,low,high,sr)),juce::dontSendNotification);
}
void ChimeraEditor::updateModeUI()
{
    lastMode=(int)processor.parameters().getRawParameterValue("mode")->load();
    const bool matrix=lastMode==2; const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    x1.setVisible(matrix && page==0); x2.setVisible(matrix && page==0); x1Label.setVisible(matrix && page==0); x2Label.setVisible(matrix && page==0);
    rigsTab.setToggleState(page==0,juce::dontSendNotification);preTab.setToggleState(page==1,juce::dontSendNotification);postTab.setToggleState(page==2,juce::dontSendNotification);
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];const bool show=(i==1 || i==2) ? page==2 : page==1;
        effect.header.setVisible(show);effect.scope.setVisible(show);effect.description.setVisible(show);effect.enabled.setVisible(show);
        for(int k=0;k<3;++k){const bool visible=show && effect.attachments[(size_t)k]!=nullptr;effect.controls[k].setVisible(visible);effect.labels[k].setVisible(visible);}
    }
    for(juce::Component* c:std::initializer_list<juce::Component*>{&lowComp,&lowCompLabel,&diVoice,&diNote}) c->setVisible(matrix && page==0);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i]; const bool show=i<count && page==0;
        for(juce::Component* c : std::initializer_list<juce::Component*>{&lane.header,&lane.range,&lane.amp,&lane.ampOn,&lane.mute,&lane.solo,&lane.polarity,&lane.cabOn,&lane.load,&lane.cabType,&lane.cabLow,&lane.cabHigh,&lane.lowLabel,&lane.highLabel,&lane.cabStatus}) c->setVisible(show);
        auto controls=lane.controls();
        for(size_t k=0;k<8;++k) { controls[k]->setVisible(show && (!matrix || k<2)); lane.knobLabels[k].setVisible(show && (!matrix || k<2)); }
        if(matrix && i==0) {
            for(juce::Component* c:std::initializer_list<juce::Component*>{&lane.drive,&lane.knobLabels[0],&lane.amp,&lane.ampOn,&lane.cabOn,&lane.cabType,&lane.load,&lane.cabLow,&lane.cabHigh,&lane.lowLabel,&lane.highLabel}) c->setVisible(false);
        }
        lane.bandTone.setVisible(show && matrix); lane.toneLabel.setVisible(show && matrix); lane.tonePivot.setVisible(show && matrix);
        lane.header.setText(matrix ? (i==0 ? "01 / LOW DI" : i==1 ? "02 / MID" : "03 / HIGH") : "RIG "+juce::String(i+1),juce::dontSendNotification);
        if(!matrix) lane.range.setText("Full-range input",juce::dontSendNotification);
    }
    routingHelp.setText(page==1 ? "PEDALBOARD / SHARED INPUT" : page==2 ? "RACK / AFTER RIG MERGE" : matrix ? "LR4 / 24 dB per octave" : lastMode==0 ? "ONE FULL-RANGE RIG" : "TWO PARALLEL RIGS / 50:50",juce::dontSendNotification);
    updateBandLabels(); layoutControls(); repaint();
}
void ChimeraEditor::paint(juce::Graphics& g)
{
    g.fillAll(background); g.addTransform(juce::AffineTransform::scale(getWidth()/1180.f));
    g.setColour(accent); g.fillRect(20,20,3,36);
    text(g,"SPECTRALFORGE  /  AMP MATRIX",194,26,284,28,12.f,muted);
    text(g,"OVERSAMPLING",816,8,110,22,10.f); text(g,"SIZE",986,8,64,22,10.f);
    g.setColour(line); g.drawHorizontalLine(67,20,1160);
    const std::array<juce::Rectangle<float>,5> panels{{{20,80,138,168},{170,80,254,168},{436,80,176,168},{624,80,326,168},{962,80,198,168}}};
    for(auto box:panels) { g.setColour(panel); g.fillRoundedRectangle(box,5); g.setColour(line); g.drawRoundedRectangle(box,5,1); }
    text(g,"INPUT",32,91,104,24,12.f,ink); text(g,"DI TRIM",32,119,106,21,10.f);
    text(g,"THRESH",182,126,63,26,10.f); text(g,"RELEASE",182,160,64,26,10.f); text(g,"HOLD",308,160,54,26,10.f);
    text(g,"SEMITONES",450,129,148,22,10.f);
    text(g,"OUTPUT",976,91,152,24,12.f,ink); text(g,"MASTER TRIM",976,119,152,21,10.f);
    meter(g,processor.inputMeter(),141,126,6,93); meter(g,processor.outputMeter(),1142,126,6,93);
    text(g,juce::String(juce::Decibels::gainToDecibels(processor.inputMeter(),-90.f),1)+" dBFS",32,210,106,22,11.f,processor.inputMeter()>=1 ? juce::Colours::salmon : muted);
    text(g,juce::String(juce::Decibels::gainToDecibels(processor.outputMeter(),-90.f),1)+" dBFS",976,210,152,22,11.f,processor.outputMeter()>=1 ? juce::Colours::salmon : muted);
    text(g,"A4",827,94,24,25,10.f);
    const bool tuning=tunerOn.getToggleState();
    const float hz=processor.tuningFrequency();
    juce::String note="--",detail=tuning ? "PLAY ONE NOTE" : "TUNER OFF";
    float cents=0;
    if(tuning && hz>0 && processor.tuningConfidence()>.85f)
    {
        const float reference=processor.parameters().getRawParameterValue("tunerref")->load();
        const double midi=69+12*std::log2(hz/reference); const int nearest=juce::roundToInt(midi);
        const std::array<const char*,12> names{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        note=juce::String(names[(size_t)((nearest%12+12)%12)])+juce::String(nearest/12-1);
        cents=float((midi-nearest)*100); detail=juce::String(hz,1)+" Hz  |  "+(cents>0 ? "+" : "")+juce::String(cents,1)+" ct";
    }
    text(g,note,638,127,90,58,38.f,tuning && hz>0 && std::abs(cents)<3 ? accent : ink);
    text(g,detail,734,140,198,30,12.f,muted);
    text(g,tuning && tunerMute.getToggleState() ? "OUTPUT MUTED" : "ORIGINAL INPUT",734,172,198,20,10.f,accent);
    g.setColour(line); g.drawLine(642,216,932,216,2.f);
    for(int i=0;i<=4;++i) { const float px=642+i*72.5f; g.drawLine(px,210,px,222,1.f); }
    if(tuning && hz>0) { const float px=787+juce::jlimit(-50.f,50.f,cents)*2.9f; g.setColour(accent); g.fillRoundedRectangle(px-2,206,4,20,1); }
    text(g,"-50",639,225,40,16,9.f); text(g,"0",778,225,24,16,9.f); text(g,"+50",906,225,30,16,9.f);
    text(g,"ROUTING",20,267,82,28,11.f,ink);
    if(page==2) {
        for(int i=0;i<2;++i) {
            const float y=330.f+i*210.f;
            g.setColour(panel);g.fillRoundedRectangle(20,y,1140,202,4);g.setColour(line);g.drawRoundedRectangle(20,y,1140,202,4,1);
            for(float x:{34.f,1146.f}) for(float sy:{y+20,y+182}) {g.setColour(background);g.fillEllipse(x-5,sy-5,10,10);g.setColour(muted);g.drawLine(x-2,sy+2,x+2,sy-2,1);}
            g.setColour(line);g.drawVerticalLine(335,y+18,y+184);g.setColour(accent.withAlpha(i==0 ? 1.f : .6f));g.fillRect(53.f,y+18,3.f,34.f);
        }
    } else {
        const int count=page==1 ? 3 : lastMode==0 ? 1 : lastMode==1 ? 2 : 3; const int width=(1140-14*(count-1))/count;
        for(int i=0;i<count;++i) {
            const float x=float(20+i*(width+14));
            g.setColour(panel); g.fillRoundedRectangle(x,330,float(width),412,page==1 ? 12.f : 5.f);
            g.setColour(line); g.drawRoundedRectangle(x,330,float(width),412,page==1 ? 12.f : 5.f,1);
            g.setColour(accent.withAlpha(i==1 ? .75f : i==2 ? .5f : 1.f));g.fillRect(x+12,330.f,float(width-24),2.f);
            if(page==1) {
                g.setColour(background);g.fillRoundedRectangle(x+16,627,float(width-32),97,6);
                for(float screw:{x+27,x+width-27}) {g.setColour(line);g.fillEllipse(screw-3,706,6,6);}
                g.setColour(effects[(size_t)(i==0 ? 3 : i==1 ? 4 : 0)].enabled.getToggleState() ? accent : line);g.fillEllipse(x+width*.5f-3,638,6,6);
            } else {g.setColour(line);g.drawHorizontalLine(616,x+12,x+width-12);}
        }
    }
    text(g,"INPUT > GATE / PITCH > DRIVE* > RIGS / CAB > MERGE > DELAY / REVERB > OUTPUT   |   *LOW DI BYPASS",20,754,875,22,11.f);
    text(g,"1.0 / DEVELOPMENT",957,754,203,22,10.f,muted,juce::Justification::centredRight);
}
void ChimeraEditor::resized()
{
    canvas.setBounds(0,0,1180,780); canvas.setTransform(juce::AffineTransform::scale(getWidth()/1180.f));
    layoutControls();
}
void ChimeraEditor::layoutControls()
{
    compareA.setBounds(490,30,30,28);compareB.setBounds(524,30,30,28);copyAB.setBounds(558,30,52,28);rigsTab.setBounds(634,30,52,28);preTab.setBounds(692,30,52,28);postTab.setBounds(750,30,52,28);
    title.setBounds(30,16,166,40); quality.setBounds(816,30,130,28); scale.setBounds(986,30,80,28); info.setBounds(1090,30,70,28);
    mode.setBounds(106,268,140,28); routingHelp.setBounds(258,268,277,28);
    x1Label.setBounds(565,258,224,20); x1.setBounds(565,280,252,28);
    x2Label.setBounds(858,258,250,20); x2.setBounds(858,280,290,28);
    gateOn.setBounds(182,93,78,24); pitchOn.setBounds(450,93,148,24); tunerOn.setBounds(638,93,72,24); tunerMute.setBounds(718,93,99,24);
    globalSliders[0].setBounds(30,148,106,32);
    globalSliders[1].setBounds(244,126,167,28);
    globalSliders[2].setBounds(180,184,111,27); globalSliders[3].setBounds(306,184,106,27);
    globalSliders[4].setBounds(448,159,152,36); globalSliders[5].setBounds(850,94,88,24); globalSliders[6].setBounds(974,148,159,32);
    gateStatus.setBounds(180,217,232,22); pitchStatus.setBounds(447,212,158,29);
    for(int position=0;position<3;++position) {
        const int i=position==0 ? 3 : position==1 ? 4 : 0;
        auto& effect=effects[(size_t)i];const int x=20+position*384;
        effect.header.setBounds(x+20,349,330,29);effect.scope.setBounds(x+20,379,330,24);
        for(int k=0;k<3;++k) {const int left=i==4 ? x+121 : x+18+k*110;effect.labels[(size_t)k].setBounds(left,420,104,24);effect.controls[(size_t)k].setBounds(left,446,104,108);}
        effect.description.setBounds(x+20,566,330,52);effect.enabled.setBounds(x+129,659,112,33);
    }
    for(int i=1;i<=2;++i) {
        auto& effect=effects[(size_t)i];const int y=330+(i-1)*210;
        effect.header.setBounds(65,y+18,258,29);effect.scope.setBounds(65,y+51,258,22);effect.description.setBounds(65,y+82,258,56);effect.enabled.setBounds(65,y+152,112,30);
        for(int k=0;k<3;++k) {const int x=370+k*251;effect.labels[(size_t)k].setBounds(x,y+23,210,24);effect.controls[(size_t)k].setBounds(x,y+53,210,123);}
    }
    diVoice.setBounds(32,398,344,32);diNote.setBounds(32,634,342,63);
    const bool matrix=lastMode==2; const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    const int width=(1140-14*(count-1))/count;
    for(int i=0;i<count;++i)
    {
        auto& lane=lanes[i]; const int x=20+i*(width+14);
        lane.header.setBounds(x+12,342,width-165,24); lane.range.setBounds(x+12,371,width-24,20);
        lane.mute.setBounds(x+width-147,344,48,23); lane.solo.setBounds(x+width-95,344,44,23); lane.polarity.setBounds(x+width-47,344,35,23);
        lane.ampOn.setBounds(x+12,398,48,32);lane.amp.setBounds(x+68,398,width-80,32);
        if(matrix)
        {
            const std::array<juce::Slider*,3> controls{i==0 ? &lowComp : &lane.drive,&lane.level,&lane.bandTone};
            const std::array<juce::Label*,3> labels{i==0 ? &lowCompLabel : &lane.knobLabels[0],&lane.knobLabels[1],&lane.toneLabel};
            for(int k=0;k<3;++k) { labels[k]->setBounds(x+12,446+k*49,82,29); controls[k]->setBounds(x+97,446+k*49,width-109,29); }
            lane.tonePivot.setBounds(x+12,585,width-24,22);
        }
        else
        {
            const int columns=count==1 ? 4 : 2, cell=(width-24)/columns;
            auto controls=lane.controls();
            for(int k=0;k<8;++k)
            {
                const int left=x+12+(k%columns)*cell, top=441+(k/columns)*(count==1 ? 80 : 42);
                lane.knobLabels[k].setBounds(left,top,cell-12,18);
                controls[k]->setBounds(left,top+18,cell-14,24);
            }
        }
        lane.cabOn.setBounds(x+12,628,48,26); lane.cabType.setBounds(x+68,628,width-148,26); lane.load.setBounds(x+width-72,628,60,26);
        const int cell=(width-24)/2;
        lane.lowLabel.setBounds(x+12,662,cell-8,18); lane.highLabel.setBounds(x+12+cell,662,cell-8,18);
        lane.cabLow.setBounds(x+12,680,cell-12,26); lane.cabHigh.setBounds(x+12+cell,680,cell-12,26);
        lane.cabStatus.setBounds(x+12,711,width-24,22);
    }
}

void ChimeraEditor::referenceFile(bool save)
{
    chooser=std::make_unique<juce::FileChooser>(save ? "Save reference (A/B + embedded IRs)" : "Load reference",juce::File{},"*.chimera");
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    const int flags=juce::FileBrowserComponent::canSelectFiles|(save ? juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode);
    chooser->launchAsync(flags,[safe,save](const juce::FileChooser& file){
        if(!safe || file.getResult()==juce::File{}) return;
        juce::MemoryBlock data;bool ok=false;
        if(save) {safe->processor.getStateInformation(data);ok=file.getResult().withFileExtension(".chimera").replaceWithData(data.getData(),data.getSize());}
        else if(file.getResult().getSize()<=64*1024*1024 && file.getResult().loadFileAsData(data)) {
            auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),(int)data.getSize());
            if(xml && xml->hasTagName("PARAMS")) {safe->processor.setStateInformation(data.getData(),(int)data.getSize());ok=true;}
        }
        if(!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Reference file",save ? "Could not save the reference file." : "This is not a valid Chimera reference file.","OK",safe);
        safe->timerCallback();
    });
}

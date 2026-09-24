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
    add(info); info.setButtonText("SETTINGS"); info.onClick=[this]{
        juce::PopupMenu menu;menu.addItem(1,"Save reference...");menu.addItem(2,"Load reference...");menu.addSeparator();menu.addItem(3,"About / IR credits");menu.addSeparator();menu.addItem(4,"Reset tuner A4 to 440 Hz");menu.addItem(5,"Clear MIDI assignments");
        const juce::Component::SafePointer<ChimeraEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&info),[safe](int result){if(!safe)return;if(result==1)safe->referenceFile(true);else if(result==2)safe->referenceFile(false);else if(result==3)safe->showInfo();else if(result==4){auto* p=safe->processor.parameters().getParameter("tunerref");p->setValueNotifyingHost(p->convertTo0to1(440));}else if(result==5)safe->processor.clearMidi();});
    };
    for(auto* button:{&compareA,&compareB,&copyAB,&rigsTab,&preTab,&postTab}) add(*button);
    compareA.onClick=[this]{processor.selectComparison(0);timerCallback();};compareB.onClick=[this]{processor.selectComparison(1);timerCallback();};copyAB.onClick=[this]{processor.copyComparison();};
    copyAB.setTooltip("Copy the active sound to the other slot. A/B stores all parameters and IR audio; both slots are saved with the project/reference file.");
    rigsTab.onClick=[this]{page=0;updateModeUI();};preTab.onClick=[this]{page=1;updateModeUI();};postTab.onClick=[this]{page=2;updateModeUI();};
    const std::array<const char*,11> effectHeaders{"OVERDRIVE","DELAY","REVERB","COMPRESSOR","ENVELOPE","FUZZ","BOOST","BUS COMP","PREAMP","EQ","CHORUS"};
    const std::array<const char*,11> effectScopes{"05 / TIGHT GAIN","05 / SPACE","06 / SPACE","01 / DYNAMICS","02 / FILTER","03 / TEXTURE","04 / SHAPING","01 / DYNAMICS","02 / COLOUR","03 / TONE","04 / MODULATION"};
    const std::array<const char*,11> effectButtons{"preon","delayon","reverbon","precompon","filteron","fuzzon","booston","buscompon","preampon","eqon","choruson"};
    const std::array<const char*,11> descriptions{"Focused saturation before the amp.\nMatrix LOW DI stays clean.","Stereo echo. SYNC follows quarter-note tempo.","Room ambience after delay.","Even out dynamics and sustain.\nShared by DI and amp paths.","Touch-controlled low-pass sweep.\nShared by DI and amp paths.","Dense, asymmetric saturation.\nMatrix LOW DI stays clean.","Clean boost with two shelves.\nMatrix LOW DI stays clean.","Linked RMS bus compressor. 6 dB soft knee. Entire merged signal.","4x-oversampled colour stage with independent output trim.","Low/high shelves and a parametric mid band, applied after merge.","Stereo chorus with rate, depth and wet mix."};
    const std::array<std::array<const char*,5>,11> effectIds{{{"predrive","pretone","prelevel",nullptr,nullptr},{"delaytime","delayfeedback","delaymix",nullptr,nullptr},{"reverbsize","reverbdamping","reverbmix",nullptr,nullptr},{"precomp","precompattack","precomplevel",nullptr,nullptr},{"filtersense","filterq","filtermix",nullptr,nullptr},{"fuzzdrive","fuzztone","fuzzlevel",nullptr,nullptr},{"boostgain","boostbass","boosttreble",nullptr,nullptr},{"busthreshold","busratio","busattack","busrelease","busmakeup"},{"preampdrive","preampcolour","preamplevel",nullptr,nullptr},{"eqlow","eqmidhz","eqmid","eqq","eqhigh"},{"chorusrate","chorusdepth","chorusmix",nullptr,nullptr}}};
    const std::array<std::array<const char*,5>,11> effectLabels{{{"DRIVE","TONE","LEVEL",nullptr,nullptr},{"TIME","FEEDBACK","MIX",nullptr,nullptr},{"SIZE","DAMPING","MIX",nullptr,nullptr},{"SUSTAIN","ATTACK","LEVEL",nullptr,nullptr},{"SENSE","Q","MIX",nullptr,nullptr},{"DRIVE","BODY","LEVEL",nullptr,nullptr},{"GAIN","BASS","TREBLE",nullptr,nullptr},{"THRESHOLD","RATIO","ATTACK","RELEASE","MAKEUP"},{"GAIN","COLOUR","TRIM",nullptr,nullptr},{"LOW 80 Hz","MID FREQ","MID GAIN","MID Q","HIGH 8 kHz"},{"RATE","DEPTH","MIX",nullptr,nullptr}}};
    const std::array<std::array<const char*,5>,11> effectSuffix{{{""," Hz"," dB",nullptr,nullptr},{" ms","","",nullptr,nullptr},{"","","",nullptr,nullptr},{""," ms"," dB",nullptr,nullptr},{"","","",nullptr,nullptr},{" dB",""," dB",nullptr,nullptr},{" dB"," dB"," dB",nullptr,nullptr},{" dB",":1"," ms"," ms"," dB"},{" dB",""," dB",nullptr,nullptr},{" dB"," Hz"," dB",""," dB"},{" Hz","","",nullptr,nullptr}}};
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];style(effect.header,15);style(effect.scope,10);style(effect.description,11.5f);
        effect.header.setText(effectHeaders[i],juce::dontSendNotification);effect.scope.setText(effectScopes[i],juce::dontSendNotification);effect.description.setText(descriptions[i],juce::dontSendNotification);
        effect.header.setTooltip(descriptions[i]);add(effect.header);add(effect.scope);add(effect.description);add(effect.enabled);effect.enabled.setClickingTogglesState(true);
        effect.enabled.setComponentID(effectButtons[i]);effect.button=std::make_unique<BA>(p.parameters(),effectButtons[i],effect.enabled);
        const bool pedal=i==0 || (i>=3 && i<=6);
        for(size_t k=0;k<5;++k) {
            if(!effectIds[i][k]) continue;
            style(effect.labels[k],pedal ? 10.5f : 10.f,juce::Justification::centred);effect.labels[k].setText(effectLabels[i][k],juce::dontSendNotification);
            setupSlider(effect.controls[k],effectLabels[i][k],effectSuffix[i][k]);
            if(pedal) {effect.controls[k].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);effect.controls[k].setTextBoxStyle(juce::Slider::TextBoxBelow,false,88,23);}
            add(effect.labels[k]);add(effect.controls[k]);effect.attachments[k]=std::make_unique<SA>(p.parameters(),effectIds[i][k],effect.controls[k]);
        }
    }
    inputMode.setName("Input mode");inputMode.addItemList({"STEREO","MONO L"},1);add(inputMode);inputModeAttachment=std::make_unique<CA>(p.parameters(),"inputmode",inputMode);
    inputMode.setTooltip("MONO L sends the left input to both channels. Stereo preserves separate channels.");
    presets.setName("Preset");presets.addItemList({"Clean Sustain","Tight Rhythm","Bass Matrix","Filter Lead","Fuzz Texture"},1);presets.setText("INIT / CUSTOM",juce::dontSendNotification);add(presets);
    presets.onChange=[this]{if(presets.getSelectedId()>0) {processor.loadFactoryPreset(presets.getSelectedId()-1);updateModeUI();}};
    for(auto* button:{&doublerOn,&midi,&tap,&hostTempo,&metronome,&presetPrevious,&presetNext,&presetSave,&presetLoad,&delaySync})add(*button);
    presetPrevious.onClick=[this]{presets.setSelectedId(presets.getSelectedId()<=1 ? 5 : presets.getSelectedId()-1);};presetNext.onClick=[this]{presets.setSelectedId(presets.getSelectedId()>=5 ? 1 : presets.getSelectedId()+1);};
    presetSave.onClick=[this]{referenceFile(true);};presetLoad.onClick=[this]{referenceFile(false);};tap.onClick=[this]{processor.tapTempo();};midi.onClick=[this]{midiMenu();};
    dualType.setName("Dual type");dualType.addItemList({"BLEND","CROSSOVER"},1);add(dualType);dualTypeAttachment=std::make_unique<CA>(p.parameters(),"dualtype",dualType);dualType.onChange=[this]{updateModeUI();};
    const std::array<juce::Slider*,4> sliders{&doublerTime,&tempo,&dualBlend,&dualFrequency};const std::array<const char*,4> sliderIds{"doublertime","tempo","dualblend","dualcross"};const std::array<const char*,4> sliderSuffix{" ms"," BPM",""," Hz"};
    for(size_t i=0;i<4;++i){setupSlider(*sliders[i],sliderIds[i],sliderSuffix[i]);add(*sliders[i]);utilitySliders[i]=std::make_unique<SA>(p.parameters(),sliderIds[i],*sliders[i]);}
    tempo.setNumDecimalPlacesToDisplay(1);dualBlend.textFromValueFunction=[](double v){return juce::String(juce::roundToInt((1-v)*100))+":"+juce::String(juce::roundToInt(v*100));};
    const std::array<juce::TextButton*,4> buttons{&doublerOn,&hostTempo,&metronome,&delaySync};const std::array<const char*,4> buttonIds{"doubleron","temposync","metronome","delaysync"};
    for(size_t i=0;i<4;++i){buttons[i]->setClickingTogglesState(true);utilityButtons[i]=std::make_unique<BA>(p.parameters(),buttonIds[i],*buttons[i]);}
    style(dualLabel,11);add(dualLabel);doublerOn.setTooltip("Stereo decorrelation; inactive on a mono bus.");metronome.setTooltip("4/4 practice click at the displayed tempo. Off by default.");delaySync.setTooltip("Quarter-note delay from TAP/manual tempo or HOST BPM.");
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
    const std::array<juce::TextButton*,4> globalToggles{&gateOn,&pitchOn,&tunerOn,&tunerMute};
    const std::array<const char*,4> ids{"gateon","transposeon","tuneron","tunermute"};
    for(size_t i=0;i<4;++i) { globalToggles[i]->setClickingTogglesState(true); add(*globalToggles[i]); globalButtons[i]=std::make_unique<BA>(state,ids[i],*globalToggles[i]); }
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
    if(current!=lastMode || lastDualCross!=(dualType.getSelectedId()==2) || lastTuner!=tunerOn.getToggleState()) updateModeUI();
    if(current==2 || (current==1 && lastDualCross)) updateBandLabels();
    compareA.setToggleState(processor.comparisonSlot()==0,juce::dontSendNotification);compareB.setToggleState(processor.comparisonSlot()==1,juce::dontSendNotification);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i];
        const bool active=processor.parameters().getRawParameterValue("cab"+juce::String(i+1))->load()>.5f;
        lane.cabStatus.setText(current==2 && i==0 ? "GAIN REDUCTION  "+juce::String(processor.lowCompMeter(),1)+" dB" : (active ? "" : "BYPASSED | ")+processor.cabStatus(i),juce::dontSendNotification);
        lane.cabStatus.setTooltip(lane.cabStatus.getText());
        lane.cabLow.setEnabled(active); lane.cabHigh.setEnabled(active);
    }
    for(auto& effect:effects) effect.enabled.setButtonText(effect.enabled.getToggleState() ? "ON" : "OFF");
    midi.setButtonText(processor.learningMidi() ? "LEARN" : "MIDI");tempo.setEnabled(!hostTempo.getToggleState());effects[1].controls[0].setEnabled(!delaySync.getToggleState());
    gateStatus.setText(gateOn.getToggleState() ? "REDUCTION  "+juce::String(-juce::Decibels::gainToDecibels(processor.gateMeter(),-90.f),1)+" dB" : "BYPASSED",juce::dontSendNotification);
    const double sr=processor.getSampleRate()>0 ? processor.getSampleRate() : 48000;
    pitchStatus.setText(pitchOn.getToggleState() ? "+ "+juce::String(1000.0*processor.pitchLatency()/sr,1)+" ms latency" : "BYPASSED | zero added latency",juce::dontSendNotification);
    repaint();
}
void ChimeraEditor::updateBandLabels()
{
    if(lastMode==1 && lastDualCross) {
        const float hz=processor.parameters().getRawParameterValue("dualcross")->load();lanes[0].range.setText("INPUT: below "+frequency(hz),juce::dontSendNotification);lanes[1].range.setText("INPUT: above "+frequency(hz),juce::dontSendNotification);
        for(int i=0;i<2;++i)lanes[i].tonePivot.setText("CROSSOVER-RELATIVE BAND TONE",juce::dontSendNotification);return;
    }
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
    lastDualCross=dualType.getSelectedId()==2;lastTuner=tunerOn.getToggleState();
    const bool matrix=lastMode==2,split=matrix || (lastMode==1 && lastDualCross); const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    x1.setVisible(matrix && page==0 && !lastTuner); x2.setVisible(matrix && page==0 && !lastTuner); x1Label.setVisible(matrix && page==0 && !lastTuner); x2Label.setVisible(matrix && page==0 && !lastTuner);
    rigsTab.setToggleState(page==0,juce::dontSendNotification);preTab.setToggleState(page==1,juce::dontSendNotification);postTab.setToggleState(page==2,juce::dontSendNotification);
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];const bool pedal=i==0 || (i>=3 && i<=6),show=pedal ? page==1 : page==2;
        effect.header.setVisible(show);effect.scope.setVisible(show);effect.description.setVisible(show && pedal);effect.enabled.setVisible(show);
        for(int k=0;k<5;++k){const bool visible=show && effect.attachments[(size_t)k]!=nullptr;effect.controls[k].setVisible(visible);effect.labels[k].setVisible(visible);}
    }
    for(juce::Component* c:std::initializer_list<juce::Component*>{&lowComp,&lowCompLabel,&diVoice,&diNote}) c->setVisible(matrix && page==0);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i]; const bool show=i<count && page==0;
        for(juce::Component* c : std::initializer_list<juce::Component*>{&lane.header,&lane.range,&lane.amp,&lane.ampOn,&lane.mute,&lane.solo,&lane.polarity,&lane.cabOn,&lane.load,&lane.cabType,&lane.cabLow,&lane.cabHigh,&lane.lowLabel,&lane.highLabel,&lane.cabStatus}) c->setVisible(show);
        auto controls=lane.controls();
        for(size_t k=0;k<8;++k) { controls[k]->setVisible(show && (!split || k<2)); lane.knobLabels[k].setVisible(show && (!split || k<2)); }
        if(matrix && i==0) {
            for(juce::Component* c:std::initializer_list<juce::Component*>{&lane.drive,&lane.knobLabels[0],&lane.amp,&lane.ampOn,&lane.cabOn,&lane.cabType,&lane.load,&lane.cabLow,&lane.cabHigh,&lane.lowLabel,&lane.highLabel}) c->setVisible(false);
        }
        lane.bandTone.setVisible(show && split); lane.toneLabel.setVisible(show && split); lane.tonePivot.setVisible(show && split);
        lane.header.setText(matrix ? (i==0 ? "01 / LOW DI" : i==1 ? "02 / MID" : "03 / HIGH") : "RIG "+juce::String(i+1),juce::dontSendNotification);
        if(!matrix) lane.range.setText("Full-range input",juce::dontSendNotification);
    }
    mode.setVisible(!lastTuner);routingHelp.setVisible(!lastTuner);dualType.setVisible(lastMode==1 && !lastTuner);dualLabel.setVisible(lastMode==1 && page==0 && !lastTuner);
    dualBlend.setVisible(lastMode==1 && !lastDualCross && page==0 && !lastTuner);dualFrequency.setVisible(lastMode==1 && lastDualCross && page==0 && !lastTuner);
    dualLabel.setText(lastDualCross ? "LOW / HIGH CROSSOVER" : "RIG 1 : RIG 2",juce::dontSendNotification);
    tunerMute.setVisible(lastTuner);globalSliders[5].setVisible(lastTuner);delaySync.setVisible(page==2);
    routingHelp.setText(page==1 ? "PEDALBOARD / SHARED INPUT" : page==2 ? "RACK / AFTER RIG MERGE" : matrix ? "LR4 / 24 dB per octave" : lastMode==0 ? "ONE FULL-RANGE RIG" : lastDualCross ? "LR4 / 24 dB per octave" : "FULL-RANGE BLEND",juce::dontSendNotification);
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
    text(g,"INPUT MODE",638,88,108,16,9.f);text(g,"PRESET",770,88,160,16,9.f);
    text(g,"Routing",20,267,80,28,12.f);
    if(tunerOn.getToggleState()) {
        g.setColour(panel);g.fillRoundedRectangle(20,258,1140,58,5);
        const float hz=processor.tuningFrequency();juce::String note="--",detail="PLAY ONE NOTE";float cents=0;
        if(hz>0 && processor.tuningConfidence()>.85f) {
            const float reference=processor.parameters().getRawParameterValue("tunerref")->load();const double midiNote=69+12*std::log2(hz/reference);const int nearest=juce::roundToInt(midiNote);
            const std::array<const char*,12> notes{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};note=juce::String(notes[(size_t)((nearest%12+12)%12)])+juce::String(nearest/12-1);cents=float((midiNote-nearest)*100);detail=juce::String(hz,1)+" Hz  |  "+juce::String(cents,1)+" ct";
        }
        text(g,note,34,262,115,45,32.f,std::abs(cents)<3 ? accent : ink);text(g,detail,158,271,245,28,13.f);
        g.setColour(line);g.drawLine(430,286,716,286,2);g.setColour(ink);g.drawVerticalLine(573,276,297);
        const float needle=573+juce::jlimit(-50.f,50.f,cents)*2.7f;g.setColour(accent);g.fillRect(needle-2,269.f,4.f,35.f);text(g,"A4",763,270,36,29,11.f);
    }
    if(page==2) {
        for(int i=0;i<6;++i) {
            const float y=330.f+i*69.f;
            g.setColour(panel);g.fillRoundedRectangle(20,y,1140,64,4);g.setColour(line);g.drawRoundedRectangle(20,y,1140,64,4,1);
            for(float x:{32.f,1148.f})for(float sy:{y+14,y+50}){g.setColour(background);g.fillEllipse(x-4,sy-4,8,8);g.setColour(muted);g.drawLine(x-2,sy+2,x+2,sy-2,1);}
            g.setColour(accent.withAlpha(1.f-i*.09f));g.fillRect(44.f,y+13,2.f,38.f);g.setColour(line);g.drawVerticalLine(290,y+8,y+56);
        }
    } else {
        const int count=page==1 ? 5 : lastMode==0 ? 1 : lastMode==1 ? 2 : 3;const int width=(1140-14*(count-1))/count;
        const std::array<juce::Colour,5> pedalColours{juce::Colour(0xff353e3b),juce::Colour(0xff353e45),juce::Colour(0xff463737),juce::Colour(0xff464135),juce::Colour(0xff3e3542)};
        for(int i=0;i<count;++i) {
            const float x=float(20+i*(width+14));g.setColour(page==1 ? pedalColours[(size_t)i] : panel);g.fillRoundedRectangle(x,330,float(width),412,page==1 ? 12.f : 5.f);
            g.setColour(line.brighter(page==1 ? .3f : 0.f));g.drawRoundedRectangle(x,330,float(width),412,page==1 ? 12.f : 5.f,1);
            g.setColour(accent.withAlpha(1.f-i*.1f));g.fillRect(x+12,330.f,float(width-24),2.f);
            if(page==1) {
                g.setColour(background.withAlpha(.55f));g.fillRoundedRectangle(x+12,640,float(width-24),85,6);
                for(float screw:{x+23,x+width-23}){g.setColour(muted);g.fillEllipse(screw-2,709,4,4);}
                g.setColour(effects[(size_t)pedalOrder[(size_t)i]].enabled.getToggleState() ? accent : line);g.fillEllipse(x+width*.5f-3,650,6,6);
            }else{g.setColour(line);g.drawHorizontalLine(616,x+12,x+width-12);}
        }
    }
    text(g,(hostTempo.getToggleState() ? "HOST " : "TEMPO ")+juce::String(processor.currentTempo(),1)+" BPM  |  "+juce::String(processor.getLatencySamples())+" SAMPLES  |  1.0 DEV",697,750,463,25,10.5f,muted,juce::Justification::centredRight);

}
void ChimeraEditor::resized()
{
    canvas.setBounds(0,0,1180,780); canvas.setTransform(juce::AffineTransform::scale(getWidth()/1180.f));
    layoutControls();
}
void ChimeraEditor::layoutControls()
{
    compareA.setBounds(490,30,30,28);compareB.setBounds(524,30,30,28);copyAB.setBounds(558,30,52,28);rigsTab.setBounds(634,30,52,28);preTab.setBounds(692,30,52,28);postTab.setBounds(750,30,52,28);
    title.setBounds(30,16,166,40); quality.setBounds(816,30,130,28); scale.setBounds(986,30,80,28); info.setBounds(504,750,86,25);
    mode.setBounds(106,268,140,28); routingHelp.setBounds(lastMode==1 ? 415 : 258,268,lastMode==1 ? 160 : 277,28);
    dualType.setBounds(260,268,144,28);dualLabel.setBounds(597,257,280,20);dualBlend.setBounds(597,280,330,28);dualFrequency.setBounds(597,280,330,28);
    x1Label.setBounds(565,258,224,20); x1.setBounds(565,280,252,28);
    x2Label.setBounds(858,258,250,20); x2.setBounds(858,280,290,28);
    gateOn.setBounds(182,93,78,24); pitchOn.setBounds(450,93,148,24); tunerOn.setBounds(20,750,64,25);tunerMute.setBounds(965,273,112,25);
    globalSliders[0].setBounds(30,148,106,32);
    globalSliders[1].setBounds(244,126,167,28);
    globalSliders[2].setBounds(180,184,111,27); globalSliders[3].setBounds(306,184,106,27);
    globalSliders[4].setBounds(448,159,152,36); globalSliders[5].setBounds(799,273,140,25); globalSliders[6].setBounds(974,148,159,32);
    gateStatus.setBounds(180,217,232,22); pitchStatus.setBounds(447,212,158,29);
    inputMode.setBounds(638,107,108,26);presets.setBounds(756,107,180,26);presetPrevious.setBounds(638,144,28,25);presetNext.setBounds(670,144,28,25);presetSave.setBounds(706,144,107,25);presetLoad.setBounds(821,144,115,25);
    doublerOn.setBounds(638,190,91,26);doublerTime.setBounds(741,190,195,26);
    midi.setBounds(91,750,61,25);tap.setBounds(160,750,44,25);tempo.setBounds(211,750,128,25);hostTempo.setBounds(347,750,58,25);metronome.setBounds(413,750,82,25);
    for(int position=0;position<5;++position) {
        auto& effect=effects[(size_t)pedalOrder[(size_t)position]];const int x=20+position*230;
        effect.header.setBounds(x+14,349,186,29);effect.scope.setBounds(x+14,378,186,24);
        for(int k=0;k<3;++k){const int left=x+(k==2 ? 62 : 10+k*101),top=k==2 ? 510 : 408;effect.labels[(size_t)k].setBounds(left,top,94,20);effect.controls[(size_t)k].setBounds(left,top+22,94,77);}
        effect.description.setBounds(x+14,610,187,28);effect.description.setTooltip(effect.description.getText());effect.enabled.setBounds(x+63,673,90,28);
    }
    for(int position=0;position<6;++position) {
        auto& effect=effects[(size_t)rackOrder[(size_t)position]];const int y=330+position*69;effect.header.setBounds(54,y+7,136,23);effect.scope.setBounds(54,y+33,135,18);effect.enabled.setBounds(198,y+20,48,26);
        int count=0;for(auto& attachment:effect.attachments)if(attachment)++count;const int cell=838/count;
        for(int k=0;k<count;++k){const int x=302+k*cell;effect.labels[(size_t)k].setBounds(x,y+5,cell-12,18);effect.controls[(size_t)k].setBounds(x,y+27,cell-12,29);}
    }
    delaySync.setBounds(250,330+4*69+20,37,26);
    diVoice.setBounds(32,398,344,32);diNote.setBounds(32,634,342,63);
    const bool matrix=lastMode==2,split=matrix || (lastMode==1 && lastDualCross); const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    const int width=(1140-14*(count-1))/count;
    for(int i=0;i<count;++i)
    {
        auto& lane=lanes[i]; const int x=20+i*(width+14);
        lane.header.setBounds(x+12,342,width-165,24); lane.range.setBounds(x+12,371,width-24,20);
        lane.mute.setBounds(x+width-147,344,48,23); lane.solo.setBounds(x+width-95,344,44,23); lane.polarity.setBounds(x+width-47,344,35,23);
        lane.ampOn.setBounds(x+12,398,48,32);lane.amp.setBounds(x+68,398,width-80,32);
        if(split)
        {
            const std::array<juce::Slider*,3> controls{matrix && i==0 ? &lowComp : &lane.drive,&lane.level,&lane.bandTone};
            const std::array<juce::Label*,3> labels{matrix && i==0 ? &lowCompLabel : &lane.knobLabels[0],&lane.knobLabels[1],&lane.toneLabel};
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
        if(ok) {safe->presets.setSelectedId(0,juce::dontSendNotification);safe->presets.setText(file.getResult().getFileNameWithoutExtension(),juce::dontSendNotification);}
        safe->timerCallback();
    });
}

void ChimeraEditor::midiMenu()
{
    juce::PopupMenu menu;menu.addSectionHeader("Learn next MIDI CC for...");
    const std::array<const char*,12> names{"Input","Output","Gate","Transpose","Pre Compressor","Envelope Filter","Fuzz","Boost","Overdrive","Delay","Reverb","Tuner"};
    for(int i=0;i<12;++i)menu.addItem(i+1,names[(size_t)i]);menu.addSeparator();menu.addItem(20,"Clear all assignments");
    const juce::Component::SafePointer<ChimeraEditor> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&midi),[safe](int result){
        if(!safe)return;const std::array<const char*,12> ids{"input","output","gateon","transpose","precompon","filteron","fuzzon","booston","preon","delayon","reverbon","tuneron"};
        if(result>=1 && result<=12)safe->processor.learnMidi(ids[(size_t)result-1]);else if(result==20)safe->processor.clearMidi();
    });
}

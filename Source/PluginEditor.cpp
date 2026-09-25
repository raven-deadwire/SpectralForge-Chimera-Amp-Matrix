#include "PluginEditor.h"
#include "HardwareArtwork.h"
#include "AmpCatalog.h"
#include "FactoryPresets.h"
#include "SupportPanel.h"

#ifndef CHIMERA_BUILD_REVISION
#define CHIMERA_BUILD_REVISION "local"
#endif

namespace {
const juce::Colour background(0xff0d1010),panel(0xff171b1b),line(0xff3a3a34),ink(0xffe1dbce),muted(0xffa4a79f),accent(0xffc4a678);
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
    if((bool)button.getProperties()["footswitch"]) {
        const float x=area.getCentreX(),y=area.getCentreY()-7;
        g.setColour(juce::Colours::black.withAlpha(.7f));g.fillEllipse(x-20,y-18,40,40);
        g.setGradientFill({juce::Colour(0xffcfcbc0),x-18,y-18,juce::Colour(0xff525655),x+16,y+18,false});g.fillEllipse(x-18,y-18,36,36);
        g.setColour(juce::Colour(0xff303534));g.drawEllipse(x-15,y-15,30,30,2);
        g.setGradientFill({juce::Colour(0xffe2dfd4),x-10,y-12,juce::Colour(0xff878a80),x+10,y+11,false});g.fillEllipse(x-11,y-11+(down?2:0),22,22);return;
    }
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
void ChimeraLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float value,float start,float end,juce::Slider& slider)
{
    const float radius=juce::jmin(float(width),float(height))*.41f;
    const juce::Point<float> centre(float(x)+width*.5f,float(y)+height*.5f);
    const float angle=start+value*(end-start),inner=radius*.78f;
    const int hardware=(int)slider.getProperties()["knobStyle"];
    const bool bright=(bool)slider.getProperties()["brightFace"];
    const auto markings=bright ? juce::Colour(0xff242a28) : ink;
    const bool metal=hardware==2 || hardware==3 || hardware==4;
    juce::Colour cap=hardware==1 ? juce::Colour(0xffddd1ad) : hardware==2 || hardware==4 ? juce::Colour(0xffbcc6c7) : hardware==3 ? juce::Colour(0xffc4a36c) : hardware==5 || hardware==8 ? juce::Colour(0xff53749a) : hardware==6 ? juce::Colour(0xffa54a3b) : hardware==7 ? juce::Colour(0xffe0bb49) : juce::Colour(0xff343b38);
    if(!slider.isEnabled()) cap=cap.interpolatedWith(juce::Colour(0xff60625b),.55f);
    for(int i=0;i<=10;++i){const float a=start+(end-start)*i/10;g.setColour(markings.withAlpha(.55f));g.drawLine({centre.getPointOnCircumference(radius,a),centre.getPointOnCircumference(radius+2,a)},1);}
    g.setColour(juce::Colours::black.withAlpha(.55f));g.fillEllipse(centre.x-inner-1,centre.y-inner+2,inner*2+2,inner*2+2);
    g.setGradientFill({metal ? cap.brighter(.35f) : juce::Colour(0xff474b47),centre.x-inner,centre.y-inner,metal ? cap.darker(.6f) : juce::Colour(0xff111715),centre.x+inner,centre.y+inner,false});g.fillEllipse(centre.x-inner,centre.y-inner,inner*2,inner*2);
    const int ribs=metal ? 32 : hardware==1 ? 16 : 24;
    for(int i=0;i<ribs;++i){const float a=juce::MathConstants<float>::twoPi*i/ribs;g.setColour(juce::Colours::black.withAlpha(.4f));g.drawLine({centre.getPointOnCircumference(inner*.88f,a),centre.getPointOnCircumference(inner,a)},1);}
    const float face=inner*(metal ? .84f : .74f);
    g.setGradientFill({cap.brighter(metal ? .4f : .12f),centre.x-face,centre.y-face,cap.darker(metal ? .25f : .32f),centre.x+face,centre.y+face,false});g.fillEllipse(centre.x-face,centre.y-face,face*2,face*2);
    g.setColour(cap.brighter(.4f).withAlpha(.5f));g.drawEllipse(centre.x-face,centre.y-face,face*2,face*2,.7f);
    const auto a=centre.getPointOnCircumference(face*.28f,angle),b=centre.getPointOnCircumference(face*.88f,angle);
    g.setColour(hardware==1 || metal || hardware==7 ? juce::Colour(0xff252a27) : juce::Colour(0xffeee8d5));g.drawLine({a,b},inner<14 ? 1.5f : 2.2f);
}
ChimeraEditor::ChimeraEditor(ChimeraProcessor& p) : AudioProcessorEditor(&p),processor(p)
{
    (void)spectralforge::art::RasterBank::get();
    setLookAndFeel(&look); canvas.setComponentID("surface"); addAndMakeVisible(canvas);
    auto add=[this](juce::Component& component){canvas.addAndMakeVisible(component);};
    title.setText("SpectralForge Chimera",juce::dontSendNotification);title.setVisible(false);
    const auto loadBrand=[](const char* name){int size=0;const auto* bytes=ChimeraArtworkData::getNamedResource(name,size);auto image=bytes ? juce::ImageFileFormat::loadFrom(bytes,(size_t)size) : juce::Image{};
        if(image.isValid() && image.hasAlphaChannel()){juce::Rectangle<int> bounds;const juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readOnly);for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x)if(pixels.getPixelColour(x,y).getAlpha()>10)bounds=bounds.getUnion({x,y,1,1});if(!bounds.isEmpty())image=image.getClippedImage(bounds);}return image;};
    wordmark=loadBrand("chimerawordmark_png");brandEmblem=loadBrand("spectralforgeemblem_png");
    mode.setName("Routing Mode"); mode.addItemList({"CLASSIC","DUAL","MATRIX"},1); add(mode);
    mode.setTooltip("Classic: one full-range rig. Dual: two parallel rigs. Matrix: three input bands.");
    quality.setName("Oversampling"); quality.addItemList({"1x","2x","4x","8x"},1); add(quality);
    quality.setTooltip("Anti-aliasing for amplifier distortion. Higher factors use more CPU. Host latency stays constant.");
    scale.setName("Interface size"); scale.addItemList({"75%","100%","125%","150%"},1); scale.setSelectedId(2); add(scale);
    scale.onChange=[this]{const float factor=float(scale.getSelectedId()+2)*.25f; setSize(juce::roundToInt(1180*factor),juce::roundToInt(780*factor));};
    add(info); info.setButtonText("SETTINGS"); info.onClick=[this]{
        juce::PopupMenu menu;menu.addItem(6,"Manual / Bug report / Updates...");menu.addSeparator();menu.addItem(1,"Save reference...");menu.addItem(2,"Load reference...");menu.addSeparator();menu.addItem(3,"About / IR credits");menu.addSeparator();menu.addItem(4,"Reset tuner A4 to 440 Hz");menu.addItem(5,"Clear MIDI assignments");
        const juce::Component::SafePointer<ChimeraEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&info),[safe](int result){if(!safe)return;if(result==6)safe->showSupport();else if(result==1)safe->referenceFile(true);else if(result==2)safe->referenceFile(false);else if(result==3)safe->showInfo();else if(result==4){auto* p=safe->processor.parameters().getParameter("tunerref");p->setValueNotifyingHost(p->convertTo0to1(440));}else if(result==5)safe->processor.clearMidi();});
    };
    info.setTooltip(juce::String("SpectralForge Chimera / Open Beta 1.0 / build ")+CHIMERA_BUILD_REVISION+"\nManual, bug reporting, updates, reference files and utility settings.");
    for(auto* button:{&compareA,&compareB,&copyAB,&irLibraryButton,&rigsTab,&preTab,&postTab}) add(*button);
    compareA.onClick=[this]{processor.selectComparison(0);markPresetCustom();timerCallback();};compareB.onClick=[this]{processor.selectComparison(1);markPresetCustom();timerCallback();};copyAB.onClick=[this]{processor.copyComparison();};
    compareA.setTooltip("Recall sound snapshot A: all parameters, models and embedded IRs. Both stereo channels are processed together.");compareB.setTooltip("Recall sound snapshot B. This is a stored sound, not the right audio channel.");
    copyAB.setTooltip("Copy the active sound to the other slot. A/B stores all parameters and IR audio; both slots are saved with the project/reference file.");
    irLibraryButton.setComponentID("irlibrary");irLibraryButton.onClick=[this]{loadIR(0);};
    preOrder.setComponentID("preorder");preOrder.setName("Pedal order");preOrder.addItemList({"SUSTAIN / COMP > ENV","TOUCH / ENV > COMP"},1);add(preOrder);preOrderAttachment=std::make_unique<CA>(p.parameters(),"preorder",preOrder);preOrder.onChange=[this]{updateModeUI();};
    preOrder.setTooltip("TOUCH lets the envelope follow your picking before compression. SUSTAIN compresses first for a steadier sweep. Fuzz, boost and drive remain after the shared clean tap.");
    gainOrder.setComponentID("gainorder");gainOrder.setName("Gain pedal order");gainOrder.addItemList({"FUZZ > BOOST > DRIVE","FUZZ > DRIVE > BOOST"},1);add(gainOrder);gainOrderAttachment=std::make_unique<CA>(p.parameters(),"gainorder",gainOrder);gainOrder.onChange=[this]{updateModeUI();};
    gainOrder.setTooltip("Boost before drive adds drive saturation. Boost after drive raises its output; a saturated amp may still add distortion instead of loudness. Fuzz stays first; Matrix LOW DI is unaffected.");
    rigsTab.onClick=[this]{page=0;updateModeUI();};preTab.onClick=[this]{page=1;updateModeUI();};postTab.onClick=[this]{page=2;updateModeUI();};
    const std::array<const char*,11> effectHeaders{"OVERDRIVE","DELAY","REVERB","COMPRESSOR","ENVELOPE","FUZZ","BOOST","BUS COMP","PREAMP","EQ","MODULATION"};
    const std::array<const char*,11> effectScopes{"05 / TIGHT GAIN","05 / SPACE","06 / SPACE","01 / DYNAMICS","02 / FILTER","03 / TEXTURE","04 / SHAPING","01 / DYNAMICS","02 / COLOUR","03 / TONE","04 / MODULATION"};
    const std::array<const char*,11> effectButtons{"preon","delayon","reverbon","precompon","filteron","fuzzon","booston","buscompon","preampon","eqon","choruson"};
    const std::array<const char*,11> descriptions{"Focused saturation before the amp.\nMatrix LOW DI stays clean.","Stereo echo. SYNC follows quarter-note tempo.","Room ambience after delay.","Even out dynamics and sustain.\nShared by DI and amp paths.","Touch-controlled low-pass sweep.\nShared by DI and amp paths.","Dense, asymmetric saturation.\nMatrix LOW DI stays clean.","Clean boost with two shelves.\nMatrix LOW DI stays clean.","Linked RMS bus compressor. 6 dB soft knee. Entire merged signal.","4x-oversampled colour stage with independent output trim.","Low/high shelves and a parametric mid band, applied after merge.","Stereo chorus with rate, depth and wet mix."};
    const std::array<std::array<const char*,5>,11> effectIds{{{"predrive","pretone","prelevel",nullptr,nullptr},{"delaytime","delayfeedback","delaymix",nullptr,nullptr},{"reverbsize","reverbdamping","reverbmix",nullptr,nullptr},{"precomp","precompattack","precomplevel",nullptr,nullptr},{"filtersense","filterq","filtermix",nullptr,nullptr},{"fuzzdrive","fuzztone","fuzzlevel",nullptr,nullptr},{"boostgain","boostbass","boosttreble",nullptr,nullptr},{"busthreshold","busratio","busattack","busrelease","busmakeup"},{"preampdrive","preampcolour","preamplevel",nullptr,nullptr},{"eqlow","eqmidhz","eqmid","eqq","eqhigh"},{"chorusrate","chorusdepth","chorusmix",nullptr,nullptr}}};
    const std::array<std::array<const char*,5>,11> effectLabels{{{"DRIVE","TONE","LEVEL",nullptr,nullptr},{"TIME","FEEDBACK","MIX",nullptr,nullptr},{"SIZE","DAMPING","MIX",nullptr,nullptr},{"SUSTAIN","ATTACK","LEVEL",nullptr,nullptr},{"SENSE","Q","MIX",nullptr,nullptr},{"DRIVE","BODY","LEVEL",nullptr,nullptr},{"GAIN","BASS","TREBLE",nullptr,nullptr},{"THRESHOLD","RATIO","ATTACK","RELEASE","MAKEUP"},{"GAIN","COLOUR","TRIM",nullptr,nullptr},{"LOW 80 Hz","MID FREQ","MID GAIN","MID Q","HIGH 8 kHz"},{"RATE","DEPTH","MIX",nullptr,nullptr}}};
    const std::array<std::array<const char*,5>,11> effectSuffix{{{""," Hz"," dB",nullptr,nullptr},{" ms","","",nullptr,nullptr},{"","","",nullptr,nullptr},{""," ms"," dB",nullptr,nullptr},{"","","",nullptr,nullptr},{" dB",""," dB",nullptr,nullptr},{" dB"," dB"," dB",nullptr,nullptr},{" dB",":1"," ms"," ms"," dB"},{" dB",""," dB",nullptr,nullptr},{" dB"," Hz"," dB",""," dB"},{" Hz","","",nullptr,nullptr}}};
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];style(effect.header,15);style(effect.scope,10);style(effect.description,10.f);
        effect.header.setText(effectHeaders[i],juce::dontSendNotification);effect.scope.setText(effectScopes[i],juce::dontSendNotification);effect.description.setText(descriptions[i],juce::dontSendNotification);
        effect.header.setTooltip(descriptions[i]);add(effect.header);add(effect.scope);add(effect.description);add(effect.enabled);effect.enabled.setClickingTogglesState(true);
        effect.enabled.setComponentID(effectButtons[i]);effect.button=std::make_unique<BA>(p.parameters(),effectButtons[i],effect.enabled);
        effect.model.setName(juce::String(spectralforge::modelFamilies[i].category)+" model");effect.model.setComponentID(spectralforge::modelFamilies[i].parameter);effect.model.addItemList(spectralforge::modelNames((int)i),1);add(effect.model);
        effect.modelAttachment=std::make_unique<CA>(p.parameters(),spectralforge::modelFamilies[i].parameter,effect.model);
        effect.model.onChange=[this]{updateHardwareStyles();repaint();};
        const bool pedal=i==0 || (i>=3 && i<=6);effect.enabled.getProperties().set("footswitch",pedal);
        for(size_t k=0;k<5;++k) {
            if(!effectIds[i][k]) continue;
            style(effect.labels[k],pedal ? 10.5f : 10.f,juce::Justification::centred);effect.labels[k].setText(effectLabels[i][k],juce::dontSendNotification);
            setupSlider(effect.controls[k],effectLabels[i][k],effectSuffix[i][k]);
            effect.controls[k].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            effect.controls[k].setTextBoxStyle(pedal ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,false,pedal ? 88 : 68,pedal ? 23 : 22);
            add(effect.labels[k]);add(effect.controls[k]);effect.attachments[k]=std::make_unique<SA>(p.parameters(),effectIds[i][k],effect.controls[k]);
        }
    }
    effects[1].controls[0].textFromValueFunction=[this](double value){return juce::String(delaySync.getToggleState() ? 60000.0/processor.currentTempo() : value,1);};
    inputMode.setName("Input mode");inputMode.addItemList({"STEREO","MONO L"},1);add(inputMode);inputModeAttachment=std::make_unique<CA>(p.parameters(),"inputmode",inputMode);
    inputMode.setTooltip("MONO L sends the left input to both channels. Stereo preserves separate channels.");
    presets.setName("Preset");
    juce::StringArray categories;for(const auto& preset:spectralforge::factoryPresets)categories.addIfNotAlreadyThere(preset.category);
    for(const auto& category:categories){juce::PopupMenu group;for(int i=0;i<spectralforge::factoryPresetCount;++i)if(category==spectralforge::factoryPresets[(size_t)i].category)group.addItem(i+1,spectralforge::factoryPresets[(size_t)i].name);presets.getRootMenu()->addSubMenu(category,group);}
    presets.setText("INIT / CUSTOM",juce::dontSendNotification);add(presets);
    presets.onChange=[this]{if(presets.getSelectedId()>0) {processor.loadFactoryPreset(presets.getSelectedId()-1);presetValues.clear();for(auto* parameter:processor.getParameters())presetValues.push_back(parameter->getValue());presets.setTooltip(spectralforge::factoryPresets[(size_t)(presets.getSelectedId()-1)].description);updateModeUI();}};
    for(auto* button:{&doublerOn,&midi,&tap,&hostTempo,&metronome,&presetPrevious,&presetNext,&presetSave,&presetLoad,&delaySync})add(*button);
    presetPrevious.onClick=[this]{presets.setSelectedId(presets.getSelectedId()<=1 ? spectralforge::factoryPresetCount : presets.getSelectedId()-1);};presetNext.onClick=[this]{presets.setSelectedId(presets.getSelectedId()>=spectralforge::factoryPresetCount ? 1 : presets.getSelectedId()+1);};
    presetSave.onClick=[this]{referenceFile(true);};presetLoad.onClick=[this]{referenceFile(false);};tap.onClick=[this]{processor.tapTempo();};midi.onClick=[this]{midiMenu();};
    dualType.setName("Dual type");dualType.addItemList({"BLEND","CROSSOVER"},1);add(dualType);dualTypeAttachment=std::make_unique<CA>(p.parameters(),"dualtype",dualType);dualType.onChange=[this]{updateModeUI();};
    const std::array<juce::Slider*,4> sliders{&doublerTime,&tempo,&dualBlend,&dualFrequency};const std::array<const char*,4> sliderIds{"doublertime","tempo","dualblend","dualcross"};const std::array<const char*,4> sliderSuffix{" ms"," BPM",""," Hz"};
    for(size_t i=0;i<4;++i){setupSlider(*sliders[i],sliderIds[i],sliderSuffix[i]);add(*sliders[i]);utilitySliders[i]=std::make_unique<SA>(p.parameters(),sliderIds[i],*sliders[i]);}
    tempo.textFromValueFunction=[this](double value){return juce::String(hostTempo.getToggleState() ? processor.currentTempo() : value,1);};
    tempo.setNumDecimalPlacesToDisplay(1);dualBlend.textFromValueFunction=[](double v){return juce::String(juce::roundToInt((1-v)*100))+":"+juce::String(juce::roundToInt(v*100));};dualBlend.updateText();
    const std::array<juce::TextButton*,4> buttons{&doublerOn,&hostTempo,&metronome,&delaySync};const std::array<const char*,4> buttonIds{"doubleron","temposync","metronome","delaysync"};
    for(size_t i=0;i<4;++i){buttons[i]->setClickingTogglesState(true);utilityButtons[i]=std::make_unique<BA>(p.parameters(),buttonIds[i],*buttons[i]);}
    style(dualLabel,11);add(dualLabel);doublerOn.setTooltip("Stereo decorrelation; inactive on a mono bus.");metronome.setTooltip("4/4 practice click at the displayed tempo. Off by default.");delaySync.setTooltip("Quarter-note delay from TAP/manual tempo or HOST BPM.");
    setupSlider(lowComp,"COMP");add(lowComp);lowCompAttachment=std::make_unique<SA>(p.parameters(),"lowcomp",lowComp);
    lowComp.setTooltip("One-knob VCA-style RMS compression: threshold and ratio move together. 0 = unity. Use LEVEL for makeup gain.");
    style(lowCompLabel,11.5f);lowCompLabel.setText("COMP",juce::dontSendNotification);add(lowCompLabel);
    setupSlider(lowAmpMix,"DI / AMP");add(lowAmpMix);lowAmpMix.setComponentID("lowampmix");lowAmpMixAttachment=std::make_unique<SA>(p.parameters(),"lowampmix",lowAmpMix);
    lowAmpMix.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100))+"% AMP";};lowAmpMix.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue()/100;};lowAmpMix.updateText();
    lowAmpMix.setTooltip("Blend compressed LOW DI with the selected head/cab. 0% = DI; 100% = amp/cab. Head drive is fixed at 0 in Matrix LOW. IR capture phase is retained.");
    style(diVoice,10);diVoice.setText("DI / AMP",juce::dontSendNotification);add(diVoice);
    style(diNote,12);diNote.setText("HEAD DRIVE: 0 / COMP BEFORE BLEND",juce::dontSendNotification);add(diNote);
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
    for(const auto i:{0,4,6}) {globalSliders[(size_t)i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);globalSliders[(size_t)i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,104,22);globalSliders[(size_t)i].getProperties().set("knobStyle",3);}
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
        style(lane.ampReference,10.5f);lane.ampReference.setColour(juce::Label::textColourId,muted);lane.ampReference.setComponentID("ampreference"+n);add(lane.ampReference);
        style(lane.header,15.f); style(lane.range,12.f); style(lane.tonePivot,11.f); style(lane.cabStatus,11.f);
        lane.range.setColour(juce::Label::textColourId,muted); lane.cabStatus.setColour(juce::Label::textColourId,muted);
        add(lane.header); add(lane.range); add(lane.tonePivot); add(lane.cabStatus);
        lane.amp.setName("Amp "+n); lane.amp.addItemList(spectralforge::ampNames(),1); add(lane.amp);
        lane.aa=std::make_unique<CA>(state,"amp"+n,lane.amp);
        lane.amp.onChange=[this]{updateHardwareStyles();repaint();};
        auto controls=lane.controls();
        for(size_t k=0;k<8;++k)
        {
            setupSlider(*controls[k],names[k],k==0 ? "" : " dB"); add(*controls[k]);
            style(lane.knobLabels[k],11.5f); lane.knobLabels[k].setText(names[k],juce::dontSendNotification); add(lane.knobLabels[k]);
            lane.sa[k]=std::make_unique<SA>(state,juce::String(parameterIds[k])+n,*controls[k]);
        }
        setupSlider(lane.bandTone,"BAND TONE"," dB"); add(lane.bandTone); style(lane.toneLabel,11.5f);
        lane.toneLabel.setText("BAND TONE",juce::dontSendNotification); add(lane.toneLabel);
        lane.bandTone.setTooltip("Tilt within this band. Negative: darker. Positive: brighter. Pivot follows the crossover.");
        lane.toneAttachment=std::make_unique<SA>(state,"bandtone"+n,lane.bandTone);
        lane.cabType.setName("Cabinet "+n);lane.cabType.setComponentID("cabinet"+n);add(lane.cabType);
        lane.cabType.selected=[this,i,n](juce::File file,int source) {
            if(file!=juce::File{}) processor.loadIR(i,file);
            else {auto* p=processor.parameters().getParameter("cabtype"+n);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(source)));p->endChangeGesture();}
            timerCallback();
        };
        lane.cabType.browse=[this,i]{loadIR(i);};lane.cabType.refresh();
        lane.cabType.setTooltip("Choose a built-in or installed cabinet IR directly. The menu refreshes when opened. Loaded IR audio is saved inside the project and A/B slots.");
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
        add(lane.details);lane.details.setComponentID("irtags"+n);lane.details.onClick=[this,i]{showIRDetails(i);};lane.details.setTooltip("Inspect or edit speaker, diameter, microphone, position, distance and provenance.");
        add(lane.load); lane.load.onClick=[this,i]{loadIR(i);}; lane.load.setTooltip("Open the cabinet library: factory, bass/guitar references and installed user IRs. Import the personal ZIP once.");
    }
    mode.onChange=[this]{updateModeUI();}; x1.onValueChange=[this]{updateBandLabels();}; x2.onValueChange=[this]{updateBandLabels();};
    setResizable(true,true); setResizeLimits(885,585,1770,1170); getConstrainer()->setFixedAspectRatio(1180.0/780.0);
    setSize(1180,780); updateModeUI(); startTimerHz(25);
}
ChimeraEditor::~ChimeraEditor() { stopTimer(); chooser.reset(); for(auto& dialog:dialogs)if(dialog)delete dialog.getComponent();dialogs.clear();support->cancel();support.reset();setLookAndFeel(nullptr); }
void ChimeraEditor::setupSlider(juce::Slider& slider,const juce::String& name,const juce::String& suffix)
{
    slider.setName(name); slider.setTooltip(name); slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,66,24); slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(name=="DRIVE" ? 2 : 1);
}
void ChimeraEditor::loadIR(int lane)
{
    const juce::Component::SafePointer<ChimeraEditor> safe(this);juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new IRBrowserPanel([safe,lane](juce::File file,int factory){if(!safe)return;
        if(factory) {auto* p=safe->processor.parameters().getParameter("cabtype"+juce::String(lane+1));p->setValueNotifyingHost(p->convertTo0to1(float(factory)));}
        else safe->processor.loadIR(lane,file);safe->timerCallback();}));
    options.dialogTitle="SpectralForge Chimera / Cabinet library / Rig "+juce::String(lane+1);options.dialogBackgroundColour=background;
    options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
}
void ChimeraEditor::chooseIR(int lane,bool folder)
{
    chooser=std::make_unique<juce::FileChooser>(folder ? "Choose IR collection folder" : "Load cabinet IR",juce::File{},folder ? "" : "*.wav;*.aif;*.aiff");
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|(folder ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),[safe,lane,folder](const juce::FileChooser& file){if(!safe)return;if(folder && file.getResult().isDirectory()){safe->processor.parameters().state.setProperty("irFolder",file.getResult().getFullPathName(),nullptr);safe->browseIR(lane,file.getResult());}else if(file.getResult().existsAsFile()){safe->processor.loadIR(lane,file.getResult());safe->timerCallback();}});
}
void ChimeraEditor::browseIR(int lane,const juce::File& folder)
{
    const juce::Component::SafePointer<ChimeraEditor> safe(this);juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new IRBrowserPanel(folder,[safe,lane](juce::File file){if(safe){safe->processor.loadIR(lane,file);safe->timerCallback();}}));options.dialogTitle="Chimera IR collection";options.dialogBackgroundColour=background;options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
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
    for(int i=0;i<count;++i) if(x>=20+i*(width+14) && x<20+i*(width+14)+width) { processor.loadIR(i,juce::File(files[0])); timerCallback(); break; }
}
void ChimeraEditor::showInfo()
{
    const auto copyrightNotice = juce::String::fromUTF8(
        "Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.\n\n"
        "Third-party software and assets are subject to their respective copyright notices and license terms.");
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"SpectralForge Chimera",
        juce::String("SpectralForge Chimera | Open Beta 1.0\nRavenForge Luthier Intelligence\nBuild ")+CHIMERA_BUILD_REVISION+"\n\nAmp voices are algorithmic interpretations, not verified hardware replicas.\n\nFactory IRs: jesterdyne, CC BY 4.0\nEngl Celestion V30 SM57 center-01.wav\nhttps://freesound.org/s/116735/\nJensen Cab SM57 center.wav\nhttps://freesound.org/s/116743/\nhttps://creativecommons.org/licenses/by/4.0/\nFiles unchanged; normalised and resampled during playback.\n\nPitch: Chimera STFT / JUCE FFT\n\nComplete notices are supplied with the download.\n\n"+copyrightNotice,"OK",this);
}
void ChimeraEditor::timerCallback()
{
    updateHardwareStyles();
    if(!presetValues.empty()) {const auto& params=processor.getParameters();bool changed=presetValues.size()!=size_t(params.size());for(int i=0;!changed && i<params.size();++i)changed=std::abs(params[i]->getValue()-presetValues[(size_t)i])>1e-6f;if(changed)markPresetCustom();}
    const int current=(int)processor.parameters().getRawParameterValue("mode")->load();
    if(lastBoostAfterDrive!=(gainOrder.getSelectedId()==2) || lastEnvelopeFirst!=(preOrder.getSelectedId()==2) || current!=lastMode || lastDualCross!=(dualType.getSelectedId()==2) || lastTuner!=tunerOn.getToggleState()) updateModeUI();
    if(current==2 || (current==1 && lastDualCross)) updateBandLabels();
    compareA.setToggleState(processor.comparisonSlot()==0,juce::dontSendNotification);compareB.setToggleState(processor.comparisonSlot()==1,juce::dontSendNotification);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i];
        const bool active=processor.parameters().getRawParameterValue("cab"+juce::String(i+1))->load()>.5f;
        lane.cabType.sync((int)processor.parameters().getRawParameterValue("cabtype"+juce::String(i+1))->load(),processor.userIRName(i));
        lane.cabStatus.setText(current==2 && i==0 ? "GR "+juce::String(processor.lowCompMeter(),1)+" dB | DRIVE 0 | "+processor.cabStatus(i) : (active ? "" : "BYPASSED | ")+processor.cabStatus(i),juce::dontSendNotification);
        lane.cabStatus.setTooltip(lane.cabStatus.getText()+"\n"+processor.cabMetadata(i).summary());
        lane.cabLow.setEnabled(active); lane.cabHigh.setEnabled(active);
    }
    for(size_t i=0;i<effects.size();++i){
        auto& effect=effects[i];effect.enabled.setButtonText(effect.enabled.getToggleState() ? "ON" : "OFF");
        const auto& model=spectralforge::modelInfo((int)i,effect.model.getSelectedId()-1);
        const juce::String scope=i==0 || i==5 || i==6 ? "Matrix LOW DI stays clean." : i==3 || i==4 ? "Shared by DI and amp paths." : "Applied after rig merge.";
        const auto description=juce::String(model.character)+"\n"+scope;
        effect.model.setTooltip(juce::String("Reference: ")+model.reference+"\n"+description);
        effect.header.setTooltip(description);effect.description.setTooltip(description);
        effect.description.setText(i==3 ? "GAIN REDUCTION  "+juce::String(effect.enabled.getToggleState() ? processor.preCompressorReduction() : 0.f,1)+" dB" : juce::String(model.character).replace(" / ","\n"),juce::dontSendNotification);effect.scope.setText(model.reference,juce::dontSendNotification);
    }
    const int eq=effects[9].model.getSelectedId();effects[9].labels[0].setText(eq==2 ? "LOW 110 Hz" : eq==3 ? "LOW 60 Hz" : "LOW 80 Hz",juce::dontSendNotification);effects[9].labels[4].setText(eq==2 ? "HIGH 12 kHz" : eq==3 ? "HIGH 10 kHz" : "HIGH 8 kHz",juce::dontSendNotification);
    midi.setButtonText(processor.learningMidi() ? "LEARN" : "MIDI");tempo.setEnabled(!hostTempo.getToggleState());effects[1].controls[0].setEnabled(!delaySync.getToggleState());effects[1].controls[0].updateText();tempo.updateText();
    gateStatus.setText(gateOn.getToggleState() ? "REDUCTION  "+juce::String(-juce::Decibels::gainToDecibels(processor.gateMeter(),-90.f),1)+" dB" : "BYPASSED",juce::dontSendNotification);
    const double sr=processor.getSampleRate()>0 ? processor.getSampleRate() : 48000;
    pitchStatus.setText(pitchOn.getToggleState() ? "+ "+juce::String(1000.0*processor.pitchLatency()/sr,1)+" ms latency" : "BYPASSED | zero added latency",juce::dontSendNotification);
    repaint();
}
void ChimeraEditor::updateHardwareStyles()
{
    auto apply=[](juce::Slider& slider,int hardware,bool bright) {
        slider.getProperties().set("knobStyle",hardware);slider.getProperties().set("brightFace",bright);
        slider.setColour(juce::Slider::textBoxTextColourId,bright ? juce::Colour(0xff202923) : ink);
        slider.setColour(juce::Slider::textBoxBackgroundColourId,bright ? juce::Colour(0xffeae5d9).withAlpha(.67f) : background.withAlpha(.8f));
    };
    for(size_t family=0;family<effects.size();++family) {
        auto& effect=effects[family];const int model=juce::jmax(0,effect.model.getSelectedId()-1);
        if(effect.lastModel==model)continue;
        effect.lastModel=model;const bool pedal=family==0 || (family>=3 && family<=6);
        const auto hardware=pedal ? spectralforge::art::pedalStyle((int)family,model) : spectralforge::art::rackStyle((int)family,model);
        const auto labelInk=hardware.brightFace ? juce::Colour(0xff202923) : ink;
        // A fixed native header strip keeps names readable over dark trim on
        // otherwise bright housings (M87, AW-3, Bass DI and FET references).
        effect.header.setColour(juce::Label::textColourId,ink);
        effect.scope.setColour(juce::Label::textColourId,pedal ? labelInk.withAlpha(.85f) : muted);
        effect.description.setColour(juce::Label::textColourId,ink);
        effect.enabled.getProperties().set("brightFace",hardware.brightFace);
        for(size_t k=0;k<effect.controls.size();++k) {
            int knob=hardware.knobStyle;
            if(knob==8)knob=k==0 ? 6 : 5;
            else if(knob==5)knob=k%3==0 ? 5 : k%3==1 ? 7 : 0;
            apply(effect.controls[k],knob,hardware.brightFace);
            effect.labels[k].setColour(juce::Label::textColourId,labelInk);
            // Rack trim and decorative meter rims can cross a live label.
            // Keep each control name on the same readable field as its value.
            effect.labels[k].setColour(juce::Label::backgroundColourId,pedal ? juce::Colours::transparentBlack
                : hardware.brightFace ? juce::Colour(0xffeae5d9).withAlpha(.90f) : background.withAlpha(.88f));
        }
    }
    for(size_t i=0;i<lanes.size();++i) {
        auto& lane=lanes[i];const int model=juce::jmax(0,lane.amp.getSelectedId()-1);
        if(lane.lastModel==model)continue;
        lane.lastModel=model;const auto& amp=spectralforge::ampInfo(model);lane.ampReference.setText(amp.reference,juce::dontSendNotification);lane.amp.setTooltip(juce::String(amp.name)+" / "+amp.reference+"\n"+amp.character);lane.ampReference.setTooltip(juce::String(amp.reference)+"\nAlgorithmic voicing reference; not a measured hardware replica.");const auto hardware=spectralforge::art::headStyle(model);
        for(auto* slider:lane.controls())apply(*slider,hardware.knobStyle,false);
        apply(lane.bandTone,hardware.knobStyle,false);
        if(i==0)apply(lowComp,hardware.knobStyle,false);
    }
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
    lastEnvelopeFirst=preOrder.getSelectedId()==2;pedalOrder=lastEnvelopeFirst ? std::array<int,5>{4,3,5,6,0} : std::array<int,5>{3,4,5,6,0};
    lastBoostAfterDrive=gainOrder.getSelectedId()==2;if(lastBoostAfterDrive)std::swap(pedalOrder[3],pedalOrder[4]);
    preOrder.setVisible(page==1 && !tunerOn.getToggleState());gainOrder.setVisible(page==1 && !tunerOn.getToggleState());
    lastMode=(int)processor.parameters().getRawParameterValue("mode")->load();
    lastDualCross=dualType.getSelectedId()==2;lastTuner=tunerOn.getToggleState();
    const bool matrix=lastMode==2,split=matrix || (lastMode==1 && lastDualCross); const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    x1.setVisible(matrix && page==0 && !lastTuner); x2.setVisible(matrix && page==0 && !lastTuner); x1Label.setVisible(matrix && page==0 && !lastTuner); x2Label.setVisible(matrix && page==0 && !lastTuner);
    rigsTab.setToggleState(page==0,juce::dontSendNotification);preTab.setToggleState(page==1,juce::dontSendNotification);postTab.setToggleState(page==2,juce::dontSendNotification);
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];const bool pedal=i==0 || (i>=3 && i<=6),show=pedal ? page==1 : page==2;
        effect.header.setVisible(show);effect.scope.setVisible(show);effect.description.setVisible(show && pedal);effect.enabled.setVisible(show);effect.model.setVisible(show);
        for(int k=0;k<5;++k){const bool visible=show && effect.attachments[(size_t)k]!=nullptr;effect.controls[k].setVisible(visible);effect.labels[k].setVisible(visible);}
    }
    for(juce::Component* c:std::initializer_list<juce::Component*>{&lowComp,&lowCompLabel,&lowAmpMix,&diVoice}) c->setVisible(matrix && page==0);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i]; const bool show=i<count && page==0;
        for(juce::Component* c : std::initializer_list<juce::Component*>{&lane.header,&lane.range,&lane.ampReference,&lane.amp,&lane.ampOn,&lane.mute,&lane.solo,&lane.polarity,&lane.cabOn,&lane.load,&lane.details,&lane.cabType,&lane.cabLow,&lane.cabHigh,&lane.lowLabel,&lane.highLabel,&lane.cabStatus}) c->setVisible(show);
        auto controls=lane.controls();
        for(size_t k=0;k<8;++k) { controls[k]->setVisible(show && (!split || k<2)); lane.knobLabels[k].setVisible(show && (!split || k<2)); }
        if(matrix && i==0) {
            for(juce::Component* c:std::initializer_list<juce::Component*>{&lane.drive,&lane.knobLabels[0]}) c->setVisible(false);
        }
        lane.bandTone.setVisible(show && split); lane.toneLabel.setVisible(show && split); lane.tonePivot.setVisible(show && split);
        lane.header.setText(matrix ? (i==0 ? "01 / LOW" : i==1 ? "02 / MID" : "03 / HIGH") : "RIG "+juce::String(i+1),juce::dontSendNotification);
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
    spectralforge::art::raster(g,spectralforge::art::Surface::workbench,{0,0,1180,780});
    g.setColour(background.withAlpha(.40f));g.fillRect(0,0,1180,780);
    g.setColour(accent.withAlpha(.7f));g.fillRect(20,20,2,36);
    text(g,"SOUND A / B",490,11,121,14,8.5f,muted);
    if(brandEmblem.isValid())g.drawImageWithin(brandEmblem,25,5,62,60,juce::RectanglePlacement::centred);
    if(wordmark.isValid())g.drawImageWithin(wordmark,96,4,365,59,juce::RectanglePlacement::centred);
    else {text(g,"SPECTRALFORGE",106,5,350,15,10.f);text(g,"CHIMERA",103,17,360,45,36.f,ink);}
    text(g,"OVERSAMPLING",816,8,110,22,10.f); text(g,"SIZE",986,8,64,22,10.f);
    g.setColour(line); g.drawHorizontalLine(67,20,1160);
    const std::array<juce::Rectangle<float>,5> panels{{{20,80,138,168},{170,80,254,168},{436,80,176,168},{624,80,326,168},{962,80,198,168}}};
    for(auto box:panels) { g.setColour(panel.withAlpha(.88f)); g.fillRoundedRectangle(box,5); g.setColour(line); g.drawRoundedRectangle(box,5,1); }
    text(g,"INPUT",32,91,104,24,12.f,ink);
    text(g,"THRESH",182,126,63,26,10.f); text(g,"RELEASE",182,160,64,26,10.f); text(g,"HOLD",308,160,54,26,10.f);
    text(g,"OUTPUT",976,91,152,24,12.f,ink);
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
            const float y=330.f+i*69.f;const auto& effect=effects[(size_t)rackOrder[(size_t)i]];
            const auto colour=juce::Colour(spectralforge::modelInfo(rackOrder[(size_t)i],effect.model.getSelectedId()-1).colour);
            g.setColour(panel.withAlpha(.9f));g.fillRoundedRectangle(20,y,1140,64,4);
            spectralforge::art::rack(g,{296,y,864,64},rackOrder[(size_t)i],effect.model.getSelectedId()-1);
            g.setColour(colour.withAlpha(.85f));g.fillRect(28.f,y+8,2.f,46.f);
            g.setColour(effect.enabled.getToggleState() ? juce::Colour(0xffa5c1a0) : line);g.fillEllipse(36,y+26,5,5);
            const float level=processor.postModuleLevel(i);
            const float readout=i==0 ? (effect.enabled.getToggleState() ? processor.postCompressorReduction() : 0.f) : juce::Decibels::gainToDecibels(level,-60.f);
            const auto meterInk=spectralforge::art::rackStyle(rackOrder[(size_t)i],effect.model.getSelectedId()-1).brightFace ? juce::Colour(0xff26312c) : ink;
            const float amount=i==0 ? juce::jlimit(0.f,1.f,readout/24.f) : juce::jlimit(0.f,1.f,(readout+60.f)/60.f);
            g.setColour(background.withAlpha(.83f));g.fillRoundedRectangle(318,y+16,112,32,3);
            text(g,i==0 ? "GR  "+juce::String(readout,1)+" dB" : "OUT  "+juce::String(readout,1)+" dBFS",322,(int)y+17,104,14,8.5f,ink,juce::Justification::centred);
            for(int segment=0;segment<16;++segment){g.setColour(segment<float(amount*16) ? (level>=1 && i!=0 ? juce::Colour(0xffe49b73) : juce::Colour(0xffa5c1a0)) : line);g.fillRect(326.f+segment*6,y+35,4.f,5.f);}
            g.setColour(meterInk.withAlpha(.22f));g.drawVerticalLine(447,y+10,y+54);
        }
    } else {
        const int count=page==1 ? 5 : lastMode==0 ? 1 : lastMode==1 ? 2 : 3;const int width=(1140-14*(count-1))/count;
        for(int i=0;i<count;++i) {
            const float x=float(20+i*(width+14));
            if(page==1) {
                const int family=pedalOrder[(size_t)i];const auto& effect=effects[(size_t)family];const int model=effect.model.getSelectedId()-1;
                const auto colour=juce::Colour(spectralforge::modelInfo(family,model).colour);
                spectralforge::art::pedal(g,{x,330,float(width),412},family,model);
                g.setColour(background.withAlpha(.88f));g.fillRoundedRectangle(x+13,344,float(width-26),22,3);
                g.setColour(background.withAlpha(.85f));g.fillRoundedRectangle(x+13,619,float(width-26),28,3);
                g.setColour(colour.withAlpha(.65f));g.fillRect(x+14,339.f,float(width-28),2.f);
                const auto live=juce::Colour(0xffb8d99c);
                g.setColour(effect.enabled.getToggleState() ? live.withAlpha(.18f) : juce::Colours::transparentBlack);g.fillEllipse(x+width*.5f-10,645,20,20);
                g.setColour(effect.enabled.getToggleState() ? live : line);g.fillEllipse(x+width*.5f-3,652,6,6);
                g.setColour(background.withAlpha(.7f));g.fillRoundedRectangle(x+60,719,float(width-120),15,2);
                text(g,juce::String(i+1)+" / IN > OUT",(int)x+16,719,width-32,14,8.f,ink,juce::Justification::centred);
            } else {
                const auto& lane=lanes[(size_t)i];const bool di=lastMode==2 && i==0;const auto colour=di ? juce::Colour(0xff40544c) : spectralforge::art::ampColour(lane.amp.getSelectedId()-1);
                g.setColour(panel.withAlpha(.8f));g.fillRoundedRectangle(x,330,float(width),412,5);
                g.setColour(line.withAlpha(.65f));g.drawRoundedRectangle(x,330,float(width),412,5,1);
                if(lastMode==0) {
                    spectralforge::art::head(g,{x+12,450,466,170},lane.amp.getSelectedId()-1);
                    g.setColour(background.withAlpha(.65f));g.fillRoundedRectangle(x+490,434,float(width-502),186,3);
                } else {
                    g.setColour(background.withAlpha(.48f));g.fillRoundedRectangle(x+10,434,float(width-20),86,3);
                    spectralforge::art::head(g,{x+12,450,float(width-24),70},lane.amp.getSelectedId()-1);
                    g.setColour(background.withAlpha(.65f));g.fillRoundedRectangle(x+10,524,float(width-20),96,3);
                }
                g.setColour(colour.withAlpha(.65f));g.fillRect(x+12,332.f,float(width-24),2.f);
                spectralforge::art::cabinet(g,{x+12,680,51,43},processor.cabMetadata(i));
                if(di) {
                    const float reduction=juce::jlimit(0.f,1.f,processor.lowCompMeter()/24.f);
                    g.setColour(line);g.fillRoundedRectangle(x+17,638,float(width-34),3,1.5f);
                    if(reduction>0.f){g.setColour(accent);g.fillRoundedRectangle(x+17,638,float(width-34)*reduction,3,1.5f);}
                }
            }
        }
    }
    text(g,"CPU "+juce::String(processor.cpuLoad(),1)+"%  |  PK "+juce::String(processor.cpuPeakLoad(),1)+"%",601,750,228,25,10.5f,processor.cpuPeakLoad()>=100 ? juce::Colours::salmon : muted);
    text(g,(hostTempo.getToggleState() ? "HOST " : "TEMPO ")+juce::String(processor.currentTempo(),1)+" BPM  |  "+juce::String(processor.getLatencySamples())+" SAMPLES",833,750,327,25,10.5f,muted,juce::Justification::centredRight);

}
void ChimeraEditor::resized()
{
    const float factor=getWidth()/1180.f;const int choice=juce::jlimit(1,4,juce::roundToInt((factor-.75f)*4)+1);
    if(std::abs(factor-(choice+2)*.25f)<.002f) scale.setSelectedId(choice,juce::dontSendNotification);
    else {scale.setSelectedId(0,juce::dontSendNotification);scale.setText(juce::String(factor*100,0)+"%",juce::dontSendNotification);}
    canvas.setBounds(0,0,1180,780); canvas.setTransform(juce::AffineTransform::scale(getWidth()/1180.f));
    layoutControls();
}
void ChimeraEditor::layoutControls()
{
    preOrder.setBounds(585,268,257,28);gainOrder.setBounds(852,268,308,28);
    compareA.setBounds(490,30,30,28);compareB.setBounds(524,30,30,28);copyAB.setBounds(558,30,52,28);rigsTab.setBounds(634,30,52,28);preTab.setBounds(692,30,52,28);postTab.setBounds(750,30,52,28);
    title.setBounds(30,16,166,40); quality.setBounds(816,30,130,28); scale.setBounds(986,30,80,28);irLibraryButton.setBounds(1074,30,86,28); info.setBounds(504,750,86,25);
    mode.setBounds(106,268,140,28); routingHelp.setBounds(lastMode==1 ? 415 : 258,268,lastMode==1 ? 160 : 277,28);
    dualType.setBounds(260,268,144,28);dualLabel.setBounds(597,257,280,20);dualBlend.setBounds(597,280,330,28);dualFrequency.setBounds(597,280,330,28);
    x1Label.setBounds(565,258,224,20); x1.setBounds(565,280,252,28);
    x2Label.setBounds(858,258,250,20); x2.setBounds(858,280,290,28);
    gateOn.setBounds(182,93,78,24); pitchOn.setBounds(450,93,148,24); tunerOn.setBounds(20,750,64,25);tunerMute.setBounds(965,273,112,25);
    globalSliders[0].setBounds(30,115,106,92);
    globalSliders[1].setBounds(244,126,167,28);
    globalSliders[2].setBounds(180,184,111,27); globalSliders[3].setBounds(306,184,106,27);
    globalSliders[4].setBounds(448,123,152,85); globalSliders[5].setBounds(799,273,140,25); globalSliders[6].setBounds(974,115,159,92);
    gateStatus.setBounds(180,217,232,22); pitchStatus.setBounds(447,212,158,29);
    inputMode.setBounds(638,107,108,26);presets.setBounds(756,107,180,26);presetPrevious.setBounds(638,144,28,25);presetNext.setBounds(670,144,28,25);presetSave.setBounds(706,144,107,25);presetLoad.setBounds(821,144,115,25);
    doublerOn.setBounds(638,190,91,26);doublerTime.setBounds(741,190,195,26);
    midi.setBounds(91,750,61,25);tap.setBounds(160,750,44,25);tempo.setBounds(211,750,128,25);hostTempo.setBounds(347,750,58,25);metronome.setBounds(413,750,82,25);
    for(int position=0;position<5;++position) {
        auto& effect=effects[(size_t)pedalOrder[(size_t)position]];const int x=20+position*230;
        effect.header.setBounds(x+14,344,186,22);effect.model.setBounds(x+14,369,186,27);effect.scope.setBounds(x+14,399,186,18);
        for(int k=0;k<3;++k){const int left=x+(k==2 ? 62 : 10+k*101),top=k==2 ? 518 : 423;effect.labels[(size_t)k].setBounds(left,top,94,20);effect.controls[(size_t)k].setBounds(left,top+22,94,77);}
        effect.description.setBounds(x+14,619,187,28);effect.enabled.setBounds(x+63,665,90,53);
    }
    for(int position=0;position<6;++position) {
        auto& effect=effects[(size_t)rackOrder[(size_t)position]];const int y=330+position*69;effect.header.setBounds(48,y+3,186,18);effect.model.setBounds(48,y+23,182,24);effect.scope.setBounds(48,y+47,182,14);effect.enabled.setBounds(241,y+20,43,26);
        int count=0;for(auto& attachment:effect.attachments)if(attachment)++count;const int cell=692/count;
        for(int k=0;k<count;++k){const int controlWidth=juce::jmin(144,cell-7),x=458+k*cell+(cell-controlWidth)/2;effect.labels[(size_t)k].setBounds(x,y+2,controlWidth,16);effect.controls[(size_t)k].setBounds(x,y+17,controlWidth,45);}
    }
    delaySync.setBounds(241,330+4*69+47,43,15);
    diVoice.setBounds(32,615,68,20);lowAmpMix.setBounds(102,614,276,23);diNote.setVisible(false);
    const bool matrix=lastMode==2,split=matrix || (lastMode==1 && lastDualCross); const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    const int width=(1140-14*(count-1))/count;
    for(int i=0;i<count;++i)
    {
        auto& lane=lanes[i]; const int x=20+i*(width+14);
        lane.header.setBounds(x+12,342,width-165,24); lane.range.setBounds(x+12,371,width-24,20);
        lane.mute.setBounds(x+width-147,344,48,23); lane.solo.setBounds(x+width-95,344,44,23); lane.polarity.setBounds(x+width-47,344,35,23);
        lane.ampOn.setBounds(x+12,398,48,32);lane.amp.setBounds(x+68,398,width-80,32);lane.ampReference.setBounds(x+14,432,lastMode==0 ? 460 : width-28,16);
        if(split)
        {
            const std::array<juce::Slider*,3> controls{matrix && i==0 ? &lowComp : &lane.drive,&lane.level,&lane.bandTone};
            const std::array<juce::Label*,3> labels{matrix && i==0 ? &lowCompLabel : &lane.knobLabels[0],&lane.knobLabels[1],&lane.toneLabel};
            const int cell=(width-24)/3;
            for(int k=0;k<3;++k) { labels[k]->setFont(juce::FontOptions(11.f));labels[k]->setJustificationType(juce::Justification::centred);labels[k]->setBounds(x+12+k*cell,524,cell-4,18);controls[k]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);controls[k]->setTextBoxStyle(juce::Slider::TextBoxBelow,false,cell-8,21);controls[k]->setBounds(x+12+k*cell,542,cell-4,72); }
            lane.tonePivot.setBounds(x+12,615,width-24,21);if(matrix && i==0)lane.tonePivot.setVisible(false);
        }
        else
        {
            const int cell=count==1 ? (width-510)/4 : (width-24)/8;
            auto controls=lane.controls();
            for(int k=0;k<8;++k)
            {
                const int left=count==1 ? x+496+(k%4)*cell : x+12+k*cell, top=count==1 ? 438+(k/4)*89 : 526;
                lane.knobLabels[k].setBounds(left,top,cell-3,18);lane.knobLabels[k].setFont(juce::FontOptions(count==1 ? 11.f : 9.f));
                lane.knobLabels[k].setJustificationType(juce::Justification::centred);
                controls[k]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);controls[k]->setTextBoxStyle(juce::Slider::TextBoxBelow,false,cell-4,21);controls[k]->setBounds(left,top+19,cell-3,count==1 ? 66 : 71);
            }
        }
        lane.cabOn.setBounds(x+12,643,48,26); lane.cabType.setBounds(x+68,643,width-197,26); lane.load.setBounds(x+width-121,643,54,26);lane.details.setBounds(x+width-61,643,49,26);
        const int cell=(width-90)/2;
        lane.lowLabel.setBounds(x+78,674,cell-8,18); lane.highLabel.setBounds(x+78+cell,674,cell-8,18);
        lane.cabLow.setBounds(x+78,692,cell-8,26); lane.cabHigh.setBounds(x+78+cell,692,cell-8,26);
        lane.cabStatus.setBounds(x+12,719,width-24,21);
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
        if(ok) {safe->markPresetCustom();safe->presets.setSelectedId(0,juce::dontSendNotification);safe->presets.setText(file.getResult().getFileNameWithoutExtension(),juce::dontSendNotification);}
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

void ChimeraEditor::showIRDetails(int lane)
{
    const bool editable=processor.parameters().getRawParameterValue("cabtype"+juce::String(lane+1))->load()==3;
    const juce::Component::SafePointer<ChimeraEditor> safe(this);juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new IRDetailsPanel(processor.cabMetadata(lane),editable,[safe,lane](spectralforge::IRMetadata m){if(safe)safe->processor.setCabMetadata(lane,m);}));
    options.dialogTitle="Chimera IR details";options.dialogBackgroundColour=background;options.escapeKeyTriggersCloseButton=true;options.useNativeTitleBar=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
}
void ChimeraLookAndFeel::drawButtonText(juce::Graphics& g,juce::TextButton& button,bool hover,bool down)
{
    if((bool)button.getProperties()["footswitch"]) {g.setColour(background.withAlpha(.8f));g.fillRoundedRectangle(button.getWidth()*.5f-19,float(button.getHeight()-13),38,13,2);g.setFont(juce::FontOptions(9.f));g.setColour(ink);g.drawText(button.getButtonText(),0,button.getHeight()-13,button.getWidth(),13,juce::Justification::centred);return;}
    juce::LookAndFeel_V4::drawButtonText(g,button,hover,down);
}

void ChimeraEditor::showSupport()
{
    spectralforge::release::Diagnostics d;
    using Format=spectralforge::release::Diagnostics::Format;
    d.format=processor.wrapperType==juce::AudioProcessor::wrapperType_Standalone ? Format::standalone : processor.wrapperType==juce::AudioProcessor::wrapperType_VST3 ? Format::vst3 : processor.wrapperType==juce::AudioProcessor::wrapperType_AudioUnit ? Format::au : Format::other;
    d.sampleRate=processor.getSampleRate();d.blockSize=processor.getBlockSize();d.inputs=processor.getTotalNumInputChannels();d.outputs=processor.getTotalNumOutputChannels();d.latencySamples=processor.getLatencySamples();
    juce::DialogWindow::LaunchOptions options;options.content.setOwned(new ChimeraSupportPanel(support,d));options.dialogTitle="Chimera / Support & Updates";options.dialogBackgroundColour=background;options.escapeKeyTriggersCloseButton=true;options.useNativeTitleBar=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
}

void ChimeraEditor::trackDialog(juce::DialogWindow* dialog)
{
    dialogs.erase(std::remove_if(dialogs.begin(),dialogs.end(),[](const auto& item){return item==nullptr;}),dialogs.end());
    if(dialog)dialogs.emplace_back(dialog);
}
void ChimeraEditor::markPresetCustom()
{
    presetValues.clear();presets.setSelectedId(0,juce::dontSendNotification);presets.setText("CUSTOM",juce::dontSendNotification);presets.setTooltip("Edited sound or recalled reference. SAVE AS preserves the complete sound and embedded IRs.");
}

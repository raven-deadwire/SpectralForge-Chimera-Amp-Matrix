#include "CabIRFile.h"
#include "KnobInteraction.h"
#include <limits>
#include "WindowFit.h"
#include "CabRoom.h"
#include "PresetOrder.h"
#include "GuitarSignaturePresets.h"
#include "PresetSummaries.h"
#include "PluginEditor.h"
#include "DiagnosticsPanel.h"
#include "HardwareArtwork.h"
#include "AmpCatalog.h"
#include "FactoryPresets.h"
#include "SupportPanel.h"
#include "ReleaseInfo.h"
#include "RackEffectDetailPanel.h"
#include "BetaLifecycleDisplay.h"

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
// Plain sentence first for new players, then the full technical recipe.
juce::String presetTooltip(int index) {
    return juce::String::fromUTF8(spectralforge::selectablePresetSummary(index))+"\n\nDetails: "
         +juce::String::fromUTF8(spectralforge::selectablePresetDescription(index));
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
void fillPresetMenu(juce::ComboBox& presets)
{
    for(size_t groupIndex=0;groupIndex<spectralforge::presetCategoryOrder.size();++groupIndex) {
        if(groupIndex==0)presets.getRootMenu()->addSectionHeader("SIGNATURE PRESETS");
        if(groupIndex==2){presets.getRootMenu()->addSeparator();presets.getRootMenu()->addSectionHeader("FACTORY PRESETS");}
        const auto category=juce::String::fromUTF8(spectralforge::presetCategoryOrder[groupIndex]);juce::PopupMenu group;
        for(const int index:spectralforge::presetDisplayOrder())if(spectralforge::selectablePresetCategory(index)==category) {
            const auto* name=spectralforge::selectablePresetName(index);
            group.addItem(index+1,juce::String::fromUTF8(name));
        }
        if(group.getNumItems()>0)presets.getRootMenu()->addSubMenu(groupIndex<2?category.fromFirstOccurrenceOf(" / ",false,false):category,group);
    }
}
// Factory-preset CUSTOM detection compares the sound, not the tuner: tuning
// (or opening the tuner) must not turn a preset into CUSTOM.
bool comparedForPreset(const juce::AudioProcessorParameter& parameter)
{
    const auto* withID=dynamic_cast<const juce::AudioProcessorParameterWithID*>(&parameter);
    return withID==nullptr || !withID->paramID.startsWithIgnoreCase("tuner");
}
std::vector<float> presetSnapshot(juce::AudioProcessor& processor)
{
    std::vector<float> values;
    for(auto* parameter:processor.getParameters())values.push_back(comparedForPreset(*parameter) ? parameter->getValue() : std::numeric_limits<float>::quiet_NaN());
    return values;
}
bool presetEdited(juce::AudioProcessor& processor,const std::vector<float>& values)
{
    const auto& params=processor.getParameters();
    if(values.size()!=size_t(params.size()))return true;
    for(int i=0;i<params.size();++i)if(!std::isnan(values[(size_t)i]) && std::abs(params[i]->getValue()-values[(size_t)i])>1e-6f)return true;
    return false;
}
void fitCabinetWindow(juce::Component::SafePointer<juce::DialogWindow> window,int remainingChecks=40)
{
    if(window==nullptr || !window->isShowing())return;
    if(auto* peer=window->getPeer();peer!=nullptr && peer->getFrameSizeIfPresent()) {
        // LaunchOptions centres the client before enabling the native title
        // bar. Once the OS reports that frame, fit the complete window into
        // the selected display. JUCE supplies the frame and coordinate/DPI
        // conversions; the CAB workspace already fits its content uniformly.
        struct WholeWindowConstrainer final : juce::ComponentBoundsConstrainer {
            void checkBounds(juce::Rectangle<int>& bounds,const juce::Rectangle<int>&,
                             const juce::Rectangle<int>& limits,bool,bool,bool,bool) override {
                bounds=bounds.constrainedWithin(limits);
            }
        } constrainer;
        constrainer.setBoundsForComponent(window.getComponent(),window->getBounds(),false,false,false,false);
        return;
    }
    // Some window managers report decorations asynchronously. This is a
    // bounded creation-time correction, not a constraint on later user drags.
    if(remainingChecks>0)juce::Timer::callAfterDelay(25,[window,remainingChecks] {
        fitCabinetWindow(window,remainingChecks-1);
    });
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
juce::Font ChimeraLookAndFeel::getTextButtonFont(juce::TextButton& b,int) { return juce::FontOptions((bool)b.getProperties()["utilityDock"]?13.f:11.5f,juce::Font::bold); }
juce::Font ChimeraLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::FontOptions(14.f); }
void ChimeraLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool hover,bool down)
{
    auto area=button.getLocalBounds().toFloat().reduced(.5f);
    if((bool)button.getProperties()["pedalPower"]) {
        auto colour=button.findColour(button.getToggleState()?juce::TextButton::buttonOnColourId:juce::TextButton::buttonColourId);
        if(!button.isEnabled())colour=colour.withMultipliedAlpha(.45f);else if(hover || down)colour=colour.brighter(.10f);
        g.setColour(colour);g.fillRoundedRectangle(area,3.f);g.setColour(button.hasKeyboardFocus(true)?accent:colour.brighter(.25f));g.drawRoundedRectangle(area,3.f,1.f);return;
    }
    if((bool)button.getProperties()["footswitch"]) {
        const float x=area.getCentreX(),y=area.getCentreY()-7;
        g.setColour(juce::Colours::black.withAlpha(.7f));g.fillEllipse(x-20,y-18,40,40);
        g.setGradientFill({juce::Colour(0xffcfcbc0),x-18,y-18,juce::Colour(0xff525655),x+16,y+18,false});g.fillEllipse(x-18,y-18,36,36);
        g.setColour(juce::Colour(0xff303534));g.drawEllipse(x-15,y-15,30,30,2);
        g.setGradientFill({juce::Colour(0xffe2dfd4),x-10,y-12,juce::Colour(0xff878a80),x+10,y+11,false});g.fillEllipse(x-11,y-11+(down?2:0),22,22);return;
    }
    if((bool)button.getProperties()["utilityDock"]){
        const bool active=button.getToggleState()||(bool)button.getProperties()["utilityActive"];
        g.setColour(active?juce::Colour(0xff9fd4b2):juce::Colour(hover||down?0xff476657:0xff2d443b));g.fillRoundedRectangle(area,5);
        g.setColour(button.hasKeyboardFocus(true)?juce::Colours::white:juce::Colour(0xff93b4a1));g.drawRoundedRectangle(area,5,1.5f);return;
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
ChimeraEditor::ChimeraEditor(ChimeraProcessor& p) : AudioProcessorEditor(&p),processor(p),boardPanel(p)
{
    setLookAndFeel(&look); canvas.setComponentID("surface");
    canvas.draw=[this](juce::Graphics& g){paintCanvas(g);};
    editorViewport.setComponentID("editorViewport");
    editorViewport.setName("Scrollable editor");
    addAndMakeVisible(editorViewport);
    editorViewport.setViewedComponent(&scrollContent,false);
    scrollContent.addAndMakeVisible(canvas);
    auto add=[this](juce::Component& component){canvas.addAndMakeVisible(component);};
    add(callbackLoad);callbackLoad.setComponentID("callbackLoad");callbackLoad.setName("Audio callback deadline usage");
    callbackLoad.setFont(juce::FontOptions(11.f));
    callbackLoad.setTooltip("Audio callback wall time divided by the current block duration. 100% uses the entire processing deadline; a peak above 100% risks an audio dropout. This is not whole-computer CPU usage. PEAK decays over processed audio time and retains its reading when processing stops.");
    signalPath=std::make_unique<SignalPathView>(processor,true);add(*signalPath);
    signalPath->navigate=[this](const auto& node){navigateSignalPath(node);};
    signalPath->expand=[this]{openSignalPath();};
    precisionTuner=std::make_unique<PrecisionTunerPanel>(processor);add(*precisionTuner);
    add(tunerOpen);tunerOpen.setComponentID("tunerOpen");tunerOpen.setName("Open precision tuner");
    tunerOpen.setTooltip("Open precision tuning, strobe, temperament and per-string targets. Opening starts tuning; closing stops tuning. Output mute follows your selected preference.");
    tunerOpen.onClick=[this]{tunerViewOpen=!tunerViewOpen;auto* parameter=processor.parameters().getParameter("tuneron");parameter->beginChangeGesture();parameter->setValueNotifyingHost(tunerViewOpen?1.f:0.f);parameter->endChangeGesture();updateModeUI();};
    canvas.addChildComponent(inputGuide);inputGuide.refresh();
    add(easyOpen);easyOpen.setComponentID("easyOpen");easyOpen.setName("Open Easy view");
    easyOpen.setTooltip("Switch to Easy view: presets, six main knobs, Auto Set and tuner. Your tone settings stay as they are; MATCH switches off.");
    easyOpen.onClick=[this]{setEasyMode(true,true);};
    add(boardPanel);add(preEngineStatus);
    preEngineStatus.setComponentID("preEngineStatus");
    preEngineStatus.setFont(juce::FontOptions(11.f));preEngineStatus.setColour(juce::Label::textColourId,ink);
    title.setText("SpectralForge Chimera",juce::dontSendNotification);title.setVisible(false);
    const auto loadBrand=[](const char* name){int size=0;const auto* bytes=ChimeraArtworkData::getNamedResource(name,size);auto image=bytes ? juce::ImageFileFormat::loadFrom(bytes,(size_t)size) : juce::Image{};
        if(image.isValid() && image.hasAlphaChannel()){juce::Rectangle<int> bounds;const juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readOnly);for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x)if(pixels.getPixelColour(x,y).getAlpha()>10)bounds=bounds.getUnion({x,y,1,1});if(!bounds.isEmpty())image=image.getClippedImage(bounds);}return image;};
    wordmark=loadBrand("chimerawordmark_png");brandEmblem=loadBrand("spectralforgeemblem_png");
    mode.setName("Routing Mode"); mode.addItemList({"CLASSIC","DUAL","MATRIX"},1); add(mode);
    mode.setTooltip("Classic: one full-range rig. Dual: two parallel rigs. Matrix: three input bands.");
    quality.setName("Oversampling"); quality.addItemList({"1x","2x","4x","8x"},1); add(quality);
    quality.setTooltip("Anti-aliasing for amplifier distortion. Higher factors use more CPU. Host latency stays constant.");
    scale.setName("Interface size"); scale.addItemList({"75%","100%","125%","150%"},1); scale.addSeparator(); scale.addItem("Fit screen",fitScaleItem); scale.setSelectedId(2,juce::dontSendNotification); add(scale);
    scale.setTooltip(processor.wrapperType==juce::AudioProcessor::wrapperType_Standalone
                     ? "Interface size. Fit screen uses the largest readable size for this display; the window's maximise button or F11 toggles the same fit."
                     : "Interface size. Fit screen uses the largest readable size for this display, when the host allows plug-in resizing.");
    scale.onChange=[this]{
        const int id=scale.getSelectedId();
        if(id==fitScaleItem){fitToDisplay();return;}
        if(id<1 || id>4)return;
        const float factor=float(id+2)*.25f;
        auto size=spectralforge::ui::editorSizeForScale(factor);
        if(const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
            size=spectralforge::ui::boundedEditorSize(size,display->userArea,spectralforge::ui::editorWindowChrome(*this));
        setViewSize(size,factor);};
    add(info); info.setButtonText("SETTINGS"); info.onClick=[this]{
        juce::PopupMenu menu;
        menu.addSectionHeader(juce::String("BETA / ")+spectralforge::release::version+" / "+CHIMERA_BUILD_REVISION);
        menu.addSectionHeader(juce::String("Expiry UTC: ")+(CHIMERA_BETA_EXPIRY_UTC[0] ? CHIMERA_BETA_EXPIRY_UTC : "approval pending"));
        if(CHIMERA_BETA_EXPIRY_KST[0])menu.addSectionHeader(juce::String("Expiry KST: ")+CHIMERA_BETA_EXPIRY_KST);
        // Same single evaluation as the Beta / Updates notice; never a UI-owned clock.
        menu.addSectionHeader(juce::String(spectralforge::beta::remainingText(processor.betaStatus())));
        menu.addItem(6,"Beta / Updates / Manual / Bug report...");menu.addItem(7,"Review / copy / save diagnostics...");menu.addSeparator();menu.addItem(1,"Save reference...");menu.addItem(2,"Load reference...");menu.addSeparator();menu.addItem(3,"About / IR credits");menu.addSeparator();menu.addItem(4,"Reset tuner A4 to 440 Hz");menu.addItem(5,"Clear MIDI assignments");
        menu.addSeparator();menu.addItem(8,processor.testerCodeHolder().isEmpty() ? juce::String("Enter beta tester code...")
                                                                              : "Beta tester code: "+processor.testerCodeHolder()+" (no expiry)...");
        menu.addSeparator();
        const auto& match=processor.abLevelMatch();
        menu.addItem(9,match.captureLocked() ? "Capture safety: MATCH locked off by OBS" : "Capture safety: lock MATCH off",
                     !match.captureLocked(),match.captureSafe());
        const juce::Component::SafePointer<ChimeraEditor> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&info),[safe](int result){if(!safe)return;if(result==9){auto& match=safe->processor.abLevelMatch();match.setCaptureSafe(!match.captureSafe());safe->timerCallback();}else if(result==8)safe->showTesterCode();else if(result==7)safe->showDiagnostics();else if(result==6)safe->showSupport();else if(result==1)safe->referenceFile(true);else if(result==2)safe->referenceFile(false);else if(result==3)safe->showInfo();else if(result==4){auto* p=safe->processor.parameters().getParameter("tunerref");p->setValueNotifyingHost(p->convertTo0to1(440));}else if(result==5)safe->processor.clearMidi();});
    };
    info.setTooltip(processor.betaNotice());
    for(auto* button:{&compareA,&compareB,&copyAB,&matchAB,&irLibraryButton}) add(*button);
    matchAB.setComponentID("ab.match");
    matchAB.onClick=[this]{
        auto& match=processor.abLevelMatch();
        if(match.enabled()){match.setEnabled(false);timerCallback();return;}
        if(match.outputProtected())return;
        if(matchWarningAcknowledged){match.setEnabled(true);timerCallback();return;}
        const juce::Component::SafePointer<ChimeraEditor> safe(this);
        matchWarning=juce::AlertWindow::showScopedAsync(juce::MessageBoxOptions{}
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("MATCH changes the output")
            .withMessage("MATCH lowers the louder A/B sound at the plugin output. Turn it off before recording, exporting, freezing or OBS capture. "
                         "Host-reported recording and offline rendering switch it off automatically. Realtime exports and unreported recording cannot be detected. Use SETTINGS > Capture safety to lock MATCH off for external capture.")
            .withButton("Enable MATCH").withButton("Cancel").withAssociatedComponent(this),
            [safe](int result){if(safe && result==1){safe->matchWarningAcknowledged=true;safe->processor.abLevelMatch().setEnabled(true);safe->timerCallback();}});
    };
    matchAB.setTooltip("Level-match A/B: while you play, measure both sounds and turn the louder one down to the quieter one, so you compare tone, not volume. "
                       "Changes plugin output; not a separate monitor bus. Turn off before printing (50 ms release). Host-reported recording/offline render disarms it immediately; "
                       "External OBS capture and realtime exports cannot be detected. SETTINGS > Capture safety locks MATCH off. The updated Chimera OBS add-on locks it off automatically. SAFE means output protection, not detected recording. Not saved; off after project load or audio-device restart.");
    compareA.onClick=[this]{processor.selectComparison(0);markPresetCustom();presetOrigin=0;timerCallback();};compareB.onClick=[this]{processor.selectComparison(1);markPresetCustom();presetOrigin=0;timerCallback();};copyAB.onClick=[this]{processor.copyComparison();};
    compareA.setTooltip("Recall sound snapshot A: all parameters, models and embedded IRs. Both stereo channels are processed together.");compareB.setTooltip("Recall sound snapshot B. This is a stored sound, not the right audio channel.");
    copyAB.setTooltip("Copy the active sound to the other slot. A/B stores all parameters and IR audio; both slots are saved with the project/reference file.");
    irLibraryButton.setComponentID("irlibrary");irLibraryButton.onClick=[this]{loadIR(0);};
    preOrder.setComponentID("preorder");preOrder.setName("Pedal order");preOrder.addItemList({"SUSTAIN / COMP > ENV","TOUCH / ENV > COMP"},1);add(preOrder);preOrderAttachment=std::make_unique<CA>(p.parameters(),"preorder",preOrder);preOrder.onChange=[this]{updateModeUI();};
    preOrder.setTooltip("TOUCH lets the envelope follow your picking before compression. SUSTAIN compresses first for a steadier sweep. Fuzz, boost and drive remain after the shared clean tap.");
    gainOrder.setComponentID("gainorder");gainOrder.setName("Gain pedal order");gainOrder.addItemList({"FUZZ > BOOST > DRIVE","FUZZ > DRIVE > BOOST"},1);add(gainOrder);gainOrderAttachment=std::make_unique<CA>(p.parameters(),"gainorder",gainOrder);gainOrder.onChange=[this]{updateModeUI();};
    gainOrder.setTooltip("Boost before drive adds drive saturation. Boost after drive raises its output; a saturated amp may still add distortion instead of loudness. Fuzz stays first; Matrix LOW DI is unaffected.");
    for(int e=0;e<2;++e){
        eqPanels[(size_t)e]=std::make_unique<spectralforge::GraphicalEQPanel>(p.parameters(),p.graphicalEQ(e),e,p.eqEditing(),p.eqCaptureBoundary());
        eqPanels[(size_t)e]->onHistoryBankSelected=[this](int bank){
            const auto route=spectralforge::signalPath::read(processor);
            navigateSignalPath(*route.find(bank==0?"tone.eq":"final.eq"));
        };
        add(*eqPanels[(size_t)e]);
    }
    dynamicsPanel=std::make_unique<spectralforge::CommonDynamicsPanel>(p.parameters(),p.commonDynamics(),p.dynamicsAnalysis(),p.eqCaptureBoundary());
    canvas.addAndMakeVisible(*dynamicsPanel);
    // Dynamic EQ is edited on its band: open the EQ that holds the first dynamic band.
    dynamicsPanel->onDynamicEQRequested=[this] {
        int bank=0,band=-1;
        for(int slot=0;slot<4 && band<0;++slot)if(const auto* target=processor.parameters().getRawParameterValue(spectralforge::dynamicSlotID(slot,"target"))) {
            const int value=juce::roundToInt(target->load());if(value>=1 && value<=24){bank=(value-1)/12;band=(value-1)%12;}
        }
        const auto route=spectralforge::signalPath::read(processor);
        navigateSignalPath(*route.find(bank==0?"tone.eq":"final.eq"));
        if(band>=0)eqPanels[size_t(bank)]->selectBandForEditing(band);
    };
    dynamicsPanel->setGateUnitMeterProvider([this](int unit){return -juce::Decibels::gainToDecibels(processor.gateUnitMeter(unit),-120.f);});
    dynamicsPanel->setMeterProvider([this](spectralforge::DynamicsStage stage) {
        using Stage=spectralforge::DynamicsStage;
        switch(stage) {
            case Stage::gate:return -juce::Decibels::gainToDecibels(processor.gateMeter(),-120.f);
            case Stage::compressor:return processor.commonDynamics().compReduction.load();
            case Stage::limiter:return processor.commonDynamics().limitReduction.load();
            case Stage::deesser:return processor.deEsser().reduction();
            case Stage::preCompressor:return processor.preCompressorReduction();
            case Stage::postCompressor:return processor.postCompressorReduction();
            case Stage::lowCompressor:return processor.parameters().getRawParameterValue("mode")->load()>1.5f?processor.lowCompMeter():0.f;
            default:return 0.f;
        }
    });
    autoSetController=std::make_unique<spectralforge::autoset::Controller>(p);
    autoSetController->onStatus=[this](const spectralforge::autoset::Controller::Status& status) {
        using Controller=spectralforge::autoset::Controller;using Phase=Controller::Phase;
        // Every listening pass (Tone EQ, multiband and finish included) offers CANCEL.
        publishAutoSetStatus();const bool busy=autoSetController->busy();
        spectralforge::AutoSetReport report;
        report.listening=busy;report.finished=status.phase==Phase::done || status.phase==Phase::failed;
        for(const auto phase:status.passes)report.steps.add(Controller::phaseName(phase));
        report.current=status.pass;report.seconds=status.seconds;report.totalSeconds=status.totalSeconds;
        report.passSeconds=status.passSeconds;report.passTotalSeconds=status.passTotalSeconds;report.levelDb=status.levelDb;
        report.listeningFor=status.listeningFor;report.heard=status.heard;
        if(status.phase==Phase::failed)report.message=status.text;
        for(const auto& step:status.steps)report.rows.push_back({Controller::phaseName(step.phase),step.result,step.changed,int(step.phase)});
        dynamicsPanel->setAutoSetReport(report);
    };
    // SHOW opens the module a finished pass changed.
    dynamicsPanel->onAutoSetShow=[this](int target) {
        using Phase=spectralforge::autoset::Controller::Phase;
        const auto route=spectralforge::signalPath::read(processor);
        switch(Phase(target)) {
            case Phase::input:case Phase::gates:navigateSignalPath(*route.find("gate"));break;
            case Phase::tone:navigateSignalPath(*route.find("tone.eq"));break;
            case Phase::multiband:navigateSignalPath(*route.find("common.multiband"));break;
            case Phase::finish:navigateSignalPath(*route.find("common.deesser"));break;
            default:navigateSignalPath(*route.find("common.comp"));break;
        }
    };
    dynamicsPanel->onAutoSet=[this]{autoSetController->start();};
    dynamicsPanel->onAutoCancel=[this]{autoSetController->cancel();};
    dynamicsPanel->onAutoUndo=[this]{autoSetController->undo();};
    static_assert(spectralforge::CommonDynamicsPanel::autoLockNames.size()==spectralforge::autoset::Controller::lockOrder.size(),"Auto Set lock menu and controller locks differ");
    {const auto& o=autoSetController->currentOptions();dynamicsPanel->setAutoSetOptions({o.instrument,o.tone,int(o.style),o.finish,o.multiband,o.gates,juce::roundToInt(o.strength*100),o.locks});}
    dynamicsPanel->onAutoOptionsChanged=[this](const spectralforge::CommonDynamicsPanel::AutoSetOptions& o) {
        autoSetController->setOptions({o.instrument,o.tone,spectralforge::autoset::Style(std::clamp(o.style,0,2)),o.finish,o.multiband,o.gates,float(o.strength)/100.f,o.locks});
        if(easyView)easyView->setAutoSetOptions(o.instrument,o.style,o.tone,o.finish,o.multiband);
    };
    dynamicsPanel->setMultibandProviders([this]{auto& processorMB=processor.multibandDynamics();return processorMB.enabled()?processorMB.appliedCrossovers():processorMB.crossovers();},[this](int band){return processor.multibandDynamics().reduction(band);});
    dynamicsPanel->onCompressorSourceRequested=[this](spectralforge::CommonDynamicsPanel::Page source) {
        using Page=spectralforge::CommonDynamicsPanel::Page;
        if(source==Page::preCompressor && processor.parameters().getRawParameterValue("boardEnabled")->load()>.5f) {
            page=1;tunerViewOpen=false;dynamicsViewOpen=false;pathSelections[size_t(juce::jlimit(0,2,lastMode))]="pre";updateModeUI();boardPanel.openCompressorDetails();
            return true;
        } else if(source==Page::postCompressor && processor.parameters().getRawParameterValue(spectralforge::postNativeModeID(0))->load()>.5f) {
            // The native detail window owns the analyzer while open. Its live
            // controls stay attached to the actual current POST model bank.
            if(postPanels[0])postPanels[0]->openDetails();
            return true;
        }
        return false;
    };
    dynamicsPanel->onPageSelected=[this](spectralforge::CommonDynamicsPanel::Page selected) {
        using Page=spectralforge::CommonDynamicsPanel::Page;
        const auto* expanderAfter=processor.parameters().getRawParameterValue("expAfterRig");
        const juce::String id=selected==Page::expander?(expanderAfter && expanderAfter->load()>.5f?"gate.extra.post":"gate.extra.pre"):selected==Page::gate?(dynamicsPanel->mainGateShown()?"gate":dynamicsPanel->selectedGateSide()==1?"gate.extra.post":"gate.extra.pre"):selected==Page::limiter?"output.limiter":selected==Page::multiband?"common.multiband":selected==Page::deesser?"common.deesser":selected==Page::preCompressor?"pre.legacy3":selected==Page::postCompressor?"post.section0":selected==Page::lowCompressor?"low.comp":"common.comp";
        updateDynamicsEntries();
        pathSelections[size_t(juce::jlimit(0,2,lastMode))]=id;signalPath->select(id);
        if(pathDialog)if(auto* graph=dynamic_cast<SignalPathView*>(spectralforge::ui::dialogContent(*pathDialog)))graph->select(id);
    };
    constexpr std::array<const char*,8> dynamicsTitles{"GATE","EXPANDER","COMPRESSOR","LIMITER","MULTIBAND","DE-ESSER","SATURATION","FILTER"};
    constexpr std::array<const char*,8> dynamicsNames{"gate","expander","compressor","limiter","multiband","deesser","saturation","filter"};
    constexpr std::array<const char*,8> dynamicsHelp{
        "Open the gates: separate PRE RIG and POST RIG gates that can run together.",
        "Open the downward expander. It runs together with the gates, before or after the rig.",
        "Open the common compressor after Tone EQ. PRE, POST and Matrix LOW retain their own controls.",
        "Open the final-output sample-peak limiter.",
        "Open four-band dynamics and crossover controls.",
        "Open high-band de-esser controls.",
        "Open three-band saturation: per-band type and drive, Live or Studio 4x.",
        "Open the two-filter processor: LP/HP/BP/notch with LFO and envelope modulation."};
    for(size_t i=0;i<dynamicsEntries.size();++i) {
        auto& entry=dynamicsEntries[i];add(entry);
        entry.setButtonText(dynamicsTitles[i]);
        entry.setComponentID("commonDynamics.open."+juce::String(dynamicsNames[i]));
        entry.setTooltip(dynamicsHelp[i]);
        entry.onClick=[this,i]{
            page=3;tunerViewOpen=false;dynamicsViewOpen=true;
            dynamicsPanel->selectPage(dynamicsEntryPages[i]);updateModeUI();
        };
    }
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
        effect.model.setName(juce::String(spectralforge::modelFamilies[i].category)+" model");effect.model.setComponentID(spectralforge::modelFamilies[i].parameter);const auto menuNames=spectralforge::effectFamilyMenuNames((int)i);
        if(i==1 || i==2 || i==10)spectralforge::buildExtendedEffectMenu(*effect.model.getRootMenu(),(int)i);
        else {effect.model.getRootMenu()->addSectionHeader(effectHeaders[i]);effect.model.addItemList(menuNames,1);}add(effect.model);
        if(spectralforge::effectFamilyExtended((int)i))effect.modelSync=std::make_unique<spectralforge::ExtendedEffectModelSync>(p.parameters(),(int)i,effect.model);
        else effect.modelAttachment=std::make_unique<CA>(p.parameters(),spectralforge::modelFamilies[i].parameter,effect.model);
        effect.model.onChange=[this]{stateDirty=true;};
        const bool pedal=i==0 || (i>=3 && i<=6);effect.enabled.getProperties().set("footswitch",pedal);
        for(size_t k=0;k<5;++k) {
            if(!effectIds[i][k]) continue;
            style(effect.labels[k],pedal ? 10.5f : 10.f,juce::Justification::centred);effect.labels[k].setText(effectLabels[i][k],juce::dontSendNotification);
            setupSlider(effect.controls[k],effectLabels[i][k],effectSuffix[i][k]);
            effect.controls[k].setComponentID(effectIds[i][k]);
            spectralforge::ui::configureRotaryKnob(effect.controls[k]);
            effect.controls[k].setTextBoxStyle(pedal ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,false,pedal ? 88 : 68,pedal ? 23 : 22);
            add(effect.labels[k]);add(effect.controls[k]);effect.attachments[k]=std::make_unique<SA>(p.parameters(),effectIds[i][k],effect.controls[k]);
        }
    }
    effects[1].controls[0].textFromValueFunction=[this](double value){return juce::String(delaySync.getToggleState() ? 60000.0/processor.currentTempo() : value,1);};
    inputMode.setName("Input mode");inputMode.addItemList({"STEREO","MONO L"},1);add(inputMode);inputModeAttachment=std::make_unique<CA>(p.parameters(),"inputmode",inputMode);
    inputMode.setTooltip("MONO L sends the left input to both channels. Stereo preserves separate channels.");
    presets.setName("Preset");fillPresetMenu(presets);
    presets.setText("INIT / CUSTOM",juce::dontSendNotification);add(presets);
    presets.onChange=[this]{if(presets.getSelectedId()>0) {processor.loadFactoryPreset(presets.getSelectedId()-1);presetOrigin=presets.getSelectedId();presetValues=presetSnapshot(processor);presets.setTooltip(presetTooltip(presets.getSelectedId()-1));updateModeUI();if(easyView)easyView->refreshNow();}};
    for(auto* button:{&doublerOn,&midi,&tap,&hostTempo,&metronome,&presetPrevious,&presetNext,&presetSave,&presetLoad,&delaySync})add(*button);
    presetPrevious.onClick=[this]{presets.setSelectedId(spectralforge::adjacentPreset(presets.getSelectedId()-1,-1)+1);};presetNext.onClick=[this]{presets.setSelectedId(spectralforge::adjacentPreset(presets.getSelectedId()-1,1)+1);};
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
    const std::array<const char*,8> globalIds{"input","gatethreshold","gaterelease","gatehold","transpose","tunerref","output","gateRangeDb"};
    const std::array<const char*,8> globalNames{"Input gain","Gate threshold","Gate release","Gate hold","Transpose semitones","Tuner reference","Output gain","Gate range"};
    const std::array<const char*,8> suffix{" dB"," dB"," ms"," ms"," st"," Hz"," dB",""};
    for(size_t i=0;i<globalSliders.size();++i)
    {
        setupSlider(globalSliders[i],globalNames[i],suffix[i]); add(globalSliders[i]);
        globalAttachments[i]=std::make_unique<SA>(state,globalIds[i],globalSliders[i]);
    }
    globalSliders[7].setComponentID("gateRangeDb");
    globalSliders[7].setTooltip("Closed-gate attenuation: 0 dB = unity; finite dB = limited reduction; Full = release toward silence (legacy default). Applies at the selected PRE or POST GATE position.");
    for(const auto i:{0,4,6}) {spectralforge::ui::configureRotaryKnob(globalSliders[(size_t)i]);globalSliders[(size_t)i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,104,22);globalSliders[(size_t)i].getProperties().set("knobStyle",3);}
    globalSliders[4].setNumDecimalPlacesToDisplay(0); globalSliders[5].setNumDecimalPlacesToDisplay(0);
    globalSliders[2].setNumDecimalPlacesToDisplay(0); globalSliders[3].setNumDecimalPlacesToDisplay(0);
    for(const auto index:{1,2,3,7})globalSliders[size_t(index)].setVisible(false);
    // The top-bar dynamics grid opens each existing editor directly, including Gate / Expander.
    const std::array<juce::TextButton*,3> globalToggles{&pitchOn,&tunerOn,&tunerMute};
    const std::array<const char*,3> ids{"transposeon","tuneron","tunermute"};
    for(size_t i=0;i<globalToggles.size();++i) { globalToggles[i]->setClickingTogglesState(true); add(*globalToggles[i]); globalButtons[i]=std::make_unique<BA>(state,ids[i],*globalToggles[i]); }
    tunerOn.setTooltip("Tune the original input before the gate and transposer. Play one note at a time. Range: 16-2000 Hz.");
    tunerMute.setTooltip("Silence the output while the tuner is enabled.");
    pitchOn.setTooltip("Polyphonic transpose, -12 to +12 semitones. The added latency is shown below and reported to the host.");
    transposeMode.setName("Transpose mode"); transposeMode.addItemList({"CLASSIC","LOW-LAT","TIGHT"},1); add(transposeMode);
    // Not a host parameter (so it stays out of the frozen parameter inventory):
    // a persisted ValueTree setting driven directly here.
    transposeMode.setSelectedId(juce::jlimit(0,2,processor.transposeQuality())+1,juce::dontSendNotification);
    transposeMode.onChange=[this]{processor.setTransposeQuality(transposeMode.getSelectedId()-1);};
    transposeMode.setTooltip("Transpose quality vs latency. CLASSIC keeps full low-register quality (~43 ms). LOW-LAT halves the FFT window (~21 ms). TIGHT quarters it (~11 ms) for a snappy lead feel; low notes lose body. The active latency is reported to the host.");
    style(pitchStatus,11.f); add(pitchStatus);
    const std::array<juce::String,8> names{"DRIVE","LEVEL","BASS","LOW MID","HIGH MID","TREBLE","PRESENCE","RESONANCE"};
    const std::array<const char*,8> parameterIds{"drive","level","bass","lowmid","highmid","treble","presence","resonance"};
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i]; const auto n=juce::String(i+1);
        style(lane.ampReference,10.5f);lane.ampReference.setColour(juce::Label::textColourId,muted);lane.ampReference.setComponentID("ampreference"+n);add(lane.ampReference);
        style(lane.header,15.f); style(lane.range,12.f); style(lane.tonePivot,11.f); style(lane.cabStatus,11.f);
        lane.range.setColour(juce::Label::textColourId,muted); lane.cabStatus.setColour(juce::Label::textColourId,muted);
        add(lane.header); add(lane.range); add(lane.tonePivot); add(lane.cabStatus);
        lane.amp.setName("Amp "+n);lane.amp.setComponentID("ampSelect"+n);add(lane.amp);
        lane.amp.resetSyncExplicit(processor.selectedAmpModel(i)+1);
        lane.amp.onChange=[this,i]{auto& control=lanes[(size_t)i].amp;control.acceptSelection();processor.setAmpModel(i,control.getSelectedId()-1);stateDirty=true;};
        lane.nativePanel=std::make_unique<AmpNativePanel>(processor,i);add(*lane.nativePanel);
        lane.head=std::make_unique<AmpHeadControlView>(processor,i,false);add(*lane.head);lane.head->onOpen=[this,i]{openAmpHead(i);};
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
            else {auto* model=processor.parameters().getParameter(spectralforge::originalCabID(i,"Aon"));model->beginChangeGesture();model->setValueNotifyingHost(0);model->endChangeGesture();auto* p=processor.parameters().getParameter("cabtype"+n);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(source)));p->endChangeGesture();}
            timerCallback();
        };
        lane.cabType.browse=[this,i]{loadIR(i);};lane.cabType.exportCabinet=[this,i]{exportCabinet(i);};lane.cabType.refresh();
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
        add(lane.load); lane.load.setButtonText("PANEL");lane.load.onClick=[this,i]{openCabWorkspace(i);};
        lane.load.setTooltip("Open the cabinet room, large cabinet/microphone editor and separate IR Loader.");
    }
    for(int i=0;i<3;++i){postPanels[(size_t)i]=std::make_unique<PostNativePanel>(processor,i);add(*postPanels[(size_t)i]);}
    creativeStrip=std::make_unique<spectralforge::creativeUI::CreativeStrip>(processor);add(*creativeStrip);creativeStrip->onOpen=[this]{openCreativePanel();};
    synthPage=std::make_unique<spectralforge::synthUI::SynthPage>(processor);add(*synthPage);
    for(int position=3;position<6;++position){const int family=rackOrder[(size_t)position];auto& button=effects[(size_t)family].expand;add(button);button.setComponentID("postExpand"+juce::String(position));button.setTooltip("Open all rack controls");button.onClick=[this,family]{auto* full=new RackEffectDetailPanel(processor,family);full->setLookAndFeel(&look);juce::DialogWindow::LaunchOptions options;options.content.setOwned(full);options.dialogTitle=juce::String("CHIMERA / ")+spectralforge::modelFamilies[(size_t)family].category;options.dialogBackgroundColour=panel;options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());};}
    mode.onChange=[this]{updateModeUI();}; x1.onValueChange=[this]{updateBandLabels();}; x2.onValueChange=[this]{updateBandLabels();};
    setResizable(true,true); setResizeLimits(480,320,1770,1170);
    tunerOn.setButtonText("TUNER ACTIVE");
    hostTempo.setButtonText("HOST SYNC");metronome.setButtonText("METRONOME");
    for(auto* button:{&tunerOpen,&midi,&tap,&hostTempo,&metronome,&easyOpen,&info}){button->getProperties().set("utilityDock",true);button->setWantsKeyboardFocus(true);}
    for(const auto* id:{"mode","dualtype","tuneron","tunermute"})processor.parameters().addParameterListener(id,this);
    auto initialSize=juce::Point<int>{1180,780};
    if(const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        initialSize=spectralforge::ui::boundedEditorSize(initialSize,display->userArea,{16,64});
    setSize(initialSize.x,initialSize.y); updateModeUI(); startTimerHz(25);
}
ChimeraEditor::~ChimeraEditor() { matchWarning.close();processor.abLevelMatch().setEnabled(false);for(const auto* id:{"mode","dualtype","tuneron","tunermute"})processor.parameters().removeParameterListener(id,this);cancelPendingUpdate();stopTimer(); chooser.reset(); for(auto& dialog:dialogs)if(dialog)delete dialog.getComponent();dialogs.clear();if(support)support->cancel();support.reset();setLookAndFeel(nullptr); }
void ChimeraEditor::setupSlider(juce::Slider& slider,const juce::String& name,const juce::String& suffix)
{
    slider.setName(name); slider.setTooltip(name); slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setScrollWheelEnabled(false); // Wheel scrolling the editor must not alter audio.
    slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,66,24); slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(name=="DRIVE" ? 2 : 1);
}
void ChimeraEditor::loadIR(int lane)
{
    const juce::Component::SafePointer<ChimeraEditor> safe(this);juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new IRBrowserPanel([safe,lane](juce::File file,int factory){if(!safe)return;
        if(factory) {
            auto& state=safe->processor.parameters();
            auto* model=state.getParameter(spectralforge::originalCabID(lane,"Aon"));
            model->beginChangeGesture();model->setValueNotifyingHost(0);model->endChangeGesture();
            auto* source=state.getParameter("cabtype"+juce::String(lane+1));
            source->beginChangeGesture();source->setValueNotifyingHost(source->convertTo0to1(float(factory)));source->endChangeGesture();
        }
        else safe->processor.loadIR(lane,file);safe->timerCallback();}));
    options.dialogTitle="SpectralForge Chimera / Cabinet library / Rig "+juce::String(lane+1);options.dialogBackgroundColour=background;
    options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
}
void ChimeraEditor::openSignalPath()
{
    if(pathDialog){pathDialog->toFront(true);return;}
    auto* graph=new SignalPathView(processor,false);
    graph->select(pathSelections[size_t(juce::jlimit(0,2,lastMode))]);
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    graph->selection=[safe]{return safe?safe->pathSelections[size_t(juce::jlimit(0,2,int(safe->processor.parameters().getRawParameterValue("mode")->load())))]:juce::String{};};
    graph->navigate=[safe,graph](const auto& node){if(safe){safe->navigateSignalPath(node);graph->select(safe->pathSelections[size_t(juce::jlimit(0,2,safe->lastMode))]);}};
    pathDialog=new SignalPathWindow(graph,this);trackDialog(pathDialog);

}
void ChimeraEditor::navigateSignalPath(spectralforge::signalPath::Node node)
{
    // Own the request: refreshing a view may replace its originating button's callback.
    using Target=spectralforge::signalPath::Target;
    using DynamicsPage=spectralforge::CommonDynamicsPanel::Page;
    const int modeNow=juce::jlimit(0,2,int(processor.parameters().getRawParameterValue("mode")->load()));
    // A stale graph event must never select a lane from another routing mode.
    if(node.lane>=modeNow+1)return;
    if(node.id!="pre" && node.id!="post" && node.id!="head" && node.id!="cab" && node.id!="rigs" && !spectralforge::signalPath::read(processor).find(node.id))return;
    tunerViewOpen=false;dynamicsViewOpen=false;
    updateModeUI(); // Synchronize restored mode before applying presentation focus.
    pathSelections[size_t(modeNow)]=node.id;
    switch(node.target){
        case Target::pre:page=1;break;
        case Target::post:page=2;break;
        case Target::synth:page=4;break;
        case Target::toneEQ:case Target::finalEQ:page=3;selectedEQBank=node.target==Target::toneEQ?0:1;break;
        case Target::dynamics:page=3;dynamicsViewOpen=true;break;
        case Target::gate:page=3;dynamicsViewOpen=true;break;
        case Target::amp:case Target::low:page=0;showRigControls=true;break;
        case Target::cab:page=0;showRigControls=false;break;
        case Target::rigs:page=0;showRigControls=true;break;
        default:break;
    }
    updateModeUI();signalPath->select(node.id);
    const auto focus=[](juce::Component& c){if(c.isShowing()&&c.getWantsKeyboardFocus())c.grabKeyboardFocus();};
    switch(node.target){
        // PRE, POST and Matrix LOW compressors are edited where they sit in the rig.
        case Target::pre:boardPanel.navigateOwner(node.id.startsWith("pre.owner")?node.id.substring(9).getIntValue():-1);break;
        case Target::toneEQ:case Target::finalEQ:eqPanels[node.target==Target::toneEQ?0:1]->focusForNavigation();break;
        case Target::amp:if(node.lane>=0){focus(lanes[size_t(node.lane)].amp);if(auto* button=dynamic_cast<juce::TextButton*>(lanes[size_t(node.lane)].nativePanel->findChildWithID("ampExpand"+juce::String(node.lane+1))))button->onClick();}break;
        case Target::cab:if(node.lane>=0)openCabWorkspace(node.lane,true);break;
        case Target::dynamics:
            dynamicsPanel->selectPage(node.id=="common.multiband"?DynamicsPage::multiband:node.id=="common.deesser"?DynamicsPage::deesser:node.id=="output.limiter"?DynamicsPage::limiter:DynamicsPage::compressor);
            break;
        case Target::gate:{
            const auto& s=processor.parameters();
            const auto v=[&](const char* id){const auto* raw=s.getRawParameterValue(id);return raw && raw->load()>.5f;};
            const bool mainAfter=v("gateAfterRig");bool after=mainAfter,expander=false;
            if(node.id.startsWith("gate.extra.")) {
                // A tap node holds Gate 2 (opposite the main gate) and/or the Expander.
                after=node.id.endsWith(".post");
                const bool gate2Here=mainAfter!=after,expanderHere=v("expAfterRig")==after;
                expander=expanderHere && (!gate2Here || (v("expOn") && !v("gate2On")));
            }
            if(expander)dynamicsPanel->selectPage(DynamicsPage::expander);
            else {dynamicsPanel->selectGateSide(after?1:0);dynamicsPanel->selectPage(DynamicsPage::gate);}
            break;
        }
        case Target::transpose:focus(globalSliders[4]);break;
        case Target::input:focus(globalSliders[0]);break;
        case Target::output:focus(globalSliders[6]);break;
        case Target::tuner:{if(!tunerViewOpen)tunerOpen.onClick();break;}
        case Target::utilities:focus(doublerTime);break;
        case Target::synth:if(auto* c=synthPage->control("synthOn"))focus(*c);break;
        case Target::low:if(auto* button=dynamic_cast<juce::TextButton*>(lanes[0].nativePanel->findChildWithID("ampExpand1")))button->onClick();break;
        case Target::post:
            if(node.section>=0&&node.section<3){focus(*postPanels[size_t(node.section)]);}
            else if(node.section==spectralforge::signalPath::creativeSection){focus(creativeStrip->modelBox());}
            else if(node.section>=3 && node.section<6){focus(effects[size_t(rackOrder[size_t(node.section)])].model);}
            break;
        default:break;
    }
}
void ChimeraEditor::openAmpHead(int lane)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new AmpHeadWorkspace(processor,lane));
    options.dialogTitle="Chimera / Amplifier";options.dialogBackgroundColour=background;
    options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;
    options.componentToCentreAround=this;auto* window=options.launchAsync();trackDialog(window);fitCabinetWindow(window);
}
void ChimeraEditor::openCabWorkspace(int lane,bool focusRequested)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new CabWorkspace(processor,lane,focusRequested));
    options.dialogTitle="Chimera / Cabinet Room";options.dialogBackgroundColour=background;
    options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;
    options.componentToCentreAround=this;
    auto* window=options.launchAsync();trackDialog(window);fitCabinetWindow(window);
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
    if(!isInterestedInFileDrag(files)) return;
    if(easyMode) {
        // Easy view has its own layout: only a cabinet shown in its room
        // takes the IR, never the knobs or preset area around it.
        if(!easyView) return;
        const int lane=easyView->laneAt(easyView->getLocalPoint(this,juce::Point<int>{x,y}));
        if(lane>=0) { processor.loadIR(lane,juce::File(files[0])); timerCallback(); }
        return;
    }
    if(page!=0) return;
    const auto point=canvas.getLocalPoint(this,juce::Point<int>{x,y});x=point.x;y=point.y;
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
        juce::String("SpectralForge Chimera | ")+spectralforge::release::displayVersion+"\nRavenForge Luthier Intelligence\nBuild "+CHIMERA_BUILD_REVISION+"\n\nAmp voices are algorithmic interpretations, not verified hardware replicas.\n\nFactory IRs: jesterdyne, CC BY 4.0\nEngl Celestion V30 SM57 center-01.wav\nhttps://freesound.org/s/116735/\nJensen Cab SM57 center.wav\nhttps://freesound.org/s/116743/\nhttps://creativecommons.org/licenses/by/4.0/\nFiles unchanged; normalised and resampled during playback.\n\nPitch: Chimera STFT / JUCE FFT\n\nComplete notices are supplied with the download.\n\n"+copyrightNotice,"OK",this);
}
void ChimeraEditor::timerCallback()
{
#if CHIMERA_ROUTING_DIAGNOSTICS
    ++routeProbe.ticks; routeProbe.entered=juce::Time::getMillisecondCounterHiRes();
    routeProbe.visible=spectralforge::ui::visible(*this);
    routeProbe.messageThread=juce::MessageManager::getInstance()->isThisTheMessageThread();
    if(!routeProbe.visible)++routeProbe.skipped;
#endif
    if(++betaUiTicks>=25) {
        betaUiTicks=0;const auto status=processor.betaStatus().status;
        using Status=spectralforge::beta::Status;
        const bool warning=status==Status::warning || status==Status::expired || status==Status::clockUntrusted || status==Status::configurationError;
        info.setButtonText(warning ? "SETTINGS !" : "SETTINGS");info.setTooltip(processor.betaNotice());
    }
    // Detached ALL windows remain live when their owning tab is hidden.
    for(auto& lane:lanes){lane.nativePanel->refreshIfNeeded();if(lane.head->isShowing())lane.head->refresh();}
    for(auto& post:postPanels)post->refreshIfNeeded();
    if(!spectralforge::ui::visible(*this)){stateDirty=true;return;}
    if(easyMode && easyView)easyView->showPreset(presets.getSelectedId(),presets.getText(),presetOrigin);

    // The large factory-preset comparison is not a meter. Bound it to 5 Hz.
    if(++presetCheckTick%5==0 && !presetValues.empty() && presetEdited(processor,presetValues))markPresetCustom();
    const int current=(int)processor.parameters().getRawParameterValue("mode")->load();
    if(lastBoardEnabled!=(processor.parameters().getRawParameterValue("boardEnabled")->load()>.5f) || lastBoostAfterDrive!=(gainOrder.getSelectedId()==2) || lastEnvelopeFirst!=(preOrder.getSelectedId()==2) || current!=lastMode || lastDualCross!=(processor.parameters().getRawParameterValue("dualtype")->load()>.5f) || lastTuner!=(processor.parameters().getRawParameterValue("tuneron")->load()>.5f)) updateModeUI();
    signalPath->refresh();signalPath->select(pathSelections[size_t(current)]);
#if CHIMERA_ROUTING_DIAGNOSTICS
    ++routeProbe.refreshed; routeProbe.finished=juce::Time::getMillisecondCounterHiRes();
#endif
    refreshVisibleState();
    refreshMeters();
}
void ChimeraEditor::repaintDesign(juce::Rectangle<int> area)
{
    // The canvas owns artwork and controls, so JUCE applies the same scale,
    // scroll offset and viewport clipping to paint, invalidation and hit tests.
    canvas.repaint(area.expanded(1));
}
void ChimeraEditor::refreshVisibleState()
{
    bool hardwareDirty=stateDirty;
    for(const auto& lane:lanes)if(lane.amp.isVisible() && lane.lastModel!=processor.selectedAmpModel((int)(&lane-lanes.data())))hardwareDirty=true;
    for(const auto& effect:effects)if(effect.model.isVisible() && effect.lastModel!=juce::jmax(0,effect.model.getSelectedId()-1))hardwareDirty=true;
    if(hardwareDirty)updateHardwareStyles();
    if(page==0) {
        const std::array<double,4> next{{x1.getValue(),x2.getValue(),dualFrequency.getValue(),processor.getSampleRate()}};
        if(bandKey.update(next) || stateDirty)updateBandLabels();
    }
    compareA.setToggleState(processor.comparisonSlot()==0,juce::dontSendNotification);compareB.setToggleState(processor.comparisonSlot()==1,juce::dontSendNotification);
    {   // MATCH shows the cut it applies to the active sound.
        const auto& match=processor.abLevelMatch();const float cut=match.appliedDb();
        matchAB.setEnabled(!match.outputProtected());
        matchAB.setToggleState(match.enabled(),juce::dontSendNotification);
        const auto text=match.outputProtected() ? juce::String("SAFE") : match.enabled() && cut<-.05f ? juce::String(cut,1) : juce::String("MATCH");
        if(matchAB.getButtonText()!=text)matchAB.setButtonText(text);
    }
    for(int i=0;i<3;++i) {
        auto& lane=lanes[(size_t)i];if(!lane.amp.isVisible())continue;
        const auto n=juce::String(i+1);const bool active=processor.parameters().getRawParameterValue("cab"+n)->load()>.5f;
        const int source=(int)processor.parameters().getRawParameterValue("cabtype"+n)->load();
        const auto revision=processor.cabDisplayRevision(i);
        const std::array<uint64_t,6> key{{revision[0],revision[1],revision[2],revision[3],(uint64_t)source,(uint64_t)active}};
        if(lane.cabKey.update(key) || stateDirty) {
            lane.metadata=processor.cabMetadata(i);++metadataReads;
            lane.status=(lastMode==2 && i==0 ? juce::String{} : active?juce::String{}:juce::String("BYPASSED | "))+processor.cabStatus(i);
            const bool modeled=processor.parameters().getRawParameterValue(spectralforge::originalCabID(i,"Aon"))->load()>.5f;
            lane.cabType.sync(source,processor.userIRName(i),modeled ? lane.metadata.displayLabel : juce::String{});
            lane.details.setEnabled(!modeled);
            lane.cabStatus.setText(lane.status,juce::dontSendNotification);
            lane.cabStatus.setTooltip(lane.status+"\n"+lane.metadata.summary());
            lane.cabLow.setEnabled(active);lane.cabHigh.setEnabled(active);
            const int count=lastMode==0?1:lastMode==1?2:3,width=(1140-14*(count-1))/count;
            repaintDesign({32+i*(width+14),680,51,43});
            if(lastMode==2 && i==0)lowMeterKey.dirty=true;
        }
    }
    for(int position=3;position<6;++position) {
        const int family=rackOrder[(size_t)position];auto& effect=effects[(size_t)family];
        if(!effect.model.isVisible())continue;
        if(effect.power.update(effect.enabled.getToggleState())) {
            effect.enabled.setButtonText(effect.enabled.getToggleState()?"ON":"OFF");
            repaintDesign({34,330+position*69+24,10,10});
        }
    }
    midi.setButtonText(processor.learningMidi()?"LEARN":"MIDI");
    const std::array<double,3> nextTempo{{processor.currentTempo(),hostTempo.getToggleState()?1.:0.,delaySync.getToggleState()?1.:0.}};
    if(tempoKey.update(nextTempo) || stateDirty) {
        tempo.setEnabled(!hostTempo.getToggleState());tempo.updateText();
        if(effects[1].model.isVisible()){effects[1].controls[0].setEnabled(!delaySync.getToggleState());effects[1].controls[0].updateText();}
    }
    stateDirty=false;
}
void ChimeraEditor::refreshMeters()
{
    using spectralforge::ui::tenth;
    const auto meterKey=[](float level) {return std::array<int,2>{{spectralforge::ui::tenth(juce::Decibels::gainToDecibels(level,-90.f)),level>=1.f?1:0}};};
    if(inputMeterKey.update(meterKey(processor.inputMeter())))repaintDesign({30,124,119,110});
    if(outputMeterKey.update(meterKey(processor.outputMeter())))repaintDesign({974,124,176,110});
    const double sr=processor.getSampleRate()>0?processor.getSampleRate():48000;
    const auto pitch=pitchOn.getToggleState()?"+ "+juce::String(1000.0*processor.pitchLatency()/sr,1)+" ms latency":juce::String("BYPASSED | zero added latency");
    if(pitchText.update(pitch))pitchStatus.setText(pitch,juce::dontSendNotification);
    if(transposeMode.getSelectedId()!=processor.transposeQuality()+1)
        transposeMode.setSelectedId(processor.transposeQuality()+1,juce::dontSendNotification);
    if(page==0 && lastMode==2 && lowMeterKey.update(tenth(processor.lowCompMeter()))) {
        lanes[0].cabStatus.setText("GR "+juce::String(processor.lowCompMeter(),1)+" dB | DRIVE 0 | "+lanes[0].status,juce::dontSendNotification);
        repaintDesign({37,638,336,5});
    }
    if(page==2)for(int p=3;p<6;++p)if(postMeterKeys[(size_t)p-3].update(meterKey(processor.postModuleLevel(p))))repaintDesign({318,330+p*69+16,112,32});

    const auto cpu="AUDIO "+juce::String(processor.cpuLoad(),1)+"%  |  PEAK "+juce::String(processor.cpuPeakLoad(),1)+"%";
    if(cpuText.update(cpu)){callbackLoad.setText(cpu,juce::dontSendNotification);callbackLoad.setColour(juce::Label::textColourId,processor.cpuPeakLoad()>=100?juce::Colours::salmon:muted);}
    const auto transport=(hostTempo.getToggleState()?"HOST ":"TEMPO ")+juce::String(processor.currentTempo(),1)+" BPM  |  "+juce::String(processor.getLatencySamples())+" SAMPLES";
    if(transportText.update(transport))repaintDesign({902,763,254,15});
}
void ChimeraEditor::updateHardwareStyles()
{
    auto apply=[](juce::Slider& slider,int hardware,bool bright) {
        slider.getProperties().set("knobStyle",hardware);slider.getProperties().set("brightFace",bright);
        slider.setColour(juce::Slider::textBoxTextColourId,bright ? juce::Colour(0xff202923) : ink);
        slider.setColour(juce::Slider::textBoxBackgroundColourId,bright ? juce::Colour(0xffeae5d9).withAlpha(.67f) : background.withAlpha(.8f));
    };
    for(size_t family=0;family<effects.size();++family) {
        auto& effect=effects[family];if(!effect.model.isVisible())continue;const int model=juce::jmax(0,effect.model.getSelectedId()-1);
        if(effect.lastModel==model)continue;
        effect.lastModel=model;
        const auto& info=spectralforge::modelInfo((int)family,model);
        const auto description=juce::String(info.character)+"\nApplied after rig merge.";
        effect.model.setTooltip(juce::String(spectralforge::effectFamilyMenuName((int)family,model))+"\n"+description);
        effect.header.setTooltip(description);effect.description.setTooltip(description);
        effect.description.setText(juce::String(info.character).replace(" / ","\n"),juce::dontSendNotification);effect.scope.setText("AFTER RIG MERGE",juce::dontSendNotification);
        if(family==9){effect.labels[0].setText(model==1?"LOW 110 Hz":model==2?"LOW 60 Hz":"LOW 80 Hz",juce::dontSendNotification);effect.labels[4].setText(model==1?"HIGH 12 kHz":model==2?"HIGH 10 kHz":"HIGH 8 kHz",juce::dontSendNotification);}
        for(int position=3;position<6;++position)if(rackOrder[(size_t)position]==(int)family)repaintDesign({20,330+position*69,1140,64});
        const bool pedal=family==0 || (family>=3 && family<=6);
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
        auto& lane=lanes[i];if(!lane.amp.isVisible())continue;const int model=processor.selectedAmpModel((int)i);
        lane.amp.syncSelectedId(model+1);
        if(lane.lastModel==model)continue;
        const int count=lastMode==0?1:lastMode==1?2:3,width=(1140-14*(count-1))/count;
        repaintDesign({20+(int)i*(width+14),330,width,412});
        lane.lastModel=model;const auto& amp=spectralforge::ampInfo(model);
        lane.ampReference.setText(juce::String::fromUTF8(amp.character),juce::dontSendNotification);
        lane.amp.setTooltip(juce::String(amp.name)+"\n"+amp.character);
        lane.ampReference.setTooltip(lane.ampReference.getText());
        const auto hardware=spectralforge::art::headStyle(model);
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
void ChimeraEditor::updateDynamicsEntries()
{
    const bool showing=page==3 && dynamicsViewOpen && !tunerViewOpen;
    for(size_t i=0;i<dynamicsEntries.size();++i)
        dynamicsEntries[i].setToggleState(showing && dynamicsPanel->selectedPage()==dynamicsEntryPages[i],juce::dontSendNotification);
}
void ChimeraEditor::updateModeUI()
{
    lastEnvelopeFirst=preOrder.getSelectedId()==2;pedalOrder=lastEnvelopeFirst ? std::array<int,5>{4,3,5,6,0} : std::array<int,5>{3,4,5,6,0};
    lastBoostAfterDrive=gainOrder.getSelectedId()==2;if(lastBoostAfterDrive)std::swap(pedalOrder[3],pedalOrder[4]);
    lastBoardEnabled=processor.parameters().getRawParameterValue("boardEnabled")->load()>.5f;
    preEngineStatus.setVisible(page==1 && !tunerViewOpen);
    preEngineStatus.setText("5-SLOT PRE  /  select and edit each pedal",juce::dontSendNotification);
    boardPanel.setVisible(page==1 && !tunerViewOpen);
    preOrder.setVisible(false);gainOrder.setVisible(false);
    const int nextMode=(int)processor.parameters().getRawParameterValue("mode")->load();
    // PRE, POST and the two EQ banks are global. Changing routing mode keeps
    // their current editor open, so retain its matching path selection too.
    if(nextMode!=lastMode && lastMode>=0 && page>=1 && page<=4)pathSelections[size_t(nextMode)]=pathSelections[size_t(lastMode)];
    if(nextMode!=lastMode){showRigControls=false;if(page==0)pathSelections[size_t(nextMode)]="cab";}
    lastMode=nextMode;
    lastDualCross=processor.parameters().getRawParameterValue("dualtype")->load()>.5f;lastTuner=processor.parameters().getRawParameterValue("tuneron")->load()>.5f;
    const bool matrix=lastMode==2,split=matrix || (lastMode==1 && lastDualCross); const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    // Every routing mode opens on its room. HEAD/CAB navigation is a
    // presentation choice and must not change a rig's cabinet source.
    const bool roomVisible=page==0 && !showRigControls && !tunerViewOpen;
    if(roomVisible && !cabRoom) {
        cabRoom=std::make_unique<CabRoomOverview>(processor);canvas.addAndMakeVisible(*cabRoom);
        cabRoom->onCabinetSelected=[this](int lane){openCabWorkspace(lane,true);};
        cabRoom->onAmplifierSelected=[this](int lane){openAmpHead(lane);};
    }
    if(cabRoom) {cabRoom->setVisible(roomVisible);if(roomVisible)cabRoom->refreshState();}
    x1.setVisible(matrix && page==0 && !tunerViewOpen); x2.setVisible(matrix && page==0 && !tunerViewOpen); x1Label.setVisible(matrix && page==0 && !tunerViewOpen); x2Label.setVisible(matrix && page==0 && !tunerViewOpen);
    const bool showingDynamics=page==3 && dynamicsViewOpen && !tunerViewOpen;
    // Release any borrowed EQ editor before showing it on its original bank.
    // The outgoing FIFO consumer must stop before the incoming one starts.
    if(!showingDynamics)dynamicsPanel->setVisible(false);
    for(int e=0;e<2;++e) {
        eqPanels[size_t(e)]->setVisible(page==3 && selectedEQBank==e && !tunerViewOpen && !dynamicsViewOpen);
        eqPanels[size_t(e)]->setNavigationSelected(page==3 && pathSelections[size_t(lastMode)]==(e==0?"tone.eq":"final.eq"));
    }
    if(showingDynamics)dynamicsPanel->setVisible(true);
    // Direct dynamics entries stay reachable from every page; EQ uses the signal path.
    updateDynamicsEntries();
    for(size_t i=0;i<effects.size();++i) {
        auto& effect=effects[i];const bool show=page==2 && !tunerViewOpen && (i==10 || i==1 || i==2);
        effect.header.setVisible(show);effect.scope.setVisible(show);effect.description.setVisible(false);effect.enabled.setVisible(show);effect.model.setVisible(show);effect.expand.setVisible(show);
        for(int k=0;k<5;++k){const bool visible=show && effect.attachments[(size_t)k]!=nullptr;effect.controls[k].setVisible(visible);effect.labels[k].setVisible(visible);}
    }
    for(auto& post:postPanels) {post->setVisible(page==2 && !tunerViewOpen);post->refreshIfNeeded();}
    creativeStrip->setVisible(page==2 && !tunerViewOpen);
    synthPage->setVisible(page==4 && !tunerViewOpen);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&lowComp,&lowCompLabel,&lowAmpMix,&diVoice}) c->setVisible(false);
    for(int i=0;i<3;++i)
    {
        auto& lane=lanes[i]; const bool show=i<count && page==0 && !roomVisible && !tunerViewOpen;
        for(juce::Component* c : std::initializer_list<juce::Component*>{&lane.header,&lane.range,&lane.ampReference,&lane.amp,&lane.ampOn,&lane.mute,&lane.solo,&lane.polarity}) c->setVisible(show);
        // Cabinet edits belong exclusively to the CAB workspace.
        for(juce::Component* c:std::initializer_list<juce::Component*>{&lane.cabOn,&lane.load,&lane.details,&lane.cabType,&lane.cabLow,&lane.cabHigh,&lane.lowLabel,&lane.highLabel,&lane.cabStatus}) c->setVisible(false);
        lane.nativePanel->setVisible(show);lane.nativePanel->refreshIfNeeded();lane.head->setVisible(show);if(show)lane.head->refresh();
        auto controls=lane.controls();
        for(size_t k=0;k<8;++k) { controls[k]->setVisible(false); lane.knobLabels[k].setVisible(false); }
        if(matrix && i==0) {
            for(juce::Component* c:std::initializer_list<juce::Component*>{&lane.drive,&lane.knobLabels[0]}) c->setVisible(false);
        }
        lane.bandTone.setVisible(false); lane.toneLabel.setVisible(false); lane.tonePivot.setVisible(false);
        lane.header.setText(matrix ? (i==0 ? "01 / LOW" : i==1 ? "02 / MID" : "03 / HIGH") : "RIG "+juce::String(i+1),juce::dontSendNotification);
        if(!matrix) lane.range.setText("Full-range input",juce::dontSendNotification);
    }
    mode.setVisible(true);routingHelp.setVisible(true);dualType.setVisible(lastMode==1);dualLabel.setVisible(lastMode==1 && page==0);
    dualBlend.setVisible(lastMode==1 && !lastDualCross && page==0);dualFrequency.setVisible(lastMode==1 && lastDualCross && page==0);
    dualLabel.setText(lastDualCross ? "LOW / HIGH CROSSOVER" : "RIG 1 : RIG 2",juce::dontSendNotification);
    tunerOn.setVisible(false);tunerMute.setVisible(false);globalSliders[5].setVisible(false);delaySync.setVisible(page==2 && !tunerViewOpen);
    routingHelp.setText(page==4 ? "SYNTH / GUITAR + MIDI" : page==3 ? "TONE EQ > DYNAMICS > POST > FINAL EQ" : page==1 ? "PEDALBOARD / SHARED INPUT" : page==2 ? "RACK / AFTER RIG MERGE" : matrix ? "LR4 / 24 dB per octave" : lastMode==0 ? "ONE FULL-RANGE RIG" : lastDualCross ? "LR4 / 24 dB per octave" : "FULL-RANGE BLEND",juce::dontSendNotification);
    precisionTuner->setVisible(tunerViewOpen);if(tunerViewOpen)precisionTuner->toFront(false);
    tunerOpen.setToggleState(tunerViewOpen,juce::dontSendNotification);
    tunerOpen.getProperties().set("utilityActive",tunerViewOpen);
    tunerOpen.setButtonText(lastTuner?(processor.parameters().getRawParameterValue("tunermute")->load()>.5f?"TUNER / MUTED":"TUNER / ON"):"TUNER");
    stateDirty=true;updateBandLabels();refreshVisibleState();layoutControls();
    // User navigation must update both views with the visible editor, rather
    // than waiting for their independent automation/inspection timers.
    const auto syncPath=[this](SignalPathView* view){if(view){view->refresh();view->select(pathSelections[size_t(lastMode)]);}};
    syncPath(signalPath.get());
    if(pathDialog)syncPath(dynamic_cast<SignalPathView*>(spectralforge::ui::dialogContent(*pathDialog)));
    repaint();
}
void ChimeraEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
}
void ChimeraEditor::paintCanvas(juce::Graphics& g)
{
    g.fillAll(background);
    spectralforge::art::raster(g,spectralforge::art::Surface::workbench,{0,0,1180,780});
    g.setColour(background.withAlpha(.40f));g.fillRect(0,0,1180,780);
    g.setColour(accent.withAlpha(.7f));g.fillRect(20,20,2,36);
    text(g,"SOUND A / B",490,11,163,14,8.5f,muted);
    if(brandEmblem.isValid())g.drawImageWithin(brandEmblem,25,5,62,60,juce::RectanglePlacement::centred);
    if(wordmark.isValid())g.drawImageWithin(wordmark,96,4,365,59,juce::RectanglePlacement::centred);
    else {text(g,"SPECTRALFORGE",106,5,350,15,10.f);text(g,"CHIMERA",103,17,360,45,36.f,ink);}
    text(g,"OVERSAMPLING",816,8,110,22,10.f); text(g,"SIZE",986,8,64,22,10.f);
    g.setColour(line); g.drawHorizontalLine(67,20,1160);
    const std::array<juce::Rectangle<float>,5> panels{{{20,80,138,168},{170,80,254,168},{436,80,176,168},{624,80,326,168},{962,80,198,168}}};
    for(auto box:panels) { g.setColour(panel.withAlpha(.88f)); g.fillRoundedRectangle(box,5); g.setColour(line); g.drawRoundedRectangle(box,5,1); }
    text(g,"INPUT",32,91,104,24,12.f,ink);
    text(g,"DYNAMICS",184,88,226,22,11.f,ink);
    text(g,"OUTPUT",976,91,152,24,12.f,ink);
    meter(g,processor.inputMeter(),141,126,6,93); meter(g,processor.outputMeter(),1142,126,6,93);
    text(g,juce::String(juce::Decibels::gainToDecibels(processor.inputMeter(),-90.f),1)+" dBFS",32,210,106,22,11.f,processor.inputMeter()>=1 ? juce::Colours::salmon : muted);
    text(g,juce::String(juce::Decibels::gainToDecibels(processor.outputMeter(),-90.f),1)+" dBFS",976,210,152,22,11.f,processor.outputMeter()>=1 ? juce::Colours::salmon : muted);
    text(g,"INPUT MODE",638,88,108,16,9.f);text(g,"PRESET",770,88,160,16,9.f);
    text(g,"MODE",664,8,140,22,10.f);
    if(page==2) {
        for(int position=3;position<6;++position) {
            const float y=330.f+position*69.f;const int family=rackOrder[(size_t)position];const auto& effect=effects[(size_t)family];
            if(!g.clipRegionIntersects({20,(int)y,1140,64}))continue;
            g.setColour(panel.withAlpha(.9f));g.fillRoundedRectangle(20,y,1140,64,4);
            spectralforge::art::rack(g,{296,y,864,64},family,effect.model.getSelectedId()-1);
            g.setColour(juce::Colour(spectralforge::modelInfo(family,effect.model.getSelectedId()-1).colour).withAlpha(.85f));g.fillRect(28.f,y+8,2.f,46.f);
            g.setColour(effect.enabled.getToggleState()?juce::Colour(0xffa5c1a0):line);g.fillEllipse(36,y+26,5,5);
            const float level=processor.postModuleLevel(position);g.setColour(background.withAlpha(.88f));g.fillRoundedRectangle(318,y+16,112,32,3);
            text(g,"OUT  "+juce::String(juce::Decibels::gainToDecibels(level,-60.f),1)+" dBFS",322,(int)y+17,104,14,8.5f,ink,juce::Justification::centred);
            const float amount=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(level,-60.f)+60.f)/60.f);
            for(int segment=0;segment<16;++segment){g.setColour(segment<float(amount*16)?(level>=1?juce::Colour(0xffe49b73):juce::Colour(0xffa5c1a0)):line);g.fillRect(326.f+segment*6,y+35,4.f,5.f);}
        }
    } else if(page==0 && !(cabRoom && cabRoom->isVisible())) {
        const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;const int width=(1140-14*(count-1))/count;
        for(int i=0;i<count;++i) {
            const float x=float(20+i*(width+14));const auto& lane=lanes[(size_t)i];const bool di=lastMode==2 && i==0;
            if(!g.clipRegionIntersects({(int)x,330,width,412}))continue;
            const auto colour=spectralforge::art::ampColour(lane.amp.getSelectedId()-1);
            g.setColour(panel.withAlpha(.8f));g.fillRoundedRectangle(x,330,float(width),412,5);
            g.setColour(line.withAlpha(.65f));g.drawRoundedRectangle(x,330,float(width),412,5,1);
            if(lastMode==0) {

                g.setColour(background.withAlpha(.75f));g.fillRoundedRectangle(x+490,429,float(width-502),300,3);
            } else {

                g.setColour(background.withAlpha(.82f));g.fillRoundedRectangle(x+10,490,float(width-20),240,3);
            }
            g.setColour(colour.withAlpha(.65f));g.fillRect(x+12,332.f,float(width-24),2.f);

            if(di) {const float reduction=juce::jlimit(0.f,1.f,processor.lowCompMeter()/24.f);g.setColour(line);g.fillRoundedRectangle(x+17,733,float(width-34),2,1);if(reduction>0.f){g.setColour(accent);g.fillRoundedRectangle(x+17,733,float(width-34)*reduction,2,1);}}
        }
    }
    g.setColour(juce::Colour(0xff12231d));g.fillRoundedRectangle(14,743,1152,37,4);
    g.setColour(line);g.drawVerticalLine(252,749,774);g.drawVerticalLine(710,749,774);g.drawVerticalLine(898,749,774);
    text(g,(hostTempo.getToggleState() ? "HOST " : "TEMPO ")+juce::String(processor.currentTempo(),1)+" BPM  |  "+juce::String(processor.getLatencySamples())+" SAMPLES",902,763,254,15,9.5f,muted,juce::Justification::centredRight);

}
void ChimeraEditor::resized()
{
    // A negative old content origin can keep both JUCE scrollbars visible
    // after zooming out. Recompute at the origin, then restore the offset
    // clamped against the new visible area.
    const auto previousViewPosition=editorViewport.getViewPosition();
    editorViewport.setViewPosition(0,0);
    const float factor=requestedScale>0.f ? requestedScale : spectralforge::ui::fittedEditorScale(getLocalBounds(),{});
    const int choice=juce::jlimit(1,4,juce::roundToInt((factor-.75f)*4)+1);
    if(std::abs(factor-(choice+2)*.25f)<.002f) scale.setSelectedId(choice,juce::dontSendNotification);
    else {scale.setSelectedId(0,juce::dontSendNotification);scale.setText(juce::String(factor*100,0)+"%",juce::dontSendNotification);}
    editorViewport.setBounds(getLocalBounds());
    canvas.setBounds(0,0,1180,780); canvas.setTransform(juce::AffineTransform::scale(factor));
    if(easyView){easyView->setBounds(0,0,1180,780);easyView->setTransform(juce::AffineTransform::scale(factor));}
    const auto contentSize=spectralforge::ui::editorSizeForScale(factor);
    scrollContent.setSize(contentSize.x,contentSize.y);
    editorViewport.setViewPosition(previousViewPosition);
    layoutControls();
}
void ChimeraEditor::setViewSize(juce::Point<int> size,float preferredScale)
{
    requestedScale=preferredScale>0.f ? juce::jlimit(spectralforge::ui::minimumEditorScale,spectralforge::ui::maximumEditorScale,preferredScale) : 0.f;
    setSize(size.x,size.y);
    resized(); // A scale change may leave the outer viewport size unchanged.
}
void ChimeraEditor::fitToDisplay()
{
    // Chrome is whatever surrounds the editor in its own top-level window
    // (Standalone title bar and notification). A host's frame is outside the
    // editor's top-level component, so reserve one title-bar height there.
    auto* top=getTopLevelComponent();
    const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(top->getScreenBounds());
    if(display==nullptr){resized();return;}
    const auto size=spectralforge::ui::fittedEditorSize(display->userArea,spectralforge::ui::editorWindowChrome(*this));
    setViewSize(size);
    if(top!=this)top->setTopLeftPosition(spectralforge::ui::centredWindowPosition(display->userArea,{top->getWidth(),top->getHeight()}));
    resized();
}
void ChimeraEditor::layoutControls()
{
    callbackLoad.setBounds(902,744,254,18);
    if(cabRoom)cabRoom->setBounds(20,330,1140,412);
    for(auto& eq:eqPanels)eq->setBounds(20,330,1140,412);
    for(size_t i=0;i<dynamicsEntries.size();++i)
        dynamicsEntries[i].setBounds(182+int(i%2)*118,110+int(i/2)*34,112,30);
    dynamicsPanel->setBounds(20,330,1140,412);
    signalPath->setBounds(20,253,1140,44);
    precisionTuner->setBounds(20,330,1140,412);
    preEngineStatus.setBounds(585,302,575,24);boardPanel.setBounds(20,330,1140,415);
    preOrder.setBounds(585,268,257,28);gainOrder.setBounds(852,268,308,28);
    compareA.setBounds(490,30,28,28);compareB.setBounds(521,30,28,28);copyAB.setBounds(552,30,40,28);matchAB.setBounds(595,30,60,28);
    title.setBounds(30,16,166,40); quality.setBounds(816,30,130,28); scale.setBounds(986,30,80,28);irLibraryButton.setBounds(1074,30,86,28); info.setBounds(808,748,86,28);
    mode.setBounds(664,30,140,28);routingHelp.setBounds(20,302,278,24);
    dualType.setBounds(300,302,180,24);dualLabel.setBounds(494,303,130,22);dualBlend.setBounds(628,302,330,24);dualFrequency.setBounds(628,302,330,24);
    x1Label.setBounds(400,303,105,22);x1.setBounds(507,302,210,24);
    x2Label.setBounds(732,303,105,22);x2.setBounds(839,302,306,24);
    pitchOn.setBounds(450,93,148,24); tunerOpen.setBounds(20,748,154,28);tunerMute.setBounds(965,273,112,25);
    globalSliders[0].setBounds(30,115,106,92);
    globalSliders[1].setBounds(244,126,167,28);
    globalSliders[2].setBounds(180,173,111,27); globalSliders[3].setBounds(306,173,106,27);
    globalSliders[7].setBounds(244,201,167,26);
    globalSliders[4].setBounds(448,123,152,85); globalSliders[5].setBounds(799,273,140,25); globalSliders[6].setBounds(974,115,159,92);
    pitchStatus.setBounds(447,209,158,18);
    transposeMode.setBounds(447,229,158,20);
    inputMode.setBounds(638,107,108,26);presets.setBounds(756,107,180,26);presetPrevious.setBounds(638,144,28,25);presetNext.setBounds(670,144,28,25);presetSave.setBounds(706,144,107,25);presetLoad.setBounds(821,144,115,25);
    doublerOn.setBounds(638,190,91,26);doublerTime.setBounds(741,190,195,26);
    midi.setBounds(182,748,66,28);tap.setBounds(256,748,56,28);tempo.setBounds(320,748,132,28);hostTempo.setBounds(460,748,112,28);metronome.setBounds(580,748,126,28);
    easyOpen.setBounds(714,748,86,28);
    inputGuide.setBounds(16,2,456,60);
    for(size_t i=0;i<postPanels.size();++i)postPanels[i]->setBounds(20,330+(int)i*69,1140,64);
    creativeStrip->setBounds(310,302,850,24);
    synthPage->setBounds(20,302,1140,438);
    for(int position=3;position<6;++position) {
        auto& effect=effects[(size_t)rackOrder[(size_t)position]];const int y=330+position*69;
        effect.header.setBounds(48,y+2,186,19);effect.model.setBounds(48,y+23,182,24);effect.scope.setBounds(48,y+47,182,15);effect.enabled.setBounds(241,y+13,43,25);effect.expand.setBounds(241,y+42,43,19);
        int count=0;for(auto& attachment:effect.attachments)if(attachment)++count;const int cell=692/count;
        for(int k=0;k<count;++k){const int controlWidth=juce::jmin(144,cell-7),x=458+k*cell+(cell-controlWidth)/2;effect.labels[(size_t)k].setBounds(x,y+2,controlWidth,16);effect.controls[(size_t)k].setTextBoxStyle(juce::Slider::TextBoxRight,false,68,22);effect.controls[(size_t)k].setBounds(x,y+17,controlWidth,45);}
    }
    delaySync.setBounds(297,330+4*69+1,47,15);
    diVoice.setBounds(32,616,60,20);lowAmpMix.setBounds(90,615,158,23);diNote.setVisible(false);
    lowCompLabel.setBounds(253,616,43,20);lowComp.setSliderStyle(juce::Slider::LinearHorizontal);lowComp.setTextBoxStyle(juce::Slider::TextBoxRight,false,37,20);lowComp.setBounds(296,615,82,23);
    const bool matrix=lastMode==2,split=matrix || (lastMode==1 && lastDualCross); const int count=lastMode==0 ? 1 : lastMode==1 ? 2 : 3;
    const int width=(1140-14*(count-1))/count;
    for(int i=0;i<count;++i)
    {
        auto& lane=lanes[i]; const int x=20+i*(width+14);
        lane.header.setBounds(x+12,339,width-165,22); lane.range.setBounds(x+12,362,width-24,17);
        lane.mute.setBounds(x+width-147,344,48,23); lane.solo.setBounds(x+width-95,344,44,23); lane.polarity.setBounds(x+width-47,344,35,23);
        lane.ampOn.setBounds(x+12,382,48,29);lane.amp.setBounds(x+68,382,width-80,29);lane.ampReference.setBounds(x+14,413,width-28,18);
        lane.head->setBounds(x+12,433,lastMode==0?466:width-24,lastMode==0?198:55);
        lane.nativePanel->setBounds(lastMode==0?x+495:x+12,lastMode==0?433:490,lastMode==0?width-509:width-24,lastMode==0?300:240);
        lane.bandTone.setSliderStyle(juce::Slider::LinearHorizontal);lane.bandTone.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,20);
        lane.toneLabel.setBounds(x+12,615,68,21);lane.bandTone.setBounds(x+82,615,width-94,22);lane.tonePivot.setVisible(false);
        if(matrix && i==0){lane.bandTone.setVisible(false);lane.toneLabel.setVisible(false);}
        lane.cabOn.setBounds(x+12,643,48,26); lane.cabType.setBounds(x+68,643,width-197,26); lane.load.setBounds(x+width-121,643,54,26);lane.details.setBounds(x+width-61,643,49,26);
        const int cell=(width-90)/2;
        lane.lowLabel.setBounds(x+78,674,cell-8,18); lane.highLabel.setBounds(x+78+cell,674,cell-8,18);
        lane.cabLow.setBounds(x+78,692,cell-8,26); lane.cabHigh.setBounds(x+78+cell,692,cell-8,26);
        lane.cabStatus.setBounds(x+12,719,width-24,21);
    }
}

void ChimeraEditor::exportCabinet(int lane)
{
    // The cabinet as this rig hears it: IR or model, mic A/B blend, delay,
    // polarity, cuts. Amp, effects and dynamics are not part of the file.
    chooser=std::make_unique<juce::FileChooser>("Export cabinet IR (Rig "+juce::String(lane+1)+")",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Chimera Rig "+juce::String(lane+1)+" cabinet.wav"),"*.wav");
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,lane](const juce::FileChooser& chosen){
        if(!safe || chosen.getResult()==juce::File{}) return;
        const auto target=spectralforge::cabIRFile::wavDestination(chosen.getResult());
        const auto save=[safe,lane,target](int accepted) {
            if(!safe || accepted!=1)return;
            const auto result=safe->processor.exportCabinetIR(lane,target);
            if(result.failed())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Export cabinet IR",result.getErrorMessage(),"OK",safe);
        };
        // The native dialog confirmed the selected name, not a different name
        // produced by appending/replacing its extension.
        if(spectralforge::cabIRFile::needsAdditionalOverwriteConfirmation(chosen.getResult()))
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
                "Replace cabinet IR?",target.getFullPathName()+" already exists.","Replace","Cancel",
                safe.getComponent(),juce::ModalCallbackFunction::create(save));
        else save(1);
    });
}
void ChimeraEditor::referenceFile(bool save)
{
    chooser=std::make_unique<juce::FileChooser>(save ? "Save reference (A/B + embedded IRs)" : "Load reference",juce::File{},"*.chimera");
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    const int flags=juce::FileBrowserComponent::canSelectFiles|(save ? juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode);
    chooser->launchAsync(flags,[safe,save](const juce::FileChooser& file){
        if(!safe || file.getResult()==juce::File{}) return;
        juce::MemoryBlock data;bool ok=false;
        if(save) {if(safe->processor.tryGetStateInformation(data))ok=file.getResult().withFileExtension(".chimera").replaceWithData(data.getData(),data.getSize());}
        else if(file.getResult().getSize()<=64*1024*1024 && file.getResult().loadFileAsData(data)) {
            auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),(int)data.getSize());
            if(xml && xml->hasTagName("PARAMS")) {safe->processor.setStateInformation(data.getData(),(int)data.getSize());ok=true;}
        }
        if(!ok) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Reference file",save ? "Could not save the reference file." : "This is not a valid Chimera reference file.","OK",safe);
        if(ok) {safe->markPresetCustom();safe->presetOrigin=0;safe->presets.setSelectedId(0,juce::dontSendNotification);safe->presets.setText(file.getResult().getFileNameWithoutExtension(),juce::dontSendNotification);}
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
    if((bool)button.getProperties()["utilityDock"]){g.setColour(button.getToggleState()||(bool)button.getProperties()["utilityActive"]?background:ink);g.setFont(getTextButtonFont(button,button.getHeight()));g.drawFittedText(button.getButtonText(),button.getLocalBounds().reduced(5),juce::Justification::centred,1);return;}
    juce::LookAndFeel_V4::drawButtonText(g,button,hover,down);
}

void ChimeraEditor::showTesterCode()
{
    const auto holder=processor.testerCodeHolder();
    testerDialog=std::make_unique<juce::AlertWindow>("Beta tester code",
        holder.isEmpty() ? juce::String("Type or paste the code you received as a beta tester, e.g. CHT1-NAME-XXXXXX. It removes the beta expiry on this computer for this and later beta versions.")
                         : "Active for "+holder+". This and later beta versions do not expire on this computer.",
        juce::MessageBoxIconType::NoIcon,this);
    testerDialog->addTextEditor("code",{},"Code");
    testerDialog->addButton("APPLY",1,juce::KeyPress(juce::KeyPress::returnKey));
    if(holder.isNotEmpty())testerDialog->addButton("REMOVE",2);
    testerDialog->addButton("CANCEL",0,juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<ChimeraEditor> safe(this);
    testerDialog->enterModalState(true,juce::ModalCallbackFunction::create([safe](int result){
        if(!safe || !safe->testerDialog)return;
        const auto code=safe->testerDialog->getTextEditorContents("code");
        juce::MessageManager::callAsync([safe]{if(safe)safe->testerDialog.reset();});
        juce::String message;
        if(result==1){const auto grant=safe->processor.applyTesterCode(code);
            message=grant.valid ? "Beta tester code accepted for "+grant.name+". This and later beta versions no longer expire on this computer." : grant.error;}
        else if(result==2){safe->processor.removeTesterCode();message="Beta tester code removed. The beta expiry applies again to newly opened instances.";}
        if(message.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Beta tester code",message,"OK",safe.getComponent());
        safe->info.setTooltip(safe->processor.betaNotice());
    }),false);
}
void ChimeraEditor::showSupport()
{
    // A DAW must never join an HTTP operation while closing its plug-in view.
    // Keep the downloader in the standalone application; hosted support opens
    // the release page in the user's browser on an explicit click.
    if(processor.wrapperType==juce::AudioProcessor::wrapperType_Standalone && !support)
        support=std::make_shared<spectralforge::release::ReleaseSupport>();
    spectralforge::release::Diagnostics d;
    using Format=spectralforge::release::Diagnostics::Format;
    d.format=processor.wrapperType==juce::AudioProcessor::wrapperType_Standalone ? Format::standalone : processor.wrapperType==juce::AudioProcessor::wrapperType_VST3 ? Format::vst3 : processor.wrapperType==juce::AudioProcessor::wrapperType_AudioUnit ? Format::au : Format::other;
    d.sampleRate=processor.getSampleRate();d.blockSize=processor.getBlockSize();d.inputs=processor.getTotalNumInputChannels();d.outputs=processor.getTotalNumOutputChannels();d.latencySamples=processor.getLatencySamples();
    juce::DialogWindow::LaunchOptions options;options.content.setOwned(new ChimeraSupportPanel(support,d,
        [safe=juce::Component::SafePointer<ChimeraEditor>(this)]{return safe ? safe->processor.betaNotice() : juce::String{};},
        [safe=juce::Component::SafePointer<ChimeraEditor>(this)]{if(safe)safe->processor.recheckBetaClock();}));options.dialogTitle="Chimera / Support & Updates";options.dialogBackgroundColour=background;options.escapeKeyTriggersCloseButton=true;options.useNativeTitleBar=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
}

void ChimeraEditor::showDiagnostics()
{
    juce::DialogWindow::LaunchOptions options;options.content.setOwned(new DiagnosticsPanel(processor));
    options.dialogTitle="Chimera / Diagnostic snapshot";options.dialogBackgroundColour=background;
    options.escapeKeyTriggersCloseButton=true;options.useNativeTitleBar=true;options.resizable=false;options.componentToCentreAround=this;
    trackDialog(options.launchAsync());
}

void ChimeraEditor::trackDialog(juce::DialogWindow* dialog)
{
    dialogs.erase(std::remove_if(dialogs.begin(),dialogs.end(),[](const auto& item){return item==nullptr;}),dialogs.end());
    if(dialog){dialogs.emplace_back(dialog);spectralforge::ui::fitScrollableDialog(dialog);}
}
void ChimeraEditor::setEasyMode(bool easy,bool rememberChoice)
{
    // Easy view hides the comparison controls, so do not leave an invisible cut.
    if(easy)processor.abLevelMatch().setEnabled(false);
    if(rememberChoice)spectralforge::easy::Preference::remember(easy);
    if(easy && !easyView) {
        easyView=std::make_unique<EasyModeView>(processor);
        easyView->setBrand(wordmark,brandEmblem);
        easyView->fillPresetMenu(fillPresetMenu);
        easyView->openFullEditor=[this]{setEasyMode(false,true);};
        // Load now so the Easy description never lags behind the choice.
        easyView->choosePreset=[this](int id){presets.setSelectedId(id,juce::sendNotificationSync);easyView->showPreset(presets.getSelectedId(),presets.getText(),presetOrigin);};
        easyView->stepPreset=[this](int step){(step<0?presetPrevious:presetNext).onClick();};
        easyView->toggleTuner=[this]{tunerOpen.onClick();};
        easyView->autoSet=[this]{autoSetController->start();};
        easyView->autoCancel=[this]{autoSetController->cancel();};
        easyView->autoUndo=[this]{autoSetController->undo();};
        easyView->chooseAutoOption=[this](int id){dynamicsPanel->chooseAutoOption(id);};
        auto room=std::make_unique<CabRoomOverview>(processor);
        auto* roomView=room.get();
        easyView->setRoom(std::move(room),[roomView](int lane){return roomView->getRigBounds(lane);});
        // A run may have started or finished in the full editor before this
        // view existed; show its current state, not the constructor default.
        {const auto& o=autoSetController->currentOptions();easyView->setAutoSetOptions(o.instrument,int(o.style),o.tone,o.finish,o.multiband);}
        publishAutoSetStatus();
        scrollContent.addChildComponent(*easyView);
    }
    easyMode=easy;
    // The full editor's tuner view follows the shared tuner parameter.
    tunerViewOpen=processor.parameters().getRawParameterValue("tuneron")->load()>.5f;
    canvas.setVisible(!easy);
    if(easyView) {
        easyView->setVisible(easy);
        if(easy){easyView->showPreset(presets.getSelectedId(),presets.getText(),presetOrigin);easyView->refreshNow();}
    }
    resized();updateModeUI();
}
void ChimeraEditor::publishAutoSetStatus()
{
    // The controller decides which phases are running; every view follows it.
    const auto& status=autoSetController->current();const bool busy=autoSetController->busy();
    const auto text=busy?status.text+"  "+juce::String(juce::roundToInt(status.progress*100))+"%":status.text;
    dynamicsPanel->setAutoSetStatus(text,busy,status.canUndo);
    if(easyView)easyView->setAutoSetStatus(text,busy,status.canUndo);
}
void ChimeraEditor::markPresetCustom()
{
    presetValues.clear();presets.setSelectedId(0,juce::dontSendNotification);presets.setText("CUSTOM",juce::dontSendNotification);presets.setTooltip("Edited sound or recalled reference. SAVE AS preserves the complete sound and embedded IRs.");
}

void ChimeraEditor::openCreativePanel()
{
    auto* full=new spectralforge::creativeUI::CreativePanel(processor);full->setLookAndFeel(&look);
    juce::DialogWindow::LaunchOptions options;options.content.setOwned(full);options.dialogTitle="CHIMERA / CREATIVE";options.dialogBackgroundColour=panel;
    options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.resizable=false;options.componentToCentreAround=this;trackDialog(options.launchAsync());
}

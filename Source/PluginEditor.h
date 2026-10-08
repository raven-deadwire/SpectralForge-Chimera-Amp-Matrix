#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "CabinetSelector.h"
#include "ReleaseSupport.h"
#include "PedalBoardPanel.h"
#include "AmpSelector.h"
#include "AmpNativePanel.h"
#include "PostNativePanel.h"
#include "EffectSelectionCatalog.h"

class CabRoomOverview;

class ChimeraLookAndFeel : public juce::LookAndFeel_V4 {
public:
    ChimeraLookAndFeel();
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    juce::Font getTextButtonFont(juce::TextButton&,int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};
class ChimeraEditor : public juce::AudioProcessorEditor, private juce::Timer,
                       public juce::FileDragAndDropTarget {
public:
    explicit ChimeraEditor(ChimeraProcessor&);
    ~ChimeraEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    uint64_t metadataRefreshCount() const noexcept {return metadataReads;}
private:
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void updateModeUI();
    void updateBandLabels();
    void updateHardwareStyles();
    void refreshVisibleState();
    void refreshMeters();
    void repaintDesign(juce::Rectangle<int>);
    bool stateDirty{true};
    unsigned presetCheckTick{};
    uint64_t metadataReads{};
    spectralforge::ui::Changed<std::array<double,4>> bandKey;
    spectralforge::ui::Changed<std::array<double,3>> tempoKey;
    spectralforge::ui::Changed<std::array<int,2>> inputMeterKey,outputMeterKey;
    spectralforge::ui::Changed<std::array<float,3>> tunerKey;
    spectralforge::ui::Changed<int> lowMeterKey;
    std::array<spectralforge::ui::Changed<std::array<int,2>>,3> postMeterKeys;
    spectralforge::ui::Changed<juce::String> cpuText,transportText,gateText,pitchText;
    void setupSlider(juce::Slider&,const juce::String&,const juce::String& suffix={});
    void loadIR(int);
    void openCabWorkspace(int,bool focusRequested=false);
    void chooseIR(int,bool folder);
    void browseIR(int,const juce::File&);
    void showInfo();
    void showSupport();
    void showDiagnostics();
    void trackDialog(juce::DialogWindow*);
    void markPresetCustom();
    void showIRDetails(int);
    void referenceFile(bool save);
    void layoutControls();
    void midiMenu();
    ChimeraProcessor& processor;
    // Declared before children so their images/peers die first. The last editor
    // releases native GPU artwork while JUCE and the DLL are still alive.
    juce::SharedResourcePointer<spectralforge::art::RasterBank> artwork;
    ChimeraLookAndFeel look;
    juce::Component canvas;
    PedalBoardPanel boardPanel;
    juce::TextButton gateLocation{"POST GATE"};
    juce::Label preEngineStatus;
    std::unique_ptr<BA> gateLocationAttachment;
    bool lastBoardEnabled{};
    juce::TooltipWindow tooltips{this,600};
    // Updates own a network worker. Ordinary editor open/close must not start
    // one or join one during host project teardown.
    std::shared_ptr<spectralforge::release::ReleaseSupport> support;
    juce::Image wordmark,brandEmblem;
    std::vector<juce::Component::SafePointer<juce::DialogWindow>> dialogs;
    std::vector<float> presetValues;
    juce::Label title,routingHelp,x1Label,x2Label,gateStatus,pitchStatus;
    juce::ComboBox mode,quality,scale,inputMode,presets,dualType,preOrder,gainOrder;
    juce::Slider doublerTime,tempo,dualBlend,dualFrequency;
    juce::Label dualLabel;
    juce::TextButton doublerOn{"DOUBLER"},midi{"MIDI"},tap{"TAP"},hostTempo{"HOST"},metronome{"CLICK"},presetPrevious{"<"},presetNext{">"},presetSave{"SAVE AS"},presetLoad{"OPEN"},delaySync{"SYNC"};
    std::unique_ptr<CA> inputModeAttachment,dualTypeAttachment,preOrderAttachment,gainOrderAttachment;
    std::array<std::unique_ptr<SA>,4> utilitySliders;
    std::array<std::unique_ptr<BA>,4> utilityButtons;
    juce::Slider x1,x2;
    std::unique_ptr<CA> ma,qa;
    std::unique_ptr<SA> a1,a2;
    std::array<juce::Slider,8> globalSliders;
    std::array<std::unique_ptr<SA>,8> globalAttachments;
    juce::TextButton gateOn{"GATE"},pitchOn{"TRANSPOSE"},tunerOn{"TUNER"},tunerMute{"AUTO MUTE"},info{"INFO"};
    std::array<std::unique_ptr<BA>,4> globalButtons;
    juce::TextButton compareA{"A"},compareB{"B"},copyAB{"COPY"},irLibraryButton{"IR LIBRARY"},rigsTab{"RIGS"},preTab{"PRE"},postTab{"POST"};
    std::unique_ptr<CabRoomOverview> cabRoom;
    juce::TextButton rigControls{"RIG CONTROLS"};
    bool showRigControls{};
    int page{1}; // Open the production five-slot PRE view; RIGS/POST remain one click away.
    juce::Slider lowComp,lowAmpMix;
    juce::Label lowCompLabel,diVoice,diNote;
    std::unique_ptr<SA> lowCompAttachment,lowAmpMixAttachment;
    struct FXUI {
        int lastModel{-1};
        spectralforge::ui::Changed<bool> power;
        juce::Label header,scope,description;
        juce::ComboBox model;
        std::unique_ptr<CA> modelAttachment;
        juce::TextButton enabled{"ON"},expand{"ALL"};
        std::array<juce::Slider,5> controls;
        std::array<juce::Label,5> labels;
        std::array<std::unique_ptr<SA>,5> attachments;
        std::unique_ptr<BA> button;
    };
    std::array<std::unique_ptr<PostNativePanel>,3> postPanels;
    std::array<FXUI,11> effects; // drive, delay, reverb, comp, filter, fuzz, boost, bus, preamp, EQ, chorus
    std::array<int,5> pedalOrder{4,3,5,6,0};
    bool lastEnvelopeFirst{true},lastBoostAfterDrive{};
    static constexpr std::array<int,6> rackOrder{7,8,9,10,1,2};
    struct LaneUI {
        int lastModel{-1};
        spectralforge::ui::Changed<std::array<uint64_t,6>> cabKey;
        spectralforge::IRMetadata metadata;
        juce::String status;
        juce::Label header,range,toneLabel,tonePivot,cabStatus,ampReference;
        AmpSelector amp;
        std::unique_ptr<AmpNativePanel> nativePanel;
        CabinetSelector cabType;
        juce::Slider drive,level,bass,lm,hm,treble,pres,res,bandTone,cabLow,cabHigh;
        std::array<juce::Label,8> knobLabels;
        juce::Label lowLabel,highLabel;
        juce::TextButton mute{"MUTE"},solo{"SOLO"},polarity{"INV"},cabOn{"CAB"},ampOn{"AMP"},load{"IRs"},details{"TAGS"};
        std::array<std::unique_ptr<SA>,8> sa;
        std::unique_ptr<SA> toneAttachment,lowAttachment,highAttachment;
        std::array<std::unique_ptr<BA>,5> buttons;
        std::array<juce::Slider*,8> controls() {return {&drive,&level,&bass,&lm,&hm,&treble,&pres,&res};}
    };
    std::array<LaneUI,3> lanes;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastMode{-1};
    bool lastDualCross{},lastTuner{};
};

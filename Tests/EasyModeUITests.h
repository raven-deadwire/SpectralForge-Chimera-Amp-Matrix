#pragma once
#include "NativeUITests.h"
#include "SignalPathUITests.h"
#include "EasyMode.h"
#include "AmpNativeDSP.h"
#include "FactoryPresets.h"
#include "PresetOrder.h"
#include <map>

namespace easyModeUITests {
using namespace nativeUITests;
inline float raw(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
inline void preference() {
    struct Temporary {
        juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-easy-preference",{},false);
        ~Temporary(){folder.deleteRecursively();}
    } temporary;
    using spectralforge::easy::Preference;
    const auto file=temporary.folder.getChildFile("interface.json");
    require(Preference::startsEasy(file),"First start must open Easy view");
    require(Preference::remember(false,file) && !Preference::startsEasy(file),"Full editor choice was not remembered");
    require(Preference::remember(true,file) && Preference::startsEasy(file),"Easy view choice was not remembered");
    require(file.replaceWithText("{broken") && Preference::startsEasy(file),"Unreadable preference must fall back to Easy view");
}
// RMS change of the real native amp DSP when one control moves 0.2 -> 0.8.
inline double response(spectralforge::AmpNativeState state,int control) {
    const auto render=[](const spectralforge::AmpNativeState& s) {
        spectralforge::AmpNativeDSP dsp;dsp.prepare(48000);dsp.set(s);dsp.reset();std::vector<float> out(9600);
        for(size_t n=0;n<out.size();++n) {
            const double t=double(n)/48000.;
            out[n]=dsp.tick(float(.1*(std::sin(2*juce::MathConstants<double>::pi*82.4*t)+.5*std::sin(2*juce::MathConstants<double>::pi*660*t)
                                     +.3*std::sin(2*juce::MathConstants<double>::pi*3100*t))),0);
        }
        return out;
    };
    state.values[(size_t)control]=.2f;const auto low=render(state);
    state.values[(size_t)control]=.8f;const auto high=render(state);
    double difference=0,level=0;
    for(size_t n=2048;n<low.size();++n){difference+=std::pow(double(high[n])-low[n],2);level+=double(high[n])*high[n]+double(low[n])*low[n];}
    return level>0 ? std::sqrt(difference/level) : 0;
}
inline void mapping() {
    // Every head x channel x input route: each Easy amp knob edits a visible
    // knob that the DSP actually plays through on that input path.
    using spectralforge::easy::Role;using spectralforge::easy::ampControl;
    int paths=0;
    for(int model=0;model<spectralforge::ampModelCount;++model) {
        const auto& panel=spectralforge::ampNativePanel(model);
        std::array<bool,spectralforge::easy::roleCount> found{};
        for(int channel=0;channel<(int)panel.channels.size();++channel)
            for(int route=0;route<std::max(1,(int)panel.routes.size());++route) {
                ++paths;
                std::cout<<"EASY MAP "<<spectralforge::ampInfo(model).name<<" / "<<panel.channels[(size_t)channel];
                if(panel.routes.size()>1)std::cout<<" / "<<panel.routes[(size_t)route];
                auto state=spectralforge::defaultAmpNativeState(model);state.channel=channel;state.inputRoute=route;
                for(int role=0;role<spectralforge::easy::roleCount;++role) {
                    const int control=ampControl(model,channel,route,Role(role));
                    if(control>=0) {
                        require(spectralforge::ampNativeControlVisible(model,control,channel)
                                && panel.controls[(size_t)control].kind==spectralforge::AmpNativeControlKind::knob,"Easy knob maps to a hidden or non-knob control");
                        const double change=response(state,control);
                        if(change<1.e-3)
                            throw std::runtime_error((juce::String("Easy ")+spectralforge::easy::roleNames[(size_t)role]+" does not change the sound of "
                                +spectralforge::ampInfo(model).name+" / "+panel.channels[(size_t)channel]
                                +(panel.routes.size()>1?" / "+juce::String(panel.routes[(size_t)route]):juce::String{})).toStdString());
                        found[(size_t)role]=true;
                    }
                    std::cout<<" | "<<spectralforge::easy::roleNames[(size_t)role]<<"="<<(control>=0?panel.controls[(size_t)control].label:"-");
                }
                std::cout<<'\n';
            }
        require(found[0] && found[1] && found[3],"A head has no Easy GAIN, BASS or TREBLE control");
    }
    // Input paths and two-section heads are pinned explicitly.
    const auto key=[](int model,int channel,int route,Role role){const int c=ampControl(model,channel,route,role);
        return c>=0 ? std::string(spectralforge::ampNativePanel(model).controls[(size_t)c].key) : std::string{};};
    for(int route:{0,1,4})require(key(1,0,route,Role::gain)=="hw.high_treble","Brit Edge HIGH TREBLE / JUMPED GAIN must edit LOUDNESS I");
    for(int route:{2,3})require(key(1,0,route,Role::gain)=="hw.normal","Brit Edge NORMAL input GAIN must edit LOUDNESS II");
    require(key(19,0,0,Role::gain)=="hw.normal","Monolith NORMAL input GAIN must edit NORMAL");
    for(int route:{1,2})require(key(19,0,route,Role::gain)=="hw.brilliant","Monolith BRILLIANT / JUMPED GAIN must edit BRILLIANT");
    require(key(7,0,0,Role::gain)=="hw.db751.gain" && key(7,0,0,Role::bass)=="hw.db751.bass" && key(7,0,0,Role::middle)=="hw.db751.mid"
            && key(7,0,0,Role::treble)=="hw.db751.treble","Modern Bass Easy knobs must all shape the amplifier section");
    std::cout<<"EASY MAP checked "<<paths<<" head/channel/input paths against the DSP\n";
}
// Counts host gesture begin/end per parameter, plus value writes.
struct HostEvents final:juce::AudioProcessorListener {
    std::map<int,int> open;int begins{},ends{},changes{};
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++changes;}
    void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int index) override {++open[index];++begins;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int index) override {--open[index];++ends;}
    bool balanced() const {for(const auto& [index,count]:open)if(count!=0)return false;return true;}
};
inline void useNativeHead(ChimeraProcessor& p,int model,int channel,int route=0) {
    set(p,spectralforge::ampNativeEnabledID(0),1);set(p,spectralforge::ampNativeModelID(0),float(model));
    set(p,spectralforge::ampNativeChannelID(0,model),float(channel));set(p,spectralforge::ampNativeRouteID(0,model),float(route));
}
inline std::unique_ptr<ChimeraEditor> openEditor(ChimeraProcessor& p) {
    auto editor=std::make_unique<ChimeraEditor>(p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    return editor;
}
inline void firstBinding() {
    // Chime 30 NORMAL has no EQ. Opening Easy view the first time must
    // disable the missing knobs, not leave them live and unbound.
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    useNativeHead(*p,8,0);
    auto editor=openEditor(*p);editor->setEasyMode(true);
    auto* view=find<EasyModeView>(*editor,"easyView");require(view!=nullptr,"Easy view missing");
    for(int role=0;role<spectralforge::easy::roleCount;++role) {
        const int control=spectralforge::easy::ampControl(8,0,0,spectralforge::easy::Role(role));
        auto* knob=find<juce::Slider>(*view,"easy.knob."+juce::String(spectralforge::easy::roleNames[(size_t)role]).toLowerCase());
        require(knob!=nullptr && knob->isEnabled()==(control>=0) && view->boundParameter(role).isEmpty()==(control<0),
                "An Easy amp knob missing on Chime 30 NORMAL is still live on first open");
    }
    // Legacy-engine project: the head panel is off, so the classic lane
    // controls are what the amp plays through.
    set(*p,spectralforge::ampNativeEnabledID(0),0);set(*p,"amp1",1);settle(60);
    require(view->boundParameter(0)=="drive1" && view->boundParameter(1)=="bass1" && view->boundParameter(2)=="lowmid1" && view->boundParameter(3)=="treble1",
            "Legacy-engine project did not bind the classic lane controls");
    auto* gain=find<juce::Slider>(*view,"easy.knob.gain");require(gain->isEnabled(),"Legacy GAIN disabled");
    gain->setValue(gain->proportionOfLengthToValue(.7),juce::sendNotificationSync);
    require(std::abs(p->parameters().getParameter("drive1")->getValue()-.7f)<.01f,"Legacy GAIN did not edit drive");
    editor.reset();p->releaseResources();
}
inline void routingAndGestures() {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    useNativeHead(*p,1,0,0);
    auto editor=openEditor(*p);editor->setEasyMode(true);settle(60);
    auto* view=find<EasyModeView>(*editor,"easyView");auto* gain=find<juce::Slider>(*view,"easy.knob.gain");
    const auto highTreble=spectralforge::ampNativeControlID(0,1,spectralforge::ampNativeControlIndex(1,"hw.high_treble"));
    const auto normal=spectralforge::ampNativeControlID(0,1,spectralforge::ampNativeControlIndex(1,"hw.normal"));
    require(view->boundParameter(0)==highTreble,"Brit Edge HIGH TREBLE input not bound to LOUDNESS I");
    // A host route change rebinds at once, without waiting for the poll.
    set(*p,spectralforge::ampNativeRouteID(0,1),2);settle(20);
    require(view->boundParameter(0)==normal,"Route change did not rebind GAIN promptly");
    set(*p,spectralforge::ampNativeRouteID(0,1),0);settle(20);

    // Routing changes during a drag: the held parameter keeps its gesture,
    // and every begin is matched by an end on the same parameter.
    HostEvents events;p->addListener(&events);
    {
        juce::Slider::ScopedDragNotification drag(*gain);
        gain->setValue(gain->proportionOfLengthToValue(.3),juce::sendNotificationSync);
        set(*p,spectralforge::ampNativeRouteID(0,1),3);settle(150);
        require(view->boundParameter(0)==highTreble,"GAIN rebound in the middle of a drag");
        gain->setValue(gain->proportionOfLengthToValue(.4),juce::sendNotificationSync);
    }
    settle(40);
    require(view->boundParameter(0)==normal,"GAIN did not rebind after the drag ended");
    require(events.begins>0 && events.begins==events.ends && events.balanced(),"Drag across a routing change left a host gesture open");
    require(std::abs(p->parameters().getParameter(highTreble)->getValue()-.4f)<.01f,"Held drag stopped editing its own parameter");

    // POST REVERB edits two parameters; both gestures must close.
    auto* reverb=find<juce::Slider>(*view,"easy.knob.reverb");
    {juce::Slider::ScopedDragNotification drag(*reverb);reverb->setValue(.2,juce::sendNotificationSync);reverb->setValue(0,juce::sendNotificationSync);}
    require(events.begins==events.ends && events.balanced(),"POST REVERB left a host gesture open");

    // Closing the editor mid-drag still ends the gesture it began.
    new juce::Slider::ScopedDragNotification(*gain); // Never ended by the slider: the editor goes away first.
    require(!events.balanced(),"Drag did not begin a host gesture");
    editor.reset();settle(20);
    require(events.balanced(),"Closing the editor during a drag left a host gesture open");
    p->removeListener(&events);
    p->releaseResources();
}
inline void laneTargets() {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    auto editor=openEditor(*p);editor->setEasyMode(true);settle(60);
    auto* view=find<EasyModeView>(*editor,"easyView");auto* note=find<juce::Label>(*view,"easy.targetNote");
    require(view->targetLane()==0 && note->getText().isEmpty(),"Classic target lane");
    set(*p,"mode",1);settle(60);
    require(view->targetLane()==0 && find<juce::Label>(*view,"easy.amp")->getText().contains("amp 1 of 2"),"Dual must target and name amp 1");
    set(*p,"mute1",1);settle(40);require(note->getText().contains("muted"),"Muted target lane not explained");set(*p,"mute1",0);
    set(*p,"solo2",1);settle(40);require(note->getText().contains("soloed"),"Soloed other lane not explained");set(*p,"solo2",0);
    set(*p,"dualtype",0);set(*p,"dualblend",1);settle(40);require(note->getText().contains("BLEND"),"Blend away from amp 1 not explained");set(*p,"dualblend",.5f);
    set(*p,"mode",2);settle(60);
    require(view->targetLane()==1 && find<juce::Label>(*view,"easy.amp")->getText().contains("MID amp"),"Matrix must target and name the MID amp");
    // Only the target lane's parameters move.
    const auto before=signalPathTests::values(*p);
    auto* gain=find<juce::Slider>(*view,"easy.knob.gain");
    if(gain->isEnabled()) {
        gain->setValue(gain->proportionOfLengthToValue(gain->valueToProportionOfLength(gain->getValue())>.5?.2:.8),juce::sendNotificationSync);
        const auto after=signalPathTests::values(*p);int changed=0;
        for(size_t i=0;i<after.size();++i)changed+=after[i]!=before[i];
        const auto id=view->boundParameter(0);
        require(changed==1 && (id.startsWith(spectralforge::ampNativePrefix(spectralforge::ampNativeContext(2,1))+"_") || id.contains("_c"+juce::String(spectralforge::ampNativeContext(2,1))+"_") || id=="drive2"),
                "Matrix GAIN edited something other than the MID amp");
    }
    editor.reset();p->releaseResources();
}
inline void fileDrop(const juce::File& directory) {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    const auto file=directory.getChildFile("easy-drop-ir.wav");
    {juce::WavAudioFormat format;auto output=file.createOutputStream();
     auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(output.release(),48000,1,24,{},0));
     require(writer!=nullptr,"IR fixture writer failed");juce::AudioBuffer<float> impulse(1,32);impulse.clear();impulse.setSample(0,0,1);
     require(writer->writeFromAudioSampleBuffer(impulse,0,32),"IR fixture write failed");}
    auto editor=openEditor(*p);editor->setEasyMode(true);settle(80);
    auto* view=find<EasyModeView>(*editor,"easyView");
    const auto dropOn=[&](juce::Component& target){const auto point=editor->getLocalPoint(&target,target.getLocalBounds().getCentre());
        editor->filesDropped(juce::StringArray{file.getFullPathName()},point.x,point.y);settle(40);};
    dropOn(*find(*view,"easy.knob.bass"));
    require(p->userIRName(0).isEmpty(),"An IR dropped on the Easy knobs replaced a cabinet");
    auto* rig=find(*view,"cabRoomRig1");require(rig!=nullptr && rig->isShowing(),"Easy cabinet missing");
    dropOn(*rig);
    require(p->userIRName(0).isNotEmpty(),"An IR dropped on the Easy cabinet was ignored");
    editor.reset();p->releaseResources();file.deleteFile();
}
inline void autoSetAndPresets() {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    auto editor=openEditor(*p);
    // A run started in the full editor is shown by an Easy view created afterwards.
    auto* start=find<juce::TextButton>(*editor,"commonDynamics.autoSet");require(start!=nullptr,"Full editor AUTO SET missing");
    start->onClick();settle(20);
    editor->setEasyMode(true);settle(40);
    auto* view=find<EasyModeView>(*editor,"easyView");auto* button=find<juce::TextButton>(*view,"easy.autoSet");
    require(view->autoSetBusy() && button->getButtonText()=="CANCEL","Easy view created during Auto Set does not show CANCEL");
    require(view->autoSetOptionsText()=="Auto / Rock","Easy Auto Set options not shown");
    button->onClick();settle(20);
    auto* status=find<juce::Label>(*view,"easy.autoStatus");
    require(!view->autoSetBusy() && button->getButtonText()=="AUTO SET" && status->getText().contains("cancelled"),"Easy CANCEL did not stop Auto Set");
    if(view->chooseAutoOption)view->chooseAutoOption(13);
    require(view->autoSetOptionsText()=="Auto / Metal","Easy Auto Set option not applied");

    // Tuning does not turn a factory preset into CUSTOM; editing does, and
    // Easy view keeps the original preset's name with an edit mark.
    auto* presets=find<juce::ComboBox>(*view,"easy.preset");
    const int id=spectralforge::presetDisplayOrder()[0]+1;presets->setSelectedId(id,juce::sendNotificationSync);settle(150);
    auto* tuner=find<juce::TextButton>(*view,"easy.tuner");signalPathTests::click(*tuner);settle(300);signalPathTests::click(*tuner);settle(300);
    require(presets->getSelectedId()==id,"Using the tuner turned the preset into CUSTOM");
    auto* volume=find<juce::Slider>(*view,"easy.knob.volume");volume->setValue(volume->getValue()-3,juce::sendNotificationSync);settle(400);
    require(presets->getSelectedId()==0 && presets->getText().contains(spectralforge::selectablePresetName(id-1)) && presets->getText().contains("edited"),
            "Edited preset lost its name in Easy view");
    editor.reset();p->releaseResources();
}
inline void startupPath() {
    // The host path: createEditor() applies the remembered view and the
    // first Easy frame is already bound.
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    std::unique_ptr<juce::AudioProcessorEditor> created(p->createEditor());
    auto* editor=dynamic_cast<ChimeraEditor*>(created.get());require(editor!=nullptr,"createEditor did not return the Chimera editor");
    require(editor->easyModeActive()==spectralforge::easy::Preference::startsEasy(),"createEditor ignored the remembered view");
    if(editor->easyModeActive()) {
        auto* view=find<EasyModeView>(*editor,"easyView");
        require(view!=nullptr && view->boundParameter(0).isNotEmpty(),"First Easy frame from createEditor is unbound");
    }
    created.reset();p->releaseResources();
}
inline void editorView(const juce::File& directory) {
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    require(!editor->easyModeActive() && find(*editor,"easyView")==nullptr,"A directly constructed editor must keep the full editor");
    auto* open=find<juce::TextButton>(*editor,"easyOpen");require(open && open->isShowing(),"EASY VIEW dock entry missing");
    const auto before=signalPathTests::values(*p);
    signalPathTests::Writes writes;p->addListener(&writes);
    editor->setEasyMode(true);settle(150);
    auto* view=find<EasyModeView>(*editor,"easyView");
    require(view && view->isShowing() && !find(*editor,"surface")->isVisible(),"Easy view did not replace the full editor");
    require(signalPathTests::values(*p)==before && writes.changes==0 && writes.gestures==0,"Opening Easy view changed the sound or sent host events");
    p->removeListener(&writes);
    auto* room=find(*view,"cabRoomOverview");require(room && room->isShowing(),"Easy view does not show the cabinet room");
    require(!room->isEnabled() && room->getComponentAt(room->getLocalBounds().getCentre())==nullptr,"Easy cabinet room must be view-only");
    {auto* head=find<juce::Button>(*room,"cabRoomHead1");require(head && !head->isEnabled(),"Easy room head is still clickable");
     const int windows=juce::TopLevelWindow::getNumTopLevelWindows();head->triggerClick();settle(150);
     require(juce::TopLevelWindow::getNumTopLevelWindows()==windows,"Easy room opened a detail window");}
    for(auto* child:view->getChildren())if(child->isVisible())inside(*view,*child,"Easy control outside its view");

    // Amp knobs edit the selected head's own parameters and follow amp changes.
    const auto gainID=view->boundParameter(0);require(gainID.isNotEmpty(),"Easy GAIN is not bound to the default head");
    auto* gain=find<juce::Slider>(*view,"easy.knob.gain");require(gain && gain->isEnabled(),"Easy GAIN knob missing");
    gain->setValue(gain->proportionOfLengthToValue(.8),juce::sendNotificationSync);
    auto* parameter=p->parameters().getParameter(gainID);
    require(std::abs(parameter->getValue()-.8f)<.01f,"Easy GAIN did not edit the head's gain");
    p->setAmpModel(0,spectralforge::niflheimrAmpModel);settle(250);
    require(view->boundParameter(0)!=gainID && view->boundParameter(0).contains("niflheimr"),"Easy knobs did not follow the selected amp");

    // REVERB is 0 while off; turning it up enables the existing reverb.
    set(*p,"reverbon",0);set(*p,"reverbmix",.15f);settle(100);
    auto* reverb=find<juce::Slider>(*view,"easy.knob.reverb");require(reverb && reverb->getValue()==0,"Reverb off must show REVERB at 0");
    require(raw(*p,"reverbon")<.5f,"Showing REVERB at 0 switched the reverb on");
    reverb->setValue(.3,juce::sendNotificationSync);
    require(raw(*p,"reverbon")>.5f && std::abs(raw(*p,"reverbmix")-.3f)<.002f,"Easy REVERB did not enable and set the reverb");
    reverb->setValue(0,juce::sendNotificationSync);
    require(raw(*p,"reverbon")<.5f && std::abs(raw(*p,"reverbmix")-.3f)<.002f,"Easy REVERB at 0 must switch off and keep the mix");
    set(*p,"reverbon",1);settle(50);
    require(std::abs(reverb->getValue()-.3)<.002,"Turning reverb on elsewhere did not update Easy REVERB");
    set(*p,"reverbon",0);settle(50);
    auto* volume=find<juce::Slider>(*view,"easy.knob.volume");volume->setValue(-12,juce::sendNotificationSync);
    require(std::abs(raw(*p,"output")+12)<.01f,"Easy VOLUME did not edit output gain");

    // Presets load through the full editor, so CUSTOM detection stays shared.
    auto* presets=find<juce::ComboBox>(*view,"easy.preset");require(presets,"Easy preset menu missing");
    presets->setSelectedId(spectralforge::presetDisplayOrder()[0]+1,juce::sendNotificationSync);settle(150);
    auto* description=find<juce::Label>(*view,"easy.presetDescription");
    require(description && description->getText().isNotEmpty() && !description->getText().startsWith("Your own"),"Easy preset did not load");
    signalPathTests::capture(*editor,directory,"easy-view");

    auto* tuner=find<juce::TextButton>(*view,"easy.tuner");signalPathTests::click(*tuner);settle(150);
    require(raw(*p,"tuneron")>.5f && find(*view,"precisionTuner")!=nullptr && !room->isVisible(),"Easy TUNER did not replace the room with the tuner");
    signalPathTests::capture(*editor,directory,"easy-view-tuner");
    signalPathTests::click(*tuner);settle(150);
    require(raw(*p,"tuneron")<.5f && find(*view,"precisionTuner")==nullptr && room->isShowing(),"Easy TUNER did not close the tuner");

    const auto edited=signalPathTests::values(*p);
    signalPathTests::Writes switching;p->addListener(&switching);
    editor->setEasyMode(false);settle(150);
    require(!view->isVisible() && find(*editor,"surface")->isVisible() && signalPathTests::values(*p)==edited,"Leaving Easy view changed the sound");
    editor->setEasyMode(true);settle(150);editor->setEasyMode(false);settle(150);
    require(switching.changes==0 && switching.gestures==0 && signalPathTests::values(*p)==edited,"Switching views sent parameter changes or gestures");
    p->removeListener(&switching);
    for(int scale:{75,150}){editor->setEasyMode(true);editor->setSize(1180*scale/100,780*scale/100);settle(60);signalPathTests::capture(*editor,directory,"easy-view-"+juce::String(scale));}
    editor.reset();p->releaseResources();
}
inline void run(const juce::File& directory) {
    preference();mapping();firstBinding();routingAndGestures();laneTargets();fileDrop(directory);autoSetAndPresets();startupPath();editorView(directory);
    std::cout<<"PASS Easy view: preference, DSP-checked head mapping per input route, first binding, legacy projects, gestures across routing changes, "
               "lane targets, file drops, Auto Set state and options, preset edits, startup path and sound-neutral switching\n";
}
}

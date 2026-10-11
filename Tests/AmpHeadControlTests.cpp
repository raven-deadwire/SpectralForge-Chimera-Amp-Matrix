#include "PluginEditor.h"
#include "CabRoom.h"
#include "NativeMessageLoop.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace spectralforge;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void settle(int ms=80){check(chimeraTest::dispatchFor(ms),"Native message loop stopped");}
template<class Predicate> void awaitUI(Predicate ready,const char* message) {
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000.;
    while(!ready() && juce::Time::getMillisecondCounterHiRes()<deadline)settle(20);
    check(ready(),message);
}
void set(ChimeraProcessor& p,const juce::String& id,float value){auto* q=p.parameters().getParameter(id);check(q,"Missing parameter");q->setValueNotifyingHost(q->convertTo0to1(value));}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
template<class T>T* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return dynamic_cast<T*>(&c);for(auto* child:c.getChildren())if(auto* result=find<T>(*child,id))return result;return nullptr;}
struct Host : juce::AudioProcessorParameter::Listener {
    int writes{},begins{},ends{},open{};
    void parameterValueChanged(int,float) override {++writes;}
    void parameterGestureChanged(int,bool begin) override {if(begin){++begins;++open;}else{++ends;--open;}}
};
juce::MouseEvent mouse(juce::Component& target,juce::Point<float> point,juce::Point<float> down,bool drag){return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&target,&target,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag};}
juce::Point<float> knobCentre(juce::Slider& slider){return slider.getLookAndFeel().getSliderLayout(slider).sliderBounds.getCentre().toFloat();}
void releaseDrag(juce::Slider& slider){const auto start=knobCentre(slider);slider.mouseUp(mouse(slider,start+juce::Point<float>(0,-25),start,true));}
void drag(juce::Slider& slider,bool finish=true){const auto start=knobCentre(slider),end=start+juce::Point<float>(0,-25);slider.mouseDown(mouse(slider,start,start,false));slider.mouseDrag(mouse(slider,end,start,true));if(finish)releaseDrag(slider);}
void screenshot(juce::Component& c,const juce::File& directory,const juce::String& name){auto stream=directory.getChildFile(name+".png").createOutputStream();check(stream!=nullptr,"Capture file");stream->setPosition(0);stream->truncate();juce::PNGImageFormat png;check(png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream),"Capture PNG");}
AmpHeadWorkspace* openWorkspace() {
    auto& desktop=juce::Desktop::getInstance();
    for(int i=0;i<desktop.getNumComponents();++i)if(auto* w=find<AmpHeadWorkspace>(*desktop.getComponent(i),"ampHeadWorkspace"))return w;
    return nullptr;
}
void closeWorkspace() {
    auto* full=openWorkspace();check(full,"No head workspace to close");
    find<juce::TextButton>(*full,"ampHeadBack")->triggerClick();
    awaitUI([]{return openWorkspace()==nullptr;},"Head Back did not close dialog");
}
void checkLabelFits(juce::Label& label,const char* message) {
    const auto font=label.getLookAndFeel().getLabelFont(label);
    juce::GlyphArrangement glyphs;glyphs.addLineOfText(font,label.getText(),0,0);
    const auto available=label.getBorderSize().subtractedFrom(label.getLocalBounds());
    check(glyphs.getBoundingBox(0,-1,true).getWidth()<=float(available.getWidth()),message);
}
void checkReadout(juce::Slider& slider,bool visible) {
    auto* face=dynamic_cast<NativeControlSlider*>(&slider);check(face,"Missing interaction-aware face slider");
    check(face->isReadoutVisible()==visible,visible?"Adjusting knob has no value readout":"Idle knob shows a value readout");
    bool found=false;
    for(auto* child:slider.getChildren())if(auto* value=dynamic_cast<juce::Label*>(child)) {
        found=true;check(value->isVisible()==visible,"Value label visibility disagrees with interaction");
        if(visible)checkLabelFits(*value,"Active face value is truncated");
    }
    check(found,"Missing editable face value label");
    auto* control=dynamic_cast<NativeControlView*>(slider.getParentComponent());check(control,"Missing native face control");
    check(control->label.isVisible()!=visible,"Parameter name and active value overlap");
}
void checkFaceLayout(AmpHeadWorkspace& view,int model) {
    auto* head=find<AmpHeadControlView>(view,"ampHead1");check(head,"Missing layout head");
    std::vector<juce::Rectangle<int>> cells;
    for(auto* child:head->getChildren())if(auto* c=dynamic_cast<NativeControlView*>(child)) {
        check(head->getLocalBounds().contains(c->getBounds()),"Face control clipped outside head");
        for(const auto& cell:cells)check(!cell.intersects(c->getBounds()),"Face controls overlap");
        cells.push_back(c->getBounds());
        check(c->label.getLookAndFeel().getLabelFont(c->label).getHeight()>=12.f,"Face label is too small to read");
        checkLabelFits(c->label,"Face parameter label is truncated");
        const auto layout=c->slider.getLookAndFeel().getSliderLayout(c->slider);
        check(layout.sliderBounds.getWidth()>=30 && layout.sliderBounds.getHeight()>=30,"Knob target too small");
        // Measure the actual value label with its installed font and insets.
        for(auto* value:c->slider.getChildren())if(auto* label=dynamic_cast<juce::Label*>(value)) {
            if(label->isVisible())checkLabelFits(*label,"Face value is truncated");
        }
    }
    check(cells.size()==size_t(model==1?6:14),"Wrong layout control count");
}
juce::TextEditor* valueEditor(juce::Slider& slider) {
    juce::TextEditor* input=nullptr;
    const auto scan=[&](auto&& self,juce::Component& c)->void{if(auto* t=dynamic_cast<juce::TextEditor*>(&c))input=t;for(auto* child:c.getChildren())self(self,*child);};
    scan(scan,slider);return input;
}
void focusKnob(juce::Slider& slider) {
    slider.getTopLevelComponent()->toFront(true);slider.grabKeyboardFocus();
    awaitUI([&]{return slider.hasKeyboardFocus(false);},"Face knob did not receive keyboard focus");
}
double relativeLuminance(juce::Colour colour) {
    const auto linear=[](double channel){return channel<=.04045?channel/12.92:std::pow((channel+.055)/1.055,2.4);};
    return .2126*linear(colour.getFloatRed())+.7152*linear(colour.getFloatGreen())+.0722*linear(colour.getFloatBlue());
}
double textContrast(juce::Colour text,juce::Colour background) {
    const auto ink=relativeLuminance(background.overlaidWith(text)),paper=relativeLuminance(background);
    return (juce::jmax(ink,paper)+.05)/(juce::jmin(ink,paper)+.05);
}
void checkEditorFocus(juce::Slider& slider) {
    awaitUI([&]{auto* editor=valueEditor(slider);return editor && editor->isShowing() && editor->hasKeyboardFocus(true);},"Precise entry lost visibility or keyboard focus");
    auto* editor=valueEditor(slider);const auto background=editor->findColour(juce::TextEditor::backgroundColourId);
    check(textContrast(editor->findColour(juce::TextEditor::textColourId),background)>=4.5,"Numeric-entry text contrast is below 4.5:1");
    const auto selection=background.overlaidWith(editor->findColour(juce::TextEditor::highlightColourId));
    check(textContrast(editor->findColour(juce::TextEditor::highlightedTextColourId),selection)>=4.5,"Selected numeric-entry text contrast is below 4.5:1");
    const auto border=editor->getBorder();
    check(float(editor->getHeight()-border.getTop()-border.getBottom()-editor->getTopIndent())>=editor->getFont().getHeight(),"Numeric-entry font is clipped by editor border or indent");
}
void numeric(juce::Slider& slider,const juce::String& text) {
    if(!slider.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)))slider.showTextBox();
    auto* input=valueEditor(slider);check(input,"Value editor missing");
    input->setText(text,false);slider.hideTextBox(false);
}
void captureAdjustment(AmpHeadWorkspace& view,const juce::String& id,const juce::File& directory,const juce::String& name) {
    auto* slider=find<juce::Slider>(view,id);check(slider,"Capture knob missing");checkReadout(*slider,false);
    const auto knob=slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds;
    drag(*slider,false);checkReadout(*slider,true);
    check(slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds==knob,"Readout moved the knob during adjustment");
    screenshot(view,directory,name);
    releaseDrag(*slider);checkReadout(*slider,false);
}
}
int main(int argc,char** argv){try {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc>1 && juce::String(argv[1])=="--software-preview") {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("head-software-preview");folder.createDirectory();
        auto preview=std::make_unique<ChimeraProcessor>();
        for(int model:{1,niflheimrAmpModel}) {
            preview->setAmpModel(0,model);
            if(model==niflheimrAmpModel) {
                set(*preview,ampNativeControlID(0,model,int(niflheimr::Control::midFrequency),0),2500);
                set(*preview,ampNativeControlID(0,model,int(niflheimr::Control::blend),0),1);
            }
            AmpHeadWorkspace surface(*preview,0);surface.setVisible(true);
            for(const auto size:{juce::Point<int>{1000,642},{1180,650},{1440,780},{1680,910}}) {
                surface.setSize(size.x,size.y);checkFaceLayout(surface,model);
                screenshot(surface,folder,"model"+juce::String(model)+"-"+juce::String(size.x));
                const int index=model==niflheimrAmpModel?int(niflheimr::Control::midFrequency):0;
                auto* active=find<NativeControlSlider>(surface,ampNativeControlID(0,model,index,0));check(active,"Software preview knob missing");
                active->beginValueAdjustment();checkReadout(*active,true);
                screenshot(surface,folder,"model"+juce::String(model)+"-"+juce::String(size.x)+"-active");active->endValueAdjustment();checkReadout(*active,false);
            }
        }
        preview->releaseResources();std::cout<<"SOFTWARE PREVIEW ONLY: no native peer or interaction validation\n";return 0;
    }
    check(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()!=nullptr,"Native display required");
    const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>1?argv[1]:"amp-head-snapshots");directory.createDirectory();
    auto p=std::make_unique<ChimeraProcessor>();
    Host all;for(auto* parameter:p->getParameters())parameter->addListener(&all);
    int cases=0;
    for(int mode=0;mode<3;++mode)for(int dual=0;dual<(mode==1?2:1);++dual)for(int lane=0;lane<=mode;++lane)for(int model:{1,niflheimrAmpModel}) {
        set(*p,"mode",float(mode));set(*p,"dualtype",float(dual));p->setAmpModel(lane,model);
        auto view=std::make_unique<AmpHeadWorkspace>(*p,lane);view->addToDesktop(juce::ComponentPeer::windowIsTemporary);view->setVisible(true);settle();
        auto* head=find<AmpHeadControlView>(*view,"ampHead"+juce::String(lane+1));check(head && head->displayedContext()==ampNativeContext(mode,lane),"Wrong amp context");
        const int expected=model==1?6:14;check(int(head->controlCount())==expected,"Missing face controls");
        const int previousWrites=all.writes,previousBegins=all.begins;view->setSize(1180,650);head->refresh();settle();
        check(all.writes==previousWrites && all.begins==previousBegins,"Opening/resizing/refresh mutated audio state");
        for(int channel=0;channel<(model==1?1:5);++channel) {
            if(model!=1){auto* button=find<juce::TextButton>(*view,"ampHeadChannel"+juce::String(lane+1)+"_"+juce::String(channel));check(button,"Channel switch missing");button->triggerClick();settle();}
            check(p->selectedAmpChannel(lane)==channel,"Channel switch failed");
            std::set<juce::String> ids;
            for(const auto& knob:ampHead::knobs(model)) {
                const int index=ampHead::controlIndex(model,knob.key);const auto id=ampNativeControlID(ampNativeContext(mode,lane),model,index,channel);
                check(ids.insert(id).second,"Duplicate active binding");auto* slider=find<juce::Slider>(*head,id);check(slider && slider->isShowing(),"Missing visible knob");
                auto* parameter=p->parameters().getParameter(id);Host host;parameter->addListener(&host);
                checkReadout(*slider,false);const auto knobBounds=slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds;
                const float old=get(*p,id);drag(*slider,false);checkReadout(*slider,true);
                check(slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds==knobBounds,"Readout moved knob while dragging");
                releaseDrag(*slider);checkReadout(*slider,false);
                check(get(*p,id)!=old,"Real slider drag did not edit parameter");check(host.begins==1 && host.ends==1 && host.open==0,"Drag gesture not balanced");
                set(*p,id,parameter->convertFrom0to1(.37f));settle(2);check(std::abs(slider->getValue()-get(*p,id))<.003,"Host automation not displayed");checkReadout(*slider,false);
                if(index==0){numeric(*slider,juce::String(3.0+channel));checkReadout(*slider,false);check(std::abs(get(*p,id)-(.3f+float(channel)*.1f))<.002,"Hardware value conversion failed");
                    const auto prior=get(*p,id);check(slider->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),"Keyboard adjustment ignored");check(get(*p,id)>prior,"Keyboard adjustment not bound");checkReadout(*slider,true);
                    if(mode==0 && channel==0)awaitUI([&]{return !dynamic_cast<NativeControlSlider*>(slider)->isReadoutVisible();},"Keyboard value readout did not disappear");
                    else head->finishInteraction();
                    checkReadout(*slider,false);check(host.open==0 && host.begins==host.ends,"Numeric/keyboard gesture not balanced");}
                parameter->removeListener(&host);
            }
        }
        if(model==niflheimrAmpModel)for(int c=0;c<5;++c)check(std::abs(get(*p,ampNativeControlID(ampNativeContext(mode,lane),model,0,c))-(.301f+float(c)*.1f))<.002,"Independent channel memory lost");
        if(model==1){auto* route=find<juce::ComboBox>(*view,"ampHeadInput"+juce::String(lane+1));for(int i=1;i<=5;++i){route->setSelectedId(i,juce::sendNotificationSync);check(p->selectedAmpNativeRoute(lane)==i-1,"Input route failed");if(mode==0){settle();screenshot(*view,directory,"brit-input-route-"+juce::String(i));}}}
        screenshot(*view,directory,"head-mode"+juce::String(mode)+"-dual"+juce::String(dual)+"-lane"+juce::String(lane)+"-model"+juce::String(model));
        view.reset();check(all.open==0,"Open gesture after window destruction");++cases;
    }
    // Inspect actual enlarged workspaces at four window sizes, independently
    // of the main editor's scale. Exercise the widest frequency/percent values.
    set(*p,"mode",0);
    for(int model:{1,niflheimrAmpModel}) {
        p->setAmpModel(0,model);p->setAmpChannel(0,0);
        auto layout=std::make_unique<AmpHeadWorkspace>(*p,0);layout->addToDesktop(juce::ComponentPeer::windowIsTemporary);layout->setVisible(true);settle();
        if(model==niflheimrAmpModel) {
            set(*p,ampNativeControlID(0,model,int(niflheimr::Control::midFrequency),0),2500);
            set(*p,ampNativeControlID(0,model,int(niflheimr::Control::blend),0),1);
        }
        for(const auto size:{juce::Point<int>{1000,642},{1180,650},{1440,780},{1680,910}}) {
            layout->setSize(size.x,size.y);settle();checkFaceLayout(*layout,model);
            screenshot(*layout,directory,"workspace-"+juce::String(size.x)+"-model"+juce::String(model));
            const int index=model==niflheimrAmpModel?int(niflheimr::Control::midFrequency):0;
            captureAdjustment(*layout,ampNativeControlID(0,model,index,0),directory,"workspace-active-"+juce::String(size.x)+"-model"+juce::String(model));
        }
        layout->setSize(1000,642);settle();
        const auto editID=ampNativeControlID(0,model,0,0);auto* entry=find<juce::Slider>(*layout,editID);check(entry,"Precise-entry knob missing");
        focusKnob(*entry);
        check(entry->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Return did not open precise entry");checkReadout(*entry,true);
        checkEditorFocus(*entry);
        juce::Component::SafePointer<juce::TextEditor> editor=valueEditor(*entry);check(editor!=nullptr,"Precise-entry editor missing");
        editor->setText("6.4",false);settle(120);
        check(editor!=nullptr && valueEditor(*entry)==editor.getComponent() && editor->getText()=="6.4","Periodic layout destroyed numeric entry");
        check(editor->isShowing() && editor->hasKeyboardFocus(true),"Periodic layout hid or defocused numeric entry");
        screenshot(*layout,directory,"workspace-entry-1000-model"+juce::String(model));
        entry->hideTextBox(false);checkReadout(*entry,false);check(std::abs(get(*p,editID)-.64f)<.002,"Precise entry did not commit");
        const auto committed=get(*p,editID);const int entryWrites=all.writes;
        const auto click=knobCentre(*entry);entry->mouseDown(mouse(*entry,click,click,false));entry->mouseDoubleClick(mouse(*entry,click,click,false));entry->mouseUp(mouse(*entry,click,click,false));checkReadout(*entry,true);
        check(valueEditor(*entry),"Double-click did not open precise entry");valueEditor(*entry)->setText("9.9",false);entry->hideTextBox(true);
        checkReadout(*entry,false);check(get(*p,editID)==committed,"Cancelling numeric entry changed the knob value");
        check(all.writes==entryWrites,"Opening/cancelling numeric entry emitted host parameter writes");
        focusKnob(*entry);check(entry->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Resize entry did not open");checkEditorFocus(*entry);
        juce::Component::SafePointer<juce::TextEditor> resizedEditor=valueEditor(*entry);check(resizedEditor!=nullptr,"Resize value editor missing");
        resizedEditor->setText("9.1",false);const int resizeWrites=all.writes,resizeBegins=all.begins;
        layout->setSize(1180,650);check(resizedEditor==nullptr,"Resize retained obsolete value editor");checkReadout(*entry,false);
        check(get(*p,editID)==committed && all.writes==resizeWrites && all.begins==resizeBegins,"Resize committed unfinished numeric entry");
        layout->setSize(1000,642);checkFaceLayout(*layout,model);checkReadout(*entry,false);
        if(model==niflheimrAmpModel)captureAdjustment(*layout,ampNativeControlID(0,model,int(niflheimr::Control::blend),0),directory,"workspace-active-blend-1000-model25");
        if(model==niflheimrAmpModel)for(int channel=0;channel<5;++channel) {
            auto* named=find<juce::TextButton>(*layout,"ampHeadNamedChannel1_"+juce::String(channel));
            check(named && named->getButtonText().contains(juce::String::fromUTF8(niflheimr::channelNames[channel])),"Channel name missing");
            named->triggerClick();settle();check(p->selectedAmpChannel(0)==channel && named->getToggleState(),"Named channel selection failed");
            check(find<juce::TextButton>(*layout,"ampHeadChannel1_"+juce::String(channel))->getToggleState(),"Physical channel indicator stale");
            screenshot(*layout,directory,"named-channel-"+juce::String(channel));
        }
    }
    // Context isolation and per-channel recall, then same-model A/B, project,
    // factory preset, interrupted gesture and head/detail bidirectional sync.
    set(*p,"mode",0);p->setAmpModel(0,niflheimrAmpModel);p->setAmpChannel(0,0);
    auto view=std::make_unique<AmpHeadWorkspace>(*p,0);view->addToDesktop(juce::ComponentPeer::windowIsTemporary);view->setVisible(true);settle();
    const auto id=ampNativeControlID(0,niflheimrAmpModel,0,0);auto* slider=find<juce::Slider>(*view,id);check(slider,"Recall knob missing");
    std::array<float,6> before{};for(int c=0;c<6;++c)before[size_t(c)]=get(*p,ampNativeControlID(c,niflheimrAmpModel,0,0));
    numeric(*slider,"7.1");for(int c=1;c<6;++c)check(get(*p,ampNativeControlID(c,niflheimrAmpModel,0,0))==before[size_t(c)],"Edited an inactive context");
    const auto savedValue=get(*p,id);p->copyComparison();juce::MemoryBlock saved;p->getStateInformation(saved);
    find<juce::TextButton>(*view,"ampHeadCompareB")->triggerClick();settle();set(*p,id,.2f);find<juce::TextButton>(*view,"ampHeadCompareA")->triggerClick();settle();check(get(*p,id)==savedValue && p->comparisonSlot()==0,"A/B recall value failed");
    slider=find<juce::Slider>(*view,id);check(std::abs(slider->getValue()-savedValue)<.002,"A/B face stale");
    Host host;auto* parameter=p->parameters().getParameter(id);parameter->addListener(&host);
    drag(*slider,false);p->setStateInformation(saved.getData(),int(saved.getSize()));
    // A late drag event before the timer refresh must NOT overwrite restored values.
    slider->mouseDrag(mouse(*slider,{15,-60},{15,15},true));check(get(*p,id)==savedValue,"Stale drag overwrote restored project");checkReadout(*slider,false);settle();check(host.open==0,"Restore left gesture open");
    p->loadFactoryPreset(0);settle();p->setStateInformation(saved.getData(),int(saved.getSize()));settle();check(get(*p,id)==savedValue,"Factory/project restoration failed");
    auto* detailToggle=find<juce::TextButton>(*view,"ampHeadDetails");detailToggle->triggerClick();settle();
    auto* details=find<::AmpNativePanel>(*view,"ampNativePanel1");check(details && details->isShowing(),"Detailed fallback unavailable");
    auto* detailKnob=find<juce::Slider>(*details,id);check(detailKnob,"Detailed control missing");numeric(*detailKnob,"4.2");
    detailToggle->triggerClick();settle();slider=find<juce::Slider>(*view,id);check(std::abs(slider->getValue()-.42)<.002,"Detail-to-head synchronization failed");
    drag(*slider,false);p->setAmpChannel(0,1);awaitUI([&]{return host.open==0;},"Channel switch left gesture open");
    auto* nextChannel=find<juce::Slider>(*find<AmpHeadControlView>(*view,"ampHead1"),ampNativeControlID(0,niflheimrAmpModel,0,1));check(nextChannel,"Channel switch did not rebind readout");
    checkReadout(*nextChannel,false);p->setAmpChannel(0,0);
    awaitUI([&]{return find<juce::Slider>(*find<AmpHeadControlView>(*view,"ampHead1"),id)!=nullptr;},"Channel return did not rebind");
    slider=find<juce::Slider>(*view,id);const auto beforeCancelledEdit=get(*p,id);
    focusKnob(*slider);
    check(slider->keyPressed(juce::KeyPress(juce::KeyPress::F2Key)),"F2 did not open precise entry");checkReadout(*slider,true);
    checkEditorFocus(*slider);
    juce::Component::SafePointer<juce::TextEditor> cancelledEditor=valueEditor(*slider);check(cancelledEditor!=nullptr,"Interrupted value editor missing");cancelledEditor->setText("8.8",false);
    p->setAmpChannel(0,1);awaitUI([&]{return cancelledEditor==nullptr;},"Channel switch retained stale numeric editor");
    check(get(*p,id)==beforeCancelledEdit && host.open==0,"Channel switch committed stale numeric entry");p->setAmpChannel(0,0);
    awaitUI([&]{return find<juce::Slider>(*find<AmpHeadControlView>(*view,"ampHead1"),id)!=nullptr;},"Numeric channel return did not rebind");
    slider=find<juce::Slider>(*view,id);drag(*slider,false);set(*p,"mode",1);awaitUI([&]{return host.open==0;},"Mode change left gesture open");set(*p,"mode",0);
    awaitUI([&]{return find<juce::Slider>(*find<AmpHeadControlView>(*view,"ampHead1"),id)!=nullptr;},"Classic return did not rebind");
    slider=find<juce::Slider>(*view,id);drag(*slider,false);view.reset();check(host.open==0,"Destruction left gesture open");parameter->removeListener(&host);
    // Native editor scale + room head/cabinet navigation use independent targets.
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle();
    for(int scale:{75,100,125,150}){editor->setSize(1180*scale/100,780*scale/100);settle();screenshot(*editor,directory,"editor-"+juce::String(scale));
        const int writes=all.writes,begins=all.begins;auto* open=find<juce::TextButton>(*editor,"ampHeadOpen1");check(open && open->isShowing(),"Classic head entry missing");open->triggerClick();settle(150);
        AmpHeadWorkspace* full=nullptr;auto& desktop=juce::Desktop::getInstance();for(int i=0;i<desktop.getNumComponents();++i)if(auto* w=find<AmpHeadWorkspace>(*desktop.getComponent(i),"ampHeadWorkspace"))full=w;
        check(full && full->isShowing(),"Head did not open native enlargement");screenshot(*full,directory,"focus-from-editor-"+juce::String(scale));
        closeWorkspace();check(all.writes==writes && all.begins==begins,"Head open/back changed audio state");
    }
    for(int mode:{1,2}){set(*p,"mode",float(mode));settle();auto* room=find<CabRoomOverview>(*editor,"cabRoomOverview");check(room,"Room missing");
        // Keep production callbacks installed: prove the real destination,
        // selected bank and return path, not merely a substituted callback.
        const int writes=all.writes,begins=all.begins;
        for(int lane=0;lane<=mode;++lane){auto* entry=find<juce::Button>(*room,"cabRoomHead"+juce::String(lane+1));auto* cab=find<juce::Button>(*room,"cabRoomRig"+juce::String(lane+1));check(entry && cab,"Separate room targets missing");
            entry->triggerClick();awaitUI([]{return openWorkspace()!=nullptr;},"Room head did not open workspace");
            auto* full=openWorkspace();auto* target=find<AmpHeadControlView>(*full,"ampHead"+juce::String(lane+1));
            check(target && target->displayedContext()==ampNativeContext(mode,lane),"Room opened wrong amplifier bank");
            screenshot(*full,directory,"room-to-head-mode"+juce::String(mode)+"-lane"+juce::String(lane));closeWorkspace();
            int selectedCab=-1;const auto callback=room->onCabinetSelected;
            room->onCabinetSelected=[&](int selected){selectedCab=selected;};cab->triggerClick();settle(10);
            check(selectedCab==lane,"Wrong room cabinet lane");room->onCabinetSelected=callback;
        }
        check(all.writes==writes && all.begins==begins,"Room navigation changed sound");screenshot(*editor,directory,"room-"+juce::String(mode));}
    editor.reset();
    set(*p,"mode",2);auto cabinet=std::make_unique<CabWorkspace>(*p,1,true);cabinet->addToDesktop(juce::ComponentPeer::windowIsTemporary);cabinet->setVisible(true);settle();
    auto* cabHead=find<spectralforge::cabHead::View>(*cabinet,"cabPerspectiveHead");check(cabHead && cabHead->getWantsKeyboardFocus(),"Focused CAB head entry missing");
    const int cabWrites=all.writes;check(cabHead->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Focused CAB head keyboard entry failed");awaitUI([]{return openWorkspace()!=nullptr;},"Focused CAB head did not open workspace");
    auto* focusedTarget=find<AmpHeadControlView>(*openWorkspace(),"ampHead2");check(focusedTarget && focusedTarget->displayedContext()==ampNativeContext(2,1),"CAB opened wrong head bank");
    screenshot(*openWorkspace(),directory,"cab-to-head-matrix-mid");closeWorkspace();check(cabinet->focusedRig()==1,"CAB return lost focused rig");
    check(all.writes==cabWrites,"CAB head entry changed sound");cabinet.reset();settle();
    for(auto* parameter2:p->getParameters())parameter2->removeListener(&all);check(all.open==0,"Unbalanced host gestures");p->releaseResources();
    std::cout<<"PASS: "<<cases<<" route/model cases; 6 contexts; all 5 Niflheimr channels; native drag, transient values, numeric entry/cancel, keyboard timeout, automation, A/B/project/factory recall, interrupted gestures, room navigation and 4 editor scales\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

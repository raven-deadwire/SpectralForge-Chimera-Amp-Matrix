#include "PluginEditor.h"
#include "CabRoom.h"
#include "NativeMessageLoop.h"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace spectralforge;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void settle(int ms=80){check(chimeraTest::dispatchFor(ms),"Native message loop stopped");}
void set(ChimeraProcessor& p,const juce::String& id,float value){auto* q=p.parameters().getParameter(id);check(q,"Missing parameter");q->setValueNotifyingHost(q->convertTo0to1(value));}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
template<class T>T* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return dynamic_cast<T*>(&c);for(auto* child:c.getChildren())if(auto* result=find<T>(*child,id))return result;return nullptr;}
struct Host : juce::AudioProcessorParameter::Listener {
    int writes{},begins{},ends{},open{};
    void parameterValueChanged(int,float) override {++writes;}
    void parameterGestureChanged(int,bool begin) override {if(begin){++begins;++open;}else{++ends;--open;}}
};
juce::MouseEvent mouse(juce::Component& target,juce::Point<float> point,juce::Point<float> down,bool drag){return {juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&target,&target,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag};}
void drag(juce::Slider& slider,bool finish=true){const auto start=juce::Point<float>(15,15),end=start+juce::Point<float>(0,-25);slider.mouseDown(mouse(slider,start,start,false));slider.mouseDrag(mouse(slider,end,start,true));if(finish)slider.mouseUp(mouse(slider,end,start,true));}
void screenshot(juce::Component& c,const juce::File& directory,const juce::String& name){auto stream=directory.getChildFile(name+".png").createOutputStream();check(stream!=nullptr,"Capture file");stream->setPosition(0);stream->truncate();juce::PNGImageFormat png;check(png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream),"Capture PNG");}
void numeric(juce::Slider& slider,const juce::String& text){slider.showTextBox();juce::TextEditor* input=nullptr;const auto scan=[&](auto&& self,juce::Component& c)->void{if(auto* t=dynamic_cast<juce::TextEditor*>(&c))input=t;for(auto* child:c.getChildren())self(self,*child);};scan(scan,slider);check(input,"Value editor missing");input->setText(text,false);slider.hideTextBox(false);}
}
int main(int argc,char** argv){try {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc>1 && juce::String(argv[1])=="--software-preview") {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("head-software-preview");folder.createDirectory();
        auto preview=std::make_unique<ChimeraProcessor>();
        for(int model:{1,niflheimrAmpModel}){preview->setAmpModel(0,model);AmpHeadWorkspace surface(*preview,0);surface.setVisible(true);screenshot(surface,folder,"model"+juce::String(model));}
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
                const float old=get(*p,id);drag(*slider);check(get(*p,id)!=old,"Real slider drag did not edit parameter");check(host.begins==1 && host.ends==1 && host.open==0,"Drag gesture not balanced");
                set(*p,id,parameter->convertFrom0to1(.37f));settle(2);check(std::abs(slider->getValue()-get(*p,id))<.003,"Host automation not displayed");
                if(index==0){numeric(*slider,juce::String(3.0+channel));check(std::abs(get(*p,id)-(.3f+float(channel)*.1f))<.002,"Hardware value conversion failed");
                    const auto prior=get(*p,id);check(slider->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),"Keyboard adjustment ignored");check(get(*p,id)>prior,"Keyboard adjustment not bound");}
                parameter->removeListener(&host);
            }
        }
        if(model==niflheimrAmpModel)for(int c=0;c<5;++c)check(std::abs(get(*p,ampNativeControlID(ampNativeContext(mode,lane),model,0,c))-(.301f+float(c)*.1f))<.002,"Independent channel memory lost");
        if(model==1){auto* route=find<juce::ComboBox>(*view,"ampHeadInput"+juce::String(lane+1));for(int i=1;i<=5;++i){route->setSelectedId(i,juce::sendNotificationSync);check(p->selectedAmpNativeRoute(lane)==i-1,"Input route failed");}}
        screenshot(*view,directory,"head-mode"+juce::String(mode)+"-dual"+juce::String(dual)+"-lane"+juce::String(lane)+"-model"+juce::String(model));
        view.reset();check(all.open==0,"Open gesture after window destruction");++cases;
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
    slider->mouseDrag(mouse(*slider,{15,-60},{15,15},true));check(get(*p,id)==savedValue,"Stale drag overwrote restored project");settle();check(host.open==0,"Restore left gesture open");
    p->loadFactoryPreset(0);settle();p->setStateInformation(saved.getData(),int(saved.getSize()));settle();check(get(*p,id)==savedValue,"Factory/project restoration failed");
    auto* detailToggle=find<juce::TextButton>(*view,"ampHeadDetails");detailToggle->triggerClick();settle();
    auto* details=find<::AmpNativePanel>(*view,"ampNativePanel1");check(details && details->isShowing(),"Detailed fallback unavailable");
    auto* detailKnob=find<juce::Slider>(*details,id);check(detailKnob,"Detailed control missing");numeric(*detailKnob,"4.2");
    detailToggle->triggerClick();settle();slider=find<juce::Slider>(*view,id);check(std::abs(slider->getValue()-.42)<.002,"Detail-to-head synchronization failed");
    drag(*slider,false);p->setAmpChannel(0,1);settle();check(host.open==0,"Channel switch left gesture open");p->setAmpChannel(0,0);settle();
    slider=find<juce::Slider>(*view,id);drag(*slider,false);set(*p,"mode",1);settle();check(host.open==0,"Mode change left gesture open");set(*p,"mode",0);settle();
    slider=find<juce::Slider>(*view,id);drag(*slider,false);view.reset();check(host.open==0,"Destruction left gesture open");parameter->removeListener(&host);
    // Native editor scale + room head/cabinet navigation use independent targets.
    auto editor=std::make_unique<ChimeraEditor>(*p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle();
    for(int scale:{75,100,125,150}){editor->setSize(1180*scale/100,780*scale/100);settle();screenshot(*editor,directory,"editor-"+juce::String(scale));
        const int writes=all.writes,begins=all.begins;auto* open=find<juce::TextButton>(*editor,"ampHeadOpen1");check(open && open->isShowing(),"Classic head entry missing");open->triggerClick();settle(150);
        AmpHeadWorkspace* full=nullptr;auto& desktop=juce::Desktop::getInstance();for(int i=0;i<desktop.getNumComponents();++i)if(auto* w=find<AmpHeadWorkspace>(*desktop.getComponent(i),"ampHeadWorkspace"))full=w;
        check(full && full->isShowing(),"Head did not open native enlargement");screenshot(*full,directory,"focus-from-editor-"+juce::String(scale));
        find<juce::TextButton>(*full,"ampHeadBack")->triggerClick();settle(100);check(all.writes==writes && all.begins==begins,"Head open/back changed audio state");
    }
    for(int mode:{1,2}){set(*p,"mode",float(mode));settle();auto* room=find<CabRoomOverview>(*editor,"cabRoomOverview");check(room,"Room missing");int selectedHead=-1,selectedCab=-1;room->onAmplifierSelected=[&](int lane){selectedHead=lane;};room->onCabinetSelected=[&](int lane){selectedCab=lane;};
        const int writes=all.writes,begins=all.begins;for(int lane=0;lane<=mode;++lane){auto* head=find<juce::Button>(*room,"cabRoomHead"+juce::String(lane+1));auto* cab=find<juce::Button>(*room,"cabRoomRig"+juce::String(lane+1));check(head && cab,"Separate room targets missing");head->triggerClick();settle(10);check(selectedHead==lane,"Wrong room head lane");cab->triggerClick();settle(10);check(selectedCab==lane,"Wrong room cab lane");}
        check(all.writes==writes && all.begins==begins,"Room navigation changed sound");screenshot(*editor,directory,"room-"+juce::String(mode));}
    editor.reset();
    set(*p,"mode",2);auto cabinet=std::make_unique<CabWorkspace>(*p,1,true);cabinet->addToDesktop(juce::ComponentPeer::windowIsTemporary);cabinet->setVisible(true);settle();
    auto* cabHead=find<spectralforge::cabHead::View>(*cabinet,"cabPerspectiveHead");check(cabHead && cabHead->getWantsKeyboardFocus(),"Focused CAB head entry missing");
    const int cabWrites=all.writes;check(cabHead->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Focused CAB head keyboard entry failed");settle(100);check(all.writes==cabWrites,"CAB head entry changed sound");cabinet.reset();settle();
    for(auto* parameter2:p->getParameters())parameter2->removeListener(&all);check(all.open==0,"Unbalanced host gestures");p->releaseResources();
    std::cout<<"PASS: "<<cases<<" route/model cases; 6 contexts; all 5 Niflheimr channels; native drag, values, automation, A/B/project/factory recall, interrupted gestures, room navigation and 4 editor scales\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

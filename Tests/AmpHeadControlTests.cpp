#include "PluginEditor.h"
#include "CabRoom.h"
#include "CabHeadAppearanceTests.h"
#include "NativeMessageLoop.h"
#include "RigViewTestHelpers.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace spectralforge;
namespace {
std::string testContext;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(std::string(text)+(testContext.empty()?std::string{}:" ["+testContext+"]"));}
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
void screenshot(juce::Component& c,const juce::File& directory,const juce::String& name) {
    const auto file=directory.getChildFile(name+".png");
    juce::MemoryOutputStream encoded;
    juce::PNGImageFormat png;
    check(png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),encoded),"Capture PNG");
    auto stream=file.createOutputStream();check(stream!=nullptr,"Capture file");
    check(stream->setPosition(0) && stream->truncate().wasOk(),"Prepare capture file");
    // libpng's JUCE callback ignores OutputStream::write's return value. The
    // encoder succeeding alone therefore cannot prove that a file was saved.
    // Save the complete encoded buffer with an explicitly checked write.
    const bool complete=stream->write(encoded.getData(),encoded.getDataSize());
    stream->flush();
    if(!complete || stream->getStatus().failed() || file.getSize()!=juce::int64(encoded.getDataSize()))
        throw std::runtime_error("Capture write failed: "+file.getFullPathName().toStdString()
            +" ("+stream->getStatus().getErrorMessage().toStdString()+", expected "
            +std::to_string(encoded.getDataSize())+" bytes, saved "+std::to_string(file.getSize())+")");
}
void captureHeadAppearance(const juce::File& directory,bool nativePeer) {
    // Render the production component used by both the room and focused CAB.
    // Keep these isolated renderer captures separate from workspace captures.
    cabHead::View head;
    head.setSize(240,int(std::ceil(240.f*cabHead::projectedHeight(0)/cabPhysical::head(0).width)));
    if(nativePeer) {
        head.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        check(head.getPeer()!=nullptr,"Common head renderer did not create a native component peer");
    }
    head.setVisible(true);
    for(int model=0;model<ampModelCount;++model) {
        head.setModel(model);
        for(const int width:{240,1120}) {
            head.setSize(width,int(std::ceil(float(width)*cabHead::projectedHeight(model)/cabPhysical::head(model).width)));
            if(nativePeer)settle(10);
            screenshot(head,directory,"renderer-model"+juce::String(model)+"-"+juce::String(width));
        }
    }
}
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
        if(visible) {
            checkLabelFits(*value,"Active face value is truncated");
            auto* head=slider.findParentComponentOfClass<AmpHeadControlView>();check(head,"Active readout has no amplifier head");
            const auto plate=chimeraTest::reviewedHeadPlate(head->displayedModel())
                .transformedBy(cabHead::geometry(head->headBounds(),head->displayedModel()).sourceToView);
            check(plate.contains(head->getLocalArea(value,value->getLocalBounds()).toFloat()),
                  "Active value background or editor crosses the reviewed control plate");
            if(auto* editor=value->getCurrentTextEditor())
                check(plate.contains(head->getLocalArea(editor,editor->getLocalBounds()).toFloat()),
                      "Precise-entry editor crosses the reviewed control plate");
            const auto font=value->getLookAndFeel().getLabelFont(*value);
            const auto available=value->getBorderSize().subtractedFrom(value->getLocalBounds());
            for(double position:{slider.getMinimum(),slider.getMaximum(),
                slider.getMinimum()+.37*(slider.getMaximum()-slider.getMinimum())}) {
                juce::GlyphArrangement glyphs;glyphs.addLineOfText(font,slider.getTextFromValue(position),0,0);
                check(glyphs.getBoundingBox(0,-1,true).getWidth()<=float(available.getWidth()),
                      "Stable readout cannot fit a formatted endpoint or intermediate value");
            }
        }
    }
    check(found,"Missing editable face value label");
    auto* control=dynamic_cast<NativeControlView*>(slider.getParentComponent());check(control,"Missing native face control");
    check(control->label.isVisible()!=visible,"Parameter name and active value overlap");
}
std::vector<NativeControlView*> faceControls(juce::Component& root) {
    std::vector<NativeControlView*> result;
    const auto visit=[&](auto&& self,juce::Component& parent)->void {
        for(auto* child:parent.getChildren())if(auto* control=dynamic_cast<NativeControlView*>(child))result.push_back(control);else self(self,*child);
    };
    visit(visit,root);return result;
}
void checkFaceLayout(AmpHeadWorkspace& view,int model,int channel=0) {
    auto* head=find<AmpHeadControlView>(view,"ampHead1");check(head,"Missing layout head");
    const auto shellLayout=cabHead::geometry(head->headBounds(),model);
    const auto plate=chimeraTest::reviewedHeadPlate(model).transformedBy(shellLayout.sourceToView);
    const auto checkPlate=[&](juce::Rectangle<float> painted,const juce::String& name) {
        if(!plate.contains(painted))throw std::runtime_error("Native "+name.toStdString()
            +" leaves the reviewed blank control plate ["+testContext+", bounds "
            +painted.toString().toStdString()+", plate "+plate.toString().toStdString()+"]");
    };
    const auto checkPlateLabel=[&](juce::Label& label) {
        if(!label.isVisible())return;
        const auto area=head->getLocalArea(&label,label.getLocalBounds()).toFloat();
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText(label.getLookAndFeel().getLabelFont(label),label.getText(),area.getX(),area.getY(),
            area.getWidth(),area.getHeight(),juce::Justification::centred,1,1.f);
        const auto ink=glyphs.getBoundingBox(0,-1,true);
        checkPlate(ink.getUnion(ink.translated(0.f,1.f)),"label "+label.getText());
    };
    std::vector<juce::Rectangle<int>> cells;
    for(auto* c:faceControls(*head))if(c->isVisible()) {
        const auto bounds=head->getLocalArea(c,c->getLocalBounds());
        check(head->getLocalBounds().contains(bounds),"Face control clipped outside head");
        for(const auto& cell:cells)check(!cell.intersects(bounds),"Face controls overlap");
        cells.push_back(bounds);
        check(c->label.getLookAndFeel().getLabelFont(c->label).getHeight()>=12.f,"Face label is too small to read");
        checkLabelFits(c->label,"Face parameter label is truncated");
        if(c->slider.isVisible()) {
            const auto layout=c->slider.getLookAndFeel().getSliderLayout(c->slider);
            check(layout.sliderBounds.getWidth()>=30 && layout.sliderBounds.getHeight()>=30,"Knob target too small");
            checkPlate(head->getLocalArea(&c->slider,layout.sliderBounds).toFloat(),"knob "+c->label.getText());
            checkPlateLabel(c->label);
            for(auto* value:c->slider.getChildren())if(auto* label=dynamic_cast<juce::Label*>(value)) {
                if(label->isVisible()) {
                    checkLabelFits(*label,"Face value is truncated");checkPlateLabel(*label);
                    checkPlate(head->getLocalArea(label,label->getLocalBounds()).toFloat(),"value background "+c->label.getText());
                }
            }
        } else if(c->choice.isVisible()) {
            check(c->choice.getHeight()>=24 && c->choice.getWidth()>=48,"Face selector target too small");
            for(auto* child:c->choice.getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))checkLabelFits(*label,"Face selector text is truncated");
        } else check(c->toggle.isVisible() && c->toggle.getHeight()>=24 && c->toggle.getWidth()>=32,"Face switch target too small");
        if(auto* input=find<juce::ComboBox>(*head,"ampHeadInput1");input && input->isVisible()) {
            check(!bounds.intersects(head->getLocalArea(input,input->getLocalBounds())),"Input route overlaps face control");
        }
    }
    size_t expected=0;const auto& panel=ampNativePanel(model);
    for(size_t i=0;i<panel.controls.size();++i)if(ampNativeControlVisible(model,int(i),channel))++expected;
    check(cells.size()==expected,"Wrong layout control count");
    auto* input=find<juce::ComboBox>(*head,"ampHeadInput1");check(input,"Layout input route widget missing");
    check(input->getNumItems()==int(panel.routes.size()),"Layout input route count changed");
    for(size_t route=0;route<panel.routes.size();++route)
        check(input->getItemText(int(route))==juce::String::fromUTF8(panel.routes[route]),"Layout input route Unicode label changed");
    if(model==int(AmpModel::britEdge)) {
        check(input->isVisible(),"Brit Edge on-face input route is hidden");
        checkPlate(head->getLocalArea(input,input->getLocalBounds()).toFloat(),"input route selector");
        for(auto* child:input->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))
            checkLabelFits(*label,"Face input route text is truncated");
        bool foundRouteLabel=false;
        for(auto* child:head->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child);
            label && label->getText()=="INPUT ROUTE") {
            foundRouteLabel=true;check(label->isVisible(),"Brit Edge input route label is hidden");
            checkLabelFits(*label,"Face input route label is truncated");
            checkPlateLabel(*label);
        }
        check(foundRouteLabel,"Brit Edge input route label is missing");
    }
    check(head->caption().contains(juce::String::fromUTF8(panel.channels[size_t(channel)])),"Head caption Unicode channel name changed");
}
void checkKnobHardwareProfiles() {
    // These visible hardware pairs differ in silhouette, even where their
    // material is the same. Test rendered output, not the profile lookup table.
    constexpr const char* b7k="hw.b7k.master";
    constexpr const char* db751="hw.db751.gain";
    const juce::Rectangle<float> slot{16.f,16.f,96.f,96.f};
    const auto render=[&](ampHeadKnob::Renderer& renderer,int model,const char* key,float value=.5f) {
        juce::Image result(juce::Image::ARGB,128,128,true,juce::SoftwareImageType{});
        juce::Graphics g(result);
        renderer.draw(g,slot,value,juce::MathConstants<float>::pi*1.2f,
                      juce::MathConstants<float>::pi*2.8f,model,false,key);
        return result;
    };
    juce::Image coldBar,coldRound;
    {ampHeadKnob::Renderer renderer;coldBar=render(renderer,7,b7k);}
    {ampHeadKnob::Renderer renderer;coldRound=render(renderer,7,db751);}
    check(!chimeraTest::sameShellPixels(coldBar,coldRound),"Modern Bass hardware collapsed to one knob shape");
    {
        ampHeadKnob::Renderer renderer;
        const auto first=render(renderer,7,b7k),other=render(renderer,7,db751),again=render(renderer,7,b7k);
        check(chimeraTest::sameShellPixels(first,coldBar) && chimeraTest::sameShellPixels(other,coldRound)
              && chimeraTest::sameShellPixels(first,again),"Shared knob cache substituted or mutated another hardware profile");
        check(!chimeraTest::sameShellPixels(render(renderer,0,nullptr),render(renderer,14,nullptr)),
              "American skirt and EICH scalloped knobs have identical rendered hardware");
        check(!chimeraTest::sameShellPixels(render(renderer,1,nullptr),render(renderer,8,nullptr)),
              "British gold cap and Chime chickenhead have identical rendered hardware");
        const auto minimum=render(renderer,7,db751,0.f),maximum=render(renderer,7,db751,1.f);
        check(!chimeraTest::sameShellPixels(minimum,maximum),"Knob position marker does not move with its value");
        // At both endpoints the marker lies below this top-cap patch. Its
        // illumination must stay fixed in the same room as the head artwork.
        for(int y=38;y<48;++y)for(int x=46;x<82;++x)
            check(minimum.getPixelAt(x,y)==maximum.getPixelAt(x,y),"Turning a round knob rotates its fixed overhead illumination");
        for(const float value:{0.f,.5f,1.f}) {
            const auto chickenhead=render(renderer,8,nullptr,value);
            const auto safe=slot.expanded(2.f).getSmallestIntegerContainer();
            for(int y=0;y<chickenhead.getHeight();++y)for(int x=0;x<chickenhead.getWidth();++x)
                check(safe.contains(x,y) || chickenhead.getPixelAt(x,y).getAlpha()<=16,
                      "Rotating chickenhead paints outside its control slot");
        }
    }
    // Exercise key forwarding through actual production widgets as well as
    // Renderer: using only the amp model would lose the two Modern Bass grips.
    auto processor=std::make_unique<ChimeraProcessor>();
    set(*processor,"mode",0.f);processor->setAmpModel(0,7);processor->setAmpChannel(0,0);
    std::array<juce::String,2> ids;
    const std::array<const char*,2> keys{b7k,db751};
    for(size_t i=0;i<keys.size();++i) {
        const int index=ampHead::controlIndex(7,keys[i]);check(index>=0,"Modern Bass hardware fixture is missing");
        ids[i]=ampNativeControlID(0,7,index,0);
        auto* parameter=processor->parameters().getParameter(ids[i]);check(parameter,"Modern Bass hardware parameter is missing");
        parameter->setValueNotifyingHost(.5f);
    }
    AmpHeadWorkspace workspace(*processor,0);workspace.setVisible(true);workspace.setSize(1440,780);
    std::array<juce::Image,2> images;
    for(size_t i=0;i<ids.size();++i) {
        auto* slider=find<NativeControlSlider>(workspace,ids[i]);check(slider,"Modern Bass hardware widget is missing");
        checkReadout(*slider,false);
        const auto bounds=slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds;
        images[i]=slider->createComponentSnapshot(bounds);
    }
    check(images[0].getBounds()==images[1].getBounds(),"Modern Bass hardware fixture slots no longer have equal sizes");
    check(!chimeraTest::sameShellPixels(images[0],images[1]),"Native widgets lost their per-control Modern Bass hardware shapes");
    std::cout<<"KNOB HARDWARE: per-control native widgets, distinct physical profiles, shared cache identity, fixed light and rotating-grip containment\n";
}
void checkRoomOverviewControls() {
    // Exercise the actual room path: a standalone Painter test would miss a
    // room that calls only cabHead::paint and leaves its front plates empty.
    auto processor=std::make_unique<ChimeraProcessor>();
    const std::array<int,3> models{firstOriginalAmpModel,niflheimrAmpModel,int(AmpModel::britEdge)};
    const auto differentPixels=[](const juce::Image& a,const juce::Image& b,juce::Rectangle<int> region) {
        region=region.getIntersection(a.getBounds()).getIntersection(b.getBounds());
        int changed=0;
        for(int y=region.getY();y<region.getBottom();++y)for(int x=region.getX();x<region.getRight();++x) {
            const auto left=a.getPixelAt(x,y),right=b.getPixelAt(x,y);
            const int delta=std::abs(int(left.getRed())-int(right.getRed()))
                +std::abs(int(left.getGreen())-int(right.getGreen()))
                +std::abs(int(left.getBlue())-int(right.getBlue()));
            if(delta>=24)++changed;
        }
        return changed;
    };
    for(int mode:{1,2}) {
        set(*processor,"mode",float(mode));set(*processor,"lowampmix",1.f);
        std::array<juce::String,3> parameterIDs;
        for(int lane=0;lane<=mode;++lane) {
            const int model=models[size_t(lane)];
            processor->setAmpModel(lane,model);processor->setAmpChannel(lane,0);
            set(*processor,"ampon"+juce::String(lane+1),1.f);set(*processor,"mute"+juce::String(lane+1),0.f);
            const auto anchor=ampHead::knobs(model,0).front();
            const int index=ampHead::controlIndex(model,anchor.key);
            parameterIDs[size_t(lane)]=ampNativeControlID(ampNativeContext(mode,lane),model,index,0);
            set(*processor,parameterIDs[size_t(lane)],float(ampNativePanel(model).controls[size_t(index)].minimum));
        }
        struct ParameterWatch {
            ChimeraProcessor& processor;Host host;
            explicit ParameterWatch(ChimeraProcessor& p):processor(p){for(auto* parameter:processor.getParameters())parameter->addListener(&host);}
            ~ParameterWatch(){for(auto* parameter:processor.getParameters())parameter->removeListener(&host);}
        } watch(*processor);
        CabRoomOverview room(*processor);room.setSize(1280,760);room.setVisible(true);room.refreshState();
        std::array<juce::Component*,3> rigs{};
        std::array<juce::Rectangle<int>,3> knobRegions{};
        std::array<juce::Image,3> current;
        const auto snapshot=[&](int lane){return rigs[size_t(lane)]->createComponentSnapshot(rigs[size_t(lane)]->getLocalBounds());};
        for(int lane=0;lane<=mode;++lane) {
            testContext="room mode "+std::to_string(mode)+", lane "+std::to_string(lane);
            auto*& rig=rigs[size_t(lane)];rig=find<juce::Component>(room,"cabRoomRig"+juce::String(lane+1));
            check(rig && rig->isVisible(),"Room control regression cannot find its visible rig");
            const auto bounds=room.displayedHeadBounds(lane),face=cabHead::geometry(bounds,models[size_t(lane)]).front;
            const auto anchor=ampHead::knobs(models[size_t(lane)],0).front();
            const auto centre=face.getPosition()+juce::Point<float>{face.getWidth()*anchor.x,face.getHeight()*anchor.y};
            const float diameter=face.getWidth()*ampHead::faceStyle(models[size_t(lane)]).knobDiameter;
            knobRegions[size_t(lane)]=juce::Rectangle<float>{diameter+4.f,diameter+4.f}.withCentre(centre).getSmallestIntegerContainer();
            juce::Image bare(juce::Image::ARGB,rig->getWidth(),rig->getHeight(),true,juce::SoftwareImageType{});
            {juce::Graphics g(bare);cabHead::paint(g,bounds,models[size_t(lane)]);}
            current[size_t(lane)]=snapshot(lane);
            check(differentPixels(current[size_t(lane)],bare,knobRegions[size_t(lane)])>=4,
                  "Room head shows only its bare shell; the native knob is missing");
        }
        check(watch.host.writes==0 && watch.host.begins==0,"Creating or painting the room wrote amplifier parameters");
        room.refreshState();
        for(int lane=0;lane<=mode;++lane)
            check(chimeraTest::sameShellPixels(current[size_t(lane)],snapshot(lane)),"Idle room refresh changed its displayed controls");
        for(int lane=0;lane<=mode;++lane) {
            testContext="room mode "+std::to_string(mode)+", changed lane "+std::to_string(lane);
            const int model=models[size_t(lane)];
            const int index=ampHead::controlIndex(model,ampHead::knobs(model,0).front().key);
            set(*processor,parameterIDs[size_t(lane)],float(ampNativePanel(model).controls[size_t(index)].maximum));
            const int writes=watch.host.writes,begins=watch.host.begins;
            room.refreshState();
            for(int target=0;target<=mode;++target) {
                auto next=snapshot(target);
                if(target==lane)check(differentPixels(current[size_t(target)],next,knobRegions[size_t(target)])>0,
                                      "Changed native parameter did not update its room head knob");
                else check(chimeraTest::sameShellPixels(current[size_t(target)],next),"A native knob change repainted another lane's displayed value");
                current[size_t(target)]=next;
            }
            check(watch.host.writes==writes && watch.host.begins==begins,"Refreshing room knobs wrote parameters or host gestures");
            const auto& windows=headShell::definition(model).channelWindows;
            if(!windows[0].valid())continue;
            processor->setAmpChannel(lane,1);
            const int channelWrites=watch.host.writes,channelBegins=watch.host.begins;
            room.refreshState();
            const auto layout=cabHead::geometry(room.displayedHeadBounds(lane),model);
            const auto next=snapshot(lane);
            for(int channel=0;channel<2;++channel) {
                const auto point=cabHead::pixels(windows[size_t(channel)]).transformedBy(layout.sourceToView).getCentre().roundToInt();
                const float before=current[size_t(lane)].getPixelAt(point.x,point.y).getBrightness();
                const float after=next.getPixelAt(point.x,point.y).getBrightness();
                check(channel==1 ? after>before+.1f : before>after+.1f,
                      "Room head channel lights do not show the selected native channel");
            }
            current[size_t(lane)]=next;
            for(int target=0;target<=mode;++target)if(target!=lane)
                check(chimeraTest::sameShellPixels(current[size_t(target)],snapshot(target)),"Room channel selection changed another rig's head");
            check(watch.host.writes==channelWrites && watch.host.begins==channelBegins,
                  "Refreshing room channels wrote parameters or host gestures");
        }
        check(watch.host.open==0,"Room overview left a host gesture open");
    }
    testContext.clear();
    std::cout<<"ROOM HEAD CONTROLS: Dual/Matrix; 5 visible rigs; native knob presence, per-lane updates, channel lights and read-only refresh\n";
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
void checkStaleDiscrete(ChimeraProcessor& p,Host& all,int model,AmpNativeControlKind kind) {
    testContext="stale discrete control, model "+std::to_string(model);
    p.setAmpModel(0,model);p.setAmpChannel(0,0);
    auto view=std::make_unique<AmpHeadWorkspace>(p,0);view->addToDesktop(juce::ComponentPeer::windowIsTemporary);view->setVisible(true);settle(20);
    const auto& panel=ampNativePanel(model);int index=-1;
    for(size_t i=0;i<panel.controls.size();++i)if(panel.controls[i].kind==kind && ampNativeControlVisible(model,int(i),0)){index=int(i);break;}
    check(index>=0,"No discrete control for stale-event regression");const auto id=ampNativeControlID(0,model,index,0);
    auto* head=find<AmpHeadControlView>(*view,"ampHead1");auto* control=find<NativeControlView>(*head,id+"_control");check(control && control->isShowing(),"Stale-event face control missing");
    const auto before=get(p,id);p.setAmpChannel(0,1);const int writes=all.writes,begins=all.begins;
    // Deliver the old widget callback before the workspace timer can rebuild it.
    if(kind==AmpNativeControlKind::choice)control->choice.setSelectedId(control->choice.getSelectedId()==1?2:1,juce::sendNotificationSync);
    else {control->toggle.setToggleState(!control->toggle.getToggleState(),juce::dontSendNotification);check(bool(control->toggle.onClick),"Toggle callback missing");control->toggle.onClick();}
    check(get(p,id)==before && all.writes==writes && all.begins==begins,"Stale channel switch callback changed audio state");
    view.reset();check(all.open==0,"Stale discrete callback left host gesture open");
}
void checkQueuedRouteRebind(ChimeraProcessor& p,Host& all) {
    testContext="queued input route after model rebind";
    p.setAmpModel(0,1);p.setAmpChannel(0,0);p.setAmpNativeRoute(0,0);
    auto view=std::make_unique<AmpHeadWorkspace>(p,0);view->addToDesktop(juce::ComponentPeer::windowIsTemporary);view->setVisible(true);settle(20);
    juce::Component::SafePointer<juce::ComboBox> oldInput=find<juce::ComboBox>(*view,"ampHeadInput1");
    check(oldInput!=nullptr && oldInput->isShowing() && oldInput->getSelectedId()==1,"Queued-route source input missing");
    oldInput->setSelectedId(2,juce::sendNotificationAsync);
    check(p.selectedAmpNativeRoute(0)==0,"Queued route changed parameter synchronously");
    p.setAmpModel(0,0);const int newRoute=p.selectedAmpNativeRoute(0),writes=all.writes,begins=all.begins,ends=all.ends;
    view->refresh();check(oldInput==nullptr,"Model rebind retained the old input widget");
    auto drained=std::make_shared<bool>(false);check(juce::MessageManager::callAsync([drained]{*drained=true;}),"Unable to enqueue route-drain marker");
    awaitUI([&]{return *drained;},"Pending route callback queue did not drain");
    check(p.selectedAmpModel(0)==0 && p.selectedAmpNativeRoute(0)==newRoute,"Queued old route changed the new model bank");
    check(all.writes==writes && all.begins==begins && all.ends==ends && all.open==0,"Queued old route wrote after model rebind");
}
void checkQueuedChannelRebind(ChimeraProcessor& p,Host& all) {
    testContext="queued named channel after model rebind";
    p.setAmpModel(0,0);p.setAmpChannel(0,0);
    auto view=std::make_unique<AmpHeadWorkspace>(p,0);view->addToDesktop(juce::ComponentPeer::windowIsTemporary);view->setVisible(true);settle(20);
    juce::Component::SafePointer<juce::TextButton> oldButton=find<juce::TextButton>(*view,"ampHeadNamedChannel1_1");
    check(oldButton!=nullptr && oldButton->isShowing(),"Queued-channel source button missing");oldButton->triggerClick();
    check(p.selectedAmpChannel(0)==0,"Queued channel changed parameter synchronously");
    p.setAmpModel(0,3);p.setAmpChannel(0,0);const int writes=all.writes,begins=all.begins,ends=all.ends;
    view->refresh();check(oldButton==nullptr,"Model rebind retained the old channel button");
    auto drained=std::make_shared<bool>(false);check(juce::MessageManager::callAsync([drained]{*drained=true;}),"Unable to enqueue channel-drain marker");
    awaitUI([&]{return *drained;},"Pending channel callback queue did not drain");
    check(p.selectedAmpModel(0)==3 && p.selectedAmpChannel(0)==0,"Queued old channel changed the new model bank");
    check(all.writes==writes && all.begins==begins && all.ends==ends && all.open==0,"Queued old channel wrote after model rebind");
}
void checkAllModels(ChimeraProcessor& p,Host& all,const juce::File& directory) {
    set(p,"mode",0);int channels=0,bindings=0,knobs=0,choices=0,toggles=0,routes=0;
    for(int model=0;model<ampModelCount;++model) {
        testContext="model "+std::to_string(model);check(ampHead::supported(model),"Catalog model has no direct head controls");
        const auto& panel=ampNativePanel(model);p.setAmpModel(0,model);p.setAmpChannel(0,panel.defaultChannel);
        auto view=std::make_unique<AmpHeadWorkspace>(p,0);view->addToDesktop(juce::ComponentPeer::windowIsTemporary);view->setVisible(true);settle(20);
        auto* head=find<AmpHeadControlView>(*view,"ampHead1");check(head && head->isShowing(),"Catalog model fell back to Details");
        for(int channel=0;channel<int(panel.channels.size());++channel) {
            testContext="model "+std::to_string(model)+", channel "+std::to_string(channel);
            if(panel.channels.size()>1) {
                const auto namedID="ampHeadNamedChannel1_"+juce::String(channel);auto* named=find<juce::TextButton>(*view,namedID);
                check(named && named->isShowing(),"Catalog channel has no visible selector");
                check(named->getButtonText().contains(juce::String::fromUTF8(panel.channels[size_t(channel)])),"Catalog channel name missing");
                named->triggerClick();awaitUI([&]{auto* current=find<juce::TextButton>(*view,namedID);return p.selectedAmpChannel(0)==channel && current && current->getToggleState();},"Catalog channel selector failed");
            }
            view->refresh();check(head->displayedModel()==model && head->displayedContext()==0,"Catalog head bound wrong model or context");
            std::set<juce::String> activeIDs;size_t expectedControls=0;
            for(size_t index=0;index<panel.controls.size();++index) {
                const auto& spec=panel.controls[index];const auto id=ampNativeControlID(0,model,int(index),channel);
                auto* control=find<NativeControlView>(*head,id+"_control");const bool visible=ampNativeControlVisible(model,int(index),channel);
                check((control && control->isShowing())==visible,"Catalog channel mask does not match face visibility");
                if(!visible)continue;
                ++expectedControls;++bindings;check(activeIDs.insert(id).second,"Catalog face has duplicate parameter binding");
                testContext="model "+std::to_string(model)+", channel "+std::to_string(channel)+", "+spec.key;
                auto* parameter=p.parameters().getParameter(id);check(parameter,"Catalog face parameter missing");Host host;parameter->addListener(&host);
                if(spec.kind==AmpNativeControlKind::knob) {
                    ++knobs;check(control->slider.isShowing() && !control->choice.isVisible() && !control->toggle.isVisible(),"Catalog knob has wrong widget type");
                    set(p,id,parameter->convertFrom0to1(.37f));settle(2);checkReadout(control->slider,false);
                    const auto before=get(p,id);drag(control->slider,false);checkReadout(control->slider,true);releaseDrag(control->slider);checkReadout(control->slider,false);
                    check(get(p,id)!=before,"Catalog knob drag did not edit its parameter");check(host.begins==1 && host.ends==1 && host.open==0,"Catalog knob host gesture not balanced");
                } else if(spec.kind==AmpNativeControlKind::choice) {
                    ++choices;check(control->choice.isShowing() && !control->slider.isVisible() && !control->toggle.isVisible(),"Catalog choice has wrong widget type");
                    check(control->choice.getNumItems()==int(spec.options.size()),"Catalog selector options missing");
                    for(int option=0;option<int(spec.options.size());++option) {
                        check(control->choice.getItemText(option)==juce::String::fromUTF8(spec.options[size_t(option)]),"Catalog selector option label changed");
                        control->choice.setSelectedId(option+1,juce::sendNotificationSync);check(get(p,id)==float(option),"Catalog selector changed wrong parameter value");
                    }
                } else {
                    ++toggles;check(control->toggle.isShowing() && !control->slider.isVisible() && !control->choice.isVisible(),"Catalog toggle has wrong widget type");
                    const float target=control->toggle.getToggleState()?0.f:1.f;control->toggle.triggerClick();
                    awaitUI([&]{return get(p,id)==target;},"Catalog toggle did not edit its parameter");
                    check(control->toggle.getToggleState()==(target>.5f),"Catalog toggle visual state is stale");
                }
                check(host.open==0 && host.begins==host.ends,"Catalog control host gestures not balanced");parameter->removeListener(&host);
            }
            check(head->controlCount()==expectedControls,"Catalog head control count differs from channel mask");
            for(const auto size:{juce::Point<int>{1000,642},{1180,650},{1440,780},{1680,910}}) {
                testContext="model "+std::to_string(model)+", channel "+std::to_string(channel)+", width "+std::to_string(size.x);
                const int writes=all.writes,begins=all.begins;view->setSize(size.x,size.y);checkFaceLayout(*view,model,channel);
                check(all.writes==writes && all.begins==begins,"Catalog layout changed audio state");
                if(channel==panel.defaultChannel || size.x==1000)screenshot(*view,directory,"all-model-"+juce::String(model)+"-channel-"+juce::String(channel)+"-"+juce::String(size.x));
            }
            ++channels;
        }
        testContext="model "+std::to_string(model)+", input routes";
        auto* input=find<juce::ComboBox>(*head,"ampHeadInput1");
        if(panel.routes.size()>1) {
            check(input && input->isShowing() && input->getNumItems()==int(panel.routes.size()),"Catalog input routes unavailable");
            const int selectedChannel=p.selectedAmpChannel(0);
            for(int route=0;route<int(panel.routes.size());++route) {
                check(input->getItemText(route)==juce::String::fromUTF8(panel.routes[size_t(route)]),"Catalog input route label changed");
                input->setSelectedId(route+1,juce::sendNotificationSync);check(p.selectedAmpNativeRoute(0)==route,"Catalog input route did not change");
                check(p.selectedAmpModel(0)==model && p.selectedAmpChannel(0)==selectedChannel,"Input route changed model or channel");++routes;
            }
        } else check(!input || !input->isShowing(),"Single input route takes unnecessary face space");
        view.reset();check(all.open==0,"Catalog model destruction left a host gesture open");
    }
    checkStaleDiscrete(p,all,3,AmpNativeControlKind::choice);checkStaleDiscrete(p,all,0,AmpNativeControlKind::toggle);checkQueuedRouteRebind(p,all);checkQueuedChannelRebind(p,all);testContext.clear();
    std::cout<<"ALL HEADS: "<<ampModelCount<<" models; "<<channels<<" channels; "<<bindings<<" visible bindings ("<<knobs<<" knobs, "<<choices<<" selectors, "<<toggles<<" switches); "<<routes<<" selectable input routes; 4 workspace sizes\n";
}
}
int main(int argc,char** argv){try {
    juce::ScopedJuceInitialiser_GUI init;
    chimeraTest::checkHeadAppearance();
    std::cout<<"HEAD APPEARANCE: "<<ampModelCount<<" models; 3 sizes; complete RGBA shells, source pixel identity, single projection and corner continuity\n";
    const bool headsOnly=argc>1 && juce::String(argv[1])=="--software-heads";
    if(headsOnly) {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("head-software-preview");folder.createDirectory();
        captureHeadAppearance(folder,false);
        std::cout<<"SOFTWARE HEAD RENDERERS ONLY: "<<ampModelCount<<" models; 2 sizes; no native peer or interaction validation\n";
        return 0;
    }
    checkKnobHardwareProfiles();
    checkRoomOverviewControls();
    const bool layoutsOnly=argc>1 && juce::String(argv[1])=="--software-layouts";
    if(layoutsOnly || (argc>1 && juce::String(argv[1])=="--software-preview")) {
        const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("head-software-preview");if(!layoutsOnly)folder.createDirectory();
        auto preview=std::make_unique<ChimeraProcessor>();int channelsChecked=0,layoutsChecked=0;
        for(int model=0;model<ampModelCount;++model) {
          const auto& panel=ampNativePanel(model);const int first=layoutsOnly?0:panel.defaultChannel,last=layoutsOnly?int(panel.channels.size()):first+1;
          for(int channel=first;channel<last;++channel) {
            preview->setAmpModel(0,model);preview->setAmpChannel(0,channel);
            if(model==niflheimrAmpModel) {
                set(*preview,ampNativeControlID(0,model,int(niflheimr::Control::midFrequency),channel),2500);
                set(*preview,ampNativeControlID(0,model,int(niflheimr::Control::blend),channel),1);
            }
            AmpHeadWorkspace surface(*preview,0);surface.setVisible(true);
            for(const auto size:{juce::Point<int>{1000,642},{1180,650},{1440,780},{1680,910}}) {
                testContext="software model "+std::to_string(model)+", channel "+std::to_string(channel)+", width "+std::to_string(size.x);
                surface.setSize(size.x,size.y);checkFaceLayout(surface,model,channel);++layoutsChecked;
                if(layoutsOnly) {
                    auto* head=find<AmpHeadControlView>(surface,"ampHead1");check(head,"Software layout head missing");
                    for(auto* control:faceControls(*head))if(control->isShowing() && control->slider.isShowing()) {
                        checkReadout(control->slider,false);control->slider.beginValueAdjustment();checkReadout(control->slider,true);
                        control->slider.endValueAdjustment();checkReadout(control->slider,false);
                    }
                    continue;
                }
                screenshot(surface,folder,"model"+juce::String(model)+"-"+juce::String(size.x));
                const auto anchors=ampHead::knobs(model,channel);check(!anchors.empty(),"Software preview has no knobs");
                const int index=model==niflheimrAmpModel?int(niflheimr::Control::midFrequency):ampHead::controlIndex(model,anchors.front().key);
                auto* active=find<NativeControlSlider>(surface,ampNativeControlID(0,model,index,channel));check(active,"Software preview knob missing");
                active->beginValueAdjustment();checkReadout(*active,true);
                screenshot(surface,folder,"model"+juce::String(model)+"-"+juce::String(size.x)+"-active");active->endValueAdjustment();checkReadout(*active,false);
            }
            ++channelsChecked;
          }
        }
        testContext.clear();
        preview->releaseResources();
        if(layoutsOnly)std::cout<<"SOFTWARE LAYOUT ONLY: "<<ampModelCount<<" models; "<<channelsChecked<<" channels; "<<layoutsChecked<<" layouts; no screenshots, native peer or interaction validation\n";
        else std::cout<<"SOFTWARE PREVIEW ONLY: no native peer or interaction validation\n";
        return 0;
    }
    check(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()!=nullptr,"Native display required");
    const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>1?argv[1]:"amp-head-snapshots");directory.createDirectory();
    captureHeadAppearance(directory,true);
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
            named->triggerClick();settle();auto* current=find<juce::TextButton>(*layout,"ampHeadNamedChannel1_"+juce::String(channel));check(p->selectedAmpChannel(0)==channel && current && current->getToggleState(),"Named channel selection failed");
            check(find<juce::TextButton>(*layout,"ampHeadChannel1_"+juce::String(channel))->getToggleState(),"Physical channel indicator stale");
            screenshot(*layout,directory,"named-channel-"+juce::String(channel));
        }
    }
    checkAllModels(*p,all,directory);
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
    auto* canvas=find<juce::Component>(*editor,"surface");check(canvas,"Missing editor surface");
    rigViewTests::showControls(*canvas);
    for(int scale:{75,100,125,150}){editor->setSize(1180*scale/100,780*scale/100);settle();screenshot(*editor,directory,"editor-"+juce::String(scale));
        const int writes=all.writes,begins=all.begins;auto* open=find<juce::TextButton>(*editor,"ampHeadOpen1");check(open && open->isShowing(),"Classic head entry missing");open->triggerClick();settle(150);
        // triggerClick() is posted; a stalled macOS loop can outlast one settle.
        AmpHeadWorkspace* full=nullptr;
        awaitUI([&]{full=nullptr;auto& desktop=juce::Desktop::getInstance();
                    for(int i=0;i<desktop.getNumComponents();++i)if(auto* w=find<AmpHeadWorkspace>(*desktop.getComponent(i),"ampHeadWorkspace"))full=w;
                    return full && full->isShowing();},"Head did not open native enlargement");screenshot(*full,directory,"focus-from-editor-"+juce::String(scale));
        closeWorkspace();check(all.writes==writes && all.begins==begins,"Head open/back changed audio state");
    }
    auto* roomToggle=find<juce::TextButton>(*canvas,"path.cab");check(roomToggle && roomToggle->isShowing(),"Room return unavailable after Classic head controls");
    roomToggle->triggerClick();
    for(int mode:{0,1,2}){set(*p,"mode",float(mode));CabRoomOverview* room=nullptr;
        // The editor rebuilds routing surfaces asynchronously. Wait for the
        // requested mode's visible targets, not an arbitrary 80 ms timer tick.
        awaitUI([&]{room=find<CabRoomOverview>(*editor,"cabRoomOverview");if(!room || !room->isShowing())return false;
            for(int lane=0;lane<=mode;++lane){auto* head=find<juce::Button>(*room,"cabRoomHead"+juce::String(lane+1));auto* cab=find<juce::Button>(*room,"cabRoomRig"+juce::String(lane+1));if(!head || !cab || !head->isShowing() || !cab->isShowing())return false;}return true;},"Room routing targets did not become visible");
        // Keep production callbacks installed: prove the real destination,
        // selected bank and return path, not merely a substituted callback.
        const int writes=all.writes,begins=all.begins;
        for(int lane=0;lane<=mode;++lane){auto* entry=find<juce::Button>(*room,"cabRoomHead"+juce::String(lane+1));auto* cab=find<juce::Button>(*room,"cabRoomRig"+juce::String(lane+1));check(entry && cab,"Separate room targets missing");
            entry->triggerClick();awaitUI([]{return openWorkspace()!=nullptr;},"Room head did not open workspace");
            auto* full=openWorkspace();auto* target=find<AmpHeadControlView>(*full,"ampHead"+juce::String(lane+1));
            check(target && target->displayedContext()==ampNativeContext(mode,lane),"Room opened wrong amplifier bank");
            screenshot(*full,directory,"room-to-head-mode"+juce::String(mode)+"-lane"+juce::String(lane));closeWorkspace();
            int selectedCab=-1;const auto callback=room->onCabinetSelected;
            room->onCabinetSelected=[&](int selected){selectedCab=selected;};cab->triggerClick();
            // triggerClick is delivered through the native message queue. Do
            // not restore the production callback before a delayed macOS
            // dispatch has delivered the selected lane.
            awaitUI([&]{return selectedCab==lane;},"Wrong room cabinet lane");
            room->onCabinetSelected=callback;
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

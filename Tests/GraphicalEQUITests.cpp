#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <ctime>

using namespace spectralforge;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void settle(int ms){check(juce::MessageManager::getInstance()->runDispatchLoopUntil(ms),"UI loop exited");}
template<class T> T* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return dynamic_cast<T*>(&c);for(auto* child:c.getChildren())if(auto* p=find<T>(*child,id))return p;return nullptr;}
juce::TextButton* tab(juce::Component& c,const juce::String& name){for(auto* child:c.getChildren()){if(auto* b=dynamic_cast<juce::TextButton*>(child);b && b->getButtonText()==name)return b;if(auto* b=tab(*child,name))return b;}return nullptr;}
void set(ChimeraProcessor& p,const juce::String& id,float v){auto* q=p.parameters().getParameter(id);check(q!=nullptr,"Parameter missing");q->setValueNotifyingHost(q->convertTo0to1(v));}
float get(ChimeraProcessor& p,const juce::String& id){return p.parameters().getRawParameterValue(id)->load();}
struct HostEvents : juce::AudioProcessorParameter::Listener {
    int changes{},starts{},ends{};
    void parameterValueChanged(int,float) override {++changes;}
    void parameterGestureChanged(int,bool start) override {if(start)++starts;else ++ends;}
};
void numeric(juce::Slider& slider,const juce::String& text){slider.showTextBox();juce::TextEditor* input=nullptr;
    const auto scan=[&](auto&& self,juce::Component& c)->void{if(auto* t=dynamic_cast<juce::TextEditor*>(&c))input=t;for(auto* child:c.getChildren())self(self,*child);};scan(scan,slider);
    check(input!=nullptr,"Numeric editor did not open");input->setText(text,false);slider.hideTextBox(false);}
juce::MouseEvent mouse(juce::Component& target,juce::Point<float> position,juce::Point<float> down,bool dragging){return {juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1.f,0.f,0.f,0.f,0.f,&target,&target,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,dragging};}
}
int main(int argc,char** argv){try {
    juce::ScopedJuceInitialiser_GUI init;auto processor=std::make_unique<ChimeraProcessor>();auto editor=std::make_unique<ChimeraEditor>(*processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);check(editor->getPeer()!=nullptr,"No native UI peer");
    auto* eqTab=tab(*editor,"EQ");check(eqTab!=nullptr,"EQ navigation missing");eqTab->triggerClick();settle(80);
    const juce::File directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>1?argv[1]:"eq-ui");directory.createDirectory();
    for(int e=0;e<2;++e){auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(panel && panel->isShowing(),"EQ panel not shown");
        auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));check(selector,"Band chooser missing");
        HostEvents host;auto* frequency=processor->parameters().getParameter(eqID(e,0,"frequency"));auto* gain=processor->parameters().getParameter(eqID(e,0,"gain"));auto* q=processor->parameters().getParameter(eqID(e,0,"q"));frequency->addListener(&host);gain->addListener(&host);q->addListener(&host);
        for(int b=0;b<12;++b){selector->setSelectedId(b+1,juce::sendNotificationSync);
            auto* f=find<juce::Slider>(*panel,eqID(e,b,"frequency"));auto* g=find<juce::Slider>(*panel,eqID(e,b,"gain"));auto* qs=find<juce::Slider>(*panel,eqID(e,b,"q"));check(f && g && qs,"Node controls missing");
            numeric(*f,juce::String(200+b*317)+" Hz");numeric(*g,"3.5 dB");numeric(*qs,"1.25 Q");
            check(std::abs(get(*processor,eqID(e,b,"frequency"))-(200+b*317))<.1 && get(*processor,eqID(e,b,"gain"))==3.5f && std::abs(get(*processor,eqID(e,b,"q"))-1.25f)<.001,"Numeric node edit did not reach host");
            auto* on=find<juce::TextButton>(*panel,eqID(e,b,"enabled"));on->triggerClick();settle(5);check(get(*processor,eqID(e,b,"enabled"))==1,"Band activation not connected");
            auto* filter=find<juce::ComboBox>(*panel,eqID(e,b,"type"));filter->setSelectedId(4,juce::sendNotificationSync);check(get(*processor,eqID(e,b,"type"))==3 && !g->isEnabled(),"HP choice / gain applicability");
            filter->setSelectedId(1,juce::sendNotificationSync);
        }
        selector->setSelectedId(1,juce::sendNotificationSync);const auto start=panel->nodePosition(0),end=start+juce::Point<float>(40,-18);
        panel->mouseDown(mouse(*panel,start,start,false));panel->mouseDrag(mouse(*panel,end,start,true));panel->mouseUp(mouse(*panel,end,start,true));
        check(get(*processor,eqID(e,0,"frequency"))>200 && get(*processor,eqID(e,0,"gain"))>3.5,"Graph drag not connected");
        juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;panel->mouseWheelMove(mouse(*panel,end,end,false),wheel);check(get(*processor,eqID(e,0,"q"))>1.25,"Wheel Q not connected");
        check(host.starts>=3 && host.starts==host.ends && host.changes>0,"Unbalanced/missing host gestures");frequency->removeListener(&host);gain->removeListener(&host);q->removeListener(&host);
        auto* bypass=find<juce::TextButton>(*panel,eqID(e,-1,"bypass"));bypass->triggerClick();settle(5);check(!processor->graphicalEQ(e).isBypassed(),"Bypass not connected");
        for(int b=0;b<12;++b)set(*processor,eqID(e,b,"enabled"),b==0?1.f:0.f);set(*processor,eqID(e,0,"frequency"),1007.8125f);set(*processor,eqID(e,0,"gain"),6);processor->graphicalEQ(e).prepare(48000);
    }
    juce::AudioBuffer<float> audio(2,512);for(int k=0;k<12;++k){for(int n=0;n<512;++n){const float v=.1f*std::sin(2*juce::MathConstants<float>::pi*43*float(k*512+n)/2048);audio.setSample(0,n,v);audio.setSample(1,n,-v);}
        juce::AudioBuffer<float> copy;copy.makeCopyOf(audio);processor->graphicalEQ(0).process(audio);processor->graphicalEQ(1).process(copy);if(k%4==3)settle(50);}
    for(int e=0;e<2;++e){auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(panel->fftFrames()>0,"FFT did not update");check(std::abs(panel->spectrumDB(false,43)+20)<.1,"Stereo anti-phase canceled input FFT");check(std::abs(panel->spectrumDB(true,43)+14)<.15,"FFT did not measure actual 6 dB output");std::cout<<"FFT "<<e<<" input="<<panel->spectrumDB(false,43)<<" output="<<panel->spectrumDB(true,43)<<" us="<<panel->lastFFTTimeMicros()<<'\n';}
    for(int percent:{75,100,125,150}){editor->setSize(1180*percent/100,780*percent/100);settle(70);
        for(int e=0;e<2;++e){auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(editor->getLocalBounds().contains(editor->getLocalArea(panel,panel->getLocalBounds())),"EQ clipped at scale");for(const char* field:{"frequency","gain","q"}){auto* s=find<juce::Slider>(*panel,eqID(e,0,field));check(editor->getLocalBounds().contains(editor->getLocalArea(s,s->getLocalBounds())),"Numeric controls clipped");}}
        auto output=directory.getChildFile("eq-ui-"+juce::String(percent)+".png").createOutputStream();check(output!=nullptr,"Screenshot output");output->setPosition(0);output->truncate();juce::PNGImageFormat png;check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*output),"Screenshot encoding");
        const auto t=juce::Time::getMillisecondCounterHiRes();const auto cpu=std::clock();settle(1000);const double elapsed=juce::Time::getMillisecondCounterHiRes()-t;std::cout<<"UI idle "<<percent<<"% = "<<100000.*double(std::clock()-cpu)/CLOCKS_PER_SEC/elapsed<<"% one core (1 second no audio)\n";
    }
    tab(*editor,"RIGS")->triggerClick();settle(70);for(int e=0;e<2;++e)check(!processor->graphicalEQ(e).analyzer.enabled.load(),"Hidden EQ analyzer still enabled");
    editor.reset();for(int e=0;e<2;++e)check(!processor->graphicalEQ(e).analyzer.enabled.load(),"Closed editor analyzer still enabled");processor->releaseResources();
    std::cout<<"PASS real native peer / 24 numeric nodes / drag / Q / host gestures / 7 choices / stereo FFT / 75-150% / hidden and close\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

#include "CabRoom.h"
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI init;auto p=std::make_unique<ChimeraProcessor>();
 for(int mode:{0,2}){p->parameters().getParameter("mode")->setValueNotifyingHost(p->parameters().getParameter("mode")->convertTo0to1(float(mode)));
  CabRoomOverview view(*p);view.setSize(1140,412);view.refreshState();auto img=view.createComponentSnapshot(view.getLocalBounds(),true,1.f);
  juce::File f(juce::String(argv[1])+"/room_mode"+juce::String(mode)+".png");f.deleteFile();juce::FileOutputStream out(f);juce::PNGImageFormat().writeImageToStream(img,out);}
 {spectralforge::cabRoom::RoomControls panel(*p);panel.setRigCount(3);panel.setSize(620,60);auto img=panel.createComponentSnapshot(panel.getLocalBounds(),true,1.f);
  juce::File f(juce::String(argv[1])+"/room_panel.png");f.deleteFile();juce::FileOutputStream out(f);juce::PNGImageFormat().writeImageToStream(img,out);}
 return 0;}

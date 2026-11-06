#include "CabRoom.h"
namespace {
void write(const juce::Image& img,const juce::String& path){juce::File f(path);f.deleteFile();juce::FileOutputStream out(f);juce::PNGImageFormat().writeImageToStream(img,out);}
void set(ChimeraProcessor& p,const char* id,float v){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI init;auto p=std::make_unique<ChimeraProcessor>();const juce::String dir(argv[1]);
 for(int mode:{0,2}){set(*p,"mode",float(mode));
  CabRoomOverview view(*p);view.setSize(1140,412);view.refreshState();write(view.createComponentSnapshot(view.getLocalBounds(),true,1.f),dir+"/room_mode"+juce::String(mode)+".png");}
 {spectralforge::cabRoom::RoomControls panel(*p);panel.setRigCount(3);panel.setSize(620,60);write(panel.createComponentSnapshot(panel.getLocalBounds(),true,1.f),dir+"/room_panel.png");}
 // Room Space materials over the scenery: the defaults, then each material on
 // every surface (docs/ROOM_SPACE_1_8.md).
 set(*p,"mode",0.f);set(*p,"roomOn",1.f);set(*p,"roomSpace",1.f);
 {CabRoomOverview view(*p);view.setSize(1140,412);view.refreshState();write(view.createComponentSnapshot(view.getLocalBounds(),true,1.f),dir+"/room_space_defaults.png");}
 for(int m=0;m<int(spectralforge::roomMaterials.size());++m){
  set(*p,"roomFloor",float(m));set(*p,"roomWalls",float(m));set(*p,"roomCeiling",float(m));
  CabRoomOverview view(*p);view.setSize(1140,412);view.refreshState();
  write(view.createComponentSnapshot(view.getLocalBounds(),true,1.f),dir+"/room_space_material"+juce::String(m)+".png");}
 return 0;}

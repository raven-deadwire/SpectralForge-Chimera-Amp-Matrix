// Saves the full editor and Easy mode as PNGs (not part of the test suite):
//   ChimeraEditorSnapshotProbe <directory>
#include "PluginEditor.h"
namespace {
void save(juce::Component& c,const juce::File& f){f.deleteFile();juce::FileOutputStream out(f);juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds(),true,1.f),out);}
}
int main(int argc,char** argv){
    if(argc<2)return 2;
    juce::ScopedJuceInitialiser_GUI init;
    auto p=std::make_unique<ChimeraProcessor>();p->prepareToPlay(48000,256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
    editor->setSize(1180,780);
    for(int i=0;i<20;++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    const juce::File dir{juce::String(argv[1])};dir.createDirectory();
    auto* e=dynamic_cast<ChimeraEditor*>(editor.get());
    for(bool easy:{false,true}){
        if(e)e->setEasyMode(easy);for(int i=0;i<10;++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        save(*editor,dir.getChildFile(easy?"editor_easy.png":"editor_full.png"));
    }
    editor.reset();return 0;
}

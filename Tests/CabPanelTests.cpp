#include "IRLibrary.h"
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void fixture(const juce::File& f,double rate,int channels,int offset) {
    juce::WavAudioFormat format;auto stream=f.createOutputStream();
    auto w=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.release(),rate,(unsigned)channels,24,{},0));
    require(w!=nullptr,"fixture writer");juce::AudioBuffer<float> a(channels,512);a.clear();
    for(int c=0;c<channels;++c){a.setSample(c,offset,.5f);a.setSample(c,offset+3,.125f);}
    require(w->writeFromAudioSampleBuffer(a,0,512),"fixture write");
}
std::vector<float> render(spectralforge::Cab& cab,const juce::dsp::ProcessSpec& spec) {
    juce::AudioBuffer<float> audio((int)spec.numChannels,(int)spec.maximumBlockSize);
    // Settle all smoothing before the impulse, then clear filter/delay history.
    for(int i=0;i<32;++i){audio.clear();cab.process(audio);}cab.reset();
    std::vector<float> result;
    for(int block=0;block<32;++block){audio.clear();if(block==0)audio.setSample(0,0,.25f);cab.process(audio);
        for(int n=0;n<audio.getNumSamples();++n){auto x=audio.getSample(0,n);require(std::isfinite(x),"finite output");result.push_back(x);}
        if(spec.numChannels==2)require(audio.getMagnitude(1,0,audio.getNumSamples())<1e-8f,"channel isolation");}
    return result;
}
float difference(const std::vector<float>& a,const std::vector<float>& b){float d=0;for(size_t i=0;i<a.size();++i)d=std::max(d,std::abs(a[i]-b[i]));return d;}
int main(int argc,char** argv){
    auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("cab-panel-fixtures",{},false);
    struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}} cleanup{folder};
    try {
        require(folder.createDirectory().wasOk(),"fixture directory");
        // Deliberately misleading directory: the native decoder must read 48k headers.
        auto a=folder.getChildFile("44.1kHz-A.wav"),b=folder.getChildFile("B.wav");if(argc==3) {
            a=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);b=juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
        } else {require(argc==1,"usage: ChimeraCabPanelTests [user-IR-A user-IR-B]");fixture(a,48000,1,0);fixture(b,96000,2,12);}
        juce::MemoryBlock bytes;a.loadFileAsData(bytes);juce::String error;
        const auto decoded=spectralforge::IRLibrary::decode(bytes,a.getFileName(),error);require(decoded!=nullptr,"A decoder");
        if(argc==1)require(decoded->rate==48000,"header sample rate");
        std::cout<<"INPUT A "<<a.getFileName()<<" / "<<decoded->rate<<" Hz / "<<decoded->samples.getNumSamples()<<" frames\n";
        std::cout<<"INPUT B "<<b.getFileName()<<"\n";
        for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})for(int block:{64,256}) {
            juce::dsp::ProcessSpec spec{rate,(juce::uint32)block,(juce::uint32)channels};
            std::array<spectralforge::Cab,3> cabs;for(auto& c:cabs)c.prepare(spec);
            spectralforge::IRLibrary library({&cabs[0],&cabs[1],&cabs[2]});
            require(library.importFile(0,a).wasOk() && library.importFile(3,b).wasOk(),"native A/B file loading");
            cabs[0].secondMic()->requestedSource.store(3);library.prepare(spec,{3,0,0});library.stop();
            auto& cab=cabs[0];auto* micB=cab.secondMic();
            cab.blend=0;auto outA=render(cab,spec);cab.blend=1;auto outB=render(cab,spec);
            require(difference(outA,outB)>1e-4f,"independent mic response");
            cab.blend=.5f;auto mix=render(cab,spec);std::vector<float> expected=outA;
            for(size_t n=0;n<expected.size();++n)expected[n]=.5f*(outA[n]+outB[n]);
            require(difference(mix,expected)<1e-6f,"linear blend");
            micB->invert=true;auto inverted=render(cab,spec);for(size_t n=0;n<expected.size();++n)expected[n]=.5f*(outA[n]-outB[n]);
            require(difference(inverted,expected)<1e-6f,"B polarity");
            micB->invert=false;cab.blend=1;micB->gainDb=-6;auto quieter=render(cab,spec);
            for(size_t n=0;n<expected.size();++n)expected[n]=outB[n]*juce::Decibels::decibelsToGain(-6.f);
            require(difference(quieter,expected)<1e-6f,"B level");
            micB->gainDb=0;micB->delayMs=2;auto delayed=render(cab,spec);require(difference(delayed,outB)>1e-4f,"B signal delay");
            micB->delayMs=0;micB->setCuts(400,2000);auto filtered=render(cab,spec);require(difference(filtered,outB)>1e-4f,"B filters");
            micB->setCuts(70,9000);cab.blend=.37f;auto before=render(cab,spec);
            const auto saved=library.save();require(saved.getNumChildren()==2,"two embedded assets");
            require(library.importFile(3,folder.getChildFile("missing.wav")).failed(),"invalid IR rejection");
            library.prepare(spec,{3,0,0});library.stop();require(difference(before,render(cab,spec))<1e-7f,"failed import preserves B");
            std::array<spectralforge::Cab,3> restored;for(auto& c:restored)c.prepare(spec);
            spectralforge::IRLibrary recall({&restored[0],&restored[1],&restored[2]});
            juce::MemoryOutputStream serialized;saved.writeToStream(serialized);
            recall.restore(juce::ValueTree::readFromData(serialized.getData(),serialized.getDataSize()));
            restored[0].secondMic()->requestedSource.store(3);recall.prepare(spec,{3,0,0});recall.stop();restored[0].blend=.37f;
            require(difference(before,render(restored[0],spec))<1e-7f,"embedded A/B audio restore");
            cab.blend=0;micB->requestedSource.store(0);library.prepare(spec,{3,0,0});
            require(library.importFile(3,b).wasOk(),"muted B async import");micB->requestedSource.store(3);
            juce::AudioBuffer<float> silence(channels,block);bool ready=false;
            for(int attempt=0;attempt<2000 && !ready;++attempt) {
                silence.clear();cab.process(silence);
                ready=!library.status(3).contains("Preparing IR");
                if(!ready)juce::Thread::sleep(1);
            }
            require(ready && micB->activeSource.load()==3,"muted B must activate without moving blend");library.stop();
            std::cout<<"PASS "<<rate<<" Hz / "<<channels<<" ch / "<<block<<" frames\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

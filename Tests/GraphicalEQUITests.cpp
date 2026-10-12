#include "PluginEditor.h"
#include "NativeMessageLoop.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>
#if JUCE_WINDOWS
#include <windows.h>
#else
#include <time.h>
#endif

using namespace spectralforge;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
double processCPUSeconds(){
#if JUCE_WINDOWS
    FILETIME created{},exited{},kernel{},user{};check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)!=0,"Process CPU query");
    ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return double(k.QuadPart+u.QuadPart)*1.e-7;
#else
    timespec t{};check(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t)==0,"Process CPU query");return double(t.tv_sec)+double(t.tv_nsec)*1.e-9;
#endif
}
void settle(int ms){check(chimeraTest::dispatchFor(ms),"UI loop exited");}
template<class Predicate> bool waitFor(Predicate predicate){const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000;while(!predicate() && juce::Time::getMillisecondCounterHiRes()<deadline)settle(20);return predicate();}
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
void saveImage(const juce::Image& image,const juce::File& file) {
    juce::MemoryOutputStream encoded;juce::PNGImageFormat png;
    check(png.writeImageToStream(image,encoded),"Screenshot encoding");
    check(file.replaceWithData(encoded.getData(),encoded.getDataSize()),"Screenshot replacement failed");
    juce::MemoryBlock saved;
    if(!file.loadFileAsData(saved) || saved.getSize()!=encoded.getDataSize()
        || std::memcmp(saved.getData(),encoded.getData(),saved.getSize())!=0)
        throw std::runtime_error("Screenshot read-back did not match the completed PNG: "+file.getFileName().toStdString());
}
void snapshot(juce::Component& component,const juce::File& file) {
    saveImage(component.createComponentSnapshot(component.getLocalBounds()),file);
}
double percentile(std::vector<double> values,double fraction) {
    check(!values.empty(),"No native timing samples");std::sort(values.begin(),values.end());
    return values[std::min(values.size()-1,size_t(fraction*double(values.size()-1)))];
}
// A paced synthetic audio callback runs the production EQ processing on a
// separate thread while the native desktop dispatches real timer/paint events.
// It does not inject completed spectra or bypass the analyzer FIFO/FFT.
class SyntheticAudio {
public:
    explicit SyntheticAudio(ChimeraProcessor& p,bool wholeProcessor=false,double rate=48000,bool antiPhase=true):processor(p),fullRoute(wholeProcessor),sampleRate(rate),rightGain(antiPhase?-1.f:1.f){timings.reserve(10000);thread=std::thread([this]{run();});}
    ~SyntheticAudio(){running.store(false);if(thread.joinable())thread.join();}
    void stop(){running.store(false);if(thread.joinable())thread.join();}
    std::atomic<double> frequency{1007.8125},amplitude{.1};
    std::atomic<bool> pluck{false};
    std::atomic<uint64_t> callbacks{0};
    const std::vector<double>& callbackMicrosAfterStop() const {return timings;}
private:
    void run() {
        juce::AudioBuffer<float> first(2,128),second(2,128);juce::MidiBuffer midi;double phase=0;uint64_t total=0;
        auto deadline=std::chrono::steady_clock::now();
        const auto interval=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(128./sampleRate));
        while(running.load()) {
            const double hz=frequency.load(),level=amplitude.load();const bool harmonic=pluck.load();
            for(int n=0;n<128;++n) {
                double value=std::sin(phase);
                if(harmonic) {
                    value+=.45*std::sin(phase*2)+.27*std::sin(phase*3)+.16*std::sin(phase*5)+.08*std::sin(phase*7);
                    const double age=std::fmod(double(total)/sampleRate,.65);value*=std::exp(-age/.18);
                }
                const float sample=float(level*value);first.setSample(0,n,sample);first.setSample(1,n,sample*rightGain);
                second.setSample(0,n,sample);second.setSample(1,n,sample*rightGain);
                phase=std::fmod(phase+2*juce::MathConstants<double>::pi*hz/sampleRate,2*juce::MathConstants<double>::pi);++total;
            }
            const auto callbackStart=std::chrono::steady_clock::now();
            if(fullRoute)processor.processBlock(first,midi);
            else {processor.graphicalEQ(0).process(first);processor.graphicalEQ(1).process(second);}
            const double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-callbackStart).count();
            if(timings.size()<10000)timings.push_back(elapsed);callbacks.fetch_add(1);
            deadline+=interval;const auto now=std::chrono::steady_clock::now();
            if(now>deadline+interval*2)deadline=now; // No unbounded catch-up burst after a scheduler pause.
            std::this_thread::sleep_until(deadline);
        }
    }
    ChimeraProcessor& processor;bool fullRoute{};double sampleRate{48000};float rightGain{-1};std::atomic<bool> running{true};std::vector<double> timings;std::thread thread;
};
int spectrumPixels(const juce::Image& image,juce::Rectangle<int> bounds,bool output) {
    int count=0;bounds=bounds.getIntersection(image.getBounds());
    for(int y=bounds.getY();y<bounds.getBottom();++y)for(int x=bounds.getX();x<bounds.getRight();++x) {
        const auto colour=image.getPixelAt(x,y);const int r=colour.getRed(),g=colour.getGreen(),b=colour.getBlue();
        if(output ? (r>g+8 && g>b+8) : (b>r+14 && g>r+10))++count;
    }
    return count;
}
void liveAnalyzer(ChimeraProcessor& processor,ChimeraEditor& editor,const juce::File& directory) {
    std::ofstream timing(directory.getChildFile("eq-analyzer-native-ui.csv").getFullPathName().toStdString());
    timing<<"bank,fft_size,rate,bin_spacing_hz,window_ms,first_complete_data_observed_ms,first_component_render_complete_ms,updates,fft_p50_us,fft_p99_us,update_p50_us,update_p99_us,path_p50_us,path_p99_us,panel_paint_p50_us,panel_paint_p99_us,snapshot_p50_us,snapshot_p99_us,native_interval_p50_ms,native_interval_p99_ms,process_cpu_with_snapshot_percent\n";
    SyntheticAudio audio(processor);
    for(int e=0;e<2;++e) {
        auto* bank=find<juce::TextButton>(editor,e==0?"eq.bank.tone":"eq.bank.final");check(bank,"Selected-bank controls missing");bank->triggerClick();
        auto* panel=find<GraphicalEQPanel>(editor,e==0?"toneEQ_panel":"finalEQ_panel");
        auto* other=find<GraphicalEQPanel>(editor,e==0?"finalEQ_panel":"toneEQ_panel");
        check(panel && other && waitFor([&]{return panel->isShowing() && !other->isShowing();}),"Exactly one selected EQ bank must be visible");
        check(panel->getWidth()>=1100 && panel->graphBounds().getWidth()>1000 && panel->graphBounds().getHeight()>200,
              "Selected EQ did not receive the large editing surface");
        check(waitFor([&]{return processor.graphicalEQ(e).analyzer.enabled.load() && !processor.graphicalEQ(1-e).analyzer.enabled.load();}),
              "Hidden bank still captures audio");
        const auto hiddenFrames=other->fftFrames();
        auto* resolution=find<juce::ComboBox>(*panel,eqID(e,-1,"resolution"));
        auto* smoothing=find<juce::ComboBox>(*panel,eqID(e,-1,"smoothing"));
        check(resolution && smoothing,"Analyzer quality controls missing");
        for(int choice=1;choice<=3;++choice) {
            const int size=1<<(choice+10),bin=43*(size/2048);
            const double started=juce::Time::getMillisecondCounterHiRes();const auto before=panel->fftFrames();
            resolution->setSelectedId(choice,juce::sendNotificationSync);
            check(panel->analysisFFTSize()==size,"Resolution selector did not reach the actual FFT");
            check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>before;}),"Selected FFT did not react to the synthetic audio callback");
            const double reaction=juce::Time::getMillisecondCounterHiRes()-started;
            const auto initialRender=panel->createComponentSnapshot(panel->getLocalBounds());check(initialRender.isValid(),"First native component render failed");
            const double rendered=juce::Time::getMillisecondCounterHiRes()-started;
            settle(200);
            check(std::abs(panel->spectrumDB(false,bin)+20)<.1,"Native input FFT canceled stereo anti-phase or is not calibrated");
            check(std::abs(panel->spectrumDB(true,bin)+14)<.15,"Native FFT did not measure the actual6dB EQ output");
            std::vector<double> fft,update,path,paint,capture,interval;
            uint64_t previousFrames=panel->fftFrames();double lastFrame=juce::Time::getMillisecondCounterHiRes();
            const auto cpu=processCPUSeconds(),wall=juce::Time::getMillisecondCounterHiRes();
            while(fft.size()<100) {
                check(waitFor([&]{return panel->fftFrames()>previousFrames;}),"Live native FFT stalled");
                const auto now=juce::Time::getMillisecondCounterHiRes();interval.push_back(now-lastFrame);lastFrame=now;previousFrames=panel->fftFrames();
                fft.push_back(panel->lastFFTTimeMicros());update.push_back(panel->analysisUpdateMicros());path.push_back(panel->spectrumPathMicros());
                const auto captureStart=juce::Time::getMillisecondCounterHiRes();
                const auto image=panel->createComponentSnapshot(panel->getLocalBounds());check(image.isValid(),"Native rendered surface is invalid");
                capture.push_back(1000*(juce::Time::getMillisecondCounterHiRes()-captureStart));paint.push_back(panel->lastPaintMicros());
            }
            const double processCPU=100000*(processCPUSeconds()-cpu)/(juce::Time::getMillisecondCounterHiRes()-wall);
            timing<<e<<','<<size<<",48000,"<<48000./size<<','<<1000.*size/48000<<','<<reaction<<','<<rendered<<','<<fft.size()<<','
                  <<percentile(fft,.5)<<','<<percentile(fft,.99)<<','<<percentile(update,.5)<<','<<percentile(update,.99)<<','
                  <<percentile(path,.5)<<','<<percentile(path,.99)<<','<<percentile(paint,.5)<<','<<percentile(paint,.99)<<','
                  <<percentile(capture,.5)<<','<<percentile(capture,.99)<<','<<percentile(interval,.5)<<','<<percentile(interval,.99)<<','<<processCPU<<'\n';
            snapshot(editor,directory.getChildFile("eq-ui-bank"+juce::String(e)+"-fft"+juce::String(size)+".png"));
            std::cout<<"NATIVE synthetic audio bank="<<e<<" FFT="<<size<<" input="<<panel->spectrumDB(false,bin)
                     <<" output="<<panel->spectrumDB(true,bin)<<" first_data_observed="<<reaction<<" ms paint_p50="<<percentile(paint,.5)<<" us\n";
        }
        check(other->fftFrames()==hiddenFrames,"Hidden EQ performed spectrum transforms");
        for(int choice=1;choice<=3;++choice) {
            const auto frames=panel->fftFrames();smoothing->setSelectedId(choice,juce::sendNotificationSync);
            check(panel->analyzerHasData() && panel->fftFrames()==frames,"Smoothing change reset a live observation");
        }
        smoothing->setSelectedId(2,juce::sendNotificationSync);
        auto* input=find<juce::TextButton>(*panel,eqID(e,-1,"inputSpectrum"));
        auto* output=find<juce::TextButton>(*panel,eqID(e,-1,"outputSpectrum"));
        auto* master=find<juce::TextButton>(*panel,eqID(e,-1,"analyzer"));check(input && output && master,"Independent IN/OUT/LIVE controls missing");
        // PNG file encoding above is outside the timing interval and may block
        // the consumer long enough to trigger its correct overflow recovery.
        // Compare visible layers only after a fresh complete observation.
        const auto beforeLayers=panel->fftFrames();
        check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>beforeLayers;}),"Layer comparison did not recover a fresh audio observation after PNG encoding");
        const auto layerState=[&](const char* name){std::cout<<"NATIVE layer state bank="<<e<<' '<<name<<" hasData="<<panel->analyzerHasData()<<" frames="<<panel->fftFrames()<<" dropped="<<processor.graphicalEQ(e).analyzer.dropped.load()<<'\n';};
        layerState("both");
        const auto both=panel->createComponentSnapshot(panel->getLocalBounds());
        output->triggerClick();check(waitFor([&]{return !output->getToggleState() && panel->analyzerHasData();}),"OUT toggle did not retain a valid audio observation");
        layerState("in_only");
        const auto inOnly=panel->createComponentSnapshot(panel->getLocalBounds());
        check(spectrumPixels(both,panel->graphBounds().toNearestInt(),true)>spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),true)+30,
              "OUT visibility control did not remove the real output spectrum layer");
        input->triggerClick();check(waitFor([&]{return !input->getToggleState() && panel->analyzerHasData();}),"IN toggle did not retain a valid audio observation");
        layerState("neither");
        const auto neither=panel->createComponentSnapshot(panel->getLocalBounds());
        std::cout<<"NATIVE layer pixels bank="<<e<<" OUT both="<<spectrumPixels(both,panel->graphBounds().toNearestInt(),true)
                 <<" in_only="<<spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),true)
                 <<" IN in_only="<<spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),false)
                 <<" neither="<<spectrumPixels(neither,panel->graphBounds().toNearestInt(),false)<<'\n';
        check(spectrumPixels(inOnly,panel->graphBounds().toNearestInt(),false)>spectrumPixels(neither,panel->graphBounds().toNearestInt(),false)+30,
              "IN visibility control did not remove the real input spectrum layer");
        saveImage(both,directory.getChildFile("eq-ui-toggle-bank"+juce::String(e)+"-both.png"));
        saveImage(inOnly,directory.getChildFile("eq-ui-toggle-bank"+juce::String(e)+"-in-only.png"));
        saveImage(neither,directory.getChildFile("eq-ui-toggle-bank"+juce::String(e)+"-neither.png"));
        input->triggerClick();output->triggerClick();settle(50);
        master->triggerClick();check(waitFor([&]{return !processor.graphicalEQ(e).analyzer.enabled.load();}),"LIVE off still captures audio");
        const auto stoppedFrames=panel->fftFrames();settle(180);check(panel->fftFrames()==stoppedFrames,"LIVE off still transforms samples");
        master->triggerClick();check(waitFor([&]{return panel->analyzerHasData() && panel->fftFrames()>stoppedFrames;}),"LIVE re-enable did not acquire fresh input");
    }
    check(timing.good(),"Native timing evidence could not be written");

    auto* panel=find<GraphicalEQPanel>(editor,"finalEQ_panel");
    auto* resolution=find<juce::ComboBox>(*panel,eqID(1,-1,"resolution"));resolution->setSelectedId(2,juce::sendNotificationSync);
    const auto before=panel->fftFrames();audio.frequency.store(3000);
    check(waitFor([&]{return panel->fftFrames()>before && panel->spectrumDB(false,256)>-20.2f && panel->spectrumDB(false,86)<-70;}),"Native analyzer did not follow a new real input frequency");
    check(panel->spectrumDB(false,86)<-70,"Native raw spectrum retained the old frequency");
    check(audio.callbacks.load()>100,"Native proof did not run real paced audio callbacks");
    audio.stop();const auto atStop=panel->fftFrames();settle(100);
    const auto drained=panel->fftFrames();check(drained<=atStop+2,"Stopped producer triggered unbounded catch-up FFT work");
    const int peakBin=256;const float oldDisplay=panel->displaySpectrumDB(false,peakBin);settle(400);
    check(panel->fftFrames()==drained && panel->spectrumDB(false,peakBin)<=-119.9f && panel->displaySpectrumDB(false,peakBin)<oldDisplay,
          "Stopped audio did not decay on the native screen without fabricated FFT frames");
    snapshot(editor,directory.getChildFile("eq-ui-stopped-source.png"));
    std::cout<<"PASS native synthetic anti-phase sine / independent IN-OUT layers / master-off / stopped-source decay\n";

    // The motion proof now drives the entire production processBlock route, so
    // the product's global audio and CPU meters react alongside the real FFT.
    // Keep identical-polarity stereo here; anti-phase calibration was tested
    // independently above and must not rely on other amp/cab routing choices.
    processor.prepareToPlay(48000,128);
    auto* master=find<juce::TextButton>(*panel,eqID(1,-1,"analyzer"));
    master->triggerClick();check(waitFor([&]{return !processor.graphicalEQ(1).analyzer.enabled.load();}),"Motion capture reset did not stop the analyzer");
    master->triggerClick();check(waitFor([&]{return processor.graphicalEQ(1).analyzer.enabled.load();}),"Motion capture reset did not enable the analyzer");
    SyntheticAudio fullRoute(processor,true,48000,false);fullRoute.pluck.store(true);fullRoute.frequency.store(62.5);
    check(waitFor([&]{return panel->analyzerHasData() && processor.inputMeter()>.005f && processor.outputMeter()>1.e-5f;}),"Full-processor motion proof has no actual input/output audio");
    std::ofstream frames(directory.getChildFile("eq-analyzer-live-frames.csv").getFullPathName().toStdString());
    frames<<"frame,capture_start_ms,requested_frequency_hz,fft_size,fft_batches,rate,callback_samples,pluck_retrigger_s,pluck_decay_s,fixture,amplitude,right_gain,input_peak,output_peak\n";
    std::vector<juce::Image> liveFrames;liveFrames.reserve(32);
    const double sequenceStart=juce::Time::getMillisecondCounterHiRes();
    for(int frame=0;frame<32;++frame) {
        const double hz=frame<11?62.5:frame<22?187.5:750.;fullRoute.frequency.store(hz);settle(40);
        const double captured=juce::Time::getMillisecondCounterHiRes()-sequenceStart;
        check(panel->analyzerHasData(),"Full-processor live capture lost its audio observation");
        liveFrames.push_back(editor.createComponentSnapshot(editor.getLocalBounds()));check(liveFrames.back().isValid(),"Full-processor native capture failed");
        frames<<frame<<','<<captured<<','<<hz<<','<<panel->analysisFFTSize()<<','<<panel->fftFrames()
              <<",48000,128,0.65,0.18,synthetic_full_processBlock_harmonic_pluck,0.1,1,"<<processor.inputMeter()<<','<<processor.outputMeter()<<'\n';
    }
    fullRoute.stop();check(fullRoute.callbacks.load()>100,"Full-route proof did not produce real paced audio");
    check(frames.good(),"Live capture timestamps were not written");
    // Encoding is deliberately after the live observation and stop checks:
    // evidence generation must not manufacture UI starvation or FIFO drops.
    for(int frame=0;frame<int(liveFrames.size());++frame)
        saveImage(liveFrames[(size_t)frame],directory.getChildFile("eq-live-"+juce::String(frame).paddedLeft('0',3)+".png"));
    std::cout<<"PASS32 changing harmonic-pluck native frames through the full processBlock route / live input-output meters\n";

}
void fullProcessorNativeTiming(ChimeraProcessor& processor,ChimeraEditor& editor,const juce::File& directory) {
    processor.prepareToPlay(48000,128);
    auto* panel=find<GraphicalEQPanel>(editor,"finalEQ_panel");
    auto* resolution=find<juce::ComboBox>(*panel,eqID(1,-1,"resolution"));
    auto* master=find<juce::TextButton>(*panel,eqID(1,-1,"analyzer"));
    std::ofstream output(directory.getChildFile("eq-analyzer-full-callback.csv").getFullPathName().toStdString());
    output<<"capture,fft_size,rate,block,channels,mode,latency_samples,callbacks,whole_processor_p50_us,whole_processor_p99_us,whole_processor_max_us,budget_us,overruns\n";
    for(int size:{0,2048,4096,8192}) {
        const bool enabled=size!=0;
        if(master->getToggleState()!=enabled){master->triggerClick();settle(50);}
        if(enabled)resolution->setSelectedId(size==2048?1:size==4096?2:3,juce::sendNotificationSync);
        SyntheticAudio audio(processor,true);settle(1800);audio.stop();
        const auto& all=audio.callbackMicrosAfterStop();check(all.size()>400,"Insufficient whole-processor callbacks while native UI was active");
        const std::vector<double> samples(all.begin()+100,all.end());const double budget=128./48000*1.e6;
        const auto overruns=std::count_if(samples.begin(),samples.end(),[budget](double value){return value>budget;});
        output<<enabled<<','<<size<<",48000,128,2,"<<get(processor,"mode")<<','<<processor.getLatencySamples()<<','<<samples.size()<<','
              <<percentile(samples,.5)<<','<<percentile(samples,.99)<<','<<*std::max_element(samples.begin(),samples.end())<<','<<budget<<','<<overruns<<'\n';
        std::cout<<"NATIVE whole processor FFT="<<size<<" p50/p99="<<percentile(samples,.5)<<'/'<<percentile(samples,.99)<<" us; callbacks="<<samples.size()<<" overruns="<<overruns<<'\n';
    }
    check(output.good(),"Whole-processor timing evidence could not be written");
    // One high-rate observation resolves the FIFO's actual timer headroom on
    // this runner. Scheduling overruns are measured, never silently hidden.
    processor.prepareToPlay(192000,128);resolution->setSelectedId(3,juce::sendNotificationSync);settle(60);
    const auto before=processor.graphicalEQ(1).analyzer.dropped.load();
    const double observedStart=juce::Time::getMillisecondCounterHiRes();
    SyntheticAudio highRate(processor,true,192000);settle(2600);highRate.stop();
    const double observedElapsed=juce::Time::getMillisecondCounterHiRes()-observedStart;
    const auto producedSamples=highRate.callbacks.load()*128;
    const double effectiveRate=1000*double(producedSamples)/observedElapsed;
    const auto lost=processor.graphicalEQ(1).analyzer.dropped.load()-before;
    std::ofstream high(directory.getChildFile("eq-analyzer-high-rate.csv").getFullPathName().toStdString());
    high<<"rate,block,fft_size,fifo_capacity,requested_capture_ms,observed_elapsed_ms,callbacks,produced_samples,effective_samples_per_second,production_ratio,dropped_samples\n192000,128,8192,"<<EQAnalyzerFIFO::capacity<<",2600,"<<observedElapsed<<','<<highRate.callbacks.load()<<','<<producedSamples<<','<<effectiveRate<<','<<effectiveRate/192000<<','<<lost<<'\n';
    check(high.good(),"High-rate FIFO evidence could not be written");
    std::cout<<"NATIVE192k FFT8192 callbacks="<<highRate.callbacks.load()<<" effective_samples_per_second="<<effectiveRate<<" dropped_samples="<<lost<<" FIFOcapacity="<<EQAnalyzerFIFO::capacity<<'\n';
    processor.prepareToPlay(48000,128);
}
}
int main(int argc,char** argv){try {
    juce::ScopedJuceInitialiser_GUI init;auto processor=std::make_unique<ChimeraProcessor>();auto editor=std::make_unique<ChimeraEditor>(*processor);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);check(editor->getPeer()!=nullptr,"No native UI peer");
    auto* eqTab=tab(*editor,"EQ");check(eqTab!=nullptr,"EQ navigation missing");eqTab->triggerClick();settle(80);
    const juce::File directory=juce::File::getCurrentWorkingDirectory().getChildFile(argc>1?argv[1]:"eq-ui");directory.createDirectory();
    for(int e=0;e<2;++e){
        tab(*editor,"RIGS")->triggerClick();settle(80);
        auto* path=find<juce::TextButton>(*editor,e==0?"path.tone.eq":"path.final.eq");check(path,"EQ path target missing");path->triggerClick();
        auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(panel && waitFor([&]{return panel->isShowing() && panel->isNavigationSelected();}),"EQ path did not open the requested bank");
        auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));check(selector,"Band chooser missing");
        HostEvents host;auto* frequency=processor->parameters().getParameter(eqID(e,0,"frequency"));auto* gain=processor->parameters().getParameter(eqID(e,0,"gain"));auto* q=processor->parameters().getParameter(eqID(e,0,"q"));frequency->addListener(&host);gain->addListener(&host);q->addListener(&host);
        for(int b=0;b<12;++b){selector->setSelectedId(b+1,juce::sendNotificationSync);
            auto* f=find<juce::Slider>(*panel,eqID(e,b,"frequency"));auto* g=find<juce::Slider>(*panel,eqID(e,b,"gain"));auto* qs=find<juce::Slider>(*panel,eqID(e,b,"q"));check(f && g && qs,"Node controls missing");
            numeric(*f,juce::String(200+b*317)+" Hz");numeric(*g,"3.5 dB");numeric(*qs,"1.25 Q");
            std::cout<<"Numeric "<<e<<":"<<b<<" Hz="<<get(*processor,eqID(e,b,"frequency"))<<" dB="<<get(*processor,eqID(e,b,"gain"))<<" Q="<<get(*processor,eqID(e,b,"q"))<<'\n';
            check(std::abs(get(*processor,eqID(e,b,"frequency"))-(200+b*317))<.1 && std::abs(get(*processor,eqID(e,b,"gain"))-3.5f)<.00001f && std::abs(get(*processor,eqID(e,b,"q"))-1.25f)<.001,"Numeric node edit did not reach host");
            auto* on=find<juce::TextButton>(*panel,eqID(e,b,"enabled"));on->triggerClick();check(waitFor([&]{return get(*processor,eqID(e,b,"enabled"))==1;}),"Band activation not connected");
            auto* filter=find<juce::ComboBox>(*panel,eqID(e,b,"type"));filter->setSelectedId(4,juce::sendNotificationSync);check(get(*processor,eqID(e,b,"type"))==3 && !g->isEnabled(),"HP choice / gain applicability");
            filter->setSelectedId(1,juce::sendNotificationSync);
            if(b==0)for(int t=1;t<=7;++t){filter->setSelectedId(t,juce::sendNotificationSync);check(get(*processor,eqID(e,b,"type"))==float(t-1),"Filter menu did not reach host");}
            filter->setSelectedId(1,juce::sendNotificationSync);
        }
        selector->setSelectedId(1,juce::sendNotificationSync);const auto start=panel->nodePosition(0),end=start+juce::Point<float>(40,-18);
        panel->mouseDown(mouse(*panel,start,start,false));panel->mouseDrag(mouse(*panel,end,start,true));panel->mouseUp(mouse(*panel,end,start,true));
        check(get(*processor,eqID(e,0,"frequency"))>200 && get(*processor,eqID(e,0,"gain"))>3.5,"Graph drag not connected");
        juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;panel->mouseWheelMove(mouse(*panel,end,end,false),wheel);check(get(*processor,eqID(e,0,"q"))>1.25,"Wheel Q not connected");
        check(host.starts>=3 && host.starts==host.ends && host.changes>0,"Unbalanced/missing host gestures");frequency->removeListener(&host);gain->removeListener(&host);q->removeListener(&host);
        auto* bypass=find<juce::TextButton>(*panel,eqID(e,-1,"bypass"));bypass->triggerClick();check(waitFor([&]{return !processor->graphicalEQ(e).isBypassed();}),"Bypass not connected");
        for(int b=0;b<12;++b)set(*processor,eqID(e,b,"enabled"),b==0?1.f:0.f);set(*processor,eqID(e,0,"frequency"),1007.8125f);set(*processor,eqID(e,0,"gain"),6);processor->graphicalEQ(e).prepare(48000);
    }
    liveAnalyzer(*processor,*editor,directory);
    fullProcessorNativeTiming(*processor,*editor,directory);
    // The same visible editor must track automation, A/B and project recall for
    // every band in both banks, including values changed while another tab is open.
    for(int e=0;e<2;++e)for(int b=0;b<12;++b){set(*processor,eqID(e,b,"frequency"),float(180+e*43+b*319));set(*processor,eqID(e,b,"gain"),float(b-6+e));set(*processor,eqID(e,b,"q"),.5f+float(b)*.1f);}
    const auto verifyBanks=[&]{
        for(int e=0;e<2;++e){auto* path=find<juce::TextButton>(*editor,e==0?"path.tone.eq":"path.final.eq");path->triggerClick();
            auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(waitFor([&]{return panel->isShowing()&&panel->isNavigationSelected();}),"Restored EQ navigation stale");
            auto* selector=find<juce::ComboBox>(*panel,eqID(e,-1,"band"));
            for(int b=0;b<12;++b){selector->setSelectedId(b+1,juce::sendNotificationSync);for(const char* field:{"frequency","gain","q"}){
                auto* control=find<juce::Slider>(*panel,eqID(e,b,field));check(control && std::abs(control->getValue()-get(*processor,eqID(e,b,field)))<.02,"Restored or automated numeric EQ control is stale");
            }}selector->setSelectedId(1,juce::sendNotificationSync);
        }
    };
    verifyBanks();processor->copyComparison();juce::MemoryBlock saved;processor->getStateInformation(saved);
    processor->selectComparison(1);for(int e=0;e<2;++e)for(int b=0;b<12;++b)set(*processor,eqID(e,b,"gain"),12);verifyBanks();
    processor->selectComparison(0);verifyBanks();
    for(int e=0;e<2;++e)for(int b=0;b<12;++b)check(std::abs(get(*processor,eqID(e,b,"gain"))-float(b-6+e))<.001,"A/B bank recall changed a node");
    processor->loadFactoryPreset(0);verifyBanks();processor->setStateInformation(saved.getData(),int(saved.getSize()));verifyBanks();
    for(int e=0;e<2;++e)for(int b=0;b<12;++b)check(std::abs(get(*processor,eqID(e,b,"gain"))-float(b-6+e))<.001,"Project recall changed a node");
    for(int mode:{1,2,0}){set(*processor,"mode",float(mode));settle(100);check(find<GraphicalEQPanel>(*editor,"finalEQ_panel")->isNavigationSelected(),"Mode switch lost the global EQ selection");}
    tab(*editor,"RIGS")->triggerClick();settle(50);eqTab->triggerClick();settle(50);
    check(find<GraphicalEQPanel>(*editor,"finalEQ_panel")->isShowing(),"Final EQ selection did not survive leaving and returning to EQ");
    auto* gatePath=find<juce::TextButton>(*editor,"path.gate");check(gatePath,"Utility path target missing");gatePath->triggerClick();settle(50);eqTab->triggerClick();settle(50);
    check(find<GraphicalEQPanel>(*editor,"finalEQ_panel")->isShowing(),"Utility navigation overwrote the chosen EQ bank");
    for(int percent:{75,100,125,150}){editor->setSize(1180*percent/100,780*percent/100);settle(70);
        for(int e=0;e<2;++e){find<juce::TextButton>(*editor,e==0?"eq.bank.tone":"eq.bank.final")->triggerClick();settle(40);auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(panel->isShowing(),"Scaled bank is hidden");const auto local=panel->nodePosition(0);
            const auto global=editor->getLocalPoint(panel,local);const auto finish=global+juce::Point<float>(float(percent)/10,0);
            const auto start=panel->getLocalPoint(editor.get(),global),end=panel->getLocalPoint(editor.get(),finish);
            const float previous=get(*processor,eqID(e,0,"frequency"));panel->mouseDown(mouse(*panel,start,start,false));panel->mouseDrag(mouse(*panel,end,start,true));panel->mouseUp(mouse(*panel,end,start,true));
            check(get(*processor,eqID(e,0,"frequency"))>previous,"Graph transform drag failed at UI scale");set(*processor,eqID(e,0,"frequency"),previous);}
        for(int e=0;e<2;++e){auto* panel=find<GraphicalEQPanel>(*editor,e==0?"toneEQ_panel":"finalEQ_panel");check(editor->getLocalBounds().contains(editor->getLocalArea(panel,panel->getLocalBounds())),"EQ clipped at scale");for(const char* field:{"frequency","gain","q"}){auto* s=find<juce::Slider>(*panel,eqID(e,0,field));check(editor->getLocalBounds().contains(editor->getLocalArea(s,s->getLocalBounds())),"Numeric controls clipped");}}
        snapshot(*editor,directory.getChildFile("eq-ui-"+juce::String(percent)+".png"));
        const auto t=juce::Time::getMillisecondCounterHiRes();const auto cpu=processCPUSeconds();settle(1000);const double elapsed=juce::Time::getMillisecondCounterHiRes()-t;std::cout<<"UI idle "<<percent<<"% = "<<100000.*(processCPUSeconds()-cpu)/elapsed<<"% one core (1 second no audio, process CPU)\n";
    }
    tab(*editor,"RIGS")->triggerClick();for(int e=0;e<2;++e)check(waitFor([&]{return !processor->graphicalEQ(e).analyzer.enabled.load();}),"Hidden EQ analyzer still enabled");
    editor.reset();for(int e=0;e<2;++e)check(!processor->graphicalEQ(e).analyzer.enabled.load(),"Closed editor analyzer still enabled");processor->releaseResources();
    std::cout<<"PASS real native peer / 24 numeric nodes / drag / Q / host gestures / 7 choices / stereo FFT / 75-150% / hidden and close\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

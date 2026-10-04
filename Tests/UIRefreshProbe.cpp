#include "PluginEditor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <thread>
#if JUCE_WINDOWS
#include <windows.h>
#else
#include <time.h>
#endif

// One core = 100%. Only the message thread is UI CPU; the paced synthetic
// callback thread is measured separately. This is not a DAW/driver benchmark.
static double threadCPU() {
#if JUCE_WINDOWS
    FILETIME created, exited, kernel, user;
    GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user);
    ULARGE_INTEGER k{}, u{}; k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return (k.QuadPart+u.QuadPart)*1e-7;
#else
    timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;
#endif
}
static void pump(int ms) {juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
static void set(ChimeraProcessor& p,const char* id,float value) {
    auto* parameter=p.parameters().getParameter(id);
    if(!parameter)throw std::runtime_error(std::string("Missing parameter: ")+id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static juce::TextButton* button(juce::Component& parent,const juce::String& name) {
    if(auto* b=dynamic_cast<juce::TextButton*>(&parent);b && b->getButtonText()==name)return b;
    for(auto* child:parent.getChildren())if(auto* b=button(*child,name))return b;
    return nullptr;
}
struct ProbeEditor final : ChimeraEditor {
    using ChimeraEditor::ChimeraEditor;
    uint64_t paints{}, fullPaints{}, clipPixels{};
    void paint(juce::Graphics& g) override {
        ++paints;const auto clip=g.getClipBounds();clipPixels+=(uint64_t)clip.getWidth()*clip.getHeight();
        if(clip==getLocalBounds())++fullPaints;
        ChimeraEditor::paint(g);
    }
    void reset() {paints=fullPaints=clipPixels=0;}
};
struct AudioRunner {
    ChimeraProcessor& processor;std::atomic<bool> stop{false};std::thread worker;
    std::vector<double> durations;double cpuSeconds{};uint64_t hash{1469598103934665603ull};
    explicit AudioRunner(ChimeraProcessor& p):processor(p) {
        worker=std::thread([this] {
            juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
            auto deadline=std::chrono::steady_clock::now();const double start=threadCPU();
            // Fixed input length: open/closed hashes compare exactly despite
            // differences in startup/message dispatch duration.
            for(int block=0;block<1500 && !stop.load();++block) {
                for(int n=0;n<128;++n) {
                    const int sample=block*128+n;
                    const float envelope=(sample/12000)%2?.25f:1.f;
                    const float v=.15f*envelope*std::sin(float(sample)*2.f*juce::MathConstants<float>::pi*110.f/48000.f);
                    audio.setSample(0,n,v);audio.setSample(1,n,v);
                }
                const auto begin=std::chrono::steady_clock::now();processor.processBlock(audio,midi);
                durations.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count());
                for(int c=0;c<2;++c)for(int n=0;n<128;++n) {
                    uint32_t bits{};const float value=audio.getSample(c,n);std::memcpy(&bits,&value,sizeof(bits));hash=(hash^bits)*1099511628211ull;
                }
                deadline+=std::chrono::nanoseconds(2666667);std::this_thread::sleep_until(deadline);
            }
            cpuSeconds=threadCPU()-start;
        });
    }
    void finish(){if(worker.joinable())worker.join();}
    ~AudioRunner(){stop=true;finish();}
};
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc<3)throw std::runtime_error("usage: ChimeraUIRefreshProbe report.json source-revision [repeats=3]");
        const int repeats=argc>3?std::max(1,atoi(argv[3])):3;
        juce::Array<juce::var> rows;
        struct Scene {const char* name;const char* tab;bool editor,audio,hidden,switching;float scale;int instances;};
        const Scene scenes[]{
            {"closed_idle","",false,false,false,false,1,1},
            {"closed_audio","",false,true,false,false,1,1},
            {"open_idle_pre","PRE",true,false,false,false,1,1},
            {"open_idle_rigs","RIGS",true,false,false,false,1,1},
            {"open_idle_post","POST",true,false,false,false,1,1},
            {"meter_rigs","RIGS",true,true,false,false,1,1},
            {"meter_post","POST",true,true,false,false,1,1},
            {"meter_tuner","RIGS",true,true,false,false,1,1},
            {"panel_switch","RIGS",true,false,false,true,1,1},
            {"idle_rigs_75","RIGS",true,false,false,false,.75f,1},
            {"meter_post_150","POST",true,true,false,false,1.5f,1},
            {"hidden_editor","RIGS",true,false,true,false,1,1},
            {"two_idle_rigs","RIGS",true,false,false,false,1,2}
        };
        for(int repeat=0;repeat<repeats;++repeat)for(const auto& scene:scenes) {
            std::vector<std::unique_ptr<ChimeraProcessor>> processors;
            std::vector<std::unique_ptr<ProbeEditor>> editors;
            std::vector<std::vector<float>> parameterSnapshots;
            double openCPU=0,openWall=0;
            for(int i=0;i<scene.instances;++i) {
                auto p=std::make_unique<ChimeraProcessor>();
                // Deterministic filter-only IR path avoids asynchronous kernel
                // installation changing the closed/open audio comparison.
                for(const char* id:{"cabtype1","cabtype2","cabtype3"})set(*p,id,0);
                set(*p,"mode",2);if(juce::String(scene.name)=="meter_tuner"){set(*p,"tuneron",1);set(*p,"tunermute",0);}
                p->setPlayConfigDetails(2,2,48000,128);p->prepareToPlay(48000,128);
                std::vector<float> values;for(auto* parameter:p->getParameters())values.push_back(parameter->getValue());parameterSnapshots.push_back(values);
                if(scene.editor) {
                    const auto openStart=std::chrono::steady_clock::now();const auto openCpuStart=threadCPU();
                    auto e=std::make_unique<ProbeEditor>(*p);e->setSize(juce::roundToInt(1180*scene.scale),juce::roundToInt(780*scene.scale));
                    e->addToDesktop(juce::ComponentPeer::windowIsTemporary);e->setVisible(true);
                    if(!e->getPeer() || !e->getPeer()->getNativeHandle())throw std::runtime_error("No native display peer: CPU benchmark invalid");
                    auto* tab=button(*e,scene.tab);if(!tab)throw std::runtime_error("Missing tab");tab->onClick();
                    if(scene.hidden)e->setVisible(false);editors.push_back(std::move(e));
                    openCPU+=threadCPU()-openCpuStart;openWall+=std::chrono::duration<double>(std::chrono::steady_clock::now()-openStart).count();
                }
                processors.push_back(std::move(p));
            }
            pump(600);for(auto& e:editors){if(!scene.hidden && e->paints==0)throw std::runtime_error("No native paint dispatched: CPU benchmark invalid");e->reset();}
            std::unique_ptr<AudioRunner> audio;if(scene.audio)audio=std::make_unique<AudioRunner>(*processors[0]);
            const auto start=std::chrono::steady_clock::now();const double cpu=threadCPU();
            const int duration=scene.audio?4200:2200;
            int switches=0;
            while(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<duration) {
                pump(100);
                if(scene.switching){auto* b=button(*editors[0],(++switches%3)==0?"RIGS":switches%3==1?"PRE":"POST");b->onClick();}
            }
            const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const double used=threadCPU()-cpu;
            auto row=std::make_unique<juce::DynamicObject>();row->setProperty("scene",scene.name);row->setProperty("repeat",repeat);
            row->setProperty("scale",scene.scale);row->setProperty("instances",scene.instances);row->setProperty("wall_seconds",wall);
            row->setProperty("editor_construct_ui_cpu_ms",openCPU*1000);row->setProperty("editor_construct_wall_ms",openWall*1000);
            row->setProperty("ui_thread_cpu_seconds",used);row->setProperty("ui_one_core_percent",100*used/wall);
            uint64_t paints=0,full=0,pixels=0;for(auto& e:editors){paints+=e->paints;full+=e->fullPaints;pixels+=e->clipPixels;}
            row->setProperty("editor_paints",(juce::int64)paints);row->setProperty("full_editor_paints",(juce::int64)full);row->setProperty("editor_clip_pixels",(juce::int64)pixels);
            if(audio) {
                audio->finish();auto times=audio->durations;std::sort(times.begin(),times.end());
                const auto quantile=[&](double q){return times[(size_t)(q*(times.size()-1))];};
                row->setProperty("callback_p95_us",quantile(.95));row->setProperty("callback_p99_us",quantile(.99));
                row->setProperty("callback_mean_us",std::accumulate(times.begin(),times.end(),0.)/times.size());
                row->setProperty("callback_deadline_misses",(int)std::count_if(times.begin(),times.end(),[](double t){return t>128./48000*1e6;}));
                row->setProperty("callback_count",(int)times.size());row->setProperty("audio_thread_cpu_seconds",audio->cpuSeconds);row->setProperty("audio_one_core_percent",audio->cpuSeconds/4.*100.);
                row->setProperty("audio_output_fnv64",juce::String::toHexString((juce::int64)audio->hash));
            }
            bool unchanged=true;for(size_t i=0;i<processors.size();++i){int k=0;for(auto* p:processors[i]->getParameters())unchanged &= p->getValue()==parameterSnapshots[i][(size_t)k++];}
            row->setProperty("parameters_unchanged",unchanged);if(!unchanged)throw std::runtime_error("UI mutated processor state");
            rows.add(juce::var(row.release()));std::cout<<scene.name<<" repeat="<<repeat<<" UI="<<100*used/wall<<"% paints="<<paints<<" fullPaints="<<full<<std::endl;
            editors.clear();audio.reset();for(auto& p:processors)p->releaseResources();processors.clear();pump(80);
        }
        auto report=std::make_unique<juce::DynamicObject>();report->setProperty("source_revision",argv[2]);report->setProperty("os",juce::SystemStats::getOperatingSystemName());
        report->setProperty("cpu",juce::SystemStats::getCpuModel());report->setProperty("juce",juce::SystemStats::getJUCEVersion());
        report->setProperty("sample_rate",48000);report->setProperty("block_size",128);report->setProperty("fixture","synthetic stereo 110 Hz, 1500 blocks, filter-only cab, Matrix default");
        report->setProperty("scope","actual desktop peer; message-thread CPU only; synthetic paced callback; no DAW/driver or display-server CPU");
        report->setProperty("rows",rows);report->setProperty("ui_75_percent_stretch","BLOCKED");
        if(!juce::File(argv[1]).replaceWithText(juce::JSON::toString(juce::var(report.release()))))throw std::runtime_error("Cannot save report");
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}

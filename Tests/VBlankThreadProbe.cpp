// Fault-injection adapter around the actual JUCE VBlankThread class extracted
// at configure time. No production loop or destructor is reimplemented here.
// This tests the shutdown algorithm, not a physical Windows graphics driver.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace juce {
using HMONITOR=void*;
using HRESULT=int;
constexpr HRESULT S_OK=0;
constexpr HRESULT E_FAIL=-1;
constexpr bool FAILED(HRESULT value) {return value<0;}
struct DXGI_OUTPUT_DESC {bool AttachedToDesktop{true};HMONITOR Monitor{};};
struct IDXGIOutput {
    enum class Scenario {alwaysFail,success,mixed};
    explicit IDXGIOutput(Scenario chosen):scenario(chosen) {}
    HRESULT WaitForVBlank() {
        const int call=++calls;
        if(scenario==Scenario::alwaysFail || (scenario==Scenario::mixed && call%3==0)) {
            ++failures;return E_FAIL;
        }
        ++successes;std::this_thread::sleep_for(std::chrono::milliseconds(1));return S_OK;
    }
    HRESULT GetDesc(DXGI_OUTPUT_DESC* desc) {desc->AttachedToDesktop=true;desc->Monitor=this;return S_OK;}
    Scenario scenario;
    std::atomic<int> calls{},failures{},successes{};
};
template<class T> using ComSmartPtr=std::shared_ptr<T>;
struct ComponentPeer {
    struct VBlankListener {virtual ~VBlankListener()=default;virtual void onVBlank(double)=0;};
};
struct SystemStats {static std::string getJUCEVersion() {return "JUCE real-source fixture";}};
struct Time {
    static double getMillisecondCounterHiRes() {
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};
class Thread {
public:
    enum class Priority {highest};
    explicit Thread(const std::string&) {}
    virtual ~Thread() {if(worker.joinable())worker.join();}
    void startThread(Priority) {worker=std::thread([this]{++active;run();--active;});}
    bool stopThread(int timeout) {
        if(timeout!=-1)throw std::runtime_error("Fixture expects the production unbounded join");
        shouldExit=true;if(worker.joinable())worker.join();return true;
    }
    static void sleep(int milliseconds) {std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));}
    bool threadShouldExit() const {return shouldExit.load();}
    static inline std::atomic<int> active{};
private:
    virtual void run()=0;
    std::thread worker;
    std::atomic<bool> shouldExit{};
};
class AsyncUpdater {
public:
    virtual ~AsyncUpdater()=default;
    void triggerAsyncUpdate() {++triggered;}
    void cancelPendingUpdate() {++cancelled;}
    static inline std::atomic<int> triggered{},cancelled{};
private:
    virtual void handleAsyncUpdate()=0;
};
}
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Type) Type(const Type&)=delete; Type& operator=(const Type&)=delete;
#define JUCE_DECLARE_NON_MOVEABLE(Type) Type(Type&&)=delete; Type& operator=(Type&&)=delete;
#include CHIMERA_VBLANK_CLASS_HEADER

int main(int argc,char** argv) {
    std::cout<<std::unitbuf;
    try {
        if(argc!=3 || std::string(argv[1])!="--scenario")throw std::runtime_error("Expected --scenario always-fail|success|mixed");
        const std::string name=argv[2];
        using Scenario=juce::IDXGIOutput::Scenario;
        if(name!="always-fail" && name!="success" && name!="mixed")throw std::runtime_error("Unknown failure scenario");
        const auto scenario=name=="always-fail"?Scenario::alwaysFail:name=="success"?Scenario::success:Scenario::mixed;
        auto output=std::make_shared<juce::IDXGIOutput>(scenario);
        struct Listener final:juce::ComponentPeer::VBlankListener {void onVBlank(double) override {}} listener;
        auto worker=std::make_unique<juce::VBlankThread>(output,output.get(),listener);
        const auto readyDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        while(output->calls.load()<12 && std::chrono::steady_clock::now()<readyDeadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(output->calls.load()<12)throw std::runtime_error("Worker did not reach the injected VBlank outcomes");
        if(scenario!=Scenario::success && output->failures.load()==0)throw std::runtime_error("Failure injection never ran");
        if(scenario!=Scenario::alwaysFail && (output->successes.load()==0 || juce::AsyncUpdater::triggered.load()==0))
            throw std::runtime_error("Successful VBlank path never ran");
        std::cout<<"STAGE destruct scenario="<<name<<" calls="<<output->calls.load()<<" failures="<<output->failures.load()<<'\n';
        const auto start=std::chrono::steady_clock::now();
        worker.reset(); // The extracted production destructor performs the join.
        const double milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(milliseconds>=1000.0 || juce::Thread::active.load()!=0 || juce::AsyncUpdater::cancelled.load()!=1)
            throw std::runtime_error("VBlank shutdown was not complete within 1000 ms");
        std::cout<<"RESULT {\"scenario\":\""<<name<<"\",\"shutdown_ms\":"<<milliseconds
                 <<",\"calls\":"<<output->calls.load()<<",\"failures\":"<<output->failures.load()
                 <<",\"successes\":"<<output->successes.load()<<",\"active_threads\":"<<juce::Thread::active.load()<<"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

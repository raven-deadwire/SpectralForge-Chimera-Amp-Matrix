#!/usr/bin/env python3
"""Generate isolated variants; never edit production Source/."""
import argparse, hashlib, json, pathlib, shutil, subprocess

PIN = 'c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
ROOT = pathlib.Path(__file__).resolve().parents[2]

def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)

def prepare(dest):
    dest = pathlib.Path(dest).resolve()
    if dest.exists(): raise RuntimeError('Use a fresh output directory')
    manifest = {}
    for variant in ('A', 'B'):
        target = dest / variant
        target.mkdir(parents=True)
        # Read pinned Git objects, independent of current worktree or branch.
        names = subprocess.check_output(['git','ls-tree','-r','--name-only',PIN,'Source','Tests'],cwd=ROOT,text=True).splitlines()
        for name in names:
            if not name.endswith(('.h','.cpp')): continue
            data = subprocess.check_output(['git','show',f'{PIN}:{name}'],cwd=ROOT)
            path=target/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
            manifest[name]=hashlib.sha256(data).hexdigest()
    p=dest/'B/Source/ModeledCabConvolution.h';s=p.read_text()
    s=replace(s,'#include <complex>','#include <complex>\n#include <array>\n#include <cstring>')
    s=replace(s,'public:\n    // Only immutable',r'''public:
    // EXPERIMENT ONLY: audio-owned scratch, lifetime limited to one Cab callback.
    // Never share FFT plans, rings, cursors, tails, Prepared ownership or workers.
    // Full time-domain byte equality is the guard; no approximate signal hashing.
    struct ForwardCache {
        static constexpr int capacity=12, floats=2048;
        struct Entry {int count{},bins{};bool mono{};std::array<float,floats> input{},spectrum{};};
        std::array<Entry,capacity> entries;
        int used{};
        unsigned hits{},misses{},overflow{};
        void begin() noexcept {used=0;hits=misses=overflow=0;}
        bool load(const void* input,int count,bool mono,int bins,float* output) noexcept {
            if(count>floats || 2*bins>floats) {++misses;return false;}
            for(int i=0;i<used;++i) {
                const auto& e=entries[size_t(i)];
                if(e.count==count && e.mono==mono && e.bins==bins
                    && std::memcmp(e.input.data(),input,size_t(count)*sizeof(float))==0) {
                    std::memcpy(output,e.spectrum.data(),size_t(2*bins)*sizeof(float));++hits;return true;
                }
            }
            ++misses;return false;
        }
        void save(const void* input,int count,bool mono,int bins,const float* spectrum) noexcept {
            if(used==capacity || count>floats || 2*bins>floats) {++overflow;return;}
            auto& e=entries[size_t(used++)];e.count=count;e.mono=mono;e.bins=bins;
            std::memcpy(e.input.data(),input,size_t(count)*sizeof(float));
            std::memcpy(e.spectrum.data(),spectrum,size_t(2*bins)*sizeof(float));
        }
    };
    // Only immutable''')
    s=replace(s,'void processMono(juce::AudioBuffer<float>& buffer) noexcept','void processMono(juce::AudioBuffer<float>& buffer,ForwardCache* cache) noexcept')
    old='''            {
                SF_CAB_PROFILE_SCOPE(FFTForward);
                juce::FloatVectorOperations::copy(monoTransform.data(),monoInput.data(),fftSize);'''
    new='''            const auto bank=currentInputBank();
            auto* currentInput=inputSpectra.data()+size_t(bank.start+bank.offset)*size_t(2*bins);
            const bool reused=cache && cache->load(monoInput.data(),fftSize,true,bins,currentInput);
            if(!reused) {
                SF_CAB_PROFILE_SCOPE(FFTForward);
                juce::FloatVectorOperations::copy(monoTransform.data(),monoInput.data(),fftSize);'''
    s=replace(s,old,new)
    s=replace(s,'''                fft.performRealOnlyForwardTransform(monoTransform.data(),true);
            }
            {
                SF_CAB_PROFILE_SCOPE(SpectralMAC);
                const auto bank=currentInputBank();
                auto* currentInput=inputSpectra.data()+size_t(bank.start+bank.offset)*size_t(2*bins);
                packRealSpectrum(monoTransform.data(),currentInput,bins);''','''                fft.performRealOnlyForwardTransform(monoTransform.data(),true);
                packRealSpectrum(monoTransform.data(),currentInput,bins);
                if(cache)cache->save(monoInput.data(),fftSize,true,bins,currentInput);
            }
            {
                SF_CAB_PROFILE_SCOPE(SpectralMAC);''')
    s=replace(s,'void process(juce::AudioBuffer<float>& buffer) noexcept','void process(juce::AudioBuffer<float>& buffer,ForwardCache* cache=nullptr) noexcept')
    s=replace(s,'processMono(buffer);return;','processMono(buffer,cache);return;')
    s=replace(s,'''            {
                SF_CAB_PROFILE_SCOPE(FFTForward);
                fft.perform(inputTime.data(),transform.data(),false);
            }
            {
                SF_CAB_PROFILE_SCOPE(SpectralMAC);
                const auto bank=currentInputBank();
                auto* currentInput=inputSpectra.data()+size_t(bank.start+bank.offset)*size_t(2*fftSize);
                packSpectrum(transform.data(),currentInput,fftSize);''','''            const auto bank=currentInputBank();
            auto* currentInput=inputSpectra.data()+size_t(bank.start+bank.offset)*size_t(2*fftSize);
            static_assert(sizeof(Complex)==2*sizeof(float));
            const bool reused=cache && cache->load(inputTime.data(),2*fftSize,false,bins,currentInput);
            if(!reused) {
                SF_CAB_PROFILE_SCOPE(FFTForward);
                fft.perform(inputTime.data(),transform.data(),false);
                packSpectrum(transform.data(),currentInput,fftSize);
                if(cache)cache->save(inputTime.data(),2*fftSize,false,bins,currentInput);
            }
            {
                SF_CAB_PROFILE_SCOPE(SpectralMAC);''')
    p.write_text(s)
    p=dest/'B/Source/Cabinet.h';s=p.read_text()
    s=replace(s,'void process(juce::AudioBuffer<float>& buffer)\n','void process(juce::AudioBuffer<float>& buffer,ModeledCabConvolution::ForwardCache* cache=nullptr)\n')
    s=replace(s,'modeledConvolution->process(buffer);','modeledConvolution->process(buffer,cache);')
    s=replace(s,'    std::unique_ptr<Cab> micB;','    std::unique_ptr<Cab> micB;\n    std::unique_ptr<ModeledCabConvolution::ForwardCache> forwardCache;')
    s=replace(s,'        bWasRunning=false; preferSecondSwap=false;','        bWasRunning=false; preferSecondSwap=false;\n        if(micB)forwardCache=std::make_unique<ModeledCabConvolution::ForwardCache>();')
    s=replace(s,'    void process(juce::AudioBuffer<float>& buffer,bool allowKernelSwap=true)\n    {','''    const auto* cacheStats() const noexcept {return forwardCache.get();}
    void process(juce::AudioBuffer<float>& buffer,bool allowKernelSwap=true,ModeledCabConvolution::ForwardCache* cache=nullptr)
    {
        if(!cache && forwardCache) {forwardCache->begin();cache=forwardCache.get();}''')
    s=replace(s,'micB->process(second,!serialiseModels || fadeRemaining==0);','micB->process(second,!serialiseModels || fadeRemaining==0,cache);')
    s=replace(s,'active->process(buffer);','active->process(buffer,cache);')
    s=replace(s,'fading->process(alternate);','fading->process(alternate,cache);')
    p.write_text(s)
    (dest/'manifest.json').write_text(json.dumps({'source':PIN,'sha256':manifest,'release_approved':False},indent=2)+'\n')
    print(dest)

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('destination');prepare(a.parse_args().destination)

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cstdint>

namespace spectralforge::autoset {

// Lives for the controller's lifetime, including between runs. The audio-thread
// listener only updates lock-free atomics; it never touches snapshots or UI.
class ParameterEdits final : private juce::AudioProcessorParameter::Listener {
public:
    explicit ParameterEdits(juce::RangedAudioParameter& p):parameter(p) {parameter.addListener(this);}
    ~ParameterEdits() override {parameter.removeListener(this);}
    uint32_t revision() const noexcept {return edits.load(std::memory_order_acquire);}
    bool unchanged(uint32_t expected) const noexcept {
        return !gesture.load(std::memory_order_acquire) && revision()==expected;
    }
    bool write(float value,float previous,uint32_t expected) {
        const auto owned=[&]{return unchanged(expected) && parameter.getValue()==previous;};
        if(!owned())return false;
        if(value==previous)return true;
        { Notification own(*this,Kind::begin);parameter.beginChangeGesture(); }
        // Hosts may synchronously edit the parameter from beginChangeGesture.
        const bool mayWrite=owned();
        if(mayWrite) { Notification own(*this,Kind::value,value);parameter.setValueNotifyingHost(value); }
        { Notification own(*this,Kind::end);parameter.endChangeGesture(); }
        // Never reclaim a host edit made in a value/end-gesture callback.
        // Ranged parameters may snap/round-trip the requested normalised value.
        // The controller records the actual read-back only if no edit intervened.
        return mayWrite && unchanged(expected);
    }
    juce::RangedAudioParameter& parameter;
private:
    enum class Kind {begin,value,end};
    struct Notification {
        ParameterEdits& owner;Kind kind;float value;bool consumed{};Notification* previous;
        Notification(ParameterEdits& p,Kind k,float v=0):owner(p),kind(k),value(v),previous(current){current=this;}
        ~Notification(){current=previous;}
    };
    // Suppress only this controller's one synchronous notification. A global
    // "writing" flag would incorrectly suppress concurrent DAW automation.
    inline static thread_local Notification* current=nullptr;
    bool consume(Kind kind,float value=0) noexcept {
        if(current && &current->owner==this && current->kind==kind && !current->consumed
           && (kind!=Kind::value || current->value==value)) {current->consumed=true;return true;}
        return false;
    }
    void parameterValueChanged(int,float value) override {
        if(!consume(Kind::value,value))edits.fetch_add(1,std::memory_order_release);
    }
    void parameterGestureChanged(int,bool starting) override {
        if(!consume(starting?Kind::begin:Kind::end)) {
            gesture.store(starting,std::memory_order_release);
            edits.fetch_add(1,std::memory_order_release);
        }
    }
    static_assert(std::atomic<uint32_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    std::atomic<uint32_t> edits{0};
    std::atomic<bool> gesture{false};
};
}

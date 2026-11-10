#pragma once
#include "GraphicalEQ.h"
#include <bitset>
#include <cstring>
#include <algorithm>
#include <limits>
#include <mutex>
#include <optional>
#include <vector>

namespace spectralforge {

// The 122 released parameters remain the sole audio/automation contract.
// Occupancy describes editor slots only: an occupied disabled band is bypassed,
// and a vacant band is absent from the editor. Host enabled=true always makes a
// slot visible again, without changing GraphicalEQ's processing semantics.
class EQEditState final : private juce::AudioProcessorParameter::Listener {
public:
    static constexpr int parametersPerBank=1+5*graphicalEQBands;
    static constexpr uint16_t allSlots=(1u<<graphicalEQBands)-1u;
    static constexpr size_t historyLimit=64;

    explicit EQEditState(juce::AudioProcessorValueTreeState& parameters):state(parameters) {
        for(int bank=0;bank<2;++bank) {
            bind(bank,-1,0,"bypass");
            for(int band=0;band<graphicalEQBands;++band)
                for(int field=0;field<5;++field)bind(bank,band,field,fields[(size_t)field]);
        }
        observedEpoch=epoch.load();
    }
    ~EQEditState() override {
        active.reset();endGestures();
        for(const auto& binding:bindings)binding.parameter->removeListener(this);
    }

    uint16_t occupiedMask(int bank) const noexcept {
        if(!validBank(bank))return 0;
        // Reading enabled as well closes the tiny interval before the parameter
        // listener publishes host reoccupation, and repairs inconsistent input.
        return uint16_t(occupied[(size_t)bank].load(std::memory_order_acquire)|enabledMask(bank));
    }
    bool isOccupied(int bank,int band) const noexcept {
        return validBand(band) && (occupiedMask(bank)&bit(band))!=0;
    }
    uint64_t revision() const noexcept {return presentationRevision.load(std::memory_order_acquire);}
    uint64_t externalRevision() const noexcept {return epoch.load(std::memory_order_acquire);}

    // Editor operations below run on the message thread. Parameter listeners
    // and state-restore barriers only publish atomics; they never access history,
    // allocate, block, call a host gesture, or mutate a ValueTree on an audio thread.
    bool beginEdit(int bank,const juce::String& description) {
        reconcile();
        if(!validBank(bank) || restoreDepth.load(std::memory_order_acquire)!=0 || active)return false;
        interrupted=false;
        const auto editEpoch=epoch.load(std::memory_order_acquire);
        Action action;action.bank=bank;action.description=description;action.before=capture(bank);
        action.after=action.before;action.beforeMask=occupiedMask(bank);action.afterMask=action.beforeMask;
        action.epoch=editEpoch;
        active=std::move(action);gestureBank=bank;
        // A host update during the snapshot must invalidate it, never become
        // the epoch of a partially older before-state. No gesture has opened.
        reconcile();
        if(!alive(bank)){reconcile();interrupted=false;return false;}
        return true;
    }
    bool touchField(int bank,int band,const char* field) {
        reconcile();
        if(!alive(bank))return false;
        const int index=localIndex(band,field);if(index<0)return false;
        active->touched.set((size_t)index);
        if(!gestures.test((size_t)index)) {
            gestures.set((size_t)index);binding(bank,index).parameter->beginChangeGesture();
        }
        if(!alive(bank)){reconcile();return false;}
        return true;
    }
    bool setValue(int bank,int band,const char* field,float rawValue) {
        reconcile();
        if(interrupted || !std::isfinite(rawValue) || !validBank(bank))return false;
        const int index=localIndex(band,field);if(index<0)return false;
        if(validBand(band) && !isOccupied(bank,band) && !(index%5==1 && rawValue>.5f))return false;
        const bool discrete=!active.has_value();
        if(discrete && !beginEdit(bank,juce::String("Change ")+field))return false;
        if(!touchField(bank,band,field)){if(discrete)endEdit();return false;}
        auto& target=binding(bank,index);auto* parameter=target.parameter;
        const float normal=parameter->convertTo0to1(parameter->getNormalisableRange().snapToLegalValue(rawValue));
        if(validBand(band) && target.field==0 && normal>.5f)setOccupied(bank,uint16_t(occupiedMask(bank)|bit(band)));
        if(!sameBits(parameter->getValue(),normal)) {
            OwnWrite own(*this,target,normal);parameter->setValueNotifyingHost(normal);
        }
        const bool accepted=alive(bank) && sameNormal(parameter->getValue(),normal);
        if(!accepted)barrier();
        if(discrete)endEdit();
        return accepted;
    }
    void endEdit() {
        reconcile();
        if(!active){interrupted=false;return;}
        auto action=std::move(*active);active.reset();
        action.after=capture(action.bank);action.afterMask=occupiedMask(action.bank);
        endGestures();
        if(action.epoch!=epoch.load(std::memory_order_acquire)) {reconcile();interrupted=false;return;}
        for(size_t i=0;i<action.touched.size();++i)
            if(sameBits(action.before[i],action.after[i]))action.touched.reset(i);
        if(action.touched.any() || action.beforeMask!=action.afterMask) {
            undoStack.push_back(std::move(action));
            if(undoStack.size()>historyLimit)undoStack.erase(undoStack.begin());
            redoStack.clear();notifyMetadataChange();
        }
        interrupted=false;
    }
    // Cancellation closes gestures and establishes the already-applied values
    // as a new history baseline, without rolling an automated/restored value
    // back to the gesture's earlier value.
    void cancelEdit() {
        const bool metadataChanged=active && active->beforeMask!=occupiedMask(active->bank);
        barrier();reconcile();interrupted=false;
        if(metadataChanged)notifyMetadataChange();
    }

    int addBand(int bank,float frequency,float gain) {
        reconcile();if(!validBank(bank) || interrupted || active)return -1;
        int band=0;while(band<graphicalEQBands && isOccupied(bank,band))++band;
        if(band==graphicalEQBands || !std::isfinite(frequency) || !std::isfinite(gain))return -1;
        if(!beginEdit(bank,"Add band "+juce::String(band+1)))return -1;
        if(!alive(bank) || isOccupied(bank,band)){cancelEdit();return -1;}
        setOccupied(bank,uint16_t(occupiedMask(bank)|bit(band)));
        // Initialize a reused slot while it is silent; enable only after all
        // tuning values have been set. Host slot IDs never move or compact.
        const bool ok=setValue(bank,band,"frequency",frequency)
            && setValue(bank,band,"gain",gain)
            && setValue(bank,band,"q",.70710678f)
            && setValue(bank,band,"type",0)
            && setValue(bank,band,"enabled",1);
        endEdit();return ok?band:-1;
    }
    bool removeBand(int bank,int band) {
        reconcile();if(interrupted || active || !isOccupied(bank,band))return false;
        if(!beginEdit(bank,"Delete band "+juce::String(band+1)))return false;
        const bool ok=setValue(bank,band,"enabled",0);
        if(ok && alive(bank))setOccupied(bank,uint16_t(occupiedMask(bank)&~bit(band)));
        endEdit();return ok;
    }

    // Delete several bands as one undo step (graph group selection).
    bool removeBands(int bank,uint16_t mask) {
        reconcile();if(interrupted || active || !validBank(bank))return false;
        int count=0;for(int b=0;b<graphicalEQBands;++b)if((mask&bit(b)) && isOccupied(bank,b))++count;
        if(count==0)return false;
        if(!beginEdit(bank,"Delete "+juce::String(count)+" bands"))return false;
        const uint16_t chosen=uint16_t(mask&occupiedMask(bank));bool ok=true;
        for(int b=0;b<graphicalEQBands && ok;++b)if(chosen&bit(b))ok=setValue(bank,b,"enabled",0);
        if(ok && alive(bank))setOccupied(bank,uint16_t(occupiedMask(bank)&~chosen));
        endEdit();return ok;
    }

    // One EQ curve: its band parameters (normalised) and band occupancy.
    struct Curve {bool valid{};uint16_t mask{};std::array<float,parametersPerBank> values{};};
    // EQ-local A/B (2026-11-08): each EQ keeps two curves, A and B. The curve
    // is its 60 band parameters and band occupancy; the EQ's bypass is not
    // part of it. Switching is a recall, like the global A/B: it writes the
    // stored curve as host gestures, is not an undo step and clears the
    // history (a barrier). The first switch to an empty slot copies the
    // current curve there. Curves are saved with the project inside
    // EQ_EDIT_STATE (older versions ignore them) and so also travel with
    // each global A/B slot.
    int curveSlot(int bank) const {std::lock_guard<std::mutex> lock(curveMutex);return validBank(bank)?activeCurve[(size_t)bank]:0;}
    bool hasStoredCurve(int bank,int slot) const {
        std::lock_guard<std::mutex> lock(curveMutex);
        return validBank(bank) && (slot==0 || slot==1) && curves[(size_t)bank][(size_t)slot].valid;
    }
    bool selectCurve(int bank,int slot) {
        reconcile();if(!validBank(bank) || (slot!=0 && slot!=1) || active || restoreDepth.load(std::memory_order_acquire)!=0)return false;
        interrupted=false;
        Curve now,target;int current;
        {
            std::lock_guard<std::mutex> lock(curveMutex);
            current=activeCurve[(size_t)bank];if(slot==current)return true;
            now={true,occupiedMask(bank),capture(bank)};
            target=curves[(size_t)bank][(size_t)slot].valid?curves[(size_t)bank][(size_t)slot]:now;
        }
        // A host or automation write during the switch wins: stop, keep the
        // slot and the stored curves as they were.
        if(!writeCurve(bank,target)){reconcile();notifyMetadataChange();return false;}
        {
            std::lock_guard<std::mutex> lock(curveMutex);
            curves[(size_t)bank][(size_t)current]=now;curves[(size_t)bank][(size_t)slot]=target;activeCurve[(size_t)bank]=slot;
        }
        // A recall for this EQ: its history goes; the other EQ keeps its own.
        clearHistory(bank);presentationRevision.fetch_add(1,std::memory_order_release);notifyMetadataChange();
        return true;
    }
    // Store the current curve in the other slot; nothing audible changes.
    bool copyCurveToOtherSlot(int bank) {
        if(!validBank(bank))return false;
        {std::lock_guard<std::mutex> lock(curveMutex);curves[(size_t)bank][(size_t)(1-activeCurve[(size_t)bank])]={true,occupiedMask(bank),capture(bank)};}
        notifyMetadataChange();return true;
    }
    // Replace the other EQ's curve with this one: one undo step in that EQ.
    bool copyCurveToBank(int from,int to) {
        reconcile();if(!validBank(from) || !validBank(to) || from==to || active || restoreDepth.load(std::memory_order_acquire)!=0)return false;
        interrupted=false;
        const auto source=capture(from);const uint16_t sourceMask=occupiedMask(from);
        if(!beginEdit(to,juce::String("Copy ")+(from==0?"Tone":"Final")+" EQ curve"))return false;
        // Publish every source node first (setValue only writes occupied
        // slots), disable before retuning, and enable last, as undo does.
        setOccupied(to,uint16_t(occupiedMask(to)|sourceMask));
        bool ok=true;
        for(int phase=0;phase<3 && ok;++phase)for(int band=0;band<graphicalEQBands && ok;++band)for(int field=0;field<5 && ok;++field) {
            if(!isOccupied(to,band))continue; // vacant in both: stays vacant
            const int index=1+5*band+field;const float normal=source[(size_t)index];
            const int order=field==0?(normal>.5f?2:0):1;if(order!=phase)continue;
            auto* parameter=binding(to,index).parameter;
            ok=setValue(to,band,fields[(size_t)field],parameter->convertFrom0to1(normal));
        }
        if(ok && alive(to))setOccupied(to,uint16_t(sourceMask|enabledMask(to)));
        endEdit();return ok;
    }

    bool canUndo(){reconcile();return ready() && !undoStack.empty();}
    bool canRedo(){reconcile();return ready() && !redoStack.empty();}
    bool undo(){return replay(false);}
    bool redo(){return replay(true);}
    int nextUndoBank(){return canUndo()?undoStack.back().bank:-1;}
    int nextRedoBank(){return canRedo()?redoStack.back().bank:-1;}
    juce::String undoDescription(){return canUndo()?undoStack.back().description:juce::String{};}
    juce::String redoDescription(){return canRedo()?redoStack.back().description:juce::String{};}

    // A/B copy/recall, preset and project restoration are explicit barriers even
    // when every restored parameter has the same value. History is ephemeral.
    void barrier() noexcept {
        epoch.fetch_add(1,std::memory_order_acq_rel);presentationRevision.fetch_add(1,std::memory_order_release);
    }
    void saveMetadata(juce::ValueTree& root) const {
        for(int i=root.getNumChildren()-1;i>=0;--i)
            if(root.getChild(i).hasType("EQ_EDIT_STATE"))root.removeChild(i,nullptr);
        juce::ValueTree metadata("EQ_EDIT_STATE");metadata.setProperty("schema",1,nullptr);
        metadata.setProperty("toneOccupied",int(occupiedMask(0)),nullptr);
        metadata.setProperty("finalOccupied",int(occupiedMask(1)),nullptr);
        {
            std::lock_guard<std::mutex> lock(curveMutex);
            metadata.setProperty("toneCurve",activeCurve[0],nullptr);metadata.setProperty("finalCurve",activeCurve[1],nullptr);
            for(int bank=0;bank<2;++bank)for(int slot=0;slot<2;++slot)if(const auto& curve=curves[(size_t)bank][(size_t)slot];curve.valid) {
                // Values as exact float bit patterns.
                juce::StringArray bits;for(float v:curve.values){uint32_t b;std::memcpy(&b,&v,4);bits.add(juce::String::toHexString(juce::int64(b)));}
                juce::ValueTree node("EQ_CURVE");node.setProperty("bank",bank,nullptr);node.setProperty("slot",slot,nullptr);
                node.setProperty("mask",int(curve.mask),nullptr);node.setProperty("values",bits.joinIntoString(","),nullptr);
                metadata.appendChild(node,nullptr);
            }
        }
        root.appendChild(metadata,nullptr);
    }

    class RestoreScope {
    public:
        RestoreScope(EQEditState& owner,const juce::ValueTree& incoming,bool factory=false):edits(owner) {
            edits.restoreDepth.fetch_add(1,std::memory_order_acq_rel);edits.barrier();
            masks=factory?std::array<uint16_t,2>{0,0}:readMetadata(incoming);
            if(!factory)readCurves(incoming,curves,slots);
        }
        ~RestoreScope() {
            for(int bank=0;bank<2;++bank)
                edits.occupied[(size_t)bank].store(uint16_t(masks[(size_t)bank]|edits.enabledMask(bank)),std::memory_order_release);
            {std::lock_guard<std::mutex> lock(edits.curveMutex);edits.curves=curves;edits.activeCurve=slots;}
            edits.barrier();edits.restoreDepth.fetch_sub(1,std::memory_order_acq_rel);
        }
        RestoreScope(const RestoreScope&)=delete;
        RestoreScope& operator=(const RestoreScope&)=delete;
    private:
        EQEditState& edits;std::array<uint16_t,2> masks{};
        std::array<std::array<Curve,2>,2> curves{};std::array<int,2> slots{0,0};
    };

private:
    inline static constexpr std::array<const char*,5> fields{"enabled","frequency","gain","q","type"};
    // Write a stored curve (not the bypass) as one set of host gestures, in
    // the same order undo uses; occupancy follows the stored mask. Stops, and
    // returns false, as soon as anything else writes an EQ parameter (the
    // epoch moves) or a restore starts, as undo/redo do.
    bool writeCurve(int bank,const Curve& target) {
        const auto now=capture(bank);const uint16_t before=occupiedMask(bank);
        const auto token=epoch.load(std::memory_order_acquire);
        const auto intact=[&]{return token==epoch.load(std::memory_order_acquire) && restoreDepth.load(std::memory_order_acquire)==0;};
        gestureBank=bank;
        for(int i=1;i<parametersPerBank;++i)if(!sameBits(now[(size_t)i],target.values[(size_t)i])) {
            if(!intact()){endGestures();return false;}
            gestures.set((size_t)i);binding(bank,i).parameter->beginChangeGesture();
        }
        if(!intact()){endGestures();return false;}
        setOccupied(bank,uint16_t(before|target.mask));
        bool ok=true;
        for(int phase=0;phase<3 && ok;++phase)for(int i=1;i<parametersPerBank;++i)if(gestures.test((size_t)i)) {
            const bool enabled=(i-1)%5==0;const int order=enabled?(target.values[(size_t)i]>.5f?2:0):1;if(order!=phase)continue;
            if(!intact()){ok=false;break;}
            auto* parameter=binding(bank,i).parameter;
            {OwnWrite own(*this,binding(bank,i),target.values[(size_t)i]);parameter->setValueNotifyingHost(target.values[(size_t)i]);}
            if(!intact() || !sameNormal(parameter->getValue(),target.values[(size_t)i])){ok=false;break;}
        }
        endGestures();
        ok=ok && intact();
        setOccupied(bank,uint16_t((ok?target.mask:before)|enabledMask(bank)));
        return ok;
    }
    void clearHistory(int bank) {
        const auto other=[bank](const Action& action){return action.bank==bank;};
        undoStack.erase(std::remove_if(undoStack.begin(),undoStack.end(),other),undoStack.end());
        redoStack.erase(std::remove_if(redoStack.begin(),redoStack.end(),other),redoStack.end());
    }
    static void readCurves(const juce::ValueTree& root,std::array<std::array<Curve,2>,2>& curves,std::array<int,2>& slots) {
        // The same acceptance as the occupancy: exactly one EQ_EDIT_STATE of
        // schema 1 with valid occupancy; anything else leaves both curves empty.
        constexpr std::array<uint16_t,2> legacy{allSlots,allSlots};
        int count=0;for(auto child:root)if(child.hasType("EQ_EDIT_STATE"))++count;
        const auto metadata=root.getChildWithName("EQ_EDIT_STATE");
        if(count!=1 || metadata.getProperty("schema").toString()!="1")return;
        if(readMetadata(root)==legacy && !(metadata.getProperty("toneOccupied").toString()==juce::String(int(allSlots))
                                           && metadata.getProperty("finalOccupied").toString()==juce::String(int(allSlots))))return;
        constexpr std::array<const char*,2> keys{"toneCurve","finalCurve"};
        for(int bank=0;bank<2;++bank){const int slot=int(metadata.getProperty(keys[(size_t)bank],0));slots[(size_t)bank]=slot==1?1:0;}
        for(auto node:metadata) {
            if(!node.hasType("EQ_CURVE"))continue;
            const int bank=int(node.getProperty("bank",-1)),slot=int(node.getProperty("slot",-1)),mask=int(node.getProperty("mask",-1));
            if(!validBank(bank) || (slot!=0 && slot!=1) || mask<0 || mask>allSlots)continue;
            juce::StringArray bits;bits.addTokens(node.getProperty("values").toString(),",",{});
            if(bits.size()!=parametersPerBank)continue;
            Curve curve{true,uint16_t(mask),{}};bool ok=true;
            for(int i=0;i<parametersPerBank && ok;++i) {
                const auto text=bits[i].trim();if(text.isEmpty() || text.length()>8 || !text.containsOnly("0123456789abcdefABCDEF")){ok=false;break;}
                const uint32_t b=uint32_t(text.getHexValue64());float v;std::memcpy(&v,&b,4);
                ok=std::isfinite(v) && v>=0.f && v<=1.f;curve.values[(size_t)i]=v;
            }
            if(ok)curves[(size_t)bank][(size_t)slot]=curve;
        }
    }
    struct Binding {
        juce::String id;juce::RangedAudioParameter* parameter{};std::atomic<float>* raw{};
        int bank{},band{},field{},hostIndex{};
    };
    struct Action {
        int bank{};juce::String description;
        std::array<float,parametersPerBank> before{},after{};
        std::bitset<parametersPerBank> touched;
        uint16_t beforeMask{},afterMask{};uint64_t epoch{};
    };
    class OwnWrite {
    public:
        OwnWrite(EQEditState& edits,const Binding& targetBinding,float expected)
            :owner(&edits),target(&targetBinding),normal(expected),previous(ownWriter){ownWriter=this;}
        ~OwnWrite(){ownWriter=previous;}
        bool matches(const EQEditState* edits,const Binding& changed,float normalizedValue) const noexcept {
            // JUCE forwards the setValueNotifyingHost argument verbatim. Using
            // exact bits here also detects a nested, one-step host adjustment
            // at the top of the logarithmic frequency range.
            return owner==edits && target==&changed && sameBits(normalizedValue,normal);
        }
    private:
        EQEditState* owner{};const Binding* target{};float normal{};const OwnWrite* previous{};
    };
    inline static thread_local const OwnWrite* ownWriter=nullptr;
    static bool validBank(int bank) noexcept{return bank>=0 && bank<2;}
    static bool validBand(int band) noexcept{return band>=0 && band<graphicalEQBands;}
    static bool sameBits(float a,float b) noexcept{return std::memcmp(&a,&b,sizeof(float))==0;}
    static bool sameNormal(float a,float b) noexcept{return std::abs(a-b)<=4*std::numeric_limits<float>::epsilon();}
    static uint16_t bit(int band) noexcept{return uint16_t(1u<<band);}
    static int localIndex(int band,const char* field) {
        if(field==nullptr)return -1;
        if(band==-1)return std::strcmp(field,"bypass")==0?0:-1;
        if(!validBand(band))return -1;
        for(int f=0;f<5;++f)if(std::strcmp(field,fields[(size_t)f])==0)return 1+5*band+f;
        return -1;
    }
    Binding& binding(int bank,int index){return bindings[(size_t)(bank*parametersPerBank+index)];}
    const Binding& binding(int bank,int index) const{return bindings[(size_t)(bank*parametersPerBank+index)];}
    void bind(int bank,int band,int field,const char* key) {
        auto& b=binding(bank,band<0?0:1+5*band+field);b.id=eqID(bank,band,key);
        b.parameter=state.getParameter(b.id);b.raw=state.getRawParameterValue(b.id);
        b.bank=bank;b.band=band;b.field=field;jassert(b.parameter && b.raw);
        b.hostIndex=b.parameter->getParameterIndex();b.parameter->addListener(this);
    }
    uint16_t enabledMask(int bank) const noexcept {
        uint16_t mask=0;for(int band=0;band<graphicalEQBands;++band)
            if(binding(bank,1+5*band).raw->load(std::memory_order_relaxed)>.5f)mask=uint16_t(mask|bit(band));
        return mask;
    }
    void parameterValueChanged(int parameterIndex,float normalizedValue) override {
        // Observe the underlying parameter so equal-value host notifications
        // count as ownership changes too. APVTS::Listener suppresses those.
        for(const auto& b:bindings)if(b.hostIndex==parameterIndex) {
            if(b.band>=0 && b.field==0 && b.parameter->getValue()>.5f)
                occupied[(size_t)b.bank].fetch_or(bit(b.band),std::memory_order_release);
            presentationRevision.fetch_add(1,std::memory_order_release);
            if(ownWriter==nullptr || !ownWriter->matches(this,b,normalizedValue))epoch.fetch_add(1,std::memory_order_acq_rel);
            return;
        }
    }
    void parameterGestureChanged(int,bool) override {}
    void setOccupied(int bank,uint16_t mask) noexcept {
        occupied[(size_t)bank].store(uint16_t(mask&allSlots),std::memory_order_release);
        presentationRevision.fetch_add(1,std::memory_order_release);
    }
    std::array<float,parametersPerBank> capture(int bank) const {
        std::array<float,parametersPerBank> values{};
        for(int i=0;i<parametersPerBank;++i)values[(size_t)i]=binding(bank,i).parameter->getValue();
        return values;
    }
    bool alive(int bank) const noexcept {
        return active && active->bank==bank && active->epoch==epoch.load(std::memory_order_acquire)
            && restoreDepth.load(std::memory_order_acquire)==0;
    }
    bool ready() const noexcept{return !active && !interrupted && restoreDepth.load(std::memory_order_acquire)==0;}
    void endGestures() {
        const auto open=gestures;gestures.reset();
        for(size_t i=0;i<open.size();++i)if(open.test(i))binding(gestureBank,int(i)).parameter->endChangeGesture();
    }
    void reconcile() {
        if(observedEpoch==epoch.load(std::memory_order_acquire))return;
        if(active){active.reset();interrupted=true;endGestures();}
        undoStack.clear();redoStack.clear();observedEpoch=epoch.load(std::memory_order_acquire);
    }
    void notifyMetadataChange() {
        state.processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
    }
    bool replay(bool redo) {
        reconcile();if(!ready())return false;
        auto& from=redo?redoStack:undoStack;auto& to=redo?undoStack:redoStack;
        if(from.empty())return false;
        const auto action=from.back();const auto& expected=redo?action.before:action.after;
        const auto& target=redo?action.after:action.before;
        const auto expectedMask=redo?action.beforeMask:action.afterMask;
        const auto targetMask=redo?action.afterMask:action.beforeMask;
        const auto token=epoch.load(std::memory_order_acquire);
        if(occupiedMask(action.bank)!=expectedMask){barrier();reconcile();return false;}
        for(size_t i=0;i<action.touched.size();++i)
            if(action.touched.test(i) && !sameNormal(binding(action.bank,int(i)).parameter->getValue(),expected[i])) {
                barrier();reconcile();return false;
            }
        gestureBank=action.bank;
        for(size_t i=0;i<action.touched.size();++i)if(action.touched.test(i)) {
            if(token!=epoch.load(std::memory_order_acquire) || restoreDepth.load(std::memory_order_acquire)!=0) {
                endGestures();reconcile();return false;
            }
            gestures.set(i);binding(action.bank,int(i)).parameter->beginChangeGesture();
        }
        if(token!=epoch.load(std::memory_order_acquire) || restoreDepth.load(std::memory_order_acquire)!=0) {
            endGestures();reconcile();return false;
        }
        // Publish added nodes before making them audible. Remove nodes only
        // after enabled=false has reached the existing parameter/DSP path.
        setOccupied(action.bank,uint16_t(occupiedMask(action.bank)|targetMask));
        bool ok=true;
        for(int phase=0;phase<3 && ok;++phase)for(size_t i=0;i<action.touched.size();++i)if(action.touched.test(i)) {
            const bool enabled=i>0 && (i-1)%5==0;
            const int order=enabled?(target[i]>.5f?2:0):1;if(order!=phase)continue;
            if(token!=epoch.load(std::memory_order_acquire) || restoreDepth.load(std::memory_order_acquire)!=0){ok=false;break;}
            auto* parameter=binding(action.bank,int(i)).parameter;
            {OwnWrite own(*this,binding(action.bank,int(i)),target[i]);parameter->setValueNotifyingHost(target[i]);}
            if(!sameNormal(parameter->getValue(),target[i])){barrier();ok=false;break;}
        }
        endGestures();ok=ok && token==epoch.load(std::memory_order_acquire);
        if(!ok){reconcile();notifyMetadataChange();return false;}
        setOccupied(action.bank,uint16_t(targetMask|enabledMask(action.bank)));
        if(token!=epoch.load(std::memory_order_acquire)){reconcile();notifyMetadataChange();return false;}
        from.pop_back();to.push_back(action);notifyMetadataChange();return true;
    }
    static std::array<uint16_t,2> readMetadata(const juce::ValueTree& root) {
        const std::array<uint16_t,2> legacy{allSlots,allSlots};
        auto metadata=root.getChildWithName("EQ_EDIT_STATE");
        int count=0;for(auto child:root)if(child.hasType("EQ_EDIT_STATE"))++count;
        if(count!=1 || metadata.getProperty("schema").toString()!="1")return legacy;
        std::array<uint16_t,2> masks{};constexpr std::array<const char*,2> keys{"toneOccupied","finalOccupied"};
        for(int bank=0;bank<2;++bank) {
            const auto value=metadata.getProperty(keys[(size_t)bank]).toString();
            if(value.isEmpty() || value.length()>4 || !value.containsOnly("0123456789"))return legacy;
            const int number=value.getIntValue();if(number>allSlots)return legacy;
            masks[(size_t)bank]=uint16_t(number);
        }
        return masks;
    }

    juce::AudioProcessorValueTreeState& state;
    std::array<Binding,graphicalEQParameterCount> bindings;
    std::array<std::atomic<uint16_t>,2> occupied{};
    mutable std::mutex curveMutex;std::array<std::array<Curve,2>,2> curves{};std::array<int,2> activeCurve{0,0};
    std::atomic<uint64_t> epoch{1},presentationRevision{1};
    std::atomic<unsigned> restoreDepth{0};
    uint64_t observedEpoch{};std::optional<Action> active;
    std::vector<Action> undoStack,redoStack;
    std::bitset<parametersPerBank> gestures;int gestureBank{};bool interrupted{};
};
}

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <vector>

namespace transposeNext {
// Research only (2026-10-23): flags a pitch change without a new pick
// (legato, hammer-on, pull-off) so a read-head engine can re-anchor there,
// and estimates how long ago the change began.
//
// A sustained note repeats with its period P, so the period-prediction error
//   e = sum (x[t] - x[t-P])^2 / sum (x[t]^2 + x[t-P]^2)
// over the last 4 ms stays low. Only history is used, so a change shows up
// within a few ms. The input is differentiated first: a pitch change of df
// moves harmonic k by k df, so weighting high harmonics detects it sooner.
// P is re-fitted every 1 ms within +-4 %, which follows vibrato and bends
// (about 0.8 % per 20 ms at +-20 cents / 5.5 Hz or 200 cents / 300 ms).
//
// Tuned on synthetic material (docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md):
// - lock only when e < 0.02 (non-periodic chords never lock) and only for
//   periods <= 20 ms (fundamental >= 50 Hz; lower notes were unreliable),
// - an event needs 10 ms of lock, e > 6 x its running baseline, e > 0.06 and
//   a rise > 0.06 within 5 ms (slow drift and beating do not qualify),
// - a candidate is confirmed only if, 2 ms later, the error at the old period
//   is still > 0.15: an equal-tempered triad (C4 major, ~10 Hz beating of
//   the major third) briefly breaks the lock and then fits again (0.002-0.079
//   after 2 ms over 25 false events), a real change keeps failing it
//   (0.198-1.018 over 15 legato / hammer-on events); the 2 ms are part of the age,
// - a second route catches small steps the +-4 % re-fit follows like a bend:
//   a one-semitone legato moves the fitted period 2.9 % in 5 ms (518 -> 503
//   samples, E2 +1) while its error stays < 0.06; vibrato or a 200-cent bend
//   move it about 0.2 % in 5 ms and C4-major beating 0.14 %. A drift > 1.5 %
//   within 5 ms is a candidate, confirmed against the period before the drift
//   and dated from the last steady fit (drift < 0.3 % in 5 ms),
// - the change is dated at the last 1 ms update that still fitted (age),
// - no lock while more than 20 % of the energy passes a two-pole 45 Hz
//   low-pass (one pole at 55 Hz also blocked a 62 Hz D#2): notes under
//   the 50 Hz floor (B0, E1) otherwise lock onto a sub-period and false-trigger
//   (B0 power chord: 9 events in 3 s without this guard).
class LegatoDetector {
    std::vector<float> ring;
    std::array<double,6> recent{}, periods{};
    int size{}, write{}, hop{}, clock{}, window{}, minLag{}, maxLag{};
    int stable{}, cooldown{}, searchHold{}, events{}, recentCount{}, sinceFit{}, sinceSteady{}, confirm{}, rejected{}, confirmAgeFrom{};
    int steadyGuard{}, guarded{}, sampleRateHz{};
    double period{}, confirmPeriod{}, baseline{}, previous{}, deep{}, deeper{}, deepCoefficient{}, deepEnergy{}, allEnergy{}, energyCoefficient{};
    static constexpr int stableNeeded=10, cooldownLength=30, rise=5, confirmUpdates=2;
    static constexpr double searchThreshold=.15, lockThreshold=.02, eventRatio=6, eventFloor=.06, energyFloor=1.e-7, confirmFloor=.15,
                            driftEvent=.015, driftSteady=.003;
    float at(int age) const noexcept {int i=write-1-age;if(i<0)i+=size;return ring[size_t(i)];}
    double error(int lag) const noexcept {
        double e=0,s=1.e-20;
        for(int k=0;k<window;++k){const double a=at(k),b=at(k+lag);e+=(a-b)*(a-b);s+=a*a+b*b;}
        return e/s;
    }
public:
    void prepare(double rate) {
        sampleRateHz=int(rate);hop=std::max(1,int(rate*.001));window=int(rate*.004);
        minLag=int(rate*.0025);maxLag=int(rate*.020);
        size=maxLag+window+hop+8;ring.assign(size_t(size),0.f);
        deepCoefficient=1-std::exp(-2*juce::MathConstants<double>::pi*45/rate);energyCoefficient=1-std::exp(-1/(rate*.02));reset();
    }
    void reset() noexcept {
        std::fill(ring.begin(),ring.end(),0.f);write=clock=stable=cooldown=searchHold=events=recentCount=sinceFit=sinceSteady=confirm=rejected=confirmAgeFrom=guarded=0;
        period=baseline=previous=deep=deeper=deepEnergy=allEnergy=0;
    }
    // A pick elsewhere re-anchors anyway: drop the lock and wait.
    void notePick() noexcept {period=0;stable=0;recentCount=0;confirm=0;cooldown=cooldownLength;}
    int legatoEvents() const noexcept {return events;}
    int rejectedCandidates() const noexcept {return rejected;}
    // Research option (off = 0): a candidate needs a steady fit (drift < 0.3 %
    // in 5 ms) within `ms` before the change. A legato step leaves a steady
    // note just before it; wide vibrato (+-100 cents, 7 Hz) or a fast bend
    // has been moving for tens of ms (re-anchoring there measured worse, see
    // Tools/TransposeEvidence/legato_modulation_probe.py).
    void setSteadyGuard(double ms) noexcept {steadyGuard=ms>0 ? int(sampleRateHz*ms/1000) : 0;}
    int guardedCandidates() const noexcept {return guarded;}
    // True when the next push() runs the 1 ms update. notePick() only touches
    // update state, so a pick reported any time before that push is applied
    // exactly as if it were reported on its own sample.
    bool nextPushUpdates() const noexcept {return clock+1>=hop;}
    // One mono sample. Returns the estimated age of the change in samples on
    // the sample where a legato change is flagged, otherwise -1.
    int push(float x) noexcept {
        const float emphasised=x-float(previous);previous=x;
        deep+=deepCoefficient*(x-deep);deeper+=deepCoefficient*(deep-deeper);deepEnergy+=energyCoefficient*(deeper*deeper-deepEnergy);allEnergy+=energyCoefficient*(double(x)*x-allEnergy);
        ring[size_t(write)]=emphasised;if(++write==size)write=0;
        ++sinceFit;++sinceSteady;
        if(++clock<hop) return -1;
        clock=0;
        if(cooldown>0)--cooldown;
        double energy=0;for(int k=0;k<window;++k)energy+=double(at(k))*at(k);
        if(energy<energyFloor){period=0;stable=0;recentCount=0;confirm=0;return -1;}
        if(deepEnergy>.2*allEnergy){period=0;stable=0;recentCount=0;confirm=0;return -1;}
        if(confirm>0) {
            // Still at the old period? Then it was beating, not a new note.
            if(error(int(confirmPeriod))<=confirmFloor){confirm=0;++rejected;return -1;}
            if(--confirm>0) return -1;
            ++events;period=0;stable=0;recentCount=0;cooldown=cooldownLength;
            return confirmAgeFrom==1 ? sinceSteady : sinceFit;
        }
        if(period<=0) {
            if(searchHold>0){--searchHold;return -1;}
            // First dip below the search threshold, walked to its local
            // minimum (YIN-like: a multiple of the period is not preferred).
            int lag=minLag;double e=error(lag);
            while(lag+2<=maxLag && e>=searchThreshold){lag+=2;e=error(lag);}
            if(e<searchThreshold) {
                for(double next=error(lag+1);lag+1<=maxLag && next<e;next=error(lag+1)){++lag;e=next;}
                if(e<lockThreshold){period=lag;stable=0;recentCount=0;baseline=std::max(e,.005);sinceFit=sinceSteady=0;return -1;}
            }
            searchHold=9;   // full search at most every 10 ms
            return -1;
        }
        const int lo=std::max(minLag,int(period*.96)),hi=std::min(maxLag,int(period*1.04)+1);
        double best=1.e9;int found=lo;
        for(int lag=lo;lag<=hi;++lag){const double e=error(lag);if(e<best){best=e;found=lag;}}
        const double old=recentCount>=rise ? recent[size_t((recentCount-rise)%int(recent.size()))] : best;   // 5 updates ago
        const double before=recentCount>=rise ? periods[size_t((recentCount-rise)%int(periods.size()))] : period;   // fitted 5 updates ago
        const double drift=std::abs(found-before)/before;
        recent[size_t(recentCount%recent.size())]=best;periods[size_t(recentCount%periods.size())]=found;++recentCount;
        if(stable>=stableNeeded && cooldown==0 && best>eventRatio*baseline && best>eventFloor && recentCount>rise && best-old>eventFloor) {
            if(steadyGuard>0 && sinceSteady-sinceFit>steadyGuard){++guarded;return -1;}
            confirm=confirmUpdates;confirmPeriod=period;confirmAgeFrom=0;   // the age keeps counting from the last fit
            return -1;
        }
        if(stable>=stableNeeded && cooldown==0 && recentCount>rise && drift>driftEvent && best<lockThreshold) {
            if(steadyGuard>0 && sinceSteady>steadyGuard){++guarded;return -1;}
            confirm=confirmUpdates;confirmPeriod=before;confirmAgeFrom=1;   // age from the last steady fit
            return -1;
        }
        if(best<searchThreshold && best<3*baseline+.02 && best<lockThreshold) {
            period=found;if(stable<stableNeeded)++stable;baseline=.9*baseline+.1*std::max(best,.002);sinceFit=0;
            if(drift<driftSteady)sinceSteady=0;
        } else if(best>.6){period=0;stable=0;recentCount=0;}
        return -1;
    }
};
}

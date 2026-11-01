#pragma once
// Included by AutoSetTests.cpp after its check/near helpers.
#include <limits>

namespace powerBassRulesDetail {
using namespace spectralforge::autoset;

// Analytic two-tone input: equal f0 and 800 Hz amplitudes have a known 0 dB
// low/reference balance, independently of the target curve or amp model.
inline std::vector<float> twoTone(double rate,double f0,float fundamentalGain=1.f) {
    std::vector<float> samples(size_t(rate*ampRenderSeconds));
    for(size_t n=0;n<samples.size();++n) {
        const double t=double(n)/rate;
        samples[n]=float(.08*(fundamentalGain*std::sin(2*juce::MathConstants<double>::pi*f0*t)
                            +std::sin(2*juce::MathConstants<double>::pi*800*t)));
    }
    return samples;
}

// A deterministic renderer with known band balances. This is a rule fixture,
// not an amp model: its controllable cross-coupling exposes acceptance bugs
// that monotonic independent shelf fixtures do not exercise.
struct BalanceRenderer {
    double rate;
    std::array<std::vector<float>,powerBassBandCount> partials;
    std::array<float,powerBassBandCount> amplitudes{},target{},base{};

    std::vector<float> mix(const std::array<float,powerBassBandCount>& gains) const {
        std::vector<float> out(partials.front().size());
        for(int b=0;b<powerBassBandCount;++b)
            for(size_t n=0;n<out.size();++n)out[n]+=gains[size_t(b)]*partials[size_t(b)][n];
        return out;
    }
    explicit BalanceRenderer(double sampleRate):rate(sampleRate) {
        for(int b=0;b<powerBassBandCount;++b) {
            const double frequency=powerBassBandCentres[size_t(b)];
            auto& part=partials[size_t(b)];part.resize(size_t(rate*ampRenderSeconds));
            for(size_t n=0;n<part.size();++n)
                part[n]=float(std::sin(2*juce::MathConstants<double>::pi*frequency*double(n)/rate+.37*b));
            target[size_t(b)]=toneTargetDb(true,float(frequency),Style::rock);
            amplitudes[size_t(b)]=.02f*std::pow(10.f,target[size_t(b)]/20.f);
        }
        // Correct small Hann-window boundary leakage so neutral means the
        // specified target, not an accidentally near-threshold input.
        for(int pass=0;pass<3;++pass) {
            const auto measured=measurePowerBassSpectrum(mix(amplitudes),rate);
            check(measured.measured,"Balance renderer fixture was silent");
            const float offset=powerBassBandDb(measured.bandsDb,400,1600)-powerBassBandDb(target,400,1600);
            for(int b=0;b<powerBassBandCount;++b)
                amplitudes[size_t(b)]*=std::pow(10.f,(target[size_t(b)]+offset-measured.bandsDb[size_t(b)])/20.f);
        }
        base=measurePowerBassSpectrum(mix(amplitudes),rate).bandsDb;
    }
    std::vector<float> at(float presenceExcess,float lowExcess) const {
        const auto power=[](float db){return std::pow(10.,db/10.);};
        const double reference=power(powerBassBandDb(base,400,1600));
        const double targetReference=power(powerBassBandDb(target,400,1600));
        const double low=power(powerBassBandDb(base,25,100));
        const double desiredLow=reference*power(powerBassBandDb(target,25,100))/targetReference*power(lowExcess);
        // 1250/1600 Hz are shared with the reference. Only the upper three
        // bite bands change; solve their energy for the requested total bite.
        const double shared=power(powerBassBandDb(base,1250,1600)),upper=power(powerBassBandDb(base,2000,3150));
        const double desiredBite=reference*power(powerBassBandDb(target,1250,3150))/targetReference*power(presenceExcess);
        check(desiredBite>shared,"Requested synthetic bite is below the fixed reference overlap");
        const float lowGain=float(std::sqrt(desiredLow/low)),upperGain=float(std::sqrt((desiredBite-shared)/upper));
        auto gains=amplitudes;
        for(int b=0;b<powerBassBandCount;++b) {
            const float f=powerBassBandCentres[size_t(b)];
            if(f<=100)gains[size_t(b)]*=lowGain;
            if(f>=2000 && f<=3150)gains[size_t(b)]*=upperGain;
        }
        return mix(gains);
    }
};

inline std::vector<float> harmonicGap(double rate,double f0) {
    std::vector<float> samples(size_t(rate*ampRenderSeconds));
    for(size_t n=0;n<samples.size();++n) {
        const double t=double(n)/rate,age=std::fmod(t,.5);double wave=0;
        for(int h=1;h<=128;++h) {
            const double amplitude=h==1?1.:h<9?.015/h:.18/std::pow(double(h),.4);
            wave+=amplitude*std::sin(2*juce::MathConstants<double>::pi*f0*h*t+.17*h);
        }
        samples[n]=float(.08*wave*(1-std::exp(-age*1200))*std::exp(-age*4));
    }
    return samples;
}
} // namespace powerBassRulesDetail

inline void powerBassRules() {
    using namespace spectralforge::autoset;
    using namespace powerBassRulesDetail;
    int fundamentalCases=0,controlCases=0,nativeCases=0;
    for(double rate:{44100.,48000.,96000.}) {
        const std::vector<float> silence(size_t(rate*ampRenderSeconds));
        check(!measurePowerBassSpectrum(silence,rate).measured,"Bass spectrum accepted silence");
        check(!measurePowerBassSpectrum(std::vector<float>(128,.1f),rate).measured,"Bass spectrum accepted an incomplete capture");
        auto invalid=twoTone(rate,27.5);invalid[0]=std::numeric_limits<float>::quiet_NaN();
        check(!measurePowerBassSpectrum(invalid,rate).measured,"Bass spectrum accepted non-finite samples");
        check(!measurePowerBassSpectrum(silence,std::numeric_limits<double>::infinity()).measured,"Bass spectrum accepted an invalid rate");
        for(double f0:{27.5,30.86770632850775}) {
            const auto equal=measurePowerBassSpectrum(twoTone(rate,f0),rate);
            const auto louder=measurePowerBassSpectrum(twoTone(rate,f0,2.f),rate);
            check(equal.measured && louder.measured,"Bass spectrum did not detect A0/B0");
            const auto balance=[](const PowerBassSpectrum& spectrum) {
                return powerBassBandDb(spectrum.bandsDb,25,40)-powerBassBandDb(spectrum.bandsDb,400,1600);
            };
            check(std::abs(balance(equal))<.08f,"Bass fundamental/reference measurement is biased");
            check(near(balance(louder)-balance(equal),6.0206f,.02f),"Doubling A0/B0 did not add 6.02 dB");
            auto sparse=twoTone(rate,f0);
            std::fill(sparse.begin()+size_t(rate),sparse.end(),0.f); // one second, then a two-second pause
            check(measureSpectrum(sparse,rate,true).playing,"Sparse fixture must pass the original playing gate");
            const PowerRenderer held=[&](const std::array<float,2>&){return sparse;};
            const auto sparseResult=analysePower(sparse,sparse,sparse,rate,std::array<AmpKnob,2>{},held,true);
            check(sparseResult.playingDetected,"Long-window bass spectrum rejected valid sparse playing");
            ++fundamentalCases;
        }

        BalanceRenderer fixture(rate);
        const std::array<AmpKnob,2> knobs{{{true,.5f,0.f,1.f},{true,.5f,0.f,1.f}}};
        const std::array<float,2> initial{.5f,.5f};
        const auto neutral=fixture.at(0,0);
        for(float lowOffset:{-4.f,0.f,4.f}) {
            const PowerRenderer render=[&](const std::array<float,2>& values) {
                return fixture.at(0,lowOffset+18.f*(values[powerResonance]-.5f));
            };
            const auto base=render(initial);
            const auto result=analysePower(neutral,base,base,rate,knobs,render,true,Style::rock);
            check(result.playingDetected,"Synthetic balance control did not run");
            check(!result.changed(powerPresence),"A low-only correction changed neutral PRESENCE");
            if(lowOffset==0)check(result.changedKnobs()==0,"A target-balanced signal caused an unnecessary adjustment");
            else {
                check(result.changed(powerResonance),"An actual low-band excess/deficit was not corrected");
                check((result.proposed[powerResonance].value-.5f)*lowOffset<0,"Bass correction moved the wrong way");
                check(std::abs(result.excessAfterDb[powerResonance])<std::abs(result.excessBeforeDb[powerResonance])-powerMinGainDb,
                      "Bass correction failed its minimum audible-band improvement");
            }
            ++controlCases;
        }
        // Old score alone accepts (1.6,0) -> (.4,1.4): own error improves >1
        // dB and the other remains within tolerance, yet total error grows.
        // Retaining an improving cross-coupled control proves this is a guard,
        // not blanket suppression of PRESENCE changes.
        for(bool worsening:{true,false}) {
            auto presenceOnly=knobs;presenceOnly[powerResonance].present=false;
            const auto base=fixture.at(1.6f,0),candidate=fixture.at(worsening?.4f:.2f,worsening?1.4f:.6f);
            const PowerRenderer render=[&](const std::array<float,2>& values){return values[0]<.5f?candidate:base;};
            const auto result=analysePower(neutral,base,base,rate,presenceOnly,render,true,Style::rock);
            const auto measured=measurePowerBassSpectrum(candidate,rate);
            const std::array<float,2> candidateErrors{powerBassExcessDb(measured.bandsDb,powerPresence,Style::rock),
                                                      powerBassExcessDb(measured.bandsDb,powerResonance,Style::rock)};
            const auto total=[](const std::array<float,2>& e){return std::abs(e[0])+std::abs(e[1]);};
            check(result.excessBeforeDb[0]>powerToleranceDb && std::abs(result.excessBeforeDb[0])-std::abs(candidateErrors[0])>powerMinGainDb,
                  "Interaction fixture did not exercise the acceptance threshold");
            if(worsening) {
                check(total(candidateErrors)>total(result.excessBeforeDb)+.1f && std::abs(candidateErrors[1])<powerToleranceDb,
                      "Interaction fixture did not reproduce the free-tolerance defect");
                check(result.changedKnobs()==0,"PRESENCE worsened the combined balances within the other band's tolerance");
            } else check(result.changed(powerPresence) && total(result.excessAfterDb)<total(result.excessBeforeDb)-.5f,
                         "A genuinely improving coupled PRESENCE candidate was rejected");
            ++controlCases;
        }

        // Real native bass head, with weak h2..h8 and strong fundamental/mid
        // content. The old 50-100 Hz-only measurement raises DEPTH even though
        // the complete low-band balance starts inside the 1.5 dB tolerance.
        for(double f0:{27.5,30.86770632850775}) {
            auto state=spectralforge::defaultAmpNativeState(spectralforge::niflheimrAmpModel);
            const std::array<int,2> controls{powerControl(state.model,state.channel,powerPresence),powerControl(state.model,state.channel,powerResonance)};
            check(controls[0]>=0 && controls[1]>=0,"Niflheimr regression needs both controls");
            state.values[size_t(controls[0])]=state.values[size_t(controls[1])]=.5f;
            const auto input=harmonicGap(rate,f0);
            const auto render=nativePowerRenderer(state,{},controls,input,rate);
            const auto base=render(initial);
            const auto result=analysePower(input,base,base,rate,knobs,render,true,Style::rock);
            check(result.playingDetected,"Native low-bass regression did not run");
            check(std::abs(result.excessBeforeDb[powerResonance])<powerToleranceDb,"Native gap fixture is no longer a near-target low balance");
            check(result.proposed[powerResonance].value<=.5f,"DEPTH rose on an already sufficient sub-bass balance");
            const auto applied=measurePowerBassSpectrum(render({result.proposed[0].value,result.proposed[1].value}),rate);
            check(std::abs(powerBassExcessDb(applied.bandsDb,powerResonance,Style::rock))
                      <=std::abs(result.excessBeforeDb[powerResonance])+.01f,
                  "The native correction worsened the complete bass balance");
            ++nativeCases;
        }
    }
    std::cout<<"PASS phase 12 bass rules: "<<fundamentalCases<<" analytic A0/B0 measurements, "<<controlCases
             <<" neutral/correction/coupling controls, "<<nativeCases<<" native harmonic-gap regressions; 44.1/48/96 kHz, synthetic only\n";
}

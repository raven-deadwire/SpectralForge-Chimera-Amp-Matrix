#pragma once
#include "AutoSetCab.h"
#include <iostream>
#include <random>
#include <string>

// Synthetic, deterministic acoustic checks. The oracle evaluates the actual
// finite IR, independently of the continuous response used by the search.
namespace spectralforge::autoset::cabRegression {
namespace layout = cabLayout;
using Complex = std::complex<double>;
inline void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
inline double db(Complex h) { return 20 * std::log10(std::max(1e-15, std::abs(h))); }
inline Complex transfer(const std::vector<float>& ir, double hz, double rate) {
    const auto step = std::polar(1., -2 * originalCab::pi * hz / rate);
    Complex h{}, phase{1};
    for (float sample : ir) { h += double(sample) * phase; phase *= step; }
    return h;
}
inline layout::Settings settings(int index, int unit = 0) {
    // Authored mounting-compatible drivers, including 10/12/15-inch bass.
    constexpr int drivers[]{1, 1, 1, 13, 9, 10, 9, 11, 14};
    layout::Settings s{};
    s.layout = index; s.unit = unit; s.voice.driver = drivers[index - 1];
    s.voice.mic = index >= 4 ? 5 : 1; // Anchor 20 / Needle 57
    s.voice.base.enabled = true;
    s.voice.base.position = .5; s.voice.base.distanceCm = 10;
    return s;
}
inline std::vector<float> kernel(const layout::Settings& s, double rate) {
    return layout::generate(layout::key(s), rate);
}
// A steady input is chosen so that the CURRENT mic hears these amplitudes.
// The moved signal is the same input through the other actual generated IR.
// This isolates sub-bass-dominant playing even with the cabinet's LF roll-off.
inline std::vector<float> steady(const std::vector<float>& current,
                                 const std::vector<float>& next, double rate,
                                 const std::vector<std::pair<double, double>>& tones) {
    std::vector<float> result(size_t(rate * 4));
    for (const auto& [hz, amplitude] : tones) {
        const auto ratio = transfer(next, hz, rate) / transfer(current, hz, rate);
        const auto step = std::polar(1., 2 * originalCab::pi * hz / rate);
        Complex phase{1};
        for (auto& sample : result) {
            sample += float(amplitude * (phase * ratio).real()); phase *= step;
        }
    }
    return result;
}
inline double rmsDb(const std::vector<float>& signal) {
    double energy = 0; for (float x : signal) energy += double(x) * x;
    return 10 * std::log10(std::max(1e-30, energy / signal.size()));
}
inline std::vector<float> target(bool bass,double rate,int tilt) {
    constexpr int size=32768;
    juce::dsp::FFT fft(15);std::vector<float> bins(2*size);
    std::mt19937 rng{92};std::uniform_real_distribution<double> phase(0,2*originalCab::pi);
    for(int k=1;k<size/2;++k) {
        const double hz=k*rate/size;
        const double amplitude=hz<20?0:std::sqrt(std::pow(10.,(toneTargetDb(bass,float(hz))+(hz>3000?tilt:0))/10.)/hz);
        const double angle=phase(rng);
        bins[size_t(2*k)]=float(amplitude*std::cos(angle));bins[size_t(2*k+1)]=float(amplitude*std::sin(angle));
    }
    fft.performRealOnlyInverseTransform(bins.data());
    float peak=0;for(int n=0;n<size;++n)peak=std::max(peak,std::abs(bins[size_t(n)]));
    std::vector<float> out(size_t(rate*6));
    for(size_t n=0;n<out.size();++n)out[n]=.2f*bins[n%size]/peak;
    return out;
}
inline void notches() {
    const auto s=settings(7,5);const CabPlacement unsafe{.85,5};
    require(cabRoughnessDb(cabBandsDb(withPlacement(s,unsafe)))<cabRoughnessDb(cabBandsDb(s)),
            "The 6x10 regression no longer exercises the average-roughness blind spot");
    require(!cabPlacementSafe(s,unsafe),"The 6x10 local cancellation notch was accepted");
    double worst=0;int moves=0;
    for(double rate:{44100.,48000.,96000.})for(int index:{3,6,7}) {
        const auto current=settings(index,layout::count(index)-1);
        const auto ir=kernel(current,rate);
        const auto detail=cabDetailDb([&](double hz){return transfer(ir,hz,rate);});
        for(int tilt:{-8,8}) {
            const auto result=analyseCab(target(index>=4,rate,tilt),rate,current,index>=4);
            require(result.playingDetected,"Cabinet notch fixture was not detected");
            if(!result.changed())continue;
            ++moves;
            require(cabPlacementSafe(current,result.proposed),"Search selected an unsafe local peak/notch");
            const auto moved=kernel(withPlacement(current,result.proposed),rate);
            const double added=cabAddedExtremaDb(detail,cabDetailDb([&](double hz){return transfer(moved,hz,rate);}));
            worst=std::max(worst,added);
            require(added<=cabLocalExtremaAllowanceDb+.15,"Generated IR introduced a local peak/notch despite the model guard");
            require(result.toneErrorAfterDb<result.toneErrorBeforeDb-cabToneMinGainDb,"Safe candidate lacks the required tone improvement");
        }
    }
    require(moves>=12,"Notch protection made the search vacuous");
    std::cout<<"PASS cabinet local extrema: "<<moves<<" moves, worst generated-IR addition="<<worst<<" dB\n";
}
inline void run() {
    double worstBand = 0, worstHarmonic = 0, worstLevel = 0;
    int pairs = 0, levels = 0;
    for (double rate : {44100., 48000., 96000.}) for (int index = 1; index <= 9; ++index) {
        // Opposite columns (and bottom row on 6x10) have different interference.
        std::vector<int> units{0};
        if (layout::count(index) > 1) units.push_back(layout::count(index) - 1);
        for (int unit : units) {
            const auto s = settings(index, unit);
            require(layout::isDriverCompatible(s), "Incompatible regression driver");
            const auto current = kernel(s, rate);
            const auto now = cabBandsDb(s);
            for (CabPlacement placement : {CabPlacement{.15, 5}, CabPlacement{.85, 20}}) {
                const auto moved = withPlacement(s, placement);
                const auto next = kernel(moved, rate);
                const auto bands = cabBandsDb(moved);
                const auto label = std::string(layout::layouts[size_t(index - 1)].name)
                    + " unit " + std::to_string(unit) + " at " + std::to_string(int(rate));
                for (int b = 0; b < toneBandCount; ++b) {
                    if (toneBandCentres[size_t(b)] > 6300) continue;
                    double before = 0, after = 0;
                    for (int k = 0; k < 8; ++k) {
                        const double hz = toneBandCentres[size_t(b)] * std::pow(2., (k + .5) / 24. - 1. / 6.);
                        before += std::norm(transfer(current, hz, rate));
                        after += std::norm(transfer(next, hz, rate));
                    }
                    const double error = std::abs(10 * std::log10(after / before) - (bands[size_t(b)] - now[size_t(b)]));
                    worstBand = std::max(worstBand, error);
                    require(error < 1.5, label + " IR/model band delta differs by " + std::to_string(error));
                }
                for (double hz : {27.5, 55., 82.5, 110., 137.5, 165., 220.}) {
                    const double actual = db(transfer(next, hz, rate)) - db(transfer(current, hz, rate));
                    const double predicted = db(layout::response(moved, hz)) - db(layout::response(s, hz));
                    const double error = std::abs(actual - predicted);
                    worstHarmonic = std::max(worstHarmonic, error);
                    require(error < 1.5, label + " low harmonic delta differs by " + std::to_string(error));
                }
                ++pairs;
                // Fundamental alone, low harmonics, and weak upper content at
                // an array-interference frequency. Amplitudes describe POST-cab.
                for (const auto& tones : std::vector<std::vector<std::pair<double, double>>>{
                    {{27.5, .1}}, {{27.5, .1}, {55, .05}, {82.5, .025}, {110, .0125}},
                    {{27.5, .1}, {55, .02}, {990, .01}}}) {
                    const auto heard = steady(current, current, rate, tones);
                    const auto rendered = steady(current, next, rate, tones);
                    const double actual = rmsDb(rendered) - rmsDb(heard);
                    const double predicted = cabLevelChangeDb(measureCabEnergy(heard, rate), s, placement);
                    const double error = std::abs(predicted - actual);
                    worstLevel = std::max(worstLevel, error); ++levels;
                    require(error < .25, label + " level error " + std::to_string(error)
                        + " dB (predicted " + std::to_string(predicted) + ", actual " + std::to_string(actual) + ")");
                }
            }
        }
    }
    std::cout << "PASS cabinet acoustic regression: " << pairs << " IR pairs, " << levels
              << " level cases; worst band=" << worstBand << " dB, harmonic=" << worstHarmonic
              << " dB, level=" << worstLevel << " dB\n";
    notches();
}
}

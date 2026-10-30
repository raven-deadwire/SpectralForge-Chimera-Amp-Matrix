#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <optional>
#include <vector>

namespace spectralforge::autoset {
// Keep the channels separate until after K-weighting and squaring. A mono
// output has a silent second channel; it is never counted as dual mono.
using OutputFrame = std::array<float,2>;
struct WeightingCoefficients {
    double b0,b1,b2,a1,a2;
    double magnitude(double frequency,double rate) const {
        const auto z=std::polar(1.,-2*std::numbers::pi*frequency/rate);
        return std::abs((b0+b1*z+b2*z*z)/(1.+a1*z+a2*z*z));
    }
};
inline std::array<WeightingCoefficients,2> kWeighting(double rate) {
    double k=std::tan(std::numbers::pi*1681.974450955533/rate);
    const double q=.7071752369554196,vh=std::pow(10.,3.999843853973347/20.),vb=std::pow(vh,.4996667741545416);
    double a0=1+k/q+k*k;
    const WeightingCoefficients shelf{(vh+vb*k/q+k*k)/a0,2*(k*k-vh)/a0,(vh-vb*k/q+k*k)/a0,2*(k*k-1)/a0,(1-k/q+k*k)/a0};
    k=std::tan(std::numbers::pi*38.13547087602444/rate);
    const double qh=.5003270373238773;a0=1+k/qh+k*k;
    // BS.1770 Table 2: numerator is exactly {1,-2,1}, including at 48 kHz.
    const WeightingCoefficients highPass{1,-2,1,2*(k*k-1)/a0,(1-k/qh+k*k)/a0};
    return {shelf,highPass};
}
struct LoudnessMeasurement {
    std::optional<float> lufs;
    size_t gatedBlocks{};
};
inline LoudnessMeasurement measureLoudness(const std::vector<OutputFrame>& samples,double rate) {
    LoudnessMeasurement result;
    if(!std::isfinite(rate) || rate<8000 || rate>768000)return result;
    const size_t hop=size_t(std::llround(rate*.1)),block=4*hop;
    if(samples.size()<block)return result;
    const auto k=kWeighting(rate);
    std::array<std::array<double,4>,2> z{};
    std::vector<double> squared(samples.size());
    for(size_t n=0;n<samples.size();++n)for(size_t channel=0;channel<2;++channel) {
        double y=samples[n][channel];
        if(!std::isfinite(y))return result;
        for(size_t s=0;s<2;++s) {
            const auto& c=k[s];auto& state=z[channel];
            const double out=c.b0*y+state[2*s];
            state[2*s]=c.b1*y-c.a1*out+state[2*s+1];state[2*s+1]=c.b2*y-c.a2*out;y=out;
        }
        squared[n]+=y*y;
    }
    const auto lufs=[](double power){return -.691+10*std::log10(std::max(1e-30,power));};
    std::vector<double> blocks;double sum=0;
    for(size_t n=0;n<block;++n)sum+=squared[n];
    for(size_t start=0;;start+=hop) {
        const double power=std::max(0.,sum/double(block));
        if(lufs(power)>-70)blocks.push_back(power);
        if(start+hop+block>squared.size())break;
        for(size_t n=start;n<start+hop;++n)sum-=squared[n];
        for(size_t n=start+block;n<start+block+hop;++n)sum+=squared[n];
    }
    if(blocks.empty())return result;
    double mean=0;for(const auto power:blocks)mean+=power;mean/=double(blocks.size());
    const double relative=lufs(mean)-10;double gated=0;
    for(const auto power:blocks)if(lufs(power)>relative){gated+=power;++result.gatedBlocks;}
    if(result.gatedBlocks)result.lufs=float(lufs(gated/double(result.gatedBlocks)));
    return result;
}
inline std::optional<float> integratedLoudness(const std::vector<OutputFrame>& samples,double rate) {
    return measureLoudness(samples,rate).lufs;
}
inline std::vector<OutputFrame> monoOutput(const std::vector<float>& samples) {
    std::vector<OutputFrame> frames;frames.reserve(samples.size());
    for(const auto x:samples)frames.push_back({x,0});
    return frames;
}
inline std::optional<float> integratedLoudness(const std::vector<float>& samples,double rate) {
    return integratedLoudness(monoOutput(samples),rate);
}
}

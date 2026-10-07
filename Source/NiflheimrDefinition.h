#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace spectralforge::niflheimr {
// Authored prototype: these are independent native voices, not recovered
// circuit coefficients or approved production voicings of reference heads.
enum class Control : std::size_t {
    gain, bass, middle, treble, midFrequency, presence, depth, master,
    mass, split, fang, fold, thrust, blend, count
};
constexpr std::size_t controlCount = std::size_t(Control::count);
constexpr int channelCount = 5;
inline constexpr const char* channelNames[]{
    "Modern Tight", "Death Grind", "Industrial Bite", "Slam Impact", "Sludge Mass"
};
inline constexpr const char* channelKeys[]{
    "modern_tight", "death_grind", "industrial_bite", "slam_impact", "sludge_mass"
};
struct ControlDefinition { const char* id; const char* label; float minimum, maximum, initial; };
inline constexpr std::array<ControlDefinition, controlCount> controls{{
    {"gain","GAIN",0,1,.5f}, {"bass","BASS",0,1,.5f},
    {"middle","MID",0,1,.5f}, {"treble","TREBLE",0,1,.5f},
    {"mid_frequency","MID FREQ",150,2500,650}, {"presence","PRESENCE",0,1,.5f},
    {"depth","DEPTH",0,1,.5f}, {"master","MASTER",0,1,.5f},
    {"mass","MASS",0,1,.5f}, {"split","SPLIT",0,1,.5f},
    {"fang","FANG",0,1,.5f}, {"fold","FOLD",0,1,.5f},
    {"thrust","THRUST",0,1,.5f}, {"blend","BLEND",0,1,.65f}
}};
struct State {
    std::array<float, controlCount> values{};
    int channel = 0;
    constexpr State() {
        for (std::size_t i=0; i<controlCount; ++i) values[i]=controls[i].initial;
    }
    float& operator[](Control c) noexcept { return values[std::size_t(c)]; }
    float operator[](Control c) const noexcept { return values[std::size_t(c)]; }
    bool operator==(const State&) const = default;
    void sanitise() noexcept {
        channel=std::clamp(channel,0,channelCount-1);
        for (std::size_t i=0; i<controlCount; ++i)
            values[i]=std::isfinite(values[i])
                ? std::clamp(values[i],controls[i].minimum,controls[i].maximum)
                : controls[i].initial;
    }
};
constexpr State channelState(int channel) noexcept {
    State s;
    s.channel=std::clamp(channel,0,channelCount-1);
    return s;
}
} // namespace spectralforge::niflheimr

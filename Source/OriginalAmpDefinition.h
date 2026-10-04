#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace spectralforge::original {
// Development engine, deliberately independent of the released native catalog.
// Values are authored starting points, not recovered hardware coefficients.
enum class Control : std::size_t {
    gain, bass, middle, treble, midFrequency, presence, depth, master,
    clank, crush, impact, rot, bloom, count
};
constexpr std::size_t controlCount = std::size_t(Control::count);
struct ControlDefinition { const char* id; const char* label; float minimum, maximum, initial; };
inline constexpr std::array<ControlDefinition, controlCount> controls{{
    {"gain","GAIN",0,1,.72f}, {"bass","BASS",0,1,.5f},
    {"middle","MID",0,1,.55f}, {"treble","TREBLE",0,1,.5f},
    {"mid_frequency","MID FREQ",300,1800,850}, {"presence","PRESENCE",0,1,.5f},
    {"depth","DEPTH",0,1,.5f}, {"master","MASTER",0,1,.45f},
    {"clank","CLANK",0,1,.60f}, {"crush","CRUSH",0,1,.65f},
    {"impact","IMPACT",0,1,.65f}, {"rot","ROT",0,1,.40f},
    {"bloom","BLOOM",0,1,.35f}
}};
struct State {
    std::array<float, controlCount> values{};
    constexpr State() { for(std::size_t i=0;i<controlCount;++i) values[i]=controls[i].initial; }
    float& operator[](Control c) noexcept { return values[std::size_t(c)]; }
    float operator[](Control c) const noexcept { return values[std::size_t(c)]; }
    bool operator==(const State&) const = default;
    void sanitise() noexcept {
        for(std::size_t i=0;i<controlCount;++i)
            values[i]=std::isfinite(values[i]) ? std::clamp(values[i],controls[i].minimum,controls[i].maximum) : controls[i].initial;
    }
};
struct GainCellDefinition { float drive, couplingHz, bandwidthHz, bias; };
struct Definition {
    std::string_view id, name;
    std::array<GainCellDefinition,4> stages;
    float inputHighPassHz, outputLowPassHz, smoothingSeconds;
};
inline constexpr Definition nastrond{
    "original.nastrond.v1", "Náströnd",
    {{{3.5f,65,13000,.025f},{3.0f,100,11500,.045f},
      {2.5f,85,10000,-.030f},{1.8f,50,9000,.020f}}},
    28,12500,.020f
};
struct Preset { const char* id; const char* name; const char* role; State state; };
constexpr State presetState(float gain,float clank,float crush,float impact,float rot,float bloom) {
    State s;
    s.values[std::size_t(Control::gain)]=gain;
    s.values[std::size_t(Control::clank)]=clank;
    s.values[std::size_t(Control::crush)]=crush;
    s.values[std::size_t(Control::impact)]=impact;
    s.values[std::size_t(Control::rot)]=rot;
    s.values[std::size_t(Control::bloom)]=bloom;
    return s;
}
// These are one amp's snapshots, never five models or Deadwire replacements.
inline constexpr std::array<Preset,5> presets{{
    {"original.nastrond.fenrir.v1","Fenrir","Tight low-tuned rhythm",presetState(.72f,.85f,.55f,.65f,.15f,.10f)},
    {"original.nastrond.surtr.v1","Surtr","Dense sustained lead",presetState(.78f,.55f,.90f,.45f,.30f,.25f)},
    {"original.nastrond.nidhoggr.v1","Níðhöggr","Asymmetric decaying grind",presetState(.76f,.40f,.55f,.50f,.90f,.45f)},
    {"original.nastrond.fimbulvetr.v1","Fimbulvetr","Broad low-mid sustain",presetState(.70f,.20f,.45f,.45f,.45f,.95f)},
    {"original.nastrond.ragnarok.v1","Ragnarök","Heavy transient impact",presetState(.78f,.70f,.80f,.95f,.60f,.50f)}
}};
} // namespace spectralforge::original

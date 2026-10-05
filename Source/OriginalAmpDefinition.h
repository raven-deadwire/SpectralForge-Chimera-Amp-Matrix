#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace spectralforge::original {
// Shared Original definition for the production native route and isolated harness.
// Values are authored starting points, not recovered hardware coefficients.
enum class Control : std::size_t {
    gain, bass, middle, treble, midFrequency, presence, depth, master,
    clank, crush, impact, rot, bloom, count
};
constexpr std::size_t controlCount = std::size_t(Control::count);
constexpr int channelCount=5;
inline constexpr const char* channelNames[]{"Fenrir","Surtr","Níðhöggr","Fimbulvetr","Ragnarök"};
inline constexpr const char* channelKeys[]{"fenrir","surtr","nidhoggr","fimbulvetr","ragnarok"};
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
    int channel=0;
    bool modern=false;
    constexpr State() { for(std::size_t i=0;i<controlCount;++i) values[i]=controls[i].initial; }
    float& operator[](Control c) noexcept { return values[std::size_t(c)]; }
    float operator[](Control c) const noexcept { return values[std::size_t(c)]; }
    bool operator==(const State&) const = default;
    void sanitise() noexcept {
        channel=std::clamp(channel,0,channelCount-1);
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
// Frozen v1 snapshots for backwards-compatible offline measurement.
inline constexpr std::array<Preset,5> presets{{
    {"original.nastrond.fenrir.v1","Fenrir","Tight low-tuned rhythm",presetState(.72f,.85f,.55f,.65f,.15f,.10f)},
    {"original.nastrond.surtr.v1","Surtr","Dense sustained lead",presetState(.78f,.55f,.90f,.45f,.30f,.25f)},
    {"original.nastrond.nidhoggr.v1","Níðhöggr","Asymmetric decaying grind",presetState(.76f,.40f,.55f,.50f,.90f,.45f)},
    {"original.nastrond.fimbulvetr.v1","Fimbulvetr","Broad low-mid sustain",presetState(.70f,.20f,.45f,.45f,.45f,.95f)},
    {"original.nastrond.ragnarok.v1","Ragnarök","Heavy transient impact",presetState(.78f,.70f,.80f,.95f,.60f,.50f)}
}};
// Frozen v1 snapshots above remain readable by the measurement harness. The
// product now has five actual channels, each with its own 13-knob memory.
// These starting levels follow the owner's Dual high-gain reference: no channel
// should require an almost-maxed control panel just to enter the intended range.
constexpr State channelState(int channel) {
    State s;
    s.channel=channel;s.modern=true;
    s.values[std::size_t(Control::gain)]=.5f;
    s.values[std::size_t(Control::master)]=.5f;
    for(std::size_t i=0;i<5;++i)s.values[std::size_t(Control::clank)+i]=.5f;
    return s;
}
// The owner's strong Dual sound is the midpoint, not the upper endpoint.
// Rows express each channel's effective CLANK/CRUSH/IMPACT/ROT/BLOOM at noon.
inline constexpr std::array<std::array<float,5>,channelCount> channelMidpoints{{
    {{1.f,1.f,1.f,.55f,.65f}}, {{.85f,1.15f,1.f,.70f,.85f}},
    {{1.f,1.10f,1.f,1.f,1.f}}, {{.70f,1.05f,.95f,.85f,1.15f}},
    {{1.f,1.15f,1.15f,.95f,.95f}}
}};
struct ChannelVoice {
    std::array<float,4> drive;
    float coupling,bandwidth,asymmetry,sag;
};
// Fenrir retains the v1 transfer function for old sessions. Other channels alter
// the interstage network and power response independently of the visible knobs.
inline constexpr std::array<ChannelVoice,channelCount> channelVoices{{
    {{{1,1,1,1}},1,1,1,1},
    {{{1.12f,1.15f,1.08f,1}},.90f,.93f,1,1.15f},
    {{{1,1.05f,1.10f,1.05f}},.83f,.90f,1.65f,1.15f},
    {{{1.12f,1.08f,1.03f,1}},.65f,.83f,1.15f,1.45f},
    {{{1.05f,1.10f,1.10f,1.15f}},1.08f,1.03f,1.20f,.70f}
}};
// Audition revision: separate distortion distribution, coupling, supply timing
// and fixed voice contours. Only modern channels use this table. The frozen v1
// network above remains the migration/reference path, not a factory menu bank.
struct ChannelCharacter {
    ChannelVoice circuit;
    float preHz,preDb,lowDb,midHz,midDb,highDb,bodyDb,edgeDb;
    float sagAttackSeconds,sagReleaseSeconds,recoveryScale,feedback,transient;
    float outputDb;
};
inline constexpr std::array<ChannelCharacter,channelCount> channelCharacters{{
    // Fenrir: early bite, lean interstage lows, fast and firm recovery.
    {{{{1.35f,.85f,.62f,.55f}},1.75f,1.12f,.65f,.35f},1600,3,-3,1200,1,.5f,-2,4,.004f,.045f,.55f,.20f,.30f,5.5f},
    // Surtr: distributed compression, forward vocal mids, rounded top.
    {{{{.95f,1.40f,1.55f,1.25f}},1.05f,.85f,.85f,1.8f},750,2,0,700,5,-3,1,0,.008f,.160f,1,.12f,.03f,2.2f},
    // Nidhoggr: uneven stage drive and pronounced asymmetric recovery.
    {{{{.75f,1.75f,.55f,1.80f}},.70f,.68f,4.f,.70f},1100,-2,-1.5f,950,-2.5f,1.2f,2,2,.006f,.090f,2.4f,.07f,-.08f,1.4f},
    // Fimbulvetr: broad body and yielding supply, with open upper mids.
    {{{{1.05f,.70f,.75f,.65f}},.55f,.85f,1.8f,3.f},400,1,2,650,2,-1.5f,1,.5f,.018f,.300f,1.8f,.04f,-.15f,3.f},
    // Ragnarok: late-stage saturation, deep hit and stiff supply recovery.
    {{{{.85f,.85f,1.50f,1.80f}},.90f,1.10f,1.3f,.20f},2200,2,4,600,-3.5f,0,-2,1,.003f,.055f,.75f,.22f,.55f,3.f}
}};
} // namespace spectralforge::original

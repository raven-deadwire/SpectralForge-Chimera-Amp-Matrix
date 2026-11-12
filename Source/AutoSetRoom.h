#pragma once
#include "AutoSetSpace.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace spectralforge::autoset {

// Phase 26: the Room Core room (docs/ROOM_CORE_1_3_3.md), from how the player
// plays. A result step of the input pass, like phase 11: the same playing
// feel (notes per second) and the chosen or detected instrument.
// - Size, absorption and return by style: clean a medium, livelier room;
//   rock and metal a small, damped one, metal quietest. Bass sits lower in it.
// - Dense playing: a smaller, more absorbent and quieter room, so notes do not
//   blur; sparse playing a larger, slightly louder one.
// The step only sets the shared room; its sends, Room Advanced and the
// declared space stay the player's (a new room opens the sends only when all
// of the routing's sends are at 0).
struct RoomSetting {int size{};float absorption{},returnDb{};};
struct RoomResult {
    PlayingFeel feel{};
    bool dense{},sparse{};
    RoomSetting room{};
};
inline constexpr int roomSizeSmall=0,roomSizeMedium=1,roomSizeLarge=2;
inline constexpr float roomReturnMinDb=-36.f,roomReturnMaxDb=6.f;
// A new room's send when every send of the routing is at 0 (the parameter default).
inline constexpr float roomOpenSend=50.f;
inline const char* roomSizeName(int size) noexcept {return size<=roomSizeSmall?"Small":size==roomSizeMedium?"Medium":"Large";}

inline RoomResult analyseRoom(const std::vector<Window>& windows,bool bass,Style style) {
    RoomResult r;r.feel=measurePlayingFeel(windows);
    if(!r.feel.measured)return r;
    r.dense=r.feel.onsetsPerSecond>spaceDenseOnsets;
    r.sparse=r.feel.onsetsPerSecond<spaceSparseOnsets;
    int size=style==Style::clean?roomSizeMedium:roomSizeSmall;
    float absorption=style==Style::clean?40.f:style==Style::metal?70.f:55.f;
    float returnDb=style==Style::clean?-10.f:style==Style::metal?-20.f:-14.f;
    // Bass: a smaller, deader, quieter room keeps the low end defined.
    if(bass){size=roomSizeSmall;absorption+=10.f;returnDb-=6.f;}
    if(r.dense){--size;absorption+=15.f;returnDb-=4.f;}
    else if(r.sparse){++size;returnDb+=2.f;}
    r.room.size=std::clamp(size,roomSizeSmall,bass?roomSizeMedium:roomSizeLarge);
    r.room.absorption=std::clamp(absorption,0.f,100.f);
    r.room.returnDb=std::clamp(returnDb,roomReturnMinDb,roomReturnMaxDb);
    return r;
}
// A return in dB to the parameter's step used here (0.5 dB).
inline float roomReturnRounded(float db) noexcept {return std::clamp(std::round(db*2.f)/2.f,roomReturnMinDb,roomReturnMaxDb);}
// A new room at a strength below 1 is quieter, as a mix scales: the return
// drops by 20 log10(strength). Below the return's range the room stays off.
inline float roomNewReturn(float targetDb,float strength) noexcept {
    if(strength>=1.f)return roomReturnRounded(targetDb);
    if(strength<=0.f)return roomReturnMinDb-1.f;
    const float db=targetDb+20.f*std::log10(strength);
    return db<roomReturnMinDb?db:roomReturnRounded(db);
}

}

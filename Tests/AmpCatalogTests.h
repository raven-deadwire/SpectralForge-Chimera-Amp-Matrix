#pragma once
#include "AmpCatalog.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace ampCatalogTests {
inline void run() {
    using namespace spectralforge;
    const auto check=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    static_assert(ampModelCount==15 && static_cast<int>(AmpModel::modernBass)==7 && static_cast<int>(AmpModel::tastePunch)==14);
    constexpr std::array<const char*,15> expected{{"Glass","Brit Edge","Tight 515","Wide Rect","Liquid Lead","Iron Tube","Solid Punch","Modern Bass","Chime 30","Orange Crown","Bassman Valve","Subway Clean","Match Chime","Silk ODS","Taste Punch"}};
    int guitars=0,basses=0;
    for(int i=0;i<ampModelCount;++i) {
        check(std::strcmp(ampCatalog[size_t(i)].name,expected[size_t(i)])==0,"Amp names or raw order changed");
        check(ampIndexFromMenuId(i+1)==i,"Amp menu ID no longer maps to original raw value");
        check(ampMatches(i),"All filter excludes existing amp");
        check(ampMatches(i,AmpRole::bass)==ampCatalog[size_t(i)].bass,"Bass role mismatches inventory");
        guitars+=ampMatches(i,AmpRole::any,AmpInstrument::guitar)?1:0;
        basses+=ampMatches(i,AmpRole::any,AmpInstrument::bass)?1:0;
        bool visible=false;for(const auto& role:ampRoleChoices)visible|=ampMatches(i,role.role);
        check(visible,"Existing amp has no browse role");
    }
    check(guitars==9&&basses==6,"Instrument filter coverage changed");
    check(ampMatches(7,AmpRole::highGain,AmpInstrument::bass),"Driven bass must remain high gain discoverable");
    check(ampMatches(1,AmpRole::clean)&&ampMatches(1,AmpRole::crunch),"Overlapping roles lost");
    check(!ampMatches(-1)&&!ampMatches(15),"Invalid index accepted by filter");
    check(ampIndexFromMenuId(0)==-1&&ampIndexFromMenuId(1001)==-1,"Pending menu item became an audio model");
    int required=0;for(const auto& pending:pendingAmpTargets)required+=pending.required?1:0;
    check(pendingAmpTargets.size()==8&&required==5,"Required/candidate target separation changed");
    std::cout<<"PASS: stable 15 amp IDs/names, overlapping role and instrument filters, 8 disabled development targets\n";
}
}

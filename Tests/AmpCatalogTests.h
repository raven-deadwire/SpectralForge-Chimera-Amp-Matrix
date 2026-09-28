#pragma once
#include "AmpCatalog.h"
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
namespace ampCatalogTests {
inline void run() {
    using namespace spectralforge;
    const auto check=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    static_assert(legacyAmpModelCount==15 && ampModelCount==23 && static_cast<int>(AmpModel::modernBass)==7 && static_cast<int>(AmpModel::tastePunch)==14);
    check(legacyAmpNames().size()==15,"Legacy normalized host range changed");
    std::set<std::string> names;int guitars=0,basses=0;
    for(int i=0;i<ampModelCount;++i) {
        check(names.insert(ampInfo(i).name).second,"Duplicate Chimera amp name");
        check(ampIndexFromMenuId(i+1)==i,"UI amp ID changed");
        check(ampMatches(i),"Existing amp excluded from inventory");
        int primaryCount=0;for(const auto& role:ampRoleChoices)primaryCount+=ampPrimaryRoles[(size_t)i]==role.role;
        check(primaryCount==1,"Amp must have exactly one visible category");
        check((ampPrimaryRoles[(size_t)i]==AmpRole::bass)==ampInfo(i).bass,"Bass amp primary category incorrect");
        guitars+=!ampInfo(i).bass;basses+=ampInfo(i).bass;
    }
    check(guitars==16&&basses==7,"New amp inventory incomplete");
    check(!ampMatches(-1)&&!ampMatches(23),"Invalid index accepted");
    check(ampIndexFromMenuId(0)==-1&&ampIndexFromMenuId(1001)==-1,"Invalid popup item accepted");
    std::cout<<"PASS: 23 unique Chimera heads, one primary category each, preserved 15-value legacy host range\n";
}
}

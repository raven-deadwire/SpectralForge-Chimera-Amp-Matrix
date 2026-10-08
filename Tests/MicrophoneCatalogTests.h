#pragma once
#include "MicrophoneCatalog.h"
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

inline void runMicrophoneCatalogTests() {
    namespace M=spectralforge::micCatalog;
    const auto check=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    static_assert(M::models.size()==20);
    std::set<std::string> ids,aliases;
    int dynamic=0,ribbon=0,condenser=0,original=0;
    for(const auto& model:M::models) {
        check(ids.insert(model.id).second && aliases.insert(model.alias).second,"Microphone catalog IDs/aliases must be unique");
        check(M::byId(model.id)==&model,"Microphone catalog ID lookup failed");
        check(M::identify(model.alias)==&model,"Chimera microphone alias is not recognized");
        dynamic+=model.kind==M::Kind::dynamic;
        ribbon+=model.kind==M::Kind::ribbon;
        condenser+=model.kind==M::Kind::condenser;
        original+=model.original;
        if(model.original) {
            check(std::string(model.id)=="chimera-strike" && model.kind==M::Kind::condenser
                && std::string(model.reference).empty(),"Strike must be the original condenser without an external reference");
        } else {
            check(std::string(model.reference).size()>0 && M::identify(model.reference)==&model,"External microphone reference was lost or misidentified");
        }
    }
    check(dynamic==9 && ribbon==3 && condenser==8 && original==1,"Agreed 9 dynamic / 3 ribbon / 8 condenser catalog balance changed");
    check(M::byId("")==nullptr && M::byId("dynamic-58")==nullptr && M::byId("dynamic-609")==nullptr
        && M::byId("dynamic-52")==nullptr && M::byId("DYNAMIC-57")==nullptr,"Invalid or retired microphone ID accepted");
    check(std::string(M::kindLabel(M::Kind::dynamic))=="Dynamic"
        && std::string(M::kindLabel(M::Kind::ribbon))=="Ribbon"
        && std::string(M::kindLabel(M::Kind::condenser))=="Condenser","Microphone category labels changed");

    struct Case { const char* input; const char* id; };
    for(const auto& sample:{
            Case{"ShUrE SM-57 (filename inference)","dynamic-57"},
            Case{"Sennheiser MD 421 II","dynamic-421"},
            Case{"MD441U","dynamic-441"},
            Case{"Sennheiser e906, Flat (filename inference)","dynamic-906"},
            Case{"Electro-Voice RE-20","dynamic-20"},
            Case{"Beyerdynamic M201TG","dynamic-201"},
            Case{"beyerdynamic M 201 TG","dynamic-201"},
            Case{"M88TG","dynamic-88"},
            Case{"AKG D112MkII","dynamic-112"},
            Case{"Royer R 121","ribbon-121"},
            Case{"Coles-4038","ribbon-4038"},
            Case{"Neumann U87Ai","condenser-87"},
            Case{"AKG C414XLS","condenser-414"},
            Case{"Neumann U-47 FET","condenser-47-fet"},
            Case{"U47fet","condenser-47-fet"},
            Case{"Audio Technica AT-4050","condenser-4050"},
            Case{"Mojave MA-201fet","condenser-201-fet"},
            Case{"Mojave MA 201 FET","condenser-201-fet"},
            Case{"Neumann U-67","condenser-67"},
            Case{"CHIMERA-STRIKE","chimera-strike"}})
        check(M::identify(sample.input)==M::byId(sample.id),"Microphone spelling/variant matched the wrong catalog identity");

    for(const auto* sample:{"R1210", "RE200", "SM570", "NotSM57", "AT40500", "MD4210", "KM1840",
                            "Shure SM58", "Sennheiser e609", "Shure Beta 52A", "Audix i5", "Chimera i5",
                            "Neumann U47", "Neumann U-47 tube", "Mojave MA201", "201", "47", "57", "121",
                            "Dynamic", "Ribbon", "Condenser", "Strike", "", "Unknown"})
        check(M::identify(sample)==nullptr,"Unknown, ambiguous, retired or substring microphone was guessed");
    check(M::identify("M201TG")!=M::identify("MA201fet"),"M201 dynamic was confused with MA-201fet condenser");
    for(const auto* sample:{"SM57 + R121", "SM57/R121", "SM57 and Audix i5", "SM57 & unknown ribbon",
                            "SM57 U87", "SM57 + SM57", "SM57 blend", "SM57 MIX", "Fredman SM57",
                            "Two-mic SM57", "SM57 with room mic", "SM57 + PG57", "SM57; U47 tube",
                            "SM57 (source model names conflict)", "SM57 (unconfirmed)"})
        check(M::identify(sample)==nullptr,"Blended or conflicting microphone metadata became a single catalog model");

    for(const auto& sample:{
            Case{"Mesa_4x12_SM57_center.wav","dynamic-57"},
            Case{"Mesa_R-121_edge.wav","ribbon-121"},
            Case{"Delta_AT4050_Centre_1_Inch.wav","condenser-4050"},
            Case{"Cab_MA-201fet.wav","condenser-201-fet"},
            Case{"/captures/U87/Mesa_SM57.wav","dynamic-57"},
            Case{"C:\\captures\\R121\\Mesa_SM57.wav","dynamic-57"},
            Case{"/captures/SM57/Chimera_Strike.wav","chimera-strike"}})
        check(M::fromFilename(sample.input)==M::byId(sample.id),"Microphone filename hint failed or leaked from a parent directory");
    for(const auto* sample:{"Mesa_57.wav", "Mesa_121.wav", "Cab_201.wav", "1970_Cab_87.wav",
                            "Mesa_R1210.wav", "Bass_RE200.wav", "Cab_U47.wav", "Cab_Audix_i5.wav",
                            "/captures/SM57/Mesa.wav", "C:\\captures\\SM57\\Mesa.wav",
                            "Cab_SM57_R121.wav", "Cab_SM57_mix.wav", "Cab_SM57_mix01.wav",
                            "Cab_SM57_blend2.wav", "Cab_SM57_Fredman.wav", "Cab_SM57_dualmic.wav",
                            "Traynor1510_Apex435_Pg57_Sm57_mono_dc-1s.wav"})
        check(M::fromFilename(sample)==nullptr,"Ambiguous, mixed or path-only filename guessed a microphone");

    std::cout<<"PASS: 20 stable microphone identities, 9/3/8 categories, original condenser branding, conservative metadata and filename matching\n";
}

#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <string>
#include <vector>

namespace spectralforge::micCatalog {
enum class Kind { dynamic, ribbon, condenser };
struct Model {
    const char* id;
    const char* alias;
    const char* reference;
    Kind kind;
    bool original;
};

// Catalog identities describe microphones, not bundled captures or DSP models.
// IDs are metadata keys; array positions must never become host parameter IDs.
inline constexpr std::array<Model,20> models{{
    {"dynamic-57", "Dynamic 57", "Shure SM57", Kind::dynamic, false},
    {"dynamic-421", "Dynamic 421", "Sennheiser MD421", Kind::dynamic, false},
    {"dynamic-441", "Dynamic 441", "Sennheiser MD441", Kind::dynamic, false},
    {"dynamic-906", "Dynamic 906", "Sennheiser e906", Kind::dynamic, false},
    {"dynamic-20", "Dynamic 20", "Electro-Voice RE20", Kind::dynamic, false},
    {"dynamic-7", "Dynamic 7", "Shure SM7B", Kind::dynamic, false},
    {"dynamic-201", "Dynamic 201", "beyerdynamic M201", Kind::dynamic, false},
    {"dynamic-88", "Dynamic 88", "beyerdynamic M88", Kind::dynamic, false},
    {"dynamic-112", "Dynamic 112", "AKG D112", Kind::dynamic, false},
    {"ribbon-121", "Ribbon 121", "Royer R-121", Kind::ribbon, false},
    {"ribbon-160", "Ribbon 160", "beyerdynamic M160", Kind::ribbon, false},
    {"ribbon-4038", "Ribbon 4038", "Coles 4038", Kind::ribbon, false},
    {"condenser-87", "Condenser 87", "Neumann U87", Kind::condenser, false},
    {"condenser-414", "Condenser 414", "AKG C414", Kind::condenser, false},
    {"condenser-184", "Condenser 184", "Neumann KM184", Kind::condenser, false},
    {"condenser-47-fet", "Condenser 47 FET", "Neumann U47 fet", Kind::condenser, false},
    {"condenser-4050", "Condenser 4050", "Audio-Technica AT4050", Kind::condenser, false},
    {"condenser-201-fet", "Condenser 201 FET", "Mojave MA-201fet", Kind::condenser, false},
    {"condenser-67", "Condenser 67", "Neumann U67", Kind::condenser, false},
    {"chimera-strike", "Chimera Strike", "", Kind::condenser, true}
}};

inline const char* kindLabel(Kind kind) {
    switch(kind) {
        case Kind::dynamic: return "Dynamic";
        case Kind::ribbon: return "Ribbon";
        case Kind::condenser: return "Condenser";
    }
    return "Unknown";
}

inline const Model* byId(const juce::String& id) {
    for(const auto& model:models)if(id==model.id)return &model;
    return nullptr;
}

namespace detail {
using Words=std::vector<std::string>;
inline Words words(const juce::String& text) {
    Words result;
    std::string word;
    for(const auto c:text.toLowerCase().toStdString()) {
        if((c>='a' && c<='z') || (c>='0' && c<='9'))word+=c;
        else if(!word.empty()){result.push_back(word);word.clear();}
    }
    if(!word.empty())result.push_back(word);
    return result;
}

inline std::string compact(const juce::String& text) {
    std::string result;
    for(const auto& word:words(text))result+=word;
    return result;
}

// Match complete words, allowing a model's letters/numbers to be separated by
// spaces, underscores or hyphens. Never match R121 inside R1210, for example.
inline bool contains(const Words& text,const std::string& identity) {
    if(identity.empty())return false;
    for(size_t first=0;first<text.size();++first) {
        std::string joined;
        for(size_t last=first;last<text.size() && joined.size()<identity.size();++last) {
            joined+=text[last];
            if(joined==identity)return true;
            if(identity.compare(0,joined.size(),joined)!=0)break;
        }
    }
    return false;
}

struct Spellings { const char* id; std::array<const char*,6> tokens; };
// These are family spellings, not a claim that a particular revision was used.
// Bare numbers have no safe meaning in arbitrary IR filenames.
inline constexpr std::array<Spellings,19> spellings{{
    {"dynamic-57", {"sm57"}},
    {"dynamic-421", {"md421", "md421ii"}},
    {"dynamic-441", {"md441", "md441u"}},
    {"dynamic-906", {"e906"}},
    {"dynamic-20", {"re20"}},
    {"dynamic-7", {"sm7b"}},
    {"dynamic-201", {"m201", "m201tg"}},
    {"dynamic-88", {"m88", "m88tg"}},
    {"dynamic-112", {"d112", "d112mkii", "d112mk2"}},
    {"ribbon-121", {"r121", "royer121"}},
    {"ribbon-160", {"m160"}},
    {"ribbon-4038", {"coles4038"}},
    {"condenser-87", {"u87", "u87ai", "u87i"}},
    {"condenser-414", {"c414", "c414xls", "c414xlii", "c414buls", "c414bxlii", "c414eb"}},
    {"condenser-184", {"km184"}},
    {"condenser-47-fet", {"u47fet"}},
    {"condenser-4050", {"at4050"}},
    {"condenser-201-fet", {"ma201fet"}},
    {"condenser-67", {"u67"}}
}};

inline bool ambiguous(const juce::String& text,const Words& tokens) {
    // An explicit blend or conflicting attribution must stay unclassified,
    // including when only one of its microphones belongs to this catalog.
    if(text.containsAnyOf("+/&"))return true;
    for(const auto* marker:{"mix", "mixed", "mixture", "blend", "blended", "fredman",
                            "and", "with", "multi", "dual", "microphones", "mics",
                            "multimic", "dualmic", "twomic", "threemic", "2mic", "3mic", "x2", "x3",
                            "unknown", "unidentified", "uncertain", "unconfirmed",
                            "unspecified", "conflict", "conflicting", "not"})
        if(contains(tokens,marker))return true;
    for(const auto& word:tokens)for(const std::string prefix:{"mix", "blend", "fredman"}) {
        if(word.size()>prefix.size() && word.compare(0,prefix.size(),prefix)==0
            && word.find_first_not_of("0123456789",prefix.size())==std::string::npos)return true;
    }
    // Preserve out-of-catalog and legacy mic identities. This also prevents a
    // filename such as Apex435_PG57_SM57 from becoming a single-SM57 capture.
    for(const auto* other:{"sm58", "e609", "beta52", "beta52a", "b52a", "i5", "r10",
                           "pg57", "pg81", "pg87", "apex435", "apex235b", "at2020",
                           "at2021", "v7", "v7x", "mk012", "oktava012", "u47tube"})
        if(contains(tokens,other))return true;
    // U47 without FET is not the agreed U47 fet microphone.
    return contains(tokens,"u47") && !contains(tokens,"u47fet");
}

inline const Model* identifyText(const juce::String& text) {
    const auto tokens=words(text);
    if(tokens.empty() || ambiguous(text,tokens))return nullptr;
    const Model* found=nullptr;
    const auto add=[&found](const Model* candidate) {
        if(found!=nullptr && found!=candidate)return false;
        found=candidate;
        return true;
    };
    for(const auto& model:models) {
        if(contains(tokens,compact(model.alias)) || contains(tokens,compact(model.reference)))
            if(!add(&model))return nullptr;
    }
    for(const auto& spelling:spellings) {
        for(const auto* token:spelling.tokens) {
            if(token!=nullptr && contains(tokens,token)) {
                if(!add(byId(spelling.id)))return nullptr;
                break;
            }
        }
    }
    return found;
}
} // namespace detail

// Identification preserves the supplied metadata elsewhere; this is only a
// conservative catalog association. Unknown or mixed captures return nullptr.
inline const Model* identify(const juce::String& microphoneMetadata) {
    return detail::identifyText(microphoneMetadata);
}

inline const Model* fromFilename(const juce::String& filename) {
    // Parent folder names never supply evidence about the capture microphone.
    const auto leaf=filename.replaceCharacter('\\','/').fromLastOccurrenceOf("/",false,false);
    return detail::identifyText(leaf);
}
} // namespace spectralforge::micCatalog

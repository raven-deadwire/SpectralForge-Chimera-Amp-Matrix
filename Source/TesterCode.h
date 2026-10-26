#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#if __has_include("ChimeraTesterSecret.h")
#include "ChimeraTesterSecret.h"
#endif
#ifndef CHIMERA_TESTER_SECRET_HEX
#define CHIMERA_TESTER_SECRET_HEX ""
#endif

namespace spectralforge::beta::tester {
// Beta tester codes lift the beta expiry for this and every later beta built
// with the same secret. A code is short enough to type: CHT1-<NAME>-<CHECK>,
// e.g. CHT1-ZIEL-7K3Q9X. CHECK is the first 30 bits of
// HMAC-SHA256(secret, domain + "|" + NAME) in Crockford base32, so one code
// does not reveal another. The secret is injected at configure time from the
// CHIMERA_TESTER_SECRET environment variable (Tools/tester_code.py secret) and
// is never committed; a build without it accepts no codes. Anyone who extracts
// the secret from a binary can make codes: like the expiry itself, this is basic
// offline control, not tamper-proof DRM. The accepted code is stored for this
// user on this computer and is never written into a project, preset or reference file.
inline constexpr const char* prefix = "CHT1";
inline constexpr const char* domain = "chimera-tester-v1";
inline constexpr const char* alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
inline constexpr int minNameLength = 2;
inline constexpr int maxNameLength = 16;
inline constexpr int checkLength = 6;
inline constexpr int maxCodeLength = 64;

struct Grant {
    bool valid{};
    juce::String name, error;
};

namespace detail {
inline juce::MemoryBlock hmacSHA256(const juce::MemoryBlock& key,const juce::String& message) {
    constexpr size_t block=64;
    juce::MemoryBlock k(key);
    if(k.getSize()>block)k=juce::SHA256(k.getData(),k.getSize()).getRawData();
    k.ensureSize(block,true);
    juce::MemoryBlock inner,outer;
    for(size_t i=0;i<block;++i) {
        const auto b=(juce::uint8)k[i];
        const juce::uint8 in=b^0x36,out=b^0x5c;inner.append(&in,1);outer.append(&out,1);
    }
    const auto text=message.toUTF8();
    inner.append(text.getAddress(),text.sizeInBytes()-1);
    const auto innerHash=juce::SHA256(inner.getData(),inner.getSize()).getRawData();
    outer.append(innerHash.getData(),innerHash.getSize());
    return juce::SHA256(outer.getData(),outer.getSize()).getRawData();
}
inline juce::String check(const juce::MemoryBlock& secret,const juce::String& name) {
    const auto mac=hmacSHA256(secret,juce::String(domain)+"|"+name);
    juce::uint32 bits=0;
    for(size_t i=0;i<4;++i)bits=(bits<<8)|(juce::uint8)mac[i];
    juce::String out;
    for(int i=0;i<checkLength;++i)out+=juce::String::charToString(alphabet[(bits>>(27-5*i))&31u]);
    return out;
}
inline bool validName(const juce::String& name) {
    return name.length()>=minNameLength && name.length()<=maxNameLength
        && name.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
}
// Typing aids for the check part only: lower case, O for 0, I or L for 1.
inline juce::String canonicalCheck(juce::String text) {
    return text.toUpperCase().replaceCharacter('O','0').replaceCharacter('I','1').replaceCharacter('L','1');
}
} // namespace detail

inline juce::MemoryBlock productSecret() {
    juce::MemoryBlock secret;secret.loadFromHexString(CHIMERA_TESTER_SECRET_HEX);return secret;
}
inline bool enabled(const juce::MemoryBlock& secret=productSecret()) {return secret.getSize()>=16;}

inline Grant verify(juce::String code,const juce::MemoryBlock& secret=productSecret()) {
    if(!enabled(secret))return {false,{},"Beta tester codes are not enabled in this build."};
    code=code.removeCharacters(" \t\r\n");
    const auto parts=juce::StringArray::fromTokens(code,"-",{});
    if(code.length()>maxCodeLength || parts.size()!=3 || parts[0].toUpperCase()!=prefix)
        return {false,{},"Not a Chimera beta tester code. It looks like CHT1-NAME-XXXXXX."};
    const auto name=parts[1].toUpperCase();
    if(!detail::validName(name) || parts[2].length()!=checkLength)
        return {false,{},"The code is incomplete. Copy it again in full."};
    if(detail::canonicalCheck(parts[2])!=detail::check(secret,name))
        return {false,{},"This code is not valid for Chimera."};
    return {true,name,{}};
}
// The one spelling a verified code is stored and shown in.
inline juce::String canonical(const Grant& grant,const juce::MemoryBlock& secret=productSecret()) {
    return grant.valid?juce::String(prefix)+"-"+grant.name+"-"+detail::check(secret,grant.name):juce::String{};
}

// Per-user, per-computer storage. Only a verified code is ever written.
struct Store {
    static juce::File file() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SpectralForge/Chimera/beta-tester-code.txt");
    }
    static juce::String read(const juce::File& storage=file()) {
        return storage.existsAsFile() && storage.getSize()<=maxCodeLength ? storage.loadFileAsString().trim() : juce::String{};
    }
    static Grant load(const juce::File& storage=file(),const juce::MemoryBlock& secret=productSecret()) {
        const auto code=read(storage);
        return code.isEmpty() ? Grant{} : verify(code,secret);
    }
    static Grant save(const juce::String& code,const juce::File& storage=file(),const juce::MemoryBlock& secret=productSecret()) {
        auto grant=verify(code,secret);
        if(grant.valid && !(storage.getParentDirectory().createDirectory().wasOk() && storage.replaceWithText(canonical(grant,secret))))
            grant={false,{},"The code is valid but could not be saved on this computer."};
        return grant;
    }
    static bool clear(const juce::File& storage=file()) {return !storage.exists() || storage.deleteFile();}
};
} // namespace spectralforge::beta::tester

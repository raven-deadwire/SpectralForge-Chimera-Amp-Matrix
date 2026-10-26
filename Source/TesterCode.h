#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include "TesterCodeKey.h"

namespace spectralforge::beta::tester {
// Beta tester codes lift the beta expiry for every later beta build signed
// with the same key. A code names one tester and is verified offline against
// the public key in TesterCodeKey.h; issuing needs the owner's private key
// (Tools/tester_code.py). Like the expiry itself, this is basic offline
// control, not tamper-proof DRM. The accepted code is stored for this user on
// this computer and is never written into a project, preset or reference file.
//
// Format: CHT1-<base64url payload>.<base64url signature>. The signature is an
// RSA full-domain hash: s^e mod n == 0x00 || SHA-256(domain || i || payload)...
// It covers the exact payload bytes, so the JSON is only read, never re-encoded.
inline constexpr const char* prefix = "CHT1-";
inline constexpr const char* domain = "chimera-tester-v1";
inline constexpr int maxNameLength = 64;
inline constexpr int maxCodeLength = 2048;

struct Grant {
    bool valid{};
    juce::String name, id, issued, error;
};

namespace detail {
inline bool decodeBase64Url(const juce::String& text, juce::MemoryBlock& out) {
    if(text.isEmpty() || !text.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"))return false;
    auto standard=text.replaceCharacter('-','+').replaceCharacter('_','/');
    while(standard.length()%4!=0)standard+="=";
    juce::MemoryOutputStream decoded(out,false);
    if(!juce::Base64::convertFromBase64(decoded,standard))return false;
    decoded.flush();
    // One spelling per code: unused trailing bits must be zero.
    auto again=juce::Base64::toBase64(out.getData(),out.getSize()).replaceCharacter('+','-').replaceCharacter('/','_');
    return again.trimCharactersAtEnd("=")==text;
}
// Big-endian bytes to and from juce::BigInteger (which stores little-endian).
inline juce::BigInteger fromBigEndian(const juce::MemoryBlock& bytes) {
    juce::MemoryBlock reversed(bytes.getSize());
    for(size_t i=0;i<bytes.getSize();++i)reversed[i]=bytes[bytes.getSize()-1-i];
    juce::BigInteger value;value.loadFromMemoryBlock(reversed);return value;
}
inline juce::BigInteger encode(const juce::MemoryBlock& payload,int modulusBytes) {
    juce::MemoryBlock stream;
    for(juce::uint32 counter=0;(int)stream.getSize()<modulusBytes-1;++counter) {
        juce::MemoryBlock input(domain,std::strlen(domain));
        const juce::uint8 be[4]{juce::uint8(counter>>24),juce::uint8(counter>>16),juce::uint8(counter>>8),juce::uint8(counter)};
        input.append(be,4);input.append(payload.getData(),payload.getSize());
        stream.append(juce::SHA256(input.getData(),input.getSize()).getRawData().getData(),32);
    }
    juce::MemoryBlock message(1);message[0]=0;
    message.append(stream.getData(),(size_t)modulusBytes-1);
    return fromBigEndian(message);
}
inline bool validName(const juce::String& name) {
    if(name.isEmpty() || name.length()>maxNameLength || name.trim()!=name)return false;
    for(auto p=name.getCharPointer();!p.isEmpty();++p)if(*p<32)return false;
    return true;
}
} // namespace detail

inline juce::BigInteger publicModulus() {
    juce::BigInteger n;n.parseString(juce::String(publicModulusHex),16);return n;
}

inline Grant verify(juce::String code,const juce::BigInteger& modulus=publicModulus(),int exponent=publicExponent) {
    Grant grant;
    code=code.removeCharacters(" \t\r\n");
    if(code.length()>maxCodeLength || !code.startsWith(prefix) || code.indexOfChar('.')!=code.lastIndexOfChar('.') || !code.containsChar('.'))
        return {false,{},{},{},"Not a Chimera beta tester code."};
    const auto body=code.substring(juce::String(prefix).length());
    juce::MemoryBlock payload,signature;
    if(!detail::decodeBase64Url(body.upToFirstOccurrenceOf(".",false,false),payload)
       || !detail::decodeBase64Url(body.fromFirstOccurrenceOf(".",false,false),signature))
        return {false,{},{},{},"The code is damaged. Copy it again in full."};
    const int modulusBytes=(modulus.getHighestBit()+8)/8;
    const auto s=detail::fromBigEndian(signature);
    auto check=s;check.exponentModulo(juce::BigInteger(exponent),modulus);
    if((int)signature.getSize()!=modulusBytes || s.compare(modulus)>=0 || check!=detail::encode(payload,modulusBytes))
        return {false,{},{},{},"This code is not valid for Chimera."};
    const auto text=juce::String::fromUTF8((const char*)payload.getData(),(int)payload.getSize());
    const auto fields=juce::JSON::parse(text);
    grant.name=fields.getProperty("name",{}).toString();grant.id=fields.getProperty("id",{}).toString();
    grant.issued=fields.getProperty("iat",{}).toString();
    if(!fields.isObject() || (int)fields.getProperty("v",0)!=1 || fields.getProperty("scope",{}).toString()!="beta"
       || !detail::validName(grant.name) || grant.id.isEmpty() || grant.issued.isEmpty())
        return {false,{},{},{},"This code uses an unsupported format."};
    grant.valid=true;
    return grant;
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
    static Grant load(const juce::File& storage=file(),const juce::BigInteger& modulus=publicModulus()) {
        const auto code=read(storage);
        return code.isEmpty() ? Grant{} : verify(code,modulus);
    }
    static Grant save(const juce::String& code,const juce::File& storage=file(),const juce::BigInteger& modulus=publicModulus()) {
        auto grant=verify(code,modulus);
        if(grant.valid && !(storage.getParentDirectory().createDirectory().wasOk() && storage.replaceWithText(code.removeCharacters(" \t\r\n"))))
            grant={false,{},{},{},"The code is valid but could not be saved on this computer."};
        return grant;
    }
    static bool clear(const juce::File& storage=file()) {return !storage.exists() || storage.deleteFile();}
};
} // namespace spectralforge::beta::tester

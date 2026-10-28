#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <vector>
#if __has_include("ChimeraTesterAllowlist.h")
#include "ChimeraTesterAllowlist.h"
#else
inline constexpr const char* chimeraTesterAllowlistDigests[] = { nullptr };
#endif

#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace spectralforge::beta::tester {
// Only pre-issued full-code digests are shipped. No issuance key, HMAC or code
// generator is present in the product. The owner tool issues codes separately.
// Keep approved digests in later beta builds to preserve indefinite beta rights.
// Six Base32 characters carry only 30 bits: hashes permit offline guessing and
// this limited beta-access scheme is NOT strong licensing or anti-copy DRM.
inline constexpr const char* prefix = "CHT1";
inline constexpr const char* domain = "chimera-tester-allowlist-v1";
inline constexpr const char* alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
inline constexpr int minNameLength = 2;
inline constexpr int maxNameLength = 16;
inline constexpr int checkLength = 6;
inline constexpr int maxCodeLength = 64;
inline constexpr int maxPastedLength = 256;
using Allowlist = std::vector<juce::String>;

struct Grant {
    bool valid{};
    juce::String name, error;
    // Retain the verified spelling; never reconstruct/issue a code from a name.
    juce::String canonicalCode;
};

namespace detail {
inline bool validName(const juce::String& name) {
    return name.length()>=minNameLength && name.length()<=maxNameLength
        && name.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
}
inline juce::String canonicalCheck(juce::String text) {
    return text.toUpperCase().replaceCharacter('O','0').replaceCharacter('I','1').replaceCharacter('L','1');
}
inline bool validDigest(const juce::String& value) {
    return value.length()==64 && value.containsOnly("0123456789abcdef");
}
inline juce::String digest(const juce::String& canonicalCode) {
    const auto message=juce::String(domain)+"|"+canonicalCode;
    return juce::SHA256(message.toRawUTF8(),message.getNumBytesAsUTF8()).toHexString();
}
} // namespace detail

inline Allowlist productAllowlist() {
    Allowlist result;
    for(const auto* digest:chimeraTesterAllowlistDigests) {
        if(digest==nullptr)break;
        result.emplace_back(digest);
    }
    return result;
}
inline bool enabled(const Allowlist& allowlist=productAllowlist()) {
    if(allowlist.empty())return false;
    for(const auto& digest:allowlist)if(!detail::validDigest(digest))return false;
    return true;
}

inline Grant verify(juce::String code,const Allowlist& allowlist=productAllowlist()) {
    if(!enabled(allowlist))return {false,{},"Beta tester codes are not enabled in this build."};
    if(code.length()>maxPastedLength)return {false,{},"The pasted code is too long."};
    // Reject non-ASCII before case folding (including Unicode lookalikes).
    for(const auto character:code)if(character>127)return {false,{},"The code contains invalid characters."};
    code=code.removeCharacters(" \t\r\n");
    if(code.length()>maxCodeLength)return {false,{},"The code is too long."};
    const auto parts=juce::StringArray::fromTokens(code,"-",{});
    if(parts.size()!=3 || parts[0].toUpperCase()!=prefix)
        return {false,{},"Not a Chimera beta tester code. It looks like CHT1-NAME-XXXXXX."};
    const auto name=parts[1].toUpperCase();
    const auto check=detail::canonicalCheck(parts[2]);
    if(!detail::validName(name) || check.length()!=checkLength || !check.containsOnly(alphabet))
        return {false,{},"The code is incomplete. Copy it again in full."};
    const auto normalised=juce::String(prefix)+"-"+name+"-"+check;
    const auto digest=detail::digest(normalised);
    bool found=false;
    // Only digest equality is checked. Prefix/name alone convey no entitlement.
    for(const auto& approved:allowlist)found=found || approved==digest;
    if(!found)return {false,{},"This code is not registered for this Chimera build."};
    return {true,name,{},normalised};
}
inline juce::String canonical(const Grant& grant) {
    return grant.valid ? grant.canonicalCode : juce::String{};
}

// Per-user, per-computer storage. Only a verified code is ever written.
struct Store {
    static juce::File file() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SpectralForge/Chimera/beta-tester-code.txt");
    }
    static juce::String read(const juce::File& storage=file()) {
        if(storage.isSymbolicLink() || !storage.existsAsFile())return {};
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
        // Bound the read itself and refuse devices/FIFOs without blocking a DAW
        // during construction. O_NOFOLLOW also closes the final-link race.
        const int fd=::open(storage.getFullPathName().toRawUTF8(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
        if(fd<0)return {};
        struct stat info{};
        if(::fstat(fd,&info)!=0 || !S_ISREG(info.st_mode) || info.st_size>maxCodeLength) {
            ::close(fd);return {};
        }
        juce::MemoryBlock bytes;
        char buffer[maxCodeLength+1];
        while(bytes.getSize()<sizeof(buffer)) {
            const auto count=::read(fd,buffer,sizeof(buffer)-bytes.getSize());
            if(count<0 && errno==EINTR)continue;
            if(count<0){::close(fd);return {};}
            if(count==0)break;
            bytes.append(buffer,static_cast<size_t>(count));
        }
        ::close(fd);
#else
        juce::FileInputStream input(storage);
        if(input.failedToOpen())return {};
        juce::MemoryBlock bytes;
        input.readIntoMemoryBlock(bytes,maxCodeLength+1);
#endif
        if(bytes.getSize()==0)return {};
        if(bytes.getSize()>maxCodeLength || std::memchr(bytes.getData(),0,bytes.getSize())!=nullptr
           || !juce::CharPointer_UTF8::isValidString((const char*)bytes.getData(),(int)bytes.getSize()))return {};
        return juce::String::fromUTF8((const char*)bytes.getData(),(int)bytes.getSize()).trim();
    }
    static Grant load(const juce::File& storage=file(),const Allowlist& allowlist=productAllowlist()) {
        const auto code=read(storage);
        return code.isEmpty() ? Grant{} : verify(code,allowlist);
    }
    static Grant save(const juce::String& code,const juce::File& storage=file(),const Allowlist& allowlist=productAllowlist()) {
        auto grant=verify(code,allowlist);
        if(grant.valid && !(storage.getParentDirectory().createDirectory().wasOk() && writePrivate(storage,canonical(grant))))
            grant={false,{},"The code is valid but could not be saved on this computer."};
        return grant;
    }
    static bool clear(const juce::File& storage=file()) {
        if(storage.isSymbolicLink() || storage.isDirectory())return false;
        return !storage.exists() || storage.deleteFile();
    }
private:
    static bool writePrivate(const juce::File& storage,const juce::String& code) {
        if(storage.isSymbolicLink() || storage.isDirectory())return false;
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
        // Protect bearer credentials from the first byte, including the temporary
        // file. chmod after JUCE's default 0644 write would leave an exposure window.
        juce::TemporaryFile temporary(storage,juce::TemporaryFile::useHiddenFile);
        const auto path=temporary.getFile().getFullPathName();
        const int fd=::open(path.toRawUTF8(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd<0)return false;
        const auto* bytes=code.toRawUTF8();
        size_t left=code.getNumBytesAsUTF8();
        bool ok=true;
        while(left>0) {
            const auto count=::write(fd,bytes,left);
            if(count<0 && errno==EINTR)continue;
            if(count<=0){ok=false;break;}
            bytes+=count;left-=static_cast<size_t>(count);
        }
        if(ok && ::fsync(fd)!=0)ok=false;
        if(::close(fd)!=0)ok=false;
        return ok && ::rename(path.toRawUTF8(),storage.getFullPathName().toRawUTF8())==0;
#else
        // Windows inherits the user's application-data directory DACL.
        // Explicit Windows ACL qualification remains a platform release check.
        return storage.replaceWithText(code);
#endif
    }
};
} // namespace spectralforge::beta::tester

#include "TesterCode.h"
#include "TesterCodeFixtures.h"
#include "BetaLifecycle.h"
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
#include <sys/stat.h>
#endif
#include <iostream>
#include <stdexcept>

namespace {
using namespace spectralforge::beta::tester;
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
juce::MemoryBlock testSecret() {juce::MemoryBlock s;s.loadFromHexString(testercode::testSecretHex);return s;}

void verification() {
    const auto key=testSecret();
    // Same results as Tools/tester_code.py (HMAC-SHA256 + Crockford base32).
    for(const auto* code:{testercode::zielCode,testercode::seoMaryCode,testercode::kloopCode,testercode::shortestCode,testercode::longestCode})
        require(verify(code,key).valid,"valid fixture code rejected");
    const auto ziel=verify(testercode::zielCode,key);
    require(ziel.name=="ZIEL" && canonical(ziel,key)==juce::String(testercode::zielCode),"name or canonical spelling wrong");

    // Typing aids: case, spaces, line breaks, O for 0 and I/L for 1 in the check part.
    const juce::String kloop(testercode::kloopCode);
    require(verify(" cht1-kloop-ebsem0\r\n",key).valid,"lower-case or padded code rejected");
    require(verify(kloop.replace("EM0","EMO"),key).valid,"O typed for 0 rejected");

    // A build without the secret accepts nothing and says why.
    const auto none=verify(testercode::zielCode,juce::MemoryBlock{});
    require(!none.valid && none.error.contains("not enabled"),"build without a secret accepted a code");
    // Fixture codes use a test secret, so the product build never accepts them.
    require(!verify(testercode::zielCode).valid,"test-secret code accepted by this build");

    require(!verify(juce::String::repeatedString(" ",maxPastedLength)+testercode::zielCode,key).valid,"oversized paste accepted");
    // One code does not reveal another: same check on another name, or a changed check, fails.
    require(!verify("CHT1-BARD-FH1VVB",key).valid,"check characters moved to another name accepted");
    const juce::String ziel6(testercode::zielCode);
    for(const char c:{'0','1','A','Z'}) {
        const auto changed=ziel6.dropLastCharacters(1)+juce::String::charToString(c);
        if(changed!=ziel6)require(!verify(changed,key).valid,"changed check character accepted");
    }
    juce::MemoryBlock other(key);other[0]=char(other[0]^1);
    require(!verify(testercode::zielCode,other).valid,"code accepted with a different secret");

    for(const juce::String bad:{juce::String{},juce::String("CHT1"),juce::String("CHT1-ZIEL"),juce::String("CHT1ZIEL"),
                                 juce::String("CHT2-ZIEL-FH1VVB"),juce::String("CHT1-Z-FH1VVB"),juce::String("CHT1-ZIEL-FH1VV"),
                                 juce::String("CHT1-ZIEL-FH1VVBB"),juce::String("CHT1-ZI.EL-FH1VVB"),juce::String("CHT1-ZIEL-FH1VVB-X"),
                                 juce::String("CHT1-XXXXXXXXXXXXXXXXX-0Q5G70"),juce::String::repeatedString("A",5000)})
        require(!verify(bad,key).valid && verify(bad,key).error.isNotEmpty(),"malformed code accepted or unexplained");
}

void storage() {
    const auto directory=juce::File::createTempFile("chimera-tester");
    require(directory.createDirectory().wasOk(),"temp directory");
    const auto file=directory.getChildFile("nested/beta-tester-code.txt");
    const auto key=testSecret();
    require(!Store::load(file,key).valid && Store::read(file).isEmpty(),"empty store reported a code");
    require(!Store::save("CHT1-ZIEL-000000",file,key).valid && !file.exists(),"invalid code written to the store");
    // A valid code is stored in its one canonical spelling.
    const auto saved=Store::save(" cht1-ziel-fh1vvb \n",file,key);
    require(saved.valid && Store::read(file)==juce::String(testercode::zielCode),"valid code not stored canonically");
    require(Store::load(file,key).name=="ZIEL","stored code did not load back");
    require(!Store::load(file,juce::MemoryBlock{}).valid,"stored code unlocked a build without the secret");
    require(!Store::save("CHT1-broken",file,key).valid && Store::load(file,key).valid,"invalid entry replaced a stored code");
    require(file.replaceWithText(juce::String::repeatedString("A",maxCodeLength+10)) && Store::read(file).isEmpty(),"oversized store read");
    require(Store::clear(file) && !file.exists() && Store::clear(file),"clear failed or was not idempotent");
    require(Store::save(testercode::zielCode,file,key).valid,"restore fixture");
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
    struct stat info{};
    require(::stat(file.getFullPathName().toRawUTF8(),&info)==0 && (info.st_mode&0777)==0600,"bearer code is not owner-only");
    require(::chmod(file.getFullPathName().toRawUTF8(),0644)==0,"set old file mode");
    require(Store::save(testercode::zielCode,file,key).valid,"replace old credential");
    require(::stat(file.getFullPathName().toRawUTF8(),&info)==0 && (info.st_mode&0777)==0600,"replacement retained unsafe mode");
    const auto link=directory.getChildFile("linked-code");
    require(::symlink(file.getFullPathName().toRawUTF8(),link.getFullPathName().toRawUTF8())==0,"symlink fixture");
    require(Store::read(link).isEmpty() && !Store::save(testercode::zielCode,link,key).valid && !Store::clear(link),"linked credential followed");
    require(Store::load(file,key).valid,"symlink handling changed target");
    const auto fifo=directory.getChildFile("fifo-code");
    require(::mkfifo(fifo.getFullPathName().toRawUTF8(),0600)==0,"FIFO fixture");
    require(Store::read(fifo).isEmpty(),"FIFO accepted as credential file");
#endif
    // Copying a valid bearer code is intentionally permitted by existing policy.
    const auto other=directory.getChildFile("another-user/beta-tester-code.txt");
    require(Store::save(Store::read(file),other,key).valid,"existing multi-computer right changed");
    using namespace spectralforge::beta;
    constexpr Metadata beta2{"1.3.2-beta.2","c150db48dbd92221723667c06426f12b7220641b",1806505140,1209600,true};
    const auto after=beta2.expiresAtUnix+1;
    InstanceLatch active(beta2,after,Store::load(file,key).valid);
    require(active.processingPermitted(),"saved code did not lift expiry");
    require(Store::clear(file),"remove valid code");
    active.setTesterExtension(Store::load(file,key).valid);
    require(active.processingPermitted() && active.observe(after).status==Status::expired,"removal interrupted existing audio or retained extension");
    InstanceLatch reopened(beta2,after,Store::load(file,key).valid);
    require(!reopened.processingPermitted(),"new instance after removal bypassed expiry");
    require(Store::load(other,key).valid,"local removal unexpectedly revoked a copied code");
    const auto blocked=directory.getChildFile("blocked-code");
    require(blocked.createDirectory().wasOk(),"blocked fixture");
    require(!Store::clear(blocked),"directory reported as successful credential removal");
    require(file.replaceWithText(juce::String(testercode::zielCode)+juce::String::charToString(1)),"corrupt fixture");
    require(!Store::load(file,key).valid,"corrupt store unlocked beta");
    directory.deleteRecursively();
}
}

int main(int argc,char** argv) {
    try {
        if(argc==4 && juce::String(argv[1])=="--verify-test-code") {
            juce::MemoryBlock key;key.loadFromHexString(argv[2]);
            return verify(juce::File(argv[3]).loadFileAsString(),key).valid ? 0 : 1;
        }
        require(argc==1,"unsupported test arguments");
        verification();storage();
        std::cout<<"PASS: beta tester codes: HMAC check, typing aids, secret separation, tampering, malformed input, storage\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

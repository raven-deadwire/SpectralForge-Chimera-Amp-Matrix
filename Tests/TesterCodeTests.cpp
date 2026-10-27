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
Allowlist testAllowlist() { return testercode::allowlist(); }

void verification() {
    const auto key=testAllowlist();
    // Fixed dummy codes and full-code SHA-256 digests match the owner tool.
    for(const auto* code:{testercode::alphaCode,testercode::betaCode,testercode::gammaCode,testercode::shortestCode,testercode::longestCode})
        require(verify(code,key).valid,"valid fixture code rejected");
    const auto alpha=verify(testercode::alphaCode,key);
    require(alpha.name=="ALPHA" && canonical(alpha)==juce::String(testercode::alphaCode),"name or canonical spelling wrong");

    // Typing aids: case, spaces, line breaks, O for 0 and I/L for 1 in the check part.
    const juce::String gamma(testercode::gammaCode);
    require(verify(" cht1-gamma-6g7h10\r\n",key).valid,"lower-case or padded code rejected");
    require(verify(gamma.replace("H10","HLO"),key).valid,"O typed for 0 rejected");

    // A build without an allowlist accepts nothing and says why.
    const auto none=verify(testercode::alphaCode,Allowlist{});
    require(!none.valid && none.error.contains("not enabled"),"build without an allowlist accepted a code");
    // Fixture codes use a test allowlist, so the product build never accepts them.
    require(!verify(testercode::alphaCode).valid,"fixture code accepted by this build");

    require(!verify("CHT1-ZIEL-FH1VVB",key).valid,"legacy HMAC fixture accepted without registration");
    require(!verify("CHT1-BETA-0A1B2C",key).valid,"name binding failed");
    require(!verify("CHT1-ALPHA-０A1B2C",key).valid,"non-ASCII check accepted");
    require(!verify("CHT1-ALPHA-0A1B2C",Allowlist{key[0],"malformed"}).valid,"malformed allowlist not fail-closed");
    require(!verify(juce::String::repeatedString(" ",maxPastedLength)+testercode::alphaCode,key).valid,"oversized paste accepted");
    // One code does not reveal another: same check on another name, or a changed check, fails.
    require(!verify("CHT1-BARD-FH1VVB",key).valid,"check characters moved to another name accepted");
    const juce::String alpha6(testercode::alphaCode);
    for(const char c:{'0','1','A','Z'}) {
        const auto changed=alpha6.dropLastCharacters(1)+juce::String::charToString(c);
        if(changed!=alpha6)require(!verify(changed,key).valid,"changed check character accepted");
    }
    Allowlist other{juce::String::repeatedString("a",64)};
    require(!verify(testercode::alphaCode,other).valid,"code accepted with a different allowlist");

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
    const auto key=testAllowlist();
    require(!Store::load(file,key).valid && Store::read(file).isEmpty(),"empty store reported a code");
    require(!Store::save("CHT1-ZIEL-000000",file,key).valid && !file.exists(),"invalid code written to the store");
    // A valid code is stored in its one canonical spelling.
    const auto saved=Store::save(" cht1-alpha-0a1b2c \n",file,key);
    require(saved.valid && Store::read(file)==juce::String(testercode::alphaCode),"valid code not stored canonically");
    require(Store::load(file,key).name=="ALPHA","stored code did not load back");
    require(!Store::load(file,Allowlist{}).valid,"stored code unlocked a build without an allowlist");
    require(!Store::save("CHT1-broken",file,key).valid && Store::load(file,key).valid,"invalid entry replaced a stored code");
    require(file.replaceWithText(juce::String::repeatedString("A",maxCodeLength+10)) && Store::read(file).isEmpty(),"oversized store read");
    require(Store::clear(file) && !file.exists() && Store::clear(file),"clear failed or was not idempotent");
    require(Store::save(testercode::alphaCode,file,key).valid,"restore fixture");
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
    struct stat info{};
    require(::stat(file.getFullPathName().toRawUTF8(),&info)==0 && (info.st_mode&0777)==0600,"bearer code is not owner-only");
    require(::chmod(file.getFullPathName().toRawUTF8(),0644)==0,"set old file mode");
    require(Store::save(testercode::alphaCode,file,key).valid,"replace old credential");
    require(::stat(file.getFullPathName().toRawUTF8(),&info)==0 && (info.st_mode&0777)==0600,"replacement retained unsafe mode");
    const auto link=directory.getChildFile("linked-code");
    require(::symlink(file.getFullPathName().toRawUTF8(),link.getFullPathName().toRawUTF8())==0,"symlink fixture");
    require(Store::read(link).isEmpty() && !Store::save(testercode::alphaCode,link,key).valid && !Store::clear(link),"linked credential followed");
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
    require(file.replaceWithText(juce::String(testercode::alphaCode)+juce::String::charToString(1)),"corrupt fixture");
    require(!Store::load(file,key).valid,"corrupt store unlocked beta");
    directory.deleteRecursively();
}

// Shared by public fixtures and a private, file-only owner audit. Never print
// the supplied code/name or persist it in a repository, evidence file or log.
void migrationLifecycle(const juce::String& code, const Allowlist& key) {
    const auto grant=verify(code,key);
    require(grant.valid,"migration verification failed");
    const auto normal=canonical(grant);
    require(verify(" \t"+normal.toLowerCase()+"\r\n",key).valid,"migration typing aids failed");
    const auto separator=normal.lastIndexOfChar('-');
    const auto start=normal.substring(0,separator+1);
    const auto check=normal.substring(separator+1);
    for(const char one:{'I','L'})
        require(verify(start+check.replaceCharacter('0','O').replaceCharacter('1',one),key).valid,
                "migration check aliases failed");
    require(!verify(normal,Allowlist{}).valid,"empty list accepted migration");
    auto renamed=normal.replaceSection(5,1,normal[5]=='A' ? "B" : "A");
    require(!verify(renamed,key).valid,"unapproved name accepted");
    for(int position=0;position<checkLength;++position) {
        const auto changed=check.replaceSection(position,1,check[position]=='0' ? "1" : "0");
        require(!verify(start+changed,key).valid,"unapproved check mutation accepted");
    }
    const auto directory=juce::File::createTempFile("chimera-migration");
    require(directory.createDirectory().wasOk(),"migration private directory failed");
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
    require(::chmod(directory.getFullPathName().toRawUTF8(),0700)==0,"migration directory permissions failed");
#endif
    struct Cleanup {juce::File directory; ~Cleanup(){directory.deleteRecursively();}} cleanup{directory};
    const auto file=directory.getChildFile("code.txt");
    require(Store::save(code,file,key).valid,"migration save failed");
    require(!Store::save(renamed,file,key).valid && Store::load(file,key).valid,
            "unapproved code replaced stored grant");
    using namespace spectralforge::beta;
    // Synthetic future metadata tests version independence, not release policy.
    constexpr Metadata versions[]{
        {"1.3.2-beta.2","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",1806505140,1209600,true},
        {"9.0.0-beta.1","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",2100000000,1209600,true}};
    for(const auto& version:versions) {
        const auto loaded=Store::load(file,key);
        InstanceLatch reopened(version,version.expiresAtUnix+1,loaded.valid);
        require(reopened.processingPermitted() && reopened.observe(version.expiresAtUnix+1).status==Status::testerExtended,
                "retained digest lost rights after beta update");
    }
    const auto later=versions[1];
    const auto after=later.expiresAtUnix+1;
    InstanceLatch active(later,after,Store::load(file,key).valid);
    const auto copy=directory.getChildFile("other-computer.txt");
    require(Store::save(code,copy,key).valid,"existing copy policy changed");
    const auto blocked=directory.getChildFile("not-a-credential-file");
    require(blocked.createDirectory().wasOk() && !Store::clear(blocked),"failed removal reported success");
    require(Store::load(file,key).valid,"failed removal changed valid storage");
    require(Store::clear(file) && Store::clear(file),"migration removal failed");
    active.setTesterExtension(Store::load(file,key).valid);
    require(active.processingPermitted() && active.observe(after).status==Status::expired,
            "removal interrupted running instance or retained grant");
    InstanceLatch reopened(later,after,Store::load(file,key).valid);
    require(!reopened.processingPermitted(),"removed code survived restart");
    require(Store::load(copy,key).valid,"local removal unexpectedly revoked copied code");
    const Allowlist revoked{juce::String::repeatedString("a",64)};
    require(!Store::load(copy,revoked).valid,"removed digest accepted by later build");
    InstanceLatch revokedInstance(later,after,Store::load(copy,revoked).valid);
    require(!revokedInstance.processingPermitted(),"revoked digest bypassed expiry");
    require(Store::load(copy,key).valid,"new list modified old offline build rights");
    auto invalid=later; invalid.sourceRevision="invalid";
    InstanceLatch badBuild(invalid,after,Store::load(copy,key).valid);
    require(!badBuild.processingPermitted() && badBuild.observe(after).status==Status::configurationError,
            "migration bypassed metadata blocking");
}
}

int main(int argc,char** argv) {
    try {
        if(argc==4 && juce::String(argv[1])=="--audit-migration-code") {
            const juce::File file(argv[3]);
            require(!file.isSymbolicLink() && file.existsAsFile() && file.getSize()>0
                    && file.getSize()<=maxPastedLength,"invalid private audit input");
            migrationLifecycle(file.loadFileAsString(),Allowlist{juce::String(argv[2])});
            std::cout<<"PASS: private migration lifecycle\n";
            return 0;
        }
        if(argc==4 && juce::String(argv[1])=="--verify-test-code") {
            Allowlist key{juce::String(argv[2])};
            return verify(juce::File(argv[3]).loadFileAsString(),key).valid ? 0 : 1;
        }
        require(argc==1,"unsupported test arguments");
        verification();storage();
        for(const auto* code:{testercode::alphaCode,testercode::betaCode,testercode::gammaCode,
                              testercode::shortestCode,testercode::longestCode})
            migrationLifecycle(code,testAllowlist());
        std::cout<<"PASS: beta tester codes: digest allowlist, typing aids, no issuance secret, tampering, malformed input, storage\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

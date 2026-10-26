#include "TesterCode.h"
#include "TesterCodeFixtures.h"
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
    directory.deleteRecursively();
}
}

int main() {
    try {
        verification();storage();
        std::cout<<"PASS: beta tester codes: HMAC check, typing aids, secret separation, tampering, malformed input, storage\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

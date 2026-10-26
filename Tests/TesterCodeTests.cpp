#include "TesterCode.h"
#include "TesterCodeFixtures.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace spectralforge::beta::tester;
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
juce::BigInteger testModulus() {juce::BigInteger n;n.parseString(testercode::testModulusHex,16);return n;}

void verification() {
    const auto key=testModulus();
    const auto ascii=verify(testercode::asciiCode,key);
    require(ascii.valid && ascii.name=="Test Tester" && ascii.id=="0a1b2c3d" && ascii.issued=="2026-10-26","valid ASCII code rejected");
    const auto korean=verify(testercode::koreanCode,key);
    require(korean.valid && korean.name==juce::String::fromUTF8(u8"베타 테스터"),"valid non-ASCII name rejected");
    // Pasting may add spaces or line breaks.
    const juce::String code(testercode::asciiCode);
    require(verify(" "+code.substring(0,40)+"\n"+code.substring(40)+"\r\n",key).valid,"whitespace in a pasted code rejected");

    // Fixture codes are signed with a test key, so the product key rejects them.
    require(publicModulus().getHighestBit()==2047,"product public key is not RSA-2048");
    require(!verify(code).valid,"test-key code accepted by the product key");

    const auto dot=code.indexOfChar('.');
    const auto flip=[](juce::String text,int index){
        const auto c=text[index];text=text.replaceSection(index,1,juce::String::charToString(c=='A'?'B':'A'));return text;};
    require(!verify(flip(code,10),key).valid,"tampered payload accepted");
    require(!verify(flip(code,dot+5),key).valid,"tampered signature accepted");
    // Unused trailing bits: one spelling per code.
    for(const char c:{'A','B','C','D'}) {
        const auto changed=code.dropLastCharacters(1)+juce::String::charToString(c);
        if(changed!=code)require(!verify(changed,key).valid,"non-canonical signature spelling accepted");
    }
    const auto otherPayload=juce::String(testercode::koreanCode).upToFirstOccurrenceOf(".",false,false);
    require(!verify(otherPayload+code.fromFirstOccurrenceOf(".",true,false),key).valid,"signature moved to another payload accepted");
    for(const juce::String bad:{juce::String{},juce::String("CHT1-"),juce::String("CHT1-abc"),juce::String("CHT1-a.b.c"),
                                 juce::String("XYZ1-")+code.substring(5),code.replace(".","+."),code+code,juce::String::repeatedString("A",5000)})
        require(!verify(bad,key).valid && verify(bad,key).error.isNotEmpty(),"malformed code accepted or unexplained");
}

void storage() {
    const auto directory=juce::File::createTempFile("chimera-tester");
    require(directory.createDirectory().wasOk(),"temp directory");
    const auto file=directory.getChildFile("nested/beta-tester-code.txt");
    require(!Store::load(file).valid && Store::read(file).isEmpty(),"empty store reported a code");
    // Only a code verified against the product key may be written.
    require(!Store::save(testercode::asciiCode,file).valid && !file.exists(),"invalid code written to the store");
    require(file.getParentDirectory().createDirectory().wasOk() && file.replaceWithText(testercode::asciiCode),"write fixture");
    require(Store::read(file)==juce::String(testercode::asciiCode) && !Store::load(file).valid,"stored test-key code unlocked the product");
    require(file.replaceWithText(juce::String::repeatedString("A",maxCodeLength+10)) && Store::read(file).isEmpty(),"oversized store read");
    require(Store::clear(file) && !file.exists() && Store::clear(file),"clear failed or was not idempotent");
    // Happy path with the test key standing in for the product key.
    const auto key=testModulus();
    const auto saved=Store::save(" "+juce::String(testercode::koreanCode)+"\n",file,key);
    require(saved.valid && Store::read(file)==juce::String(testercode::koreanCode),"valid code not stored without whitespace");
    const auto loaded=Store::load(file,key);
    require(loaded.valid && loaded.name==saved.name && loaded.id==saved.id,"stored code did not load back");
    require(!Store::save("CHT1-broken.code",file,key).valid && Store::load(file,key).valid,"invalid entry replaced a stored code");
    directory.deleteRecursively();
}
}

int main() {
    try {
        verification();storage();
        std::cout<<"PASS: beta tester codes: signature, product/test key separation, tampering, canonical spelling, malformed input, storage\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

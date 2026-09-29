#include "BoardState.h"
#include <functional>
#include <iostream>
#include <fstream>
using namespace spectralforge::preparation;
int cases = 0;
void check(bool value) { if (!value) throw std::runtime_error("Check failed"); }
template<class Fn> void rejects(Fn fn) { bool failed = false; try { fn(); } catch (const std::invalid_argument&) { failed = true; } check(failed); }
void test(const char* name, const std::function<void()>& fn) { fn(); ++cases; std::cout << "PASS " << name << '\n'; }
int main(int argc, char** argv) {
 if (argc == 3 && std::string(argv[1]) == "--emit") {
    Board b; auto id=b.add("planned.sd1",{{"drive",.12345678912345678},{"tone",.7},{"level",.4}},true);
    b.bind(id,74,"tone"); b.add("planned.distortion-plus",{{"distortion",.6},{"output",.4}});
    std::ofstream out(argv[2]); out << encode(b.state()); return out ? 0 : 1;
 }
 if (argc == 3 && std::string(argv[1]) == "--read") {
    std::ifstream file(argv[2]); if(!file) return 1;
    std::ostringstream text; text << file.rdbuf();
    try { const auto s=decode(text.str()); check(decode(encode(s))==s); std::cout << "PASS cross-language state import\n"; return 0; }
    catch(const std::exception& e) {std::cerr<<e.what();return 1;}
 }

 try {
  test("five-slot hard limit and bypass still consumes a slot", [] {
    Board b; for (int i=0;i<5;++i) b.add("sd1", {{"drive",.5}});
    b.bypass(b.state().effects[0].id,true); const auto before=encode(b.state());
    rejects([&]{b.add("sixth");}); check(encode(b.state())==before);
    rejects([&]{b.duplicate(b.state().effects[0].id);}); check(b.state().effects.size()==5);
  });
  test("repeated model has independent state", [] {
    Board b; auto a=b.add("sd1",{{"drive",.2}}); auto c=b.duplicate(a);
    b.set(c,"drive",.8); check(a!=c && b.state().effects[0].parameters.at("drive")==.2);
  });
  test("movement preserves id, values and MIDI", [] {
    Board b; auto a=b.add("sd1",{{"tone",.3}}); b.bind(a,17,"tone"); b.add("eq");
    b.move(a,1); const auto& e=b.state().effects[1]; check(e.id==a && e.midi.at(17)=="tone" && e.parameters.at("tone")==.3);
  });
  test("replacement gets new identity and no stale binding", [] {
    Board b; auto a=b.add("sd1",{{"tone",.3}}); b.bind(a,17,"tone");
    auto c=b.replace(a,"distortion-plus",{{"distortion",.7}});
    check(a!=c && b.state().effects[0].midi.empty()); rejects([&]{b.set(a,"tone",.9);});
  });
  test("duplicate copies bypass but clears MIDI in one undo", [] {
    Board b; auto a=b.add("sd1",{{"drive",.2}}); b.bypass(a,true); b.bind(a,3,"drive"); b.duplicate(a);
    check(b.state().effects[1].bypass && b.state().effects[1].midi.empty()); b.undo(); check(b.state().effects.size()==1);
  });
  test("LOW membership change disclosed on crossing move", [] {
    Board b; auto a=b.add("comp",{},true); b.add("fuzz");
    check(b.move(a,1)); check(b.state().lowTap==1 && b.state().effects[1].id==a);
  });
  test("shared removal adjusts tap", [] {
    Board b; auto a=b.add("comp",{},true); b.add("filter",{},true); b.add("fuzz"); b.remove(a);
    check(b.state().lowTap==1); b.setTap(0); check(b.state().effects.size()==2);
  });
  test("invalid tap, position, key and nonfinite edits are transactional", [] {
    Board b; auto a=b.add("sd1",{{"drive",.3}}); const auto before=encode(b.state());
    rejects([&]{b.setTap(2);}); rejects([&]{b.move(a,2);}); rejects([&]{b.set(a,"unknown",.8);});
    rejects([&]{b.set(a,"drive",std::numeric_limits<double>::infinity());});
    rejects([&]{b.bind(a,128,"drive");}); check(encode(b.state())==before);
  });
  test("empty board supports all positions of tap", [] { Board b; b.setTap(0); check(!b.undo()); rejects([&]{b.setTap(1);}); });
  test("undo redo restores parameters without id reuse", [] {
    Board b; auto a=b.add("sd1",{{"drive",.2}}); auto c=b.add("eq"); b.undo(); auto d=b.add("wah");
    check(d>c && c>a && !b.redo()); rejects([&]{b.set(d,"missing",0);});
  });
  test("roundtrip precision, strings and bindings", [] {
    Board b; auto a=b.add("model \"name\"",{{"gain",.12345678912345678}},true); b.bind(a,74,"gain");
    check(decode(encode(b.state()))==b.state());
  });
  test("malformed load never changes board", [] {
    Board b; b.add("comp"); const auto before=encode(b.state());
    for (const auto& data : {std::string("bad"), before+"garbage", std::string("CHIMERA_BOARD_PREP 1\n9 0 \"legacy-v1\" 6\n")})
        rejects([&]{b.load(data);});
    check(encode(b.state())==before);
  });
  test("invalid snapshots rejected", [] {
    Board b; b.add("sd1"); auto s=b.state(); s.effects.push_back(s.effects[0]); rejects([&]{validate(s);});
    s=b.state(); s.version=2; rejects([&]{validate(s);});
    s=b.state(); s.nextId=1; rejects([&]{validate(s);});
    s=b.state(); s.effects[0].model=""; rejects([&]{validate(s);});
  });
  test("same-session import cannot rewind allocator", [] {
    Board b; b.add("comp"); const auto old=encode(b.state()); auto previous=b.add("fuzz"); b.load(old);
    check(b.add("eq")>previous);
  });
  test("legacy four order combinations and raw values retained", [] {
    for (bool envelope : {false,true}) for(bool boost : {false,true}) {
        LegacyPre old; old.envelopeFirst=envelope; old.boostAfterDrive=boost;
        for(int i=0;i<5;++i) {old.models[i]=i; old.parameters[i]={{"raw",double(i)}}; old.bypass[i]=(i%2==0);}
        auto s=migrate(old); check(s.lowTap==2 && s.effects.size()==5 && s.engine=="legacy-v1");
        check(s.effects[0].model==(envelope ? "legacy.filter.1":"legacy.comp.0"));
        check(s.effects[4].model==(boost ? "legacy.boost.3":"legacy.drive.4"));
        for(const auto& e:s.effects) check(e.bypass==(int(e.parameters.at("raw"))%2==0));
        check(decode(encode(s))==s);
    }
  });
  test("invalid legacy index rejected", [] {LegacyPre old; old.models[2]=5; rejects([&]{migrate(old);});});
  test("delete and undo restores MIDI", [] {
    Board b; auto a=b.add("sd1",{{"tone",.2}}); b.bind(a,10,"tone"); b.remove(a); b.undo();
    check(b.state().effects[0].midi.at(10)=="tone"); b.redo(); check(b.state().effects.empty());
  });
  test("maximum text size bounded", [] {rejects([]{decode(std::string(262145,' '));});});
  test("wrong-side insertion rejected", [] {Board b; b.add("comp",{},true); rejects([&]{b.insert("fuzz",0,{},false);});});
  test("JB-2 consumes exactly one slot", [] {
    Board b; b.add("jb2",{{"boss.drive",.2},{"jhs.drive",.5},{"mode",4}});
    for(int i=0;i<4;++i) { b.add("sd1"); }
    check(b.state().effects.size()==5);
  });
  std::cout << cases << " native preparation cases passed\n";
 } catch (const std::exception& e) {
  std::cerr << "FAIL after " << cases << " cases: " << e.what() << '\n'; return 1;
 }
 return 0;
}

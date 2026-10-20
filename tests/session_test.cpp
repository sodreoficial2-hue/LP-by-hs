#include "session.hpp"
#include <iostream>
#include <limits>
void check(bool ok) { if(!ok) throw std::runtime_error("Regression failed"); }
int main(int argc,char** argv) {
 const auto path=std::filesystem::temp_directory_path()/"lphs-regression.vshow";
 lp::Session s; s.favorites[0]=lp::Patch{}; s.favorites[0]->sfz="Piano com espaços.sfz";
 s.favorites[19]=lp::Patch{}; s.favorites[19]->sfz="Strings.sfz";
 check(s.programChange(0)); s.parameter(6,0.17f);
 check(s.programChange(19)); check(s.programChange(0));
 check(std::abs(s.favorites[0]->parameters[6]-0.17f)<0.0001f);
 check(!s.programChange(20)); check(!s.programChange(1));
 s.favorites[0]->cc[6]=91; s.favorites[19]->button={0xc0,3,19}; s.save(path);
 // A failed save must preserve the previous valid show.
 s.favorites[0]->parameters[0]=std::numeric_limits<float>::quiet_NaN();
 bool badSave=false; try{s.save(path);}catch(...){badSave=true;}check(badSave);
 s.favorites[0]->parameters[0]=1;
 lp::Session restored; restored.load(path);
 check(restored.favorites[0]->sfz==s.favorites[0]->sfz);
 check(restored.favorites[19]->button.kind==0xc0 && restored.favorites[19]->button.channel==3 && restored.favorites[19]->button.number==19);
 check(restored.favorites[0]->cc[6]==91); check(restored.favorites[19].has_value());
 // Read old format without button fields.
 {std::ofstream legacy(path);legacy << "LPHS_PROFILE 1\n0 \"piano.sfz\" 1 0 0 1 0 1 0 0 0 0 0 0.5 0.5 0.5";for(int i=0;i<14;++i)legacy << " -1";legacy << "\n";}
 lp::Session old;old.load(path);check(old.favorites[0]->button.kind==0);
 s.favorites[19]->button={0xc0,0,0};
 bool wrong=false;try{s.save(path);}catch(...){wrong=true;}check(wrong);
 // Migration removes old note/CC favorite triggers while preserving knobs.
 for(int kind:{0x90,0xb0}){
  std::ofstream legacy(path);legacy << "LPHS_PROFILE 2\n0 \"piano.sfz\" 1 0 0 1 0 1 0.17 0 0 0 0 0.5 0.5 0.5";for(int i=0;i<14;++i)legacy << " -1";legacy << ' ' << kind << " 3 42\n";legacy.close();
  old.load(path);check(old.favorites[0]->button.kind==0&&std::abs(old.favorites[0]->parameters[6]-.17f)<.0001f);
 }
 { std::ofstream bad(path); bad << "LPHS_PROFILE 1\n20 broken"; }
 bool rejected=false; try {restored.load(path);} catch(...) {rejected=true;}
 check(rejected && restored.favorites[19].has_value());
 std::filesystem::remove(path);
 if(argc>1) check(lp::catalog(argv[1]).size()==3);
 std::cout << "PASS: favorite recall, PC 1-20, profile roundtrip, corrupt profile preservation, SFZ catalog\n";
}

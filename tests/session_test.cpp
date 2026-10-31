#include "performance.hpp"
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
 s.parameter(1,.3f);check(s.favorites[0]->adsrEnabled);
 s.favorites[0]->cc[6]=91; s.favorites[19]->button={0xc0,3,19}; s.save(path);
 // A failed save must preserve the previous valid show.
 s.favorites[0]->parameters[0]=std::numeric_limits<float>::quiet_NaN();
 bool badSave=false; try{s.save(path);}catch(...){badSave=true;}check(badSave);
 s.favorites[0]->parameters[0]=1;
 lp::Session restored; restored.load(path);
 check(restored.favorites[0]->sfz==s.favorites[0]->sfz);check(restored.favorites[0]->adsrEnabled&&restored.favorites[0]->parameters[1]==.3f);check(!restored.favorites[19]->adsrEnabled);
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
 // Per-favorite Performance volumes survive repeated recalls and profile writes.
 lp::Performance performance;performance.name="Split palco";performance.parts[0].enabled=true;performance.parts[0].instrument="Piano.sfz";performance.parts[0].patch.parameters[1]=.31f;performance.parts[0].patch.adsrEnabled=true;
 performance.parts[1].enabled=true;performance.parts[1].instrument="Strings.sfz";performance.parts[1].patch.parameters[6]=.62f;
 lp::Session mixShow;mixShow.performanceLibrary.push_back(lp::savePerformance(performance));mixShow.favorites[3]=lp::Patch{};mixShow.favorites[3]->sfz="performance://0";
 for(float volume:{.27f,.84f,.19f}){
  performance.parts[0].patch.parameters[0]=volume;performance.parts[1].patch.parameters[0]=1-volume;performance.parts[0].patch.cc[0]=21;
  mixShow.favorites[3]->performanceMix=lp::performanceMix(performance);
  auto recall=lp::restorePerformance(mixShow.performanceLibrary[0]);lp::applyPerformanceMix(recall,*mixShow.favorites[3]->performanceMix);
  check(recall.parts[0].patch.parameters[0]==volume&&recall.parts[1].patch.parameters[0]==1-volume);
  check(recall.parts[0].patch.parameters[1]==.31f&&recall.parts[0].patch.adsrEnabled&&recall.parts[1].patch.parameters[6]==.62f);
  check(mixShow.performanceLibrary[0].parts[0].patch.parameters[0]==1); // base untouched by live mix
  mixShow.save(path);lp::Session copy;copy.load(path);check(copy.favorites[3]->performanceMix->volumes[0]==volume&&copy.favorites[3]->performanceMix->cc[0]==21);
 }
 // Previous v7 profiles continue to open without a mix override.
 mixShow.favorites[3]->performanceMix.reset();mixShow.save(path);
 {std::ifstream f(path);std::ostringstream legacy;legacy<<"LPHS_PROFILE 7\n";std::string line;std::getline(f,line);
  while(std::getline(f,line)){std::istringstream record(line);std::string tag;record>>tag;
   if(tag=="PART"||(!tag.empty()&&std::isdigit(tag[0]))){
    std::vector<std::string> fields;std::string token;while(record>>std::quoted(token))fields.push_back(token);
    const int prefix=tag=="PART"?7:1;fields.erase(fields.begin()+prefix+16+15);fields.erase(fields.begin()+prefix+15);
    legacy<<tag;for(std::size_t j=0;j<fields.size();++j){legacy<<' ';if(j==static_cast<std::size_t>(tag=="PART"?2:0))legacy<<std::quoted(fields[j]);else legacy<<fields[j];}legacy<<'\n';
   }else legacy<<line<<'\n';
  }std::ofstream(path)<<legacy.str();}
 lp::Session v7;v7.load(path);check(!v7.favorites[3]->performanceMix&&v7.performanceLibrary[0].parts[0].patch.parameters[1]==.31f);
 for(const auto& record:{"MIX 3 2 1 1 1 -1 -1 -1 -1", "MIX 3 .5 .5 .5 .5 64 -1 -1 -1", "MIX 19 .5 .5 .5 .5 -1 -1 -1 -1"}){
  mixShow.save(path);std::ofstream(path,std::ios::app)<<record<<'\n';bool rejectedMix=false;try{v7.load(path);}catch(...){rejectedMix=true;}check(rejectedMix&&v7.performanceLibrary.size()==1);
 }
 lp::Session phaser;phaser.master=.62f;phaser.masterCC=11;phaser.favorites[0]=lp::Patch{};phaser.favorites[0]->sfz="piano.sfz";phaser.favorites[0]->parameters[15]=.73f;phaser.favorites[0]->cc[15]=27;
 performance.parts[0].patch.parameters[15]=.41f;phaser.performanceLibrary.push_back(lp::savePerformance(performance));phaser.save(path);lp::Session reopened;reopened.load(path);
 check(reopened.favorites[0]->parameters[15]==.73f&&reopened.favorites[0]->cc[15]==27&&reopened.performanceLibrary[0].parts[0].patch.parameters[15]==.41f&&reopened.master==.62f&&reopened.masterCC==11);
 check(v7.favorites[3]->parameters[15]==0&&v7.favorites[3]->cc[15]==-1);
 std::filesystem::remove(path);
 if(argc>1) check(lp::catalog(argv[1]).size()==3);
 std::cout << "PASS: favorite recall, PC 1-20, profile roundtrip, corrupt profile preservation, SFZ catalog\n";
}

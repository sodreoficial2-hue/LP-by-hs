#pragma once
#include "atomic_file.hpp"
#include <array>
#include <sstream>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace lp {
// Reimplementation based on recovered behavior; not original source.
inline constexpr std::array<const char*,14> parameterNames = {
 "Volume", "Attack", "Decay", "Sustain", "Release", "Envelope Filter", "Reverb",
 "Delay", "Chorus", "Tremolo", "Compressor", "Bass", "Mid", "Treble"};
struct ButtonBinding {
 int kind=0; // 0 unassigned, 0xc0 Program Change only
 int channel=0,number=0;
 bool valid() const {return (kind==0||kind==0xc0)&&channel>=0&&channel<16&&number>=0&&number<128;}
 bool same(const ButtonBinding& b) const {return kind!=0&&kind==b.kind&&channel==b.channel&&number==b.number;}
};
struct Patch {
 bool adsrEnabled=false;
 ButtonBinding button;
 std::string sfz;
 std::array<float,14> parameters{1,0,0,1,0,1,0,0,0,0,0,0.5f,0.5f,0.5f};
 std::array<int,14> cc{};
 Patch() { cc.fill(-1); }
};
struct SavedPerformancePart {bool enabled=false;std::string instrument;int low=0,high=127,transpose=0;Patch patch{};};
struct SavedPerformance {std::string name;bool split=false;std::array<SavedPerformancePart,4> parts{};};
struct Session {
 std::array<std::optional<SavedPerformance>,20> performances{};
 std::array<std::optional<Patch>,20> favorites;
 std::optional<std::size_t> current;
 // MIDI wire values 0..19 correspond to controller labels 1..20.
 bool programChange(unsigned wireValue) {
  if(wireValue>=favorites.size() || !favorites[wireValue]) return false;
  current=wireValue; return true;
 }
 void parameter(std::size_t index,float value) {
  if(!current || !favorites[*current]) throw std::logic_error("No active favorite");
  if(index>=parameterNames.size() || !std::isfinite(value) || value<0 || value>1)
   throw std::invalid_argument("Invalid parameter");
  favorites[*current]->parameters[index]=value;
  if(index>=1&&index<=4)favorites[*current]->adsrEnabled=true;
 }
 void save(const std::filesystem::path& path) const {
  // Caller chooses path; does not auto-save shows.
  std::ostringstream f;
  f << "LPHS_PROFILE 5\n" << std::setprecision(9);
  for(std::size_t i=0;i<favorites.size();++i) if(favorites[i]) {
   auto& p=*favorites[i];
   if(!p.button.valid()||(p.button.kind!=0&&p.button.number!=static_cast<int>(i))) throw std::runtime_error("Invalid MIDI button");
   if(p.sfz.empty() || p.sfz.size()>32768) throw std::runtime_error("Invalid SFZ path");
   for(auto value:p.parameters) if(!std::isfinite(value) || value<0 || value>1) throw std::runtime_error("Invalid parameter value");
   for(auto cc:p.cc) if(cc < -1 || cc >127) throw std::runtime_error("Invalid MIDI CC");
   f << i << ' ' << std::quoted(p.sfz);
   for(auto v:p.parameters) f << ' ' << v;
   for(auto v:p.cc) f << ' ' << v;
   f << ' ' << p.button.kind << ' ' << p.button.channel << ' ' << p.button.number << ' ' << (p.adsrEnabled?1:0);
   f << '\n';
  }
  for(std::size_t a=0;a<favorites.size();++a)if(favorites[a])
   for(std::size_t b=a+1;b<favorites.size();++b)if(favorites[b]&&favorites[a]->button.same(favorites[b]->button))
    throw std::runtime_error("Duplicate MIDI button");
  for(std::size_t slot=0;slot<20;++slot)if(performances[slot]){
   const auto& performance=*performances[slot];
   if(!favorites[slot]||performance.name.empty()||performance.name.size()>128)throw std::runtime_error("Invalid saved Performance");
   f<<"PERFORMANCE "<<slot<<' '<<std::quoted(performance.name)<<' '<<(performance.split?1:0)<<'\n';
   int active=0;
   for(std::size_t part=0;part<4;++part){
    const auto& p=performance.parts[part];
    if(!p.enabled)continue;
    ++active;
    if(p.instrument.empty()||p.instrument.size()>32768||p.low<0||p.high>127||p.low>p.high||p.transpose< -48||p.transpose>48)throw std::runtime_error("Invalid Performance zone");
    f<<"PART "<<slot<<' '<<part<<' '<<std::quoted(p.instrument)<<' '<<p.low<<' '<<p.high<<' '<<p.transpose<<' '<<(p.patch.adsrEnabled?1:0);
    for(float v:p.patch.parameters){if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid Performance parameter");f<<' '<<v;}
    for(int cc:p.patch.cc){if(cc< -1||cc>127)throw std::runtime_error("Invalid Performance MIDI CC");f<<' '<<cc;}
    f<<'\n';
   }
   if(active==0)throw std::runtime_error("Empty Performance");
  }
  atomicWrite(path,f.str());
 }
 void load(const std::filesystem::path& path) {
  std::ifstream f(path,std::ios::binary); std::string magic; int version=0;
  if(!(f>>magic>>version) || magic!="LPHS_PROFILE" || (version<1||version>5))
   throw std::runtime_error("Invalid profile header");
  Session candidate; unsigned slot;
  for(;;) {
   f>>std::ws; if(f.eof()) break;
   if(version==5){
    std::string line;std::getline(f,line);
    std::istringstream record(line);
    std::string tag;record>>tag;
    if(tag=="PERFORMANCE"){
     int split=-1;if(!(record>>slot)||slot>=20||candidate.performances[slot])throw std::runtime_error("Invalid Performance slot");
     SavedPerformance p;if(!(record>>std::quoted(p.name)>>split)||p.name.empty()||p.name.size()>128||(split!=0&&split!=1))throw std::runtime_error("Invalid Performance header");
     p.split=split!=0;candidate.performances[slot]=std::move(p);continue;
    }
    if(tag=="PART"){
     unsigned part=0;int adsr=-1;if(!(record>>slot>>part)||slot>=20||part>=4||!candidate.performances[slot])throw std::runtime_error("Invalid Performance part");
     auto& p=candidate.performances[slot]->parts[part];
     if(p.enabled||!(record>>std::quoted(p.instrument)>>p.low>>p.high>>p.transpose>>adsr)||p.instrument.empty()||p.low<0||p.high>127||p.low>p.high||p.transpose< -48||p.transpose>48||(adsr!=0&&adsr!=1))throw std::runtime_error("Invalid Performance part");
     p.enabled=true;p.patch.adsrEnabled=adsr!=0;
     for(auto& value:p.patch.parameters)if(!(record>>value)||!std::isfinite(value)||value<0||value>1)throw std::runtime_error("Invalid Performance parameter");
     for(auto& cc:p.patch.cc)if(!(record>>cc)||cc< -1||cc>127)throw std::runtime_error("Invalid Performance CC");
     std::string extra;if(record>>extra)throw std::runtime_error("Trailing Performance data");
     continue;
    }
    if(!(std::istringstream(tag)>>slot)||slot>=20||candidate.favorites[slot])throw std::runtime_error("Invalid slot");
    std::string rest;std::getline(record,rest);
    std::istringstream favorite(rest);
    Patch p;if(!(favorite>>std::quoted(p.sfz))||p.sfz.empty()||p.sfz.size()>32768)throw std::runtime_error("Invalid favorite reference");
    for(auto& v:p.parameters)if(!(favorite>>v)||!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid parameter value");
    for(auto& v:p.cc)if(!(favorite>>v)||v< -1||v>127)throw std::runtime_error("Invalid MIDI CC");
    int adsr=-1;if(!(favorite>>p.button.kind>>p.button.channel>>p.button.number>>adsr)||!p.button.valid()||(p.button.kind&&p.button.number!=static_cast<int>(slot))||(adsr!=0&&adsr!=1))throw std::runtime_error("Invalid favorite button");
    p.adsrEnabled=adsr!=0;
    candidate.favorites[slot]=std::move(p);continue;
   }
   if(!(f>>slot) || slot>=20 || candidate.favorites[slot]) throw std::runtime_error("Invalid slot");
   Patch p; if(!(f>>std::quoted(p.sfz)) || p.sfz.empty() || p.sfz.size()>32768)
    throw std::runtime_error("Invalid SFZ path");
   for(auto& v:p.parameters) if(!(f>>v) || !std::isfinite(v) || v<0 || v>1)
    throw std::runtime_error("Invalid parameter value");
   for(auto& v:p.cc) if(!(f>>v) || v < -1 || v >127) throw std::runtime_error("Invalid MIDI CC");
   if(version>=2) {
    if(!(f>>p.button.kind>>p.button.channel>>p.button.number))throw std::runtime_error("Invalid MIDI button");
    if(version==2) {
     // Earlier reconstruction used notes/CC, contrary to the requested contract.
     if((p.button.kind!=0&&p.button.kind!=0x90&&p.button.kind!=0xb0)||p.button.channel<0||p.button.channel>15||p.button.number<0||p.button.number>127)
      throw std::runtime_error("Invalid legacy MIDI button");
     p.button={}; // Preserve sounds and knobs, remove obsolete favorite triggers.
    }
    if(!p.button.valid()||(p.button.kind!=0&&p.button.number!=static_cast<int>(slot)))
     throw std::runtime_error("Favorite must use its corresponding Program Change");
   }
   if(version>=4){int enabled=-1;if(!(f>>enabled)||(enabled!=0&&enabled!=1))throw std::runtime_error("Invalid ADSR state");p.adsrEnabled=enabled==1;}
   else p.adsrEnabled=p.parameters[1]!=0||p.parameters[2]!=0||p.parameters[3]!=1||p.parameters[4]!=0;
   for(const auto& other:candidate.favorites)if(other&&p.button.same(other->button))
    throw std::runtime_error("Duplicate MIDI button");
   candidate.favorites[slot]=std::move(p);
  }
  if(version==5)for(std::size_t slot=0;slot<20;++slot)if(candidate.performances[slot]){
   if(!candidate.favorites[slot])throw std::runtime_error("Orphan Performance");
   bool any=false;for(const auto& part:candidate.performances[slot]->parts)any|=part.enabled;
   if(!any)throw std::runtime_error("Empty Performance");
  }
  *this=std::move(candidate); // Failure never replaces current show.
 }
};
inline std::vector<std::filesystem::path> catalog(const std::filesystem::path& root) {
 std::vector<std::filesystem::path> result;
 if(!std::filesystem::is_directory(root)) return result;
 for(const auto& e:std::filesystem::recursive_directory_iterator(root)) {
  auto ext=e.path().extension().string();
  std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return std::tolower(c);});
  if(e.is_regular_file() && ext==".sfz") result.push_back(e.path());
 }
 std::sort(result.begin(),result.end()); return result;
}
}

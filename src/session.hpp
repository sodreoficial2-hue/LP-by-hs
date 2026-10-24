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
inline constexpr std::array<const char*,15> parameterNames = {
 "Volume", "Attack", "Decay", "Sustain", "Release", "Envelope Filter", "Reverb",
 "Delay", "Chorus", "Tremolo", "Compressor", "Bass", "Mid", "Treble", "Portamento"};
struct ButtonBinding {
 int kind=0; // 0 unassigned, 0xc0 Program Change only
 int channel=0,number=0;
 bool valid() const {return (kind==0||kind==0xc0)&&channel>=0&&channel<16&&number>=0&&number<128;}
 bool same(const ButtonBinding& b) const {return kind!=0&&kind==b.kind&&channel==b.channel&&number==b.number;}
};
struct PerformanceMix {
 std::array<float,4> volumes{1,1,1,1};
 std::array<int,4> cc{-1,-1,-1,-1};
};
struct Patch {
 std::optional<PerformanceMix> performanceMix;
 bool adsrEnabled=false;
 ButtonBinding button;
 std::string sfz;
 std::array<float,15> parameters{1,0,0,1,0,1,0,0,0,0,0,0.5f,0.5f,0.5f,0};
 std::array<int,15> cc{};
 Patch() { cc.fill(-1); }
};
struct SavedPerformancePart {bool enabled=false;std::string instrument;int low=0,high=127,transpose=0;Patch patch{};};
struct SavedPerformance {std::string name;bool split=false;std::array<SavedPerformancePart,4> parts{};};
struct Session {
 std::array<std::optional<SavedPerformance>,20> performances{}; // Legacy v5 migration only.
 std::vector<SavedPerformance> performanceLibrary; // Independent, per-show collection.
 std::array<std::optional<Patch>,20> favorites;
 std::optional<std::size_t> current;
 float master=1.f;int masterCC=-1;
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
  f << "LPHS_PROFILE 8\n" << std::setprecision(9);
  if(!std::isfinite(master)||master<0||master>1||masterCC< -1||masterCC>119||masterCC==64)throw std::runtime_error("Invalid Master");
  f<<"MASTER "<<master<<' '<<masterCC<<'\n';
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
   if(p.performanceMix){
    if(p.sfz.rfind("performance://",0)!=0)throw std::runtime_error("Mix requires a Performance favorite");
    f<<"MIX "<<i;
    for(float v:p.performanceMix->volumes){if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid mix volume");f<<' '<<v;}
    for(int cc:p.performanceMix->cc){if(cc< -1||cc>119||cc==64)throw std::runtime_error("Invalid mix CC");f<<' '<<cc;}
    f<<'\n';
   }
  }
  for(std::size_t a=0;a<favorites.size();++a)if(favorites[a])
   for(std::size_t b=a+1;b<favorites.size();++b)if(favorites[b]&&favorites[a]->button.same(favorites[b]->button))
    throw std::runtime_error("Duplicate MIDI button");
  if(performanceLibrary.size()>256)throw std::runtime_error("Too many Performances");
  for(std::size_t slot=0;slot<performanceLibrary.size();++slot){
   const auto& performance=performanceLibrary[slot];
   if(performance.name.empty()||performance.name.size()>128)throw std::runtime_error("Invalid saved Performance");
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
  if(!(f>>magic>>version) || magic!="LPHS_PROFILE" || (version<1||version>8))
   throw std::runtime_error("Invalid profile header");
  Session candidate; unsigned slot;
  for(;;) {
   f>>std::ws; if(f.eof()) break;
   if(version>=5){
    std::string line;std::getline(f,line);
    std::istringstream record(line);
    std::string tag;record>>tag;
    if(tag=="MIX"&&version>=8){
     if(!(record>>slot)||slot>=20||!candidate.favorites[slot]||candidate.favorites[slot]->performanceMix||candidate.favorites[slot]->sfz.rfind("performance://",0)!=0)throw std::runtime_error("Invalid mix favorite");
     PerformanceMix mix;
     for(float& v:mix.volumes)if(!(record>>v)||!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid mix volume");
     for(int& cc:mix.cc)if(!(record>>cc)||cc< -1||cc>119||cc==64)throw std::runtime_error("Invalid mix CC");
     std::string extra;if(record>>extra)throw std::runtime_error("Trailing mix data");
     candidate.favorites[slot]->performanceMix=mix;continue;
    }
    if(tag=="MASTER"&&version>=7){
     if(!(record>>candidate.master>>candidate.masterCC)||!std::isfinite(candidate.master)||candidate.master<0||candidate.master>1||candidate.masterCC< -1||candidate.masterCC>119||candidate.masterCC==64)throw std::runtime_error("Invalid Master");
     std::string extra;if(record>>extra)throw std::runtime_error("Trailing Master data");continue;
    }
    if(tag=="PERFORMANCE"){
     int split=-1;if(!(record>>slot)||slot>255||(version==5&&slot>=20)||(version==5&&candidate.performances[slot])||(version>=6&&slot!=candidate.performanceLibrary.size()))throw std::runtime_error("Invalid Performance slot");
     SavedPerformance p;if(!(record>>std::quoted(p.name)>>split)||p.name.empty()||p.name.size()>128||(split!=0&&split!=1))throw std::runtime_error("Invalid Performance header");
     p.split=split!=0;if(version==5)candidate.performances[slot]=std::move(p);else candidate.performanceLibrary.push_back(std::move(p));continue;
    }
    if(tag=="PART"){
     unsigned part=0;int adsr=-1;if(!(record>>slot>>part)||part>=4||(version==5?(slot>=20||!candidate.performances[slot]):slot>=candidate.performanceLibrary.size()))throw std::runtime_error("Invalid Performance part");
     auto& p=(version==5?candidate.performances[slot]->parts[part]:candidate.performanceLibrary[slot].parts[part]);
     if(p.enabled||!(record>>std::quoted(p.instrument)>>p.low>>p.high>>p.transpose>>adsr)||p.instrument.empty()||p.low<0||p.high>127||p.low>p.high||p.transpose< -48||p.transpose>48||(adsr!=0&&adsr!=1))throw std::runtime_error("Invalid Performance part");
     p.enabled=true;p.patch.adsrEnabled=adsr!=0;
     for(std::size_t j=0;j<(version>=7?15u:14u);++j)if(!(record>>p.patch.parameters[j])||!std::isfinite(p.patch.parameters[j])||p.patch.parameters[j]<0||p.patch.parameters[j]>1)throw std::runtime_error("Invalid Performance parameter");
     for(std::size_t j=0;j<(version>=7?15u:14u);++j)if(!(record>>p.patch.cc[j])||p.patch.cc[j]< -1||p.patch.cc[j]>127)throw std::runtime_error("Invalid Performance CC");
     std::string extra;if(record>>extra)throw std::runtime_error("Trailing Performance data");
     continue;
    }
    if(!(std::istringstream(tag)>>slot)||slot>=20||candidate.favorites[slot])throw std::runtime_error("Invalid slot");
    std::string rest;std::getline(record,rest);
    std::istringstream favorite(rest);
    Patch p;if(!(favorite>>std::quoted(p.sfz))||p.sfz.empty()||p.sfz.size()>32768)throw std::runtime_error("Invalid favorite reference");
    for(std::size_t j=0;j<(version>=7?15u:14u);++j)if(!(favorite>>p.parameters[j])||!std::isfinite(p.parameters[j])||p.parameters[j]<0||p.parameters[j]>1)throw std::runtime_error("Invalid parameter value");
    for(std::size_t j=0;j<(version>=7?15u:14u);++j)if(!(favorite>>p.cc[j])||p.cc[j]< -1||p.cc[j]>127)throw std::runtime_error("Invalid MIDI CC");
    int adsr=-1;if(!(favorite>>p.button.kind>>p.button.channel>>p.button.number>>adsr)||!p.button.valid()||(p.button.kind&&p.button.number!=static_cast<int>(slot))||(adsr!=0&&adsr!=1))throw std::runtime_error("Invalid favorite button");
    p.adsrEnabled=adsr!=0;
    candidate.favorites[slot]=std::move(p);continue;
   }
   if(!(f>>slot) || slot>=20 || candidate.favorites[slot]) throw std::runtime_error("Invalid slot");
   Patch p; if(!(f>>std::quoted(p.sfz)) || p.sfz.empty() || p.sfz.size()>32768)
    throw std::runtime_error("Invalid SFZ path");
   for(std::size_t j=0;j<14;++j) if(!(f>>p.parameters[j]) || !std::isfinite(p.parameters[j]) || p.parameters[j]<0 || p.parameters[j]>1)
    throw std::runtime_error("Invalid parameter value");
   for(std::size_t j=0;j<14;++j) if(!(f>>p.cc[j]) || p.cc[j] < -1 || p.cc[j] >127) throw std::runtime_error("Invalid MIDI CC");
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
  if(version==5)for(std::size_t slot=0;slot<20;++slot)if(candidate.performances[slot]){
   candidate.performanceLibrary.push_back(*candidate.performances[slot]);
   candidate.favorites[slot].reset();candidate.performances[slot].reset();
  }
  for(const auto& performance:candidate.performanceLibrary){bool any=false;for(const auto& part:performance.parts)any|=part.enabled;if(!any)throw std::runtime_error("Empty Performance");}
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

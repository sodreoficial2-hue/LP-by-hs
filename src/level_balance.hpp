#pragma once
#include "atomic_file.hpp"
#include "sf2_reference.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <vector>

namespace lp {
// A preset identity is independent of a show/favorite and includes SF2 bank/program.
inline std::string levelKey(const std::string& reference){
 auto file=[](const std::filesystem::path& path){return std::filesystem::weakly_canonical(std::filesystem::absolute(path)).generic_u8string();};
 if(auto sf2=parseSf2Reference(reference))return "sf2:"+std::to_string(sf2->bank)+":"+std::to_string(sf2->program)+":"+file(sf2->path);
 return "sfz:"+file(std::filesystem::u8path(reference));
}
inline std::string levelStamp(const std::string& reference){
 auto sf2=parseSf2Reference(reference);auto path=sf2?sf2->path:std::filesystem::u8path(reference);
 if(!std::filesystem::is_regular_file(path))throw std::runtime_error("Timbre ausente durante o equilibrio de volumes");
 return std::to_string(std::filesystem::file_size(path))+":"+std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count());
}
struct LevelResult {
 float gain=1,rms=0,peak=0;
 bool audible=false;
 bool valid()const{return std::isfinite(gain)&&gain>0&&gain<=4&&std::isfinite(rms)&&rms>=0&&std::isfinite(peak)&&peak>=0&&(!audible||(rms>0&&peak>0));}
};
// Fixed 48 kHz offline analysis. Remove DC/very low rumble in the detector only;
// playback audio is never filtered. This is a level estimate, not certified LUFS.
class LevelMeter {
 struct HighPass {double x=0,y=0;double tick(double v){double out=v-x+.995*y;x=v;y=out;return out;}};
 std::array<HighPass,2> hp{};
 double energy=0;int samples=0;float peak=0;
 std::vector<double> windows;
 void flush(){if(samples){double mean=energy/samples;if(mean>1e-10)windows.push_back(mean);}energy=0;samples=0;}
public:
 void segment(){flush();hp={};}
 void add(const float* left,const float* right,int frames,bool measureLevel=true){
  for(int i=0;i<frames;++i){
   if(!std::isfinite(left[i])||!std::isfinite(right[i]))throw std::runtime_error("Saida invalida durante a calibracao");
   peak=std::max({peak,std::abs(left[i]),std::abs(right[i])});
   if(measureLevel){double l=hp[0].tick(left[i]),r=hp[1].tick(right[i]);energy+=(l*l+r*r)*.5;++samples;if(samples==12000)flush();}
  }
 }
 LevelResult finish(){
  flush();LevelResult result;result.peak=peak;
  if(windows.empty()||peak<1e-5f)return result; // Never amplify silence/noise.
  std::sort(windows.begin(),windows.end());
  // Upper quartile reduces the influence of silent keys and long decays.
  result.rms=static_cast<float>(std::sqrt(windows[(windows.size()-1)*3/4]));
  if(result.rms<1e-5f)return result;
  result.audible=true;
  constexpr float target=.063095734f; // -24 dBFS detector RMS; fixed across runs.
  constexpr float peakCeiling=.35481339f; // -9 dBFS measured test peaks, not a limiter.
  result.gain=std::min(std::clamp(target/result.rms,.015848932f,3.9810717f),peakCeiling/peak);
  if(!result.valid())throw std::runtime_error("Compensacao de volume invalida");
  return result;
 }
};
class LevelCache {
 struct Entry {std::string stamp;LevelResult result;};
 std::map<std::string,Entry> entries;
public:
 void load(const std::filesystem::path& file){
  if(!std::filesystem::exists(file)){entries.clear();return;}
  if(std::filesystem::file_size(file)>32*1024*1024)throw std::runtime_error("Cache de volumes excede o limite");
  std::ifstream in(file,std::ios::binary);in.imbue(std::locale::classic());std::string header,line;
  if(!std::getline(in,header))throw std::runtime_error("Cache de volumes invalido");
  if(!header.empty()&&header.back()=='\r')header.pop_back();
  if(header!="LPHS_LEVEL_BALANCE 1")throw std::runtime_error("Cache de volumes invalido");
  std::map<std::string,Entry> candidate;
  while(std::getline(in,line)){
   if(line.empty())continue;
   std::istringstream row(line);row.imbue(std::locale::classic());std::string key,extra;Entry entry;int audible=-1;
   if(!(row>>std::quoted(key)>>std::quoted(entry.stamp)>>entry.result.gain>>entry.result.rms>>entry.result.peak>>audible)||key.empty()||key.size()>32768||entry.stamp.empty()||entry.stamp.size()>512||(audible!=0&&audible!=1)||(row>>extra))throw std::runtime_error("Registro de volume invalido");
   entry.result.audible=audible!=0;
   if(!entry.result.valid()||!candidate.emplace(key,entry).second)throw std::runtime_error("Registro de volume invalido");
  }
  if(in.bad())throw std::runtime_error("Falha ao ler volumes salvos");
  entries=std::move(candidate);
 }
 bool find(const std::string& key,const std::string& stamp,LevelResult& result)const{
  auto it=entries.find(key);if(it==entries.end()||it->second.stamp!=stamp)return false;result=it->second.result;return true;
 }
 void save(const std::filesystem::path& file,const std::string& key,const std::string& stamp,const LevelResult& result){
  if(key.empty()||key.size()>32768||stamp.empty()||stamp.size()>512||!result.valid())throw std::invalid_argument("Compensacao de volume invalida");
  auto candidate=entries;candidate[key]={stamp,result};
  std::ostringstream out;out.imbue(std::locale::classic());out<<"LPHS_LEVEL_BALANCE 1\n"<<std::setprecision(9);
  for(const auto& pair:candidate)out<<std::quoted(pair.first)<<' '<<std::quoted(pair.second.stamp)<<' '<<pair.second.result.gain<<' '<<pair.second.result.rms<<' '<<pair.second.result.peak<<' '<<pair.second.result.audible<<'\n';
  atomicWrite(file,out.str());entries=std::move(candidate);
 }
};
}

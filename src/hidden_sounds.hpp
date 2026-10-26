#pragma once
#include "atomic_file.hpp"
#include "sf2_reference.hpp"
#include "session.hpp"
#include <set>
#include <sstream>
#include <iomanip>
namespace lp {
// Catalog exclusions only: files, other SF2 presets and saved show references survive.
inline std::string soundExclusionKey(const std::string& reference,const std::filesystem::path& base){
 auto pathKey=[&](const std::filesystem::path& file){
  auto absolute=std::filesystem::absolute(file).lexically_normal();auto relative=absolute.lexically_relative(std::filesystem::absolute(base).lexically_normal());
  if(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..")return std::string("relative:")+relative.generic_u8string();
  return std::string("absolute:")+absolute.generic_u8string();
 };
 if(auto sf2=parseSf2Reference(reference))return "sf2:"+std::to_string(sf2->bank)+":"+std::to_string(sf2->program)+":"+pathKey(sf2->path);
 if(reference.rfind("bank://",0)==0)return reference;
 return "sfz:"+pathKey(std::filesystem::u8path(reference));
}
inline std::string performanceExclusionKey(const SavedPerformance& p,const std::filesystem::path& base){
 // Content identity avoids hiding an unrelated performance at the same profile index.
 std::ostringstream out;out<<"performance:"<<std::quoted(p.name)<<' '<<p.split;
 for(const auto& part:p.parts){out<<' '<<part.enabled;if(part.enabled)out<<' '<<std::quoted(soundExclusionKey(part.instrument,base))<<' '<<part.low<<' '<<part.high<<' '<<part.transpose;}
 return out.str();
}
class HiddenSounds {
 std::set<std::string> keys;
 static constexpr std::size_t limit=16*1024*1024;
public:
 bool contains(const std::string& key)const{return keys.count(key)!=0;}
 void load(const std::filesystem::path& file){
  std::set<std::string> candidate;
  if(!std::filesystem::exists(file)){keys.clear();return;}
  if(std::filesystem::file_size(file)>limit)throw std::runtime_error("Lista de timbres excluidos excede o limite");
  std::ifstream in(file,std::ios::binary);std::string line;
  if(!std::getline(in,line))throw std::runtime_error("Lista de timbres excluidos invalida");
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="LPHS_HIDDEN_SOUNDS 1")throw std::runtime_error("Lista de timbres excluidos invalida");
  while(std::getline(in,line)){
   if(line.empty())continue;std::istringstream row(line);std::string key,extra;
   if(!(row>>std::quoted(key))||key.empty()||key.size()>32768||(row>>extra))throw std::runtime_error("Timbre excluido invalido");
   candidate.insert(std::move(key));
  }
  if(in.bad())throw std::runtime_error("Falha ao ler timbres excluidos");keys=std::move(candidate);
 }
 const std::set<std::string>& entries()const{return keys;}
 void restore(const std::string& key,const std::filesystem::path& file){
  auto candidate=keys;
  if(key.empty())candidate.clear();else candidate.erase(key);
  std::ostringstream out;out<<"LPHS_HIDDEN_SOUNDS 1\n";
  for(const auto& k:candidate)out<<std::quoted(k)<<'\n';
  atomicWrite(file,out.str());keys=std::move(candidate);
 }
 void exclude(const std::string& key,const std::filesystem::path& file){
  if(key.empty()||key.size()>32768)throw std::invalid_argument("Timbre excluido invalido");
  if(contains(key))return;auto candidate=keys;candidate.insert(key);std::ostringstream out;out<<"LPHS_HIDDEN_SOUNDS 1\n";
  for(const auto& k:candidate)out<<std::quoted(k)<<'\n';auto data=out.str();
  if(data.size()>limit)throw std::runtime_error("Lista de timbres excluidos excede o limite");
  atomicWrite(file,data);keys=std::move(candidate); // Commit memory only after durable save.
 }
};
}

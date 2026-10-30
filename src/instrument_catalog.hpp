#pragma once
#include "session.hpp"
#include "display_name.hpp"
#include "sf2_reference.hpp"
#include <set>

namespace lp {
inline constexpr std::array<const char*,15> instrumentCategories{
 "Pianos acústicos","Pianos elétricos","Órgãos","Cordas","Metais","Leads","Pads","Percussão","Bateria","Acordeons","Outros","Contrabaixos","Guitarras","Performance","Meus Sons"};
struct Instrument {
 std::string id,name,license,mode,customName;
 bool userImported=false;
 std::filesystem::path sfz;
 unsigned category=10;
 int testNote=60;
};
inline std::vector<Instrument> loadInstrumentBank(const std::filesystem::path& root,const std::set<std::string>& deletedIds={},std::vector<std::string>* warnings=nullptr){
 std::ifstream in(root/"catalog.txt",std::ios::binary);std::string line;
 if(!std::getline(in,line))throw std::runtime_error("Catálogo de timbres inválido");
 if(!line.empty()&&line.back()=='\r')line.pop_back();
 if(line!="LPHS_BANK 1")throw std::runtime_error("Catálogo de timbres inválido");
 std::vector<Instrument> result;std::set<std::string> ids,paths;
 while(std::getline(in,line)){
  if(line.empty()||line[0]=='#')continue;
  Instrument p;std::string path,extra;std::istringstream row(line);
  if(!(row>>std::quoted(p.id)>>p.category>>std::quoted(p.name)>>std::quoted(path)>>p.testNote>>std::quoted(p.mode)>>std::quoted(p.license))||(row>>extra)||p.id.empty()||p.name.empty()||p.category>=instrumentCategories.size()||p.testNote<0||p.testNote>127||(p.mode!="poly"&&p.mode!="mono"&&p.mode!="kit"))throw std::runtime_error("Entrada inválida no catálogo de timbres");
  auto relative=std::filesystem::u8path(path);
  if(relative.empty()||relative.is_absolute()||relative.has_root_name()||relative.extension()!=".sfz")throw std::runtime_error("Caminho inválido no banco de timbres");
  for(const auto& part:relative)if(part=="..")throw std::runtime_error("Caminho fora do banco de timbres");
  p.sfz=std::filesystem::absolute(root/relative).lexically_normal();
  if(deletedIds.count(p.id)&&!std::filesystem::exists(p.sfz))continue;
  if(!std::filesystem::is_regular_file(p.sfz)||!ids.insert(p.id).second||!paths.insert(path).second){if(warnings){warnings->push_back("Timbre ausente ou duplicado: "+p.id);continue;}throw std::runtime_error("Timbre ausente ou duplicado: "+p.id);}
  result.push_back(std::move(p));
 }
 if(result.empty()&&deletedIds.empty()&&!warnings)throw std::runtime_error("Banco de timbres vazio");
 return result;
}
// Stable IDs keep new favorites usable after moving the portable application.
inline std::filesystem::path resolveInstrument(const std::string& reference,const std::vector<Instrument>& bank){
 constexpr const char* prefix="bank://";
 if(reference.compare(0,7,prefix)==0){auto id=reference.substr(7);for(const auto& p:bank)if(p.id==id)return p.sfz;throw std::runtime_error("Timbre do banco não instalado: "+id);}
 return std::filesystem::u8path(reference); // Preserve existing imported/legacy favorites.
}
inline std::string instrumentReference(const Instrument& p){return p.id.empty()?p.sfz.u8string():"bank://"+p.id;}
inline std::string instrumentDisplayName(const Instrument& instrument){
 if(!instrument.customName.empty())return instrument.customName;
 return soundDisplayName(instrument.name,instrument.sfz.u8string().rfind("sf2://",0)==0);
}
inline std::string instrumentName(const std::string& reference,const std::vector<Instrument>& bank){
 for(const auto& p:bank)if(instrumentReference(p)==reference)return instrumentDisplayName(p);
 if(reference.compare(0,7,"bank://")==0)return soundDisplayName(reference.substr(7))+" (ausente)";
 if(auto preset=parseSf2Reference(reference))return soundDisplayName(preset->path.stem().u8string());
 return soundDisplayName(std::filesystem::u8path(reference).stem().u8string());
}
}

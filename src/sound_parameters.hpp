#pragma once
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "hidden_sounds.hpp"
#include "performance.hpp"
#include "recoverable_file.hpp"
#include <map>
namespace lp {
// Identity belongs to the instrument, never its category, favorite slot or profile.
inline std::string soundParameterKey(const std::string& reference,const std::filesystem::path& base){
 if(reference.empty()||reference.rfind("performance://",0)==0)throw std::runtime_error("Identidade de timbre inválida para memorização");
 auto normalized=reference;
 if(auto sf2=parseSf2Reference(reference)){
  if(sf2->path.is_relative())normalized=makeSf2Reference(base/sf2->path,sf2->bank,sf2->program);
 }else if(reference.rfind("bank://",0)!=0){auto path=std::filesystem::u8path(reference);if(path.is_relative())normalized=(base/path).u8string();}
 return soundExclusionKey(normalized,base);
}
struct SoundParameters {
 std::array<float,16> values=Patch{}.parameters;
 std::array<int,16> cc=Patch{}.cc;
 bool adsr=false;
 explicit SoundParameters(const Patch& patch):values(patch.parameters),cc(patch.cc),adsr(patch.adsrEnabled){}
 SoundParameters()=default;
 void validate()const{for(float v:values)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Parâmetro de timbre inválido");for(int c:cc)if(c< -1||c>=120||c==64)throw std::runtime_error("Mapeamento de timbre inválido");}
 void apply(Patch& patch)const{patch.parameters=values;patch.cc=cc;patch.adsrEnabled=adsr;}
 bool operator==(const SoundParameters& other)const{return values==other.values&&cc==other.cc&&adsr==other.adsr;}
};
// A portable identity for the Performance layout, excluding all patch values.
// Each part has its own controls: editing its piano never changes a standalone piano.
inline std::string performanceParameterKey(const SavedPerformance& performance,const std::filesystem::path& base){return performanceExclusionKey(performance,base);}
class SoundParameterStore {
 std::map<std::string,SoundParameters> entries;
 std::optional<std::pair<float,int>> master;
 static constexpr std::size_t maxEntries=65536;
 static std::string partKey(const std::string& identity,int part,const std::string& reference,const std::filesystem::path& base){return identity+"/part="+std::to_string(part)+"/"+soundParameterKey(reference,base);}
 static void validateMaster(float value,int cc){if(!std::isfinite(value)||value<0||value>1||cc< -1||cc>=120||cc==64)throw std::runtime_error("Master memorizado inválido");}
public:
 bool remember(const std::string& key,const Patch& patch){
  SoundParameters next(patch);next.validate();if(key.empty()||key.size()>65536)throw std::runtime_error("Identidade de timbre excede o limite");
  auto found=entries.find(key);if(found!=entries.end()&&found->second==next)return false;
  if(found==entries.end()&&entries.size()>=maxEntries)throw std::runtime_error("Limite de timbres memorizados atingido");entries[key]=next;return true;
 }
 bool restore(const std::string& key,Patch& patch)const{auto found=entries.find(key);if(found==entries.end())return false;found->second.apply(patch);return true;}
 bool clearBinding(int cc){bool changed=false;for(auto& entry:entries)for(auto& binding:entry.second.cc)if(binding==cc){binding=-1;changed=true;}return changed;}
 bool rememberMaster(float value,int cc){validateMaster(value,cc);auto next=std::make_pair(value,cc);if(master&&*master==next)return false;master=next;return true;}
 void restoreMaster(Session& session)const{if(master){session.master=master->first;session.masterCC=master->second;}}
 bool rememberPerformance(const SavedPerformance& identity,const Performance& performance,const std::filesystem::path& base){
  auto key=performanceParameterKey(identity,base);bool changed=false;
  for(int i=0;i<4;++i)if(identity.parts[i].enabled&&performance.parts[i].enabled&&soundParameterKey(identity.parts[i].instrument,base)==soundParameterKey(performance.parts[i].instrument,base))changed=remember(partKey(key,i,identity.parts[i].instrument,base),performance.parts[i].patch)||changed;
  return changed;
 }
 bool restorePerformance(const SavedPerformance& identity,Performance& performance,const std::filesystem::path& base)const{
  auto key=performanceParameterKey(identity,base);bool restored=false;
  for(int i=0;i<4;++i)if(identity.parts[i].enabled&&performance.parts[i].enabled&&soundParameterKey(identity.parts[i].instrument,base)==soundParameterKey(performance.parts[i].instrument,base))restored=restore(partKey(key,i,identity.parts[i].instrument,base),performance.parts[i].patch)||restored;
  return restored;
 }
 void restoreSession(Session& session,const std::filesystem::path& base)const{
  restoreMaster(session);
  for(auto& p:session.favorites)if(p&&p->sfz.rfind("performance://",0)!=0)restore(soundParameterKey(p->sfz,base),*p);
 }
 std::string serialize()const{
  std::ostringstream out;out<<std::setprecision(9)<<"LPHS_SOUND_PARAMETERS 1\n"<<bool(master);
  if(master)out<<' '<<master->first<<' '<<master->second;out<<'\n'<<entries.size()<<'\n';
  for(const auto& pair:entries){out<<std::quoted(pair.first)<<' '<<pair.second.adsr;for(float v:pair.second.values)out<<' '<<v;for(int c:pair.second.cc)out<<' '<<c;out<<'\n';}
  auto bytes=out.str();if(bytes.size()>32*1024*1024)throw std::runtime_error("Memória de timbres excede o limite de 32 MiB");return bytes;
 }
 bool load(const std::filesystem::path& file){
  if(!std::filesystem::exists(file)){entries.clear();master.reset();return false;}
  return recoverFile(file,[&](const std::filesystem::path& source){
   SoundParameterStore next;std::istringstream in(boundedFile(source));std::string header;std::getline(in,header);if(header!="LPHS_SOUND_PARAMETERS 1")throw std::runtime_error("Memória permanente de timbres inválida");
   int hasMaster=0;if(!(in>>hasMaster)||(hasMaster!=0&&hasMaster!=1))throw std::runtime_error("Master memorizado inválido");
   if(hasMaster){float value;int cc;if(!(in>>value>>cc))throw std::runtime_error("Master memorizado incompleto");next.rememberMaster(value,cc);}
   std::size_t count=0;if(!(in>>count)||count>maxEntries)throw std::runtime_error("Contagem de timbres memorizados inválida");
   for(std::size_t i=0;i<count;++i){std::string key;int adsr;Patch patch;if(!(in>>std::quoted(key)>>adsr)||(adsr!=0&&adsr!=1))throw std::runtime_error("Registro de timbre memorizado inválido");patch.adsrEnabled=adsr;
    for(auto& v:patch.parameters)if(!(in>>v))throw std::runtime_error("Parâmetro memorizado incompleto");for(auto& cc:patch.cc)if(!(in>>cc))throw std::runtime_error("Mapeamento memorizado incompleto");
    if(next.entries.count(key))throw std::runtime_error("Identidade de timbre memorizado duplicada");next.remember(key,patch);
   }
   std::string extra;if(in>>extra)throw std::runtime_error("Dados extras na memória de timbres");*this=std::move(next);
  });
 }
};
// Parameter persistence does not turn a knob adjustment into a request to save
// the show. Favorite membership, PC buttons, paths and arrangements stay manual.
inline std::string showStructureSnapshot(Session show){
 show.performanceLibrary.clear();show.master=1;show.masterCC=-1;
 auto normalize=[](Patch& p){p.parameters=Patch{}.parameters;p.cc=Patch{}.cc;p.adsrEnabled=false;p.performanceMix.reset();};
 for(auto& p:show.favorites)if(p)normalize(*p);
 for(auto& performance:show.performances)if(performance)for(auto& part:performance->parts)normalize(part.patch);
 return show.serialize();
}
}

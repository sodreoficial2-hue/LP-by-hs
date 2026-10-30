#pragma once
#include "session.hpp"
#include <numeric>
#include <map>
#include "recoverable_file.hpp"
namespace lp {
inline std::string performanceIdentity(const SavedPerformance& p){Session s;s.performanceLibrary.push_back(p);return s.serialize();}
inline void remapPerformanceFavorites(Session& s,const std::vector<std::size_t>& map){
 for(auto& favorite:s.favorites)if(favorite&&favorite->sfz.rfind("performance://",0)==0){
  auto suffix=favorite->sfz.substr(14);std::size_t used=0;auto index=std::stoul(suffix,&used);
  if(used!=suffix.size()||index>=map.size())throw std::runtime_error("Invalid Performance favorite");
  favorite->sfz="performance://"+std::to_string(map[index]);
 }
}
inline void erasePerformance(Session& s,std::size_t index){
 if(index>=s.performanceLibrary.size())throw std::out_of_range("Performance missing");
 for(std::size_t slot=0;slot<s.favorites.size();++slot){auto& f=s.favorites[slot];if(!f||f->sfz.rfind("performance://",0)!=0)continue;
  auto n=std::stoul(f->sfz.substr(14));if(n==index){f.reset();s.performances[slot].reset();if(s.current==slot)s.current.reset();}
  else if(n>index)f->sfz="performance://"+std::to_string(n-1);
 }
 s.performanceLibrary.erase(s.performanceLibrary.begin()+index);
}
inline void replacePerformanceSnapshot(Session& show,const std::string& original,int hint,const SavedPerformance& replacement){
 int target=-1;
 if(hint>=0&&static_cast<std::size_t>(hint)<show.performanceLibrary.size()&&performanceIdentity(show.performanceLibrary[hint])==original)target=hint;
 else for(std::size_t i=0;i<show.performanceLibrary.size();++i)if(performanceIdentity(show.performanceLibrary[i])==original){if(target>=0)throw std::runtime_error("Há cópias idênticas da Performance original. Use Salvar como nova.");target=static_cast<int>(i);}
 if(target<0)throw std::runtime_error("A Performance original mudou ou foi removida. Use Salvar como nova.");
 show.performanceLibrary[target]=replacement;
}
// A separate atomic file contains performances only, never show/MIDI/audio state.
class PerformanceStore {
 std::filesystem::path path;Session stored;unsigned long long completed=0;
public:
 bool load(const std::filesystem::path& file){path=file;stored={};completed=0;if(!std::filesystem::exists(file))return false;
  return recoverFile(file,[&](const std::filesystem::path& source){auto bytes=boundedFile(source);Session next;unsigned long long epoch=0;
   std::istringstream in(bytes);std::string header;std::getline(in,header);if(header=="LPHS_PERFORMANCES 2"){std::string payload,extra;if(!(in>>epoch>>std::quoted(payload))||(in>>extra))throw std::runtime_error("Biblioteca permanente inválida");std::istringstream profile(payload);next.read(profile);}else{std::istringstream profile(bytes);next.read(profile);}
   stored=std::move(next);completed=epoch;
  });
 }
 unsigned long long nextEpoch()const{return completed+1;}
 bool completedDraft(unsigned long long epoch)const{return epoch&&epoch<=completed;}

 void mergeInto(Session& show)const{
  auto next=show;auto library=stored.performanceLibrary;std::vector<std::size_t> map;
  std::map<std::string,std::size_t> identities;for(std::size_t i=0;i<library.size();++i)identities.emplace(performanceIdentity(library[i]),i);
  for(const auto& performance:show.performanceLibrary){
   auto identity=performanceIdentity(performance);auto found=identities.find(identity);std::size_t index=found==identities.end()?library.size():found->second;
   if(index==library.size()){if(library.size()>=256)throw std::runtime_error("Limite de 256 Performances");identities.emplace(std::move(identity),index);library.push_back(performance);}
   map.push_back(index);
  }
  remapPerformanceFavorites(next,map);next.performanceLibrary=std::move(library);show=std::move(next);
 }
 void commit(const Session& show,unsigned long long savedDraft=0){
  if(path.empty())throw std::runtime_error("Armazenamento permanente de Performances indisponível");
  Session next;next.performanceLibrary=show.performanceLibrary;auto epoch=std::max(completed,savedDraft);std::ostringstream out;out<<"LPHS_PERFORMANCES 2\n"<<epoch<<'\n'<<std::quoted(next.serialize())<<'\n';backedUpWrite(path,out.str());stored=std::move(next);completed=epoch;
 }
};
}

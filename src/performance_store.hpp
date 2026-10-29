#pragma once
#include "session.hpp"
#include <numeric>
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
// A separate atomic file contains performances only, never show/MIDI/audio state.
class PerformanceStore {
 std::filesystem::path path;Session stored;
public:
 void load(const std::filesystem::path& file){Session next;if(std::filesystem::exists(file))next.load(file);path=file;stored=std::move(next);}
 void mergeInto(Session& show)const{
  auto next=show;auto library=stored.performanceLibrary;std::vector<std::size_t> map;
  for(const auto& performance:show.performanceLibrary){
   auto identity=performanceIdentity(performance);std::size_t index=0;
   for(;index<library.size();++index)if(performanceIdentity(library[index])==identity)break;
   if(index==library.size()){if(library.size()>=256)throw std::runtime_error("Limite de 256 Performances");library.push_back(performance);}
   map.push_back(index);
  }
  remapPerformanceFavorites(next,map);next.performanceLibrary=std::move(library);show=std::move(next);
 }
 void commit(const Session& show){
  if(path.empty())throw std::runtime_error("Armazenamento permanente de Performances indisponível");
  Session next;next.performanceLibrary=show.performanceLibrary;next.save(path);stored=std::move(next);
 }
};
}

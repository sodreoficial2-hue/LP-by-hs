#pragma once
#include "performance.hpp"
#include <iomanip>
namespace lp {
inline void copyPerformancePart(Performance& p,std::size_t from,std::size_t to){
 if(from>=4||to>=4||from==to)throw std::invalid_argument("Invalid layer copy");
 auto low=p.parts[to].low,high=p.parts[to].high;p.parts[to]=p.parts[from];p.parts[to].low=low;p.parts[to].high=high;
}
inline void swapPerformanceParts(Performance& p,std::size_t a,std::size_t b){
 if(a>=4||b>=4||a==b)throw std::invalid_argument("Invalid layer swap");
 auto al=p.parts[a].low,ah=p.parts[a].high,bl=p.parts[b].low,bh=p.parts[b].high;
 std::swap(p.parts[a],p.parts[b]);p.parts[a].low=al;p.parts[a].high=ah;p.parts[b].low=bl;p.parts[b].high=bh;
}
inline std::string midiNoteLabel(int note){
 if(note<0||note>127)throw std::invalid_argument("Invalid note");
 constexpr const char* names[]={"Dó","Dó#","Ré","Ré#","Mi","Fá","Fá#","Sol","Sol#","Lá","Lá#","Si"};return std::string(names[note%12])+std::to_string(note/12-1)+" (MIDI "+std::to_string(note)+")";
}
struct PerformanceWorkspace {
 std::array<int,4> categories{0,0,0,0};std::array<std::string,4> searches{};
 std::array<bool,4> muted{};int solo=-1,mode=0,selectedPart=0;bool customZones=false;
 unsigned long long epoch=0;std::string original;int originalIndex=-1; // Snapshot guard plus a hint for identical copies.
 unsigned auditionMask()const{unsigned mask=0;for(int i=0;i<4;++i)if(!muted[i]&&(solo<0||solo==i))mask|=1u<<i;return mask;}
 std::string serialize(const Performance& performance)const{
  auto p=performance;auto name=p.name;p.name=name.empty()?"Rascunho":name;Session session;bool any=false;for(const auto& part:p.parts)any|=part.enabled;if(any)session.performanceLibrary.push_back(savePerformance(p));
  std::ostringstream out;out<<"LPHS_WORKSPACE 3\n"<<mode<<' '<<selectedPart<<' '<<customZones<<' '<<solo<<' '<<originalIndex<<' '<<epoch<<'\n'<<std::quoted(name)<<'\n'<<std::quoted(original)<<'\n';
  for(int i=0;i<4;++i)out<<categories[i]<<' '<<muted[i]<<' '<<std::quoted(searches[i])<<'\n';out<<std::quoted(session.serialize())<<'\n';return out.str();
 }
 void save(const std::filesystem::path& file,const Performance& p)const{atomicWrite(file,serialize(p));}
 bool load(const std::filesystem::path& file,Performance& performance){
  if(!std::filesystem::exists(file))return false;std::ifstream in(file,std::ios::binary);return read(in,performance);
 }
 bool read(std::istream& in,Performance& performance){
  std::string magic,name,bytes;int version=0;PerformanceWorkspace candidate;
  if(!(in>>magic>>version)||magic!="LPHS_WORKSPACE"||(version<1||version>3)||!(in>>candidate.mode>>candidate.selectedPart>>candidate.customZones>>candidate.solo)||candidate.mode<0||candidate.mode>4||candidate.selectedPart<0||candidate.selectedPart>3||candidate.solo< -1||candidate.solo>3)throw std::runtime_error("Rascunho de Performance inválido");
  if(version>=2&&(!(in>>candidate.originalIndex)||candidate.originalIndex< -1||candidate.originalIndex>255))throw std::runtime_error("Referência de edição inválida");
  if(version>=3&&!(in>>candidate.epoch))throw std::runtime_error("Versão de rascunho inválida");
  if(!(in>>std::quoted(name)>>std::quoted(candidate.original))||name.size()>512)throw std::runtime_error("Nome de rascunho inválido");
  for(int i=0;i<4;++i)if(!(in>>candidate.categories[i]>>candidate.muted[i]>>std::quoted(candidate.searches[i]))||candidate.categories[i]<0||candidate.categories[i]>14||candidate.categories[i]==13||candidate.searches[i].size()>1024)throw std::runtime_error("Busca de camada inválida");
  if(!(in>>std::quoted(bytes)))throw std::runtime_error("Rascunho incompleto");std::string extra;if(in>>extra)throw std::runtime_error("Dados inesperados no rascunho");
  Session session;std::istringstream payload(bytes);session.read(payload);if(session.performanceLibrary.size()>1)throw std::runtime_error("Rascunho inválido");
  Performance next;if(!session.performanceLibrary.empty())next=restorePerformance(session.performanceLibrary[0]);next.name=name;
  *this=std::move(candidate);performance=std::move(next);return true;
 }
};
}

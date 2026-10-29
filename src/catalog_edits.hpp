#pragma once
#include "hidden_sounds.hpp"
#include "instrument_catalog.hpp"
#include <map>
#include <regex>
namespace lp {
struct CatalogEdit {std::string name;int category=-1;bool deleted=false;};
// Portable catalog identities; automatic durable preferences independent of shows.
class CatalogEdits {
 std::map<std::string,CatalogEdit> records;
public:
 const auto& entries()const{return records;}
 void load(const std::filesystem::path& file){
  if(!std::filesystem::exists(file)){records.clear();return;}
  if(std::filesystem::file_size(file)>16*1024*1024)throw std::runtime_error("Catálogo de alterações muito grande");
  std::ifstream in(file);std::string header,line;std::map<std::string,CatalogEdit> next;
  if(!std::getline(in,header)||header!="LPHS_CATALOG_EDITS 1")throw std::runtime_error("Catálogo de alterações inválido");
  while(std::getline(in,line)){if(line.empty())continue;std::istringstream row(line);std::string key,extra;CatalogEdit e;
   if(!(row>>std::quoted(key)>>std::quoted(e.name)>>e.category>>e.deleted)||(row>>extra)||key.empty()||key.size()>32768||e.name.size()>1024||e.category < -1||e.category>14||e.category==13)throw std::runtime_error("Alteração de timbre inválida");
   next[key]=e;
  }
  if(in.bad())throw std::runtime_error("Falha na leitura das alterações");records=std::move(next);
 }
 CatalogEdit get(const std::string& key)const{auto i=records.find(key);return i==records.end()?CatalogEdit{}:i->second;}
 void set(const std::string& key,CatalogEdit edit,const std::filesystem::path& file){
  if(key.empty()||edit.name.size()>1024||edit.category < -1||edit.category>14||edit.category==13)throw std::invalid_argument("Alteração inválida");
  auto next=records;next[key]=std::move(edit);std::ostringstream out;out<<"LPHS_CATALOG_EDITS 1\n";
  for(const auto& [k,e]:next)out<<std::quoted(k)<<' '<<std::quoted(e.name)<<' '<<e.category<<' '<<e.deleted<<'\n';
  atomicWrite(file,out.str());records=std::move(next);
 }
 std::set<std::string> deletedBankIds()const{std::set<std::string> ids;for(const auto& [key,e]:records)if(e.deleted&&key.rfind("bank://",0)==0)ids.insert(key.substr(7));return ids;}
 void apply(Instrument& p,const std::filesystem::path& base)const{auto e=get(soundExclusionKey(instrumentReference(p),base));p.customName=e.name;if(e.category>=0)p.category=static_cast<unsigned>(e.category);}
};
// Only remove a standalone SF2. Its real preset inventory must be checked by the caller.
inline std::filesystem::path soundSource(const Instrument& p){if(auto sf2=parseSf2Reference(p.sfz.u8string()))return sf2->path;return p.sfz;}
inline void ensureUnsharedDefinition(const std::filesystem::path& source,const std::filesystem::path& base,const std::vector<Instrument>& bank){
 auto target=std::filesystem::weakly_canonical(source);
 unsigned references=0;for(const auto& p:bank)if(std::filesystem::weakly_canonical(soundSource(p))==target)++references;
 if(references>1)throw std::runtime_error("Arquivo compartilhado por outros timbres; use Ocultar");
 if(source.extension()!=".sfz"&&source.extension()!=".SFZ")return;
 std::set<std::filesystem::path> roots{base,source.parent_path()},visited;
 for(const auto& p:bank)if(!parseSf2Reference(p.sfz.u8string()))roots.insert(p.sfz.parent_path());
 const std::regex include(R"rx(#include\s+"([^"]+)")rx");
 for(const auto& root:roots){if(!std::filesystem::is_directory(root))continue;
  for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){
   if(!entry.is_regular_file())continue;auto ext=entry.path().extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
   if(ext!=".sfz"&&ext!=".sfh"&&ext!=".inc")continue;
   auto file=std::filesystem::weakly_canonical(entry.path());if(file==target||!visited.insert(file).second)continue;
   if(std::filesystem::file_size(file)>16*1024*1024)throw std::runtime_error("Definição SFZ muito grande para verificar compartilhamento");
   std::ifstream in(file);if(!in)throw std::runtime_error("Não foi possível verificar os arquivos compartilhados");std::string line;
   while(std::getline(in,line)){auto at=line.find("//");if(at!=std::string::npos)line.resize(at);if(line.find("#include")==std::string::npos)continue;
    std::smatch match;if(!std::regex_search(line,match,include)||match[1].str().find('$')!=std::string::npos)throw std::runtime_error("SFZ com inclusão dinâmica; não é possível garantir exclusão isolada. Use Ocultar.");
    auto text=match[1].str();std::replace(text.begin(),text.end(),'\\','/');
    if(std::filesystem::weakly_canonical(file.parent_path()/std::filesystem::u8path(text))==target)throw std::runtime_error("Esta definição é compartilhada por outro SFZ. Use Ocultar.");
   }
   if(in.bad())throw std::runtime_error("Falha na leitura de arquivos compartilhados");
  }
 }
}
inline void deleteSoundFile(const std::filesystem::path& path){
 if(std::filesystem::is_symlink(std::filesystem::symlink_status(path)))throw std::runtime_error("Não é permitido deletar timbre por link simbólico");
 auto ext=path.extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 if(ext!=".sf2"&&ext!=".sfz")throw std::runtime_error("Arquivo de timbre inválido");
 if(!std::filesystem::is_regular_file(path)||!std::filesystem::remove(path))throw std::runtime_error("Não foi possível apagar o arquivo de origem");
}
}

#pragma once
#include "session.hpp"
#include "sf2_reference.hpp"
#include <functional>
#include <cwctype>
namespace lp {
inline std::filesystem::path referencePath(const std::string& reference){
 if(reference.rfind("bank://",0)==0||reference.rfind("performance://",0)==0)return {};
 if(auto sf2=parseSf2Reference(reference))return sf2->path;
 return std::filesystem::u8path(reference);
}
inline std::string withReferencePath(const std::string& reference,const std::filesystem::path& path){
 if(auto sf2=parseSf2Reference(reference))return "sf2://"+std::to_string(sf2->bank)+":"+std::to_string(sf2->program)+":"+path.generic_u8string();
 return path.generic_u8string();
}
inline void mapProfilePaths(Session& session,const std::function<std::string(const std::string&)>& convert){
 for(auto& favorite:session.favorites)if(favorite)favorite->sfz=convert(favorite->sfz);
 for(auto& performance:session.performanceLibrary)for(auto& part:performance.parts)if(part.enabled)part.instrument=convert(part.instrument);
}
inline bool safeRelative(const std::filesystem::path& relative){
 if(relative.empty()||relative.is_absolute()||relative.has_root_name())return false;
 for(const auto& part:relative)if(part=="..")return false;
 return true;
}
inline std::string portableReference(const std::string& reference,const std::filesystem::path& app,const std::filesystem::path& profile){
 auto path=referencePath(reference);if(path.empty())return reference;
 auto absolute=std::filesystem::absolute(path).lexically_normal();
 for(const auto& root:std::array<std::pair<std::filesystem::path,const char*>,2>{{{app,"app-path://"},{profile,"profile-path://"}}}){
  auto relative=absolute.lexically_relative(std::filesystem::absolute(root.first).lexically_normal());
  if(safeRelative(relative))return withReferencePath(reference,std::filesystem::u8path(std::string(root.second)+relative.generic_u8string()));
 }
 return reference;
}
inline std::string expandedReference(const std::string& reference,const std::filesystem::path& app,const std::filesystem::path& profile){
 auto path=referencePath(reference);if(path.empty())return reference;
 auto value=path.generic_u8string(); // filesystem may normalize // in a path URI.
 for(const auto& root:std::array<std::pair<const char*,std::filesystem::path>,2>{{{"app-path:",app},{"profile-path:",profile}}}){
  if(value.rfind(root.first,0)!=0)continue;
  auto rest=value.substr(std::char_traits<char>::length(root.first));while(!rest.empty()&&rest.front()=='/')rest.erase(0,1);
  auto relative=std::filesystem::u8path(rest);if(!safeRelative(relative))throw std::runtime_error("Caminho relativo inválido no perfil");
  return withReferencePath(reference,std::filesystem::absolute(root.second/relative).lexically_normal());
 }
 return reference;
}
inline bool matchesSearch(std::wstring text,std::wstring query){
 auto fold=[](std::wstring& s){for(auto& c:s){if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
  const std::wstring accents=L"áàâãäéèêëíìîïóòôõöúùûüç",plain=L"aaaaaeeeeiiiiooooouuuuc";const std::wstring upper=L"ÁÀÂÃÄÉÈÊËÍÌÎÏÓÒÔÕÖÚÙÛÜÇ";auto i=accents.find(c);if(i==std::wstring::npos)i=upper.find(c);if(i!=std::wstring::npos)c=plain[i];}};
 fold(text);fold(query);return text.find(query)!=std::wstring::npos;
}
}

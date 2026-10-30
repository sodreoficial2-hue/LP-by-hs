#pragma once
#include "recoverable_file.hpp"
#include <map>
#include <set>
#include <regex>
#include <cctype>
#include <functional>
namespace lp {
inline std::string sfzDependencyStamp(const std::filesystem::path& root){
 std::uint64_t hash=1469598103934665603ull;std::set<std::filesystem::path> visited,dependencies;std::map<std::string,std::string> macros;std::filesystem::path defaultPath;bool dynamic=false;
 auto mix=[&](const std::string& value){for(unsigned char c:value){hash^=c;hash*=1099511628211ull;}};
 auto trim=[](std::string text){auto a=text.find_first_not_of(" \t\r\n\"");if(a==text.npos)return std::string{};return text.substr(a,text.find_last_not_of(" \t\r\n\"")-a+1);};
 auto expand=[&](std::string text){for(int pass=0;pass<16&&text.find('$')!=text.npos;++pass){bool changed=false;for(const auto& [key,value]:macros){std::size_t at=0;while((at=text.find(key,at))!=text.npos){auto end=at+key.size();if(end<text.size()&&(std::isalnum(static_cast<unsigned char>(text[end]))||text[end]=='_')){at=end;continue;}text.replace(at,key.size(),value);at+=value.size();changed=true;}}if(!changed)break;}if(text.find('$')!=text.npos)dynamic=true;std::replace(text.begin(),text.end(),'\\','/');return trim(text);};
 std::function<void(std::filesystem::path,int)> inspect;
 inspect=[&](std::filesystem::path file,int depth){file=std::filesystem::absolute(file).lexically_normal();dependencies.insert(file);if(!visited.insert(file).second)return;if(depth>32||visited.size()>4096)throw std::runtime_error("Dependências SFZ excedem o limite");if(!std::filesystem::is_regular_file(file))return;
  std::istringstream in(boundedFile(file));std::string line;static const std::regex define(R"(^\s*#define\s+(\$\w+)\s+(.+)$)"),include(R"rx(#include\s+"([^"]+)")rx"),opcode(R"((sample|default_path)\s*=\s*(.*?)(?=\s+[A-Za-z_]\w*\s*=|<|$))");
  while(std::getline(in,line)){auto comment=line.find("//");if(comment!=line.npos)line.resize(comment);std::smatch match;if(std::regex_search(line,match,define)){macros[match[1]]=trim(match[2]);continue;}
   for(auto it=std::sregex_iterator(line.begin(),line.end(),include);it!=std::sregex_iterator();++it)inspect(file.parent_path()/std::filesystem::u8path(expand((*it)[1])),depth+1);
   for(auto it=std::sregex_iterator(line.begin(),line.end(),opcode);it!=std::sregex_iterator();++it){auto value=expand((*it)[2]);if((*it)[1]=="default_path"){defaultPath=std::filesystem::u8path(value);continue;}if(value.empty()||value[0]=='*')continue;auto sample=std::filesystem::u8path(value);dependencies.insert(std::filesystem::absolute(root.parent_path()/defaultPath/sample).lexically_normal());dependencies.insert(std::filesystem::absolute(file.parent_path()/defaultPath/sample).lexically_normal());}
  }
 };
 inspect(root,0);
 if(dynamic)for(auto it=std::filesystem::recursive_directory_iterator(root.parent_path(),std::filesystem::directory_options::skip_permission_denied);it!=std::filesystem::recursive_directory_iterator();++it){if(it.depth()>12){it.disable_recursion_pending();continue;}if(it->is_regular_file())dependencies.insert(it->path());if(dependencies.size()>100000)throw std::runtime_error("Biblioteca SFZ dinâmica excede o limite");}
 for(const auto& file:dependencies){mix(file.generic_u8string());std::error_code ec;auto size=std::filesystem::file_size(file,ec);if(ec){mix("missing");continue;}mix(std::to_string(size));auto time=std::filesystem::last_write_time(file,ec);mix(ec?"unknown":std::to_string(time.time_since_epoch().count()));}
 return "sfz-dependencies-2:"+std::to_string(hash);
}
}

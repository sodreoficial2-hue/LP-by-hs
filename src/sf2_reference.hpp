#pragma once
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
namespace lp {
struct Sf2Reference {std::filesystem::path path;int bank=0,program=0;};
inline std::string makeSf2Reference(const std::filesystem::path& path,int bank,int program){
 if(bank<0||bank>16383||program<0||program>127)throw std::invalid_argument("Invalid SF2 preset");
 return "sf2://"+std::to_string(bank)+":"+std::to_string(program)+":"+std::filesystem::absolute(path).u8string();
}
inline std::optional<Sf2Reference> parseSf2Reference(const std::string& value){
 if(value.rfind("sf2://",0)!=0)return std::nullopt;
 auto start=std::size_t(6),colon=value.find(':',start);
 if(colon==std::string::npos)throw std::runtime_error("Invalid SF2 reference");
 auto second=value.find(':',colon+1);
 if(second==std::string::npos)throw std::runtime_error("Invalid SF2 reference");
 auto number=[](const std::string& v,int max){if(v.empty()||v.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid SF2 number");int n=std::stoi(v);if(n>max)throw std::runtime_error("Invalid SF2 number");return n;};
 Sf2Reference ref;ref.bank=number(value.substr(start,colon-start),16383);ref.program=number(value.substr(colon+1,second-colon-1),127);
 ref.path=std::filesystem::u8path(value.substr(second+1));
 if(ref.path.empty())throw std::runtime_error("Empty SF2 path");
 return ref;
}
}

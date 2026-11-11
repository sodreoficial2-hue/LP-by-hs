#pragma once
#include "sf2_reference.hpp"
namespace lp {
// Resolve on the owner thread before publishing load work. Preserve SF2 preset
// identity and never interpret catalog/performance IDs as filesystem paths.
inline std::string normalizedSoundReference(const std::string& reference,const std::filesystem::path& base){
 if(reference.empty()||reference.rfind("bank://",0)==0||reference.rfind("performance://",0)==0)return reference;
 auto sf2=parseSf2Reference(reference);auto path=sf2?sf2->path:std::filesystem::u8path(reference);
 if(path.is_relative()&&!base.empty())path=base/path;
 path=std::filesystem::absolute(path).lexically_normal();
 return sf2?makeSf2Reference(path,sf2->bank,sf2->program):path.u8string();
}
inline std::string portableSoundReference(const std::string& reference,const std::filesystem::path& base){
 auto normalized=normalizedSoundReference(reference,base);
 if(base.empty()||normalized.empty()||normalized.rfind("bank://",0)==0||normalized.rfind("performance://",0)==0)return normalized;
 auto sf2=parseSf2Reference(normalized);auto path=sf2?sf2->path:std::filesystem::u8path(normalized);
 auto relative=path.lexically_relative(std::filesystem::absolute(base).lexically_normal());
 if(relative.empty()||relative.is_absolute()||relative.has_root_name())return normalized;
 for(const auto& part:relative)if(part=="..")return normalized;
 return sf2?"sf2://"+std::to_string(sf2->bank)+":"+std::to_string(sf2->program)+":"+relative.generic_u8string():relative.generic_u8string();
}
}

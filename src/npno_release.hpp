#pragma once
// Migrate generated NPNO releases to pedal-up-only and update their controls.
// Original PCM, attack/pedal regions and unknown metadata fields are preserved.
#include "sfz_conversion.hpp"
#include <fstream>
#include <regex>
#include <optional>
#include <iomanip>
#ifndef PICOJSON_USE_INT64
#define PICOJSON_USE_INT64
#endif
#include "../third_party/picojson/picojson.h"
namespace lp {
namespace release_detail {
inline std::string read(const std::filesystem::path& path){
 if(std::filesystem::file_size(path)>64u*1024u*1024u)throw std::runtime_error("Sidecar de soltura excede limite");
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Não foi possível ler controles de soltura");
 return {std::istreambuf_iterator<char>(file),{}};
}
using Json=picojson::value;
inline Json parse(const std::string& text){
 Json value;std::string error;auto end=picojson::parse(value,text.begin(),text.end(),&error);
 while(end!=text.end()&&std::isspace(static_cast<unsigned char>(*end)))++end;
 if(!error.empty()||end!=text.end())throw std::runtime_error("JSON de soltura inválido: "+error);
 return value;
}
inline Json object(const std::string& text){auto value=parse(text);if(!value.is<picojson::object>())throw std::runtime_error("Objeto JSON de soltura esperado");return value;}
inline std::optional<std::string> field(const std::string& text,const std::string& name){
 auto value=object(text);const auto& fields=value.get<picojson::object>();auto it=fields.find(name);
 if(it==fields.end())return {};if(!it->second.is<double>()&&!it->second.is<bool>())throw std::runtime_error("Tipo inválido no sidecar: "+name);return it->second.serialize();
}
inline double number(const Json& value,const std::string& name,double fallback){
 const auto& fields=value.get<picojson::object>();auto it=fields.find(name);if(it==fields.end())return fallback;
 if(!it->second.is<double>()||!std::isfinite(it->second.get<double>()))throw std::runtime_error("Valor inválido no sidecar: "+name);return it->second.get<double>();
}
inline bool flag(const Json& value,const std::string& name,bool fallback=false){
 const auto& fields=value.get<picojson::object>();auto it=fields.find(name);if(it==fields.end())return fallback;
 if(!it->second.is<bool>())throw std::runtime_error("Tipo inválido no sidecar: "+name);return it->second.get<bool>();
}
inline double number(const std::string& text,const std::string& name,double fallback){return number(object(text),name,fallback);}
inline std::string numeric(double value){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<value;return out.str();}
inline void set(std::string& text,const std::string& name,const std::string& value){
 auto json=object(text);json.get<picojson::object>()[name]=parse(value);text=json.serialize();
}
inline bool releaseRegion(const std::string& line){
 // Generated SFZ is one region per line. Ignore comments and require a whole
 // opcode value; filenames/comments containing trigger=release are not regions.
 auto comment=line.find("//");auto code=line.substr(0,comment);
 static const std::regex expression(R"((^|[ \t])trigger\s*=\s*(release|release_key)(?=[ \t\r]|$))");
 return code.find("<region>")!=code.npos&&std::regex_search(code,expression);
}
inline std::string digest(const std::string& bytes){return sha256Bytes(std::vector<std::uint8_t>(bytes.begin(),bytes.end()));}
inline std::filesystem::path journalPath(const std::filesystem::path& original){auto path=original;path+=".release-pending.json";return path;}
inline const std::vector<std::string>& suffixes(){static const std::vector<std::string> value={".release-base.txt",".converted.sfz",".meta.json",".audit.json"};return value;}
inline bool recover(const std::filesystem::path& original){
 auto path=journalPath(original);if(!std::filesystem::exists(path))return false;
 // No target file is touched until the complete durable intent is validated.
 auto journal=object(read(path));if(number(journal,"version",0)!=1)throw std::runtime_error("Transação de soltura desconhecida");
 auto files=journal.get("files");if(!files.is<picojson::object>())throw std::runtime_error("Transação de soltura incompleta");
 const auto& entries=files.get<picojson::object>();if(entries.size()<3||entries.size()>4)throw std::runtime_error("Transação de soltura incompleta");
 for(const auto& entry:entries){
  if(std::find(suffixes().begin(),suffixes().end(),entry.first)==suffixes().end()||!entry.second.is<picojson::object>())throw std::runtime_error("Destino de soltura inválido");
  const auto& bytes=entry.second.get("bytes");const auto& hash=entry.second.get("sha256");
  if(!bytes.is<std::string>()||!hash.is<std::string>()||digest(bytes.get<std::string>())!=hash.get<std::string>())throw std::runtime_error("Transação de soltura corrompida; banco preservado");
  if(entry.first==".meta.json"||entry.first==".audit.json")object(bytes.get<std::string>());
 }
 for(const auto& suffix:suffixes())if(suffix!=".audit.json"&&!entries.count(suffix))throw std::runtime_error("Transação de soltura incompleta");
 for(const auto& suffix:suffixes()){auto it=entries.find(suffix);if(it==entries.end())continue;auto target=original;target+=suffix;const auto& bytes=it->second.get("bytes").get<std::string>();atomicWrite(target,bytes);if(read(target)!=bytes)throw std::runtime_error("Falha ao verificar transação de soltura");}
 std::filesystem::remove(path);
 syncParentDirectory(path);return true;
}
inline void commit(const std::filesystem::path& original,const std::string& baseline,const std::string& sfz,const std::string& meta,const std::string& audit){
 picojson::object files;std::vector<std::string> content={baseline,sfz,meta,audit};
 for(std::size_t i=0;i<content.size();++i){if(i==3&&audit.empty())continue;picojson::object entry;entry["bytes"]=Json(content[i]);entry["sha256"]=Json(digest(content[i]));files[suffixes()[i]]=Json(entry);}
 picojson::object journal;journal["version"]=Json(1.0);journal["files"]=Json(files);
 auto bytes=Json(journal).serialize();if(bytes.size()>64u*1024u*1024u)throw std::runtime_error("Transação de soltura excede limite");
 // A failed/interrupted commit keeps this intent; startup rolls it forward.
 atomicWrite(journalPath(original),bytes);recover(original);
}
inline void opcode(std::string& line,const std::string& name,double value){
 std::smatch match;std::regex expression("(^|[ \t])"+name+"=([-+0-9.eE]+)");
 if(!std::regex_search(line,match,expression))throw std::runtime_error("Controle SFZ de soltura ausente: "+name);
 line.replace(match.position(2),match.length(2),numeric(value));
}
inline double opcodeValue(const std::string& line,const std::string& name){
 std::smatch match;std::regex expression("(^|[ \t])"+name+"=([-+0-9.eE]+)");
 if(!std::regex_search(line,match,expression))throw std::runtime_error("Controle SFZ de soltura ausente: "+name);
 std::istringstream input(match[2].str());input.imbue(std::locale::classic());double value;if(!(input>>value)||!std::isfinite(value))throw std::runtime_error("Ganho SFZ inválido");return value;
}
}
inline bool updateNpnoRelease(std::filesystem::path original,double gain,bool enabled){
 if(!std::isfinite(gain)||gain< -24||gain>0)throw std::runtime_error("Intensidade de soltura deve estar entre -24 e 0 dB");
 auto suffix=std::string(".converted.sfz");auto name=original.u8string();
 if(name.size()>suffix.size()&&name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)original=std::filesystem::u8path(name.substr(0,name.size()-suffix.size()));
 auto extension=timbreExtension(original);if(extension!=".npno"&&extension!=".npmo")return false;
 bool recovered=release_detail::recover(original);
 auto metadata=original;metadata+=".meta.json";if(!std::filesystem::exists(metadata))return false;
 auto oldMeta=release_detail::read(metadata);
 auto meta=release_detail::object(oldMeta);
 if(!meta.get("format").is<std::string>()||meta.get("format").get<std::string>()!="npno")throw std::runtime_error("Sidecar não pertence ao importador NPNO");
 auto& fields=meta.get<picojson::object>();
 if(release_detail::number(meta,"pedal_release_revision",0)==2&&fields.count("release_key_gain_db")&&fields.count("release_key_enabled")&&release_detail::number(meta,"release_key_gain_db",0)==gain&&release_detail::flag(meta,"release_key_enabled")==enabled)return recovered;
 auto sfz=derivedSfz(original);auto oldSfz=release_detail::read(sfz);auto templatePath=original;templatePath+=".release-base.txt";
 auto baseline=std::filesystem::exists(templatePath)?release_detail::read(templatePath):oldSfz;
 auto isRelease=release_detail::releaseRegion;
 std::vector<std::string> releaseLines;std::istringstream templateInput(baseline);std::string line;
 while(std::getline(templateInput,line))if(isRelease(line))releaseLines.push_back(line);
 // Old templates contain an unpedaled bank followed by a duplicate passthrough.
 // Select only the former; current templates contain one pedal-only bank.
 static const std::regex keyTrigger(R"((^|[ \t])trigger\s*=\s*release_key(?=[ \t\r]|$))");
 bool oldTemplate=std::any_of(releaseLines.begin(),releaseLines.end(),[](const auto& region){return std::regex_search(region,keyTrigger);});
 if(oldTemplate)releaseLines.erase(std::remove_if(releaseLines.begin(),releaseLines.end(),[](const auto& r){return r.find("locc64=64")!=std::string::npos;}),releaseLines.end());
 auto zones=fields.find("zones");
 if(zones==fields.end()||!zones->second.is<picojson::array>())throw std::runtime_error("Zonas de soltura ausentes; banco preservado");
 picojson::array objects;std::size_t zone=0;std::string auditRegions="[";
 for(auto entry:zones->second.get<picojson::array>()){
  if(!entry.is<picojson::object>()||!entry.get("sample").is<std::string>())throw std::runtime_error("Zona de soltura inválida; banco preservado");
  if(release_detail::flag(entry,"release_trigger")){
   if(release_detail::flag(entry,"release_key_pedal_passthrough"))continue;
   if(zone>=releaseLines.size())throw std::runtime_error("Template de soltura incompleto; banco preservado");
   double base=release_detail::number(entry,"release_key_base_volume_db",release_detail::number(entry,"volume_db",0));
   double raw=release_detail::number(entry,"source_loop_decay_q23",0);if(raw<0||raw>4294967295.0)throw std::runtime_error("Q23 de soltura inválido");
   auto seconds=npnoLoopDecaySeconds(static_cast<std::uint32_t>(raw));seconds=seconds<0?.25:std::clamp(seconds,.12,.45);
   auto& zoneFields=entry.get<picojson::object>();
   zoneFields["pedal_release_only"]=release_detail::Json(true);zoneFields["release_key_pedal_passthrough"]=release_detail::Json(false);
   zoneFields["cc64_low"]=release_detail::Json(64.0);zoneFields["cc64_high"]=release_detail::Json(-1.0);
   zoneFields["release_key_base_volume_db"]=release_detail::Json(base);
   zoneFields["volume_db"]=release_detail::Json(std::clamp(base+gain,-144.0,10.8));
   zoneFields["release_seconds"]=release_detail::Json(seconds);zoneFields["muted"]=release_detail::Json(!enabled);
   auto& region=releaseLines[zone++];
   region=std::regex_replace(region,keyTrigger,"$1trigger=release");
   region=std::regex_replace(region,std::regex("[ \t]+(?:locc64|hicc64)=[-+0-9]+"),"");region=std::regex_replace(region,std::regex("[ \t]+sustain_lo=[-+0-9.]+"),"");region+=" locc64=64 sustain_lo=64";
   release_detail::opcode(region,"volume",std::clamp(release_detail::opcodeValue(region,"volume")+gain,-144.0,10.8));
   release_detail::opcode(region,"ampeg_release",seconds);
   if(auditRegions.size()>1)auditRegions+=',';
   auditRegions+="{\"zone\":"+std::to_string(objects.size())+",\"ampeg_release_seconds\":"+release_detail::numeric(seconds)+",\"release_key_gain_db\":"+release_detail::numeric(gain)+",\"muted\":"+(enabled?"false":"true")+",\"pedal_release_only\":true}";
  }
  objects.push_back(std::move(entry));
 }
 auditRegions+=']';
 if(zone!=releaseLines.size())throw std::runtime_error("Template e metadados de soltura divergem; banco preservado");
 fields["zones"]=release_detail::Json(objects);fields["release_key_gain_db"]=release_detail::Json(gain);fields["release_key_enabled"]=release_detail::Json(enabled);fields["pedal_release_revision"]=release_detail::Json(2.0);
 auto updatedMeta=meta.serialize();
 std::istringstream current(oldSfz);std::string updatedSfz;bool inserted=false;
 while(std::getline(current,line)){
  if(isRelease(line))continue;
  updatedSfz+=line+'\n';
  if(!inserted&&line.find("// source_bank=2;")!=std::string::npos){inserted=true;if(enabled)for(const auto& region:releaseLines)updatedSfz+=region+'\n';}
 }
 if(zone&&!inserted)throw std::runtime_error("Grupo SFZ de soltura ausente; banco preservado");
 auto audit=original;audit+=".audit.json";std::string newAudit;
 if(std::filesystem::exists(audit)){
  auto value=release_detail::object(release_detail::read(audit));auto& report=value.get<picojson::object>();
  report["pedal_release_revision"]=release_detail::Json(2.0);report["regions_after_calibration_policy"]=release_detail::Json(double(objects.size()));
  report["release_key_gain_db"]=release_detail::Json(gain);report["release_key_enabled"]=release_detail::Json(enabled);
  report["release_key_regions"]=release_detail::parse(auditRegions);newAudit=value.serialize();
 }
 release_detail::object(updatedMeta);if(!newAudit.empty())release_detail::object(newAudit);
 release_detail::commit(original,baseline,updatedSfz,updatedMeta,newAudit);
 return true;
}
}

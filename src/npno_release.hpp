#pragma once
// Update only release_key controls in banks produced by this converter.
// Original PCM, attack/pedal lines and unknown metadata fields are preserved.
#include "sfz_conversion.hpp"
#include <fstream>
#include <regex>
#include <optional>
#include <iomanip>
namespace lp {
namespace release_detail {
inline std::string read(const std::filesystem::path& path){
 if(std::filesystem::file_size(path)>64u*1024u*1024u)throw std::runtime_error("Sidecar de soltura excede limite");
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Não foi possível ler controles de soltura");
 return {std::istreambuf_iterator<char>(file),{}};
}
inline std::optional<std::string> field(const std::string& text,const std::string& name){
 std::smatch match;std::regex expression("\""+name+"\"\\s*:\\s*([-+0-9.eE]+|true|false)");
 if(!std::regex_search(text,match,expression))return {};return match[1].str();
}
inline double number(const std::string& text,const std::string& name,double fallback){
 auto value=field(text,name);if(!value)return fallback;std::istringstream input(*value);input.imbue(std::locale::classic());double result;
 if(!(input>>result)||!std::isfinite(result))throw std::runtime_error("Valor inválido no sidecar: "+name);return result;
}
inline std::string numeric(double value){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<value;return out.str();}
inline void set(std::string& text,const std::string& name,const std::string& value){
 std::smatch match;std::regex expression("\""+name+"\"\\s*:\\s*([-+0-9.eE]+|true|false)");
 if(std::regex_search(text,match,expression))text.replace(match.position(1),match.length(1),value);
 else {auto end=text.rfind('}');if(end==text.npos)throw std::runtime_error("Sidecar de soltura inválido");text.insert(end,", \""+name+"\": "+value);}
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
 auto metadata=original;metadata+=".meta.json";if(!std::filesystem::exists(metadata))return false;
 auto oldMeta=release_detail::read(metadata);
 if(oldMeta.find("\"format\": \"npno\"")==std::string::npos)throw std::runtime_error("Sidecar não pertence ao importador NPNO");
 auto oldGain=release_detail::field(oldMeta,"release_key_gain_db"),oldEnabled=release_detail::field(oldMeta,"release_key_enabled");
 if((oldMeta.find("\"release_key_pedal_passthrough\"")!=std::string::npos||oldMeta.find("\"release_trigger\": true")==std::string::npos)&&oldGain&&oldEnabled&&release_detail::number(oldMeta,"release_key_gain_db",0)==gain&&(*oldEnabled=="true")==enabled)return false;
 auto sfz=derivedSfz(original);auto oldSfz=release_detail::read(sfz);auto templatePath=original;templatePath+=".release-base.txt";
 auto baseline=std::filesystem::exists(templatePath)?release_detail::read(templatePath):oldSfz;
 std::vector<std::string> releaseLines;std::istringstream templateInput(baseline);std::string line;
 while(std::getline(templateInput,line))if(line.find("trigger=release_key")!=std::string::npos&&line.find("locc64=64")==std::string::npos)releaseLines.push_back(line);
 std::string updatedMeta;std::istringstream input(oldMeta);std::size_t zone=0,zoneIndex=0;std::string auditRegions="[";std::vector<std::string> migratedPedalLines;
 auto appendAudit=[&](const std::string& object,std::size_t index){if(release_detail::field(object,"release_trigger")!=std::optional<std::string>("true"))return;
  if(auditRegions.size()>1)auditRegions+=',';
  bool passthrough=release_detail::field(object,"release_key_pedal_passthrough")==std::optional<std::string>("true"),muted=release_detail::field(object,"muted")==std::optional<std::string>("true");
  auditRegions+="{\"zone\":"+std::to_string(index)+",\"ampeg_release_seconds\":"+release_detail::numeric(release_detail::number(object,"release_seconds",.25))+",\"release_key_gain_db\":"+release_detail::numeric(passthrough?0:gain)+",\"muted\":"+(muted?"true":"false")+"}";
 };
 while(std::getline(input,line)){
  // Canonical sidecars emit exactly one flat object per converted zone.
  if(line.find("{\"sample\":")!=std::string::npos&&release_detail::field(line,"release_trigger")==std::optional<std::string>("true")&&release_detail::field(line,"release_key_pedal_passthrough")!=std::optional<std::string>("true")){
   if(zone>=releaseLines.size())throw std::runtime_error("Template de soltura incompleto; banco preservado");
   bool legacy=release_detail::number(line,"cc64_low",-1)<64&&release_detail::number(line,"cc64_high",-1)!=63;
   std::string pedalLine=line;
   double base=release_detail::number(line,"release_key_base_volume_db",release_detail::number(line,"volume_db",0));
   double raw=release_detail::number(line,"source_loop_decay_q23",0);if(raw<0||raw>4294967295.0)throw std::runtime_error("Q23 de soltura inválido");auto seconds=npnoLoopDecaySeconds(static_cast<std::uint32_t>(raw));seconds=seconds<0?.25:std::clamp(seconds,.12,.45);
   release_detail::set(line,"release_key_pedal_passthrough","false");release_detail::set(line,"cc64_low","-1");release_detail::set(line,"cc64_high","63");
   release_detail::set(line,"release_key_base_volume_db",release_detail::numeric(base));
   release_detail::set(line,"volume_db",release_detail::numeric(std::clamp(base+gain,-144.0,10.8)));
   release_detail::set(line,"release_seconds",release_detail::numeric(seconds));release_detail::set(line,"muted",enabled?"false":"true");
   auto& region=releaseLines[zone++];
   if(legacy){
    auto passthrough=region+" locc64=64";migratedPedalLines.push_back(passthrough);region+=" hicc64=63";
    release_detail::set(pedalLine,"release_key_pedal_passthrough","true");release_detail::set(pedalLine,"release_key_base_volume_db",release_detail::numeric(base));release_detail::set(pedalLine,"volume_db",release_detail::numeric(base));release_detail::set(pedalLine,"cc64_low","64");release_detail::set(pedalLine,"cc64_high","-1");release_detail::set(pedalLine,"release_seconds","0.2");release_detail::set(pedalLine,"muted","false");
    auto comma=line.find_last_not_of(" \t");bool hadComma=comma!=line.npos&&line[comma]==',';if(hadComma)line.erase(comma,1);auto pedalComma=pedalLine.find_last_not_of(" \t");if(pedalComma!=pedalLine.npos&&pedalLine[pedalComma]==',')pedalLine.erase(pedalComma,1);
    updatedMeta+=line+",\n";appendAudit(line,zoneIndex);line=pedalLine+(hadComma?",":"");++zoneIndex;
   }
   release_detail::opcode(region,"volume",std::clamp(release_detail::opcodeValue(region,"volume")+gain,-144.0,10.8));release_detail::opcode(region,"ampeg_release",seconds);
  }
  if(line.find("{\"sample\":")!=std::string::npos){appendAudit(line,zoneIndex);++zoneIndex;}
  updatedMeta+=line+'\n';
 }
 auditRegions+=']';
 if(zone!=releaseLines.size())throw std::runtime_error("Template e metadados de soltura divergem; banco preservado");
 release_detail::set(updatedMeta,"release_key_gain_db",release_detail::numeric(gain));release_detail::set(updatedMeta,"release_key_enabled",enabled?"true":"false");
 std::istringstream current(oldSfz);std::string updatedSfz;bool inserted=false;
 while(std::getline(current,line)){
  if(line.find("trigger=release_key")!=std::string::npos&&line.find("locc64=64")==std::string::npos)continue;
  updatedSfz+=line+'\n';
  if(!inserted&&line.find("// source_bank=2;")!=std::string::npos){inserted=true;if(enabled)for(const auto& region:releaseLines)updatedSfz+=region+'\n';for(const auto& region:migratedPedalLines)updatedSfz+=region+'\n';}
 }
 if(zone&&!inserted)throw std::runtime_error("Grupo SFZ de soltura ausente; banco preservado");
 auto audit=original;audit+=".audit.json";std::string oldAudit,newAudit;
 if(std::filesystem::exists(audit)){oldAudit=release_detail::read(audit);newAudit=oldAudit;release_detail::set(newAudit,"release_key_gain_db",release_detail::numeric(gain));release_detail::set(newAudit,"release_key_enabled",enabled?"true":"false");
  std::regex listExpression("\"release_key_regions\"\\s*:\\s*\\[[^\\]]*\\]");std::smatch match;auto fieldText="\"release_key_regions\":"+auditRegions;
  if(std::regex_search(newAudit,match,listExpression))newAudit.replace(match.position(),match.length(),fieldText);else{auto end=newAudit.rfind('}');newAudit.insert(end,", "+fieldText);}
 }
 if(!std::filesystem::exists(templatePath))atomicWrite(templatePath,baseline);
 try{atomicWrite(sfz,updatedSfz);atomicWrite(metadata,updatedMeta);if(!newAudit.empty())atomicWrite(audit,newAudit);}
 catch(...){atomicWrite(sfz,oldSfz);atomicWrite(metadata,oldMeta);if(!oldAudit.empty())atomicWrite(audit,oldAudit);throw;}
 return true;
}
}

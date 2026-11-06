#pragma once
// Migrate generated NPNO releases to pedal-up-only and update their controls.
// Original PCM, attack/pedal regions and unknown metadata fields are preserved.
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
 if(release_detail::field(oldMeta,"pedal_release_revision")==std::optional<std::string>("2")&&oldGain&&oldEnabled&&release_detail::number(oldMeta,"release_key_gain_db",0)==gain&&(*oldEnabled=="true")==enabled)return false;
 auto sfz=derivedSfz(original);auto oldSfz=release_detail::read(sfz);auto templatePath=original;templatePath+=".release-base.txt";
 auto baseline=std::filesystem::exists(templatePath)?release_detail::read(templatePath):oldSfz;
 auto isRelease=[](const std::string& line){return line.find("trigger=release")!=std::string::npos;};
 std::vector<std::string> releaseLines;std::istringstream templateInput(baseline);std::string line;
 while(std::getline(templateInput,line))if(isRelease(line))releaseLines.push_back(line);
 // Old templates contain an unpedaled bank followed by a duplicate passthrough.
 // Select only the former; current templates contain one pedal-only bank.
 bool oldTemplate=baseline.find("trigger=release_key")!=std::string::npos;
 if(oldTemplate)releaseLines.erase(std::remove_if(releaseLines.begin(),releaseLines.end(),[](const auto& r){return r.find("locc64=64")!=std::string::npos;}),releaseLines.end());
 auto begin=oldMeta.find("\"zones\": ["),end=oldMeta.find("\n  ]",begin);
 if(begin==oldMeta.npos||end==oldMeta.npos)throw std::runtime_error("Zonas de soltura ausentes; banco preservado");
 begin=oldMeta.find('[',begin)+1;
 std::istringstream input(oldMeta.substr(begin,end-begin));std::vector<std::string> objects;std::size_t zone=0;std::string auditRegions="[";
 while(std::getline(input,line)){
  if(line.find("{\"sample\":")==std::string::npos)continue;
  auto last=line.find_last_not_of(" \t\r,");line.erase(last+1);
  if(release_detail::field(line,"release_trigger")==std::optional<std::string>("true")){
   if(release_detail::field(line,"release_key_pedal_passthrough")==std::optional<std::string>("true"))continue;
   if(zone>=releaseLines.size())throw std::runtime_error("Template de soltura incompleto; banco preservado");
   double base=release_detail::number(line,"release_key_base_volume_db",release_detail::number(line,"volume_db",0));
   double raw=release_detail::number(line,"source_loop_decay_q23",0);if(raw<0||raw>4294967295.0)throw std::runtime_error("Q23 de soltura inválido");
   auto seconds=npnoLoopDecaySeconds(static_cast<std::uint32_t>(raw));seconds=seconds<0?.25:std::clamp(seconds,.12,.45);
   release_detail::set(line,"pedal_release_only","true");release_detail::set(line,"release_key_pedal_passthrough","false");
   release_detail::set(line,"cc64_low","64");release_detail::set(line,"cc64_high","-1");
   release_detail::set(line,"release_key_base_volume_db",release_detail::numeric(base));
   release_detail::set(line,"volume_db",release_detail::numeric(std::clamp(base+gain,-144.0,10.8)));
   release_detail::set(line,"release_seconds",release_detail::numeric(seconds));release_detail::set(line,"muted",enabled?"false":"true");
   auto& region=releaseLines[zone++];
   region=std::regex_replace(region,std::regex("trigger=release_key"),"trigger=release");
   region=std::regex_replace(region,std::regex("[ \t]+(?:locc64|hicc64)=[-+0-9]+"),"");region=std::regex_replace(region,std::regex("[ \t]+sustain_lo=[-+0-9.]+"),"");region+=" locc64=64 sustain_lo=64";
   release_detail::opcode(region,"volume",std::clamp(release_detail::opcodeValue(region,"volume")+gain,-144.0,10.8));
   release_detail::opcode(region,"ampeg_release",seconds);
   if(auditRegions.size()>1)auditRegions+=',';
   auditRegions+="{\"zone\":"+std::to_string(objects.size())+",\"ampeg_release_seconds\":"+release_detail::numeric(seconds)+",\"release_key_gain_db\":"+release_detail::numeric(gain)+",\"muted\":"+(enabled?"false":"true")+",\"pedal_release_only\":true}";
  }
  objects.push_back(line);
 }
 auditRegions+=']';
 if(zone!=releaseLines.size())throw std::runtime_error("Template e metadados de soltura divergem; banco preservado");
 std::string updatedMeta=oldMeta.substr(0,begin);for(std::size_t i=0;i<objects.size();++i)updatedMeta+='\n'+objects[i]+(i+1<objects.size()?",":"");updatedMeta+=oldMeta.substr(end);
 release_detail::set(updatedMeta,"release_key_gain_db",release_detail::numeric(gain));release_detail::set(updatedMeta,"release_key_enabled",enabled?"true":"false");
 release_detail::set(updatedMeta,"pedal_release_revision","2");
 std::istringstream current(oldSfz);std::string updatedSfz;bool inserted=false;
 while(std::getline(current,line)){
  if(isRelease(line))continue;
  updatedSfz+=line+'\n';
  if(!inserted&&line.find("// source_bank=2;")!=std::string::npos){inserted=true;if(enabled)for(const auto& region:releaseLines)updatedSfz+=region+'\n';}
 }
 if(zone&&!inserted)throw std::runtime_error("Grupo SFZ de soltura ausente; banco preservado");
 auto audit=original;audit+=".audit.json";std::string oldAudit,newAudit;
 if(std::filesystem::exists(audit)){oldAudit=release_detail::read(audit);newAudit=oldAudit;release_detail::set(newAudit,"pedal_release_revision","2");release_detail::set(newAudit,"regions_after_calibration_policy",std::to_string(objects.size()));release_detail::set(newAudit,"release_key_gain_db",release_detail::numeric(gain));release_detail::set(newAudit,"release_key_enabled",enabled?"true":"false");
  std::regex listExpression("\"release_key_regions\"\\s*:\\s*\\[[^\\]]*\\]");std::smatch match;auto fieldText="\"release_key_regions\":"+auditRegions;
  if(std::regex_search(newAudit,match,listExpression))newAudit.replace(match.position(),match.length(),fieldText);else{auto end=newAudit.rfind('}');newAudit.insert(end,", "+fieldText);}
 }
 if(!std::filesystem::exists(templatePath))atomicWrite(templatePath,baseline);
 try{atomicWrite(sfz,updatedSfz);atomicWrite(metadata,updatedMeta);if(!newAudit.empty())atomicWrite(audit,newAudit);}
 catch(...){atomicWrite(sfz,oldSfz);atomicWrite(metadata,oldMeta);if(!oldAudit.empty())atomicWrite(audit,oldAudit);throw;}
 return true;
}
}

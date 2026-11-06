#include "npno_release.hpp"
#include "session.hpp"
#include <iostream>
using namespace lp;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::string attacks(const std::string& text){std::string result,line;std::istringstream in(text);while(std::getline(in,line))if(line.find("trigger=release")==std::string::npos)result+=line+'\n';return result;}
int main(int argc,char** argv){try{
 require(argc==4,"fixture sndfile output");auto root=std::filesystem::absolute(argv[3]);std::filesystem::remove_all(root);
 MetaSidecar meta;ConversionOptions options;options.releaseKeyGainDb=0;
 auto sfz=npnoParaSFZ(argv[1],root,meta,argv[2],{},options);auto original=root/std::filesystem::path(argv[1]).filename();
 auto baseline=release_detail::read(sfz);auto pcmHashes=meta.sampleHashes;
 require(updateNpnoRelease(original,-12,false),"disable must update controls");
 {
  std::istringstream disabled(release_detail::read(sfz));std::string line;while(std::getline(disabled,line))require(line.find("trigger=release")==std::string::npos,"disabled release must omit all auxiliary regions");
 }
 require(attacks(release_detail::read(sfz))==attacks(baseline),"runtime update changed attack/pedal regions");
 require(updateNpnoRelease(original,-8,true),"enable must restore regions");
 auto once=release_detail::read(sfz);
 require(!updateNpnoRelease(original,-8,true),"identical controls must be reused");
 require(release_detail::read(sfz)==once,"runtime update compounded gain");
 require(updateNpnoRelease(original,-24,true)&&updateNpnoRelease(original,-8,true),"gain range not applied");
 require(release_detail::read(sfz)==once,"round trip changed controls");
 for(std::size_t i=0;i<meta.samples.size();++i)require(sha256File(root/meta.samples[i])==pcmHashes[i],"runtime changed PCM");
 auto json=release_detail::read(std::filesystem::path(original.u8string()+".meta.json"));
 require(release_detail::number(json,"release_key_gain_db",0)==-8,"effective gain absent from metadata");
 require(release_detail::field(json,"release_key_enabled")==std::optional<std::string>("true"),"enabled state absent");
 auto before=release_detail::read(sfz);bool rejected=false;try{updateNpnoRelease(original,1,true);}catch(const std::exception&){rejected=true;}
 require(rejected&&release_detail::read(sfz)==before,"invalid setting modified bank");

 // Migrate the old generated schema, whose single release bank also served pedal.
 auto legacyMeta=meta;legacyMeta.zones.erase(std::remove_if(legacyMeta.zones.begin(),legacyMeta.zones.end(),[](const auto& z){return z.releaseKeyPedalPassthrough;}),legacyMeta.zones.end());
 for(auto& z:legacyMeta.zones)if(z.releaseTrigger){z.pedalReleaseOnly=false;z.cc64Low=z.cc64High=-1;z.volumeDb=z.releaseKeyBaseVolumeDb;z.releaseSeconds=.2;}
 auto legacyJson=legacyMeta.json();
 for(const std::string& field:{"release_key_gain_db","release_key_enabled","release_key_base_volume_db","release_key_pedal_passthrough"})legacyJson=std::regex_replace(legacyJson,std::regex(",\\s*\""+field+"\"\\s*:\\s*([-+0-9.eE]+|true|false)"),"");
 atomicWrite(std::filesystem::path(original.u8string()+".meta.json"),legacyJson);
 atomicWrite(sfz,writeSfzInstrument(legacyMeta.zones));std::filesystem::remove(std::filesystem::path(original.u8string()+".release-base.txt"));
 require(updateNpnoRelease(original,-8,true),"old bank must migrate without reimporting");
 auto migrated=release_detail::read(sfz);require(migrated.find("locc64=64")!=std::string::npos&&migrated.find("trigger=release_key")==std::string::npos,"old bank did not retain pedal-specific release");
 require(updateNpnoRelease(original,-12,true)&&updateNpnoRelease(original,-8,true),"old bank controls not reusable");
 require(release_detail::read(sfz)==migrated,"old bank migration duplicated or compounded release");
 // Reproduce the immediately preceding two-bank schema, including disabled
 // unpedaled controls and the unconditionally enabled pedal passthrough.
 auto split=legacyMeta;std::vector<ConvertedZone> duplicates;
 for(auto& z:split.zones)if(z.releaseTrigger){
  z.releaseKeyBaseVolumeDb=z.volumeDb;auto copy=z;copy.cc64Low=64;copy.releaseKeyPedalPassthrough=true;duplicates.push_back(copy);
  z.cc64High=63;z.volumeDb-=8;z.muted=true;
 }
 split.zones.insert(split.zones.end(),duplicates.begin(),duplicates.end());split.releaseKeyGainDb=-8;split.releaseKeyEnabled=false;
 atomicWrite(std::filesystem::path(original.u8string()+".meta.json"),split.json());
 atomicWrite(sfz,writeSfzInstrument(split.zones));for(auto& z:split.zones)if(z.releaseTrigger){z.muted=false;z.volumeDb=z.releaseKeyBaseVolumeDb;}
 atomicWrite(std::filesystem::path(original.u8string()+".release-base.txt"),writeSfzInstrument(split.zones));
 require(updateNpnoRelease(original,-8,false),"unchanged controls must still migrate old trigger schema");
 require(release_detail::read(sfz).find("trigger=release")==std::string::npos,"migration left enabled passthrough");
 require(updateNpnoRelease(original,-8,true),"migrated disabled release must restore");
 auto corrected=release_detail::read(sfz);require(corrected.find("trigger=release_key")==std::string::npos,"migration retained key trigger");
 require(updateNpnoRelease(original,-12,true)&&updateNpnoRelease(original,-8,true),"split migration controls not reusable");
 require(release_detail::read(sfz)==corrected,"split migration duplicated/compounded release");
 Session show;show.releaseKeyGainDb=-13;show.releaseKeyEnabled=false;std::istringstream profile(show.serialize());Session restored;restored.read(profile);
 require(restored.releaseKeyGainDb==-13&&!restored.releaseKeyEnabled,"profile did not preserve release settings");
 std::istringstream legacy("LPHS_PROFILE 10\nMASTER 1 -1\n");restored.read(legacy);
 require(restored.releaseKeyGainDb==-8&&restored.releaseKeyEnabled,"old profile defaults changed");
 std::cout<<"PASS: no-reimport release controls, unchanged PCM/attack/pedal-on, no gain accumulation, profiles and legacy defaults\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "npno_release.hpp"
#include "session.hpp"
#include <iostream>
using namespace lp;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::string attacks(const std::string& text){std::string result,line;std::istringstream in(text);while(std::getline(in,line))if(line.find("trigger=release_key")==std::string::npos||line.find("locc64=64")!=std::string::npos)result+=line+'\n';return result;}
int main(int argc,char** argv){try{
 require(argc==4,"fixture sndfile output");auto root=std::filesystem::absolute(argv[3]);std::filesystem::remove_all(root);
 MetaSidecar meta;ConversionOptions options;options.releaseKeyGainDb=0;
 auto sfz=npnoParaSFZ(argv[1],root,meta,argv[2],{},options);auto original=root/std::filesystem::path(argv[1]).filename();
 auto baseline=release_detail::read(sfz);auto pcmHashes=meta.sampleHashes;
 require(updateNpnoRelease(original,-12,false),"disable must update controls");
 {
  std::istringstream disabled(release_detail::read(sfz));std::string line;while(std::getline(disabled,line))if(line.find("trigger=release_key")!=std::string::npos)require(line.find("locc64=64")!=std::string::npos,"disabled unpedaled release must omit controlled regions");
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
 Session show;show.releaseKeyGainDb=-13;show.releaseKeyEnabled=false;std::istringstream profile(show.serialize());Session restored;restored.read(profile);
 require(restored.releaseKeyGainDb==-13&&!restored.releaseKeyEnabled,"profile did not preserve release settings");
 std::istringstream legacy("LPHS_PROFILE 10\nMASTER 1 -1\n");restored.read(legacy);
 require(restored.releaseKeyGainDb==-8&&restored.releaseKeyEnabled,"old profile defaults changed");
 std::cout<<"PASS: no-reimport release controls, unchanged PCM/attack/pedal, no gain accumulation, profiles and legacy defaults\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

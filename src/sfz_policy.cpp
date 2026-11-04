#include "sfz_conversion.hpp"
#include <map>
#include <set>
#include <tuple>
namespace lp {
namespace {
using Condition=std::tuple<int,int,bool,int,int,int>;
Condition condition(const ConvertedZone& z){return {z.program,z.root,z.releaseTrigger,z.cc64Low,z.cc64High,z.hasPan?static_cast<int>(z.pan*1000):0};}
bool sameTrigger(const ConvertedZone& a,const ConvertedZone& b){return a.program==b.program&&a.releaseTrigger==b.releaseTrigger&&a.cc64Low==b.cc64Low&&a.cc64High==b.cc64High;}
}
void auditVelocityCoverage(const std::vector<ConvertedZone>& zones){
 std::map<std::pair<Condition,int>,std::vector<const ConvertedZone*>> groups;
 for(const auto& z:zones)if(!z.muted)groups[{condition(z),std::max(1,z.sequencePosition)}].push_back(&z);
 std::map<Condition,int> lengths;
 for(const auto& z:zones)if(!z.muted&&z.sequenceLength>1){auto& n=lengths[condition(z)];if(n&&n!=z.sequenceLength)throw std::runtime_error("Auditoria RR: comprimentos inconsistentes");n=z.sequenceLength;}
 for(const auto& entry:lengths){for(int slot=1;slot<=entry.second;++slot)if(!groups.count({entry.first,slot}))throw std::runtime_error("Auditoria RR: posição ausente "+std::to_string(slot));}
 for(const auto& entry:groups){
  std::set<std::pair<int,int>> layers;for(auto z:entry.second)layers.insert({std::max(1,z->velLow),z->velHigh});
  // An intentionally velocity-limited single region is valid; layered maps must cover 1..127.
  if(layers.size()<2)continue;
  for(int key=0;key<128;++key){std::array<int,128> count{};bool present=false;
   for(auto z:entry.second)if(key>=z->keyLow&&key<=z->keyHigh){present=true;for(int v=std::max(1,z->velLow);v<=z->velHigh;++v)++count[v];}
   if(present)for(int v=1;v<=127;++v)if(count[v]!=1)throw std::runtime_error("Auditoria velocity: lacuna/sobreposição na tecla "+std::to_string(key)+", velocity "+std::to_string(v));
  }
 }
}
void prepareSfzZones(std::vector<ConvertedZone>& zones,MetaSidecar& meta,const ConversionOptions& options){
 if(zones.size()>65536)throw std::runtime_error("Instrumento excede limite de 65536 regiões SFZ");
 if(options.crossfadeWidth<1||options.crossfadeWidth>16)throw std::runtime_error("Crossfade velocity deve ter 1 a 16 passos MIDI");
 for(auto& z:zones){z.sourceVelLow=std::max(1,z.velLow);z.sourceVelHigh=z.velHigh;z.velLow=std::max(1,z.velLow);if(z.keyLow<0||z.keyHigh>127||z.keyLow>z.keyHigh||z.velLow>z.velHigh||z.velHigh>127)throw std::runtime_error("Região fora dos limites MIDI");}
 // Split neighboring roots at the overlap midpoint. Original ranges remain in source metadata.
 for(std::size_t i=0;i<zones.size();++i)for(std::size_t j=i+1;j<zones.size();++j){auto& a=zones[i];auto& b=zones[j];
  if(a.muted||b.muted||!sameTrigger(a,b)||a.velLow>b.velHigh||b.velLow>a.velHigh||a.keyLow>b.keyHigh||b.keyLow>a.keyHigh)continue;
  if(a.root!=b.root){auto* low=a.root<b.root?&a:&b;auto* high=a.root<b.root?&b:&a;
   int first=std::max(a.keyLow,b.keyLow),last=std::min(a.keyHigh,b.keyHigh);int split=std::clamp((low->root+high->root)/2,first-1,last);
   low->keyHigh=std::min(low->keyHigh,split);high->keyLow=std::max(high->keyLow,split+1);
   if(low->keyLow>low->keyHigh||high->keyLow>high->keyHigh)throw std::runtime_error("Sobreposição de teclas sem partição válida; use mapeamento distinto");
   meta.limitations.push_back("Aviso: sobreposição de teclas particionada sem duplicar vozes; fades de tecla originais preservados no sidecar.");
  }else if(a.hasPan&&b.hasPan&&a.channels==1&&b.channels==1&&a.pan==-b.pan&&std::abs(a.pan)==100&&a.sample.size()>6&&b.sample.size()>6&&a.sample.substr(0,a.sample.size()-6)==b.sample.substr(0,b.sample.size()-6)&&((a.sample.substr(a.sample.size()-6)=="-L.wav"&&b.sample.substr(b.sample.size()-6)=="-R.wav")||(a.sample.substr(a.sample.size()-6)=="-R.wav"&&b.sample.substr(b.sample.size()-6)=="-L.wav"))){continue;}else if(!(a.sequenceLength>1&&a.sequenceLength==b.sequenceLength&&a.sequencePosition!=b.sequencePosition)){
   throw std::runtime_error("Regiões sobrepostas sem RR explícito: root "+std::to_string(a.root)+"; alternativas desconhecidas não podem disparar juntas");
  }
 }
 auditVelocityCoverage(zones);
 meta.preservedFeatures.push_back("velocity_coverage=audited_before_crossfade; round_robin=seq_length/seq_position_only; resample=false; resample_error_cents=0");
 meta.preservedFeatures.push_back(options.velocityCrossfade?"velocity_crossfade=on; width="+std::to_string(options.crossfadeWidth):"velocity_crossfade=off (default; bounded voice cost)");
 if(!options.velocityCrossfade)return;
 const auto original=zones;
 for(std::size_t i=0;i<zones.size();++i)for(std::size_t j=0;j<zones.size();++j){auto& a=zones[i];auto& b=zones[j];const auto& oa=original[i];const auto& ob=original[j];
  if(oa.muted||ob.muted||i==j||condition(oa)!=condition(ob)||oa.keyLow!=ob.keyLow||oa.keyHigh!=ob.keyHigh||oa.sequencePosition!=ob.sequencePosition||oa.velHigh+1!=ob.velLow)continue;
  int width=std::min({options.crossfadeWidth,(oa.velHigh-oa.velLow+1)/2,(ob.velHigh-ob.velLow+1)/2});
  if(width==0){meta.limitations.push_back("Crossfade omitido: camada estreita demais para dois pontos de interpolação");continue;}
  int lo=std::max(oa.velLow,ob.velLow-width),hi=std::min(ob.velHigh,ob.velLow+width-1);
  a.velHigh=hi;b.velLow=lo;a.xfoutLowVel=lo;a.xfoutHighVel=hi;b.xfinLowVel=lo;b.xfinHighVel=hi;
 }
}
std::string writeSfzInstrument(const std::vector<ConvertedZone>& zones){
 auto s=sfzStream();
 // Stable first-seen group order, stable source order inside each group. Regions
 // retain their controls too: a new group cannot leak inherited CC/trigger state.
 using Group=std::tuple<int,int,bool,int,int>;
 std::vector<Group> order;std::map<Group,std::vector<const ConvertedZone*>> groups;
 for(const auto& z:zones){Group key{z.program,z.sourceBank,z.releaseTrigger,z.cc64Low,z.cc64High};if(!groups.count(key))order.push_back(key);groups[key].push_back(&z);}
 for(const auto& key:order){writeSfzGroup(s);s<<"// source_bank="<<std::get<1>(key)<<"; controls repeated per region\n";for(auto z:groups[key])writeSfzRegion(s,*z);}
 return s.str();
}
}

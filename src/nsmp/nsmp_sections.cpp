// SPDX-License-Identifier: AGPL-3.0-or-later
// Port: OpenNord ns4/nsmp.ts parseNsmpSections/readNsmpZones/readPerNoteRows.
#include "nsmp.hpp"
#include <set>
namespace lp::nsmp {
std::vector<Section> parseNsmpSections(const Bytes& b,const Identity& id){View v(b,id.end);std::vector<Section> sections;auto pos=id.body;while(pos<id.end){if(sections.size()>=2048)throw std::runtime_error("Nord: limite de seções");v.require(pos,id.headerSize);Section s;s.header=pos;if(id.legacy)s.tag=".";for(std::size_t i=0;i<(id.legacy?3u:4u);++i){auto c=v.at(pos+i);s.tag+=c>=32&&c<127?char(c):'.';}s.version=v.be(pos+(id.legacy?3:4),id.legacy?2:4);s.size=v.be(pos+(id.legacy?5:8),4);s.payload=pos+id.headerSize;v.require(s.payload,s.size);s.end=s.payload+s.size;sections.push_back(s);pos=s.end;}if(sections.empty())throw std::runtime_error("Nord: arquivo sem seções");return sections;}
std::string readName(const Bytes& b,const Section& s){std::string run;View v(b,s.end);for(auto i=s.payload;i<s.end;++i){auto c=v.at(i);if(c>=32&&c<127)run+=char(c);else if(run.size()>=2)break;else run.clear();}while(!run.empty()&&run.back()==' ')run.pop_back();auto p=run.find_first_not_of(' ');return p==std::string::npos?std::string{}:run.substr(p);}
namespace {
bool suffix(const std::string& tag,const std::string& end){return tag.size()>=end.size()&&tag.compare(tag.size()-end.size(),end.size(),end)==0;}
std::int32_t s24(std::uint32_t raw){return raw>=0x800000?std::int32_t(raw)-0x1000000:std::int32_t(raw);}
Zone zone16(View v,std::size_t o){v.require(o,16);Zone z;z.record=o;z.strokeEntry=o+8;z.root=v.at(o);z.high=v.at(o+1);z.low=v.at(o+2);z.mode=v.at(o+3);z.playback=v.at(o+4);z.oneShot=v.at(o+5);z.id=v.be(o+8,4);z.velLow=v.at(o+14);z.velHigh=v.at(o+15);return z;}
void tile(std::vector<Zone>& zones,const std::map<std::uint32_t,int>& roots){std::stable_sort(zones.begin(),zones.end(),[](auto& a,auto& b){return a.high>b.high;});for(std::size_t i=0;i<zones.size();++i){auto& z=zones[i];z.low=i+1<zones.size()?zones[i+1].high+1:0;auto r=roots.find(z.id);if(r==roots.end())throw std::runtime_error("Nord: zona referencia stroke ausente");z.root=r->second;}}
}
std::vector<Zone> readNsmpZones(const Bytes& b,const Identity& id,const std::vector<Section>& sections){
 const Section* map=nullptr;std::map<std::uint32_t,int> roots;std::size_t strokeCount=0;for(const auto& s:sections){if(suffix(s.tag,"map"))map=&s;if(suffix(s.tag,"stk")){View v(b,s.end);v.require(s.payload,6);auto gid=v.be(s.payload,4);if(!roots.emplace(gid,v.at(s.payload+5)).second)throw std::runtime_error("Nord: id de stroke duplicado");++strokeCount;}}
 if(!map||!strokeCount)throw std::runtime_error("Nord: mapa ou strokes ausentes");if(strokeCount>512)throw std::runtime_error("Nord: limite de 512 strokes");if(map->size>131072)throw std::runtime_error("Nord: seção map excede limite de 128 KiB");View v(b,map->end);std::vector<Zone> zones;auto p=map->payload;
 if(id.legacy&&map->version!=10){
  auto bytes=strokeCount*12;if(bytes>map->size)throw std::runtime_error("Nord: tabela legada truncada");
  bool found=false;for(auto cursor=map->end;cursor>p;){auto o=--cursor;if(o+2>map->end||v.be(o,2)!=strokeCount||o+2+bytes>map->end)continue;auto rec=o+2;bool valid=true;for(std::size_t i=0;i<strokeCount;++i){auto r=rec+i*12;if(v.be(r+6,4)>127||v.be(r+10,2)!=1||!roots.count(v.be(r,3))){valid=false;break;}}if(!valid)continue;
   for(std::size_t i=0;i<strokeCount;++i){auto r=rec+i*12;Zone z;z.record=r;z.id=v.be(r,3);z.high=v.be(r+6,4);z.level=v.be(r+3,3)/1048576.;zones.push_back(z);}found=true;break;
  }if(!found)throw std::runtime_error("Nord: tabela de zonas v9 não reconhecida");tile(zones,roots);
 }else if(id.legacy||map->version==12){
  const auto table=p+15+128*6;bool exact=false;
  if(id.legacy&&table+3<=map->end){auto count=v.be(table,3);if(count>0&&count<=512&&table+3+count*15==map->end){exact=true;for(unsigned i=0;i<count;++i){auto r=table+3+i*15;Zone z;z.record=r;z.id=v.be(r,3);z.high=v.at(r+9);z.level=v.be(r+3,3)/1048576.;z.detune=dsp2Detune(s24(v.be(r+6,3)));zones.push_back(z);}}}
  if(!exact){ // Port upstream's maximal-run/tail tie-break fallback for v10/v12.
   const std::size_t rec=id.legacy?15:11;std::size_t best=0,bestN=0,bestTrailer=map->size;
   auto valid=[&](std::size_t r){if(r+rec>map->end)return false;if(!id.legacy)return v.at(r)==0&&v.at(r+6)==1&&v.at(r+3)>=1&&v.at(r+3)<=127&&roots.count(v.be(r+7,4));
    return roots.count(v.at(r))&&v.at(r+7)<=127&&((v.at(r+8)==0&&v.at(r+9)==1)||v.at(r+1)==0x10||(v.at(r+7)>=1&&v.at(r+4)==0&&v.at(r+5)==0&&v.at(r+6)==0&&v.at(r+8)==0&&v.at(r+10)==0&&v.at(r+11)==0&&v.at(r+12)==0&&v.at(r+13)==0&&v.at(r+14)==0));};
   for(auto start=p+6;start+rec<=map->end;++start){std::size_t n=0;for(auto r=start;valid(r)&&n<512;r+=rec)++n;auto trailer=map->end-start-n*rec;if(n>bestN||(n==bestN&&n&&trailer<bestTrailer)){best=start;bestN=n;bestTrailer=trailer;}}
   for(std::size_t i=0;i<bestN;++i){auto r=best+i*rec;Zone z;z.record=r;z.id=id.legacy?v.at(r):v.be(r+7,4);z.high=v.at(r+(id.legacy?7:3));zones.push_back(z);}
  }tile(zones,roots);
 }else if(map->version>=21){
  auto valid=[&](std::size_t r){if(r+16>map->end)return false;auto gid=v.be(r+8,4);if(!roots.count(gid)&&roots.count(v.at(r+11)))gid=v.at(r+11);return v.at(r)<=127&&v.at(r+1)<=127&&v.at(r+2)<=127&&roots.count(gid)&&((v.at(r+12)==0&&v.at(r+13)==1&&v.at(r+14)==0)||v.at(r+7)==1);};
  auto read=[&](std::size_t r){auto z=zone16(v,r);if(!roots.count(z.id)&&roots.count(v.at(r+11)))z.id=v.at(r+11);return z;};
  auto rec=p+6+128*10+32;auto count=rec<=map->end?v.at(rec-1):0;auto span=std::size_t(count)*16;
  if(count&&rec+span<=map->end&&map->end-rec-span<=8&&valid(rec)){for(unsigned i=0;i<count;++i){if(!valid(rec+i*16))throw std::runtime_error("Nord: zona codec4 inválida");zones.push_back(read(rec+i*16));}}
  if(zones.empty()&&count){ // Multi-stroke zones: 8-byte head + n*8 stroke entries.
   auto r=rec;bool layered=false,ok=true;for(unsigned i=0;i<count&&ok;++i){if(r+8>map->end||v.at(r)>127||v.at(r+2)>v.at(r)||v.at(r)>v.at(r+1)||v.at(r+1)>127){ok=false;break;}auto n=v.be(r+6,2);if(!n||n>64||r+8+n*8>map->end){ok=false;break;}layered|=n>1;for(unsigned j=0;j<n;++j){auto e=r+8+j*8;Zone z;z.record=r;z.strokeEntry=e;z.id=v.be(e,4);if(!roots.count(z.id)){ok=false;break;}z.root=v.at(r);z.high=v.at(r+1);z.low=v.at(r+2);z.mode=v.at(r+3);z.playback=v.at(r+4);z.oneShot=v.at(r+5);z.velLow=v.at(e+6);z.velHigh=v.at(e+7);zones.push_back(z);}r+=8+n*8;}
   if(!ok||!layered||r>map->end||map->end-r>8)zones.clear();
  }
  if(zones.empty()){std::size_t best=0,bestN=0,bestTrailer=map->size;for(auto start=p+6;start+16<=map->end;++start){std::size_t n=0;for(auto r=start;valid(r)&&n<512;r+=16)++n;auto trailer=map->end-start-n*16;if(n>bestN||(n==bestN&&n&&trailer<bestTrailer)){best=start;bestN=n;bestTrailer=trailer;}}for(std::size_t i=0;i<bestN;++i)zones.push_back(read(best+i*16));}
 }else{
  auto rec=p+6+128*6+1;v.require(rec-1,1);auto count=v.at(rec-1);v.require(rec,count*16);for(unsigned i=0;i<count;++i)zones.push_back(zone16(v,rec+i*16));
 }
 if(zones.empty()||zones.size()>4096)throw std::runtime_error("Nord: mapa de zonas não reconhecido");for(const auto& z:zones)if(!roots.count(z.id)||z.root<0||z.root>127||z.low<0||z.high>127||z.low>z.high||z.velLow<0||z.velHigh>127||z.velLow>z.velHigh)throw std::runtime_error("Nord: zona fora dos limites MIDI");return zones;
}
std::vector<std::array<std::uint8_t,6>> readPerNoteRows(const Bytes& b,const Identity& id,const Section& map){
 View v(b,map.end);auto rowLen=!id.legacy&&id.codec>=4?10u:6u;auto first=map.payload+6;
 if(id.legacy){auto table=map.payload+15+128*6;auto rec=map.version==10?15u:12u;if(table+3<=map.end&&table+3+std::size_t(v.be(table,3))*rec==map.end)first=map.payload+15;else if(!(map.size>=6+768&&(map.size-6-768)%15==0))return {};}
 if(first+128*rowLen>map.end)return {};std::vector<std::array<std::uint8_t,6>> rows(128);for(unsigned i=0;i<128;++i)for(unsigned j=0;j<6;++j)rows[i][j]=v.at(first+i*rowLen+j);return rows;
}
}

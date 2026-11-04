// SPDX-License-Identifier: AGPL-3.0-or-later
// Byte layout and PCM reconstruction: see third_party/npno/NOTICE.md.
#include "npno_reader.hpp"
#include "nsmp/nsmp.hpp"
#include "sfz_conversion.hpp"
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
namespace lp::npno {
namespace {
constexpr std::size_t directory=0x732,recordSize=0x76,overlap=64;
struct View {
 const Bytes& b;std::size_t end;
 void require(std::size_t at,std::size_t n)const{if(at>end||n>end-at)throw std::runtime_error("Módulo de piano truncado: offset "+std::to_string(at)+", tamanho "+std::to_string(n));}
 unsigned byte(std::size_t at)const{require(at,1);return b[at];}
 std::uint32_t be(std::size_t at,unsigned n)const{require(at,n);std::uint32_t x=0;for(unsigned i=0;i<n;++i)x=(x<<8)|b[at+i];return x;}
 std::uint32_t le(std::size_t at,unsigned n)const{require(at,n);std::uint32_t x=0;for(unsigned i=0;i<n;++i)x|=std::uint32_t(b[at+i])<<(8*i);return x;}
 int signedByte(std::size_t at)const{auto x=byte(at);return x>=128?int(x)-256:int(x);}
 int signedWord(std::size_t at)const{auto x=be(at,2);return x>=32768?int(x)-65536:int(x);}
 bool tag(std::size_t at,const char* text)const{require(at,4);return std::equal(text,text+4,b.begin()+at);}
};
std::string versionName(unsigned version){return std::to_string(version/100)+"."+std::to_string(version%100);}
void assignVelocity(Module& m){
 for(int root=0;root<128;++root)for(int bank=0;bank<3;++bank){
  std::vector<Stroke*> layers;for(auto& s:m.strokes)if(s.root==root&&s.bank==bank)layers.push_back(&s);
  if(layers.empty())continue;
  int previous=0;bool first=true;
  for(auto* s:layers){
   if(!first&&s->layer<previous)throw std::runtime_error("Módulo de piano: camadas fora de ordem no root "+std::to_string(root));
   if(m.streamVersion==0x450)s->threshold=((previous+s->layer)*5+1)/2;
   previous=s->layer;first=false;
  }
  // The softest reachable threshold wins; ties choose the last directory record.
  for(int v=1;v<=127;++v){Stroke* pick=layers.front();int best=-1;
   for(auto* s:layers)if(v<=topVelocity(s->threshold)&&s->threshold>=best){pick=s;best=s->threshold;}
   if(pick->velHigh==0)pick->velLow=v;
   else if(pick->velHigh!=v-1)throw std::runtime_error("Módulo de piano: faixa de velocity descontínua");
   pick->velHigh=v;
  }
  for(auto* s:layers)if(s->velHigh==0)m.warnings.push_back("Camada sem velocity alcançável: registro "+std::to_string(s->index)+", root "+std::to_string(root)+"; áudio e metadados preservados.");
 }
}
}
int topVelocity(int threshold){
 // Reference curve sampled at ten-unit intervals; interpolation is approximate.
 static constexpr int reach[]={127,107,105,99,93,85,79,73,67,61,57,53,51,45,43,39,33,31,27,23,21};
 if(threshold<0||threshold>200)return 0;int step=threshold/10,into=threshold%10;
 return reach[step]-((reach[step]-reach[std::min(step+1,20)])*into+9)/10;
}
bool sniff(const std::filesystem::path& path){
 std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Não foi possível ler módulo de piano");
 Bytes b(52);f.read(reinterpret_cast<char*>(b.data()),b.size());b.resize(static_cast<std::size_t>(f.gcount()));View v{b,b.size()};
 if(b.size()<24||!v.tag(0,"CBIN")||!v.tag(8,"npno"))return false;
 auto type=v.le(4,4);if(type>1)throw std::runtime_error("Módulo de piano: versão de contêiner não suportada "+std::to_string(type));
 auto at=type==1?44u:24u;return b.size()>=at+4&&v.tag(at,"CNSP");
}
Module readModule(const std::filesystem::path& path){
 Module m;auto size=std::filesystem::file_size(path);
 if(size>512u*1024u*1024u)throw std::runtime_error("Módulo de piano excede limite de importação de 512 MiB");
 m.bytes.resize(static_cast<std::size_t>(size));std::ifstream f(path,std::ios::binary);
 if(!f||!f.read(reinterpret_cast<char*>(m.bytes.data()),m.bytes.size()))throw std::runtime_error("Falha ao ler módulo de piano");
 View envelope{m.bytes,m.bytes.size()};envelope.require(0,24);
 if(!envelope.tag(0,"CBIN")||!envelope.tag(8,"npno"))throw std::runtime_error("Módulo de piano: assinatura desconhecida");
 auto& h=m.header;h.formatType=envelope.le(4,4);h.tag="npno";
 if(h.formatType>1)throw std::runtime_error("Módulo de piano: versão de contêiner não suportada "+std::to_string(h.formatType));
 h.bank=static_cast<std::uint16_t>(envelope.le(12,2));h.slot=static_cast<std::uint16_t>(envelope.le(14,2));h.category=envelope.le(16,4);h.version=envelope.le(20,4);
 static constexpr unsigned revisions[]={500,520,530,540,600,610,620,630};
 if(std::find(std::begin(revisions),std::end(revisions),h.version)==std::end(revisions))throw std::runtime_error("Módulo de piano: versão não suportada "+versionName(h.version)+" ("+std::to_string(h.version)+")");
 m.base=h.formatType==1?44:24;envelope.require(m.base,h.formatType==0?2:0);m.end=m.bytes.size()-(h.formatType==0?2:0);
 h.recordOffset=m.base;h.recordLength=m.end-m.base;View v{m.bytes,m.end};v.require(m.base,directory);
 if(!v.tag(m.base,"CNSP"))throw std::runtime_error("Módulo de piano: assinatura CNSP desconhecida");
 m.streamVersion=int(v.be(m.base+4,2));auto echo=v.be(m.base+0x61c,2);
 if(m.streamVersion!=0x450&&m.streamVersion!=0x464)throw std::runtime_error("Módulo de piano: versão interna não suportada "+std::to_string(m.streamVersion));
 if(echo!=unsigned(m.streamVersion))throw std::runtime_error("Módulo de piano: versões internas divergentes "+std::to_string(m.streamVersion)+"/"+std::to_string(echo));
 if((h.version/100==5)!=(m.streamVersion==0x450))throw std::runtime_error("Módulo de piano: revisão "+versionName(h.version)+" incompatível com versão interna "+std::to_string(m.streamVersion));
 if(h.formatType==1){h.crc32=envelope.le(24,4);m.checksumValid=nsmp::crc32(m.bytes,m.base,m.end)==h.crc32;}
 else m.checksumValid=nsmp::crc16(m.bytes,m.end)==envelope.le(m.end,2);
 if(!m.checksumValid)m.warnings.push_back("Checksum inconsistente; leitura continuou com validação estrutural e do áudio.");
 for(std::size_t i=0;i<32;++i){auto c=v.byte(m.base+0x1c+i);if(!c)break;m.name+=c>=32&&c<=126?char(c):'?';}
 if(m.name.empty())m.name=path.stem().u8string();m.channels=int(v.be(m.base+0x61e,2));auto count=v.be(m.base+0x620,2);
 if(m.channels<1||m.channels>2||count==0||count>8192)throw std::runtime_error("Módulo de piano: canais ou contagem de amostras inválidos (limite: 8192 registros)");
 v.require(m.base+directory,std::size_t(count)*recordSize);
 m.gain=v.signedByte(m.base+0x40c);m.damperTop=int(v.byte(m.base+0x40d));
 if(m.damperTop>127)throw std::runtime_error("Módulo de piano: limite de abafamento fora da faixa MIDI");
 std::vector<int> roots;for(int root=0;root<128;++root){auto n=v.be(m.base+0x622+root*2,2);if(n>count||roots.size()+n>count)throw std::runtime_error("Módulo de piano: tabela de roots excede diretório");roots.insert(roots.end(),n,root);}
 if(roots.size()!=count)throw std::runtime_error("Módulo de piano: soma de camadas por root difere do diretório");
 std::size_t next=m.base+directory+std::size_t(count)*recordSize;std::uint64_t totalValues=0;
 for(std::size_t i=0;i<count;++i){
  Stroke s;s.index=i;s.recordOffset=m.base+directory+i*recordSize;auto r=s.recordOffset;
  s.root=roots[i];s.audioOffset=m.base+v.be(r,4);s.bank=int(v.byte(r+4));s.layer=int(v.byte(r+5));s.frames=v.be(r+6,4);s.blocks=v.be(r+0xa,2);
  if(s.bank>2)throw std::runtime_error("Módulo de piano: banco de região não suportado "+std::to_string(s.bank));
  auto blockBytes=std::size_t(1022*m.channels);auto span=s.blocks*blockBytes;
  if(s.blocks==0||s.frames==0||s.frames>s.blocks*((blockBytes-2)*8/m.channels-overlap))throw std::runtime_error("Módulo de piano: tamanho declarado de áudio inválido");
  totalValues+=std::uint64_t(s.frames)*m.channels;
  if(s.frames>16000000u||totalValues>256000000u)throw std::runtime_error("Módulo de piano: limite de PCM excedido");
  if(s.audioOffset<next)throw std::runtime_error("Módulo de piano: blocos de áudio sobrepostos ao diretório ou outra amostra");
  v.require(s.audioOffset,span);
  for(auto at=next;at<s.audioOffset;++at)if(v.byte(at)!=0)throw std::runtime_error("Módulo de piano: dados não identificados entre blocos de áudio");
  next=s.audioOffset+span;
  for(int c=0;c<m.channels;++c)for(int k=0;k<4;++k)s.history[c][k]=static_cast<std::int16_t>(v.signedWord(r+0xc+c*8+k*2));
  s.loopStart=v.be(r+0x1c,4);s.loopLength=v.be(r+0x20,4);s.loopDecay=v.be(r+0x2e,4);s.trim=v.signedWord(r+0x34);s.threshold=int(v.byte(r+0x74));
  if(s.loopLength&&(s.loopStart>=s.frames||s.loopLength>s.frames-s.loopStart))throw std::runtime_error("Módulo de piano: loop fora dos limites da amostra");
  m.strokes.push_back(s);
 }
 for(auto at=next;at<m.end;++at)if(v.byte(at)!=0)throw std::runtime_error("Módulo de piano: dados finais não identificados");
 for(int key=0;key<128;++key){
  int root=int(v.byte(m.base+0x8c+key));m.keyRoots[key]=root;m.fineTune[key]=v.signedByte(m.base+0x18c+key);
  if(root==255)continue;
  if(root>127||std::find(roots.begin(),roots.end(),root)==roots.end())throw std::runtime_error("Módulo de piano: tecla "+std::to_string(key)+" referencia root inexistente");
  bool attack=false;for(const auto& s:m.strokes)if(s.root==root&&s.bank==0)attack=true;
  if(!attack)throw std::runtime_error("Módulo de piano: root sem camada de ataque");
  bool undamped=key>m.damperTop;
  if(!m.zones.empty()&&m.zones.back().high==key-1&&m.zones.back().root==root&&m.zones.back().fineTune==m.fineTune[key]&&m.zones.back().undamped==undamped)m.zones.back().high=key;
  else m.zones.push_back({root,key,key,m.fineTune[key],undamped});
 }
 if(m.zones.empty())throw std::runtime_error("Módulo de piano: mapa de teclado sem zonas tocáveis");
 assignVelocity(m);return m;
}
Audio decodeStroke(const Module& m,const Stroke& s){
 View v{m.bytes,m.end};Audio audio;audio.lanes.resize(m.channels);for(auto& lane:audio.lanes)lane.reserve(s.frames);
 static constexpr int predictors[5][4]={{0,0,0,0},{1,0,0,0},{2,-1,0,0},{3,-3,1,0},{4,-6,4,-1}};
 std::array<std::array<std::int64_t,4>,2> history{};
 for(int c=0;c<m.channels;++c)for(int k=0;k<4;++k)history[c][k]=s.history[c][3-k];
 std::vector<std::vector<std::int16_t>> tail(m.channels);std::size_t emitted=0;
 for(std::size_t block=0;block<s.blocks;++block){
  auto blockBytes=std::size_t(1022*m.channels),at=s.audioOffset+block*blockBytes;v.require(at,blockBytes);
  auto header=v.be(at,2);unsigned width=header&31,order=(header>>5)&7;
  if(width<1||width>16||order>4)throw std::runtime_error("Módulo de piano: cabeçalho de áudio corrompido no registro "+std::to_string(s.index)+", bloco "+std::to_string(block));
  std::size_t frames=(blockBytes-2)*8/(width*m.channels),own=frames-overlap;
  if(own<overlap||emitted>s.frames||own>s.frames-emitted)throw std::runtime_error("Módulo de piano: frames decodificados excedem declaração");
  auto nextHistory=history;std::size_t bit=0;std::vector<std::vector<std::int16_t>> pcm(m.channels);for(auto& lane:pcm)lane.reserve(frames);
  for(std::size_t frame=0;frame<frames;++frame){
   for(int c=0;c<m.channels;++c){
    std::uint32_t raw=0;for(unsigned got=0;got<width;){auto shift=unsigned(bit%16),take=std::min(16-shift,width-got);auto word=v.be(at+2+(bit/16)*2,2);raw|=((word>>shift)&((1u<<take)-1))<<got;got+=take;bit+=take;}
    std::int64_t value=(raw&(1u<<(width-1)))?std::int64_t(raw)-(1ll<<width):raw;
    for(unsigned k=0;k<order;++k)value+=predictors[order][k]*history[c][k];
    if(value<-32768||value>32767)throw std::runtime_error("Módulo de piano: PCM fora de 16 bits; áudio corrompido ou codec não suportado");
    for(int k=3;k>0;--k)history[c][k]=history[c][k-1];history[c][0]=value;pcm[c].push_back(static_cast<std::int16_t>(value));
   }
   if(frame+1==own)nextHistory=history;
  }
  for(int c=0;c<m.channels;++c){
   if(block){if(!std::equal(tail[c].begin(),tail[c].end(),pcm[c].begin()))throw std::runtime_error("Módulo de piano: repetição de blocos inconsistente no registro "+std::to_string(s.index));audio.overlapChecked+=overlap;}
   audio.lanes[c].insert(audio.lanes[c].end(),pcm[c].begin(),pcm[c].begin()+own);tail[c].assign(pcm[c].begin()+own,pcm[c].end());
  }
  emitted+=own;history=nextHistory;
 }
 if(emitted!=s.frames)throw std::runtime_error("Módulo de piano: contagem final de frames divergente");return audio;
}
std::string Module::metadataJson()const{
 auto o=sfzStream();o.str("");o<<"{\"body_offset\":"<<base<<",\"stream_version\":"<<streamVersion<<",\"sample_rate\":"<<sampleRate<<",\"channels\":"<<channels<<",\"gain_tenths_db\":"<<gain<<",\"damper_top\":"<<damperTop<<",\"velocity_profile\":\"reference_touch_0_approximate\",\"global_detune\":null,\"prefix_hex\":\""<<bytesHex(bytes.data()+base,directory)<<"\",\"strokes\":[";
 for(std::size_t i=0;i<strokes.size();++i){if(i)o<<',';const auto& s=strokes[i];o<<"{\"index\":"<<i<<",\"record_offset\":"<<s.recordOffset<<",\"root\":"<<s.root<<",\"bank\":"<<s.bank<<",\"layer\":"<<s.layer<<",\"threshold\":"<<s.threshold<<",\"velocity_low\":"<<s.velLow<<",\"velocity_high\":"<<s.velHigh<<",\"audio_offset\":"<<s.audioOffset<<",\"frames\":"<<s.frames<<",\"blocks\":"<<s.blocks<<",\"loop_start\":"<<s.loopStart<<",\"loop_length\":"<<s.loopLength<<",\"loop_decay_q23\":"<<s.loopDecay<<",\"trim\":"<<s.trim<<",\"record_hex\":\""<<bytesHex(bytes.data()+s.recordOffset,recordSize)<<"\"}";}
 o<<"]}";return o.str();
}
}

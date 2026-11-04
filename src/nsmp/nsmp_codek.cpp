// SPDX-License-Identifier: AGPL-3.0-or-later
// Port: OpenNord ns4/nsmp-codec.ts decodeStroke and nsmp.ts stroke locator/loop.
// Upstream spells the file "codec", not "codek". See NOTICE.md for the pinned source.
#include "nsmp.hpp"
#include <cmath>
#include <limits>
namespace lp::nsmp {
namespace {
constexpr int coeff[8][7]={{0},{1},{2,-1},{3,-3,1},{4,-6,4,-1},{5,-10,10,-5,1},{6,-15,20,-15,6,-1},{7,-21,35,-35,21,-7,1}};
struct Header {unsigned count,order,width;bool stop,linear,decay;};
Header block(std::uint32_t w){unsigned order=(w>>14)&15,width=((w>>19)&15)+1;return {w&0x3fff,order,width,order==0&&width==1,bool(w&(1u<<23)),bool(w&(1u<<18))};}
struct History {std::array<std::int32_t,32> ring{};unsigned head=0;};
void emit(History& h,std::vector<std::int32_t>& out,std::int32_t residual,unsigned order){std::int64_t value=residual;for(unsigned k=0;k<order;++k)value+=std::int64_t(coeff[order][k])*h.ring[(h.head-1-k)&31];if(value<std::numeric_limits<std::int32_t>::min()||value>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("Nord: predição divergiu");h.ring[h.head]=static_cast<std::int32_t>(value);h.head=(h.head+1)&31;out.push_back(static_cast<std::int32_t>(value));}
std::int32_t signedBits(std::uint32_t value,unsigned width){return value>=(1u<<(width-1))?std::int32_t(value)-std::int32_t(1u<<width):std::int32_t(value);}
double peak(const Decoded& d){double p=0;for(const auto& ch:d.channels)for(auto v:ch)p=std::max(p,std::abs(double(v)));return p;}
bool reaches(const Bytes& b,std::size_t start,std::size_t end,bool u24,unsigned minBlocks){View v(b,end);auto hb=u24?3u:4u,wb=hb*8;unsigned blocks=0;auto o=start;while(o+hb<=end){auto h=block(v.be(o,hb));if(h.stop)return blocks>=minBlocks&&end-(o+hb)<=hb;if(h.order>7||!h.count)return false;auto n=hb+((std::size_t(h.count)*h.width+wb-1)/wb)*hb;if(n>end-o)return false;o+=n;++blocks;}return false;}
}
Decoded decodeStroke(const Bytes& b,std::size_t start,std::size_t end,int nCh,bool wordInterleaved,bool u24){
 if(nCh<1||nCh>2||start>end)throw std::runtime_error("Nord: canais/stream inválidos");View v(b,end);v.require(start,0);Decoded d;d.start=start;d.channels.resize(nCh);std::array<History,2> history{};const unsigned hb=u24?3:4,wordBits=hb*8;std::size_t o=start,index=0;bool stopped=false;
 while(o+hb<=end){auto h=block(v.be(o,hb));if(h.linear)d.marks.push_back({(o-start)/hb,index,h.decay});o+=hb;
  if(wordInterleaved&&h.count==0)continue;
  // Upstream codec4 stops on a final order0/width1 sentinel whose declared
  // samples have no residual payload. Make that implicit EOF rule explicit.
  if(wordInterleaved&&h.stop&&o==end){stopped=true;break;}
  if(!wordInterleaved&&h.stop){stopped=true;break;}
  if(h.order>7||h.count==0)throw std::runtime_error("Nord: cabeçalho de bloco inválido");
  if(index+h.count>maxStrokeValues)throw std::runtime_error("Nord: limite de PCM por stroke");
  if(wordInterleaved){
   // Codec 4 has a separate MSB stream for each channel, interleaved in 32-bit words.
   if(h.count%unsigned(nCh))throw std::runtime_error("Nord: bloco com canais incompletos");auto perCh=h.count/nCh;std::size_t words=(std::size_t(perCh)*h.width+31)/32,bytes=words*nCh*4;v.require(o,bytes);
   for(int ch=0;ch<nCh;++ch)for(unsigned i=0;i<perCh;++i){std::uint32_t raw=0;for(unsigned k=0;k<h.width;++k){auto bit=std::size_t(i)*h.width+k;auto p=o+(bit/32*nCh+ch)*4;auto word=v.be(p,4);raw=(raw<<1)|((word>>(31-bit%32))&1);}emit(history[ch],d.channels[ch],signedBits(raw,h.width),h.order);}o+=bytes;
  }else{
   std::size_t bytes=((std::size_t(h.count)*h.width+wordBits-1)/wordBits)*hb;v.require(o,bytes);
   for(unsigned i=0;i<h.count;++i){auto raw=readBits(b,o*8+std::size_t(i)*h.width,h.width);auto ch=(index+i)%nCh;emit(history[ch],d.channels[ch],signedBits(raw,h.width),h.order);}o+=bytes;
  }
  index+=h.count;
 }
 if(!wordInterleaved&&!stopped)throw std::runtime_error("Nord: stream sem stop sentinel");if(wordInterleaved&&o!=end)throw std::runtime_error("Nord: resto de bloco truncado");if(d.channels.empty()||d.channels[0].empty())throw std::runtime_error("Nord: stroke sem áudio");for(const auto& ch:d.channels)if(ch.size()!=d.channels[0].size())throw std::runtime_error("Nord: canais de tamanhos diferentes");d.end=o;return d;
}
std::optional<Loop> strokeLoop(const Bytes& b,const Section& section,const Decoded& audio){
 View v(b,section.end);auto p=section.payload;if(section.size<0x31||audio.marks.empty())return {};
 auto u1=v.be(p+0x12,4),u2=v.be(p+0x1b,4),u3=v.be(p+0x24,4),u4=v.be(p+0x2d,4);if(!(u1<=u2&&u2<=u3&&u3<=u4)||u4-u1==0||u4-u1>(1u<<27))return {};
 auto frameAt=[&](std::uint32_t word,std::int64_t shift)->std::optional<std::size_t>{auto key=std::int64_t(word)+shift;if(key<0)return {};for(const auto& mark:audio.marks)if(mark.word==static_cast<std::size_t>(key))return mark.sample/audio.channels.size();return {};};
 auto span=u4-u1;std::array<std::int64_t,2> shifts{{std::int64_t(audio.marks.back().word)-span,0}};
 for(auto shift:shifts){auto start=frameAt(u3-u1,shift);if(u3!=u4&&!start)continue;Loop loop;loop.end=audio.channels[0].size();loop.start=u3!=u4?*start:loop.end;loop.alt=frameAt(u2-u1,shift).value_or(0);loop.loops=u3!=u4&&loop.start<loop.end;loop.decayDbPerSecond=loop.loops?nsmp::loopDecayDbPerSecond(v.be(p+0x28,3)):0;return loop;}return {};
}
Stroke decodeStrokeSection(const Bytes& b,const Identity& id,const Section& section){
 View v(b,section.end);v.require(section.payload,0x31);auto p=section.payload;int hint=static_cast<int>(v.at(p+8));if(hint!=1&&hint!=2)hint=0;std::optional<Decoded> best;double bestPeak=0;
 if(id.legacy){
  for(bool openingOnly:{true,false}){
   for(auto start=p+0x31;start<std::min(section.end,p+0x260);++start){if(start+3>section.end)break;auto h=block(v.be(start,3));if(openingOnly&&!(h.order==0&&h.linear))continue;if(!reaches(b,start,section.end,true,openingOnly?3:21))continue;
    std::vector<int> channels=hint?std::vector<int>{hint}:std::vector<int>{2,1};for(auto ch:channels)try{auto d=decodeStroke(b,start,section.end,ch,false,true);auto amplitude=peak(d);if(d.channels[0].size()>=100&&amplitude<(1<<26)&&(!best||amplitude<bestPeak)){best=std::move(d);bestPeak=amplitude;}}catch(const std::exception&){}
    if(best&&bestPeak<(1<<14))break;
   }if(best)break;
  }
 }else{
  std::array<int,2> channels=hint==2?std::array<int,2>{{2,1}}:std::array<int,2>{{1,2}};
  for(std::size_t hdr=0x60;hdr<=0x180&&!best;hdr+=4){auto start=p+hdr;if(start>=section.end)break;for(auto ch:channels)try{auto d=decodeStroke(b,start,section.end,ch,id.codec==4,false);if(d.end>=section.end-std::min<std::size_t>(8,section.size)&&peak(d)<(1<<26)&&d.channels[0].size()>100){best=std::move(d);break;}}catch(const std::exception&){} }
 }
 if(!best)throw std::runtime_error("Nord: não foi possível decodificar um stroke NW1; original preservado");Stroke stroke;stroke.id=v.be(p,4);stroke.rate=static_cast<int>(v.be(p+6,2));if(stroke.rate<8000||stroke.rate>96000)stroke.rate=35002;stroke.section=section;stroke.audio=std::move(*best);stroke.loop=nsmp::strokeLoop(b,section,stroke.audio);return stroke;
}
}

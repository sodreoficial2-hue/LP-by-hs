#pragma once
// SPDX-License-Identifier: AGPL-3.0-or-later
// C++ port of simonflore/opennord @67ba1b657ff65a803d568f0ba1fba08598fdc830.
// See third_party/opennord/NOTICE.md and LICENSE. No audio callback work.
#include "timbre_format.hpp"
#include <array>
#include <map>
#include <optional>
namespace lp::nsmp {
using Bytes=std::vector<std::uint8_t>;
constexpr std::size_t maxPcmValues=64000000,maxStrokeValues=16000000;
struct View {
 const Bytes& b;std::size_t end;
 explicit View(const Bytes& bytes):b(bytes),end(bytes.size()){}
 View(const Bytes& bytes,std::size_t limit):b(bytes),end(limit){if(limit>bytes.size())throw std::runtime_error("Nord: limite inválido");}
 void require(std::size_t p,std::size_t n)const{if(p>end||n>end-p)throw std::runtime_error("Nord: registro truncado");}
 unsigned at(std::size_t p)const{require(p,1);return b[p];}
 std::uint32_t be(std::size_t p,unsigned n)const{require(p,n);std::uint32_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|b[p+i];return v;}
 std::uint32_t le(std::size_t p,unsigned n)const{require(p,n);std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint32_t(b[p+i])<<(8*i);return v;}
};
struct Identity {CbinHeader header;int codec=0;bool legacy=false;std::size_t body=0,headerSize=12,end=0;};
struct Section {std::string tag;std::uint32_t version=0;std::size_t header=0,payload=0,size=0,end=0;};
struct Zone {
 std::uint32_t id=0;int root=60,low=0,high=127,velLow=0,velHigh=127,mode=0,playback=0,oneShot=0;
 std::size_t record=0,strokeEntry=0;double level=1,detune=0;
};
struct Mark {std::size_t word=0,sample=0;bool decay=false;};
struct Decoded {std::vector<std::vector<std::int32_t>> channels;std::vector<Mark> marks;std::size_t end=0,start=0;};
struct Loop {std::size_t start=0,end=0,alt=0;bool loops=false;double decayDbPerSecond=0;};
struct Stroke {std::uint32_t id=0;int rate=35002;Section section;Decoded audio;std::optional<Loop> loop;};
Bytes readFile(const std::filesystem::path& path);
CbinHeader readCbin(const Bytes& b);
Identity identifyNsmp(const Bytes& b);
std::vector<Section> parseNsmpSections(const Bytes& b,const Identity& id);
std::string readName(const Bytes& b,const Section& section);
std::vector<Zone> readNsmpZones(const Bytes& b,const Identity& id,const std::vector<Section>& sections);
std::vector<std::array<std::uint8_t,6>> readPerNoteRows(const Bytes& b,const Identity& id,const Section& map);
Decoded decodeStroke(const Bytes& b,std::size_t start,std::size_t end,int channels,bool wordInterleaved,bool u24);
Stroke decodeStrokeSection(const Bytes& b,const Identity& id,const Section& section);
std::optional<Loop> strokeLoop(const Bytes& b,const Section& section,const Decoded& audio);
std::uint32_t readBits(const Bytes& b,std::size_t bit,unsigned width);
void writeBits(Bytes& b,std::size_t bit,unsigned width,std::uint32_t value);
std::uint32_t crc32(const Bytes& b,std::size_t first,std::size_t end);
std::uint16_t crc16(const Bytes& b,std::size_t end);
bool verifyChecksum(const Bytes& b,const Identity& id);
double dsp2Level(std::uint32_t raw);
double dsp2Detune(std::int32_t raw);
double loopDecayDbPerSecond(std::uint32_t raw);
}

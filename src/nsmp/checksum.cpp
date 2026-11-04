// SPDX-License-Identifier: AGPL-3.0-or-later
// Port: OpenNord clavia/checksum.ts and crc16.ts.
#include "nsmp.hpp"
namespace lp::nsmp {
std::uint32_t crc32(const Bytes& b,std::size_t first,std::size_t end){View(b).require(first,end>=first?end-first:b.size()+1);std::uint32_t crc=0xffffffffu;for(auto i=first;i<end;++i){crc^=b[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}return crc^0xffffffffu;}
std::uint16_t crc16(const Bytes& b,std::size_t end){View(b).require(0,end);std::uint16_t crc=0xffff;for(std::size_t i=0;i<end;++i){crc^=std::uint16_t(b[i])<<8;for(int j=0;j<8;++j)crc=static_cast<std::uint16_t>((crc<<1)^((crc&0x8000)?0x1021:0));}return crc;}
bool verifyChecksum(const Bytes& b,const Identity& id){if(id.header.formatType==1)return crc32(b,44,b.size())==id.header.crc32;return crc16(b,id.end)==View(b).le(id.end,2);}
}

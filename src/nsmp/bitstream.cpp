// SPDX-License-Identifier: AGPL-3.0-or-later
// Port: OpenNord clavia/bitstream.ts, with strict bounds instead of zero padding.
#include "nsmp.hpp"
namespace lp::nsmp {
std::uint32_t readBits(const Bytes& b,std::size_t bit,unsigned width){if(width>32||bit>b.size()*8||width>b.size()*8-bit)throw std::runtime_error("Nord: bit-field truncado");std::uint32_t v=0;for(unsigned i=0;i<width;++i){auto p=bit+i;v=(v<<1)|((b[p/8]>>(7-p%8))&1);}return v;}
void writeBits(Bytes& b,std::size_t bit,unsigned width,std::uint32_t value){if(width>32||bit>b.size()*8||width>b.size()*8-bit)throw std::runtime_error("Nord: escrita de bits fora do registro");for(unsigned i=0;i<width;++i){auto p=bit+i;auto mask=std::uint8_t(1u<<(7-p%8));b[p/8]=static_cast<std::uint8_t>((b[p/8]&~mask)|(((value>>(width-1-i))&1)?mask:0));}}
}

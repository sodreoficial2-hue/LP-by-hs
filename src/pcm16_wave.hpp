#pragma once
// Exact little-endian RIFF/WAVE PCM16 layout emitted by the pinned libsndfile
// path. Byte equivalence is a release gate, not inferred from equal PCM alone.
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <limits>
namespace lp {
inline std::vector<std::uint8_t> pcm16Wave(int rate,int channels,const std::vector<std::int16_t>& pcm){
 if(rate<8000||rate>192000||channels<1||channels>2||pcm.empty()||pcm.size()%channels||pcm.size()>(std::numeric_limits<std::uint32_t>::max()-36u)/2u)throw std::runtime_error("WAV: áudio PCM16 inválido");
 std::vector<std::uint8_t> bytes(44+pcm.size()*2);
 auto put=[&](std::size_t at,std::uint32_t value,unsigned n){for(unsigned i=0;i<n;++i)bytes[at+i]=std::uint8_t(value>>(8*i));};
 auto tag=[&](std::size_t at,const char* text){for(int i=0;i<4;++i)bytes[at+i]=std::uint8_t(text[i]);};
 tag(0,"RIFF");put(4,36+pcm.size()*2,4);tag(8,"WAVE");tag(12,"fmt ");put(16,16,4);put(20,1,2);put(22,channels,2);put(24,rate,4);put(28,rate*channels*2,4);put(32,channels*2,2);put(34,16,2);tag(36,"data");put(40,pcm.size()*2,4);
 for(std::size_t i=0;i<pcm.size();++i)put(44+i*2,static_cast<std::uint16_t>(pcm[i]),2);
 return bytes;
}
}

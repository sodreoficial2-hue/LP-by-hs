#pragma once
#include "sf2_engine.hpp"
#include <fstream>
#include <cstdint>
#include <stdexcept>
namespace lp {
// Read only RIFF/preset metadata, never load sample PCM during expansion discovery.
inline std::vector<Sf2Preset> readSf2PresetHeaders(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("SF2 indisponivel");
 const auto bytes=std::filesystem::file_size(path);
 auto read=[&](char* p,std::size_t n){if(!in.read(p,static_cast<std::streamsize>(n)))throw std::runtime_error("SF2 truncado");};
 auto u16=[](const char* p){return unsigned(static_cast<unsigned char>(p[0]))|(unsigned(static_cast<unsigned char>(p[1]))<<8);};
 auto u32=[&](const char* p){return std::uint64_t(u16(p))|(std::uint64_t(u16(p+2))<<16);};
 char root[12];read(root,12);if(std::string(root,4)!="RIFF"||std::string(root+8,4)!="sfbk"||u32(root+4)+8>bytes)throw std::runtime_error("Formato SF2 invalido");
 const auto end=u32(root+4)+8;
 for(std::uint64_t at=12;at+8<=end;){
  in.seekg(at);char chunk[8];read(chunk,8);auto size=u32(chunk+4),next=at+8+size;
  if(next>end)throw std::runtime_error("Chunk SF2 truncado");
  if(std::string(chunk,4)=="LIST"&&size>=4){char kind[4];read(kind,4);
   if(std::string(kind,4)=="pdta")for(std::uint64_t pos=at+12;pos+8<=next;){
    in.seekg(pos);char header[8];read(header,8);auto length=u32(header+4),stop=pos+8+length;
    if(stop>next)throw std::runtime_error("Metadados SF2 truncados");
    if(std::string(header,4)=="phdr"){
     if(length<38||length%38||length/38>65536)throw std::runtime_error("Presets SF2 invalidos");
     std::vector<Sf2Preset> result;
     for(std::uint64_t n=0;n<length/38;++n){char item[38];read(item,38);if(n+1==length/38)break;
      auto bank=u16(item+22),program=u16(item+20);if(bank>16383||program>127)throw std::runtime_error("Endereco SF2 invalido");
      std::size_t len=0;while(len<20&&item[len])++len;result.push_back({static_cast<int>(bank),static_cast<int>(program),std::string(item,len)});
     }
     return result;
    }
    pos=stop+(length&1);
   }
  }
  at=next+(size&1);
 }
 throw std::runtime_error("SF2 sem presets");
}
}

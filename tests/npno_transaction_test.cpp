#include "sfz_conversion.hpp"
#include <fstream>
#include <atomic>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=4)throw std::runtime_error("fixture sndfile output");
 auto root=std::filesystem::absolute(argv[3]);std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 for(std::size_t n:{0u,1u,55u,56u,63u,64u,65u,127u,128u,1024u}){
  std::vector<std::uint8_t> bytes(n);for(std::size_t i=0;i<n;++i)bytes[i]=std::uint8_t(i*37);
  lp::atomicWrite(root/"digest.bin",std::string(bytes.begin(),bytes.end()));
  if(lp::sha256Bytes(bytes)!=lp::sha256File(root/"digest.bin"))throw std::runtime_error("SHA256 RIFF buffer padding mismatch");
 }
 std::filesystem::path original=argv[1];auto originalHash=lp::sha256File(original);
 std::atomic<unsigned> progress{0};lp::ConversionOptions options;
 options.cancelled=[&]{return progress.load()>=2;};
 bool cancelled=false;
 try{lp::importarTimbreConvertido(original,root/"imports",argv[2],{},[&](const std::string& text){if(text.rfind("convertendo amostra ",0)==0)++progress;},options);}
 catch(const lp::ConversionCancelled&){cancelled=true;}
 if(!cancelled||lp::sha256File(original)!=originalHash)throw std::runtime_error("Cancellation did not preserve the original");
 if(std::filesystem::exists(root/"imports")&&!std::filesystem::is_empty(root/"imports"))throw std::runtime_error("Cancellation left staged or published bank files");
 lp::WavWriter writer(argv[2]);std::vector<std::int16_t> pcm{-32768,-1,0,1,32767,1234};std::string hash;
 writer.writePcm16Hashed(root/"stereo.wav",48000,pcm,2,&hash);
 if(hash!=lp::sha256File(root/"stereo.wav"))throw std::runtime_error("WAV hash does not describe written bytes");
 std::cout<<"PASS: SHA256 padding boundaries, interleaved RIFF hash, cancellation rollback and original preservation\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "sfz_conversion.hpp"
#include <iomanip>
namespace lp {
void WavWriter::unload(){if(!lib)return;
#ifdef _WIN32
 FreeLibrary(lib);
#else
 dlclose(lib);
#endif
 lib=nullptr;}
WavWriter::WavWriter(const std::filesystem::path& library){
#ifdef _WIN32
 lib=LoadLibraryExW(std::filesystem::absolute(library).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
 lib=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
 if(!lib)throw std::runtime_error("libsndfile indisponível para gravar WAV");
#ifdef _WIN32
 open=reinterpret_cast<decltype(open)>(symbol("sf_wchar_open"));
#else
 open=reinterpret_cast<decltype(open)>(symbol("sf_open"));
#endif
 writeFrames=reinterpret_cast<decltype(writeFrames)>(symbol("sf_writef_float"));closeFile=reinterpret_cast<decltype(closeFile)>(symbol("sf_close"));if(!open||!writeFrames||!closeFile){unload();throw std::runtime_error("libsndfile sem funções de gravação WAV");}
}
void WavWriter::write(const std::filesystem::path& path,int rate,int channels,const std::vector<float>& pcm){
 if(rate<8000||rate>192000||channels<1||channels>2||pcm.empty()||pcm.size()%channels)throw std::runtime_error("WAV: áudio inválido");for(float x:pcm)if(!std::isfinite(x))throw std::runtime_error("WAV: PCM não finito");
 Info info{};info.samplerate=rate;info.channels=channels;info.format=0x10006; // SF_FORMAT_WAV | SF_FORMAT_FLOAT, no 16-bit quantization.
 auto file=open(path.c_str(),0x20,&info);if(!file)throw std::runtime_error("Não foi possível criar WAV");bool closed=false;
 try{std::int64_t pos=0,count=static_cast<std::int64_t>(pcm.size()/channels);while(pos<count){auto written=writeFrames(file,pcm.data()+pos*channels,std::min<std::int64_t>(8192,count-pos));if(written<=0)throw std::runtime_error("Falha na gravação WAV");pos+=written;}auto result=closeFile(file);closed=true;if(result)throw std::runtime_error("Falha ao concluir WAV");}catch(...){if(!closed)closeFile(file);std::error_code ec;std::filesystem::remove(path,ec);throw;}
}
std::ostringstream sfzStream(){std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(12)<<"// Live Performance offline conversion; see original .meta.json\n";return s;}
void WavWriter::writePcm16(const std::filesystem::path& path,int rate,const std::vector<std::int16_t>& pcm,int channels){
 if(rate<8000||rate>192000||pcm.empty()||channels<1||channels>2||pcm.size()%channels)throw std::runtime_error("WAV: áudio PCM16 inválido");
 auto writeShort=reinterpret_cast<std::int64_t (*)(void*,const std::int16_t*,std::int64_t)>(symbol("sf_writef_short"));
 if(!writeShort)throw std::runtime_error("libsndfile sem função de gravação PCM16");
 Info info{};info.samplerate=rate;info.channels=channels;info.format=0x10002;
 auto file=open(path.c_str(),0x20,&info);if(!file)throw std::runtime_error("Não foi possível criar WAV PCM16");bool closed=false;
 try{std::int64_t pos=0,count=static_cast<std::int64_t>(pcm.size()/channels);while(pos<count){auto n=writeShort(file,pcm.data()+pos*channels,std::min<std::int64_t>(8192,count-pos));if(n<=0)throw std::runtime_error("Falha ao gravar WAV PCM16");pos+=n;}auto result=closeFile(file);closed=true;if(result)throw std::runtime_error("Falha ao concluir WAV PCM16");}
 catch(...){if(!closed)closeFile(file);std::error_code ignored;std::filesystem::remove(path,ignored);throw;}
}
void writeSfzGroup(std::ostream& out){out<<"<group>\n";}
double linearGainDb(double gain){return std::isfinite(gain)&&gain>0?std::max(-144.0,20*std::log10(gain)):-144.0;}
bool writeSfzRegion(std::ostream& s,const ConvertedZone& z,std::vector<std::string>* warnings){
 if(z.muted||!std::isfinite(z.volumeDb)){
  if(warnings)warnings->push_back("Região omitida: ganho nulo, negativo ou não finito: "+z.sample);
  return false;
 }
 if(!std::isfinite(z.moduleGainDb)||!std::isfinite(z.decaySeconds)||!std::isfinite(z.sustainPercent)||z.sustainPercent>100)throw std::runtime_error("SFZ: ganho de módulo/decay/sustain inválido");
 if(!std::isfinite(z.tuneCents)||!std::isfinite(z.pan)||!std::isfinite(z.releaseSeconds))throw std::runtime_error("SFZ: afinação/pan/envelope não finito");
 if(z.root<0||z.root>127||z.keyLow<0||z.keyHigh>127||z.keyLow>z.keyHigh||z.velLow<0||z.velHigh>127||z.velLow>z.velHigh)throw std::runtime_error("SFZ: faixa MIDI inválida");
 auto path=z.sample;std::replace(path.begin(),path.end(),'\\','/');
 auto component=std::filesystem::u8path(path);
 if(path.empty()||path[0]=='/'||path.find(':')!=std::string::npos||path.find('\n')!=std::string::npos||path.find('\r')!=std::string::npos||path.find('=')!=std::string::npos||path.find('<')!=std::string::npos||path.find('>')!=std::string::npos)throw std::runtime_error("SFZ: caminho de amostra inválido");
 for(const auto& part:component)if(part=="..")throw std::runtime_error("SFZ: caminho de amostra sai da pasta");
 auto end=z.sampleEnd?z.sampleEnd:z.frames;
 if(!z.frames||z.sampleStart>=end||end>z.frames)throw std::runtime_error("SFZ: limites da amostra inválidos");
 if((z.loop||z.sourceLoopMode>0)&&(z.loopStart<z.sampleStart||z.loopStart>=z.loopEnd||z.loopEnd>end))throw std::runtime_error("SFZ: loop fora dos limites da amostra");
 if((!z.sequenceLength&&z.sequencePosition)||z.sequenceLength<0||z.sequenceLength>100||z.sequencePosition<0||(z.sequenceLength&&(!z.sequencePosition||z.sequencePosition>z.sequenceLength)))throw std::runtime_error("SFZ: sequência RR inválida");
 // All generated names are ASCII, relative, and controlled by the converter.
 s<<"<region> sample="<<path<<" lokey="<<z.keyLow<<" hikey="<<z.keyHigh<<" pitch_keycenter="<<z.root<<" lovel="<<z.velLow<<" hivel="<<z.velHigh<<" offset="<<z.sampleStart<<" end="<<end-1;
 if(z.zoneMode==1)s<<" pitch_keytrack=0";
 int transpose=static_cast<int>(std::trunc(z.tuneCents/100));if(transpose<-127||transpose>127)throw std::runtime_error("Afinação excede limites SFZ; original preservado");
 s<<" volume="<<std::max(-144.0,z.volumeDb+z.moduleGainDb)<<" transpose="<<transpose<<" tune="<<z.tuneCents-transpose*100;
 if(z.softVelocityCeiling<0||z.softVelocityCeiling>127)throw std::runtime_error("SFZ: limite de dinâmica suave inválido");
 if(z.softVelocityCeiling){s<<" amp_veltrack=100 amp_velcurve_0=0 amp_velcurve_"<<z.softVelocityCeiling<<"=1";if(z.softVelocityCeiling<127)s<<" amp_velcurve_127=1";}
 else if(z.ampVeltrack!=100)s<<" amp_veltrack="<<z.ampVeltrack;
 if(z.hasPan)s<<" pan="<<z.pan;
 if(z.releaseTrigger)s<<" trigger=release_key";
 if(z.releaseSeconds>=0)s<<" ampeg_release="<<z.releaseSeconds;
 if(z.decaySeconds>=0)s<<" ampeg_decay="<<z.decaySeconds;
 if(z.sustainPercent>=0)s<<" ampeg_sustain="<<z.sustainPercent;
 if(z.cc64Low>=0)s<<" locc64="<<z.cc64Low;
 if(z.cc64High>=0)s<<" hicc64="<<z.cc64High;
 if(z.sequenceLength>0)s<<" seq_length="<<z.sequenceLength<<" seq_position="<<z.sequencePosition;
 if(z.xfinLowVel>=0)s<<" xfin_lovel="<<z.xfinLowVel<<" xfin_hivel="<<z.xfinHighVel;
 if(z.xfoutLowVel>=0)s<<" xfout_lovel="<<z.xfoutLowVel<<" xfout_hivel="<<z.xfoutHighVel;
 if(z.sourceLoopMode==0)s<<" loop_mode=no_loop";
 else if(z.sourceLoopMode==1)s<<" loop_mode=loop_sustain loop_start="<<z.loopStart<<" loop_end="<<z.loopEnd-1;
 else if(z.sourceLoopMode==2)s<<" loop_mode=loop_continuous loop_start="<<z.loopStart<<" loop_end="<<z.loopEnd-1;
 else if(z.zoneOneShot)s<<" loop_mode=one_shot";
 else if(z.loop)s<<" loop_mode=loop_continuous loop_start="<<z.loopStart<<" loop_end="<<z.loopEnd-1; // sfizz uses inclusive ends; Nord frames are exclusive.
 else s<<" loop_mode=no_loop";
 s<<'\n';return true;
}
std::string bytesHex(const std::uint8_t* data,std::size_t size){static const char hex[]="0123456789abcdef";std::string s;s.reserve(size*2);for(std::size_t i=0;i<size;++i){s+=hex[data[i]>>4];s+=hex[data[i]&15];}return s;}
}
namespace lp {
ConversionOutput::ConversionOutput(const std::filesystem::path& directory):dir(directory){if(dir.empty()||std::filesystem::exists(dir))throw std::runtime_error("Use uma pasta nova para converter; saída existente preservada: "+dir.u8string());if(!dir.parent_path().empty())std::filesystem::create_directories(dir.parent_path());if(!std::filesystem::create_directory(dir))throw std::runtime_error("Não foi possível criar pasta de conversão");}
std::filesystem::path ConversionOutput::finish(const std::filesystem::path& original,MetaSidecar& meta,const std::string& sfz){
 auto copy=dir/original.filename();std::filesystem::copy_file(original,copy);if(sha256File(copy)!=meta.originalSha256)throw std::runtime_error("Original mudou durante a conversão");auto output=derivedSfz(copy);atomicWrite(output,sfz);auto sidecar=copy;sidecar+=".meta.json";atomicWrite(sidecar,meta.json());done=true;return output;
}
}

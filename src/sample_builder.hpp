#pragma once
// Offline sampler builder. No work in this file runs on the audio callback.
#include "atomic_file.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <regex>
#include <vector>
#ifndef _WIN32
#include <dlfcn.h>
#endif
namespace lp::sampling {
constexpr double pi=3.14159265358979323846;
struct Sample {
 std::string source;
 int rate=44100,channels=1,root=-1,velocity=0,low=0,high=127,velLow=1,velHigh=127;
 double cents=0,confidence=0,energy=0;
 bool explicitVelocity=false,loop=false;
 std::uint32_t loopStart=0,loopEnd=0;
 std::vector<float> pcm;
 std::size_t frames()const{return pcm.size()/channels;}
};
struct Project {std::vector<Sample> samples;std::vector<std::string> warnings;};
inline void cancelled(const std::function<bool()>& cancel){if(cancel&&cancel())throw std::runtime_error("Criação cancelada; arquivos originais preservados.");}

// Bind the existing portable's libsndfile; it also handles Unicode Windows paths.
class Decoder {
 struct Info {std::int64_t frames;int samplerate,channels,format,sections,seekable;};
#ifdef _WIN32
 HMODULE lib{};
 void* symbol(const char* s){return reinterpret_cast<void*>(GetProcAddress(lib,s));}
#else
 void* lib{};
 void* symbol(const char* s){return dlsym(lib,s);}
#endif
 void* (*open)(const char*,int,Info*){};
#ifdef _WIN32
 void* (*wopen)(const wchar_t*,int,Info*){};
#endif
 std::int64_t (*read)(void*,float*,std::int64_t){};int (*closeFile)(void*){};
 public:
 explicit Decoder(const std::filesystem::path& library){
#ifdef _WIN32
 lib=LoadLibraryExW(std::filesystem::absolute(library).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
 lib=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
 if(!lib)throw std::runtime_error("Biblioteca sndfile indisponível para importar áudio.");
 open=reinterpret_cast<decltype(open)>(symbol("sf_open"));read=reinterpret_cast<decltype(read)>(symbol("sf_readf_float"));closeFile=reinterpret_cast<decltype(closeFile)>(symbol("sf_close"));
#ifdef _WIN32
 wopen=reinterpret_cast<decltype(wopen)>(symbol("sf_wchar_open"));
#endif
 if(!open||!read||!closeFile){unload();throw std::runtime_error("Biblioteca de áudio incompatível.");}
 }
 ~Decoder(){unload();}Decoder(const Decoder&)=delete;Decoder& operator=(const Decoder&)=delete;
 void unload(){if(lib){
#ifdef _WIN32
 FreeLibrary(lib);
#else
 dlclose(lib);
#endif
 lib=nullptr;}}
 Sample decode(const std::filesystem::path& path,const std::function<bool()>& cancel={}){
 Info info{};void* file=nullptr;
#ifdef _WIN32
 if(!wopen)throw std::runtime_error("sndfile sem suporte a caminhos Unicode.");
 file=wopen(path.c_str(),0x10,&info);
#else
 file=open(path.c_str(),0x10,&info);
#endif
 if(!file)throw std::runtime_error("Áudio inválido ou formato não suportado: "+path.filename().u8string());
 struct Guard {void* file;int (*close)(void*);~Guard(){close(file);}} guard{file,closeFile};
 if(info.frames<32||info.samplerate<8000||info.samplerate>192000||info.channels<1||info.channels>2||info.frames>std::int64_t(info.samplerate)*120||info.frames*info.channels>32000000)
  throw std::runtime_error("Use áudio mono/estéreo de até 120 segundos e 8–192 kHz (máximo de 32 milhões de amostras).");
 Sample s;s.source=path.filename().u8string();s.rate=info.samplerate;s.channels=info.channels;s.pcm.resize(static_cast<std::size_t>(info.frames)*s.channels);
 std::int64_t position=0;
 while(position<info.frames){cancelled(cancel);auto n=read(file,s.pcm.data()+position*s.channels,std::min<std::int64_t>(8192,info.frames-position));if(n<=0)throw std::runtime_error("Áudio incompleto: "+s.source);position+=n;}
 for(float value:s.pcm)if(!std::isfinite(value)||std::abs(value)>16)throw std::runtime_error("Áudio contém valores inválidos: "+s.source);
 return s;
 }
};
inline void metadata(Sample& s){
 std::smatch m;const auto& name=s.source;
 if(std::regex_search(name,m,std::regex("(?:^|[^a-zA-Z0-9])(?:midi|note|key)([0-9]{1,3})(?:[^0-9]|$)",std::regex::icase))){int n=std::stoi(m[1]);if(n<=127)s.root=n;}
 else if(std::regex_search(name,m,std::regex("(?:^|[^a-zA-Z0-9])([A-Ga-g])([#b]?)(-?[0-9])(?:[^a-zA-Z0-9]|$)"))){
  const std::string letters="C D EF G A B";char c=static_cast<char>(std::toupper(m[1].str()[0]));int semitone=static_cast<int>(letters.find(c));int accidental=m[2]=="#"?1:(m[2]=="b"?-1:0);int note=12*(std::stoi(m[3])+1)+semitone+accidental;if(note>=0&&note<=127)s.root=note;
 }
 if(std::regex_search(name,m,std::regex("(?:^|[^a-zA-Z0-9])(?:vel|velocity|v)([0-9]{1,3})(?:[^0-9]|$)",std::regex::icase))){int v=std::stoi(m[1]);if(v>=1&&v<=127){s.velocity=v;s.explicitVelocity=true;}}
}
inline double rms(const Sample& s,std::size_t first,std::size_t last){double sum=0;for(auto i=first;i<last;++i)for(int c=0;c<s.channels;++c){double x=s.pcm[i*s.channels+c];sum+=x*x;}return last>first?std::sqrt(sum/((last-first)*s.channels)):0;}
// Silence-separated notes become independent samples. A 180 ms gap prevents
// cutting the normal waveform zero crossings or short tremolo troughs.
inline std::vector<Sample> split(const Sample& input){
 const auto n=input.frames();const auto hop=std::max(1,input.rate/100);double peak=0;
 for(float x:input.pcm)peak=std::max(peak,std::abs(double(x)));
 if(peak<1e-6)throw std::runtime_error("Gravação silenciosa: "+input.source);
 const double threshold=std::max(1e-6,peak*.001);std::vector<std::pair<std::size_t,std::size_t>> zones;
 std::size_t start=n,last=0;
 for(std::size_t i=0;i<n;i+=hop){auto end=std::min(n,i+hop);double level=rms(input,i,end);if(level>threshold){if(start==n)start=i;last=end;}else if(start!=n&&i-last>=static_cast<std::size_t>(input.rate*.18)){zones.emplace_back(start,last);start=n;}}
 if(start!=n)zones.emplace_back(start,last);
 std::vector<Sample> result;
 for(auto zone:zones){if(zone.second-zone.first<static_cast<std::size_t>(input.rate*.04))continue;auto first=zone.first>static_cast<std::size_t>(hop)?zone.first-hop:0;auto end=std::min(n,zone.second+hop);Sample s;s.source=input.source;s.rate=input.rate;s.channels=input.channels;s.root=input.root;s.velocity=input.velocity;s.explicitVelocity=input.explicitVelocity;s.pcm.assign(input.pcm.begin()+first*s.channels,input.pcm.begin()+end*s.channels);s.energy=rms(s,0,std::min(s.frames(),static_cast<std::size_t>(s.rate*.3)));result.push_back(std::move(s));}
 if(result.empty())throw std::runtime_error("Nenhuma nota com duração suficiente: "+input.source);
 return result;
}
// YIN difference/CMND, with a modest analysis rate. First convincing minimum
// avoids the octave errors of simply choosing the largest autocorrelation peak.
inline void pitch(Sample& s){
 if(s.root>=0){s.confidence=1;return;}
 const int stride=std::max(1,s.rate/12000);const double rate=double(s.rate)/stride;
 const std::size_t offset=std::min(s.frames()/8,static_cast<std::size_t>(s.rate*.08));
 const auto count=std::min<std::size_t>(8192,(s.frames()-offset)/stride);if(count<128)return;
 std::vector<double> x(count);int channel=0;
 if(s.channels==2){double a=0,b=0;for(std::size_t i=offset;i<s.frames();i+=stride){a+=std::abs(s.pcm[i*2]);b+=std::abs(s.pcm[i*2+1]);}channel=b>a?1:0;}
 for(std::size_t i=0;i<count;++i){double sum=0;for(int j=0;j<stride;++j)sum+=s.pcm[(offset+i*stride+j)*s.channels+channel];x[i]=sum/stride;}
 const int maxLag=std::min<int>(count/2,static_cast<int>(rate/24));const int minLag=std::max(2,static_cast<int>(rate/4200));
 std::vector<double> d(maxLag+1,1);double cumulative=0;const auto window=count-maxLag;
 for(int lag=1;lag<=maxLag;++lag){double sum=0;for(std::size_t i=0;i<window;++i){double v=x[i]-x[i+lag];sum+=v*v;}cumulative+=sum;d[lag]=cumulative>1e-14?sum*lag/cumulative:1;}
 int chosen=0;
 for(int lag=minLag;lag<maxLag;++lag)if(d[lag]<.15){while(lag+1<maxLag&&d[lag+1]<d[lag])++lag;chosen=lag;break;}
 if(!chosen)return;
 double lag=chosen;if(chosen>1&&chosen<maxLag){double denominator=d[chosen-1]-2*d[chosen]+d[chosen+1];if(std::abs(denominator)>1e-12)lag+=std::clamp(.5*(d[chosen-1]-d[chosen+1])/denominator,-.5,.5);}
 double midi=69+12*std::log2(rate/lag/440);int root=static_cast<int>(std::lround(midi));s.confidence=1-d[chosen];
 if(root>=0&&root<=127){s.root=root;s.cents=(midi-root)*100;}
}
inline void findLoop(Sample& s){
 s.loop=false;s.loopStart=s.loopEnd=0;if(s.root<0||s.frames()<static_cast<std::size_t>(s.rate*.8))return;
 auto a=s.frames()*4/10,b=s.frames()*8/10;double early=rms(s,a,a+s.rate/10),late=rms(s,b,b+s.rate/10);
 if(early<1e-5||late/early<.8||late/early>1.25)return; // leave decaying piano samples intact
 const double period=s.rate/(440*std::pow(2.,(s.root-69+s.cents/100)/12));
 const auto span=static_cast<std::size_t>(std::round((b-a)/period)*period);if(span<128||a+span+128>=s.frames())return;
 double best=1e9;std::size_t end=0;const int radius=std::max(2,static_cast<int>(period/2));
 for(int delta=-radius;delta<=radius;++delta){auto candidate=static_cast<std::ptrdiff_t>(a+span)+delta;if(candidate<=static_cast<std::ptrdiff_t>(a+128)||candidate+128>=static_cast<std::ptrdiff_t>(s.frames()))continue;double diff=0,power=0;
  for(std::size_t j=0;j<128;++j)for(int c=0;c<s.channels;++c){double x=s.pcm[(a+j)*s.channels+c],y=s.pcm[(candidate+j)*s.channels+c];diff+=(x-y)*(x-y);power+=x*x+y*y;}
  double error=diff/std::max(1e-12,power);if(error<best){best=error;end=candidate;}}
 if(best<.005){s.loop=true;s.loopStart=static_cast<std::uint32_t>(a);s.loopEnd=static_cast<std::uint32_t>(end);}
}
inline void map(Project& p){
 std::map<int,std::vector<Sample*>> groups;for(auto& s:p.samples)if(s.root>=0&&s.root<=127)groups[s.root].push_back(&s);
 for(auto group=groups.begin();group!=groups.end();++group){auto& layers=group->second;if(layers.size()>127)throw std::runtime_error("Mais de 127 camadas para uma nota; reduza as amostras.");
  int low=group==groups.begin()?0:(std::prev(group)->first+group->first)/2+1;
  int high=std::next(group)==groups.end()?127:(group->first+std::next(group)->first)/2;
  std::stable_sort(layers.begin(),layers.end(),[](auto* a,auto* b){return a->energy<b->energy;});
  for(std::size_t i=0;i<layers.size();++i)if(!layers[i]->explicitVelocity)layers[i]->velocity=layers.size()==1?80:1+static_cast<int>(std::lround(126.*i/(layers.size()-1)));
  std::stable_sort(layers.begin(),layers.end(),[](auto* a,auto* b){return a->velocity<b->velocity;});
  for(std::size_t i=0;i<layers.size();++i){auto& s=*layers[i];if(i&&s.velocity==layers[i-1]->velocity)throw std::runtime_error("Duas camadas têm a mesma intensidade. Ajuste os valores no editor.");s.low=low;s.high=high;s.velLow=i?(layers[i-1]->velocity+s.velocity)/2+1:1;s.velHigh=i+1<layers.size()?(s.velocity+layers[i+1]->velocity)/2:127;}
 }
}
inline Project analyze(const std::vector<std::filesystem::path>& paths,const std::filesystem::path& library,const std::function<bool()>& cancel={}){
 if(paths.empty()||paths.size()>256)throw std::runtime_error("Selecione entre 1 e 256 arquivos.");
 Decoder decoder(library);Project project;std::size_t total=0;
 for(const auto& path:paths){cancelled(cancel);auto input=decoder.decode(path,cancel);metadata(input);auto parts=split(input);
  for(auto& s:parts){cancelled(cancel);total+=s.pcm.size();if(total>64000000||project.samples.size()>=512)throw std::runtime_error("Limite de 512 notas ou 64 milhões de amostras excedido. Importe em lotes menores.");pitch(s);findLoop(s);if(s.root<0)project.warnings.push_back("Nota não identificada: "+s.source);if(!s.explicitVelocity)project.warnings.push_back("Intensidade estimada pelo áudio: "+s.source);project.samples.push_back(std::move(s));}
 }map(project);return project;
}

// SF2.01 RIFF writer: preserve stereo via linked left/right samples and zones.
inline void u16(std::string& b,int v){b.push_back(static_cast<char>(v&255));b.push_back(static_cast<char>((v>>8)&255));}
inline void u32(std::string& b,std::uint32_t v){u16(b,v&65535);u16(b,v>>16);}
inline std::string chunk(const std::string& tag,std::string bytes){std::string result=tag;u32(result,static_cast<std::uint32_t>(bytes.size()));result+=bytes;if(bytes.size()%2)result+='\0';return result;}
inline void fixedName(std::string& bytes,const std::string& name){std::string safe;for(unsigned char c:name){if(safe.size()==19)break;safe+=c>=32&&c<127?static_cast<char>(c):'_';}safe.resize(20,'\0');bytes+=safe;}
inline void generator(std::string& b,int op,int value){u16(b,op);u16(b,value);}
inline std::string soundfont(const Project& project,const std::string& name){
 if(project.samples.empty()||project.samples.size()>512)throw std::runtime_error("Nenhuma amostra válida para exportar.");
 std::string pcm,shdr,igen,ibag;int zones=0,genCount=0,sampleIndex=0;std::uint32_t offset=0;float peak=0;
 for(const auto& s:project.samples){if(s.root<0||s.root>127||s.velLow<1||s.velHigh>127||s.velLow>s.velHigh||s.low<0||s.high>127||s.low>s.high||s.frames()<32||s.rate<8000||s.rate>192000||s.channels<1||s.channels>2||!std::isfinite(s.cents)||std::abs(s.cents)>100)throw std::runtime_error("Corrija as notas ou camadas pendentes antes de criar o SF2.");if(s.loop&&(s.loopStart>=s.loopEnd||s.loopEnd>s.frames()-2||s.loopEnd-s.loopStart<32))throw std::runtime_error("Loop inválido.");for(float v:s.pcm){if(!std::isfinite(v))throw std::runtime_error("Amostra inválida.");peak=std::max(peak,std::abs(v));}}
 // One shared downward gain prevents clipping without erasing layer differences.
 const double gain=peak>.98f?.98/peak:1;
 for(const auto& s:project.samples){for(int c=0;c<s.channels;++c){
  fixedName(shdr,"Sample "+std::to_string(sampleIndex));u32(shdr,offset);u32(shdr,offset+static_cast<std::uint32_t>(s.frames()));u32(shdr,offset+(s.loop?s.loopStart:8));u32(shdr,offset+(s.loop?s.loopEnd:static_cast<std::uint32_t>(s.frames()-8)));u32(shdr,s.rate);shdr+=static_cast<char>(s.root);shdr+=static_cast<char>(static_cast<std::int8_t>(std::lround(-s.cents)));u16(shdr,s.channels==2?sampleIndex+(c==0?1:-1):0);u16(shdr,s.channels==2?(c==0?4:2):1);
  for(std::size_t i=0;i<s.frames();++i)u16(pcm,static_cast<std::int16_t>(std::lround(std::clamp(double(s.pcm[i*s.channels+c])*gain,-1.,1.)*32767)));
  for(int guard=0;guard<46;++guard)u16(pcm,0);
  offset+=static_cast<std::uint32_t>(s.frames()+46);
  u16(ibag,genCount);u16(ibag,0);generator(igen,43,s.low|(s.high<<8));generator(igen,44,s.velLow|(s.velHigh<<8));generator(igen,34,-12000);generator(igen,38,-2400);generator(igen,54,s.loop?1:0);genCount+=5;
  if(s.channels==2){generator(igen,17,c==0?-500:500);++genCount;}
  generator(igen,53,sampleIndex++);++genCount;++zones;
 }}
 u16(ibag,genCount);u16(ibag,0);generator(igen,0,0);
 fixedName(shdr,"EOS");for(int i=0;i<5;++i)u32(shdr,0);shdr.append(6,'\0');
 std::string inst;fixedName(inst,name);u16(inst,0);fixedName(inst,"EOI");u16(inst,zones);
 std::string phdr;fixedName(phdr,name);u16(phdr,0);u16(phdr,0);u16(phdr,0);for(int i=0;i<3;++i)u32(phdr,0);fixedName(phdr,"EOP");u16(phdr,0);u16(phdr,0);u16(phdr,1);for(int i=0;i<3;++i)u32(phdr,0);
 std::string pbag;u16(pbag,0);u16(pbag,0);u16(pbag,1);u16(pbag,0);std::string pgen;generator(pgen,41,0);generator(pgen,0,0);
 std::string version;u16(version,2);u16(version,1);std::string title=name.substr(0,200);title+='\0';if(title.size()%2)title+='\0';
 auto info=chunk("LIST","INFO"+chunk("ifil",version)+chunk("isng",std::string("EMU8000\0",8))+chunk("INAM",title));
 auto sdta=chunk("LIST","sdta"+chunk("smpl",std::move(pcm)));
 auto pdta=chunk("LIST","pdta"+chunk("phdr",phdr)+chunk("pbag",pbag)+chunk("pmod",std::string(10,'\0'))+chunk("pgen",pgen)+chunk("inst",inst)+chunk("ibag",ibag)+chunk("imod",std::string(10,'\0'))+chunk("igen",igen)+chunk("shdr",shdr));
 return chunk("RIFF","sfbk"+info+sdta+pdta);
}
inline void save(const Project& project,const std::string& name,const std::filesystem::path& path){atomicWrite(path,soundfont(project,name));}
}

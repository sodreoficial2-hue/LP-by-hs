#include "timbre_format.hpp"
#include "atomic_file.hpp"
#include "nsmp/nsmp.hpp"
#include "npno_reader.hpp"
#include <locale>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace lp {
namespace {
std::array<unsigned char,64> head(const std::filesystem::path& path,std::size_t& count){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Não foi possível ler o timbre");std::array<unsigned char,64>b{};in.read(reinterpret_cast<char*>(b.data()),b.size());count=static_cast<std::size_t>(in.gcount());return b;}
std::uint32_t le(const unsigned char* b){return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
bool magic(const unsigned char* b,const char* s){return std::equal(s,s+4,b);}
std::string quote(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;else o<<c;}o<<'"';return o.str();}
// SHA-256 (FIPS 180-4), streaming original bytes; no platform crypto DLL dependency.
struct Sha256 {
 std::array<std::uint32_t,8> h{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
 static std::uint32_t rotr(std::uint32_t x,int n){return (x>>n)|(x<<(32-n));}
 void block(const unsigned char* b){
  static constexpr std::uint32_t k[]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  std::uint32_t w[64];for(int i=0;i<16;++i)w[i]=(std::uint32_t(b[4*i])<<24)|(std::uint32_t(b[4*i+1])<<16)|(std::uint32_t(b[4*i+2])<<8)|b[4*i+3];
  for(int i=16;i<64;++i){auto x=w[i-15],y=w[i-2];w[i]=w[i-16]+(rotr(x,7)^rotr(x,18)^(x>>3))+w[i-7]+(rotr(y,17)^rotr(y,19)^(y>>10));}
  auto a=h[0],c=h[2],b0=h[1],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
  for(int i=0;i<64;++i){auto t1=hh+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];auto t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b0)^(a&c)^(b0&c));hh=g;g=f;f=e;e=d+t1;d=c;c=b0;b0=a;a=t1+t2;}
  h[0]+=a;h[1]+=b0;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
 }
};
}
std::string sha256File(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Não foi possível calcular SHA-256");Sha256 s;std::array<unsigned char,64>b{};std::uint64_t length=0;std::size_t n=0;
 while(in.read(reinterpret_cast<char*>(b.data()),64)){s.block(b.data());length+=64;}
 if(in.bad()||(!in.eof()&&in.fail()))throw std::runtime_error("Falha na leitura do SHA-256");n=static_cast<std::size_t>(in.gcount());length+=n;b[n++]=0x80;
 if(n>56){std::fill(b.begin()+n,b.end(),0);s.block(b.data());n=0;}
 std::fill(b.begin()+n,b.begin()+56,0);for(int i=0;i<8;++i)b[63-i]=static_cast<unsigned char>((length*8)>>(8*i));s.block(b.data());std::ostringstream o;o<<std::hex<<std::setfill('0');for(auto v:s.h)o<<std::setw(8)<<v;return o.str();
}
std::string sha256Bytes(const std::vector<std::uint8_t>& bytes){
 Sha256 s;std::size_t at=0;while(bytes.size()-at>=64){s.block(bytes.data()+at);at+=64;}
 std::array<unsigned char,64> b{};auto n=bytes.size()-at;std::copy(bytes.begin()+at,bytes.end(),b.begin());b[n++]=0x80;
 if(n>56){s.block(b.data());b.fill(0);}
 auto length=std::uint64_t(bytes.size())*8;for(int i=0;i<8;++i)b[63-i]=static_cast<unsigned char>(length>>(8*i));s.block(b.data());std::ostringstream o;o<<std::hex<<std::setfill('0');for(auto v:s.h)o<<std::setw(8)<<v;return o.str();
}
CbinHeader lerHeaderCbin(const std::filesystem::path& path){return nsmp::readCbin(nsmp::readFile(path));}
TimbreFormat detectarVersaoNsmp(const std::filesystem::path& path){auto id=nsmp::identifyNsmp(nsmp::readFile(path));auto e=timbreExtension(path);if((e==".nsmp3"&&id.codec!=3)||(e==".nsmp4"&&id.codec!=4)||(e==".nsmp"&&id.codec>2))throw std::runtime_error("Extensão e geração Nord não correspondem");return TimbreFormat::NSMP;}
TimbreFormat detectarTimbreFormat(const std::filesystem::path& path){
 std::size_t n;auto b=head(path,n);auto e=timbreExtension(path);
 if(e==".nsmpproj")throw std::runtime_error("NSMPPROJ é um projeto de texto, não uma amostra binária; exporte NSMP/NSMP3/NSMP4 no editor de origem");
 if(n>=4&&magic(b.data(),"CBIN")){
  if(n>=12&&magic(b.data()+8,"npno")){if(npno::sniff(path))return TimbreFormat::NPNO;throw std::runtime_error("Módulo de piano truncado ou sem assinatura CNSP");}
  if(e==".npno"||e==".npmo")throw std::runtime_error("Módulo de piano: assinatura de formato desconhecida");
  return detectarVersaoNsmp(path);
 }
 if(e==".npno"||e==".npmo")throw std::runtime_error("Módulo de piano: assinatura desconhecida (CBIN/npno/CNSP ausente)");
 if(n>=12&&magic(b.data(),"RIFF")&&magic(b.data()+8,"sfbk"))return TimbreFormat::SF2;
 // Recognize the compressed SoundFont family; the normal SF2 reader rejects sfArk audio explicitly.
 if(std::search(b.begin(),b.begin()+n,"sfArk","sfArk"+5)!=b.begin()+n)return TimbreFormat::SF2;
 if(e==".nsmp"||e==".nsmp3"||e==".nsmp4")throw std::runtime_error("Amostra Nord sem assinatura CBIN");
 if(e==".nki") {if(n<4)throw std::runtime_error("NKI vazio ou truncado");if(magic(b.data(),"RIFF"))throw std::runtime_error("Arquivo de áudio renomeado como NKI");return TimbreFormat::NKI;} // Binary payload is validated by nki::read; filename fallback remains explicit.
 if(e==".sf2"||e==".sfark")return TimbreFormat::SF2;
 if(e==".sfz")return TimbreFormat::SFZ;
 std::string text(reinterpret_cast<char*>(b.data()),n);if(text.find("<region>")!=std::string::npos||text.find("<group>")!=std::string::npos||text.find("<global>")!=std::string::npos)return TimbreFormat::SFZ;return TimbreFormat::UNKNOWN;
}
std::string MetaSidecar::json()const{
 std::ostringstream o;o.imbue(std::locale::classic());o<<"{\n  \"schema\": 2,\n  \"name\": "<<quote(name)<<",\n  \"format\": "<<quote(format)<<",\n  \"format_version\": "<<quote(version)<<",\n  \"original_sha256\": "<<quote(originalSha256)<<",\n  \"signature_hex\": "<<quote(signatureHex)<<",\n  \"signature_confirmed\": "<<(signatureConfirmed?"true":"false")<<",\n  \"status\": "<<quote(status)<<",\n  \"limitations\": [";
 for(std::size_t i=0;i<limitations.size();++i){if(i)o<<',';o<<'\n'<<"    "<<quote(limitations[i]);}o<<"\n  ],\n  \"zones\": [";
 for(std::size_t i=0;i<zones.size();++i){if(i)o<<',';auto& z=zones[i];o<<"\n    {\"sample\": "<<quote(z.sample)<<", \"root_source\": "<<quote(z.rootSource)<<", \"root\": "<<z.root<<", \"key_low\": "<<z.keyLow<<", \"key_high\": "<<z.keyHigh<<", \"velocity_low\": "<<z.velLow<<", \"velocity_high\": "<<z.velHigh<<", \"sample_rate\": "<<z.rate<<", \"channels\": "<<z.channels<<", \"frames\": "<<z.frames<<", \"loop\": "<<(z.loop?"true":"false")<<", \"loop_start\": "<<z.loopStart<<", \"loop_end_exclusive\": "<<z.loopEnd<<", \"alternate_start\": "<<z.altStart<<", \"volume_db\": "<<z.volumeDb<<", \"tune_cents\": "<<z.tuneCents<<", \"loop_decay_db_per_second\": "<<z.decayDbPerSecond<<", \"raw_pcm_peak\": "<<z.pcmPeak<<", \"zone_mode\": "<<z.zoneMode<<", \"zone_playback\": "<<z.zonePlayback<<", \"zone_one_shot\": "<<z.zoneOneShot;o<<", \"pan\": "<<z.pan<<", \"pan_present\": "<<(z.hasPan?"true":"false")<<", \"release_trigger\": "<<(z.releaseTrigger?"true":"false")<<", \"release_seconds\": "<<z.releaseSeconds<<", \"cc64_low\": "<<z.cc64Low<<", \"cc64_high\": "<<z.cc64High;o<<", \"program\": "<<z.program<<", \"sample_start\": "<<z.sampleStart<<", \"sample_end_exclusive\": "<<z.sampleEnd<<", \"seq_length\": "<<z.sequenceLength<<", \"seq_position\": "<<z.sequencePosition<<", \"source_loop_mode\": "<<z.sourceLoopMode<<", \"xfin_lovel\": "<<z.xfinLowVel<<", \"xfin_hivel\": "<<z.xfinHighVel<<", \"xfout_lovel\": "<<z.xfoutLowVel<<", \"xfout_hivel\": "<<z.xfoutHighVel<<", \"source_bank\": "<<z.sourceBank<<", \"source_velocity_low\": "<<z.sourceVelLow<<", \"source_velocity_high\": "<<z.sourceVelHigh<<", \"amp_veltrack\": "<<z.ampVeltrack<<", \"calibrated\": "<<(z.calibrated?"true":"false")<<", \"calibration_offset_db\": "<<z.calibrationOffsetDb<<", \"calibration_residual_db\": "<<z.calibrationResidualDb<<", \"canonical_band\": "<<z.canonicalBand<<", \"pedal_class\": "<<z.pedalClass<<", \"source_stroke\": "<<z.sourceStroke<<", \"band_inherited\": "<<(z.bandInherited?"true":"false")<<", \"resonance\": "<<(z.npnoResonance?"true":"false")<<", \"pedal_offset_db\": "<<z.pedalOffsetDb<<", \"pedal_residual_db\": "<<z.pedalResidualDb<<", \"module_gain_db\": "<<z.moduleGainDb<<", \"source_loop_decay_q23\": "<<z.sourceLoopDecay<<", \"soft_velocity_ceiling\": "<<z.softVelocityCeiling<<", \"ampeg_decay_seconds\": "<<z.decaySeconds<<", \"ampeg_sustain_percent\": "<<z.sustainPercent<<", \"muted\": "<<(z.muted?"true":"false");if(format=="npno"&&z.releaseTrigger)o<<", \"pedal_release_only\": "<<(z.pedalReleaseOnly?"true":"false")<<", \"release_key_base_volume_db\": "<<z.releaseKeyBaseVolumeDb<<", \"release_key_pedal_passthrough\": "<<(z.releaseKeyPedalPassthrough?"true":"false");o<<'}';}o<<"\n  ],\n  \"sample_sha256\": [";for(std::size_t i=0;i<sampleHashes.size();++i){if(i)o<<',';o<<quote(sampleHashes[i]);}o<<"],\n  \"codec\": "<<codec<<",\n  \"checksum_valid\": "<<(checksumValid?"true":"false")<<",\n  \"preserved_features\": [";for(std::size_t i=0;i<preservedFeatures.size();++i){if(i)o<<',';o<<quote(preservedFeatures[i]);}o<<"]";if(format=="npno")o<<",\n  \"release_key_gain_db\": "<<releaseKeyGainDb<<",\n  \"release_key_enabled\": "<<(releaseKeyEnabled?"true":"false");if(!sourceMetadataJson.empty())o<<",\n  \"source_metadata\": "<<sourceMetadataJson;o<<"\n}\n";return o.str();
}
std::filesystem::path importarTimbreConvertido(const std::filesystem::path& original,const std::filesystem::path& destination,const std::filesystem::path& library,const std::filesystem::path& root,const ConversionProgress& progress,const ConversionOptions& options){
 conversionCheckpoint(options);
 auto f=detectarTimbreFormat(original);if(!convertedTimbre(original))throw std::runtime_error("Formato não requer conversão");
 auto originalSize=std::filesystem::file_size(original);auto originalTime=std::filesystem::last_write_time(original);
 auto hash=sha256File(original);auto effective=options;effective.originalSha256=hash;std::filesystem::create_directories(destination);std::filesystem::path stage,final;bool reserved=false;
 for(unsigned i=0;i<10000;++i){final=destination/(hash+"-"+std::to_string(i));stage=final;stage+=".staging";if(std::filesystem::exists(final))continue;if(std::filesystem::create_directory(stage)){reserved=true;break;}}
 if(!reserved)throw std::runtime_error("Não foi possível reservar pasta de importação");
 try{
  MetaSidecar meta;auto bank=stage/"bank";
  if(f==TimbreFormat::NPNO)npnoParaSFZ(original,bank,meta,library,progress,effective);else if(f==TimbreFormat::NKI)nkiParaSFZ(original,bank,meta,library,root,options);else nsmpParaSFZ(original,bank,meta,library,options);
  conversionCheckpoint(options);
  if(hash!=meta.originalSha256||std::filesystem::file_size(original)!=originalSize||std::filesystem::last_write_time(original)!=originalTime)throw std::runtime_error("Original mudou durante a importação");
  std::filesystem::rename(bank,final);std::error_code ignored;std::filesystem::remove(stage,ignored);return final/original.filename();
 }catch(...){std::error_code ignored;std::filesystem::remove_all(stage,ignored);throw;}
}
}

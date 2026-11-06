#include "npno_reader.hpp"
#include "instrument_catalog.hpp"
#include "sample_builder.hpp"
#include "nsmp/nsmp.hpp"
#include <fstream>
#include <iostream>
using namespace lp;
namespace {
void check(bool ok,const std::string& text){if(!ok)throw std::runtime_error(text);}
std::string textFile(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"missing file "+p.u8string());return {std::istreambuf_iterator<char>(in),{}};}
void save(const std::filesystem::path& p,const npno::Bytes& b){atomicWrite(p,std::string(b.begin(),b.end()));}
template<class F>void rejects(F fn,const std::string& fragment={}){bool failed=false;try{fn();}catch(const std::exception& e){failed=true;check(fragment.empty()||std::string(e.what()).find(fragment)!=std::string::npos,"unexpected diagnostic: "+std::string(e.what()));}check(failed,"expected clean failure");}
void put(npno::Bytes& b,std::size_t at,std::uint32_t x,unsigned n,bool little=false){for(unsigned k=0;k<n;++k)b.at(at+(little?k:n-1-k))=static_cast<std::uint8_t>(x>>(8*k));}
void seal(npno::Bytes& b){bool modern=b.at(4)==1;if(modern)put(b,24,nsmp::crc32(b,44,b.size()),4,true);else put(b,b.size()-2,nsmp::crc16(b,b.size()-2),2,true);}
}
int main(int argc,char** argv){try{
 check(argc==4,"arguments: sndfile output fixture-dir");std::filesystem::path library=argv[1],root=std::filesystem::absolute(argv[2]),fixtures=argv[3];std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 std::ifstream golden(fixtures/"golden.txt");check(bool(golden),"golden vectors");std::string marker,name;std::size_t files=0;
 sampling::Decoder decoder(library);
 while(golden>>marker){std::size_t n;golden>>name>>n;check(marker=="FILE","golden framing");auto input=fixtures/name;check(detectarTimbreFormat(input)==TimbreFormat::NPNO,"byte signature classification");auto m=npno::readModule(input);check(m.checksumValid&&m.strokes.size()==n,"directory checksum/count");std::vector<npno::Audio> audio;
  for(std::size_t i=0;i<n;++i){std::size_t index,frames;int channels;std::string rawFile;golden>>marker>>index>>frames>>channels>>rawFile;check(marker=="PCM"&&index==i,"golden record order");auto pcm=npno::decodeStroke(m,m.strokes[i]);auto raw=textFile(fixtures/rawFile);check(raw.size()==frames*channels*2&&pcm.lanes.size()==std::size_t(channels),"PCM dimensions");std::size_t at=0;
   for(const auto& lane:pcm.lanes){check(lane.size()==frames,"frame count");for(auto value:lane){unsigned bits=static_cast<unsigned char>(raw[at])|(unsigned(static_cast<unsigned char>(raw[at+1]))<<8);int expected=bits>=32768?int(bits)-65536:int(bits);check(value==expected,"PCM differential mismatch "+name+" record "+std::to_string(i));at+=2;}}
   audio.push_back(std::move(pcm));
  }
  MetaSidecar meta;auto output=root/name;std::vector<std::string> progress;ConversionOptions legacy;legacy.velocityCrossfadeExplicit=true;legacy.stereoMonoFallback=true;legacy.npnoPedalResonance=false;auto sfz=npnoParaSFZ(input,output,meta,library,[&](const std::string& s){progress.push_back(s);},legacy);
  check(progress.front()=="lendo módulo..."&&progress.back()=="concluído","status sequence");check(meta.samples.size()==n*m.channels,"all sample records/channels exported");check(sha256File(input)==sha256File(output/input.filename()),"original preserved");check(!meta.sourceMetadataJson.empty()&&meta.json().find("source_metadata")!=std::string::npos,"raw unsupported fields preserved");
  // Explicit legacy output above keeps the independent per-lane PCM oracle.
  std::size_t sample=0;for(const auto& pcm:audio)for(const auto& lane:pcm.lanes){auto file=output/meta.samples[sample++];auto decoded=decoder.decode(file);check(decoded.channels==1&&decoded.rate==35002&&decoded.frames()==lane.size(),"mono WAV rate/frames");for(std::size_t j=0;j<lane.size();++j)check(decoded.pcm[j]==float(lane[j])/32768,"WAV PCM16 exact preservation");}
  // Default output must preserve every PCM16 value, interleaving L/R once.
  MetaSidecar modern;auto newOutput=root/(name+"-stereo");npnoParaSFZ(input,newOutput,modern,library);
  check(modern.samples.size()==n,"one native WAV per stroke");
  auto modernMeta=modern.json();check(modernMeta.find("release_model=conservative")!=std::string::npos&&modernMeta.find("stereo_output=")!=std::string::npos&&modernMeta.find("velocity_crossfade=on; width=8")!=std::string::npos,"sidecar retains release/stereo/crossfade decisions");
  for(std::size_t i=0;i<n;++i){auto decoded=decoder.decode(newOutput/modern.samples[i]);check(decoded.channels==m.channels&&decoded.rate==35002&&decoded.frames()==audio[i].lanes[0].size(),"native stereo dimensions");for(std::size_t f=0;f<decoded.frames();++f)for(int c=0;c<m.channels;++c)check(decoded.pcm[f*m.channels+c]==float(audio[i].lanes[c][f])/32768,"native stereo interleaved PCM16 exact preservation");}

  auto report=output/input.filename();report+=".audit.json";auto audit=textFile(report);check(audit.find("\"divergences\":0")!=std::string::npos&&audit.find("\"gaps\":0,\"overlaps\":0")!=std::string::npos,"audit coverage/divergence");
  auto content=textFile(sfz);check(content.find("<group>")!=std::string::npos&&content.find("sample=Samples/npno-")!=std::string::npos,"common writer groups/relative samples");
  if(name!="tine-demo.npno"){
   check(m.zones.size()==4,"per-key tuning separates adjacent zones");check(meta.zones.size()==std::size_t(4*9*m.channels),"all velocity/pedal/release regions, with one pedal-only release bank");
   check(content.find("loop_start=100 loop_end=899")!=std::string::npos,"loop inclusive end");check(content.find("tune=2.1")!=std::string::npos,"per-key tuning");check(content.find("trigger=release ")!=std::string::npos&&content.find("trigger=release_key")==std::string::npos,"bank function");
   for(int group=0;group<4;++group)for(int bank=0;bank<3;++bank)for(int channel=0;channel<m.channels;++channel){std::array<int,128> seen{};for(int layer=0;layer<3;++layer){auto& z=meta.zones[(group*9+bank*3+layer)*m.channels+channel];check(z.velLow==(layer==0?100:layer==1?62:1)&&z.velHigh==(layer==0?127:layer==1?99:61),"independent measured-curve velocity thresholds");for(int v=z.velLow;v<=z.velHigh;++v)++seen[v];}for(int v=1;v<=127;++v)check(seen[v]==1,"complete exclusive velocity coverage");}
   if(m.channels==2)check(content.find("pan=-100")!=std::string::npos&&content.find("pan=100")!=std::string::npos,"stereo kept as two mono lanes");
  }else check(m.strokes.size()==3&&m.strokes[0].root==52&&m.strokes[0].frames==53876,"public demo structure");
  ++files;
 }
 check(files==3,"three independent fixtures");
 auto source=fixtures/"layers-v6.npno";auto bytes=nsmp::readFile(source);auto module=npno::readModule(source);auto bad=root/"bad.npno";
 auto mutation=[&](const std::string& label,auto change,const std::string& error={}){std::cout<<"Rejecting "<<label<<std::endl;auto b=bytes;change(b);save(bad,b);auto output=root/label;MetaSidecar m;rejects([&]{npnoParaSFZ(bad,output,m,library);},error);check(!std::filesystem::exists(output),"failed conversion rollback "+label);};
 mutation("short",[](auto& b){b.resize(10);},"truncado");mutation("truncated",[](auto& b){b.resize(b.size()-100);},"truncado");
 mutation("unknown-version",[](auto& b){put(b,20,999,4,true);},"999");mutation("unknown-stream",[&](auto& b){put(b,module.base+4,0x499,2);},"1177");
 mutation("bad-signature",[](auto& b){b[0]='X';},"assinatura");mutation("bad-loop",[&](auto& b){put(b,module.strokes[0].recordOffset+0x20,0xffffffffu,4);},"loop");
 mutation("bad-block",[&](auto& b){put(b,module.strokes[0].audioOffset,0,2);},"áudio corrompido");
 mutation("bad-count",[&](auto& b){put(b,module.base+0x622+48*2,65535,2);},"diretório");
 mutation("bad-offset",[&](auto& b){put(b,module.strokes[0].recordOffset,0,4);},"sobrepostos");
 mutation("bad-pcm",[&](auto& b){auto at=module.strokes[0].audioOffset;unsigned width=b[at+1]&31;std::size_t own=(1022*module.channels-2)*8/(width*module.channels)-64,bit=own*module.channels*width;b[at+2+(bit/16)*2+(bit%16<8?1:0)]^=static_cast<std::uint8_t>(1u<<(bit%8));});
 auto checksum=bytes;checksum[24]^=1;save(bad,checksum);MetaSidecar meta;npnoParaSFZ(bad,root/"checksum-warning",meta,library);check(!meta.checksumValid&&!meta.limitations.empty(),"checksum warns and continues");
 // Signature, never the suffix alone, decides the format. Alias retains original identity.
 for(const char* suffix:{".npno",".NPNO",".npmo",".NPMO"}){auto alias=root/(std::string("alias")+suffix);save(alias,bytes);check(detectarTimbreFormat(alias)==TimbreFormat::NPNO,"alias sniff");auto original=importarTimbreConvertido(alias,root/"imports",library);check(playableInstrument(original)==derivedSfz(original)&&std::filesystem::exists(playableInstrument(original)),"catalog derivative resolution");std::ofstream(original.parent_path()/"catalog.txt")<<"LPHS_BANK 1\n\"piano\" 14 \"Piano\" \""<<original.filename().u8string()<<"\" 60 \"poly\" \"user\"\n";check(loadInstrumentBank(original.parent_path()).size()==1,"catalog alias accepted");}
 save(root/"content.bin",bytes);check(detectarTimbreFormat(root/"content.bin")==TimbreFormat::NPNO,"signature without extension");atomicWrite(root/"false.npno","<region> sample=*sine");rejects([&]{detectarTimbreFormat(root/"false.npno");},"assinatura");
 auto bank=root/"unsafe";std::filesystem::create_directory(bank);std::ofstream(bank/"catalog.txt")<<"LPHS_BANK 1\n\"p\" 14 \"Piano\" \"../bad.npno\" 60 \"poly\" \"user\"\n";rejects([&]{loadInstrumentBank(bank);});
 auto incomplete=bytes;incomplete[module.strokes[1].recordOffset+0x74]=0;incomplete[module.strokes[2].recordOffset+0x74]=255;seal(incomplete);save(bad,incomplete);npnoParaSFZ(bad,root/"unreachable-layer",meta,library);bool warned=false;for(const auto& l:meta.limitations)if(l.find("sem velocity alcançável")!=std::string::npos)warned=true;check(warned&&meta.samples.size()==18,"unreachable layer warned and PCM retained");
 ConversionOptions crossOptions;crossOptions.velocityCrossfade=true;npnoParaSFZ(fixtures/"layers-v5.npno",root/"crossfade-check",meta,library,{},crossOptions);check(textFile(root/"crossfade-check/layers-v5.npno.converted.sfz").find("xfin_lovel=")!=std::string::npos,"NPNO crossfade option reaches the common writer");
 std::cout<<"PASS: public demo and independent v5/v6 multilayer PCM goldens; mono/stereo PCM16 WAV; loops/tuning/pedal/release; 1..127 velocity coverage; audit, aliases, preserved original/catalog, malformed-file rollback\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

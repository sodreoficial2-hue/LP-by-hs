#include "timbre_format.hpp"
#include "sfz_conversion.hpp"
#include "nki_reader.hpp"
#include <cmath>
#include <map>
#include <charconv>
#include <set>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace lp {
namespace {
std::vector<std::filesystem::path> scanWav(const std::filesystem::path& root){
 if(!std::filesystem::is_directory(root))return {};
 std::vector<std::filesystem::path> result;std::size_t entries=0;std::uintmax_t bytes=0;
 for(auto it=std::filesystem::recursive_directory_iterator(root);it!=std::filesystem::recursive_directory_iterator();++it){
  if(++entries>20000)throw std::runtime_error("Pasta muito grande; escolha uma pasta Samples específica");
  if(it->is_symlink()){if(it->is_directory())it.disable_recursion_pending();continue;}
  if(it.depth()>8)throw std::runtime_error("Pasta de amostras muito profunda; escolha uma subpasta");
  if(it->is_regular_file()&&timbreExtension(it->path())==".wav"){
   bytes+=it->file_size();if(result.size()>=256||bytes>256u*1024u*1024u)throw std::runtime_error("Limite da Etapa A: 256 WAVs e 256 MiB. Escolha uma subpasta menor.");result.push_back(it->path());
  }
 }
 std::sort(result.begin(),result.end());return result;
}
std::filesystem::path samplesFolder(const std::filesystem::path& input){
 // Only explicit companion directories: never silently combine another library's tree.
 auto parent=input.parent_path();for(int level=0;level<2;++level){
  std::vector<std::filesystem::path> found;
  if(std::filesystem::is_directory(parent))for(const auto& e:std::filesystem::directory_iterator(parent)){
   if(e.is_symlink()||!e.is_directory())continue;auto name=e.path().filename().u8string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(name=="samples")found.push_back(e.path());
  }
  if(found.size()>1)throw MissingNkiSamples("Mais de uma pasta Samples; selecione a pasta específica para este instrumento");
  if(found.size()==1)return found.front();
  parent=parent.parent_path();
 }
 // Flat packages: WAV companions directly beside the NKI (no recursive fallback).
 if(std::filesystem::is_directory(input.parent_path()))for(const auto& e:std::filesystem::directory_iterator(input.parent_path()))if(!e.is_symlink()&&e.is_regular_file()&&timbreExtension(e.path())==".wav")return input.parent_path();
 throw MissingNkiSamples("NKI sem amostras externas localizadas; decodificação integral pendente. Selecione Samples ou a raiz desta biblioteca KONTAKT");
}
}
static std::filesystem::path nkiFallback(const std::filesystem::path& input,const std::filesystem::path& output,MetaSidecar& out,const std::filesystem::path& library,const std::filesystem::path& sampleRoot){
 if(detectarTimbreFormat(input)!=TimbreFormat::NKI)throw std::runtime_error("Arquivo não reconhecido como candidato NKI");out=MetaSidecar{};out.name=input.stem().u8string();out.format="NKI";out.version="unknown";out.originalSha256=sha256File(input);out.status="approximate_companion_wav_sfz_map";
 std::ifstream in(input,std::ios::binary);std::uint8_t signature[16]{};in.read(reinterpret_cast<char*>(signature),16);out.signatureHex=bytesHex(signature,static_cast<std::size_t>(in.gcount()));
 out.limitations={
  "Aviso: fallback por nome ativo porque nenhum payload K2_Container reconhecido foi localizado; não representa o mapa binário original.",
  "TODO NKI binary signature: extension plus first bytes captured as diagnostic only; no real Kontakt fixture, no binary codec offsets invented. This does not block companion-WAV conversion.",
  "Etapa A: every WAV in the selected subtree is used. Real NKI sample references are not parsed; choose the exact instrument subfolder when Samples is shared.",
  "Filename C4=MIDI60, sharps/flats and midi60/note60/key60 identify roots. Known roots split at neighboring-root midpoints; explicit vNN velocities split within the root group. Files without velocity metadata use the full velocity range and may overlap.",
  "Unknown roots use one full-range region per sample with pitch_keytrack=0 (unpitched). They may overlap other samples; no pitch or dynamic-layer inference is fabricated.",
  "TODO Etapa B: actual Kontakt key/velocity ranges, loops, tuning, groups, scripts, round robins, keyswitches, articulation, envelopes, filters, effects and modulators. Unknown values remain in the preserved NKI, not invented JSON fields.",
  "WAV originals are copied byte-for-byte under stable ASCII relative names after real libsndfile decoding validation. No pitch detection, trimming, resampling, normalization, 16-bit conversion or inferred loops. SFZ uses no_loop and existing sfizz envelope defaults.",
  "Implementation limits: 256 WAVs, 256 MiB source WAV files, 20k visited entries/depth 8, mono/stereo 8..192 kHz, >=32 frames and <=120 seconds per sample, 64 million decoded values total. NKI without WAV companions gives an explicit pending-monolith diagnostic."
 };
 auto root=sampleRoot.empty()?samplesFolder(input):sampleRoot;auto paths=scanWav(root);if(paths.empty())throw MissingNkiSamples("NKI sem amostras externas localizadas; decodificação integral pendente. Selecione a pasta Samples.");
 struct SampleInfo {sampling::Sample sample;std::filesystem::path path;std::string hash;std::size_t frames=0;};std::vector<SampleInfo> info;std::set<int> roots;std::map<int,std::set<int>> velocities;sampling::Decoder decoder(library);std::size_t total=0;
 for(const auto& path:paths){auto hash=sha256File(path);auto s=decoder.decode(path);if(hash!=sha256File(path))throw std::runtime_error("WAV mudou durante leitura");sampling::metadata(s);total+=s.pcm.size();if(total>64000000)throw std::runtime_error("Limite total de PCM excedido");if(s.root>=0){roots.insert(s.root);if(s.explicitVelocity)velocities[s.root].insert(s.velocity);} // Retain metadata, not all PCM, while copying the original WAV.
  auto frames=s.frames();s.pcm.clear();s.pcm.shrink_to_fit();info.push_back({std::move(s),path,hash,frames});
 }
 ConversionOutput transaction(output);std::filesystem::create_directory(output/"Samples");auto sfz=sfzStream();
 for(std::size_t i=0;i<info.size();++i){auto& s=info[i].sample;auto relative="Samples/sample-"+std::to_string(i)+".wav";auto dest=output/std::filesystem::u8path(relative);std::filesystem::copy_file(info[i].path,dest);if(sha256File(dest)!=info[i].hash)throw std::runtime_error("WAV mudou durante cópia");out.samples.push_back(std::filesystem::u8path(relative));out.sampleHashes.push_back(info[i].hash);out.preservedFeatures.push_back("WAV origin="+info[i].path.u8string()+" copied="+relative+" sha256="+info[i].hash);
  ConvertedZone z;z.sample=relative;z.rate=s.rate;z.channels=s.channels;z.frames=info[i].frames;
  if(s.root<0){z.root=60;z.rootSource="unknown_unpitched_full_range";z.zoneMode=1;}
  else {z.root=s.root;z.rootSource="filename_C4_equals_60";auto r=roots.find(s.root);z.keyLow=r==roots.begin()?0:(*std::prev(r)+s.root)/2+1;z.keyHigh=std::next(r)==roots.end()?127:(s.root+*std::next(r))/2;
   if(s.explicitVelocity){auto& values=velocities[s.root];auto v=values.find(s.velocity);z.velLow=v==values.begin()?1:(*std::prev(v)+s.velocity)/2+1;z.velHigh=std::next(v)==values.end()?127:(s.velocity+*std::next(v))/2;}
  }out.zones.push_back(z);writeSfzRegion(sfz,z);
 }
 return transaction.finish(input,out,sfz.str());
}
namespace {
double number(const std::map<std::string,std::string>& v,const std::string& key,double fallback){
 auto it=v.find(key);if(it==v.end())return fallback;double n=0;
 auto result=std::from_chars(it->second.data(),it->second.data()+it->second.size(),n);
 if(result.ec!=std::errc()||result.ptr!=it->second.data()+it->second.size())throw std::runtime_error("NKI: campo numérico inválido "+key+"="+it->second);
 return n;
}
int integer(const std::map<std::string,std::string>& v,const std::string& key,int fallback,int maximum=127){
 auto n=number(v,key,fallback);if(!std::isfinite(n)||n<0||n>maximum||n!=std::floor(n))throw std::runtime_error("NKI: "+key+" fora de 0.."+std::to_string(maximum));return static_cast<int>(n);
}
std::string safeName(std::string name){for(auto& c:name)if(c=='/'||c=='\\'||c==':'||static_cast<unsigned char>(c)<32||c=='<'||c=='>'||c=='"'||c=='|'||c=='?'||c=='*')c='_';if(name.empty()||name=="."||name=="..")name="program";if(name.size()>100)name="program";return name;}
std::filesystem::path externalPath(const std::filesystem::path& input,std::string sample){
 std::replace(sample.begin(),sample.end(),'\\','/');auto p=std::filesystem::u8path(sample);
 if(p.is_absolute()||sample.find(':')!=std::string::npos||sample.find('\0')!=std::string::npos)throw std::runtime_error("NKI: sampleFile deve ser relativo à pasta do instrumento: "+sample);
 return (input.parent_path()/p).lexically_normal();
}
std::vector<std::pair<std::size_t,std::size_t>> embeddedAudio(const nki::Document& d){
 std::vector<std::pair<std::size_t,std::size_t>> found;
 for(auto at=d.payloadEnd;at+12<=d.bytes.size();++at){
  bool riff=std::equal(d.bytes.begin()+at,d.bytes.begin()+at+4,"RIFF")&&std::equal(d.bytes.begin()+at+8,d.bytes.begin()+at+12,"WAVE");
  bool aiff=std::equal(d.bytes.begin()+at,d.bytes.begin()+at+4,"FORM")&&(std::equal(d.bytes.begin()+at+8,d.bytes.begin()+at+12,"AIFF")||std::equal(d.bytes.begin()+at+8,d.bytes.begin()+at+12,"AIFC"));
  if(!riff&&!aiff)continue;std::uint64_t n=0;for(int k=0;k<4;++k)n|=std::uint64_t(d.bytes[at+4+k])<<(8*(riff?k:3-k));n+=8;
  if(n<12||n>d.bytes.size()-at)throw std::runtime_error("NKI monolítico: amostra RIFF/FORM truncada");
  found.push_back({at,n});if(found.size()>256)throw std::runtime_error("NKI monolítico: muitas amostras embutidas");at+=n-1;
 }
 return found;
}
}
std::filesystem::path nkiParaSFZ(const std::filesystem::path& input,const std::filesystem::path& output,MetaSidecar& out,const std::filesystem::path& library,const std::filesystem::path& sampleRoot,const ConversionOptions& options){
 if(detectarTimbreFormat(input)!=TimbreFormat::NKI)throw std::runtime_error("NKI: arquivo de outro formato");
 auto doc=nki::read(input);if(!doc.recognized)return nkiFallback(input,output,out,library,sampleRoot);
 out=MetaSidecar{};out.name=input.stem().u8string();out.format="NKI";out.version=doc.root.attr("version","XML");out.status="binary_k2_xml_sfz";out.signatureConfirmed=true;out.signatureHex="zlib_or_gzip/K2_Container";out.originalSha256=sha256File(input);
 out.preservedFeatures.push_back("Full source XML (includes unmapped fields): "+doc.xml);
 out.preservedFeatures.push_back("compressed_payload_offset="+std::to_string(doc.payloadOffset)+"; compressed_payload_end="+std::to_string(doc.payloadEnd));
 out.limitations={"NKI K2_Container XML layout only; unsupported encrypted/NCW/NKS/NIS monoliths are not claimed.","zoneTune is interpreted as a frequency ratio (1=unity); zonePan uses -100..100. Other units require a corpus reference; raw XML is preserved.","sampleEnd/loopEnd are interpreted as inclusive frame indices; converted internal end is exclusive and SFZ subtracts one. No frame rounding.","loopMode 0=no_loop, 1=loop_sustain (CC64 respected), 2=loop_continuous through the release envelope; until_end is not silently changed to one_shot.","fadeLowKey/fadeHighKey are preserved; overlapping roots are partitioned with a warning instead of enabling extra voices.","Embedded plain RIFF/FORM after XML is supported only for one unambiguous sample reference. Other monolith layouts fail explicitly."};
 auto programs=doc.root.child("Programs");if(!programs)throw std::runtime_error("NKI XML: Programs ausente");
 std::set<std::string> refs;
 for(const auto& program:programs->children)if(program.tag=="K2_Program")if(auto groups=program.child("Groups"))for(const auto& group:groups->children)if(group.tag=="K2_Group")if(auto zones=group.child("Zones"))for(const auto& zone:zones->children)if(zone.tag=="Zone"){auto v=nki::values(zone);if(v.count("sampleFile")&&!v["sampleFile"].empty())refs.insert(v["sampleFile"]);}
 auto embedded=embeddedAudio(doc);ConversionOutput transaction(output);std::filesystem::create_directory(output/"Samples");
 sampling::Decoder decoder(library);WavWriter wav(library);std::map<std::string,ConvertedZone> samples;std::size_t total=0;int programIndex=0;std::vector<std::string> sfzNames;
 for(const auto& program:programs->children){if(program.tag!="K2_Program")continue;if(++programIndex>128)throw std::runtime_error("NKI: limite de 128 programas");
  auto programName="program-"+std::to_string(programIndex)+"-"+safeName(program.attr("name","program"))+".sfz";sfzNames.push_back(programName);out.preservedFeatures.push_back("program="+program.attr("name")+"; sfz="+programName);
  auto groups=program.child("Groups");if(!groups){out.limitations.push_back("Programa sem Groups: "+program.attr("name"));continue;}
  for(const auto& group:groups->children){if(group.tag!="K2_Group")continue;auto groupValues=nki::values(group);auto zones=group.child("Zones");if(!zones)continue;
   for(const auto& zone:zones->children){if(zone.tag!="Zone")continue;if(out.zones.size()>8192)throw std::runtime_error("NKI: limite de 8192 zonas");auto values=nki::values(zone);auto reference=values["sampleFile"];
    if(reference.empty()){out.limitations.push_back("Zona sem sampleFile descartada no programa "+std::to_string(programIndex));continue;}
    ConvertedZone z;
    try{z.root=integer(values,"rootKey",60);z.keyLow=integer(values,"lowKey",0);z.keyHigh=integer(values,"highKey",127);z.velLow=std::max(1,integer(values,"lowVelo",0));z.velHigh=integer(values,"highVelo",127);integer(values,"fadeLowKey",0);integer(values,"fadeHighKey",0);
     if(z.keyLow>z.keyHigh||z.velLow>z.velHigh)throw std::runtime_error("NKI: faixa invertida");
    }catch(const std::exception& e){out.limitations.push_back(std::string("Zona descartada: ")+e.what());continue;}
    auto gain=number(values,"zoneVolume",1.0);z.volumeDb=linearGainDb(gain);if(!std::isfinite(gain)||gain<=0){out.limitations.push_back("Zona omitida: zoneVolume nulo/negativo/NaN; piso -144 dB; sample="+reference);continue;}
    auto found=samples.find(reference);if(found==samples.end()){
     if(samples.size()>=256)throw std::runtime_error("NKI: limite de 256 amostras");auto path=externalPath(input,reference);std::filesystem::path temporary;
     if(!std::filesystem::is_regular_file(path)){
      if(refs.size()==1&&embedded.size()==1){auto [at,n]=embedded.front();temporary=output/"embedded-source.wav";atomicWrite(temporary,std::string(reinterpret_cast<const char*>(doc.bytes.data()+at),n));path=temporary;out.preservedFeatures.push_back("Embedded plain RIFF/FORM sample extracted; offset="+std::to_string(at)+" bytes="+std::to_string(n));}
      else throw MissingNkiSamples("NKI: sample externo faltante: "+reference+"; monólito não reconhecido ou ambíguo");
     }
     auto sourceHash=sha256File(path);auto sample=decoder.decode(path);total+=sample.pcm.size();if(total>64000000)throw std::runtime_error("NKI: limite total de PCM excedido");
     ConvertedZone audio;audio.sample="Samples/sample-"+std::to_string(samples.size())+".wav";audio.rate=sample.rate;audio.channels=sample.channels;audio.frames=sample.frames();wav.write(output/std::filesystem::u8path(audio.sample),audio.rate,audio.channels,sample.pcm);
     if(sourceHash!=sha256File(path))throw std::runtime_error("NKI: sample mudou durante leitura: "+reference);
     out.samples.push_back(std::filesystem::u8path(audio.sample));out.sampleHashes.push_back(sha256File(output/std::filesystem::u8path(audio.sample)));out.preservedFeatures.push_back("source_sample="+reference+"; sha256="+sourceHash+"; sample_rate="+std::to_string(audio.rate)+"; resample=false");
     if(!temporary.empty())std::filesystem::remove(temporary);found=samples.emplace(reference,audio).first;
    }
    z.sample=found->second.sample;z.rate=found->second.rate;z.channels=found->second.channels;z.frames=found->second.frames;z.program=programIndex;z.rootSource="K2_Container.Zone.rootKey";
    z.sampleStart=integer(values,"sampleStart",0,static_cast<int>(z.frames-1));z.sampleEnd=values.count("sampleEnd")?integer(values,"sampleEnd",0,static_cast<int>(z.frames-1))+1:z.frames;
    z.sourceLoopMode=integer(values,"loopMode",0,2);z.loop=z.sourceLoopMode>0;
    if(z.loop){z.loopStart=integer(values,"loopStart",0,static_cast<int>(z.frames-1));z.loopEnd=integer(values,"loopEnd",static_cast<int>(z.frames-1),static_cast<int>(z.frames-1))+1;}
    if(values.count("zonePan")){z.pan=number(values,"zonePan",0);if(!std::isfinite(z.pan)||z.pan< -100||z.pan>100)throw std::runtime_error("NKI: zonePan fora de -100..100");z.hasPan=true;}
    auto tune=number(values,"zoneTune",1);if(!std::isfinite(tune)||tune<=0)throw std::runtime_error("NKI: zoneTune deve ser razão positiva");z.tuneCents=1200*std::log2(tune);
    auto seq=groupValues;for(const auto& v:values)seq[v.first]=v.second;z.sequenceLength=integer(seq,"seqLength",0,100);z.sequencePosition=integer(seq,"seqPosition",z.sequenceLength?1:0,100);
    if(seq.count("roundRobinLength"))z.sequenceLength=integer(seq,"roundRobinLength",0,100);if(seq.count("roundRobinPosition"))z.sequencePosition=integer(seq,"roundRobinPosition",1,100);
    z.releaseTrigger=groupValues["releaseTrigger"]=="yes";z.releaseSeconds=0.2;out.zones.push_back(z);
   }
  }
 }
 if(out.zones.empty())throw std::runtime_error("NKI XML: nenhuma zona válida com áudio");
 prepareSfzZones(out.zones,out,options);
 std::string first;
 for(std::size_t i=0;i<sfzNames.size();++i){std::vector<ConvertedZone> selected;for(const auto& z:out.zones)if(z.program==int(i+1))selected.push_back(z);if(selected.empty())continue;auto sfz=writeSfzInstrument(selected);atomicWrite(output/std::filesystem::u8path(sfzNames[i]),sfz);if(first.empty())first=sfz;}
 return transaction.finish(input,out,first);
}
}

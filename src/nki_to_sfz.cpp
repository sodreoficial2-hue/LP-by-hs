#include "timbre_format.hpp"
#include "sfz_conversion.hpp"
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
std::filesystem::path nkiParaSFZ(const std::filesystem::path& input,const std::filesystem::path& output,MetaSidecar& out,const std::filesystem::path& library,const std::filesystem::path& sampleRoot){
 if(detectarTimbreFormat(input)!=TimbreFormat::NKI)throw std::runtime_error("Arquivo não reconhecido como candidato NKI");out=MetaSidecar{};out.name=input.stem().u8string();out.format="NKI";out.version="unknown";out.originalSha256=sha256File(input);out.status="approximate_companion_wav_sfz_map";
 std::ifstream in(input,std::ios::binary);std::uint8_t signature[16]{};in.read(reinterpret_cast<char*>(signature),16);out.signatureHex=bytesHex(signature,static_cast<std::size_t>(in.gcount()));
 out.limitations={
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
}

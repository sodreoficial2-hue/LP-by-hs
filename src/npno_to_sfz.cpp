// SPDX-License-Identifier: AGPL-3.0-or-later
// Offline adapter only; all SFZ regions use the common writer.
#include "npno_reader.hpp"
#include "sfz_conversion.hpp"
#include <map>
namespace lp {
std::filesystem::path npnoParaSFZ(const std::filesystem::path& input,const std::filesystem::path& output,MetaSidecar& out,const std::filesystem::path& library,const ConversionProgress& progress){
 auto status=[&](const std::string& text){if(progress)progress(text);};status("lendo módulo...");
 out=MetaSidecar{};out.originalSha256=sha256File(input);auto module=npno::readModule(input);
 out.name=module.name;out.format="npno";out.version=std::to_string(module.header.version);out.codec=module.streamVersion;
 out.signatureConfirmed=true;out.signatureHex="4342494e/6e706e6f/434e5350";out.checksumValid=module.checksumValid;out.status="converted_npno_sfz_wav";
 out.sourceMetadataJson=module.metadataJson();out.limitations=module.warnings;
 for(const auto& warning:module.warnings)status("Aviso: "+warning);
 out.limitations.insert(out.limitations.end(),{
  "Velocity uses a measured reference Touch 0 curve, interpolated with up to two MIDI-velocity steps of uncertainty; other keyboard/touch profiles can differ. Every reachable layer is kept separate; coverage is audited per root/bank.",
  "Playback uses 35002 Hz and the stored root, following the hardware-tested reference. The alternate reference uses 44100 Hz and root+4. No rate field or resampling is invented.",
  "Per-key fine tune is 0.7 cents per unit (reference measurement 0.6..0.8). No global-detune field was identified; no guessed global tuning is applied.",
  "Bank 1 replaces the attack while CC64>=64; bank 2 uses trigger=release_key. Pedal resonance and release envelopes on the source DSP are not reproduced exactly.",
  "Note-off fades and damping use conservative SFZ releases of 0.2 s, or 2 s for undamped upper keys, rather than an unverified conversion of raw fade bytes. Release-trigger regions use 0.2 s. Source fade/release-level tables are retained raw.",
  "Loop bounds are mapped exactly (exclusive source end becomes inclusive SFZ end). Loop-only decay and the 14-entry decay ladder are retained raw without inventing a global envelope.",
  "No confirmed pan, crossfade, round-robin or global-detune fields exist in the consulted schema. Take identifiers are retained, not treated as round robin. Entire prefix and each 118-byte record remain in source_metadata.",
  "WAV is PCM16 mono without normalization. Stereo records become two mono files, hard-panned L/R; no downmix or discarded channel. Runtime voice usage doubles for those records. Region gain follows trim/global gain; v6 combined boost is capped at 10.8 dB.",
  "SFZ default velocity amplitude tracking remains in use; source DSP loudness curves, decay, pedal interactions and effect processing are not an exact emulation. Compare listening before live use.",
  "Known outer revisions 500/520/530/540 and 600/610/620/630 are routed by validated internal 0x450/0x464 layouts. Corpus-wide compatibility is not established. Limits: 512 MiB input, 16 million frames per record, 256 million total PCM values."
 });
 ConversionOutput transaction(output);std::filesystem::create_directory(output/"Samples");WavWriter wav(library);
 auto sfz=sfzStream();std::string stem;
 for(unsigned char c:input.stem().u8string()){if(stem.size()==32)break;if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')stem+=char(c);}
 if(stem.empty())stem="module";auto prefix="npno-"+stem+"-"+out.originalSha256.substr(0,12);
 std::vector<std::vector<std::string>> names(module.strokes.size());std::uint64_t declared=0,extracted=0,compressed=0,overlapChecked=0;
 std::size_t emittedRegions=0,expectedRegions=0,groups=0,recordsExtracted=0;
 // Expected totals come from the parsed directory/map, before any output is emitted.
 for(const auto& s:module.strokes){declared+=std::uint64_t(s.frames)*module.channels*2;compressed+=std::uint64_t(s.blocks)*1022*module.channels;}
 for(const auto& z:module.zones)for(const auto& s:module.strokes)if(z.root==s.root&&s.velHigh)expectedRegions+=module.channels;
 if(expectedRegions>65536)throw std::runtime_error("Módulo de piano excede limite de 65536 regiões SFZ");
 for(const auto& stroke:module.strokes){
  status("convertendo amostra "+std::to_string(stroke.index+1)+" de "+std::to_string(module.strokes.size())+"...");
  auto pcm=npno::decodeStroke(module,stroke);++recordsExtracted;overlapChecked+=pcm.overlapChecked;
  for(int c=0;c<module.channels;++c){
   auto relative="Samples/"+prefix+"-"+std::to_string(stroke.index)+(module.channels==1?"":c==0?"-L":"-R")+".wav";
   auto file=output/std::filesystem::u8path(relative);wav.writePcm16(file,npno::sampleRate,pcm.lanes[c]);
   names[stroke.index].push_back(relative);out.samples.push_back(std::filesystem::u8path(relative));out.sampleHashes.push_back(sha256File(file));extracted+=pcm.lanes[c].size()*2;
  }
 }
 auto audit=sfzStream();audit.str("");audit<<"{\n\"schema\":1,\n\"velocity_profile\":\"reference_touch_0_approximate\",\n\"velocity_coverage\":[";
 bool firstCoverage=true;
 for(const auto& keyZone:module.zones){
  writeSfzGroup(sfz);++groups;std::array<bool,3> banks{};for(const auto& s:module.strokes)if(s.root==keyZone.root)banks[s.bank]=true;
  for(int bank=0;bank<3;++bank){if(!banks[bank])continue;std::array<int,128> coverage{};
   for(const auto& s:module.strokes)if(s.root==keyZone.root&&s.bank==bank&&s.velHigh){for(int v=s.velLow;v<=s.velHigh;++v)++coverage[v];}
   for(int v=1;v<=127;++v)if(coverage[v]!=1)throw std::runtime_error("Faixa de velocity incompleta ou sobreposta: zona "+std::to_string(groups)+", banco "+std::to_string(bank)+", velocity "+std::to_string(v));
   if(!firstCoverage)audit<<',';firstCoverage=false;audit<<"{\"group\":"<<groups<<",\"root\":"<<keyZone.root<<",\"bank\":"<<bank<<",\"low\":1,\"high\":127,\"gaps\":0,\"overlaps\":0}";
  }
  std::size_t layer=0;for(const auto& stroke:module.strokes){if(stroke.root!=keyZone.root||!stroke.velHigh)continue;++layer;
   status("convertendo zona "+std::to_string(groups)+" de "+std::to_string(module.zones.size())+"... camada de velocidade "+std::to_string(layer)+"...");
   for(int c=0;c<module.channels;++c){
    ConvertedZone z;z.sample=names[stroke.index][c];z.rootSource="npno_keymap";z.root=stroke.root;z.keyLow=keyZone.low;z.keyHigh=keyZone.high;z.velLow=stroke.velLow;z.velHigh=stroke.velHigh;z.frames=stroke.frames;z.rate=npno::sampleRate;z.channels=1;z.tuneCents=keyZone.fineTune*0.7;
    z.loop=stroke.loopLength!=0;z.loopStart=stroke.loopStart;z.loopEnd=stroke.loopStart+stroke.loopLength;
    auto gain=module.gain*0.1-stroke.trim*(module.streamVersion==0x450?1.0:0.1);
    z.volumeDb=module.streamVersion==0x464?std::min(10.8,gain):gain;
    if(module.streamVersion==0x450&&stroke.trim<0){z.volumeDb=-144;out.limitations.push_back("Registro "+std::to_string(stroke.index)+": trim negativo de v5 representa silêncio; SFZ usa -144 dB.");}
    z.releaseSeconds=keyZone.undamped?2.0:0.2;z.releaseTrigger=stroke.bank==2;
    if(z.releaseTrigger)z.releaseSeconds=0.2;
    if(module.channels==2){z.hasPan=true;z.pan=c==0?-100:100;}
    if(banks[1]){if(stroke.bank==0)z.cc64High=63;else if(stroke.bank==1)z.cc64Low=64;}
    writeSfzRegion(sfz,z);out.zones.push_back(z);++emittedRegions;
   }
  }
 }
 auto expectedWavs=module.strokes.size()*module.channels;
 if(groups!=module.zones.size()||recordsExtracted!=module.strokes.size()||out.samples.size()!=expectedWavs||declared!=extracted||expectedRegions!=emittedRegions)throw std::runtime_error("Módulo de piano: auditoria de extração divergente; importação cancelada");
 audit<<"],\n\"zones_expected\":"<<module.zones.size()<<",\"zones_generated\":"<<groups<<",\n\"sample_records_expected\":"<<module.strokes.size()<<",\"sample_records_extracted\":"<<recordsExtracted<<",\n\"wav_files_expected\":"<<expectedWavs<<",\"wav_files_generated\":"<<out.samples.size()<<",\n\"regions_expected\":"<<expectedRegions<<",\"regions_generated\":"<<emittedRegions<<",\n\"pcm_bytes_declared\":"<<declared<<",\"pcm_bytes_extracted\":"<<extracted<<",\n\"compressed_audio_bytes\":"<<compressed<<",\"overlap_values_checked\":"<<overlapChecked<<",\n\"divergences\":0,\"audio_fidelity_verified\":false\n}\n";
 auto report=output/input.filename();report+=".audit.json";atomicWrite(report,audit.str());
 out.preservedFeatures={"Original binary with SHA-256; full body prefix and every sample record in source_metadata; per-channel WAV hashes; separate extraction and velocity audit.","All source sample records are decoded, including unmapped/unreachable layers; only reachable regions are played."};
 auto result=transaction.finish(input,out,sfz.str());status("concluído");return result;
}
}

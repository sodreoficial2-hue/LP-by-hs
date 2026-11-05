// SPDX-License-Identifier: AGPL-3.0-or-later
// Offline adapter only; all SFZ regions use the common writer.
#include "npno_reader.hpp"
#include "sfz_conversion.hpp"
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include "pcm16_wave.hpp"
namespace lp {
namespace {
// Explicit project defaults, NOT measured Nord DSP coefficients. Offline T3
// calibration may replace these offsets after measuring the source banks.
constexpr double releaseBankInitialGainDb=-18.0;
constexpr double monoFallbackPanGainDb=-3.01029995664; // equal-power stereo center vs hard-pan mono.
}
std::filesystem::path npnoParaSFZ(const std::filesystem::path& input,const std::filesystem::path& output,MetaSidecar& out,const std::filesystem::path& library,const ConversionProgress& progress,const ConversionOptions& options){
 auto status=[&](const std::string& text){conversionCheckpoint(options);if(progress)progress(text);};status("lendo módulo...");
 out=MetaSidecar{};out.originalSha256=options.originalSha256.empty()?sha256File(input):options.originalSha256;auto module=npno::readModule(input);
 out.name=module.name;out.format="npno";out.version=std::to_string(module.header.version);out.codec=module.streamVersion;
 out.signatureConfirmed=true;out.signatureHex="4342494e/6e706e6f/434e5350";out.checksumValid=module.checksumValid;out.status="converted_npno_sfz_wav";
 out.sourceMetadataJson=module.metadataJson();out.limitations=module.warnings;
 for(const auto& warning:module.warnings)status("Aviso: "+warning);
 out.limitations.insert(out.limitations.end(),{
  "Velocity usa Touch 0 empírico legado; estimativa histórica de dois passos MIDI não confirmada independentemente. nord-format 0.8.0 e OpenNord divergem; curva e regras efetivas registradas no source_metadata. Cobertura auditada por root/banco antes do crossfade.",
  "Playback uses 35002 Hz and the stored root, following the hardware-tested reference. The alternate reference uses 44100 Hz and root+4. No rate field or resampling is invented.",
  "Per-key fine tune is 0.7 cents per unit (reference measurement 0.6..0.8). No global-detune field was identified; no guessed global tuning is applied.",
  "Banco 1 igualado por RMS ao banco 0 quando sfizz está disponível; ressonância extra de ataque é aproximação, sem simulação física de cordas. Banco 2 usa release_key.",
  "Note-off fades and damping use conservative SFZ releases of 0.2 s, or 2 s for undamped upper keys, rather than an unverified conversion of raw fade bytes. Release-trigger regions use 0.2 s. Source fade/release-level tables are retained raw.",
  "Loop bounds are mapped exactly (exclusive source end becomes inclusive SFZ end). loopDecay usa aproximação de projeto registrada; tabela de 14 entradas permanece crua, sem interpretação inventada.",
  "No confirmed pan, crossfade, round-robin or global-detune fields exist in the consulted schema. Take identifiers are retained, not treated as round robin. Entire prefix and each 118-byte record remain in source_metadata.",
  "WAV PCM16 mono/stereo sem normalização; estéreo intercalado por padrão, par mono hard-pan por opção. Ganho final limitado a [-144,+10.8] dB.",
  "Curva local de amplitude progressiva até o topo da camada suave evita pianissimo plano; acima desse topo o ganho fica unitário, sem tracking SFZ global duplicado. A curva dinâmica contínua do DSP Nord não é conhecida: equalização de pedal usa RMS; regressão isotônica por camada com PCM/trim é opcional; não comprova equivalência auditiva.",
  "Banco 2 mantém -18 dB inicial; ressonância não reage retroativamente a notas antigas.",
  "damperTop confirma apenas o limite de amortecimento, não ausência de banco 2 acima dele: regiões release_key mantidas; sem confirmação independente para silenciá-las.",
  "Known outer revisions 500/520/530/540 and 600/610/620/630 are routed by validated internal 0x450/0x464 layouts. Corpus-wide compatibility is not established. Limits: 512 MiB input, 16 million frames per record, 256 million total PCM values."
 });
 ConversionOutput transaction(output);std::filesystem::create_directory(output/"Samples");
 auto effective=options;if(!effective.velocityCrossfadeExplicit)effective.velocityCrossfade=true;
 int lanes=module.channels==2&&!options.stereoMonoFallback?1:module.channels;
 out.preservedFeatures.push_back(lanes==1&&module.channels==2?"stereo_output=interleaved_pcm16; voices_per_stroke=1":"stereo_output=mono_lanes; voices_per_stroke="+std::to_string(lanes));
 if(module.channels==2)out.preservedFeatures.push_back("stereo_pan_law=sfizz_equal_power; mono_fallback_region_gain_db=-3.01029995664; WAV_PCM_unchanged=true");
 out.preservedFeatures.push_back("release_model=conservative; damped_seconds=0.2; undamped_seconds=2.0; release_key_seconds=0.2; raw_fades_preserved=true");
 out.preservedFeatures.push_back("pedal_equalization=offline_RMS; release_bank_initial_gain_db=-18; resonance_model=bounded_extra_attack");
 auto sfz=sfzStream();std::string stem;
 for(unsigned char c:input.stem().u8string()){if(stem.size()==32)break;if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')stem+=char(c);}
 if(stem.empty())stem="module";auto prefix="npno-"+stem+"-"+out.originalSha256.substr(0,12);
 std::vector<double> peaks(module.strokes.size());std::vector<std::vector<std::string>> names(module.strokes.size());std::uint64_t declared=0,extracted=0,compressed=0,overlapChecked=0;
 std::size_t emittedRegions=0,expectedRegions=0,groups=0,recordsExtracted=0;
 // Expected totals come from the parsed directory/map, before any output is emitted.
 for(const auto& s:module.strokes){declared+=std::uint64_t(s.frames)*module.channels*2;compressed+=std::uint64_t(s.blocks)*1022*module.channels;}
 for(const auto& z:module.zones){for(const auto& band:module.velocityBands)if(band.root==z.root)expectedRegions+=lanes*(band.pedalStroke>=0?2:1);for(const auto& s:module.strokes)if(z.root==s.root&&s.bank==2&&s.velHigh)expectedRegions+=lanes;}
 if(expectedRegions>65536)throw std::runtime_error("Módulo de piano excede limite de 65536 regiões SFZ");
 struct StrokeResult {std::vector<std::string> hashes;std::uint64_t bytes=0,overlap=0;};
 std::vector<StrokeResult> strokeResults(module.strokes.size());std::mutex progressMutex,errorMutex;std::exception_ptr failure;std::atomic<std::size_t> nextStroke{0};std::atomic<bool> failed{false};
 auto convertStroke=[&](const npno::Stroke& stroke,WavWriter& wav){
  {std::lock_guard<std::mutex> lock(progressMutex);status("convertendo amostra "+std::to_string(stroke.index+1)+" de "+std::to_string(module.strokes.size())+"...");}
  auto pcm=npno::decodeStroke(module,stroke);for(const auto& lane:pcm.lanes)for(auto value:lane)peaks[stroke.index]=std::max(peaks[stroke.index],std::abs(double(value))/32768.0);strokeResults[stroke.index].overlap=pcm.overlapChecked;
  for(int c=0;c<lanes;++c){
   bool stereo=module.channels==2&&lanes==1;
   auto relative="Samples/"+prefix+"-"+std::to_string(stroke.index)+(module.channels==1||stereo?"":c==0?"-L":"-R")+".wav";
   std::vector<std::int16_t> interleaved;
   if(stereo){interleaved.reserve(stroke.frames*2);for(std::size_t f=0;f<stroke.frames;++f){interleaved.push_back(pcm.lanes[0][f]);interleaved.push_back(pcm.lanes[1][f]);}}
   const auto& values=stereo?interleaved:pcm.lanes[c];
   auto file=output/std::filesystem::u8path(relative);std::string hash;wav.writePcm16Hashed(file,npno::sampleRate,values,stereo?2:1,&hash);
   names[stroke.index].push_back(relative);strokeResults[stroke.index].hashes.push_back(std::move(hash));strokeResults[stroke.index].bytes+=values.size()*2;
  }
 };
 auto work=[&](){try{WavWriter wav(library);for(;;){if(failed.load())return;conversionCheckpoint(options);auto i=nextStroke.fetch_add(1);if(i>=module.strokes.size())return;convertStroke(module.strokes[i],wav);}}
  catch(...){std::lock_guard<std::mutex> lock(errorMutex);if(!failure)failure=std::current_exception();failed=true;}};
 // Bound peak decoding/interleaving memory as well as CPU competition with live audio.
 auto workers=std::min<std::size_t>({std::max(1u,std::thread::hardware_concurrency()),4u,module.strokes.size()});
 std::vector<std::thread> pool;
 try{for(std::size_t n=1;n<workers;++n)pool.emplace_back(work);work();}
 catch(...){failed=true;for(auto& thread:pool)thread.join();throw;}
 for(auto& thread:pool)thread.join();if(failure)std::rethrow_exception(failure);
 for(const auto& stroke:module.strokes){++recordsExtracted;auto& result=strokeResults[stroke.index];overlapChecked+=result.overlap;extracted+=result.bytes;
  for(std::size_t lane=0;lane<names[stroke.index].size();++lane){out.samples.push_back(std::filesystem::u8path(names[stroke.index][lane]));out.sampleHashes.push_back(std::move(result.hashes[lane]));}
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
   for(int c=0;c<lanes;++c){
    ConvertedZone z;z.sample=names[stroke.index][c];z.rootSource="npno_keymap";z.root=stroke.root;z.keyLow=keyZone.low;z.keyHigh=keyZone.high;z.velLow=stroke.velLow;z.velHigh=stroke.velHigh;z.frames=stroke.frames;z.rate=npno::sampleRate;z.channels=module.channels==2&&lanes==1?2:1;z.sourceBank=stroke.bank;z.tuneCents=keyZone.fineTune*0.7;
    z.loop=stroke.loopLength!=0;z.loopStart=stroke.loopStart;z.loopEnd=stroke.loopStart+stroke.loopLength;
    auto gain=module.gain*0.1-stroke.trim*(module.streamVersion==0x450?1.0:0.1);
    z.volumeDb=module.streamVersion==0x464?std::min(10.8,gain):gain;
    z.volumeDb=std::clamp(z.volumeDb,-144.0,10.8);

    if(stroke.bank==2)z.volumeDb=std::max(-144.0,z.volumeDb+releaseBankInitialGainDb);
    // Remove sfizz's additional velocity amplitude law only on this format.
    // Source PCM and trim supply inter-layer dynamics; measured calibration is optional.
    z.ampVeltrack=0;
    z.releaseSeconds=keyZone.undamped?2.0:0.2;z.releaseTrigger=stroke.bank==2;
    if(z.releaseTrigger)z.releaseSeconds=0.2;
    if(module.channels==2&&lanes==2){z.hasPan=true;z.pan=c==0?-100:100;z.volumeDb=std::max(-144.0,z.volumeDb+monoFallbackPanGainDb);}
    if(banks[1]){if(stroke.bank==0)z.cc64High=63;else if(stroke.bank==1)z.cc64Low=64;}
    z.pcmPeak=peaks[stroke.index];z.sourceStroke=stroke.index;z.sourceLoopDecay=stroke.loopDecay;if(z.loop&&!z.releaseTrigger){z.decaySeconds=npnoLoopDecaySeconds(stroke.loopDecay);if(z.decaySeconds>=0)z.sustainPercent=25;}
    if(stroke.bank<2){for(const auto& band:module.velocityBands)if(band.root==stroke.root&&(stroke.bank==0?band.attackStroke:band.pedalStroke)==int(stroke.index)){auto r=z;r.velLow=band.low;r.velHigh=band.high;r.canonicalBand=band.index;r.pedalClass=band.pedalClass;r.bandInherited=band.inherited;out.zones.push_back(r);++emittedRegions;}}
    else {out.zones.push_back(z);++emittedRegions;}
   }
  }
 }
 auto expectedWavs=module.strokes.size()*lanes;
 if(groups!=module.zones.size()||recordsExtracted!=module.strokes.size()||out.samples.size()!=expectedWavs||declared!=extracted||expectedRegions!=emittedRegions)throw std::runtime_error("Módulo de piano: auditoria de extração divergente; importação cancelada");
 audit<<"],\n\"zones_expected\":"<<module.zones.size()<<",\"zones_generated\":"<<groups<<",\n\"sample_records_expected\":"<<module.strokes.size()<<",\"sample_records_extracted\":"<<recordsExtracted<<",\n\"wav_files_expected\":"<<expectedWavs<<",\"wav_files_generated\":"<<out.samples.size()<<",\n\"regions_expected\":"<<expectedRegions<<",\"regions_generated\":"<<emittedRegions<<",\n\"pcm_bytes_declared\":"<<declared<<",\"pcm_bytes_extracted\":"<<extracted<<",\n\"compressed_audio_bytes\":"<<compressed<<",\"overlap_values_checked\":"<<overlapChecked<<",\n\"divergences\":0,\"audio_fidelity_verified\":false\n}\n";
 auto report=output/input.filename();report+=".audit.json";
 out.preservedFeatures.insert(out.preservedFeatures.end(),{"Original binary with SHA-256; full body prefix and every sample record in source_metadata; per-channel WAV hashes; separate extraction and velocity audit.","All source sample records are decoded, including unmapped/unreachable layers; only reachable regions are played."});
 status("calibrando níveis e sustain offline...");calibrateSfzZones(out.zones,out,effective,output);prepareNpnoSustain(out.zones,out,effective,output,library);prepareSfzZones(out.zones,out,effective);auto auditText=audit.str();auto at=auditText.find("\"divergences\"");auditText.insert(at,"\"regions_after_calibration_policy\":"+std::to_string(out.zones.size())+",\n");atomicWrite(report,auditText);auto result=transaction.finish(input,out,writeSfzInstrument(out.zones));status("concluído");return result;
}
}

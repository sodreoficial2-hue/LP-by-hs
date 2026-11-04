// SPDX-License-Identifier: AGPL-3.0-or-later
// Port/adapter: OpenNord nsmp.ts + nsmp-audio.ts normalizeChannels.
#include "nsmp/nsmp.hpp"
#include "sfz_conversion.hpp"
#include <set>
#include <iomanip>
namespace lp {
std::filesystem::path nsmpParaSFZ(const std::filesystem::path& input,const std::filesystem::path& output,MetaSidecar& out,const std::filesystem::path& library,const ConversionOptions& options){
 out=MetaSidecar{};out.originalSha256=sha256File(input);auto b=nsmp::readFile(input);auto id=nsmp::identifyNsmp(b);auto sections=nsmp::parseNsmpSections(b,id);auto zones=nsmp::readNsmpZones(b,id,sections);out.name=input.stem().u8string();out.codec=id.codec;out.format=id.codec>=3?"NSMP"+std::to_string(id.codec):"NSMP";out.version=std::to_string(id.header.version);out.signatureHex="4342494e";out.signatureConfirmed=true;out.checksumValid=nsmp::verifyChecksum(b,id);out.status="converted_nord_sfz_wav";
 out.limitations={
  "Source: OpenNord 67ba1b657ff65a803d568f0ba1fba08598fdc830, AGPL-3.0-or-later. C++ codec/zone port; original opaque headers retained below.",
  "PCM scaling follows OpenNord normalizeChannels: one shared peak across a stroke's channels, not the unrecovered Nord header normalize gain. Map global/per-key/known legacy per-zone level and detune are applied separately. Hardware loudness equivalence is NOT established.",
  "WAV uses IEEE float32. Raw integer PCM, predictor state and channel packing are tested separately against OpenNord golden vectors; no SF2/16-bit export is used.",
  "Replay rate follows stroke u16 pitch base (OpenNord fallback 35002 Hz); this numeric rate is source-derived, not independently verified on Nord hardware.",
  "Loop ends in Nord are exclusive; SFZ/sfizz ends are inclusive, so end/loop_end subtract one frame. Only pointers resolving to decoded segment marks become loops.",
  "Alternate start and loop-only decay are preserved in sidecar, not approximated with a global SFZ envelope. Nord style envelopes/effects, SampleUnison/random alternatives and Tru-Vibrato are preserved as raw map/sty data, without inventing opcode equivalents.",
  "zoneMode=1 maps to pitch_keytrack=0; other unrecognized zone mode/playback flags are preserved in sidecar. oneShot uses loop_mode=one_shot; otherwise known loops use loop_continuous.",
  "Per-key gain/tune variation is expressed with separate SFZ regions per key when necessary; zero gain regions are omitted explicitly. Unmapped strokes are still exported as WAV and listed in sidecar features.",
  "Implementation rejects malformed/truncated data instead of OpenNord's best-effort partial decoding; limits: 256 MiB input, 2048 sections, 512 strokes, 4096 source zones, 16 million PCM values per stroke and 64 million total."
 };
 if(!out.checksumValid)out.limitations.push_back("Original checksum mismatch (CRC32/CRC16). Reported without blocking conversion; structural bounds remain mandatory.");
 const nsmp::Section* map=nullptr;
 for(const auto& s:sections){auto tail=[&](const char* suffix){return s.tag.size()>=3&&s.tag.compare(s.tag.size()-3,3,suffix)==0;};if(tail("hdr")){auto name=nsmp::readName(b,s);if(!name.empty())out.name=name;}if(tail("map"))map=&s;
  if(!tail("stk")){if(s.size>1024*1024)throw std::runtime_error("Nord: metadados muito grandes para sidecar");out.preservedFeatures.push_back("section="+s.tag+" version="+std::to_string(s.version)+" payload_offset="+std::to_string(s.payload)+" bytes="+bytesHex(b.data()+s.payload,s.size));}
 }
 if(!map)throw std::runtime_error("Nord: seção map ausente");nsmp::View mv(b,map->end);mv.require(map->payload,6);auto globalLevel=mv.be(map->payload,3)/1048576.;auto rawGlobal=mv.be(map->payload+3,3);auto signedGlobal=rawGlobal>=0x800000?std::int32_t(rawGlobal)-0x1000000:std::int32_t(rawGlobal);auto globalTune=nsmp::dsp2Detune(signedGlobal);auto rows=nsmp::readPerNoteRows(b,id,*map);
 if(rows.empty())out.limitations.push_back("Per-key level/detune rows unavailable in this source layout; only global and known zone values applied, original map preserved.");
 ConversionOutput transaction(output);std::filesystem::create_directory(output/"Samples");WavWriter wav(library);auto sfz=sfzStream();std::size_t total=0,index=0;std::set<std::uint32_t> mapped;
 for(const auto& section:sections){if(section.tag.size()<3||section.tag.compare(section.tag.size()-3,3,"stk")!=0)continue;
  auto stroke=nsmp::decodeStrokeSection(b,id,section);auto frames=stroke.audio.channels[0].size();auto channels=stroke.audio.channels.size();total+=frames*channels;if(total>nsmp::maxPcmValues)throw std::runtime_error("Nord: limite total de PCM excedido");
  double peak=0;for(const auto& ch:stroke.audio.channels)for(auto x:ch)peak=std::max(peak,std::abs(double(x)));std::vector<float> pcm(frames*channels);for(std::size_t i=0;i<frames;++i)for(std::size_t c=0;c<channels;++c)pcm[i*channels+c]=peak>0?static_cast<float>(stroke.audio.channels[c][i]/peak):0;
  std::ostringstream sample;sample<<"Samples/stroke-"<<std::setfill('0')<<std::setw(4)<<index++<<"-"<<stroke.id<<".wav";auto relative=sample.str();wav.write(output/std::filesystem::u8path(relative),stroke.rate,static_cast<int>(channels),pcm);out.sampleHashes.push_back(sha256File(output/std::filesystem::u8path(relative)));out.samples.push_back(std::filesystem::u8path(relative));
  out.preservedFeatures.push_back("stroke="+std::to_string(stroke.id)+" header="+bytesHex(b.data()+section.payload,stroke.audio.start-section.payload)+" raw_pcm_peak="+std::to_string(peak)+" exported="+relative);
  bool used=false;for(const auto& zone:zones)if(zone.id==stroke.id){used=true;mapped.insert(zone.id);ConvertedZone z;z.sample=relative;z.rootSource="Nord_map_and_stroke_header";z.root=zone.root;z.keyLow=zone.low;z.keyHigh=zone.high;z.velLow=zone.velLow;z.velHigh=zone.velHigh;z.rate=stroke.rate;z.channels=static_cast<int>(channels);z.frames=frames;z.zoneMode=zone.mode;z.zonePlayback=zone.playback;z.zoneOneShot=zone.oneShot;z.pcmPeak=peak;
   if(stroke.loop){z.loop=stroke.loop->loops;z.loopStart=stroke.loop->start;z.loopEnd=stroke.loop->end;z.altStart=stroke.loop->alt;z.decayDbPerSecond=stroke.loop->decayDbPerSecond;}else out.limitations.push_back("stroke="+std::to_string(stroke.id)+": loop pointers not resolvable; no loop emitted, original header preserved.");
   // Preserve per-key differences: consolidate adjacent keys only when the actual level/detune agrees.
   for(int key=zone.low;key<=zone.high;){auto level=globalLevel*zone.level;auto tune=globalTune+zone.detune;if(!rows.empty()){const auto& r=rows[key];auto gain=(std::uint32_t(r[0])<<16)|(std::uint32_t(r[1])<<8)|r[2];auto raw=(std::uint32_t(r[3])<<16)|(std::uint32_t(r[4])<<8)|r[5];level*=gain/1048576.;tune+=nsmp::dsp2Detune(raw>=0x800000?std::int32_t(raw)-0x1000000:std::int32_t(raw));}
    int last=key;while(last<zone.high&&(rows.empty()||rows[last+1]==rows[key]))++last;auto region=z;region.keyLow=key;region.keyHigh=last;region.volumeDb=linearGainDb(level);region.muted=!(std::isfinite(level)&&level>0);region.tuneCents=tune;out.zones.push_back(region);if(level>0)writeSfzRegion(sfz,region);else out.preservedFeatures.push_back("Muted region omitted: stroke="+std::to_string(stroke.id)+" keys="+std::to_string(key)+".."+std::to_string(last));key=last+1;
   }
  }if(!used)out.preservedFeatures.push_back("Unmapped stroke exported without a playing SFZ region: id="+std::to_string(stroke.id));
 }
 for(const auto& zone:zones)if(!mapped.count(zone.id))throw std::runtime_error("Nord: zona perdeu seu áudio; conversão cancelada");
 calibrateSfzZones(out.zones,out,options,output);prepareSfzZones(out.zones,out,options);return transaction.finish(input,out,writeSfzInstrument(out.zones));
}
}

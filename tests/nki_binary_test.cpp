#include "nki_reader.hpp"
#include "sfz_conversion.hpp"
#include <iostream>
#include <fstream>
#include <set>
using namespace lp;
void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
std::string text(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"missing "+p.u8string());return {std::istreambuf_iterator<char>(f),{}};}
template<class F>void rejects(F f,const std::string& contains){try{f();}catch(const std::exception& e){check(std::string(e.what()).find(contains)!=std::string::npos,"wrong error: "+std::string(e.what()));return;}throw std::runtime_error("expected rejection: "+contains);}
int main(int argc,char** argv){try{
 check(argc==4,"sndfile fixtures output");std::filesystem::path lib=argv[1],fixtures=argv[2],root=std::filesystem::absolute(argv[3]);std::filesystem::remove_all(root);std::filesystem::create_directories(root);MetaSidecar m;
 for(const char* name:{"multi.nki","gzip.nki","offset-0.nki","offset-17.nki","offset-255.nki","false-candidate.nki"}){
  auto sfz=nkiParaSFZ(fixtures/name,root/name,m,lib);check(m.signatureConfirmed&&m.status=="binary_k2_xml_sfz","must use binary path");check(m.zones.size()==3,"valid zones/defaults/discards");auto a=text(sfz);check(a.find("loop_mode=loop_sustain loop_start=200 loop_end=22049")!=std::string::npos,"mode1 exact points");check(a.find("loop_mode=loop_continuous loop_start=100 loop_end=44099")!=std::string::npos,"mode2 exact points");check(a.find("hikey=70")!=std::string::npos&&a.find("lokey=71")!=std::string::npos,"neighbor overlap split");check(std::abs(m.zones[0].volumeDb+6.020599913279624)<1e-9,"linear gain dB");check(std::abs(m.zones[1].tuneCents-0.5)<1e-9,"fractional ratio tune");check(m.zones[2].rate==48000&&m.zones[2].channels==2&&m.zones[2].root==60,"stereo rate and defaults");check(text(root/name/std::filesystem::u8path(u8"program-2-Estéreo.sfz")).find("no_loop")!=std::string::npos,"separate program output");check(m.preservedFeatures[0].find("opaqueField")!=std::string::npos,"unknown fields preserved");check(m.limitations.size()>=10,"skip and overlap warnings");
 }
 nkiParaSFZ(fixtures/"aiff.nki",root/"aiff",m,lib);check(m.zones[0].channels==2&&m.zones[0].rate==44100,"external AIFF stereo original rate");
 auto mono=nkiParaSFZ(fixtures/"monolith.nki",root/"mono",m,lib);check(m.signatureConfirmed&&m.zones.size()==1&&m.zones[0].rate==44100,"plain embedded RIFF monolith");
 nkiParaSFZ(fixtures/"fallback.nki",root/"fallback",m,lib,fixtures/"fallback-samples");check(!m.signatureConfirmed&&m.limitations[0].find("fallback")!=std::string::npos,"declared fallback");
 for(const auto& item:std::vector<std::pair<std::string,std::string>>{{"compressed-corrupt.nki","corrompido"},{"expansion-limit.nki","16 MiB"},{"missing.nki","arquivo ausente.wav"},{"truncated.nki","truncado"},{"malformed-xml.nki","XML inválido"},{"dtd.nki","DTD"},{"gap.nki","Auditoria velocity"},{"overlap-velocity.nki","sobrepostas"}}){
  rejects([&]{nkiParaSFZ(fixtures/item.first,root/item.first,m,lib);},item.second);check(!std::filesystem::exists(root/item.first),"rollback after error");
 }
 rejects([&]{detectarTimbreFormat(fixtures/"project.nsmpproj");},"projeto de texto");
 auto renamed=root/"false.nsmp3";atomicWrite(renamed,text(fixtures/"multi.nki"));rejects([&]{detectarTimbreFormat(renamed);},"CBIN");
 auto rr=nkiParaSFZ(fixtures/"rr.nki",root/"rr",m,lib);check(m.zones.size()==4,"stable RR region count");auto rrText=text(rr);check(rrText.find("seq_length=2 seq_position=2")!=std::string::npos,"explicit RR");
 auto noCross=rrText;ConversionOptions opt;opt.velocityCrossfade=true;nkiParaSFZ(fixtures/"rr.nki",root/"crossfade",m,lib,{},opt);check(text(root/"crossfade/rr.nki.converted.sfz").find("xfin_lovel=")!=std::string::npos&&noCross.find("xfin_lovel=")==std::string::npos,"crossfade opt in");
 ConvertedZone z;z.sample="Samples/áudio com espaço.wav";z.frames=100;auto stream=sfzStream();check(writeSfzRegion(stream,z),"UTF8 sample path");check(stream.str().find(z.sample)!=std::string::npos,"spaces preserved, never percent encoded");z.sample="C:/absolute.wav";rejects([&]{writeSfzRegion(stream,z);},"caminho");z.sample="Samples/x.wav";z.volumeDb=std::numeric_limits<double>::quiet_NaN();std::vector<std::string> warnings;check(!writeSfzRegion(stream,z,&warnings)&&!warnings.empty(),"NaN omitted with warning");check(linearGainDb(0)==-144&&linearGainDb(-1)==-144,"floor");
 std::cout<<"PASS: NKI binary zlib/gzip offsets, programs, defaults/discards, linear gain, loops, overlap partition, Unicode/stereo, embedded RIFF, explicit fallback, preserved XML, RR/crossfade, errors/rollback and project rejection\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

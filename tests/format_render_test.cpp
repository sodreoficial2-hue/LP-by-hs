#include "sfizz_probe.hpp"
#include "sfz_conversion.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
using namespace lp;
void check(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
double energy(const std::array<std::vector<float>,2>& a){double e=0;for(const auto& c:a)for(auto v:c){check(std::isfinite(v),"nonfinite output");e+=double(v)*v;}return e;}
double frequency(const std::vector<float>& samples,int rate,std::size_t first,std::size_t last){std::vector<double> crossings;last=std::min(last,samples.size());for(std::size_t i=std::max<std::size_t>(first,1);i<last;++i)if(samples[i-1]<0&&samples[i]>=0)crossings.push_back(i-1+(-samples[i-1])/(samples[i]-samples[i-1]));check(crossings.size()>2,"not enough reference zero crossings");return rate*(crossings.size()-1)/(crossings.back()-crossings.front());}
int main(int argc,char** argv){try{
 check(argc==5,"sfizz sndfile data output");std::filesystem::path lib=argv[1],sndfile=argv[2],data=std::filesystem::absolute(argv[3]),output=std::filesystem::absolute(argv[4]);std::filesystem::remove_all(output);std::filesystem::create_directories(output);
 auto fixtures=std::filesystem::exists(data/"tests/fixtures")?data/"tests/fixtures":data/"import-fixtures/tests/fixtures";
 MetaSidecar nki,nsm,npn;auto nkiFile=nkiParaSFZ(data/"nki-fixtures/rr.nki",output/"nki",nki,sndfile);auto nsmFile=nsmpParaSFZ(data/"pitch-fixtures/pitch-og.nsmp",output/"nsmp",nsm,sndfile);auto npnFile=npnoParaSFZ(data/"pitch-fixtures/pitch-v5.npno",output/"npno",npn,sndfile);
 sampling::Decoder decoder(sndfile);WavWriter writer(sndfile);std::ofstream report(output/"metrics.json");report<<"{\"sfizz_version\":\"1.2.3\",\"render_rate\":48000,\"voice_limit\":128,\"pitch\":[";bool first=true;
 // Compare the actual exported sample frequency with the output at its stored root.
 // NKI is an absolute 440-Hz fixture; binary sample formats use their independent tones.
 for(auto item:std::vector<std::tuple<std::string,std::filesystem::path,MetaSidecar*>>{{"NKI",nkiFile,&nki},{"NSMP",nsmFile,&nsm},{"NPNO",npnFile,&npn}}){
  auto name=std::get<0>(item);auto sfz=std::get<1>(item);auto& meta=*std::get<2>(item);const auto& z=meta.zones[0];auto audio=decoder.decode(sfz.parent_path()/std::filesystem::u8path(z.sample));std::vector<float> mono;for(std::size_t i=0;i<audio.frames();++i)mono.push_back(audio.pcm[i*audio.channels]);
  std::size_t end=std::min<std::size_t>(audio.frames(),8000);if(end<500)end=std::min<std::size_t>(audio.frames(),850);
  double expected=frequency(mono,audio.rate,20,end)*std::pow(2.,z.tuneCents/1200.);SfizzProbe synth(lib);synth.load(sfz);synth.on(synth.synth,0,z.root,std::clamp(z.velLow+1,1,z.velHigh));auto rendered=synth.render(std::max<int>(1024,end*48000/audio.rate));
  auto measured=frequency(rendered[0],48000,30,std::min<std::size_t>(rendered[0].size(),end*48000/audio.rate));double cents=1200*std::log2(measured/expected);check(std::abs(cents)<3,name+" pitch tolerance exceeded: "+std::to_string(cents));
  check(std::abs(1200*std::log2(measured/440.))<3,name+" absolute 440 Hz reference");
  if(!first)report<<',';first=false;report<<"{\"format\":\""<<name<<"\",\"source_rate\":"<<audio.rate<<",\"expected_hz\":"<<expected<<",\"rendered_hz\":"<<measured<<",\"error_cents\":"<<cents;
  std::array<double,3> dynamicEnergy{};std::vector<float> preview;int sounded=0,unmapped=0;
  for(int pedal:{0,127})for(int velocity:{1,64,127})for(int key=0;key<128;++key){
   bool mapped=false;for(const auto& region:meta.zones)if(key>=region.keyLow&&key<=region.keyHigh&&velocity>=region.velLow&&velocity<=region.velHigh&&!region.releaseTrigger&&(region.cc64Low<0||pedal>=region.cc64Low)&&(region.cc64High<0||pedal<=region.cc64High))mapped=true;
   if(!mapped){++unmapped;continue;}SfizzProbe note(lib);note.load(sfz);note.cc(note.synth,0,64,pedal);note.on(note.synth,0,key,velocity);auto sound=note.render(1024);check(energy(sound)>1e-17,name+" mapped note silent "+std::to_string(key));if(key==z.root&&pedal==0)dynamicEnergy[velocity==1?0:velocity==64?1:2]=energy(sound);note.off(note.synth,0,key,0);note.cc(note.synth,0,64,0);note.render(12288);++sounded;
   if(key==z.root&&velocity==127&&pedal==0)for(std::size_t i=0;i<sound[0].size();++i){preview.push_back(sound[0][i]);preview.push_back(sound[1][i]);}
  }
  check(dynamicEnergy[0]>0&&dynamicEnergy[0]<dynamicEnergy[1]&&dynamicEnergy[1]<dynamicEnergy[2],name+" velocities must have increasing measured energy");report<<",\"velocity_energy\":["<<dynamicEnergy[0]<<','<<dynamicEnergy[1]<<','<<dynamicEnergy[2]<<"]}";
  if(!preview.empty())writer.write(output/(name+"-preview.wav"),48000,2,preview);
  std::cout<<name<<" pitch_cents="<<cents<<" sounded="<<sounded<<" unmapped_source_keys="<<unmapped<<'\n';
 }
 // Additional family/generation pitch gates use the same independently encoded 440-Hz input.
 for(const char* name:{"pitch-v2.nsmp","pitch-v3.nsmp3","pitch-v4.nsmp4","pitch-v6.npno"}){
  std::filesystem::remove_all(output/name);
  MetaSidecar m;auto file=data/"pitch-fixtures"/name;auto sfz=timbreExtension(file)==".npno"?npnoParaSFZ(file,output/name,m,sndfile):nsmpParaSFZ(file,output/name,m,sndfile);SfizzProbe synth(lib);synth.load(sfz);synth.on(synth.synth,0,69,100);auto audio=synth.render(8192);auto measured=frequency(audio[0],48000,400,8000);auto cents=1200*std::log2(measured/440.);check(std::abs(cents)<3,std::string(name)+" 440-Hz pitch gate");report<<",{\"format\":\""<<name<<"\",\"rendered_hz\":"<<measured<<",\"error_cents\":"<<cents<<'}';
 }
 report<<"],\"round_robin\":{";
 {SfizzProbe rr(lib);rr.load(nkiFile);check(rr.regions(rr.synth)==4,"RR count must not grow per key");rr.on(rr.synth,0,69,32);rr.render(512);check(rr.voices(rr.synth)==1,"one selected RR voice per note");rr.on(rr.synth,0,70,32);rr.render(512);check(rr.voices(rr.synth)==2,"two notes require exactly two voices, not four");report<<"\"regions\":4,\"one_note_voices\":1,\"two_note_voices\":2}";}
 {SfizzProbe pedal(lib);pedal.load(nkiFile);pedal.cc(pedal.synth,0,64,127);pedal.on(pedal.synth,0,69,32);pedal.render(512);pedal.off(pedal.synth,0,69,0);pedal.render(24000);check(pedal.voices(pedal.synth)>0,"pedal must hold sustain loop");pedal.cc(pedal.synth,0,64,0);pedal.render(24000);check(pedal.voices(pedal.synth)==0,"pedal up must release voice");report<<",\"pedal_hold_release\":true";}
 {std::filesystem::remove_all(output/"multi");MetaSidecar m;auto multi=nkiParaSFZ(data/"nki-fixtures/multi.nki",output/"multi",m,sndfile);for(int key:{68,69,70,71}){SfizzProbe single(lib);single.load(multi);single.on(single.synth,0,key,100);single.render(512);check(single.voices(single.synth)==1,"overlap key must trigger one zone");}}
 {auto dir=output/"unicode";std::filesystem::remove_all(dir);std::filesystem::create_directories(dir/"Samples");auto relative=std::filesystem::u8path(u8"Samples/áudio com espaço.wav");std::filesystem::copy_file(output/"nki/Samples/sample-0.wav",dir/relative);ConvertedZone z;z.sample=relative.generic_u8string();z.frames=88200;z.root=69;auto sfz=dir/"unicode.sfz";atomicWrite(sfz,writeSfzInstrument({z}));SfizzProbe unicode(lib);unicode.load(sfz);unicode.on(unicode.synth,0,69,100);check(energy(unicode.render(2048))>1e-10,"SFZ UTF8/space path must actually load");}
 report<<",\"load\":[";
 for(int cross=0;cross<2;++cross){
  auto dir=output/(cross?"large-on":"large-off");std::filesystem::remove_all(dir);std::filesystem::create_directories(dir/"Samples");std::filesystem::copy_file(output/"nki/Samples/sample-0.wav",dir/"Samples/tone.wav");std::vector<ConvertedZone> zones;
  for(int key=21;key<=108;++key)for(int layer=0;layer<6;++layer)for(int rr=1;rr<=2;++rr){ConvertedZone z;z.sample="Samples/tone.wav";z.frames=88200;z.rate=44100;z.channels=1;z.root=69;z.keyLow=z.keyHigh=key;z.velLow=layer*21+1;z.velHigh=layer==5?127:(layer+1)*21;z.sequenceLength=2;z.sequencePosition=rr;z.loop=true;z.loopStart=44100;z.loopEnd=88200;z.releaseSeconds=10;zones.push_back(z);}
  MetaSidecar m;ConversionOptions options;options.velocityCrossfade=cross;prepareSfzZones(zones,m,options);auto sfz=dir/"large.sfz";atomicWrite(sfz,writeSfzInstrument(zones));SfizzProbe large(lib);large.load(sfz);large.cc(large.synth,0,64,127);int peak=0,exhausted=-1;auto begin=std::chrono::steady_clock::now();
  for(int chord=0;chord<24;++chord){for(int k=0;k<8;++k){int key=48+k;large.on(large.synth,0,key,21);}large.render(4800);for(int k=0;k<8;++k)large.off(large.synth,0,48+k,0);int active=large.voices(large.synth);peak=std::max(peak,active);if(active>=128&&exhausted<0)exhausted=chord;}
  auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();check(peak>=128&&exhausted>=0,"load fixture did not reach configured voice budget");if(cross)report<<',';report<<"{\"crossfade\":"<<(cross?"true":"false")<<",\"regions\":"<<zones.size()<<",\"peak_voices\":"<<peak<<",\"first_limit_chord\":"<<exhausted+1<<",\"simulated_seconds_at_limit\":"<<(exhausted+1)*0.1<<",\"render_cpu_ms\":"<<ms<<'}';std::cout<<"crossfade="<<cross<<" peak_voices="<<peak<<" first_limit_chord="<<exhausted+1<<" simulated_seconds="<<(exhausted+1)*.1<<" cpu_ms="<<ms<<'\n';
 }
 report<<"],\"original_keyboard_audio_comparison\":false}\n";
 std::cout<<"PASS: real sfizz pitch <3 cents, mapped MIDI scale dynamics, CC64 loops, RR voices and sustained 88-key/6-layer/2-RR load\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

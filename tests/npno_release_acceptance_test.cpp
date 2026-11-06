#include "npno_release.hpp"
#include "sfizz_probe.hpp"
#include <iostream>
using Audio=std::array<std::vector<float>,2>;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double peak(const Audio& data){double result=0;for(const auto& channel:data)for(float x:channel){check(std::isfinite(x),"nonfinite audio");result=std::max(result,std::abs(double(x)));}return result;}
int main(int argc,char** argv){try{
 check(argc==5,"sfizz sndfile fixture output");auto root=std::filesystem::absolute(argv[4]);std::filesystem::remove_all(root);
 lp::ConversionOptions options;options.sustainSfizzLibrary=argv[1];lp::MetaSidecar meta;
 auto full=lp::npnoParaSFZ(argv[3],root,meta,argv[2],{},options);
 auto releases=meta.zones,muted=meta.zones;
 for(auto& z:releases)if(!z.releaseTrigger)z.muted=true;
 for(auto& z:muted)if(z.releaseTrigger)z.muted=true;
 // rt_dead is used only in the isolated diagnostic: no attack voices exist.
 auto text=lp::writeSfzInstrument(releases);
 text=std::regex_replace(text,std::regex("trigger=release "),"trigger=release rt_dead=on ");
 auto releaseFile=root/"isolated.sfz",mutedFile=root/"muted.sfz";
 lp::atomicWrite(releaseFile,text);lp::atomicWrite(mutedFile,lp::writeSfzInstrument(muted));
 for(int velocity:{40,80,110})for(int hold:{6000,96000,240000}){
  SfizzProbe synth(argv[1]);synth.load(releaseFile);
  check(peak(synth.render(256))<1e-9,"startup triggered release");
  synth.on(synth.synth,0,60,velocity);check(peak(synth.render(hold))<1e-9,"key-down triggered release");
  synth.off(synth.synth,0,60,0);check(peak(synth.render(48000))<1e-9,"key-up without pedal triggered release");
  synth.cc(synth.synth,0,64,64);synth.on(synth.synth,0,60,velocity);
  check(peak(synth.render(hold))<1e-9,"holding key/pedal triggered release");
  synth.off(synth.synth,0,60,0);check(peak(synth.render(24000))<1e-9,"key-up with pedal down triggered release");
  synth.cc(synth.synth,0,64,127);check(peak(synth.render(256))<1e-9,"pedal-down value change triggered release");
  synth.cc(synth.synth,0,64,63);auto releasePeak=peak(synth.render(48000));std::cout<<"velocity="<<velocity<<" hold="<<hold<<" peak="<<releasePeak<<std::endl;check(releasePeak>1e-8,"pedal-up failed to play queued release");
  synth.render(240000);
  for(int value:{63,0,0,64,127,0}){synth.cc(synth.synth,0,64,value);check(peak(synth.render(48000))<1e-9,"pedal without pending notes retriggered release");}
  // A finger still holding a key keeps its damper raised after pedal-up.
  synth.cc(synth.synth,0,64,127);synth.on(synth.synth,0,60,velocity);synth.render(hold);
  synth.cc(synth.synth,0,64,0);check(peak(synth.render(24000))<1e-9,"pedal-up released a physically held key");
  synth.off(synth.synth,0,60,0);check(peak(synth.render(48000))<1e-9,"key-up after pedal triggered release");
 }
 // Complete instrument: muting the auxiliary must leave attack and key-off tail
 // byte-identical, and must create a difference only after actual pedal-up.
 for(int pedal:{0,127}){
  SfizzProbe on(argv[1]),off(argv[1]);on.load(full);off.load(mutedFile);
  on.cc(on.synth,0,64,pedal);off.cc(off.synth,0,64,pedal);
  on.on(on.synth,0,60,80);off.on(off.synth,0,60,80);
  check(on.render(96000)==off.render(96000),"attack changed");
  on.off(on.synth,0,60,0);off.off(off.synth,0,60,0);
  check(on.render(24000)==off.render(24000),"auxiliary sounded at key release");
  if(pedal){on.cc(on.synth,0,64,0);off.cc(off.synth,0,64,0);check(on.render(12000)!=off.render(12000),"full instrument pedal-up has no auxiliary");}
 }
 for(std::size_t i=0;i<meta.samples.size();++i)check(lp::sha256File(root/meta.samples[i])==meta.sampleHashes[i],"PCM changed");
 lp::atomicWrite(root/"pedal-release-acceptance.json","{\"accepted\":true,\"velocities\":[40,80,110],\"hold_seconds\":[0.125,2,5],\"key_release_silent\":true,\"pedal_up_audible\":true,\"repeated_cc_no_retrigger\":true,\"normal_tail_preserved\":true}\n");
 std::cout<<"PASS: actual sfizz; no auxiliary on key-up (pedal up/down), pedal-up-only queue, threshold 64/63, long holds, repeated CC, held keys, unchanged attack/string tail and PCM\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

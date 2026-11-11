#include "audio_host.hpp"
#include "my_sounds.hpp"
#include "sound_parameters.hpp"
#include <cassert>
#include <fstream>
#include <thread>
#include <iostream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);
namespace lp {struct AudioHostGraphProbe {static std::string reference(const AudioHost& h,int i=0){return h.preparedParts[i]?h.preparedParts[i]->reference:"";}static float gain(const AudioHost& h,int i=0){return h.preparedParts[i]->levelGain;}};}
void ready(lp::AudioHost& h){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!h.pollPerformance()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){
 assert(argc==2);auto lib=std::filesystem::absolute(argv[1]),cwd=std::filesystem::current_path();auto root=std::filesystem::temp_directory_path()/"lp-sound-paths";std::filesystem::remove_all(root);
 auto app=root/"app",other=root/"cwd",moved=root/"moved";std::filesystem::create_directories(app);std::filesystem::create_directories(other);
 std::ofstream(app/"sound.sfz")<<"fixture";std::ofstream(app/"piano.sf2")<<"fixture";std::ofstream(other/"external.sfz")<<"fixture";
 std::filesystem::current_path(other);
 auto reference=(app/"sound.sfz").u8string();assert(lp::normalizedSoundReference("sub/../sound.sfz",app)==reference);
 assert(lp::normalizedSoundReference("bank://piano",app)=="bank://piano");assert(lp::normalizedSoundReference("performance://0",app)=="performance://0");
 auto sf2=lp::normalizedSoundReference("sf2://2:7:piano.sf2",app);assert(sf2==lp::makeSf2Reference(app/"piano.sf2",2,7));
 assert(lp::portableSoundReference(sf2,app)=="sf2://2:7:piano.sf2");assert(lp::portableSoundReference((other/"external.sfz").u8string(),app)==(other/"external.sfz").u8string());
 lp::saveMySoundRecords(app/"my_sounds.catalog",{{reference,"Piano com espaços"},{sf2,"Outro preset"},{(other/"external.sfz").u8string(),"Externo"}},app);
 auto records=lp::loadMySoundRecords(app/"my_sounds.catalog",app);assert(records[0].first==reference&&records[1].first==sf2);
 assert(lp::soundParameterKey(records[0].first,app)==lp::soundParameterKey(reference,app));
 // Legacy absolute and new relative records retain instrument/parameter identity.
 lp::atomicWrite(app/"legacy.catalog","LPHS_MY_SOUNDS 1\r\n\""+reference+"\" \"Piano\"\r\n");assert(lp::loadMySoundRecords(app/"legacy.catalog",app)[0].first==reference);
 {
  lp::AudioHost host(lib);host.setBase(app);host.setLevelCompensations({{lp::levelKey(reference),.5f}});host.start(1,128);auto p=lp::Patch{}.parameters;
  bool rejected=false;try{host.setBase(other);}catch(const std::logic_error&){rejected=true;}assert(rejected);
  host.load("sub/../sound.sfz",p);host.sendRealtime({0x90,60,100});float l[256],r[256];fakePump(l,r,256);assert(l[255]==.125f&&lp::AudioHostGraphProbe::reference(host)==reference);
  std::filesystem::rename(app/"sound.sfz",app/"renamed.sfz");host.requestSound("./sound.sfz",p);ready(host);fakePump(l,r,256);assert(lp::AudioHostGraphProbe::reference(host)==reference);std::filesystem::rename(app/"renamed.sfz",app/"sound.sfz");
  host.invalidateSound("sub/../sound.sfz");assert(lp::AudioHostGraphProbe::reference(host).empty());
  lp::Performance performance;performance.name="Paths";performance.parts[0].enabled=true;performance.parts[0].instrument="./sound.sfz";performance.parts[1].enabled=true;performance.parts[1].instrument="sub/../sound.sfz";
  host.loadPerformance(performance);assert(lp::AudioHostGraphProbe::reference(host,1)==reference&&lp::AudioHostGraphProbe::gain(host,1)==.5f);
  host.requestPerformance(performance);ready(host);assert(lp::AudioHostGraphProbe::reference(host,1)==reference&&lp::AudioHostGraphProbe::gain(host,1)==.5f);host.stop();
 }
 std::filesystem::rename(app,moved);auto restored=lp::loadMySoundRecords(moved/"my_sounds.catalog",moved);
 assert(restored[0].first==(moved/"sound.sfz").u8string());assert(lp::soundParameterKey(restored[0].first,moved)==lp::soundParameterKey(reference,app));auto preset=lp::parseSf2Reference(restored[1].first);assert(preset&&preset->bank==2&&preset->program==7&&preset->path==moved/"piano.sf2");assert(restored[2].first==(other/"external.sfz").u8string());
 std::filesystem::current_path(cwd);std::filesystem::remove_all(root);std::cout<<"PASS: portable catalog, legacy identity, moved folder, external files, SF2 presets, sync/async loads, cache and calibration independent of CWD\n";
}

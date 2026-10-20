#include "audio_host.hpp"
#include "audio_preferences.hpp"
#include <fstream>
#include <iostream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);void fakeFailOpen();int fakeStarts();int fakeDevice();
void check(bool v){if(!v)throw std::runtime_error("Audio host regression failed");}
int main(int argc,char** argv){
 check(argc==2);auto dir=std::filesystem::temp_directory_path()/"lp-audio-test";std::filesystem::create_directories(dir);
 {std::ofstream good(dir/"good.sfz");good<<"fixture";std::ofstream broken(dir/"broken.sfz");broken<<"fixture";}
 auto preferences=dir/"permanent"/"audio.settings";
 {
 lp::AudioHost host(argv[1]);auto devices=host.devices();check(devices.size()==2&&devices[0].id==1&&devices[1].id==3);
 float left[2048]{},right[2048]{};auto p=lp::Patch{}.parameters;
 lp::connectAndRemember(host,1,128,preferences);check(host.running()&&host.latency()==.004);fakePump(left,right,2048);check(left[0]==0);
 host.load(dir/"good.sfz",p);check(host.loaded()&&host.running()&&host.bufferSize()==128&&fakeDevice()==1);
 host.send({0x90,60,100});fakePump(left,right,2048);check(left[0]==.25f&&right[2047]==.25f);
 p[0]=.5f;int before=fakeStarts();host.parameters(p);fakePump(left,right,2048);check(left[0]==.125f&&fakeStarts()==before);
 for(auto path:{dir/"broken.sfz",dir/"missing.sfz"}){bool rejected=false;try{host.load(path,p);}catch(...){rejected=true;}check(rejected&&host.loaded()&&host.running()&&fakeStarts()==before);fakePump(left,right,2048);check(left[0]==.125f);}
 bool rejected=false;try{host.start(0,128);}catch(...){rejected=true;}check(rejected&&host.running());
 fakeFailOpen();rejected=false;try{host.start(1,256);}catch(...){rejected=true;}check(rejected&&host.running()&&host.bufferSize()==128);
 fakeFailOpen();rejected=false;try{host.load(dir/"good.sfz",p);}catch(...){rejected=true;}check(rejected&&host.loaded()&&host.running()&&host.bufferSize()==128);
 host.send({0x90,60,100});fakePump(left,right,2048);check(left[0]==.125f);
 host.send({0x90,60,100});for(int i=0;i<1200;++i)host.send({0x90,60,100});fakePump(left,right,2048);check(left[0]==0);
 host.send({0x90,60,100});fakePump(left,right,2048);host.panic();fakePump(left,right,2048);check(left[0]==0);
 fakePump(left,right,2048,paOutputUnderflow);check(host.xruns()==1);
 host.controlPanel(1,nullptr);check(host.running()&&host.bufferSize()==128);
 host.unload();check(!host.loaded()&&host.running());fakePump(left,right,2048);check(left[0]==0);host.stop();check(!host.running());
 }
 {lp::AudioHost reopened(argv[1]);auto saved=lp::restoreRememberedAudio(reopened,preferences);check(reopened.running()&&reopened.selectedDevice()==1&&reopened.bufferSize()==128&&saved.driver=="ASIO4ALL");lp::connectAndRemember(reopened,3,256,preferences);}
 {lp::AudioHost reopened(argv[1]);auto saved=lp::restoreRememberedAudio(reopened,preferences);check(reopened.running()&&reopened.selectedDevice()==3&&reopened.bufferSize()==256&&saved.driver=="TASCAM US-1800");}
 lp::AudioSettings{"Unavailable driver",128}.save(preferences);
 {lp::AudioHost reopened(argv[1]);bool unavailable=false;try{lp::restoreRememberedAudio(reopened,preferences);}catch(...){unavailable=true;}check(unavailable&&!reopened.running());}
 check(lp::resolveAsioDriver({{8,"TASCAM"},{13,"ASIO4ALL"}},"ASIO4ALL")==13);
 for(const auto& available:std::vector<std::vector<lp::AudioDevice>>{{{8,"TASCAM"}},{{1,"ASIO4ALL"},{2,"ASIO4ALL"}}}){bool rejected=false;try{lp::resolveAsioDriver(available,"ASIO4ALL");}catch(...){rejected=true;}check(rejected);}
 auto path=dir/"audio.settings";lp::AudioSettings{"ASIO4ALL",128}.save(path);auto setting=lp::AudioSettings::load(path);check(setting.driver=="ASIO4ALL"&&setting.buffer==128);
 {std::ofstream corrupt(path);corrupt<<"LPHS_ASIO 1\n\"ASIO4ALL\" 1\n";}bool rejected=false;try{lp::AudioSettings::load(path);}catch(...){rejected=true;}check(rejected);
 std::filesystem::remove_all(dir);
 std::cout<<"PASS: ASIO-only enumeration, stereo callback, MIDI, live DSP, failed SFZ preservation, same-driver restart, failed driver rollback, overflow panic, panel, audio settings\n";
}

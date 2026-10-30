#include "level_calibration.hpp"
#include "audio_host.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);
#define check(v) do{if(!(v))throw std::runtime_error("Calibration regression line "+std::to_string(__LINE__));}while(false)
lp::LevelProgress run(const std::filesystem::path& library,const std::filesystem::path& file,const std::vector<lp::Instrument>& instruments){
 lp::LevelCalibrationJob job(library,file,instruments);
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(45);
 for(;;){auto result=job.snapshot();if(result.done)return result;if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Calibration timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
}
int main(int argc,char** argv){
 check(argc==2);auto dir=std::filesystem::temp_directory_path()/("lp-calibration-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
 lp::Instrument first;first.sfz=dir/"first.sfz";first.name="First";std::ofstream(first.sfz)<<"test";
 lp::Instrument second=first;second.sfz=dir/"second.sfz";second.name="Second";std::ofstream(second.sfz)<<"test";
 auto file=dir/"levels.cache";
 // Calibration is entirely offline: an already-playing ASIO test stream is intact.
 lp::AudioHost host(argv[1]);host.load(first.sfz,lp::Patch{}.parameters);host.start(1);host.send({0x90,60,100});
 auto result=run(argv[1],file,{first});check(result.failed==0&&result.analyzed==1&&result.reused==0&&result.gains.size()==1);
 float left[256]{},right[256]{};fakePump(left,right,256);check(left[0]==.25f&&host.running());host.stop();
 // A fresh worker/process-style cache load performs zero synth analyses.
 result=run(argv[1],file,{first});check(result.failed==0&&result.analyzed==0&&result.reused==1);
 result=run(argv[1],file,{first,second});check(result.failed==0&&result.analyzed==1&&result.reused==1);
 std::ofstream(first.sfz,std::ios::app)<<"modified";
 result=run(argv[1],file,{first,second});check(result.failed==0&&result.analyzed==1&&result.reused==1);
 lp::Instrument broken=first;broken.sfz=dir/"broken.sfz";std::ofstream(broken.sfz)<<"test";
 result=run(argv[1],file,{first,broken});check(result.failed==1&&result.reused==1&&result.gains.size()==1);
 auto calibrated=result.gains.at(lp::levelKey(first.sfz.u8string()));
 // Static compensation multiplies manual volumes and survives driver restart.
 lp::AudioHost balanced(argv[1]);balanced.setLevelCompensations({{lp::levelKey(first.sfz.u8string()),calibrated}});
 balanced.load(first.sfz,lp::Patch{}.parameters);balanced.start(1);balanced.send({0x90,60,100});fakePump(left,right,256);check(std::abs(left[0]-.25f*calibrated)<1e-6f);
 auto p=lp::Patch{}.parameters;p[0]=.5f;balanced.parameters(p);fakePump(left,right,256);check(std::abs(left[0]-.125f*calibrated)<1e-6f);
 balanced.start(1);balanced.send({0x90,60,100});fakePump(left,right,256);check(std::abs(left[0]-.125f*calibrated)<1e-6f);
 bool rejected=false;try{balanced.setLevelCompensations({});}catch(...){rejected=true;}check(rejected);balanced.stop();
 lp::Performance performance;performance.name="Calibrated layers";
 performance.parts[0].enabled=performance.parts[1].enabled=true;performance.parts[0].instrument=first.sfz.u8string();performance.parts[1].instrument=second.sfz.u8string();
 lp::AudioHost layered(argv[1]);layered.setLevelCompensations({{lp::levelKey(first.sfz.u8string()),.5f},{lp::levelKey(second.sfz.u8string()),.25f}});
 layered.loadPerformance(performance);layered.start(1);layered.send({0x90,60,100});fakePump(left,right,256);check(std::abs(left[0]-.1875f)<1e-6f);layered.stop();
 // Corrupt data are preserved as a backup and recalibrated, never trusted.
 std::ofstream(file)<<"invalid";result=run(argv[1],file,{first});bool preserved=false;for(const auto& entry:std::filesystem::directory_iterator(dir))preserved|=entry.path().filename().string().find("levels.cache.invalid.")==0;check(result.failed==0&&result.analyzed+result.reused==1&&preserved);
 // Cancellation joins before state is destroyed, with no UI/native pointers.
 {lp::LevelCalibrationJob cancelled(argv[1],dir/"cancelled.cache",{first,second});}
 std::filesystem::remove_all(dir);
 std::cout<<"PASS: offline startup calibration, permanent reuse, incremental new/modified presets, isolated failures, untouched live stream, compensated playback/layers/driver restart, corruption recovery and cancellation\n";
}

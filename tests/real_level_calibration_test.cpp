#include "level_calibration.hpp"
#include "audio_host.hpp"
#include <iostream>
#include <chrono>
#include <thread>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);
#define check(v) do{if(!(v))throw std::runtime_error("Real calibration line "+std::to_string(__LINE__));}while(false)
int main(int argc,char** argv){
 check(argc==4);std::atomic<bool> cancel{false};
 lp::LevelCalibrator calibrator(argv[1]);
 lp::Instrument instrument;instrument.name="Synthetic SF2 level reference";
 instrument.sfz=std::filesystem::u8path(lp::makeSf2Reference(argv[2],0,0));
 auto loud=calibrator.measure(instrument,cancel);
 instrument.sfz=std::filesystem::u8path(lp::makeSf2Reference(argv[2],0,1));
 auto quiet=calibrator.measure(instrument,cancel);
 check(loud.audible&&quiet.audible&&quiet.gain>loud.gain);
 float before=20*std::log10(loud.rms/quiet.rms);
 float after=20*std::log10((loud.rms*loud.gain)/(quiet.rms*quiet.gain));
 std::cerr<<"Measured: loud="<<loud.rms<<" gain="<<loud.gain<<" peak="<<loud.peak<<" quiet="<<quiet.rms<<" gain="<<quiet.gain<<" peak="<<quiet.peak<<" before="<<before<<" after="<<after<<'\n';
 check(before>9&&before<11);check(std::abs(after)<1);
 check(loud.peak*loud.gain<.355&&quiet.peak*quiet.gain<.355);
 lp::AudioHost host(argv[1]);
 host.setLevelCompensations({{lp::levelKey(lp::makeSf2Reference(argv[2],0,0)),loud.gain},{lp::levelKey(lp::makeSf2Reference(argv[2],0,1)),quiet.gain}});
 auto playback=[&](int program,float volume){
  auto p=lp::Patch{}.parameters;p[0]=volume;host.loadSf2(argv[2],0,program,p);host.start(1);host.send({0x90,60,110});
  double sum=0;std::array<float,256> left{},right{};
  for(int i=0;i<188;++i){fakePump(left.data(),right.data(),256);for(int j=0;j<256;++j)sum+=(left[j]*left[j]+right[j]*right[j])*.5;}
  host.stop();return std::sqrt(sum/(188*256));
 };
 auto actualLoud=playback(0,1),actualQuiet=playback(1,1);
 auto actualDifference=20*std::log10(actualLoud/actualQuiet);check(std::abs(actualDifference)<1);
 check(std::abs(playback(1,.5f)/actualQuiet-.5)<.01);
 // Restart behavior in fresh worker instances, using real rendered SF2 presets.
 auto run=[&](std::vector<lp::Instrument> bank){
  lp::LevelCalibrationJob job(argv[1],std::filesystem::path(argv[3])/"real-levels.cache",std::move(bank));
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
  for(;;){auto result=job.snapshot();if(result.done)return result;if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Real calibration timed out");std::this_thread::sleep_for(std::chrono::milliseconds(20));}
 };
 auto first=run({instrument});check(first.failed==0&&first.analyzed==1);
 auto next=run({instrument});check(next.failed==0&&next.analyzed==0&&next.reused==1);
 auto other=instrument;other.sfz=std::filesystem::u8path(lp::makeSf2Reference(argv[2],0,0));
 auto added=run({instrument,other});check(added.failed==0&&added.analyzed==1&&added.reused==1);
 std::cout<<"PASS: real FluidSynth offline SF2 calibration; initial difference="<<before<<" dB, compensated difference="<<after<<" dB, host playback difference="<<actualDifference<<" dB; independent presets, manual volume, restart reuse and incremental addition\n";
}

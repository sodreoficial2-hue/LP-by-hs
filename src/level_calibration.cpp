#include "level_calibration.hpp"
#include "sfizz_engine.hpp"
#include "sf2_engine.hpp"
#include <set>

namespace lp {
struct LevelCalibrator::Impl {
 std::filesystem::path library,sf2Path;
 std::unique_ptr<SfizzEngine> sfz;
 std::unique_ptr<Sf2Engine> sf2;
 explicit Impl(std::filesystem::path path):library(std::move(path)){}
};
LevelCalibrator::LevelCalibrator(const std::filesystem::path& library):impl(new Impl(library)){}
LevelCalibrator::~LevelCalibrator()=default;
LevelResult LevelCalibrator::measure(const Instrument& instrument,const std::atomic<bool>& cancel){
 auto& x=*impl;auto reference=instrument.sfz.u8string();auto sf2=parseSf2Reference(reference);
 if(sf2){
  x.sfz.reset();
  if(!x.sf2||x.sf2Path!=sf2->path){
   x.sf2.reset();x.sf2Path.clear();x.sf2=std::make_unique<Sf2Engine>(x.library.parent_path()/"libfluidsynth-3.dll",48000);
   x.sf2->load(sf2->path);x.sf2Path=sf2->path;
  }
  x.sf2->panic();x.sf2->select(sf2->bank,sf2->program);x.sf2->parameters(Patch{}.parameters);
 }else{
  x.sf2.reset();x.sf2Path.clear();
  if(!x.sfz)x.sfz=std::make_unique<SfizzEngine>(x.library,48000,256,true);
  x.sfz->load(instrument.sfz);x.sfz->parameters(Patch{}.parameters);
 }
 auto silence=[&]{if(sf2)x.sf2->panic();else x.sfz->panic();};
 auto on=[&](int n,int v){if(sf2)x.sf2->noteOn(n,v);else x.sfz->noteOn(n,v);};
 auto off=[&](int n){if(sf2)x.sf2->noteOff(n);else x.sfz->noteOff(n);};
 LevelMeter meter;std::array<float,256> left{},right{};
 auto render=[&](int frames,bool level){
  while(frames>0){
   if(cancel.load())throw std::runtime_error("Calibracao cancelada");
   int count=std::min(frames,256);
   if(sf2)x.sf2->render(left.data(),right.data(),count);else x.sfz->render(left.data(),right.data(),count);
   meter.add(left.data(),right.data(),count,level);frames-=count;
  }
 };
 bool kit=instrument.mode=="kit"||instrument.category==8||(sf2&&sf2->bank==128);
 bool bass=instrument.category==11;
 std::set<int> notes=kit?std::set<int>{36,38,42,46,49,51,60,64}:bass?std::set<int>{28,36,43,48,55}:std::set<int>{48,55,60,67,72};
 notes.insert(std::clamp(instrument.testNote,0,127));
 for(int velocity:{88,120})for(int note:notes){
  silence();meter.segment();on(note,velocity);render(kit?24000:72000,true);off(note);render(12000,true);
 }
 // Harder notes/chords constrain the gain, but do not set the average level.
 for(int root:kit?std::vector<int>{36,38}:bass?std::vector<int>{36,48}:std::vector<int>{48,60}){
  silence();meter.segment();on(root,127);
  if(!kit){on(root+7,127);on(root+12,127);}
  render(48000,false);silence();
 }
 return meter.finish();
}
LevelCalibrationJob::LevelCalibrationJob(const std::filesystem::path& library,const std::filesystem::path& cache,std::vector<Instrument> instruments){
 progress.total=instruments.size();worker=std::thread(&LevelCalibrationJob::run,this,library,cache,std::move(instruments));
}
LevelCalibrationJob::~LevelCalibrationJob(){cancel.store(true);if(worker.joinable())worker.join();}
LevelProgress LevelCalibrationJob::snapshot()const{std::lock_guard<std::mutex> lock(mutex);return progress;}
void LevelCalibrationJob::run(std::filesystem::path library,std::filesystem::path file,std::vector<Instrument> instruments){
 try{
  LevelCache cache;
  try{recoverFile(file,[&](const std::filesystem::path& source){cache.load(source);});}catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);progress.error=e.what();cache=LevelCache{};}
  LevelCalibrator calibrator(library);
  for(const auto& instrument:instruments){
   if(cancel.load())break;
   {std::lock_guard<std::mutex> lock(mutex);progress.name=instrumentDisplayName(instrument);}
   try{
    auto reference=instrument.sfz.u8string(),key=levelKey(reference),stamp=levelStamp(reference);LevelResult result;
    bool reused=cache.find(key,stamp,result);
    if(!reused){
     result=calibrator.measure(instrument,cancel);
     if(cancel.load())break;
     if(levelStamp(reference)!=stamp)throw std::runtime_error("Timbre alterado durante a calibracao; tente novamente");
     cache.save(file,key,stamp,result); // Flush/atomic replace before announcing success.
    }
    std::lock_guard<std::mutex> lock(mutex);progress.gains[key]=result.gain;
    if(reused)++progress.reused;else ++progress.analyzed;
    if(!result.audible)++progress.silent;
   }catch(const std::exception& e){
    if(cancel.load())break;
    std::lock_guard<std::mutex> lock(mutex);++progress.failed;progress.error=e.what();
   }
   std::lock_guard<std::mutex> lock(mutex);++progress.completed;
  }
 }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);progress.error=e.what();++progress.failed;}
 std::lock_guard<std::mutex> lock(mutex);progress.cancelled=cancel.load();progress.done=true;
}
}

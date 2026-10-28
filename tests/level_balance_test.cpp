#include "level_balance.hpp"
#include <iostream>
#include <limits>
#include <chrono>
#define check(v) do{if(!(v))throw std::runtime_error("Level regression line "+std::to_string(__LINE__));}while(false)
lp::LevelResult tone(float amplitude){
 lp::LevelMeter meter;std::array<float,256> left{},right{};
 for(int pos=0;pos<48000;pos+=256){int size=std::min(256,48000-pos);for(int i=0;i<size;++i)left[i]=right[i]=amplitude*std::sin(6.28318530718f*440*(pos+i)/48000);meter.add(left.data(),right.data(),size);}
 return meter.finish();
}
int main(){
 auto quiet=tone(.04f),loud=tone(.2f);check(quiet.audible&&loud.audible);
 check(std::abs(20*std::log10(quiet.rms*quiet.gain/(loud.rms*loud.gain)))<.01f);
 check(quiet.gain>1&&loud.gain<1);
 check(tone(.00001f).gain<4); // A nearly silent preset cannot receive unbounded gain.
 lp::LevelMeter silence;std::array<float,256> zeros{};silence.add(zeros.data(),zeros.data(),256);check(!silence.finish().audible&&silence.finish().gain==1);
 lp::LevelMeter spike;std::array<float,256> pulse{};pulse[0]=8;spike.add(pulse.data(),zeros.data(),256,false);check(!spike.finish().audible);
 lp::LevelMeter bounded;std::array<float,256> samples{};
 for(int pos=0;pos<48000;pos+=256){for(int i=0;i<256;++i)samples[i]=.01f*std::sin(6.28318530718f*440*(pos+i)/48000);bounded.add(samples.data(),samples.data(),256);}
 bounded.add(pulse.data(),zeros.data(),256,false);auto constrained=bounded.finish();check(constrained.gain*8<=.354814f);
 samples[0]=std::numeric_limits<float>::quiet_NaN();bool invalid=false;try{bounded.add(samples.data(),zeros.data(),1);}catch(...){invalid=true;}check(invalid);
 auto dir=std::filesystem::temp_directory_path()/("lp-level-cache-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
 auto file=dir/"levels.cache",sfz=dir/"Piano com espacos.sfz";std::ofstream(sfz)<<"test";
 auto key=lp::levelKey(sfz.u8string()),stamp=lp::levelStamp(sfz.u8string());
 lp::LevelCache cache;cache.load(file);lp::LevelResult result;check(!cache.find(key,stamp,result));cache.save(file,key,stamp,quiet);
 {lp::LevelCache reopened;reopened.load(file);check(reopened.find(key,stamp,result)&&result.gain==quiet.gain);check(!reopened.find(key,stamp+"changed",result));}
 auto preset0=lp::levelKey(lp::makeSf2Reference(sfz,0,0)),preset1=lp::levelKey(lp::makeSf2Reference(sfz,0,1));check(preset0!=preset1);
 cache.save(file,preset0,stamp,loud);cache.save(file,preset1,stamp,quiet);
 lp::LevelCache reopened;reopened.load(file);check(reopened.find(preset0,stamp,result)&&result.gain==loud.gain);check(reopened.find(preset1,stamp,result)&&result.gain==quiet.gain);
 check(lp::levelKey((dir/".."/dir.filename()/sfz.filename()).u8string())==key);
 // Interrupted/failed writes never replace a previously saved entry.
 bool failed=false;try{cache.save(dir/"missing"/"levels",key,stamp,loud);}catch(...){failed=true;}check(failed);cache.load(file);check(cache.find(key,stamp,result)&&result.gain==quiet.gain);
 std::ofstream(file)<<"LPHS_LEVEL_BALANCE 1\n\"key\" \"stamp\" nan 0 0 1\n";
 failed=false;try{cache.load(file);}catch(...){failed=true;}check(failed&&cache.find(key,stamp,result));
 std::filesystem::remove_all(dir);
 std::cout<<"PASS: approximate level matching, bounded static gain, peak headroom, silence/nonfinite safety, persistent cache, per-preset identity and failed-write preservation\n";
}

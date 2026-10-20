#include "sfizz_engine.hpp"
#include "instrument_catalog.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void word(std::ofstream& out,unsigned v,int bytes){for(int i=0;i<bytes;++i)out.put(static_cast<char>((v>>(i*8))&255));}
std::vector<float> render(lp::SfizzEngine& engine,int frames){std::vector<float> left(frames),right(frames);for(int i=0;i<frames;i+=256)engine.render(left.data()+i,right.data()+i,std::min(256,frames-i));return left;}
double rms(const std::vector<float>& v,int begin=0){double sum=0;for(std::size_t i=begin;i<v.size();++i){check(std::isfinite(v[i]),"Non-finite audio");sum+=v[i]*v[i];}return std::sqrt(sum/(v.size()-begin));}
double frequency(const std::vector<float>& v,int begin,int end){double first=-1,last=-1;int count=0;for(int i=std::max(1,begin);i<end;++i)if(v[i-1]<=0&&v[i]>0){double crossing=i-1-v[i-1]/(v[i]-v[i-1]);if(first<0)first=crossing;last=crossing;++count;}check(count>3,"Not enough waveform cycles");return (count-1)*48000/(last-first);}
std::pair<double,double> variation(const std::vector<float>& v){double lo=1e9,hi=0;for(int i=4800;i+2400<=static_cast<int>(v.size());i+=2400){double f=frequency(v,i,i+2400);lo=std::min(lo,f);hi=std::max(hi,f);}return {lo,hi};}
int main(int argc,char** argv){try{
 check(argc>=2,"Expected real sfizz library");auto dir=std::filesystem::temp_directory_path()/"lp-real-performance";std::filesystem::create_directories(dir);
 {std::ofstream f(dir/"tone.wav",std::ios::binary);f.write("RIFF",4);word(f,36+96000,4);f.write("WAVEfmt ",8);word(f,16,4);word(f,1,2);word(f,1,2);word(f,48000,4);word(f,96000,4);word(f,2,2);word(f,16,2);f.write("data",4);word(f,96000,4);for(int i=0;i<48000;++i)word(f,static_cast<unsigned short>(static_cast<short>(12000*std::sin(6.283185307179586*440*i/48000))),2);}
 {std::ofstream f(dir/"performance.sfz");f<<"<global> bend_up=200 bend_down=-200 ampeg_attack=0.001 ampeg_release=0.03 amp_veltrack=0 pitchlfo_freq=5 pitchlfo_depth_oncc1=100\n<region> sample=tone.wav pitch_keycenter=69 loop_mode=loop_continuous loop_start=0 loop_end=47999\n";}
 lp::SfizzEngine engine(argv[1],48000,256,true);engine.load(dir/"performance.sfz");engine.noteOn(69,100);auto baseline=render(engine,24000);double center=frequency(baseline,4800,24000);check(std::abs(center-440)<.5,"Center pitch incorrect");
 engine.pitchBend(8191);auto high=render(engine,24000);double up=frequency(high,4800,24000);check(std::abs(up-493.883)<1,"Positive pitch bend incorrect");
 engine.pitchBend(-8192);auto low=render(engine,24000);double down=frequency(low,4800,24000);check(std::abs(down-391.995)<1,"Negative pitch bend incorrect");
 engine.pitchBend(0);auto returned=render(engine,24000);check(std::abs(frequency(returned,4800,24000)-440)<.5,"Wheel center must restore tuning");
 for(int invalid:{-8193,8192}){bool rejected=false;try{engine.pitchBend(invalid);}catch(const std::exception&){rejected=true;}check(rejected,"Invalid bend must be rejected");}
 engine.cc(1,127);auto moving=variation(render(engine,48000));check(moving.second-moving.first>30,"Modulation must change held-note pitch");engine.cc(1,0);auto still=variation(render(engine,48000));check(still.second-still.first<.5,"Modulation zero must stop vibrato");
 for(int value:{64,127}){engine.panic();engine.cc(64,value);engine.noteOn(69,100);render(engine,4096);engine.noteOff(69);auto held=render(engine,48000);check(rms(held,24000)>.02,"Pedal must sustain released key");engine.cc(64,0);auto released=render(engine,24000);check(rms(released,12000)<1e-6,"Pedal release must end held voice");}
 engine.cc(64,63);engine.noteOn(69,100);render(engine,4096);engine.noteOff(69);check(rms(render(engine,24000),12000)<1e-6,"CC64 value 63 must be pedal up");
 std::cout<<"PASS: REAL sfizz sustain threshold/release; pitch center="<<center<<" Hz, down="<<down<<" Hz, up="<<up<<" Hz; live modulation range="<<moving.first<<".."<<moving.second<<" Hz and neutral return\n";
 if(argc>2&&argv[2][0]){auto bank=lp::loadInstrumentBank(argv[2]);for(const auto& p:bank){
  if(p.id!="sax-soprano"&&p.id!="sax-alto"&&p.id!="drawbar"&&p.id!="synth-square-mono")continue;
  engine.load(p.sfz);engine.cc(1,0);engine.cc(64,127);engine.noteOn(p.testNote,100);render(engine,4800);engine.noteOff(p.testNote);
  double held=rms(render(engine,96000),72000);check(held>1e-5,"Installed instrument must sustain released key");engine.cc(64,0);
  double released=rms(render(engine,144000),120000);check(released<held*.01,"Installed instrument must release pedal-held key");
  std::cout<<"PASS: installed pedal "<<p.id<<" held RMS="<<held<<" released="<<released<<'\n';
 }}
 std::filesystem::remove_all(dir);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

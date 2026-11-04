#include "timbre_format.hpp"
#include "sfizz_engine.hpp"
#include "sample_builder.hpp"
#include <array>
#include <fstream>
#include <iostream>
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
std::array<std::vector<float>,2> render(lp::SfizzEngine& engine,int frames){
 std::array<std::vector<float>,2> pcm{std::vector<float>(frames),std::vector<float>(frames)};
 for(int at=0;at<frames;at+=256)engine.render(pcm[0].data()+at,pcm[1].data()+at,std::min(256,frames-at));return pcm;
}
double energy(const std::array<std::vector<float>,2>& audio){double sum=0;for(const auto& c:audio)for(float x:c){check(std::isfinite(x),"non-finite engine output");sum+=double(x)*x;}return sum;}
}
int main(int argc,char** argv){try{
 check(argc==5,"arguments: sfizz sndfile fixtures output");auto root=std::filesystem::absolute(std::filesystem::u8path(argv[4]));std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 lp::SfizzEngine engine(std::filesystem::u8path(argv[1]),48000,256,true);
 for(const char* name:{"tine-demo.npno","layers-v5.npno","layers-v6.npno"}){
  lp::MetaSidecar meta;auto sfz=lp::npnoParaSFZ(std::filesystem::u8path(argv[3])/name,root/name,meta,std::filesystem::u8path(argv[2]));engine.load(sfz);check(engine.regions()==int(meta.zones.size()),"real sfizz region count differs from audited output");
  for(int pedal:{0,127})for(int velocity:{1,30,80,127}){
   engine.panic();engine.cc(64,pedal);engine.noteOn(48,velocity);auto audio=render(engine,8192);check(energy(audio)>1e-15,"real sfizz emitted no audio for a velocity/pedal combination");engine.noteOff(48);check(energy(render(engine,2048))>1e-15,"release path emitted no audio");
  }
  engine.panic();engine.cc(64,0);engine.noteOn(52,100);auto audio=render(engine,48000);engine.noteOff(52);
  // Audible preview of the public synthesized demo, not an original-instrument comparison.
  if(std::string(name)=="tine-demo.npno"){
   std::string fmt;lp::sampling::u16(fmt,1);lp::sampling::u16(fmt,2);lp::sampling::u32(fmt,48000);lp::sampling::u32(fmt,192000);lp::sampling::u16(fmt,4);lp::sampling::u16(fmt,16);std::string pcm;
   for(std::size_t i=0;i<audio[0].size();++i)for(int c=0;c<2;++c)lp::sampling::u16(pcm,static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(std::clamp(audio[c][i],-1.f,1.f)*32767))));
   lp::atomicWrite(root/"preview.wav",lp::sampling::chunk("RIFF","WAVE"+lp::sampling::chunk("fmt ",fmt)+lp::sampling::chunk("data",pcm)));
  }
 }
 std::cout<<"PASS: real sfizz loaded every audited region and rendered all three fixtures at velocities 1/30/80/127, CC64 off/on and note release. No original keyboard recording was supplied for listening comparison.\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

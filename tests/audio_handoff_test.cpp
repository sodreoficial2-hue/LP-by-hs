#include "audio_host.hpp"
#include <thread>
#include <atomic>
#include <fstream>
#include <cassert>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);int fakeStarts();
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-handoff-test";std::filesystem::create_directories(root);auto file=root/"sound.sfz";{std::ofstream out(file);out<<"fixture";}
 lp::AudioHost host(argv[1]);host.start(1,128);auto p=lp::Patch{}.parameters;host.load(file,p);host.send({0x90,60,100});float left[256]{},right[256]{};fakePump(left,right,256);assert(left[0]==.25f);
 auto starts=fakeStarts();host.load(file,p);fakePump(left,right,256);assert(left[0]>.24f); // old voice survives even without a new note-on
 host.send({0x90,64,100});fakePump(left,right,256);assert(left[0]>.49f); // both generations audible
 host.send({0x80,60,0});host.send({0x80,64,0});fakePump(left,right,256);assert(left[0]==0); // releases reach both generations
 host.send({0xb0,64,127});host.send({0x90,60,100});fakePump(left,right,256);host.load(file,p);host.send({0x80,60,0});fakePump(left,right,256);assert(left[0]>0);host.send({0xb0,64,0});fakePump(left,right,256);assert(left[0]==0);
 lp::Performance layer;layer.name="Split";layer.mode=lp::PerformanceMode::Split;layer.parts[0].enabled=true;layer.parts[0].instrument=file.u8string();layer.parts[0].low=0;layer.parts[0].high=63;layer.parts[0].transpose=12;
 host.loadPerformance(layer);host.send({0x90,40,100});fakePump(left,right,256);assert(left[0]>0);host.load(file,p);host.send({0x80,40,0});fakePump(left,right,256);assert(left[0]==0); // retiring split keeps original transposition
 std::atomic<bool> running{true};std::atomic<int> blocks{0};
 std::thread callback([&]{float l[128],r[128];while(running.load()){fakePump(l,r,128);for(float value:l)assert(std::isfinite(value));++blocks;}});
 for(int i=0;i<40;++i){p[0]=.25f+(i%4)*.1f;host.load(file,p);host.send({0x90,60,100});host.send({0x80,60,0});host.collectRetired();}
 running=false;callback.join();assert(blocks>0&&fakeStarts()==starts);host.panic();fakePump(left,right,256);assert(left[0]==0&&!host.takeFault());host.collectRetired();host.stop();std::filesystem::remove_all(root);
}

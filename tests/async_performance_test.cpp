#include "audio_host.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <fstream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);int fakeStarts();
bool wait(lp::AudioHost& host){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(std::chrono::steady_clock::now()<deadline){if(host.pollPerformance())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;}
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-async-performance-test";std::filesystem::create_directories(root);for(auto name:{"slow.sfz","fast.sfz","broken.sfz"})std::ofstream(root/name)<<"fixture";
 lp::AudioHost host(argv[1]);host.start(1,128);auto starts=fakeStarts();float left[256]{},right[256]{};
 lp::Performance p;p.name="Layers";for(auto& part:p.parts){part.enabled=true;part.instrument=(root/"fast.sfz").u8string();}
 host.requestPerformance(p);assert(host.performanceLoading());assert(wait(host));host.sendRealtime({0x90,60,100});fakePump(left,right,256);assert(left[0]==1.f);
 host.setPerformanceAuditionMask(1);fakePump(left,right,256);assert(left[0]==.25f);host.setPerformanceAuditionMask(15);fakePump(left,right,256);assert(left[0]==.25f);host.sendRealtime({0x90,64,100});fakePump(left,right,256);assert(left[0]==1.f);
 // Queue an expensive old choice, replace it repeatedly, and publish only newest.
 auto slow=p;slow.parts[0].instrument=(root/"slow.sfz").u8string();host.requestPerformance(slow);
 for(int n=0;n<8;++n){auto latest=p;latest.parts[0].patch.parameters[0]=.2f;host.requestPerformance(latest);}assert(wait(host));host.sendRealtime({0x90,60,100});fakePump(left,right,256);assert(std::abs(left[0]-.8f)<1e-6);
 // Parameters changed during preparation must win over the job snapshot.
 host.requestPerformance(slow);auto params=lp::Patch{}.parameters;params[0]=.7f;host.setPerformancePartParameters(0,params,false);assert(wait(host));host.sendRealtime({0x80,60,0});host.sendRealtime({0x90,60,100});fakePump(left,right,256);assert(std::abs(left[0]-.925f)<1e-6);
 // A later single-sound selection cancels outstanding layer jobs.
 host.requestPerformance(slow);host.load(root/"fast.sfz",lp::Patch{}.parameters);std::this_thread::sleep_for(std::chrono::milliseconds(220));assert(!host.pollPerformance()&&!host.performanceActive());host.sendRealtime({0x80,60,0});host.sendRealtime({0x90,60,100});fakePump(left,right,256);assert(left[0]==.25f);
 auto bad=p;bad.parts[0].instrument=(root/"broken.sfz").u8string();host.requestPerformance(bad);bool failed=false;try{wait(host);}catch(...){failed=true;}assert(failed&&!host.performanceLoading());host.unload();fakePump(left,right,256);assert(left[0]==0&&fakeStarts()==starts&&!host.takeFault());
 std::filesystem::remove_all(root);
}

#include "audio_host.hpp"
#include <cassert>
#include <thread>
#include <chrono>
#include <fstream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);int fakeStarts();
int main(int argc,char** argv){assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-stability-stress";std::filesystem::create_directories(root);for(int i=0;i<5;++i)std::ofstream(root/(std::to_string(i)+".sfz"))<<"fixture";
 lp::AudioHost host(argv[1]);host.enableSmoothTransitions(true);host.start(1,128);auto starts=fakeStarts();std::atomic<bool> run{true};std::atomic<int> blocks{0};
 std::thread audio([&]{float l[128],r[128];while(run){fakePump(l,r,128);for(float x:l)assert(std::isfinite(x));++blocks;}});
 lp::Performance p;p.name="Stress four layers";for(int i=0;i<4;++i){p.parts[i].enabled=true;p.parts[i].instrument=(root/(std::to_string(i)+".sfz")).u8string();}
 for(int n=0;n<300;++n){p.parts[n%4].instrument=(root/(std::to_string(n%5)+".sfz")).u8string();host.requestPerformance(p);host.sendRealtime({0xb0,64,static_cast<std::uint8_t>((n%2)*127)});host.sendRealtime({0x90,static_cast<std::uint8_t>(48+n%24),100});host.sendRealtime({0x80,static_cast<std::uint8_t>(48+n%24),0});host.setPerformanceAuditionMask(n%3?15:1);host.pollPerformance();host.collectRetired();}
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(host.performanceLoading()&&std::chrono::steady_clock::now()<deadline){host.pollPerformance();std::this_thread::sleep_for(std::chrono::milliseconds(1));}assert(!host.performanceLoading());host.panic();run=false;audio.join();assert(blocks>0&&fakeStarts()==starts&&!host.takeFault());host.stop();std::filesystem::remove_all(root);
}

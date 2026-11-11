#include "audio_host.hpp"
#include <cassert>
#include <fstream>
#include <thread>
#include <iostream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);
void ready(lp::AudioHost& host){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!host.pollPerformance()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-live-recall-gain";std::filesystem::create_directories(root);
 auto a=root/"piano.sfz",b=root/"organ.sfz";std::ofstream(a)<<"fixture";std::ofstream(b)<<"fixture";
 lp::AudioHost h(argv[1]);h.enableSmoothTransitions(true);h.start(1,128);float l[256]{},r[256]{};
 auto p=lp::Patch{}.parameters;p[0]=.2f;
 h.requestSound(a.u8string(),p);ready(h);h.sendRealtime({0x90,60,100});for(int i=0;i<8;++i)fakePump(l,r,256);
 assert(std::abs(l[255]-.05f)<1e-5f);
 for(int turn=0;turn<8;++turn){
  // The old held note deliberately remains sounding, reproducing overlap in
  // favorites and ensuring this is a test of mixing rather than load timing.
  p[0]=turn%2?.2f:.8f;h.requestSound((turn%2?a:b).u8string(),p);ready(h);h.sendRealtime({0x90,64,100});
  float expected=.25f*p[0];
  for(int block=0;block<400;++block){fakePump(l,r,256);if(block>=6)for(float value:l)assert(std::abs(value-expected)<1e-5f);}
 }
 // Once the new attack has entered, its pauses must not resurrect a held tail.
 p[0]=.8f;h.requestSound(b.u8string(),p);ready(h);h.sendRealtime({0x90,67,100});for(int i=0;i<8;++i)fakePump(l,r,256);
 h.sendRealtime({0x80,67,0});for(int i=0;i<50;++i){fakePump(l,r,256);for(float value:l)assert(value==0);}
 assert(!h.takeFault());h.stop();std::filesystem::remove_all(root);
 std::cout<<"PASS: recalled gain stable by 35 ms and throughout 2-second tails, alternating sounds and no tail resurrection\n";
}

#include "audio_host.hpp"
#include <cassert>
#include <fstream>
#include <thread>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);int fakeStarts();
namespace lp {
// Exercise the warm SF2 cache lifecycle with the deterministic SFZ renderer.
// The graph handoff is shared by both real engines; no file-size policy changes.
struct AudioHostGraphProbe {
 static void remember(AudioHost& host){for(int i=0;i<4;++i)if(host.preparedParts[i])host.warmParts.emplace_back(i,host.preparedParts[i]);}
};
}
void ready(lp::AudioHost& host){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!host.pollPerformance()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-cached-recall";std::filesystem::create_directories(root);for(auto name:{"a.sfz","b.sfz"})std::ofstream(root/name)<<"fixture";
 lp::AudioHost host(argv[1]);host.start(1,128);auto starts=fakeStarts();float l[256]{},r[256]{};
 lp::Performance a;a.name="A";for(auto& p:a.parts){p.enabled=true;p.instrument=(root/"a.sfz").u8string();}
 host.requestPerformance(a);ready(host);host.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[0]==1);
 lp::AudioHostGraphProbe::remember(host);
 host.requestSound((root/"b.sfz").u8string(),lp::Patch{}.parameters);ready(host);fakePump(l,r,256);assert(l[0]>0); // Four-layer outgoing tail, no abrupt discard.
 // Immediate return while the outgoing graph is still rendering: it must not
 // become audible twice, and its old notes must not return as active notes.
 host.requestPerformance(a);ready(host);fakePump(l,r,256);assert(l[0]==0);
 host.sendRealtime({0x90,64,100});fakePump(l,r,256);assert(l[0]==1);
 host.requestSound((root/"b.sfz").u8string(),lp::Patch{}.parameters);ready(host);fakePump(l,r,256);assert(l[0]>0);
 float previous=l[0];for(int i=0;i<400;++i){fakePump(l,r,256);assert(l[0]<=previous+.000001f);previous=l[0];}assert(l[255]==0);
 // Return after the old tail has expired: frozen notes and FX are cleared.
 host.requestPerformance(a);ready(host);fakePump(l,r,256);assert(l[0]==0);
 host.sendRealtime({0x90,67,100});fakePump(l,r,256);assert(l[0]==1);host.sendRealtime({0x80,67,0});fakePump(l,r,256);assert(l[0]==0);
 assert(fakeStarts()==starts&&!host.takeFault());host.stop();std::filesystem::remove_all(root);
}

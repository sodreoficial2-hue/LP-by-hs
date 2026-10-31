#include "audio_host.hpp"
#include <cassert>
#include <fstream>
#include <thread>
#include <iostream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);int fakeStarts();
namespace lp {struct AudioHostGraphProbe {
 static std::string activeName(const AudioHost& host){return host.active?host.active->performance.name:"";}
 static bool waiting(const AudioHost& host){return host.waiting!=nullptr;}
 static void remember(AudioHost& host){for(int i=0;i<4;++i)if(host.preparedParts[i])host.warmParts.emplace_back(i,host.preparedParts[i]);}
};}
void ready(lp::AudioHost& h){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!h.pollPerformance()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-transition-budget";std::filesystem::create_directories(root);
 lp::AudioHost h(argv[1]);h.enableSmoothTransitions(true);h.start(1,128);const auto starts=fakeStarts();float l[256]{},r[256]{};
 auto perf=[&](const char* name){std::ofstream(root/name)<<"fixture";lp::Performance p;p.name=name;for(auto& part:p.parts){part.enabled=true;part.instrument=(root/name).u8string();}return p;};
 auto a=perf("a.sfz"),b=perf("b.sfz"),c=perf("c.sfz"),d=perf("d.sfz");
 h.requestPerformance(a);ready(h);h.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[255]==1);lp::AudioHostGraphProbe::remember(h);
 h.requestPerformance(b);ready(h);h.sendRealtime({0x90,64,100});fakePump(l,r,256);assert(h.synthCount()==8);for(auto value:l)assert(value<=1.00001f);assert(!h.takeOutputClip()||l[255]<=1.00001f);
 auto ticket=h.beginMidiSwitch();h.requestPerformance(c);ready(h);h.sendRealtime({0x90,67,100});h.acknowledgeMidiSwitch(ticket);
 float previous=l[255];int blocks=0;
 do{fakePump(l,r,256);for(float value:l){assert(std::isfinite(value)&&value<=1.00001f);assert(std::abs(value-previous)<.01f);previous=value;}assert(h.synthCount()<=8);++blocks;}while(lp::AudioHostGraphProbe::activeName(h)!="c.sfz"&&blocks<20);
 assert(blocks>1&&blocks<10);assert(lp::AudioHostGraphProbe::activeName(h)=="c.sfz"); // MIDI note waited for the actual new graph.
 // Newest pending selection supersedes a graph waiting for a fade.
 h.requestPerformance(d);ready(h);fakePump(l,r,256);assert(lp::AudioHostGraphProbe::waiting(h));
 h.requestPerformance(a);ready(h);for(int i=0;i<12;++i)fakePump(l,r,256);assert(lp::AudioHostGraphProbe::activeName(h)=="a.sfz");
 // All old tails expire, and cached notes do not return on the recalled graph.
 for(int i=0;i<400;++i)fakePump(l,r,256);assert(l[255]==0&&h.synthCount()==4);
 h.sendRealtime({0x90,72,100});fakePump(l,r,256);assert(l[255]==1);assert(fakeStarts()==starts&&!h.takeFault());
 h.panic();fakePump(l,r,256);assert(l[255]==0);h.unload();fakePump(l,r,256);assert(h.synthCount()==0);
 // Immediate cached A -> B -> A recall: let the shared tail finish its short
 // fade before panic/reset, so the old note is neither cut nor resurrected.
 h.requestPerformance(a);ready(h);h.sendRealtime({0x90,60,100});fakePump(l,r,256);
 h.requestPerformance(b);ready(h);fakePump(l,r,256);previous=l[255];assert(previous>0);
 h.requestPerformance(a);ready(h);blocks=0;
 do{fakePump(l,r,256);for(float value:l){assert(std::abs(value-previous)<.01f);previous=value;}++blocks;}while(lp::AudioHostGraphProbe::activeName(h)!="a.sfz"&&blocks<20);
 assert(blocks>1&&blocks<10);for(int i=0;i<10;++i)fakePump(l,r,256);assert(l[255]==0);
 h.unload();fakePump(l,r,256);h.stop();std::filesystem::remove_all(root);
 std::cout<<"PASS: bounded eight-engine fades, normalized overlapping peaks, queued MIDI, latest choice, cached recall, truthful engine count\n";
}

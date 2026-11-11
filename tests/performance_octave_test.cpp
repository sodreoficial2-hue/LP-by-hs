#include "audio_host.hpp"
#include "performance_workspace.hpp"
#include "performance_store.hpp"
#include <cassert>
#include <fstream>
#include <thread>
#include <iostream>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);
void ready(lp::AudioHost& h){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!h.pollPerformance()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-layer-octaves";std::filesystem::remove_all(root);std::filesystem::create_directories(root);auto file=root/"octave-probe.sfz";std::ofstream(file)<<"fixture";
 lp::Performance p;p.name="Piano original, pad agudo";for(int i=0;i<2;++i){p.parts[i].enabled=true;p.parts[i].instrument=file.u8string();}
 lp::setPerformancePartOctave(p,1,1);assert(p.parts[0].transpose==0&&p.noteFor(1,60)==72&&p.noteFor(0,60)==60);
 for(int octave=-4;octave<=4;++octave){auto copy=p;lp::setPerformancePartOctave(copy,1,octave);assert(copy.parts[1].transpose==12*octave&&copy.parts[0].transpose==0);}
 for(auto [part,octave]:{std::pair<int,int>{4,0},{1,5},{1,-5},{2,0}}){auto old=p;bool rejected=false;try{lp::setPerformancePartOctave(p,part,octave);}catch(const std::invalid_argument&){rejected=true;}assert(rejected&&p.parts[1].transpose==old.parts[1].transpose);}
 lp::PerformanceWorkspace workspace;workspace.save(root/"draft",p);lp::Performance restored;lp::PerformanceWorkspace loaded;assert(loaded.load(root/"draft",restored)&&restored.parts[1].transpose==12);
 lp::Session session;session.performanceLibrary.push_back(lp::savePerformance(p));session.favorites[1]=lp::Patch{};session.favorites[1]->sfz="performance://0";session.save(root/"show");lp::Session reopened;reopened.load(root/"show");assert(reopened.performanceLibrary[0].parts[1].transpose==12&&reopened.favorites[1]->sfz=="performance://0");
 lp::PerformanceStore store;store.load(root/"library");store.commit(session);lp::PerformanceStore disk;disk.load(root/"library");lp::Session fresh;disk.mergeInto(fresh);assert(fresh.performanceLibrary[0].parts[1].transpose==12);
 auto copy=p;lp::copyPerformancePart(copy,1,2);assert(copy.parts[2].transpose==12);lp::swapPerformanceParts(copy,0,1);assert(copy.parts[0].transpose==12&&copy.parts[1].transpose==0);
 lp::AudioHost h(argv[1]);h.start(1,128);float l[256],r[256];h.loadPerformance(p);h.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[255]==(60+72)/512.f);h.sendRealtime({0x80,60,0});fakePump(l,r,256);assert(l[255]==0);
 // Routing uses the input key range before adding the octave.
 p.mode=lp::PerformanceMode::Split;p.parts[0].enabled=false;p.parts[1].low=60;p.parts[1].high=64;
 h.loadPerformance(p);fakePump(l,r,256);h.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[255]==72/512.f);h.sendRealtime({0x80,60,0});fakePump(l,r,256);assert(l[255]==0);
 h.sendRealtime({0x90,72,100});fakePump(l,r,256);assert(l[255]==0);
 // Changing a held layer's octave cleans the old routed voice. Note-offs and
 // pedal-up for the old mapping cannot leave a stuck note in the recalled part.
 h.sendRealtime({0xb0,64,127});h.sendRealtime({0x90,60,100});fakePump(l,r,256);lp::setPerformancePartOctave(p,1,-1);h.requestPerformance(p);ready(h);fakePump(l,r,256);assert(l[255]==0);h.sendRealtime({0x80,60,0});h.sendRealtime({0xb0,64,0});fakePump(l,r,256);assert(l[255]==0);
 h.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[255]==48/512.f);h.sendRealtime({0x80,60,0});fakePump(l,r,256);assert(l[255]==0);
 p.mode=lp::PerformanceMode::Layer;lp::setPerformancePartOctave(p,1,4);h.loadPerformance(p);fakePump(l,r,256);h.sendRealtime({0x90,127,100});fakePump(l,r,256);assert(l[255]==0);lp::setPerformancePartOctave(p,1,-4);h.loadPerformance(p);fakePump(l,r,256);h.sendRealtime({0x90,0,100});fakePump(l,r,256);assert(l[255]==0);
 assert(!h.takeFault());h.stop();std::filesystem::remove_all(root);std::cout<<"PASS: independent octaves, MIDI routing and bounds, split ranges, held-note cleanup, profile/library/draft persistence and copy/swap\n";
}

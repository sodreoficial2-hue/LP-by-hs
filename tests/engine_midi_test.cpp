#include "sfizz_engine.hpp"
#include "midi.hpp"
#include <thread>
#include <iostream>
void check(bool v) {if(!v)throw std::runtime_error("Engine/MIDI regression failed");}
int main(int argc,char** argv) {
 check(argc==2);
 auto dir=std::filesystem::temp_directory_path()/"lphs-engine-test";
 std::filesystem::create_directories(dir);
 for(auto name:{"good.sfz","broken.sfz","empty.sfz"}) {std::ofstream f(dir/name); f<<"fixture";}
 {
  lp::SfizzEngine engine(argv[1]); float l[256]{},r[256]{};
  engine.render(l,r,256); check(l[0]==0 && !engine.loaded());
  engine.load(dir/"good.sfz");check(engine.loaded() && engine.regions()==3);
  engine.noteOn(60,100);engine.render(l,r,256);check(l[0]==0.25f && r[255]==0.25f);
  for(auto bad:{"missing.sfz","broken.sfz","empty.sfz"}) {
   bool rejected=false;try {engine.load(dir/bad);}catch(...){rejected=true;}
   check(rejected && engine.loaded() && engine.regions()==3);
   engine.render(l,r,256);check(l[0]==0.25f);
  }
  auto parameters=lp::Patch{}.parameters;parameters[0]=.5f;engine.parameters(parameters);engine.render(l,r,256);check(l[0]==.125f);
  engine.noteOn(60,0);engine.render(l,r,256);check(l[0]==0);
  engine.noteOn(60,100);engine.panic();engine.render(l,r,256);check(l[0]==0);
  bool rejected=false;try{engine.render(l,r,257);}catch(...){rejected=true;}check(rejected);
  engine.unload();engine.render(l,r,256);check(!engine.loaded() && l[0]==0);
 }
 for(auto name:{"good.sfz","broken.sfz","empty.sfz"}) std::filesystem::remove(dir/name);
 std::filesystem::remove(dir);
 lp::Session s;s.favorites[0]=lp::Patch{};s.favorites[0]->sfz="piano.sfz";s.programChange(0);
 s.favorites[19]=lp::Patch{};lp::MidiRouter router(s);
 check(router.route({0xc0,19,0}).action==lp::MidiAction::Favorite);
 check(router.route({0xc0,20,0}).action==lp::MidiAction::Ignore);
 check(router.route({0x90,60,0}).action==lp::MidiAction::NoteOff);
 check(router.route({0x90,60,100}).action==lp::MidiAction::NoteOn);
 check(router.route({0x90,200,100}).action==lp::MidiAction::Ignore);
 router.learn(6);check(router.route({0xb0,91,64}).action==lp::MidiAction::Learned);
 check(router.route({0xb0,91,32}).action==lp::MidiAction::Parameter);
 check(std::abs(s.favorites[0]->parameters[6]-32/127.f)<0.00001f);
 router.learn(7);router.route({0xb0,91,0});check(s.favorites[0]->cc[6]==-1 && s.favorites[0]->cc[7]==91);
 // Buttons are channel-specific; consume reserved note releases and CC edges.
 router.learnFavorite(19);check(router.route({0x92,48,100}).action==lp::MidiAction::Learned);
 check(router.route({0x92,48,100}).action==lp::MidiAction::Ignore);
 check(router.route({0x82,48,64}).action==lp::MidiAction::Ignore);
 check(router.route({0x92,48,100}).action==lp::MidiAction::Favorite);
 check(router.route({0x91,48,100}).action==lp::MidiAction::NoteOn);
 router.learnFavorite(0);router.route({0xb3,22,127});
 check(router.route({0xb3,22,127}).action==lp::MidiAction::Ignore);
 check(router.route({0xb3,22,0}).action==lp::MidiAction::Ignore);
 check(router.route({0xb3,22,127}).action==lp::MidiAction::Favorite);
 router.learnFavorite(19);router.route({0xb3,22,127});check(s.favorites[0]->button.kind==0);
 router.resetButtons();check(router.route({0xb3,22,127}).index==19);
 router.learnFavorite(0);router.cancelLearn();check(router.route({0x90,65,100}).action==lp::MidiAction::NoteOn);
 lp::MidiQueue<4> small;check(small.push({0x90,1,1}));check(small.push({0x90,2,2}));
 check(small.push({0x90,3,3}));check(!small.push({0x90,4,4}));check(small.takeOverflow());check(!small.takeOverflow());
 lp::MidiQueue<> queue;
 std::thread producer([&]{for(unsigned i=0;i<100000;++i)while(!queue.push({0x90,static_cast<std::uint8_t>(i%128),0}))std::this_thread::yield();});
 for(unsigned i=0;i<100000;++i){lp::MidiMessage m;while(!queue.pop(m))std::this_thread::yield();check(m.data1==i%128);}
 producer.join();std::cout<<"PASS: sfizz ABI, safe load failure, stereo render, note-off, panic, MIDI Learn, PC, queue overflow and concurrent order\n";
}

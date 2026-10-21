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
  auto native=engine.envelope(0);auto adsr=lp::Patch{}.parameters;adsr[1]=.5f;adsr[2]=.25f;adsr[3]=.2f;adsr[4]=.5f;
  engine.parameters(adsr,true);for(int region=0;region<3;++region){auto actual=engine.envelope(region);check(std::abs(actual[0]-2)<1e-6f&&std::abs(actual[1]-.5f)<1e-6f&&std::abs(actual[2]-20)<1e-6f&&actual[3]>3);}
  engine.parameters(adsr,false);check(engine.envelope(0)==native);check(engine.envelope(1)[2]==50.f);
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
 // All 14 panel faders learn CC, apply the very first value, and persist per favorite.
 for(int parameter=0;parameter<14;++parameter){
  router.learn(parameter);check(router.learning()&&router.pendingParameter()==static_cast<std::size_t>(parameter));
  check(router.route({0x90,60,100}).action==lp::MidiAction::NoteOn);check(router.pendingParameter().has_value());
  auto cc=static_cast<std::uint8_t>(10+parameter);
  auto learned=router.route({0xb4,cc,96});check(learned.action==lp::MidiAction::Learned&&learned.index==parameter&&learned.value==cc&&!router.learning());
  check(s.favorites[0]->cc[parameter]==cc&&std::abs(s.favorites[0]->parameters[parameter]-96/127.f)<1e-6f);
  check(router.route({0xb4,cc,0}).action==lp::MidiAction::Parameter);check(s.favorites[0]->parameters[parameter]==0);
  check(router.route({0xb4,cc,127}).action==lp::MidiAction::Parameter);check(s.favorites[0]->parameters[parameter]==1);
 }
 auto profile=std::filesystem::temp_directory_path()/"lp-panel-midi-learn.lphs";s.favorites[19]->sfz="strings.sfz";s.save(profile);
 lp::Session restored;restored.load(profile);restored.programChange(0);lp::MidiRouter restoredRouter(restored);
 check(restoredRouter.route({0xb4,16,32}).action==lp::MidiAction::Parameter);check(std::abs(restored.favorites[0]->parameters[6]-32/127.f)<1e-6f);std::filesystem::remove(profile);
 router.learn(6);router.cancelLearn();auto previous=s.favorites[0]->cc[6];router.route({0xb4,90,1});check(s.favorites[0]->cc[6]==previous&&!router.learning());
 router.learn(6);s.programChange(19);router.route({0xb4,90,1});check(!router.learning()&&s.favorites[19]->cc[6]==-1);s.programChange(0);
 router.learn(6);check(router.route({0xc0,19,0}).action==lp::MidiAction::Favorite&&!router.learning());
 // Favorite Learn accepts PC only and preserves exact slot numbering.
 for(int slot=0;slot<20;++slot){s.favorites[slot]=lp::Patch{};s.favorites[slot]->sfz="piano.sfz";}
 for(int slot=0;slot<20;++slot){auto pc=static_cast<std::uint8_t>(slot);auto result=router.route({0xc0,pc,0});check(result.action==lp::MidiAction::Favorite&&result.index==slot);}
 router.learnFavorite(19);
 check(router.route({0x92,48,100}).action==lp::MidiAction::NoteOn);
 check(router.route({0x82,48,64}).action==lp::MidiAction::NoteOff);
 check(router.route({0xb3,22,127}).action==lp::MidiAction::Control);
 check(s.favorites[19]->button.kind==0);
 check(router.route({0xc2,0,0}).action==lp::MidiAction::WrongProgram);
 check(s.favorites[19]->button.kind==0);
 check(router.route({0xc2,19,0}).action==lp::MidiAction::FavoriteLearned);
 check(s.favorites[19]->button.kind==0xc0&&s.favorites[19]->button.number==19&&s.favorites[19]->button.channel==2);
 check(router.route({0xc2,19,0}).action==lp::MidiAction::Favorite);
 check(router.route({0xc2,19,0}).action==lp::MidiAction::Favorite); // Repeated PC remains valid.
 check(router.route({0xc1,19,0}).action==lp::MidiAction::Ignore);
 router.learnFavorite(0);router.cancelLearn();check(router.route({0xc3,0,0}).action==lp::MidiAction::Favorite);
 check(s.favorites[0]->button.kind==0);
 // Pedal and reset stay native during Learn; bend preserves every MIDI bit.
 router.learn(6);check(router.route({0xb0,64,127}).action==lp::MidiAction::Control&&router.learning());
 // Pitch bend is now an eligible knob source, so Learn captures it.
 auto bendLearn=router.route({0xe7,0,0});
 check(bendLearn.action==lp::MidiAction::Learned&&bendLearn.value==257&&!router.learning());
 check(s.favorites[0]->cc[6]==257&&s.favorites[0]->parameters[6]==0.f);
 check(router.route({0xe7,127,127}).action==lp::MidiAction::Parameter);
 check(std::abs(s.favorites[0]->parameters[6]-1.f)<1e-6f);
 check(router.route({0xe7,1,64}).action==lp::MidiAction::Parameter);
 router.learn(6);check(router.route({0xb0,121,0}).action==lp::MidiAction::Control&&router.learning());
 s.favorites[0]->cc[6]=64;check(router.route({0xb0,64,0}).action==lp::MidiAction::Control);
 auto mod=router.route({0xb0,1,100});check(mod.action==lp::MidiAction::Learned&&lp::forwardsToEngine(mod,{0xb0,1,100}));
 mod=router.route({0xb0,1,50});check(mod.action==lp::MidiAction::Parameter&&lp::forwardsToEngine(mod,{0xb0,1,50}));
 lp::MidiQueue<4> smallQueue;check(smallQueue.push({0x90,1,1}));check(smallQueue.push({0x90,2,2}));
 check(smallQueue.push({0x90,3,3}));check(!smallQueue.push({0x90,4,4}));check(smallQueue.takeOverflow());check(!smallQueue.takeOverflow());
 lp::MidiQueue<> queue;
 std::thread producer([&]{for(unsigned i=0;i<100000;++i)while(!queue.push({0x90,static_cast<std::uint8_t>(i%128),0}))std::this_thread::yield();});
 for(unsigned i=0;i<100000;++i){lp::MidiMessage m;while(!queue.pop(m))std::this_thread::yield();check(m.data1==i%128);}
 producer.join();std::cout<<"PASS: sfizz ABI, safe load failure, stereo render, note-off, panic, MIDI Learn, PC, queue overflow and concurrent order\n";
}

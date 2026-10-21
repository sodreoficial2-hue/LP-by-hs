#include "performance.hpp"
#include "midi.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace lp;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
 Performance p;p.name="Stage 4 voices";p.mode=PerformanceMode::Layer;
 for(int i=0;i<4;++i){p.parts[i].enabled=true;p.parts[i].instrument="voice"+std::to_string(i)+".sfz";}
 require(p.valid(),"four voices valid");
 for(int note=0;note<128;++note)for(int i=0;i<4;++i)require(p.noteFor(i,note)==note,"layer routing");
 p.mode=PerformanceMode::Split;
 // 2+2 split.
 for(int i=0;i<4;++i){p.parts[i].low=i<2?0:64;p.parts[i].high=i<2?63:127;}
 for(int note=0;note<128;++note)for(int i=0;i<4;++i)require(p.receives(i,note)==(i<2?note<64:note>=64),"2+2 split");
 // 3+1 split.
 for(int i=0;i<4;++i){p.parts[i].low=i<3?0:64;p.parts[i].high=i<3?63:127;}
 require(p.receives(2,32)&&!p.receives(2,80)&&p.receives(3,80),"3+1 split");
 // 2+1+1 split.
 for(int i=0;i<4;++i){p.parts[i].low=i<2?0:(i==2?43:86);p.parts[i].high=i<2?42:(i==2?85:127);}
 require(p.receives(0,42)&&p.receives(1,42)&&p.receives(2,43)&&p.receives(3,86),"three zones");
 // Four zones.
 for(int i=0;i<4;++i){p.parts[i].low=i*32;p.parts[i].high=i==3?127:i*32+31;}
 for(int note=0;note<128;++note){int count=0;for(int i=0;i<4;++i)count+=p.receives(i,note);require(count==1,"four zones non-overlap");}
 p.parts[0].transpose=12;require(p.noteFor(0,20)==32,"transpose");
 p.parts[0].transpose=-48;require(p.noteFor(0,20)==-1,"transpose underflow");
 p.parts[0].transpose=49;require(!p.valid(),"reject bad transpose");
 Session session;session.favorites[0]=Patch{};session.favorites[0]->sfz="piano.sfz";session.current=0;
 MidiRouter router(session);router.learn(6);
 auto learned=router.route({0xb0,91,100});
 require(learned.action==MidiAction::Learned&&session.favorites[0]->cc[6]==91,"learn knob");
 require(std::abs(session.favorites[0]->parameters[6]-100.f/127.f)<.001f,"first movement applies");
 auto changed=router.route({0xb0,91,25});
 require(changed.action==MidiAction::Parameter&&std::abs(session.favorites[0]->parameters[6]-25.f/127.f)<.001f,"mapped CC controls parameter");
 router.learn(7);require(router.route({0xb0,64,127}).action==MidiAction::Control,"sustain not learned");
 require(router.route({0xb0,92,64}).action==MidiAction::Learned&&session.favorites[0]->cc[7]==92,"learn after sustain");
 router.learn(8);router.cancelLearn();require(router.route({0xb0,93,64}).action==MidiAction::Control,"cancel learn");
 std::cout<<"PASS: 4 layer/split configurations, MIDI knob CC learn, sustain exclusion, cancel"<<std::endl;
}

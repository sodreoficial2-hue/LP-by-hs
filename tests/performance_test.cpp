#include "../src/performance.hpp"
#include <cassert>
int main(){
 lp::Performance p;p.name="Piano + Strings / Bass";p.mode=lp::PerformanceMode::Split;
 p.parts[0].enabled=true;p.parts[0].instrument="bass.sfz";p.parts[0].low=0;p.parts[0].high=59;
 p.parts[1].enabled=true;p.parts[1].instrument="piano.sfz";p.parts[1].low=60;p.parts[1].high=127;
 p.parts[2].enabled=true;p.parts[2].instrument="strings.sf2";p.parts[2].low=60;p.parts[2].high=127;
 assert(p.valid());assert(p.receives(0,48));assert(!p.receives(0,72));assert(p.receives(1,72)&&p.receives(2,72));
 p.parts[3].enabled=true;p.parts[3].instrument="pad.sfz";p.parts[3].low=60;p.parts[3].high=127;
 assert(p.receives(3,72));p.mode=lp::PerformanceMode::Layer;assert(p.receives(0,72));
 p.parts[0].transpose=12;assert(p.noteFor(0,60)==72);
 p.parts[0].low=90;p.parts[0].high=20;assert(!p.valid());
}

#include "master_midi.hpp"
#include <cassert>
#include <sstream>
int main(){
 for(int channel=0;channel<16;++channel)for(int cc=0;cc<128;++cc){
  lp::Session show;lp::Patch preview;lp::Performance p;p.name="Layer";p.parts[0].enabled=true;p.parts[0].instrument="test.sfz";
  show.favorites[0]=lp::Patch{};show.favorites[0]->sfz="test.sfz";show.favorites[0]->cc[15]=cc;preview.cc[0]=cc;p.parts[0].patch.cc[0]=cc;show.performanceLibrary.push_back(lp::savePerformance(p));
  const auto before=show.serialize();lp::MidiMessage m{static_cast<unsigned char>(0xb0|channel),static_cast<unsigned char>(cc),32};
  bool learned=lp::learnMasterControl(show,preview,p,m);assert(learned==(cc<120&&cc!=64));
  if(!learned){assert(show.serialize()==before);continue;}
  assert(show.masterCC==cc&&show.master==32/127.f&&show.favorites[0]->cc[15]==-1&&preview.cc[0]==-1&&p.parts[0].patch.cc[0]==-1&&show.performanceLibrary[0].parts[0].patch.cc[0]==-1);
  std::istringstream in(show.serialize());lp::Session restored;restored.read(in);assert(restored.masterCC==cc);m.data2=99;assert(lp::applyMasterControl(restored,m)&&restored.master==99/127.f);
  for(auto kind:{0x80,0x90,0xc0,0xe0}){auto other=m;other.status=kind|channel;assert(!lp::learnMasterControl(restored,preview,p,other));assert(!lp::applyMasterControl(restored,other));}
 }
}

#include "ui_state.hpp"
#include "mono_glide.hpp"
#include "session.hpp"
#include <cassert>
#include <vector>
#include <cmath>
int main(){
 lp::ParameterPanel panel;panel.toggle(100);assert(panel.open&&!panel.expired(30099));panel.touch(20000);assert(!panel.expired(49999)&&panel.expired(50000));panel.toggle(50000);assert(!panel.open);
 for(int mode=1;mode<=4;++mode){assert(lp::splitZone(mode,0)[1]==47);for(int note=0;note<128;++note){int count=0;for(int part=0;part<4;++part){auto z=lp::splitZone(mode,part);count+=note>=z[0]&&note<=z[1];}assert(count>=1);}}
 assert(lp::splitZone(4,1)[1]==63&&lp::splitZone(4,2)[1]==95&&lp::splitZone(3,2)[1]==85);
 lp::MonoGlide glide;glide.amount=.5f;std::vector<int> on,off;int bend=0;
 auto start=[&](int n,int){on.push_back(n);};auto stop=[&](int n){off.push_back(n);};auto wheel=[&](int b){bend=b;};
 glide.on(60,100,48000,start,stop,wheel);assert(bend==0);glide.on(72,90,48000,start,stop,wheel);assert(off.back()==60&&on.back()==72&&bend<0);int first=glide.bend();glide.advance(4800);assert(glide.bend()>first&&glide.bend()<0);glide.advance(96000);assert(glide.bend()==0);
 glide.off(72,48000,start,stop,wheel);assert(on.back()==60&&bend>0);glide.off(60,48000,start,stop,wheel);glide.reset();assert(glide.bend()==0);
 lp::Session show;show.master=.36f;show.masterCC=7;show.favorites[0]=lp::Patch{};show.favorites[0]->sfz="lead.sfz";show.favorites[0]->parameters[14]=.62f;show.favorites[0]->cc[14]=22;
 auto file=std::filesystem::temp_directory_path()/"lp-layout-profile-test.lphs";show.save(file);lp::Session loaded;loaded.load(file);assert(std::abs(loaded.master-.36f)<.0001f&&loaded.masterCC==7&&loaded.favorites[0]->cc[14]==22&&std::abs(loaded.favorites[0]->parameters[14]-.62f)<.0001f);
 // A v6 file has 14 values and CCs; Master/portamento default neutrally.
 {std::ofstream f(file);f<<"LPHS_PROFILE 6\n0 \"old.sfz\" 1 0 0 1 0 1 0 0 0 0 0 .5 .5 .5";for(int i=0;i<14;++i)f<<" -1";f<<" 0 0 0 0\n";}
 loaded.load(file);assert(loaded.master==1&&loaded.favorites[0]->parameters[14]==0&&loaded.favorites[0]->cc[14]==-1);std::filesystem::remove(file);
}

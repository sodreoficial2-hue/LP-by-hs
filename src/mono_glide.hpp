#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace lp {
// Last-note-priority monophonic glide. Fixed storage; called only by synth owner.
// Zero bypasses this helper entirely, preserving polyphonic instruments.
class MonoGlide {
 std::array<int,128> held{},velocities{};int count=0,active=-1;float pitch=0,target=0,step=0;bool seen=false;
public:
 float amount=0;int wheel=0;
 void reset(){count=0;active=-1;seen=false;step=0;wheel=0;}
 template<class On,class Off,class Bend> void on(int note,int velocity,float rate,On start,Off stop,Bend bend){
  remove(note);held[count]=note;velocities[count++]=velocity;
  select(note,velocity,rate,start,stop,bend);
 }
 template<class On,class Off,class Bend> void off(int note,float rate,On start,Off stop,Bend bend){
  remove(note);if(note!=active)return;
  if(count)select(held[count-1],velocities[count-1],rate,start,stop,bend);
  else {stop(active);active=-1;}
 }
 int bend() const {float offset=(active>=0?pitch-target:0)+wheel/8192.f*2.f;return std::clamp(static_cast<int>(offset*8192.f/96.f),-8192,8191);}
 void advance(int frames){if(step>0)pitch=std::min(target,pitch+step*frames);else if(step<0)pitch=std::max(target,pitch+step*frames);}
private:
 void remove(int note){for(int i=0;i<count;++i)if(held[i]==note){for(int j=i+1;j<count;++j){held[j-1]=held[j];velocities[j-1]=velocities[j];}--count;break;}}
 template<class On,class Off,class Bend> void select(int note,int velocity,float rate,On start,Off stop,Bend bend){
  if(active>=0)stop(active);
  active=note;target=static_cast<float>(note);
  if(!seen){pitch=target;seen=true;}
  // Full interval in 5..2000 ms, with a squared control taper.
  float seconds=.005f+amount*amount*1.995f;step=(target-pitch)/std::max(1.f,rate*seconds);
  bend(this->bend());start(note,velocity);
 }
};
}

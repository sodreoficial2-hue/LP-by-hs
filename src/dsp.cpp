#include "dsp.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace lp {
namespace {constexpr float pi=3.14159265358979323846f;
float clean(float x) noexcept {return std::isfinite(x)?x:0.f;}
}
float Dsp::Biquad::tick(float x) noexcept {
 float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;
}
void Dsp::Biquad::peak(float rate,float frequency,float db) {
 float a=std::pow(10.f,db/40),w=2*pi*std::min(frequency,rate*.4f)/rate;
 float alpha=std::sin(w)/1.4f,den=1+alpha/a;
 b0=(1+alpha*a)/den;b1=-2*std::cos(w)/den;b2=(1-alpha*a)/den;
 a1=b1;a2=(1-alpha/a)/den;
}
float Dsp::Comb::tick(float x) noexcept {
 float y=samples[cursor];samples[cursor]=x+y*.72f;
 if(++cursor==samples.size())cursor=0;
 return y;
}
Dsp::Dsp(float sampleRate):rate(sampleRate) {
 if(!std::isfinite(rate)||rate<8000||rate>384000)throw std::invalid_argument("Invalid DSP sample rate");
 constexpr float lengths[]{.0297f,.0371f,.0411f,.0437f};
 for(int c=0;c<2;++c){
  delay[c].resize(static_cast<std::size_t>(rate*.32f));
  chorus[c].resize(static_cast<std::size_t>(rate*.05f)+2);
  for(int k=0;k<4;++k)reverb[c][k].samples.resize(static_cast<std::size_t>(rate*(lengths[k]+c*.0013f)));
 }
 parameters(p);
}
void Dsp::parameters(const std::array<float,14>& values) {
 for(float v:values)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid DSP parameter");
 p=values;float hz=80*std::pow(200.f,p[5]);cutoff=1-std::exp(-2*pi*hz/rate);
 constexpr float frequencies[]{120,1000,6000};
 for(auto& channel:eq)for(int k=0;k<3;++k)channel[k].peak(rate,frequencies[k],24*(p[11+k]-.5f));
}
void Dsp::reset() noexcept {
 filter.fill(0);envelope=phase=chorusPhase=0;delayAt=chorusAt=0;
 for(auto& channel:eq)for(auto& band:channel)band.z1=band.z2=0;
 for(int c=0;c<2;++c){std::fill(delay[c].begin(),delay[c].end(),0);std::fill(chorus[c].begin(),chorus[c].end(),0);
  for(auto& comb:reverb[c]){std::fill(comb.samples.begin(),comb.samples.end(),0);comb.cursor=0;}}
}
void Dsp::process(float* left,float* right,int frames) noexcept {
 if(!left||!right||left==right||frames<=0)return;
 const float attack=std::exp(-1.f/(rate*.005f)),release=std::exp(-1.f/(rate*.10f));
 const float threshold=std::pow(10.f,(-6-24*p[10])/20),ratio=1+7*p[10];
 for(int i=0;i<frames;++i){
  float x[]{clean(left[i]),clean(right[i])};
  for(int c=0;c<2;++c){
   if(p[5]<1){filter[c]+=cutoff*(x[c]-filter[c]);x[c]=filter[c];}
   for(int k=0;k<3;++k)if(p[11+k]!=.5f)x[c]=eq[c][k].tick(x[c]);
  }
  float peak=std::max(std::abs(x[0]),std::abs(x[1]));
  float coefficient=peak>envelope?attack:release;envelope=coefficient*envelope+(1-coefficient)*peak;
  float gain=p[10]>0&&envelope>threshold?std::pow(threshold/envelope,1-1/ratio):1;
  float trem=1-p[9]*(.5f+.5f*std::sin(phase));
  for(int c=0;c<2;++c){
   float dry=x[c]*gain*trem;
   // Delay feedback remains below unity; wet controls never alter dry gain.
   float echo=delay[c][delayAt];delay[c][delayAt]=dry+echo*.35f;
   float offset=rate*(.018f+.004f*std::sin(chorusPhase+c*pi/2));
   float read=static_cast<float>(chorusAt)-offset;
   if(read<0)read+=chorus[c].size();
   auto a=static_cast<std::size_t>(read),b=(a+1)%chorus[c].size();float frac=read-a;
   float mod=chorus[c][a]*(1-frac)+chorus[c][b]*frac;chorus[c][chorusAt]=dry;
   float room=0;for(auto& comb:reverb[c])room+=comb.tick(dry*.25f);
   float out=(dry+p[7]*echo*.5f+p[8]*mod*.4f+p[6]*room*.35f)*p[0];
   (c?right:left)[i]=clean(out);
  }
  if(++delayAt==delay[0].size())delayAt=0;
  if(++chorusAt==chorus[0].size())chorusAt=0;
  phase+=2*pi*5/rate;chorusPhase+=2*pi*.8f/rate;
  if(phase>=2*pi)phase-=2*pi;
  if(chorusPhase>=2*pi)chorusPhase-=2*pi;
 }
}
}

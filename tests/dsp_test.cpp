#include "dsp.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
float rms(const std::vector<float>& x){double sum=0;for(std::size_t i=x.size()/2;i<x.size();++i)sum+=x[i]*x[i];return std::sqrt(sum/(x.size()-x.size()/2));}
float tone(float hz,int parameter,float value,float amplitude=.4f){
 lp::Dsp dsp;auto p=lp::Patch{}.parameters;p[parameter]=value;dsp.parameters(p);
 std::vector<float> l(48000),r(48000);for(int i=0;i<48000;++i)l[i]=r[i]=amplitude*std::sin(6.2831853f*hz*i/48000);
 dsp.process(l.data(),r.data(),48000);return rms(l);
}
int main(){
 lp::Dsp dsp;auto p=lp::Patch{}.parameters;std::vector<float> l(48000),r(48000);
 for(int i=0;i<48000;++i){l[i]=.25f;r[i]=-.125f;}
 dsp.process(l.data(),r.data(),48000);check(l[24000]==.25f&&r[24000]==-.125f,"Neutral DSP must preserve stereo samples");
 p[0]=.2f;dsp.parameters(p);dsp.process(l.data(),r.data(),48000);check(std::abs(l[0]-.05f)<1e-6f,"Volume gain");
 check(tone(10000,5,.1f,.0001f)<tone(10000,5,1,.0001f)*.02f,"Envelope filter closes on quiet signal");
 check(tone(10000,5,.1f,.4f)/.4f>tone(10000,5,.1f,.0001f)/.0001f*20,"Envelope filter opens with playing intensity");
 for(int k=0;k<3;++k){float hz=k==0?120:k==1?1000:6000;check(tone(hz,11+k,1)>tone(hz,11+k,.5f)*2,"EQ boost");check(tone(hz,11+k,0)<tone(hz,11+k,.5f)*.5f,"EQ cut");}
 check(tone(1000,10,1)<tone(1000,10,0)*.4f,"Compressor reduces sustained loud signal");
 check(tone(1000,9,1)<tone(1000,9,0)*.8f,"Tremolo modulation");
 for(int effect:{6,7,8}){
  dsp.reset();p=lp::Patch{}.parameters;p[effect]=1;dsp.parameters(p);l.assign(48000,0);r.assign(48000,0);l[0]=1;
  dsp.process(l.data(),r.data(),48000);double tail=0;for(int i=1;i<48000;++i)tail+=l[i]*l[i];
  check(tail>1e-5,"Time effect generates an impulse tail");
  for(float x:r)check(x==0,"No accidental cross-channel leakage");
  dsp.reset();l.assign(48000,0);dsp.process(l.data(),r.data(),48000);for(float x:l)check(x==0,"Reset clears tails");
 }
 // At the same quiet input level, the filter closes after a preceding loud phrase.
 dsp.reset();p=lp::Patch{}.parameters;p[5]=.05f;dsp.parameters(p);l.assign(72000,0);r.assign(72000,0);
 for(int i=0;i<72000;++i)l[i]=r[i]=(i<24000?.4f:.002f)*std::sin(6.2831853f*6000*i/48000);
 dsp.process(l.data(),r.data(),72000);double early=0,late=0;for(int i=25000;i<26000;++i)early+=l[i]*l[i];for(int i=65000;i<66000;++i)late+=l[i]*l[i];
 check(early>late*10,"Envelope filter closes during recovery");
 // Processing blocks must not reset phases or delay memory.
 lp::Dsp whole,split;p.fill(1);whole.parameters(p);split.parameters(p);
 std::vector<float> a(20000),b(20000),c,d;for(int i=0;i<20000;++i)a[i]=b[i]=.3f*std::sin(i*.3f);c=a;d=b;
 whole.process(a.data(),b.data(),20000);for(int i=0;i<20000;i+=37)split.process(c.data()+i,d.data()+i,std::min(37,20000-i));
 for(int i=0;i<20000;++i)check(a[i]==c[i]&&b[i]==d[i]&&std::isfinite(a[i]),"Block continuity and finite full-effects output");
 // Phaser produces frequency-dependent cancellation, with stereo modulation.
 lp::Dsp phaser;p=lp::Patch{}.parameters;p[15]=1;phaser.parameters(p);
 a.resize(48000);b.resize(48000);for(int i=0;i<48000;++i)a[i]=b[i]=.3f*std::sin(i*.13f);c=a;
 phaser.process(a.data(),b.data(),48000);double difference=0,stereo=0;for(int i=24000;i<48000;++i){difference+=std::abs(a[i]-c[i]);stereo+=std::abs(a[i]-b[i]);check(std::isfinite(a[i])&&std::abs(a[i])<1,"Phaser stays bounded");}
 check(difference>10&&stereo>1,"Phaser must change the signal and sweep in stereo");
 phaser.reset();a.assign(48000,0);b=a;phaser.process(a.data(),b.data(),48000);for(float x:a)check(x==0,"Phaser reset clears history");
 p.fill(1);
 for(float rate:{8000.f,44100.f,96000.f,384000.f}){lp::Dsp alternate(rate);alternate.parameters(p);alternate.process(a.data(),b.data(),20000);for(float x:a)check(std::isfinite(x),"Supported sample rates");}
 p[7]=std::numeric_limits<float>::quiet_NaN();bool rejected=false;try{dsp.parameters(p);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Reject non-finite parameters");
 std::cout<<"PASS: neutral stereo, volume, filter, 3-band EQ, compressor, tremolo, reverb/delay/chorus tails, reset, block continuity, sample rates\n";
}

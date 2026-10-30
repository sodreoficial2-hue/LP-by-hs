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
 float y=samples[cursor];samples[cursor]=x+y*.72f;if(std::abs(samples[cursor])<1e-20f)samples[cursor]=0;
 if(++cursor==samples.size())cursor=0;
 return y;
}
float Dsp::Hall::Diffuser::tick(float x) noexcept {
 float delayed=samples[at],y=delayed-.6f*x;
 samples[at]=x+.6f*y;if(++at==samples.size())at=0;return y;
}
void Dsp::Hall::prepare(float rate,int channel) {
 constexpr float seconds[]{.0311f,.0379f,.0433f,.0531f,.0617f,.0713f,.0797f,.0893f};
 constexpr float diffuse[]{.0047f,.0073f,.0113f,.0137f};
 for(int k=0;k<8;++k)lines[k].samples.resize(static_cast<std::size_t>(rate*(seconds[k]+channel*.0011f)));
 for(int k=0;k<4;++k)diffusion[k].samples.resize(static_cast<std::size_t>(rate*(diffuse[k]+channel*.0003f)));
 preDelay.resize(static_cast<std::size_t>(rate*.028f));
 hp=1-std::exp(-2*pi*180/rate);damping=1-std::exp(-2*pi*6000/rate);
 smooth=1-std::exp(-1/(rate*.03f));decay(rate,2.5f);
 for(auto& line:lines)line.gain=line.target;
}
void Dsp::Hall::decay(float rate,float seconds) {
 for(auto& line:lines)line.target=std::pow(10.f,-3.f*line.samples.size()/(rate*seconds));
}
void Dsp::Hall::reset() noexcept {
 inputLow=0;preAt=0;std::fill(preDelay.begin(),preDelay.end(),0);
 for(auto& line:lines){line.at=0;line.low=0;line.gain=line.target;std::fill(line.samples.begin(),line.samples.end(),0);}
 for(auto& d:diffusion){d.at=0;std::fill(d.samples.begin(),d.samples.end(),0);}
}
float Dsp::Hall::tick(float x) noexcept {
 inputLow+=hp*(x-inputLow);x-=inputLow;
 float input=preDelay[preAt];preDelay[preAt]=x;if(++preAt==preDelay.size())preAt=0;
 input=diffusion[1].tick(diffusion[0].tick(input));
 std::array<float,8> y{};float sum=0,out=0;
 constexpr float signs[]{1,-1,-1,1,-1,1,1,-1};
 for(int k=0;k<8;++k){auto& line=lines[k];float v=line.samples[line.at];
  line.low+=damping*(v-line.low);line.gain+=smooth*(line.target-line.gain);
  y[k]=line.low*line.gain;sum+=y[k];out+=v*signs[k];}
 // I - 2 vv^T with v=(1,...,1)/sqrt(8): energy-preserving mixing.
 for(int k=0;k<8;++k){auto& line=lines[k];line.samples[line.at]=y[k]-.25f*sum+input*signs[k]*.35355339f;
  if(++line.at==line.samples.size())line.at=0;}
 return diffusion[3].tick(diffusion[2].tick(out*.35355339f));
}
Dsp::Dsp(float sampleRate):rate(sampleRate) {
 if(!std::isfinite(rate)||rate<8000||rate>384000)throw std::invalid_argument("Invalid DSP sample rate");
 constexpr float lengths[]{.0297f,.0371f,.0411f,.0437f};
 for(int c=0;c<2;++c){
  hall[c].prepare(rate,c);
  delay[c].resize(static_cast<std::size_t>(rate*.32f));
  chorus[c].resize(static_cast<std::size_t>(rate*.05f)+2);
  for(int k=0;k<4;++k)reverb[c][k].samples.resize(static_cast<std::size_t>(rate*(lengths[k]+c*.0013f)));
 }
 // Precompute the resonant low-pass coefficients: no tan/pow in sample loop.
 const float maxHz=std::min(16000.f,rate*.45f),k=1/1.2f;
 for(std::size_t i=0;i<cutoffTable.size();++i){float hz=80*std::pow(maxHz/80,i/511.f);float g=std::tan(pi*hz/rate);float a1=1/(1+g*(g+k));cutoffTable[i]={a1,g*a1,g*g*a1};}
 followerAttack=std::exp(-1/(rate*.002f));followerRelease=std::exp(-1/(rate*.08f));
 attackCoeff=std::exp(-1.f/(rate*.005f));releaseCoeff=std::exp(-1.f/(rate*.10f));volumeCoeff=1-std::exp(-1.f/(rate*.003f));hallCoeff=1-std::exp(-1/(rate*.03f));
 parameters(p);
}
void Dsp::parameters(const std::array<float,15>& values) {
 for(float v:values)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid DSP parameter");
 if(p[7]>0&&values[7]==0)delayTail=static_cast<int>(rate*4);if(p[8]>0&&values[8]==0)chorusTail=static_cast<int>(rate*.06f);if(p[6]>0&&values[6]==0)reverbTail=static_cast<int>(rate*10);
 auto previous=p;p=values;if(previous[10]!=p[10]){thresholdValue=std::pow(10.f,(-6-24*p[10])/20);ratioValue=1+7*p[10];}
 float upper=std::clamp((p[6]-.7f)/.3f,0.f,1.f);
 hallTarget=upper*upper*(3-2*upper);
 if(previous[6]!=p[6])for(auto& room:hall)room.decay(rate,2.5f+6.5f*hallTarget);
 constexpr float frequencies[]{120,1000,6000};
 for(auto& channel:eq)for(int k=0;k<3;++k)if(previous[11+k]!=p[11+k])channel[k].peak(rate,frequencies[k],24*(p[11+k]-.5f));
}
void Dsp::reset() noexcept {
 delayTail=chorusTail=reverbTail=0;volumeSmoothed=p[0];
 for(auto& room:hall)room.reset();hallMix=hallTarget;
 filter.fill({});follower=0;envelope=phase=chorusPhase=0;delayAt=chorusAt=0;
 for(auto& channel:eq)for(auto& band:channel)band.z1=band.z2=0;
 for(int c=0;c<2;++c){std::fill(delay[c].begin(),delay[c].end(),0);std::fill(chorus[c].begin(),chorus[c].end(),0);
  for(auto& comb:reverb[c]){std::fill(comb.samples.begin(),comb.samples.end(),0);comb.cursor=0;}}
}
void Dsp::process(float* left,float* right,int frames) noexcept {
 if(!left||!right||left==right||frames<=0)return;
 const float attack=attackCoeff,release=releaseCoeff;
 const float threshold=thresholdValue,ratio=ratioValue;
 const float volumeSmoothing=volumeCoeff;
 const bool timeEffects=p[6]>0||p[7]>0||p[8]>0||delayTail||chorusTail||reverbTail;if(timeEffects)effectFrames+=frames;
 const float hallSmoothing=hallCoeff;
 for(int i=0;i<frames;++i){
  hallMix+=hallSmoothing*(hallTarget-hallMix);
  float x[]{clean(left[i]),clean(right[i])};
  if(p[5]<1){float detector=std::max(std::abs(x[0]),std::abs(x[1]));
  float followerCoefficient=detector>follower?followerAttack:followerRelease;
  follower=followerCoefficient*follower+(1-followerCoefficient)*detector;
  float opening=p[5]+(1-p[5])*std::clamp(follower*4,0.f,1.f);
  float tablePosition=std::clamp(opening,0.f,1.f)*511;
  auto position=static_cast<std::size_t>(tablePosition);float fraction=tablePosition-position;
  const auto& lo=cutoffTable[position];const auto& hi=cutoffTable[std::min<std::size_t>(position+1,511)];
  FilterCoefficients coeff{lo.a1+fraction*(hi.a1-lo.a1),lo.a2+fraction*(hi.a2-lo.a2),lo.a3+fraction*(hi.a3-lo.a3)};
  for(int c=0;c<2;++c){
   auto& state=filter[c];float v3=x[c]-state.integrator2;
   float v1=coeff.a1*state.integrator1+coeff.a2*v3;
   float v2=state.integrator2+coeff.a2*state.integrator1+coeff.a3*v3;
   state.integrator1=2*v1-state.integrator1;state.integrator2=2*v2-state.integrator2;
   if(p[5]<1)x[c]=v2;
  }}
  for(int c=0;c<2;++c)for(int k=0;k<3;++k)if(p[11+k]!=.5f)x[c]=eq[c][k].tick(x[c]);
  float peak=std::max(std::abs(x[0]),std::abs(x[1]));
  float coefficient=peak>envelope?attack:release;envelope=coefficient*envelope+(1-coefficient)*peak;
  float gain=p[10]>0&&envelope>threshold?std::pow(threshold/envelope,1-1/ratio):1;
  float trem=p[9]>0?1-p[9]*(.5f+.5f*std::sin(phase)):1;
  if(smoothVolume)volumeSmoothed+=volumeSmoothing*(p[0]-volumeSmoothed);else volumeSmoothed=p[0];
  for(int c=0;c<2;++c){
   float dry=x[c]*gain*trem;
   // Delay feedback remains below unity; wet controls never alter dry gain.
   float echo=0;if(p[7]>0||delayTail){echo=delay[c][delayAt];delay[c][delayAt]=(p[7]>0?dry:0)+echo*.35f;}
   float mod=0;if(p[8]>0||chorusTail){float offset=rate*(.018f+.004f*std::sin(chorusPhase+c*pi/2));
   float read=static_cast<float>(chorusAt)-offset;
   if(read<0)read+=chorus[c].size();
   auto a=static_cast<std::size_t>(read),b=(a+1)%chorus[c].size();float frac=read-a;
   mod=chorus[c][a]*(1-frac)+chorus[c][b]*frac;chorus[c][chorusAt]=p[8]>0?dry:0;}
   float room=0,largeRoom=0;if(p[6]>0||reverbTail){for(auto& comb:reverb[c])room+=comb.tick(p[6]>0?dry*.25f:0);if(hallMix>1e-6f||reverbTail)largeRoom=hall[c].tick(p[6]>0?dry*hallMix:0);}
   float reverberation=p[6]*room*.35f*(1-.5f*hallMix)+largeRoom*.65f*hallMix;
   float out=(dry+p[7]*echo*.5f+p[8]*mod*.4f+reverberation)*volumeSmoothed;
   (c?right:left)[i]=clean(out);
  }
  if(delayTail)--delayTail;if(chorusTail)--chorusTail;if(reverbTail)--reverbTail;
  if(++delayAt==delay[0].size())delayAt=0;
  if(++chorusAt==chorus[0].size())chorusAt=0;
  phase+=2*pi*5/rate;chorusPhase+=2*pi*.8f/rate;
  if(phase>=2*pi)phase-=2*pi;
  if(chorusPhase>=2*pi)chorusPhase-=2*pi;
 }
}
}

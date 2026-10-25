#include "dsp.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
struct Result {double early=0,late=0,end=0,peak=0;};
Result impulse(float amount,float rate){
 lp::Dsp dsp(rate);auto p=lp::Patch{}.parameters;p[6]=amount;dsp.parameters(p);dsp.reset();
 const int n=int(rate*16);std::vector<float> l(n),r(n);l[0]=r[0]=.5f;
 // Small blocks exercise the actual persistent audio-callback state.
 for(int i=0;i<n;i+=127)dsp.process(l.data()+i,r.data()+i,std::min(127,n-i));
 Result result;
 for(int i=1;i<n;++i){require(std::isfinite(l[i])&&std::isfinite(r[i]),"Finite reverb tail");double e=l[i]*l[i]+r[i]*r[i];
  if(i>rate*.1&&i<rate)result.early+=e;
  if(i>rate*2&&i<rate*4)result.late+=e;
  if(i>rate*14)result.end+=e;
  result.peak=std::max(result.peak,double(std::max(std::abs(l[i]),std::abs(r[i]))));}
 require(l[0]==.5f&&r[0]==.5f,"Reverb preserves direct attack and gain");
 dsp.reset();std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);dsp.process(l.data(),r.data(),n);
 for(int i=0;i<n;++i)require(l[i]==0&&r[i]==0,"Reset clears all hall memory");return result;
}
int main(){
 // Compare with the previous four-comb impulse response below/at the boundary.
 for(float amount:{0.f,.3f,.7f}){
  lp::Dsp dsp;auto p=lp::Patch{}.parameters;p[6]=amount;dsp.parameters(p);
  std::vector<float> l(48000),r(48000);l[0]=r[0]=.5f;dsp.process(l.data(),r.data(),48000);
  constexpr float lengths[]{.0297f,.0371f,.0411f,.0437f};
  for(int channel=0;channel<2;++channel){std::array<std::vector<float>,4> comb;std::array<int,4> at{};
   for(int k=0;k<4;++k)comb[k].resize(static_cast<int>(48000*(lengths[k]+channel*.0013f)));
   for(int i=0;i<48000;++i){float dry=i==0?.5f:0,room=0;
    for(int k=0;k<4;++k){float v=comb[k][at[k]];comb[k][at[k]]=dry*.25f+v*.72f;room+=v;if(++at[k]==int(comb[k].size()))at[k]=0;}
    require((channel?r:l)[i]==dry+amount*room*.35f,"Exact legacy response through 70 percent");}
  }
 }

 for(float rate:{8000.f,44100.f,48000.f,96000.f}){
  auto small=impulse(.7f,rate),medium=impulse(.85f,rate),large=impulse(1,rate);
  require(small.late<1e-12,"Legacy reverb remains short at 70 percent");
  require(medium.late>1e-7&&large.late>medium.late*2,"Upper range progressively extends late energy");
  require(large.end<large.late*.001,"Large hall decays instead of sustaining indefinitely");
  require(large.peak<.5,"Impulse return remains bounded");
  std::cout<<rate<<" Hz: late energy 70/85/100="<<small.late<<"/"<<medium.late<<"/"<<large.late<<"; final="<<large.end<<"\n";
 }
 // A silent control sweep must never generate sound or non-finite state.
 lp::Dsp dsp;auto p=lp::Patch{}.parameters;float l[64]{},r[64]{};
 for(int block=0;block<2000;++block){p[6]=(block%101)/100.f;dsp.parameters(p);dsp.process(l,r,64);for(int i=0;i<64;++i)require(l[i]==0&&r[i]==0,"Silent automation stays silent");}
}

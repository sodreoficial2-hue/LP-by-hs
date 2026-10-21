#include "sf2_engine.hpp"
#include "session.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(int argc,char** argv){
 try{
  if(argc!=3)throw std::runtime_error("Expected FluidSynth library and SF2 path");
  lp::Sf2Engine synth(argv[1]);synth.load(std::filesystem::u8path(argv[2]));
  auto presets=synth.presets();
  if(presets.size()<10)throw std::runtime_error("Real SF2 presets not found");
  synth.select(presets.front().bank,presets.front().program);
  auto p=lp::Patch{}.parameters;
  p[1]=0.15f;p[2]=0.22f;p[3]=0.6f;p[4]=0.3f;p[6]=0.2f;p[8]=0.15f;
  synth.parameters(p,true);synth.cc(1,90);synth.cc(64,127);synth.pitchBend(9000);
  synth.noteOn(60,110);
  std::vector<float> left(8192),right(8192);
  synth.render(left.data(),right.data(),static_cast<int>(left.size()));
  double energy=0;for(float x:left){if(!std::isfinite(x))throw std::runtime_error("Invalid SF2 sample");energy+=double(x)*x;}
  if(energy<1e-8)throw std::runtime_error("Silent SF2 render");
  synth.noteOff(60);synth.cc(64,0);synth.parameters(p,false);synth.panic();
  std::cout<<"PASS real SF2 presets="<<presets.size()<<" energy="<<energy<<" ADSR/effects/MIDI calls\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

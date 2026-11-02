#include "sample_builder.hpp"
#include "sf2_engine.hpp"
#include <cassert>
#include <iostream>
using namespace lp::sampling;
Sample make(double amplitude,bool harmonic){Sample s;s.root=69;s.rate=48000;s.velocity=harmonic?110:20;s.explicitVelocity=true;for(int i=0;i<48000;++i)s.pcm.push_back(static_cast<float>(amplitude*(std::sin(2*pi*440*i/s.rate)+(harmonic?.5*std::sin(2*pi*1320*i/s.rate):0))));s.energy=rms(s,0,s.frames());findLoop(s);return s;}
double band(const std::vector<float>& data,double hz){double re=0,im=0;for(std::size_t i=4800;i<24000;++i){re+=data[i]*std::cos(2*pi*hz*i/48000);im+=data[i]*std::sin(2*pi*hz*i/48000);}return std::hypot(re,im)/19200;}
int main(int argc,char** argv){assert(argc==3);Project p;p.samples={make(.08,false),make(.4,true)};map(p);auto file=std::filesystem::path(argv[2]);save(p,"Dynamic instrument",file);lp::Sf2Engine synth(argv[1]);synth.load(file);synth.select(0,0);assert(synth.presets().size()==1);
 std::vector<float> left(48000),right(48000);double softHarmonic=0,softFundamental=0;
 for(int velocity:{20,110}){synth.panic();synth.noteOn(69,velocity);synth.render(left.data(),right.data(),48000);for(float v:left)assert(std::isfinite(v));auto fundamental=band(left,440),harmonic=band(left,1320);assert(fundamental>1e-5);if(velocity==20){softFundamental=fundamental;softHarmonic=harmonic/fundamental;}else{assert(fundamental>softFundamental*2);assert(harmonic/fundamental>.1);assert(harmonic/fundamental>softHarmonic*10);}}
 synth.panic();synth.noteOn(81,110);synth.render(left.data(),right.data(),48000);assert(band(left,880)>band(left,440)*10);
 // The generated font also sustains beyond the original one-second recording.
 synth.render(left.data(),right.data(),48000);assert(band(left,880)>1e-5);synth.noteOff(81);synth.panic();synth.render(left.data(),right.data(),48000);for(float v:left)assert(std::abs(v)<1e-5);
 std::cout<<"PASS real FluidSynth: generated SF2 loads, separate velocity timbres, transposition, sustain and panic\n";
}

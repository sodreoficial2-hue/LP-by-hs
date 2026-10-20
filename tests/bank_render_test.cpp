#include "instrument_catalog.hpp"
#include "sfizz_engine.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("Expected sfizz library and bank root");
 auto bank=lp::loadInstrumentBank(argv[2]);lp::SfizzEngine engine(argv[1],48000,256,true);std::array<unsigned,11> counts{};
 std::cout<<"id\tnote\tregions\tvelocity\trms\tpeak\n";
 for(const auto& instrument:bank){++counts[instrument.category];engine.load(instrument.sfz);if(engine.regions()==0)throw std::runtime_error("Empty instrument: "+instrument.id);
  std::vector<int> velocities{40,90,120};
  if(instrument.id=="piano-c5")velocities={10,30,35,40,45,48,54,60,68,76,84,92,100,108,116,124};
  std::vector<int> notes{instrument.testNote};
  if(instrument.id=="percussion-kit")notes={54,60,64};
  if(instrument.id=="sonor-kit")notes={36,38,42,43};
  for(int note:notes)for(int velocity:velocities){
   engine.panic();engine.noteOn(note,velocity);double energy=0;float peak=0;std::array<float,256> left{},right{};
   for(int i=0;i<48000;i+=256){int n=std::min(256,48000-i);engine.render(left.data(),right.data(),n);for(int j=0;j<n;++j){if(!std::isfinite(left[j])||!std::isfinite(right[j]))throw std::runtime_error("Non-finite audio: "+instrument.id);energy+=left[j]*left[j]+right[j]*right[j];peak=std::max({peak,std::abs(left[j]),std::abs(right[j])});}}
   double rms=std::sqrt(energy/96000);if(rms<1e-6)throw std::runtime_error("Silent instrument: "+instrument.id+" velocity "+std::to_string(velocity));std::cout<<instrument.id<<'\t'<<note<<'\t'<<engine.regions()<<'\t'<<velocity<<'\t'<<rms<<'\t'<<peak<<std::endl;engine.noteOff(note);
  }
 }
 for(auto n:counts)if(!n)throw std::runtime_error("Category without a working instrument");
 std::cout<<"PASS: "<<bank.size()<<" instruments, 11 populated categories, 3 velocities, finite audible real SFZ rendering\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

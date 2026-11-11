#include "sfizz_engine.hpp"
#include "dsp.hpp"
#include <atomic>
#include <cstdlib>
#include <new>
#include <cassert>
#include <fstream>
#include <iostream>
static std::atomic<unsigned> allocations{0};static thread_local bool measuring=false;
void* operator new(std::size_t n){if(measuring)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){
 assert(argc>=2);std::string mode=argv[1];
 if(mode=="dsp"){
  lp::Dsp dsp;float l[]{.9f,1.2f,-.9f,-1.2f},r[]{.9f,1.2f,-.9f,-1.2f};dsp.process(l,r,4);
  float expected[]{.9f,1.2f,-.9f,-1.2f};for(int i=0;i<4;++i)if(l[i]!=expected[i]||r[i]!=expected[i]){std::cerr<<"FAIL: neutral DSP altered sample "<<expected[i]<<" into "<<l[i]<<'\n';return 1;}
 }else{
  assert(argc==3);auto path=std::filesystem::temp_directory_path()/"lp-realtime-envelope.sfz";std::ofstream(path)<<"fixture";
  lp::SfizzEngine synth(argv[2]);synth.load(path);auto values=lp::Patch{}.parameters;
  measuring=true;synth.parameters(values,true);values[14]=.5f;synth.parameters(values,true);values[14]=0;synth.parameters(values,false);measuring=false;
  std::filesystem::remove(path);if(allocations.load()){std::cerr<<"FAIL: "<<allocations<<" allocations during live parameter updates\n";return 1;}
 }
 std::cout<<"PASS: "<<mode<<'\n';
}

#include "sfizz_engine.hpp"
#include "dsp.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
struct sfizz_synth_t;
namespace lp {
namespace {
class Module {
#ifdef _WIN32
 HMODULE handle{};
#else
 void* handle{};
#endif
public:
 explicit Module(const std::filesystem::path& path) {
  // Absolute paths avoid loading a different sfizz from the working directory.
  auto absolute=std::filesystem::absolute(path);
#ifdef _WIN32
  handle=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
  handle=dlopen(absolute.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
  if(!handle) throw std::runtime_error("Cannot load sfizz library or its dependencies");
 }
 ~Module() {
#ifdef _WIN32
  if(handle) FreeLibrary(handle);
#else
  if(handle) dlclose(handle);
#endif
 }
 template<class T> T symbol(const char* name) {
#ifdef _WIN32
  auto address=GetProcAddress(handle,name);
#else
  auto address=dlsym(handle,name);
#endif
  if(!address) throw std::runtime_error(std::string("Missing sfizz API: ")+name);
  T function{};
  static_assert(sizeof(function)==sizeof(address),"Unsupported dynamic function-pointer ABI");
  std::memcpy(&function,&address,sizeof(function));
  return function;
 }
 Module(const Module&)=delete;
 Module& operator=(const Module&)=delete;
};
void midiValue(int value) { if(value<0 || value>127) throw std::invalid_argument("MIDI value outside 0..127"); }
}
struct SfizzEngine::Impl {
 Module module;
 Dsp dsp;
 sfizz_synth_t* (*create)();
 void (*freeSynth)(sfizz_synth_t*);
 bool (*loadFile)(sfizz_synth_t*,const char*);
 void (*setRate)(sfizz_synth_t*,float);
 void (*setBlock)(sfizz_synth_t*,int);
 void (*on)(sfizz_synth_t*,int,int,int);
 void (*off)(sfizz_synth_t*,int,int,int);
 void (*control)(sfizz_synth_t*,int,int,int);
 void (*silence)(sfizz_synth_t*);
 void (*renderBlock)(sfizz_synth_t*,float**,int,int);
 int (*numRegions)(sfizz_synth_t*);
 float rate; int block;
 sfizz_synth_t* synth{};
 Impl(const std::filesystem::path& library,float r,int b):module(library),dsp(r),rate(r),block(b) {
#define LP_BIND(member,name) member=module.symbol<decltype(member)>(name)
  LP_BIND(create,"sfizz_create_synth"); LP_BIND(freeSynth,"sfizz_free");
  LP_BIND(loadFile,"sfizz_load_file"); LP_BIND(setRate,"sfizz_set_sample_rate");
  LP_BIND(setBlock,"sfizz_set_samples_per_block"); LP_BIND(on,"sfizz_send_note_on");
  LP_BIND(off,"sfizz_send_note_off"); LP_BIND(control,"sfizz_send_cc");
  LP_BIND(silence,"sfizz_all_sound_off"); LP_BIND(renderBlock,"sfizz_render_block");
  LP_BIND(numRegions,"sfizz_get_num_regions");
#undef LP_BIND
 }
 ~Impl() { if(synth) freeSynth(synth); }
};
SfizzEngine::SfizzEngine(const std::filesystem::path& dll,float rate,int block) {
 if(!std::isfinite(rate) || rate<8000 || rate>384000 || block<1 || block>16384)
  throw std::invalid_argument("Invalid sample rate or block size");
 impl=std::make_unique<Impl>(dll,rate,block);
}
SfizzEngine::~SfizzEngine()=default;
void SfizzEngine::load(const std::filesystem::path& path) {
 if(!std::filesystem::is_regular_file(path)) throw std::runtime_error("SFZ file not found");
 auto& a=*impl;
 auto candidate=std::unique_ptr<sfizz_synth_t,decltype(a.freeSynth)>(a.create(),a.freeSynth);
 if(!candidate) throw std::runtime_error("sfizz could not create a synthesizer");
 a.setRate(candidate.get(),a.rate); a.setBlock(candidate.get(),a.block);
 const auto utf8=std::filesystem::absolute(path).u8string();
 if(!a.loadFile(candidate.get(),utf8.c_str()) || a.numRegions(candidate.get())<=0)
  throw std::runtime_error("SFZ failed to load or has no regions");
 auto previous=a.synth; a.synth=candidate.release(); a.dsp.reset();
 if(previous) a.freeSynth(previous);
}
void SfizzEngine::parameters(const std::array<float,14>& values) {impl->dsp.parameters(values);}
void SfizzEngine::unload() {impl->dsp.reset();if(impl->synth){impl->freeSynth(impl->synth);impl->synth=nullptr;}}
bool SfizzEngine::loaded() const noexcept { return impl->synth!=nullptr; }
int SfizzEngine::regions() const { return loaded()?impl->numRegions(impl->synth):0; }
void SfizzEngine::noteOn(int note,int velocity) {
 midiValue(note); midiValue(velocity);
 if(loaded()) { if(velocity) impl->on(impl->synth,0,note,velocity); else impl->off(impl->synth,0,note,0); }
}
void SfizzEngine::noteOff(int note,int velocity) { midiValue(note); midiValue(velocity); if(loaded()) impl->off(impl->synth,0,note,velocity); }
void SfizzEngine::cc(int number,int value) { midiValue(number); midiValue(value); if(loaded()) impl->control(impl->synth,0,number,value); }
void SfizzEngine::panic() { if(loaded()) impl->silence(impl->synth); impl->dsp.reset(); }
void SfizzEngine::render(float* left,float* right,int frames) {
 if(!left || !right || left==right || frames<1 || frames>impl->block) throw std::invalid_argument("Invalid stereo render block");
 if(!loaded()) {std::fill_n(left,frames,0);std::fill_n(right,frames,0);return;}
 float* channels[]{left,right}; impl->renderBlock(impl->synth,channels,2,frames);
 impl->dsp.process(left,right,frames);
}
}

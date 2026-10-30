#include "sfizz_engine.hpp"
#include "dsp.hpp"
#include "envelope.hpp"
#include "mono_glide.hpp"
#include "../third_party/sfizz/sfizz_message.h"
#include <vector>
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
struct sfizz_client_t;
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
 Dsp dsp;MonoGlide glide;
 sfizz_client_t* (*createClient)(void*);
 void (*freeClient)(sfizz_client_t*);
 void (*receiveCallback)(sfizz_client_t*,sfizz_receive_t*);
 void (*message)(sfizz_synth_t*,sfizz_client_t*,int,const char*,const char*,const sfizz_arg_t*);
 sfizz_client_t* client{};
 float* capture=nullptr;bool captured=false;
 struct RegionEnvelope {std::array<std::string,4> path;std::array<float,4> original;std::array<std::string,2> bendPath;std::array<float,2> bendRange;};
 std::vector<RegionEnvelope> envelopes;
 std::array<float,4> lastEnvelope{};bool adsrEnabled=false;
 static void receive(void* user,int,const char*,const char* signature,const sfizz_arg_t* args){
  auto& state=*static_cast<Impl*>(user);
  if(state.capture&&signature&&std::strcmp(signature,"f")==0&&args){*state.capture=args[0].f;state.captured=true;}
 }
 float query(sfizz_synth_t* synth,const std::string& path){
  float value=0;capture=&value;captured=false;message(synth,client,0,path.c_str(),"",nullptr);capture=nullptr;
  if(!captured||!std::isfinite(value))throw std::runtime_error("sfizz ADSR readback unavailable");
  return value;
 }
 sfizz_synth_t* (*create)();
 void (*freeSynth)(sfizz_synth_t*);
 bool (*loadFile)(sfizz_synth_t*,const char*);
 void (*setRate)(sfizz_synth_t*,float);
 void (*setBlock)(sfizz_synth_t*,int);
 void (*on)(sfizz_synth_t*,int,int,int);
 void (*off)(sfizz_synth_t*,int,int,int);
 void (*control)(sfizz_synth_t*,int,int,int);
 void (*silence)(sfizz_synth_t*);
 void (*bend)(sfizz_synth_t*,int,int);
 void (*enableOffline)(sfizz_synth_t*);
 bool offline=false;
 void (*renderBlock)(sfizz_synth_t*,float**,int,int);
 int (*numRegions)(sfizz_synth_t*);
 float rate; int block;
 sfizz_synth_t* synth{};
 Impl(const std::filesystem::path& library,float r,int b,bool offlineMode):module(library),dsp(r),offline(offlineMode),rate(r),block(b) {
#define LP_BIND(member,name) member=module.symbol<decltype(member)>(name)
  LP_BIND(createClient,"sfizz_create_client");LP_BIND(freeClient,"sfizz_delete_client");
  LP_BIND(receiveCallback,"sfizz_set_receive_callback");LP_BIND(message,"sfizz_send_message");
  LP_BIND(create,"sfizz_create_synth"); LP_BIND(freeSynth,"sfizz_free");
  LP_BIND(loadFile,"sfizz_load_file"); LP_BIND(setRate,"sfizz_set_sample_rate");
  LP_BIND(setBlock,"sfizz_set_samples_per_block"); LP_BIND(on,"sfizz_send_note_on");
  LP_BIND(off,"sfizz_send_note_off"); LP_BIND(control,"sfizz_send_cc");
  LP_BIND(enableOffline,"sfizz_enable_freewheeling"); LP_BIND(silence,"sfizz_all_sound_off"); LP_BIND(renderBlock,"sfizz_render_block");
  LP_BIND(numRegions,"sfizz_get_num_regions");LP_BIND(bend,"sfizz_send_pitch_wheel");
#undef LP_BIND
  client=createClient(this);if(!client)throw std::runtime_error("sfizz messaging client unavailable");
  receiveCallback(client,receive);
 }
 ~Impl() { if(synth) freeSynth(synth);if(client)freeClient(client); }
};
SfizzEngine::SfizzEngine(const std::filesystem::path& dll,float rate,int block,bool offline) {
 if(!std::isfinite(rate) || rate<8000 || rate>384000 || block<1 || block>16384)
  throw std::invalid_argument("Invalid sample rate or block size");
 impl=std::make_unique<Impl>(dll,rate,block,offline);
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
 if(a.offline)a.enableOffline(candidate.get());
 std::vector<Impl::RegionEnvelope> envelopes;
 int count=a.numRegions(candidate.get());envelopes.reserve(static_cast<std::size_t>(count));
 constexpr const char* names[]{"ampeg_attack","ampeg_decay","ampeg_sustain","ampeg_release"};
 for(int region=0;region<count;++region){Impl::RegionEnvelope e;
  for(int p=0;p<4;++p){e.path[p]="/region"+std::to_string(region)+"/"+names[p];e.original[p]=a.query(candidate.get(),e.path[p]);}
  for(int b=0;b<2;++b){e.bendPath[b]="/region"+std::to_string(region)+(b?"/bend_down":"/bend_up");e.bendRange[b]=a.query(candidate.get(),e.bendPath[b]);}
  envelopes.push_back(std::move(e));
 }
 a.envelopes=std::move(envelopes);a.adsrEnabled=false;a.glide.reset();a.glide.amount=0;
 auto previous=a.synth; a.synth=candidate.release(); a.dsp.reset();
 if(previous) a.freeSynth(previous);
}
void SfizzEngine::parameters(const std::array<float,15>& values,bool enabled) {
 auto& a=*impl;a.dsp.parameters(values);auto mapped=envelopeValues(values);
 if(loaded()&&((values[14]>0)!=(a.glide.amount>0))){
  a.silence(a.synth);a.glide.reset();
  for(const auto& region:a.envelopes)for(int b=0;b<2;++b){sfizz_arg_t arg{};arg.f=values[14]>0?(b?-9600.f:9600.f):region.bendRange[b];a.message(a.synth,a.client,0,region.bendPath[b].c_str(),"f",&arg);}
 }
 a.glide.amount=values[14];
 if(loaded()&&(enabled!=a.adsrEnabled||(enabled&&mapped!=a.lastEnvelope))){
  for(const auto& region:a.envelopes)for(int p=0;p<4;++p){sfizz_arg_t arg{};arg.f=enabled?mapped[p]:region.original[p];a.message(a.synth,a.client,0,region.path[p].c_str(),"f",&arg);}
 }
 a.adsrEnabled=enabled;a.lastEnvelope=mapped;
}
std::array<float,4> SfizzEngine::envelope(int region){
 if(region<0||static_cast<std::size_t>(region)>=impl->envelopes.size()||!loaded())throw std::out_of_range("Invalid sfizz region");
 std::array<float,4> result;for(int p=0;p<4;++p)result[p]=impl->query(impl->synth,impl->envelopes[region].path[p]);return result;
}
void SfizzEngine::unload() {impl->dsp.reset();impl->envelopes.clear();impl->adsrEnabled=false;if(impl->synth){impl->freeSynth(impl->synth);impl->synth=nullptr;}}
bool SfizzEngine::loaded() const noexcept { return impl->synth!=nullptr; }
int SfizzEngine::regions() const { return loaded()?impl->numRegions(impl->synth):0; }
void SfizzEngine::noteOn(int note,int velocity) {
 midiValue(note);midiValue(velocity);if(!loaded())return;auto& a=*impl;
 if(!velocity){noteOff(note);return;}
 if(a.glide.amount>0)a.glide.on(note,velocity,a.rate,[&](int n,int v){a.on(a.synth,0,n,v);},[&](int n){a.off(a.synth,0,n,0);},[&](int bend){a.bend(a.synth,0,bend);});
 else a.on(a.synth,0,note,velocity);
}
void SfizzEngine::noteOff(int note,int velocity){midiValue(note);midiValue(velocity);if(!loaded())return;auto& a=*impl;
 if(a.glide.amount>0)a.glide.off(note,a.rate,[&](int n,int v){a.on(a.synth,0,n,v);},[&](int n){a.off(a.synth,0,n,velocity);},[&](int bend){a.bend(a.synth,0,bend);});
 else a.off(a.synth,0,note,velocity);
}
void SfizzEngine::cc(int number,int value){midiValue(number);midiValue(value);if(loaded()){if(number>=120)impl->glide.reset();impl->control(impl->synth,0,number,number==64?(value>=64?127:0):value);}}
void SfizzEngine::pitchBend(int value){if(value< -8192||value>8191)throw std::invalid_argument("Invalid signed MIDI pitch bend");auto& a=*impl;a.glide.wheel=value;if(loaded())a.bend(a.synth,0,a.glide.amount>0?a.glide.bend():value);}
void SfizzEngine::panic(){impl->glide.reset();if(loaded())impl->silence(impl->synth);impl->dsp.reset();}
void SfizzEngine::smoothing(bool enabled){impl->dsp.smoothing(enabled);}
void SfizzEngine::render(float* left,float* right,int frames) {
 if(!left || !right || left==right || frames<1 || frames>impl->block) throw std::invalid_argument("Invalid stereo render block");
 if(!loaded()) {std::fill_n(left,frames,0);std::fill_n(right,frames,0);return;}
 for(int pos=0;pos<frames;){int size=impl->glide.amount>0?std::min(32,frames-pos):frames-pos;
  if(impl->glide.amount>0){impl->glide.advance(size);impl->bend(impl->synth,0,impl->glide.bend());}
  float* channels[]{left+pos,right+pos};impl->renderBlock(impl->synth,channels,2,size);pos+=size;
 }
 impl->dsp.process(left,right,frames);
}
}

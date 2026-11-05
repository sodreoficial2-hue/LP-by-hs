#pragma once
// SPDX-License-Identifier: AGPL-3.0-or-later
// Private offline ABI. Does not instantiate or modify the live audio host.
#include "sfz_conversion.hpp"
#include <array>
namespace lp {
class OfflineSfizz {
#ifdef _WIN32
 HMODULE lib{};void* symbol(const char* n){return reinterpret_cast<void*>(GetProcAddress(lib,n));}
#else
 void* lib{};void* symbol(const char* n){return dlsym(lib,n);}
#endif
 void* synth{};void* (*create)(){};void (*destroy)(void*){};
 bool (*loadFile)(void*,const char*){};void (*rate)(void*,float){};void (*block)(void*,int){};
 void (*on)(void*,int,int,int){};void (*off)(void*,int,int,int){};void (*cc)(void*,int,int,int){};
 void (*freewheel)(void*){};void (*render)(void*,float**,int,int){};
 template<class T>void bind(T& f,const char* n){f=reinterpret_cast<T>(symbol(n));if(!f)throw std::runtime_error(std::string("sfizz offline sem função: ")+n);}
 void close(){if(synth&&destroy)destroy(synth);synth=nullptr;if(lib){
#ifdef _WIN32
 FreeLibrary(lib);
#else
 dlclose(lib);
#endif
 lib=nullptr;}}
public:
 explicit OfflineSfizz(const std::filesystem::path& path){
#ifdef _WIN32
 lib=LoadLibraryExW(std::filesystem::absolute(path).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
 lib=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
 if(!lib)throw std::runtime_error("sfizz real indisponível para calibração offline");
 try{bind(create,"sfizz_create_synth");bind(destroy,"sfizz_free");bind(loadFile,"sfizz_load_file");bind(rate,"sfizz_set_sample_rate");bind(block,"sfizz_set_samples_per_block");bind(on,"sfizz_send_note_on");bind(off,"sfizz_send_note_off");bind(cc,"sfizz_send_cc");bind(freewheel,"sfizz_enable_freewheeling");bind(render,"sfizz_render_block");}catch(...){close();throw;}
 }
 ~OfflineSfizz(){close();}OfflineSfizz(const OfflineSfizz&)=delete;OfflineSfizz& operator=(const OfflineSfizz&)=delete;
 double chordPeak(const std::filesystem::path& path,const std::vector<int>& keys,int velocity,int pedal){
  if(synth){destroy(synth);synth=nullptr;}synth=create();if(!synth)throw std::runtime_error("Falha ao criar sfizz offline");rate(synth,48000);block(synth,256);freewheel(synth);
  if(!loadFile(synth,path.u8string().c_str()))throw std::runtime_error("Falha no probe de headroom");cc(synth,0,64,pedal);for(int k:keys)on(synth,0,k,velocity);
  std::array<float,256> l{},r{};float* ptrs[]={l.data(),r.data()};double peak=0;
  for(int b=0;b<188;++b){render(synth,ptrs,2,256);for(int i=0;i<256;++i){if(!std::isfinite(l[i])||!std::isfinite(r[i]))throw std::runtime_error("Headroom: áudio não finito");peak=std::max(peak,double(std::max(std::abs(l[i]),std::abs(r[i]))));}}return peak;
 }
 double rmsDb(const std::filesystem::path& path,int key,int velocity,int pedal,bool release){
  if(synth){destroy(synth);synth=nullptr;}synth=create();if(!synth)throw std::runtime_error("Falha ao criar sfizz offline");rate(synth,48000);block(synth,256);freewheel(synth);
  auto name=path.u8string();if(!loadFile(synth,name.c_str()))throw std::runtime_error("Falha ao carregar instrumento de calibração offline");
  cc(synth,0,64,pedal);on(synth,0,key,velocity);
  std::array<float,256> left{},right{};float* ptrs[]={left.data(),right.data()};
  // Release bank is measured alone, with a note-off after priming its note-on.
  if(release){render(synth,ptrs,2,256);off(synth,0,key,0);}
  double sum=0;constexpr int blocks=16;
  for(int b=0;b<blocks;++b){render(synth,ptrs,2,256);for(int i=0;i<256;++i){if(!std::isfinite(left[i])||!std::isfinite(right[i]))throw std::runtime_error("Calibração: áudio não finito");sum+=double(left[i])*left[i]+double(right[i])*right[i];}}
  if(sum<=1e-24)throw std::runtime_error("Calibração: região silenciosa; não é possível inferir ganho na tecla "+std::to_string(key)+", velocity "+std::to_string(velocity));
  return 10*std::log10(sum/(blocks*256*2));
 }
};
}

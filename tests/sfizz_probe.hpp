#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef min
#undef max
#else
#include <dlfcn.h>
#endif
#include <array>
#include <vector>
#include <filesystem>
#include <stdexcept>
struct SfizzProbe {
 void* lib{};void* synth{};
 void* (*create)(){};void (*destroy)(void*){};bool (*loadFile)(void*,const char*){};
 void (*rate)(void*,float){};void (*block)(void*,int){};void (*limit)(void*,int){};
 void (*on)(void*,int,int,int){};void (*off)(void*,int,int,int){};void (*cc)(void*,int,int,int){};
 void (*freewheel)(void*){};void (*silence)(void*){};void (*renderBlock)(void*,float**,int,int){};int (*voices)(void*){};int (*regions)(void*){};
 template<class T>void bind(T& fn,const char* name){
#ifdef _WIN32
 fn=reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(lib),name));
#else
 fn=reinterpret_cast<T>(dlsym(lib,name));
#endif
if(!fn)throw std::runtime_error(std::string("sfizz API missing: ")+name);}
 explicit SfizzProbe(const std::filesystem::path& library){
#ifdef _WIN32
  lib=LoadLibraryW(library.c_str());if(!lib)throw std::runtime_error("sfizz load failed: Windows error "+std::to_string(GetLastError()));
#else
  lib=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);if(!lib)throw std::runtime_error(std::string("sfizz load failed: ")+dlerror());
#endif
  bind(create,"sfizz_create_synth");bind(destroy,"sfizz_free");bind(loadFile,"sfizz_load_file");bind(rate,"sfizz_set_sample_rate");bind(block,"sfizz_set_samples_per_block");bind(limit,"sfizz_set_num_voices");bind(on,"sfizz_send_note_on");bind(off,"sfizz_send_note_off");bind(cc,"sfizz_send_cc");bind(freewheel,"sfizz_enable_freewheeling");bind(silence,"sfizz_all_sound_off");bind(renderBlock,"sfizz_render_block");bind(voices,"sfizz_get_num_active_voices");bind(regions,"sfizz_get_num_regions");synth=create();if(!synth)throw std::runtime_error("sfizz create failed");rate(synth,48000);block(synth,256);limit(synth,128);freewheel(synth);
 }
 ~SfizzProbe(){if(synth)destroy(synth);if(lib){
#ifdef _WIN32
 FreeLibrary(static_cast<HMODULE>(lib));
#else
 dlclose(lib);
#endif
 }}
 void load(const std::filesystem::path& p){if(!loadFile(synth,p.u8string().c_str()))throw std::runtime_error("sfizz failed to load "+p.u8string());}
 std::array<std::vector<float>,2> render(int frames){std::array<std::vector<float>,2> data{std::vector<float>(frames),std::vector<float>(frames)};for(int at=0;at<frames;at+=256){float* ptrs[]={data[0].data()+at,data[1].data()+at};renderBlock(synth,ptrs,2,std::min(256,frames-at));}return data;}
};


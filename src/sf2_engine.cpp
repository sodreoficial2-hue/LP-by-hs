#include "sf2_engine.hpp"
#include "dsp.hpp"
#include "envelope.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace lp {
namespace {
struct Settings;struct Synth;struct Font;struct Preset;
#ifdef _WIN32
using Lib=HMODULE;
Lib open(const std::filesystem::path& p){return LoadLibraryExW(std::filesystem::absolute(p).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);}
void* sym(Lib l,const char* n){return reinterpret_cast<void*>(GetProcAddress(l,n));}
void close(Lib l){if(l)FreeLibrary(l);}
#else
using Lib=void*;
Lib open(const std::filesystem::path& p){return dlopen(p.c_str(),RTLD_NOW|RTLD_LOCAL);}
void* sym(Lib l,const char* n){return dlsym(l,n);}
void close(Lib l){if(l)dlclose(l);}
#endif
template<class T>T bind(Lib l,const char* n){auto p=sym(l,n);if(!p)throw std::runtime_error(std::string("Missing FluidSynth symbol: ")+n);return reinterpret_cast<T>(p);}
}
struct Sf2Engine::Impl {
 Lib lib{};Settings* settings{};Synth* synth{};int font=-1;
 Dsp dsp{48000};bool adsrEnabled=false;std::array<float,4> envelope{};
 Settings* (*newSettings)(){};void (*deleteSettings)(Settings*){};
 int (*setNum)(Settings*,const char*,double){};
 Synth* (*newSynth)(Settings*){};void (*deleteSynth)(Synth*){};
 int (*sfload)(Synth*,const char*,int){};
 Font* (*getFont)(Synth*,int){};
 void (*iterStart)(Font*){};int (*iterNext)(Font*,Preset**){};
 int (*bank)(Preset*){};int (*program)(Preset*){};const char* (*name)(Preset*){};
 int (*select)(Synth*,int,int,int,int){};
 int (*on)(Synth*,int,int,int){};int (*off)(Synth*,int,int){};
 int (*cc)(Synth*,int,int,int){};int (*bend)(Synth*,int,int){};
 int (*silence)(Synth*,int){};int (*setGen)(Synth*,int,int,float){};int (*write)(Synth*,int,void*,int,int,void*,int,int){};
 ~Impl(){if(synth)deleteSynth(synth);if(settings)deleteSettings(settings);close(lib);}
};
Sf2Engine::Sf2Engine(const std::filesystem::path& path,int rate):impl(new Impl){
 auto& x=*impl;x.lib=open(path);if(!x.lib)throw std::runtime_error("FluidSynth DLL unavailable");
#define B(field,symbol) x.field=bind<decltype(x.field)>(x.lib,symbol)
 B(newSettings,"new_fluid_settings");B(deleteSettings,"delete_fluid_settings");
 B(setNum,"fluid_settings_setnum");B(newSynth,"new_fluid_synth");B(deleteSynth,"delete_fluid_synth");
 B(sfload,"fluid_synth_sfload");B(getFont,"fluid_synth_get_sfont_by_id");
 B(iterStart,"fluid_sfont_iteration_start");B(iterNext,"fluid_sfont_iteration_next");
 B(bank,"fluid_preset_get_banknum");B(program,"fluid_preset_get_num");B(name,"fluid_preset_get_name");
 B(select,"fluid_synth_program_select");B(on,"fluid_synth_noteon");B(off,"fluid_synth_noteoff");
 B(cc,"fluid_synth_cc");B(bend,"fluid_synth_pitch_bend");B(silence,"fluid_synth_all_sounds_off");
 B(write,"fluid_synth_write_float");B(setGen,"fluid_synth_set_gen");
#undef B
 x.settings=x.newSettings();if(!x.settings)throw std::runtime_error("FluidSynth settings failed");
 x.setNum(x.settings,"synth.sample-rate",rate);x.synth=x.newSynth(x.settings);
 if(!x.synth)throw std::runtime_error("FluidSynth synth failed");
}
Sf2Engine::~Sf2Engine()=default;
void Sf2Engine::load(const std::filesystem::path& path){
 if(!std::filesystem::is_regular_file(path))throw std::runtime_error("SF2 missing");
 // Load into a fresh engine per instrument swap; never load in the ASIO callback.
 auto name=std::filesystem::absolute(path).u8string();
 int font=impl->sfload(impl->synth,name.c_str(),0);
 if(font<0)throw std::runtime_error("SF2 loading failed");impl->font=font;
}
std::vector<Sf2Preset> Sf2Engine::presets() const{
 std::vector<Sf2Preset> result;if(!loaded())return result;
 auto& x=*impl;auto* f=x.getFont(x.synth,x.font);if(!f)return result;
 x.iterStart(f);Preset* p=nullptr;while(x.iterNext(f,&p)){if(!p)break;auto* n=x.name(p);result.push_back({x.bank(p),x.program(p),n?n:"Unnamed"});}
 return result;
}
void Sf2Engine::select(int bank,int program){
 if(!loaded()||bank<0||bank>16383||program<0||program>127||impl->select(impl->synth,0,impl->font,bank,program)!=0)
 throw std::runtime_error("SF2 preset not found");
}
void Sf2Engine::noteOn(int n,int v){if(loaded())impl->on(impl->synth,0,n,v);}
void Sf2Engine::noteOff(int n){if(loaded())impl->off(impl->synth,0,n);}
void Sf2Engine::cc(int n,int v){if(loaded())impl->cc(impl->synth,0,n,v);}
void Sf2Engine::pitchBend(int v){if(loaded())impl->bend(impl->synth,0,std::clamp(v,0,16383));}
void Sf2Engine::panic(){if(loaded())impl->silence(impl->synth,0);impl->dsp.reset();}
void Sf2Engine::parameters(const std::array<float,14>& values,bool enabled){
 impl->dsp.parameters(values);
 if(!loaded())return;
 auto env=envelopeValues(values);
 if(!enabled){if(impl->adsrEnabled){for(int gen:{34,36,37,38})impl->setGen(impl->synth,0,gen,0.f);}impl->adsrEnabled=false;return;}
 if(impl->adsrEnabled && env==impl->envelope)return;
 // SoundFont volume envelope generators: timecents and centibels.
 auto timecents=[](float seconds){return std::clamp(1200.f*std::log2(std::max(seconds,.001f)),-12000.f,8000.f);};
 impl->setGen(impl->synth,0,34,timecents(env[0])); // attack
 impl->setGen(impl->synth,0,36,timecents(env[1])); // decay
 impl->setGen(impl->synth,0,37,(100.f-env[2])*14.4f); // sustain attenuation, 0..1440 cB
 impl->setGen(impl->synth,0,38,timecents(env[3])); // release
 impl->envelope=env;impl->adsrEnabled=true;
}
void Sf2Engine::render(float* l,float* r,int frames){
 if(!loaded()){std::fill_n(l,frames,0);std::fill_n(r,frames,0);return;}
 if(impl->write(impl->synth,frames,l,0,1,r,0,1)!=0)throw std::runtime_error("SF2 render failed");
 impl->dsp.process(l,r,frames);
}
bool Sf2Engine::loaded() const noexcept{return impl->font>=0;}
}

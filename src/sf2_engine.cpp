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
struct Settings;struct Synth;struct Font;struct Preset;struct Mod;
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
 Dsp dsp{48000};float glideAmount=0;int glideMs=-1;bool glideReady=false;std::array<bool,128> keys{};float rate=48000;bool adsrEnabled=false;std::array<float,4> envelope{};
 Settings* (*newSettings)(){};void (*deleteSettings)(Settings*){};
 int (*setNum)(Settings*,const char*,double){};
 Synth* (*newSynth)(Settings*){};void (*deleteSynth)(Synth*){};
 int (*sfload)(Synth*,const char*,int){};
 Font* (*getFont)(Synth*,int){};
 void (*iterStart)(Font*){};Preset* (*iterNext)(Font*){};
 int (*bank)(Preset*){};int (*program)(Preset*){};const char* (*name)(Preset*){};
 int (*select)(Synth*,int,int,int,int){};
 int (*on)(Synth*,int,int,int){};int (*off)(Synth*,int,int){};
 int (*cc)(Synth*,int,int,int){};int (*bend)(Synth*,int,int){};
 int (*sensitivity)(Synth*,int,int){};
 int (*portamentoMode)(Synth*,int,int){};int (*legatoMode)(Synth*,int,int){};
 int (*portamentoTimeMode)(Synth*,int){}; // Optional, introduced in FluidSynth 2.5.
 void clearHeldNotes(){for(int key=0;key<128;++key)if(keys[key]){off(synth,0,key);keys[key]=false;}}
 static void check(int status,const char* action){if(status!=0)throw std::runtime_error(action);}
 void configureGlide(float amount){
  if(!std::isfinite(amount)||amount<0||amount>1)throw std::invalid_argument("Invalid SF2 portamento");
  const bool enabled=amount>0,changed=!glideReady||enabled!=(glideAmount>0);
  if(changed){
   clearHeldNotes();
   check(silence(synth,0),"SF2 portamento voice reset failed");
   check(cc(synth,0,123,0),"SF2 portamento note reset failed");
   // CC126/127 use FluidSynth's own fixed-size mono note stack. Zero remains polyphonic.
   check(cc(synth,0,enabled?126:127,enabled?1:0),"SF2 mono/poly setup failed");
   check(legatoMode(synth,0,1),"SF2 legato setup failed"); // multi-retrigger/reuse voices
   check(portamentoMode(synth,0,0),"SF2 portamento mode failed"); // each note
   check(cc(synth,0,65,enabled?127:0),"SF2 portamento switch failed");
  }
  const int milliseconds=enabled?static_cast<int>(std::lround(5+amount*amount*1995)):0;
  if(changed||milliseconds!=glideMs){
   // 14-bit milliseconds. LSB also selects linear time on 2.5+ in auto mode.
   check(cc(synth,0,37,milliseconds&127),"SF2 portamento time LSB failed");
   check(cc(synth,0,5,milliseconds>>7),"SF2 portamento time MSB failed");
  }
  glideAmount=amount;glideMs=milliseconds;glideReady=true;
 }
 Mod* (*newMod)(){};void (*deleteMod)(Mod*){};void (*modSource1)(Mod*,int,int){};void (*modSource2)(Mod*,int,int){};void (*modDest)(Mod*,int){};int (*removeDefaultMod)(Synth*,const Mod*){};
 int (*silence)(Synth*,int){};int (*setGen)(Synth*,int,int,float){};int (*write)(Synth*,int,void*,int,int,void*,int,int){};
 ~Impl(){if(synth)deleteSynth(synth);if(settings)deleteSettings(settings);close(lib);}
};
Sf2Engine::Sf2Engine(const std::filesystem::path& path,int rate):impl(new Impl){
 auto& x=*impl;x.rate=static_cast<float>(rate);x.dsp=Dsp(static_cast<float>(rate));x.lib=open(path);if(!x.lib)throw std::runtime_error("FluidSynth DLL unavailable");
#define B(field,symbol) x.field=bind<decltype(x.field)>(x.lib,symbol)
 B(newSettings,"new_fluid_settings");B(deleteSettings,"delete_fluid_settings");
 B(setNum,"fluid_settings_setnum");B(newSynth,"new_fluid_synth");B(deleteSynth,"delete_fluid_synth");
 B(sfload,"fluid_synth_sfload");B(getFont,"fluid_synth_get_sfont_by_id");
 B(iterStart,"fluid_sfont_iteration_start");B(iterNext,"fluid_sfont_iteration_next");
 B(bank,"fluid_preset_get_banknum");B(program,"fluid_preset_get_num");B(name,"fluid_preset_get_name");
 B(select,"fluid_synth_program_select");B(on,"fluid_synth_noteon");B(off,"fluid_synth_noteoff");
 B(sensitivity,"fluid_synth_pitch_wheel_sens");B(cc,"fluid_synth_cc");B(bend,"fluid_synth_pitch_bend");B(silence,"fluid_synth_all_sounds_off");
 B(portamentoMode,"fluid_synth_set_portamento_mode");B(legatoMode,"fluid_synth_set_legato_mode");
 B(write,"fluid_synth_write_float");B(setGen,"fluid_synth_set_gen");
 B(newMod,"new_fluid_mod");B(deleteMod,"delete_fluid_mod");B(modSource1,"fluid_mod_set_source1");B(modSource2,"fluid_mod_set_source2");B(modDest,"fluid_mod_set_dest");B(removeDefaultMod,"fluid_synth_remove_default_mod");
#undef B
 x.settings=x.newSettings();if(!x.settings)throw std::runtime_error("FluidSynth settings failed");
 x.setNum(x.settings,"synth.sample-rate",rate);x.synth=x.newSynth(x.settings);
 if(!x.synth)throw std::runtime_error("FluidSynth synth failed");
 x.portamentoTimeMode=reinterpret_cast<decltype(x.portamentoTimeMode)>(sym(x.lib,"fluid_synth_set_portamento_time_mode"));
 if(x.portamentoTimeMode)Impl::check(x.portamentoTimeMode(x.synth,2),"SF2 linear portamento time setup failed");
 Impl::check(x.sensitivity(x.synth,0,2),"SF2 pitch wheel setup failed");
 x.configureGlide(0);
 // SF2.01's default CC1 -> vibrato LFO modulator is additive even for
 // SoundFonts which use CC1 for filter/other destinations. Remove only
 // that default; embedded SF2 modulators and CC1 messages stay untouched.
 // Public API flags: CC | linear | unipolar | positive = 0x10; destination 6
 // (vibLfoToPitch), as specified by SoundFont 2.01.
 if(auto* mod=x.newMod()){
  x.modSource1(mod,1,0x10);
  x.modSource2(mod,0,0);
  x.modDest(mod,6);
  x.removeDefaultMod(x.synth,mod);
  x.deleteMod(mod);
 }

}
Sf2Engine::~Sf2Engine()=default;
void Sf2Engine::load(const std::filesystem::path& path){
 if(!std::filesystem::is_regular_file(path))throw std::runtime_error("SF2 missing");
 // Load into a fresh engine per instrument swap; never load in the ASIO callback.
 auto name=std::filesystem::absolute(path).u8string();
 int font=impl->sfload(impl->synth,name.c_str(),0);
 if(font<0)throw std::runtime_error("SF2 loading failed");
 impl->font=font;
}
std::vector<Sf2Preset> Sf2Engine::presets() const{
 std::vector<Sf2Preset> result;if(!loaded())return result;
 auto& x=*impl;auto* f=x.getFont(x.synth,x.font);if(!f)return result;
 x.iterStart(f);while(auto* p=x.iterNext(f)){auto* n=x.name(p);result.push_back({x.bank(p),x.program(p),n?n:"Unnamed"});}
 return result;
}
void Sf2Engine::select(int bank,int program){
 if(!loaded()||bank<0||bank>16383||program<0||program>127||impl->select(impl->synth,0,impl->font,bank,program)!=0)
 throw std::runtime_error("SF2 preset not found");
}
void Sf2Engine::noteOn(int n,int v){if(!loaded()||n<0||n>127||v<0||v>127)return;if(!v){noteOff(n);return;}if(impl->keys[n])impl->off(impl->synth,0,n);impl->on(impl->synth,0,n,v);impl->keys[n]=true;}
void Sf2Engine::noteOff(int n){if(loaded()&&n>=0&&n<128){impl->off(impl->synth,0,n);impl->keys[n]=false;}}
void Sf2Engine::cc(int n,int v){
 if(!loaded())return;auto& x=*impl;
 // The host's knob owns native portamento. MIDI Learn still updates the host parameter.
 // Keep CC1, sustain (CC64), pitch bend and SoundFont-specific modulators intact.
 if(n>=124&&n<=127){x.clearHeldNotes();x.cc(x.synth,0,123,0);return;}
 if(n==65||n==68||n==84||((n==5||n==37)&&x.glideAmount>0))return;
 if(n==120||n==123)x.clearHeldNotes();
 x.cc(x.synth,0,n,v);
 if(n==121){x.cc(x.synth,0,65,x.glideAmount>0?127:0);x.glideMs=-1;x.configureGlide(x.glideAmount);}
}
void Sf2Engine::pitchBend(int v){if(loaded())impl->bend(impl->synth,0,std::clamp(v,0,16383));}
void Sf2Engine::panic(){
 auto& x=*impl;if(loaded()){
  x.clearHeldNotes();x.cc(x.synth,0,123,0);x.silence(x.synth,0);
  // Clear the previous-note source so the first note after panic starts in tune.
  x.cc(x.synth,0,65,0);if(x.glideAmount>0)x.cc(x.synth,0,65,127);x.bend(x.synth,0,8192);
 }x.dsp.reset();
}
void Sf2Engine::parameters(const std::array<float,15>& values,bool enabled){
 impl->dsp.parameters(values);
 impl->configureGlide(values[14]);
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
void Sf2Engine::smoothing(bool enabled){impl->dsp.smoothing(enabled);}
void Sf2Engine::render(float* l,float* r,int frames){
 if(!loaded()){std::fill_n(l,frames,0);std::fill_n(r,frames,0);return;}
 // Native FluidSynth voice DSP performs the pitch sweep internally; no shared bend ramp.
 if(impl->write(impl->synth,frames,l,0,1,r,0,1)!=0)throw std::runtime_error("SF2 render failed");
 impl->dsp.process(l,r,frames);
}
bool Sf2Engine::loaded() const noexcept{return impl->font>=0;}
}

// Test double: validates host/ABI behavior; never ships as the real sound engine.
#include <cstring>
#include <array>
#include <cstdio>
#include "../third_party/sfizz/sfizz_message.h"
#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C"
#endif
struct sfizz_synth_t {int regions=0;bool playing=false;std::array<std::array<float,4>,3> envelope{{{{.01f,.2f,75.f,.3f}},{{.02f,.4f,50.f,.6f}},{{.03f,.8f,25.f,.9f}}}};};
struct sfizz_client_t {void* user;sfizz_receive_t* receive=nullptr;};
EXPORT sfizz_client_t* sfizz_create_client(void* user){return new sfizz_client_t{user};}
EXPORT void sfizz_delete_client(sfizz_client_t* c){delete c;}
EXPORT void sfizz_set_receive_callback(sfizz_client_t* c,sfizz_receive_t* callback){c->receive=callback;}
EXPORT void sfizz_send_message(sfizz_synth_t* p,sfizz_client_t* c,int,const char* path,const char* signature,const sfizz_arg_t* args){
 int region=-1;char name[32]{};if(std::sscanf(path,"/region%d/%31s",&region,name)!=2||region<0||region>=p->regions)return;
 const char* names[]{"ampeg_attack","ampeg_decay","ampeg_sustain","ampeg_release"};
 for(int k=0;k<4;++k)if(std::strcmp(name,names[k])==0){
  if(std::strcmp(signature,"f")==0)p->envelope[region][k]=args[0].f;
  else if(c->receive){sfizz_arg_t arg{};arg.f=p->envelope[region][k];c->receive(c->user,0,path,"f",&arg);}
 }
}
EXPORT sfizz_synth_t* sfizz_create_synth(){return new sfizz_synth_t;}
EXPORT void sfizz_free(sfizz_synth_t* p){delete p;}
EXPORT bool sfizz_load_file(sfizz_synth_t* p,const char* path){
 if(std::strstr(path,"broken"))return false;
 p->regions=std::strstr(path,"empty")?0:3;return true;
}
EXPORT void sfizz_set_sample_rate(sfizz_synth_t*,float){}
EXPORT void sfizz_set_samples_per_block(sfizz_synth_t*,int){}
EXPORT void sfizz_send_note_on(sfizz_synth_t* p,int,int,int){p->playing=true;}
EXPORT void sfizz_send_note_off(sfizz_synth_t* p,int,int,int){p->playing=false;}
EXPORT void sfizz_send_cc(sfizz_synth_t* p,int,int cc,int){if(cc==120||cc==123)p->playing=false;}
EXPORT void sfizz_enable_freewheeling(sfizz_synth_t*){}
EXPORT void sfizz_all_sound_off(sfizz_synth_t* p){p->playing=false;}
EXPORT int sfizz_get_num_regions(sfizz_synth_t* p){return p->regions;}
EXPORT void sfizz_render_block(sfizz_synth_t* p,float** channels,int count,int frames){
 for(int c=0;c<count;++c)for(int i=0;i<frames;++i)channels[c][i]=p->playing?0.25f:0;
}

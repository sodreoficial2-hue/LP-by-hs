// Test double: validates host/ABI behavior; never ships as the real sound engine.
#include <cstring>
#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C"
#endif
struct sfizz_synth_t {int regions=0;bool playing=false;};
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
EXPORT void sfizz_all_sound_off(sfizz_synth_t* p){p->playing=false;}
EXPORT int sfizz_get_num_regions(sfizz_synth_t* p){return p->regions;}
EXPORT void sfizz_render_block(sfizz_synth_t* p,float** channels,int count,int frames){
 for(int c=0;c<count;++c)for(int i=0;i<frames;++i)channels[c][i]=p->playing?0.25f:0;
}

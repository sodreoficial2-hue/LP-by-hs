// Test double only. Production links real PortAudio with ASIO enabled.
#include "portaudio.h"
#include "pa_asio.h"
#include <stdexcept>
static PaStreamCallback* renderCallback=nullptr;static void* context=nullptr;
static bool active=false;static int selected=-1;static int starts=0;static bool failNext=false;
static PaHostApiInfo asio{1,paASIO,"ASIO",3,-1,1},wasapi{1,paWASAPI,"WASAPI",1,-1,0};
static PaDeviceInfo windows{2,"Windows output",1,0,2,0,0,0,.01,48000},driver{2,"ASIO4ALL",0,0,2,0,0,0,.004,48000},mono{2,"Mono ASIO",0,0,1,0,0,0,.004,48000};
static PaDeviceInfo tascam{2,"TASCAM US-1800",0,0,2,0,0,0,.004,48000};
extern "C" {
PaError Pa_Initialize(){return 0;}PaError Pa_Terminate(){return 0;}
PaDeviceIndex Pa_GetDeviceCount(){return 4;}
const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex id){return id==0?&windows:id==1?&driver:id==2?&mono:id==3?&tascam:nullptr;}
const PaHostApiInfo* Pa_GetHostApiInfo(PaHostApiIndex id){return id==0?&asio:id==1?&wasapi:nullptr;}
const char* Pa_GetErrorText(PaError){return "simulated driver failure";}
PaError Pa_IsFormatSupported(const PaStreamParameters*,const PaStreamParameters* out,double rate){return (out->device==1||out->device==3)&&rate==48000?0:paInvalidDevice;}
PaError Pa_OpenStream(PaStream** stream,const PaStreamParameters*,const PaStreamParameters* out,double,unsigned long,PaStreamFlags,PaStreamCallback* cb,void* user){
 if(failNext){failNext=false;return paDeviceUnavailable;}
 selected=out->device;renderCallback=cb;context=user;*stream=reinterpret_cast<PaStream*>(1);return 0;
}
PaError Pa_StartStream(PaStream*){active=true;++starts;return 0;}
PaError Pa_StopStream(PaStream*){active=false;return 0;}
PaError Pa_AbortStream(PaStream*){active=false;return 0;}
PaError Pa_CloseStream(PaStream*){active=false;renderCallback=nullptr;context=nullptr;return 0;}
const PaStreamInfo* Pa_GetStreamInfo(PaStream*){static PaStreamInfo info{1,0,.004,48000};return &info;}
PaError PaAsio_ShowControlPanel(PaDeviceIndex id,void*){return id==1||id==3?0:paInvalidDevice;}
}
int fakePump(float* left,float* right,unsigned long frames,PaStreamCallbackFlags flags=0){if(!active||!renderCallback)throw std::runtime_error("No callback running");float* out[]{left,right};return renderCallback(nullptr,out,frames,nullptr,flags,context);}
void fakeFailOpen(){failNext=true;}int fakeStarts(){return starts;}int fakeDevice(){return selected;}

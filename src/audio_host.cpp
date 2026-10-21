#include "audio_host.hpp"
#include "pa_asio.h"
#include <algorithm>
#include <stdexcept>
namespace lp {
namespace {
void paCheck(PaError result){if(result<0)throw std::runtime_error(std::string("ASIO: ")+Pa_GetErrorText(result));}
void requireAsio(int device){
 auto info=Pa_GetDeviceInfo(device);auto api=info?Pa_GetHostApiInfo(info->hostApi):nullptr;
 if(!info||!api||api->type!=paASIO||info->maxOutputChannels<2)throw std::runtime_error("Select a stereo ASIO driver; no Windows audio fallback is enabled");
}
}
AudioHost::AudioHost(const std::filesystem::path& sfizz):library(sfizz){
 static_assert(std::atomic<float>::is_always_lock_free,"Audio parameters must be lock free");
 for(std::size_t i=0;i<values.size();++i)values[i].store(current[i]);
 engine=std::make_unique<SfizzEngine>(library);
 fluidLibrary=library.parent_path()/"libfluidsynth-3.dll";
 paCheck(Pa_Initialize());
}
AudioHost::~AudioHost(){
 // Pa_CloseStream synchronizes callback completion before synth destruction.
 if(stream){Pa_AbortStream(stream);Pa_CloseStream(stream);stream=nullptr;}
 Pa_Terminate();
}
std::vector<AudioDevice> AudioHost::devices() const {
 auto count=Pa_GetDeviceCount();paCheck(count);std::vector<AudioDevice> result;
 for(int i=0;i<count;++i){auto info=Pa_GetDeviceInfo(i);auto api=info?Pa_GetHostApiInfo(info->hostApi):nullptr;
  if(info&&api&&api->type==paASIO&&info->maxOutputChannels>=2)result.push_back({i,info->name});}
 return result;
}
void AudioHost::open(int id,unsigned long size){
 requireAsio(id);auto info=Pa_GetDeviceInfo(id);
 PaStreamParameters out{};out.device=id;out.channelCount=2;out.sampleFormat=paFloat32|paNonInterleaved;
 out.suggestedLatency=info->defaultLowOutputLatency;
 paCheck(Pa_IsFormatSupported(nullptr,&out,48000));
 PaStream* candidate=nullptr;paCheck(Pa_OpenStream(&candidate,nullptr,&out,48000,size,paNoFlag,callback,this));
 restorePerformance();
 auto result=Pa_StartStream(candidate);
 if(result<0){Pa_CloseStream(candidate);paCheck(result);}
 stream=candidate;device=id;buffer=size;
}
void AudioHost::start(int id,unsigned long size){
 requireAsio(id);
 if(size!=0&&(size<16||size>8192))throw std::invalid_argument("Invalid ASIO buffer (16..8192 or driver default)");
 auto oldDevice=device;auto oldBuffer=buffer;bool restart=running();stop();
 try{open(id,size);}catch(...){if(restart){try{open(oldDevice,oldBuffer);}catch(...){fault.store(true);}}throw;}
}
void AudioHost::stop(){
 if(stream){auto old=stream;auto stopped=Pa_StopStream(old);if(stopped<0)Pa_AbortStream(old);
  auto closed=Pa_CloseStream(old);if(closed<0){fault.store(true);paCheck(closed);}stream=nullptr;}
 clearQueue();engine->panic();if(sf2)sf2->panic();
}
void AudioHost::controlPanel(int id,void* window){
 requireAsio(id);bool restart=running();auto oldDevice=device;auto oldBuffer=buffer;stop();
 auto result=PaAsio_ShowControlPanel(id,window);
 if(restart)open(oldDevice,oldBuffer);
 paCheck(result);
}
void AudioHost::load(const std::filesystem::path& path,const std::array<float,14>& parameters,bool enabled){
 auto candidate=std::make_unique<SfizzEngine>(library);candidate->load(path);candidate->parameters(parameters,enabled);
 bool restart=running();auto id=device;auto size=buffer;stop();
 auto previousInstrument=instrument;auto previousParameters=current;bool previousAdsr=currentAdsr;
 engine.swap(candidate);sf2Active=false;restorePerformance();instrument=path;this->parameters(parameters,enabled);
 if(restart){try{open(id,size);}catch(...){engine.swap(candidate);sf2Active=false;instrument=previousInstrument;this->parameters(previousParameters,previousAdsr);try{open(id,size);}catch(...){fault.store(true);}throw;}}
}
void AudioHost::loadSf2(const std::filesystem::path& path,int bank,int program,const std::array<float,14>& params){
 auto candidate=std::make_unique<Sf2Engine>(fluidLibrary);candidate->load(path);candidate->select(bank,program);
 bool restart=running();auto id=device;auto size=buffer;stop();
 auto old=std::move(sf2);bool wasSf2=sf2Active;auto oldPath=instrument;
 sf2=std::move(candidate);sf2Active=true;instrument=path;this->parameters(params,false);restorePerformance();
 if(restart){try{open(id,size);}catch(...){sf2=std::move(old);sf2Active=wasSf2;instrument=oldPath;try{open(id,size);}catch(...){fault.store(true);}throw;}}
}
std::vector<Sf2Preset> AudioHost::sf2Presets(const std::filesystem::path& path) const{
 Sf2Engine temporary(fluidLibrary);temporary.load(path);return temporary.presets();
}
void AudioHost::unload(){bool restart=running();auto id=device;auto size=buffer;stop();engine->unload();sf2.reset();sf2Active=false;instrument.clear();if(restart)open(id,size);}
void AudioHost::parameters(const std::array<float,14>& params,bool enabled){
 for(float v:params)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid audio parameter");
 revision.fetch_add(1,std::memory_order_seq_cst);
 desiredAdsr.store(enabled,std::memory_order_seq_cst);
 for(std::size_t i=0;i<params.size();++i)values[i].store(params[i],std::memory_order_seq_cst);
 revision.fetch_add(1,std::memory_order_seq_cst);
}
void AudioHost::resetPerformance() noexcept {modulation.store(0);sustain.store(0);bend.store(0);bendSeen.store(true);}
void AudioHost::restorePerformance(){auto mod=modulation.load(),pedal=sustain.load();if(mod>=0)(sf2Active?sf2->cc(1,mod):engine->cc(1,mod));if(pedal>=0)(sf2Active?sf2->cc(64,pedal):engine->cc(64,pedal));if(bendSeen.load())(sf2Active?sf2->pitchBend(bend.load()+8192):engine->pitchBend(bend.load()));}
void AudioHost::send(MidiMessage message) noexcept {
 if(!message.valid())return;
 auto kind=message.status&0xf0;
 if(kind==0xe0){bend.store(message.pitchBend());bendSeen.store(true);}
 if(kind==0xb0){if(message.data1==1)modulation.store(message.data2);if(message.data1==64)sustain.store(message.data2);if(message.data1==121)resetPerformance();}
 if(stream)midi.push(message);
}
void AudioHost::panic() noexcept {resetPerformance();panicRequested.store(true,std::memory_order_release);}
void AudioHost::clearQueue() noexcept {MidiMessage m;for(int i=0;i<1024&&midi.pop(m);++i){}midi.takeOverflow();panicRequested.store(false);}
double AudioHost::latency() const noexcept {auto info=stream?Pa_GetStreamInfo(stream):nullptr;return info?info->outputLatency:0;}
int AudioHost::callback(const void*,void* output,unsigned long frames,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags flags,void* user) noexcept {
 return static_cast<AudioHost*>(user)->render(static_cast<float**>(output),frames,flags);
}
int AudioHost::render(float** output,unsigned long frames,PaStreamCallbackFlags flags) noexcept {
 if(!output||!output[0]||!output[1]){fault.store(true);return paAbort;}
 if(flags&paOutputUnderflow)underruns.fetch_add(1);
 try{
  unsigned before=revision.load(std::memory_order_seq_cst);
  if(!(before&1)&&before!=applied){std::array<float,14> snapshot;for(std::size_t i=0;i<snapshot.size();++i)snapshot[i]=values[i].load(std::memory_order_seq_cst);
   bool enabled=desiredAdsr.load(std::memory_order_seq_cst);
   if(before==revision.load(std::memory_order_seq_cst)){if(!sf2Active)engine->parameters(snapshot,enabled);current=snapshot;currentAdsr=enabled;applied=before;}}
  if(midi.takeOverflow()||panicRequested.exchange(false)){
   MidiMessage ignored;for(int i=0;i<1024&&midi.pop(ignored);++i){}if(sf2Active)sf2->panic();else engine->panic();resetPerformance();restorePerformance();
  }else{
   MidiMessage m;for(int i=0;i<1024&&midi.pop(m);++i){auto kind=m.status&0xf0;
    if(kind==0x90)(sf2Active?sf2->noteOn(m.data1,m.data2):engine->noteOn(m.data1,m.data2));else if(kind==0x80)(sf2Active?sf2->noteOff(m.data1):engine->noteOff(m.data1,m.data2));else if(kind==0xb0){(sf2Active?sf2->cc(m.data1,m.data2):engine->cc(m.data1,m.data2));if(m.data1==121){(sf2Active?sf2->cc(1,0):engine->cc(1,0));(sf2Active?sf2->cc(64,0):engine->cc(64,0));(sf2Active?sf2->pitchBend(8192):engine->pitchBend(0));}}else if(kind==0xe0)(sf2Active?sf2->pitchBend(m.pitchBend()+8192):engine->pitchBend(m.pitchBend()));}
  }
  for(unsigned long i=0;i<frames;){int size=static_cast<int>(std::min<unsigned long>(256,frames-i));if(sf2Active)sf2->render(output[0]+i,output[1]+i,size);else engine->render(output[0]+i,output[1]+i,size);i+=size;}
  return paContinue;
 }catch(...){std::fill_n(output[0],frames,0);std::fill_n(output[1],frames,0);fault.store(true);return paContinue;}
}
}

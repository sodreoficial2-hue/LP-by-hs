#include "audio_host.hpp"
#include "pa_asio.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>
namespace lp {
namespace {
void paCheck(PaError result){if(result<0)throw std::runtime_error(std::string("ASIO: ")+Pa_GetErrorText(result));}
void requireAsio(int device){
 auto info=Pa_GetDeviceInfo(device);auto api=info?Pa_GetHostApiInfo(info->hostApi):nullptr;
 if(!info||!api||api->type!=paASIO||info->maxOutputChannels<2)throw std::runtime_error("Selecione um driver ASIO com saída estéreo");
}
void validParameters(const std::array<float,15>& p){for(float v:p)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid audio parameter");}
}
AudioHost::AudioHost(const std::filesystem::path& sfizz):library(sfizz){
 static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<Graph*>::is_always_lock_free,"Audio handoff must be lock free");
 for(std::size_t i=0;i<values.size();++i)values[i].store(current[i]);
 for(auto& p:partControls)for(std::size_t i=0;i<15;++i)p.values[i].store(current[i]);
 fluidLibrary=library.parent_path()/"libfluidsynth-3.dll";
 // Fail early for an unavailable engine, before initializing PortAudio.
 auto probe=std::make_unique<SfizzEngine>(library);
 paCheck(Pa_Initialize());
}
AudioHost::~AudioHost(){
 if(stream){Pa_AbortStream(stream);Pa_CloseStream(stream);stream=nullptr;}
 destroyGraphs();Pa_Terminate();
}
void AudioHost::retire(Graph* graph) noexcept {
 if(!graph)return;
 auto head=retired.load(std::memory_order_relaxed);
 do{graph->next=head;}while(!retired.compare_exchange_weak(head,graph,std::memory_order_release,std::memory_order_relaxed));
}
void AudioHost::collectRetired() noexcept {
 auto graph=retired.exchange(nullptr,std::memory_order_acquire);
 while(graph){auto next=graph->next;delete graph;graph=next;}
}
void AudioHost::destroyGraphs() noexcept {
 delete pending.exchange(nullptr);delete active;active=nullptr;
 for(auto& tail:tails){delete tail;tail=nullptr;}
 collectRetired();
}
std::vector<AudioDevice> AudioHost::devices() const {
 auto count=Pa_GetDeviceCount();paCheck(count);std::vector<AudioDevice> result;
 for(int i=0;i<count;++i){auto info=Pa_GetDeviceInfo(i);auto api=info?Pa_GetHostApiInfo(info->hostApi):nullptr;
  if(info&&api&&api->type==paASIO&&info->maxOutputChannels>=2)result.push_back({i,info->name});}
 return result;
}
std::unique_ptr<AudioHost::Graph> AudioHost::prepare(double sampleRate){
 auto result=std::make_unique<Graph>();result->layered=activePerformance;result->performance=performance;
 for(std::size_t i=0;i<4;++i){
  if(activePerformance&&!performance.parts[i].enabled)continue;
  if(!activePerformance&&(i||instrument.empty()))continue;
  auto ref=activePerformance?parseSf2Reference(performance.parts[i].instrument):std::optional<Sf2Reference>{};
  auto path=activePerformance?std::filesystem::u8path(performance.parts[i].instrument):instrument;
  const auto reference=ref?makeSf2Reference(ref->path,ref->bank,ref->program):
   (!activePerformance&&sf2Active?makeSf2Reference(path,sf2Bank,sf2Program):path.u8string());
  if(activePerformance&&preparedParts[i]&&preparedParts[i]->reference==reference&&preparedParts[i]->sampleRate==sampleRate){
   result->voices[i]=preparedParts[i];continue;
  }
  result->voices[i]=std::make_shared<PartSynth>();auto& voice=*result->voices[i];voice.reference=reference;voice.sampleRate=sampleRate;
  auto calibrated=levelGains.find(levelKey(reference));
  if(calibrated!=levelGains.end())voice.levelGain=calibrated->second;
  const auto& params=activePerformance?performance.parts[i].patch.parameters:current;
  bool adsr=activePerformance?performance.parts[i].patch.adsrEnabled:currentAdsr;
  if(ref||(!activePerformance&&sf2Active)){
   voice.sf2=std::make_unique<Sf2Engine>(fluidLibrary,static_cast<int>(sampleRate));
   voice.sf2->load(ref?ref->path:path);voice.sf2->select(ref?ref->bank:sf2Bank,ref?ref->program:sf2Program);voice.sf2->parameters(params,adsr);
  }else{voice.sfz=std::make_unique<SfizzEngine>(library,static_cast<float>(sampleRate));voice.sfz->load(path);voice.sfz->parameters(params,adsr);}
  result->applied[i]=activePerformance?partControls[i].revision.load():revision.load();
 }
 return result;
}
void AudioHost::publish(std::unique_ptr<Graph> graph){
 preparedParts=graph->layered?graph->voices:decltype(preparedParts){};
 collectRetired();
 if(running())delete pending.exchange(graph.release(),std::memory_order_acq_rel);
 else{destroyGraphs();active=graph.release();if(active)controls(*active);}
}
void AudioHost::open(int id,unsigned long size){
 requireAsio(id);auto info=Pa_GetDeviceInfo(id);
 PaStreamParameters out{};out.device=id;out.channelCount=2;out.sampleFormat=paFloat32|paNonInterleaved;out.suggestedLatency=info->defaultLowOutputLatency;
 const double rates[]{info->defaultSampleRate,48000.,44100.,96000.,88200.};double negotiated=0;
 for(double value:rates)if(value>=8000&&value<=384000&&Pa_IsFormatSupported(nullptr,&out,value)==paFormatIsSupported){negotiated=value;break;}
 if(!negotiated)throw std::runtime_error("ASIO: nenhuma frequência compatível informada pelo driver");
 auto graph=prepare(negotiated);PaStream* candidate=nullptr;
 paCheck(Pa_OpenStream(&candidate,nullptr,&out,negotiated,size,paNoFlag,callback,this));
 // The previous callback has been joined by stop(). Retain its graph until the
 // new stream has started, so a failed driver start can be rolled back safely.
 auto previous=active;active=graph.release();auto oldRate=rate;rate=negotiated;
 try{controls(*active);paCheck(Pa_StartStream(candidate));preparedParts=active->layered?active->voices:decltype(preparedParts){};}
 catch(...){Pa_CloseStream(candidate);delete active;active=previous;rate=oldRate;throw;}
 delete previous;stream=candidate;acceptingMidi.store(true);device=id;buffer=size;
}
void AudioHost::start(int id,unsigned long size){
 requireAsio(id);if(size!=0&&(size<16||size>8192))throw std::invalid_argument("Invalid ASIO buffer");
 auto oldDevice=device;auto oldBuffer=buffer;bool restart=running();stop();
 try{open(id,size);}catch(...){if(restart){try{open(oldDevice,oldBuffer);}catch(...){fault.store(true);}}throw;}
}
void AudioHost::stop(){
 acceptingMidi.store(false);
 if(stream){auto old=stream;auto stopped=Pa_StopStream(old);if(stopped<0)Pa_AbortStream(old);paCheck(Pa_CloseStream(old));stream=nullptr;}
 clearQueue();if(active)silence(*active);
 for(auto& tail:tails){delete tail;tail=nullptr;}delete pending.exchange(nullptr);collectRetired();
}
void AudioHost::controlPanel(int id,void* window){requireAsio(id);bool restart=running();auto oldDevice=device;auto oldBuffer=buffer;stop();auto result=PaAsio_ShowControlPanel(id,window);if(restart)open(oldDevice,oldBuffer);paCheck(result);}
void AudioHost::load(const std::filesystem::path& path,const std::array<float,15>& params,bool adsr){
 validParameters(params);auto oldPath=instrument;auto oldParams=current;bool oldAdsr=currentAdsr,oldPerformance=activePerformance,oldSf2=sf2Active;
 instrument=path;current=params;currentAdsr=adsr;activePerformance=false;sf2Active=false;
 try{auto graph=prepare(rate);parameters(params,adsr);publish(std::move(graph));}
 catch(...){instrument=oldPath;current=oldParams;currentAdsr=oldAdsr;activePerformance=oldPerformance;sf2Active=oldSf2;throw;}
}
void AudioHost::loadSf2(const std::filesystem::path& path,int bank,int program,const std::array<float,15>& params,bool adsr){
 validParameters(params);auto oldPath=instrument;auto oldParams=current;auto oldBank=sf2Bank,oldProgram=sf2Program;bool oldAdsr=currentAdsr,oldPerformance=activePerformance,oldSf2=sf2Active;
 instrument=path;current=params;currentAdsr=adsr;activePerformance=false;sf2Active=true;sf2Bank=bank;sf2Program=program;
 try{auto graph=prepare(rate);parameters(params,adsr);publish(std::move(graph));}
 catch(...){instrument=oldPath;current=oldParams;currentAdsr=oldAdsr;activePerformance=oldPerformance;sf2Active=oldSf2;sf2Bank=oldBank;sf2Program=oldProgram;throw;}
}
std::vector<Sf2Preset> AudioHost::sf2Presets(const std::filesystem::path& path)const{Sf2Engine temporary(fluidLibrary);temporary.load(path);return temporary.presets();}
void AudioHost::setLevelCompensations(const std::map<std::string,float>& gains){
 if(running()||loaded())throw std::logic_error("Apply level calibration before playback");
 for(const auto& pair:gains)if(pair.first.empty()||!std::isfinite(pair.second)||pair.second<=0||pair.second>4)throw std::invalid_argument("Invalid calibrated gain");
 levelGains=gains;
}
void AudioHost::unload(){instrument.clear();activePerformance=false;auto graph=std::make_unique<Graph>();graph->hardSwitch=true;publish(std::move(graph));}
void AudioHost::loadPerformance(const Performance& selected){
 if(!selected.valid())throw std::invalid_argument("Invalid Performance");
 for(const auto& p:selected.parts)if(p.enabled)validParameters(p.patch.parameters);
 auto previous=performance;bool wasPerformance=activePerformance;auto previousPath=instrument;
 performance=selected;activePerformance=true;
 for(const auto& p:selected.parts)if(p.enabled){instrument=std::filesystem::u8path(p.instrument);break;}
 try{
  auto graph=prepare(rate);
  for(std::size_t i=0;i<4;++i){auto& control=partControls[i];control.revision.fetch_add(1);for(std::size_t j=0;j<15;++j)control.values[j].store(selected.parts[i].patch.parameters[j]);control.adsr.store(selected.parts[i].patch.adsrEnabled);control.revision.fetch_add(1);graph->applied[i]=0;}
  publish(std::move(graph));
 }catch(...){performance=previous;activePerformance=wasPerformance;instrument=previousPath;throw;}
}
void AudioHost::setPerformancePartParameters(std::size_t i,const std::array<float,15>& params,bool adsr){
 if(!activePerformance||i>=4||!performance.parts[i].enabled)throw std::invalid_argument("Inactive Performance part");validParameters(params);
 performance.parts[i].patch.parameters=params;performance.parts[i].patch.adsrEnabled=adsr;
 auto& control=partControls[i];control.revision.fetch_add(1);for(std::size_t j=0;j<15;++j)control.values[j].store(params[j]);control.adsr.store(adsr);control.revision.fetch_add(1);
}
void AudioHost::parameters(const std::array<float,15>& params,bool adsr){
 validParameters(params);current=params;currentAdsr=adsr;revision.fetch_add(1);desiredAdsr.store(adsr);for(std::size_t i=0;i<15;++i)values[i].store(params[i]);revision.fetch_add(1);
}
void AudioHost::resetPerformance()noexcept{modulation.store(0);sustain.store(0);bend.store(0);bendSeen.store(true);}
void AudioHost::controls(Graph& graph){
 if(modulation.load()>=0)event(graph,{0xb0,1,static_cast<std::uint8_t>(modulation.load())});
 if(sustain.load()>=0)event(graph,{0xb0,64,static_cast<std::uint8_t>(sustain.load())});
 if(bendSeen.load()){auto value=bend.load()+8192;event(graph,{0xe0,static_cast<std::uint8_t>(value&127),static_cast<std::uint8_t>(value>>7)});}
}
void AudioHost::silence(Graph& graph)noexcept{for(auto& voice:graph.voices){if(!voice)continue;auto& v=*voice;if(v.sf2)v.sf2->panic();if(v.sfz)v.sfz->panic();}}
void AudioHost::event(Graph& graph,MidiMessage m,bool tail){
 auto kind=m.status&0xf0;
 // Retiring voices receive releases and controllers, never new note-ons.
 if(tail&&kind==0x90&&m.data2)return;
 for(std::size_t i=0;i<4;++i){if(!graph.voices[i])continue;auto& v=*graph.voices[i];
  if(kind==0x90||kind==0x80){int note=graph.layered?graph.performance.noteFor(i,m.data1):m.data1;if(note<0)continue;
   if(kind==0x90&&m.data2){if(v.sf2)v.sf2->noteOn(note,m.data2);else v.sfz->noteOn(note,m.data2);}
   else{if(v.sf2)v.sf2->noteOff(note);else v.sfz->noteOff(note,m.data2);}
  }else if(kind==0xb0){if(v.sf2)v.sf2->cc(m.data1,m.data2);else v.sfz->cc(m.data1,m.data2);}
  else if(kind==0xe0){if(v.sf2)v.sf2->pitchBend(m.pitchBend()+8192);else v.sfz->pitchBend(m.pitchBend());}
 }
}
void AudioHost::send(MidiMessage m)noexcept{
 if(!m.valid())return;auto kind=m.status&0xf0;
 if(kind==0xe0){bend.store(m.pitchBend());bendSeen.store(true);}if(kind==0xb0){if(m.data1==1)modulation.store(m.data2);if(m.data1==64)sustain.store(m.data2);if(m.data1==121)resetPerformance();}
 if(acceptingMidi.load())midi.push(m);
}
void AudioHost::sendRealtime(MidiMessage m)noexcept{
 if(!m.valid())return;auto kind=m.status&0xf0;
 if(kind==0xe0){bend.store(m.pitchBend());bendSeen.store(true);}if(kind==0xb0){if(m.data1==1)modulation.store(m.data2);if(m.data1==64)sustain.store(m.data2);if(m.data1==121)resetPerformance();}
 if(acceptingMidi.load())hardwareMidi.push(m);
}
void AudioHost::panic()noexcept{resetPerformance();panicRequested.store(true);}
void AudioHost::clearQueue()noexcept{MidiMessage m;for(int i=0;i<1024&&midi.pop(m);++i){}midi.takeOverflow();for(int i=0;i<1024&&hardwareMidi.pop(m);++i){}hardwareMidi.takeOverflow();panicRequested.store(false);}
double AudioHost::latency()const noexcept{auto info=stream?Pa_GetStreamInfo(stream):nullptr;return info?info->outputLatency:0;}
void AudioHost::renderGraph(Graph& graph,float* left,float* right,int frames){
 std::fill_n(left,frames,0);std::fill_n(right,frames,0);
 for(auto& voice:graph.voices){if(!voice)continue;auto& v=*voice;
  if(v.sf2)v.sf2->render(scratchLeft.data(),scratchRight.data(),frames);else v.sfz->render(scratchLeft.data(),scratchRight.data(),frames);
  for(int j=0;j<frames;++j){left[j]+=scratchLeft[j]*v.levelGain;right[j]+=scratchRight[j]*v.levelGain;}
 }
}
int AudioHost::callback(const void*,void* output,unsigned long frames,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags flags,void* user)noexcept{return static_cast<AudioHost*>(user)->render(static_cast<float**>(output),frames,flags);}
int AudioHost::render(float** output,unsigned long frames,PaStreamCallbackFlags flags)noexcept{
 if(!output||!output[0]||!output[1]){fault.store(true);return paAbort;}if(flags&paOutputUnderflow){underruns.fetch_add(1);for(auto& tail:tails){retire(tail);tail=nullptr;}}
 try{
  if(auto next=pending.exchange(nullptr,std::memory_order_acq_rel)){
   // Two bounded retiring graphs cap CPU/RAM during rapid sound changes.
   if(active&&active->layered&&next->layered)for(std::size_t i=0;i<4;++i){
    const auto& a=active->performance.parts[i];const auto& b=next->performance.parts[i];
    if(next->voices[i]&&next->voices[i]==active->voices[i]&&(active->performance.mode!=next->performance.mode||a.low!=b.low||a.high!=b.high||a.transpose!=b.transpose)){
     auto& voice=*next->voices[i];if(voice.sf2)voice.sf2->panic();else voice.sfz->panic();
    }
   }
   if(next->hardSwitch||next->layered||(active&&active->layered)){
    retire(active);for(auto& tail:tails){retire(tail);tail=nullptr;}
   }else{retire(tails[1]);tails[1]=tails[0];tailFrames[1]=tailFrames[0];tails[0]=active;tailFrames[0]=0;}
   active=next;controls(*active);
  }
  if(active)for(std::size_t i=0;i<4;++i){if(!active->voices[i])continue;auto& v=*active->voices[i];
   auto& rev=active->layered?partControls[i].revision:revision;unsigned before=rev.load();if((before&1)||before==active->applied[i])continue;
   std::array<float,15> params;for(std::size_t j=0;j<15;++j)params[j]=active->layered?partControls[i].values[j].load():values[j].load();bool adsr=active->layered?partControls[i].adsr.load():desiredAdsr.load();
   if(before==rev.load()){if(v.sf2)v.sf2->parameters(params,adsr);else v.sfz->parameters(params,adsr);active->applied[i]=before;}
  }
  const bool overflow=midi.takeOverflow()|hardwareMidi.takeOverflow();
  if(overflow||panicRequested.exchange(false)){
   MidiMessage ignored;for(int i=0;i<1024&&midi.pop(ignored);++i){}for(int i=0;i<1024&&hardwareMidi.pop(ignored);++i){}if(active)silence(*active);for(auto& t:tails){retire(t);t=nullptr;}resetPerformance();if(active)controls(*active);
  }else{MidiMessage m;for(auto queue:{&hardwareMidi,&midi})for(int i=0;i<1024&&queue->pop(m);++i){if(active)event(*active,m);for(auto tail:tails)if(tail)event(*tail,m,true);}}
  float peak=0,gain=master.load();
  for(unsigned long offset=0;offset<frames;){int size=static_cast<int>(std::min<unsigned long>(256,frames-offset));
   if(active)renderGraph(*active,output[0]+offset,output[1]+offset,size);else{std::fill_n(output[0]+offset,size,0);std::fill_n(output[1]+offset,size,0);}
   for(std::size_t t=0;t<tails.size();++t)if(tails[t]){
    renderGraph(*tails[t],tailLeft.data(),tailRight.data(),size);
    for(int j=0;j<size;++j){float fade=std::max(0.f,1.f-static_cast<float>((tailFrames[t]+j)/(rate*2.0)));output[0][offset+j]+=tailLeft[j]*fade;output[1][offset+j]+=tailRight[j]*fade;}
    tailFrames[t]+=size;if(tailFrames[t]>=rate*2){retire(tails[t]);tails[t]=nullptr;}
   }
   for(int j=0;j<size;++j)for(int c=0;c<2;++c){auto& sample=output[c][offset+j];sample*=gain;if(!std::isfinite(sample)){sample=0;meterClip.store(true);}peak=std::max(peak,std::abs(sample));}
   offset+=size;
  }
  auto prior=meterPeak.load();while(prior<peak&&!meterPeak.compare_exchange_weak(prior,peak)){}if(peak>=1)meterClip.store(true);
  return paContinue;
 }catch(...){std::fill_n(output[0],frames,0);std::fill_n(output[1],frames,0);fault.store(true);return paContinue;}
}
}

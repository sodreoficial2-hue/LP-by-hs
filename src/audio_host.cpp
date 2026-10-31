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
void validParameters(const std::array<float,16>& p){for(float v:p)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid audio parameter");}
}
AudioHost::AudioHost(const std::filesystem::path& sfizz):library(sfizz){
 static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<Graph*>::is_always_lock_free,"Audio handoff must be lock free");
 for(std::size_t i=0;i<values.size();++i)values[i].store(current[i]);
 for(auto& p:partControls)for(std::size_t i=0;i<16;++i)p.values[i].store(current[i]);
 fluidLibrary=library.parent_path()/"libfluidsynth-3.dll";
 // Fail early for an unavailable engine, before initializing PortAudio.
 auto probe=std::make_unique<SfizzEngine>(library);
 paCheck(Pa_Initialize());
}
AudioHost::~AudioHost(){
 {std::lock_guard<std::mutex> lock(loadMutex);loadStopping=true;loadJob.reset();}loadWake.notify_one();if(loadThread.joinable())loadThread.join();
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
 while(graph){auto next=graph->next;dispose(std::unique_ptr<Graph>(graph));graph=next;}
}
void AudioHost::destroyGraphs() noexcept {
 delete pending.exchange(nullptr);delete waiting;waiting=nullptr;delete active;active=nullptr;
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
 auto result=std::make_unique<Graph>();result->generation=requestedGeneration.load();result->layered=activePerformance;result->performance=performance;
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
  if(voice.sf2)voice.sf2->smoothing(smoothTransitions);else voice.sfz->smoothing(smoothTransitions);
  result->applied[i]=activePerformance?partControls[i].revision.load():revision.load();
 }
 return result;
}
void AudioHost::publish(std::unique_ptr<Graph> graph){
 publishedGeneration=graph->generation;
 for(int i=0;i<4;++i)if(preparedParts[i]&&preparedParts[i]!=graph->voices[i]&&preparedParts[i]->sf2){
  auto sf2=parseSf2Reference(preparedParts[i]->reference);std::error_code ec;auto bytes=sf2?std::filesystem::file_size(sf2->path,ec):0;
  if(!ec&&bytes&&bytes<=8*1024*1024)warmParts.push_back({i,preparedParts[i]});
 }
 while(warmParts.size()>4){auto discarded=std::make_unique<Graph>();discarded->voices[0]=std::move(warmParts.front().second);warmParts.erase(warmParts.begin());dispose(std::move(discarded));}
 preparedParts=graph->voices;
 collectRetired();
 if(running())dispose(std::unique_ptr<Graph>(pending.exchange(graph.release(),std::memory_order_acq_rel)));
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
 publishedGeneration=active->generation;delete previous;stream=candidate;acceptingMidi.store(true);device=id;buffer=size;
}
void AudioHost::start(int id,unsigned long size){
 requireAsio(id);if(size!=0&&(size<16||size>8192))throw std::invalid_argument("Invalid ASIO buffer");
 auto oldDevice=device;auto oldBuffer=buffer;bool restart=running();stop();
 try{open(id,size);}catch(...){if(restart){try{open(oldDevice,oldBuffer);}catch(...){fault.store(true);}}throw;}
}
void AudioHost::stop(){
 cancelPerformanceLoad();
 acceptingMidi.store(false);
 if(stream){auto old=stream;auto stopped=Pa_StopStream(old);if(stopped<0)Pa_AbortStream(old);paCheck(Pa_CloseStream(old));stream=nullptr;}
 clearQueue();if(active)silence(*active);
 for(auto& tail:tails){delete tail;tail=nullptr;}delete pending.exchange(nullptr);delete waiting;waiting=nullptr;collectRetired();
}
void AudioHost::controlPanel(int id,void* window){requireAsio(id);bool restart=running();auto oldDevice=device;auto oldBuffer=buffer;stop();auto result=PaAsio_ShowControlPanel(id,window);if(restart)open(oldDevice,oldBuffer);paCheck(result);}
void AudioHost::load(const std::filesystem::path& path,const std::array<float,16>& params,bool adsr){
 cancelPerformanceLoad();
 validParameters(params);auto oldPath=instrument;auto oldParams=current;bool oldAdsr=currentAdsr,oldPerformance=activePerformance,oldSf2=sf2Active;
 instrument=path;current=params;currentAdsr=adsr;activePerformance=false;sf2Active=false;
 try{auto graph=prepare(rate);parameters(params,adsr);publish(std::move(graph));}
 catch(...){instrument=oldPath;current=oldParams;currentAdsr=oldAdsr;activePerformance=oldPerformance;sf2Active=oldSf2;throw;}
}
void AudioHost::loadSf2(const std::filesystem::path& path,int bank,int program,const std::array<float,16>& params,bool adsr){
 cancelPerformanceLoad();
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
void AudioHost::releaseResources(){
 stop();{std::lock_guard<std::mutex> lock(loadMutex);loadStopping=true;loadJob.reset();}loadWake.notify_one();if(loadThread.joinable())loadThread.join();
 {std::lock_guard<std::mutex> lock(loadMutex);loadResult.reset();garbage.clear();loadError.clear();loadStopping=false;}
 destroyGraphs();preparedParts={};warmParts.clear();requestedControls={};instrument.clear();activePerformance=false;midiBarrier.store(false);liveSynths.store(0);
}
void AudioHost::unload(){cancelPerformanceLoad();instrument.clear();activePerformance=false;auto graph=std::make_unique<Graph>();graph->generation=loadGeneration;requestedGeneration.store(loadGeneration);graph->hardSwitch=true;publish(std::move(graph));}
void AudioHost::loadPerformance(const Performance& selected){
 cancelPerformanceLoad();
 if(!selected.valid())throw std::invalid_argument("Invalid Performance");
 for(const auto& p:selected.parts)if(p.enabled)validParameters(p.patch.parameters);
 auto previous=performance;bool wasPerformance=activePerformance;auto previousPath=instrument;
 performance=selected;activePerformance=true;
 for(const auto& p:selected.parts)if(p.enabled){instrument=std::filesystem::u8path(p.instrument);break;}
 try{
  auto graph=prepare(rate);
  for(std::size_t i=0;i<4;++i){auto& control=partControls[i];control.revision.fetch_add(1);for(std::size_t j=0;j<16;++j)control.values[j].store(selected.parts[i].patch.parameters[j]);control.adsr.store(selected.parts[i].patch.adsrEnabled);control.revision.fetch_add(1);graph->applied[i]=0;}
  publish(std::move(graph));
 }catch(...){performance=previous;activePerformance=wasPerformance;instrument=previousPath;throw;}
}
void AudioHost::restoreModel(){if(!rollback)return;auto old=std::move(*rollback);rollback.reset();instrument=std::move(old.instrument);performance=std::move(old.performance);current=old.parameters;currentAdsr=old.adsr;activePerformance=old.layered;sf2Active=old.sf2;sf2Bank=old.bank;sf2Program=old.program;requestedControls=std::move(old.controls);requestedGeneration.store(old.generation);}
void AudioHost::cancelPerformanceLoad(){
 restoreModel();
 ++loadGeneration;requestedGeneration.store(publishedGeneration);asyncLoading=false;requestedControls={};std::lock_guard<std::mutex> lock(loadMutex);loadJob.reset();if(loadResult)garbage.push_back(std::move(loadResult));loadError.clear();loadWake.notify_one();
}
std::unique_ptr<AudioHost::Graph> AudioHost::preparePerformance(const LoadJob& job){
 auto graph=std::make_unique<Graph>();graph->layered=job.layered;graph->performance=job.selected;graph->controls=job.controls;graph->generation=job.id;
 for(std::size_t i=0;i<4;++i){const auto& part=job.selected.parts[i];if(!part.enabled)continue;
  auto cached=job.cache[i];if(!cached||cached->reference!=part.instrument)for(auto it=job.warm.rbegin();it!=job.warm.rend();++it)if(it->first==static_cast<int>(i)&&it->second->reference==part.instrument){cached=it->second;break;}if(cached&&cached->reference==part.instrument&&cached->sampleRate==job.sampleRate){graph->voices[i]=cached;continue;}
  auto voice=std::make_shared<PartSynth>();voice->reference=part.instrument;voice->sampleRate=job.sampleRate;
  auto gain=job.gains.find(levelKey(part.instrument));if(gain!=job.gains.end())voice->levelGain=gain->second;
  if(auto sf2=parseSf2Reference(part.instrument)){voice->sf2=std::make_unique<Sf2Engine>(fluidLibrary,static_cast<int>(job.sampleRate));voice->sf2->load(sf2->path);voice->sf2->select(sf2->bank,sf2->program);voice->sf2->parameters(part.patch.parameters,part.patch.adsrEnabled);}
  else{voice->sfz=std::make_unique<SfizzEngine>(library,static_cast<float>(job.sampleRate));voice->sfz->load(std::filesystem::u8path(part.instrument));voice->sfz->parameters(part.patch.parameters,part.patch.adsrEnabled);}
  if(voice->sf2)voice->sf2->smoothing(smoothTransitions);else voice->sfz->smoothing(smoothTransitions);
  graph->voices[i]=std::move(voice);
 }
 return graph;
}
void AudioHost::dispose(std::unique_ptr<Graph> graph)noexcept{try{
 if(!graph)return;if(!loadThread.joinable()){graph.reset();return;}
 {std::lock_guard<std::mutex> lock(loadMutex);garbage.push_back(std::move(graph));}loadWake.notify_one();
 }catch(...){graph.reset();}
}
void AudioHost::loadWorker(){
 for(;;){std::optional<LoadJob> job;std::vector<std::unique_ptr<Graph>> discarded;
  {std::unique_lock<std::mutex> lock(loadMutex);loadWake.wait(lock,[&]{return loadStopping||loadJob.has_value()||!garbage.empty();});discarded.swap(garbage);if(loadStopping)return;if(loadJob){job=std::move(loadJob);loadJob.reset();}}
  discarded.clear();if(!job)continue;
  std::unique_ptr<Graph> graph;std::string error;
  try{graph=preparePerformance(*job);}catch(const std::exception& e){error=e.what();}catch(...){error="Falha ao preparar som";}
  bool notify=false;
  {std::lock_guard<std::mutex> lock(loadMutex);if(job->id==requestedGeneration.load()){if(loadResult)garbage.push_back(std::move(loadResult));loadResult=std::move(graph);loadError=std::move(error);resultGeneration=job->id;notify=true;}}
  // Post a completion event; the UI never waits for a periodic meter refresh.
  if(notify&&loadNotification)try{loadNotification();}catch(...){}
 }
}
void AudioHost::requestGraph(const Performance& selected,bool layered){
 if(!selected.valid())throw std::invalid_argument("Invalid sound arrangement");for(const auto& p:selected.parts)if(p.enabled)validParameters(p.patch.parameters);
 if(!asyncLoading)rollback=Model{instrument,performance,current,currentAdsr,activePerformance,sf2Active,sf2Bank,sf2Program,requestedControls,publishedGeneration};
 if(!loadThread.joinable())loadThread=std::thread([this]{loadWorker();});
 std::array<std::shared_ptr<PartControl>,4> controls;
 for(std::size_t i=0;i<4;++i){controls[i]=std::make_shared<PartControl>();for(std::size_t j=0;j<16;++j)controls[i]->values[j].store(selected.parts[i].patch.parameters[j]);controls[i]->adsr.store(selected.parts[i].patch.adsrEnabled);}
 auto id=++loadGeneration;requestedGeneration.store(id);LoadJob job{id,selected,layered,controls,rate,levelGains,preparedParts,warmParts};
 performance=selected;activePerformance=layered;requestedControls=controls;for(const auto& part:selected.parts)if(part.enabled){instrument=std::filesystem::u8path(part.instrument);break;}
 if(!layered){auto ref=parseSf2Reference(selected.parts[0].instrument);sf2Active=ref.has_value();if(ref){sf2Bank=ref->bank;sf2Program=ref->program;instrument=ref->path;}current=selected.parts[0].patch.parameters;currentAdsr=selected.parts[0].patch.adsrEnabled;}
 {std::lock_guard<std::mutex> lock(loadMutex);loadJob=std::move(job);if(loadResult)garbage.push_back(std::move(loadResult));loadError.clear();asyncLoading=true;}loadWake.notify_one();
}
void AudioHost::requestPerformance(const Performance& selected){requestGraph(selected,true);}
void AudioHost::requestSound(const std::string& reference,const std::array<float,16>& params,bool adsr){
 Performance sound;sound.name="Timbre";sound.parts[0].enabled=true;sound.parts[0].instrument=reference;sound.parts[0].patch.parameters=params;sound.parts[0].patch.adsrEnabled=adsr;requestGraph(sound,false);
}
bool AudioHost::pollPerformance(){
 std::unique_ptr<Graph> graph;std::string error;
 {std::lock_guard<std::mutex> lock(loadMutex);if(resultGeneration!=loadGeneration||(!loadResult&&loadError.empty()))return false;graph=std::move(loadResult);error=std::move(loadError);loadError.clear();}
 asyncLoading=false;if(!error.empty()){restoreModel();if(midiBarrier.load())panic();throw std::runtime_error(error);}publish(std::move(graph));rollback.reset();return true;
}
void AudioHost::setPerformancePartParameters(std::size_t i,const std::array<float,16>& params,bool adsr){
 if(!activePerformance||i>=4||!performance.parts[i].enabled)throw std::invalid_argument("Inactive Performance part");validParameters(params);
 performance.parts[i].patch.parameters=params;performance.parts[i].patch.adsrEnabled=adsr;
 auto& control=requestedControls[i]?*requestedControls[i]:partControls[i];control.revision.fetch_add(1);for(std::size_t j=0;j<16;++j)control.values[j].store(params[j]);control.adsr.store(adsr);control.revision.fetch_add(1);
}
void AudioHost::parameters(const std::array<float,16>& params,bool adsr){
 validParameters(params);current=params;currentAdsr=adsr;if(requestedControls[0]&&!activePerformance){auto& c=*requestedControls[0];c.revision.fetch_add(1);for(std::size_t j=0;j<16;++j)c.values[j].store(params[j]);c.adsr.store(adsr);c.revision.fetch_add(1);return;}revision.fetch_add(1);desiredAdsr.store(adsr);for(std::size_t i=0;i<16;++i)values[i].store(params[i]);revision.fetch_add(1);
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
  if(!(graph.renderMask&(1u<<i)))continue;
  if(kind==0x90||kind==0x80){int note=graph.layered?graph.performance.noteFor(i,m.data1):m.data1;if(note<0)continue;
   if(kind==0x90&&m.data2){if(graph.layered&&!(graph.audible&(1u<<i)))continue;if(v.sf2)v.sf2->noteOn(note,m.data2);else v.sfz->noteOn(note,m.data2);}
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
 for(std::size_t i=0;i<4;++i){auto& voice=graph.voices[i];if(!voice||!(graph.renderMask&(1u<<i))||(graph.layered&&!(graph.audible&(1u<<i))))continue;auto& v=*voice;
  if(v.sf2)v.sf2->render(scratchLeft.data(),scratchRight.data(),frames);else v.sfz->render(scratchLeft.data(),scratchRight.data(),frames);
  for(int j=0;j<frames;++j){left[j]+=scratchLeft[j]*v.levelGain;right[j]+=scratchRight[j]*v.levelGain;}
 }
}
unsigned AudioHost::graphVoices(const Graph* graph)const noexcept{
 unsigned n=0;if(graph)for(std::size_t i=0;i<4;++i)if(graph->voices[i]&&(graph->renderMask&(1u<<i))&&(!graph->layered||(graph->audible&(1u<<i))))++n;return n;
}
bool AudioHost::handoffReady(const Graph& next)noexcept{
 if(next.hardSwitch||!smoothTransitions)return true;
 auto shared=[&](const std::shared_ptr<PartSynth>& voice){for(const auto& candidate:next.voices)if(voice==candidate)return true;return false;};
 auto outgoing=[&](const Graph* graph){unsigned count=0;if(graph)for(std::size_t i=0;i<4;++i)if(graph->voices[i]&&(graph->renderMask&(1u<<i))&&!shared(graph->voices[i]))++count;return count;};
 auto shorten=[&](int t){tailStep[t]=std::max(tailStep[t],tailGain[t]/std::max(1.f,static_cast<float>(rate*.020)));};
 bool ready=true;
 // Finish the tail before cleaning/reusing its cached engine, avoiding a cut.
 for(int t=0;t<2;++t)if(tails[t])for(std::size_t i=0;i<4;++i)if(tails[t]->voices[i]&&(tails[t]->renderMask&(1u<<i))&&shared(tails[t]->voices[i])){shorten(t);ready=false;break;}
 unsigned cost=graphVoices(&next)+outgoing(active)+outgoing(tails[0])+outgoing(tails[1]);
 bool slot=outgoing(active)&&tails[0]&&tails[1];
 for(int t=1;t>=0&&(slot||cost>8);--t)if(tails[t]){shorten(t);cost-=outgoing(tails[t]);slot=false;ready=false;}
 return ready;
}
int AudioHost::callback(const void*,void* output,unsigned long frames,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags flags,void* user)noexcept{return static_cast<AudioHost*>(user)->render(static_cast<float**>(output),frames,flags);}
int AudioHost::render(float** output,unsigned long frames,PaStreamCallbackFlags flags)noexcept{
 auto started=std::chrono::steady_clock::now();lastFrames.store(frames);
 if(!output||!output[0]||!output[1]){fault.store(true);return paAbort;}if(flags&paOutputUnderflow){underruns.fetch_add(1);for(auto& tail:tails){retire(tail);tail=nullptr;}}
 try{
  if(auto newest=pending.exchange(nullptr,std::memory_order_acq_rel)){retire(waiting);waiting=newest;}
  if(waiting&&handoffReady(*waiting)){
   auto next=waiting;waiting=nullptr;
   // A cached synth may still hold notes/DSP from an earlier graph. Only
   // unchanged, continuously active parts retain their live state. All other
   // recalls start clean, on this callback thread (never on the loader).
   for(std::size_t i=0;i<4;++i){auto voice=next->voices[i];if(!voice)continue;
    bool continuous=active&&active->voices[i]==voice&&(active->renderMask&(1u<<i));
    if(continuous){const auto& a=active->performance.parts[i];const auto& b=next->performance.parts[i];
     continuous=active->layered==next->layered&&(!next->layered||(active->performance.mode==next->performance.mode&&a.low==b.low&&a.high==b.high&&a.transpose==b.transpose));}
    // Render shared engines exactly once; never render the same synth as a tail.
    for(auto graph:{active,tails[0],tails[1]})if(graph)for(std::size_t j=0;j<4;++j)if(graph->voices[j]==voice)graph->renderMask&=~(1u<<j);
    if(!continuous){if(voice->sf2)voice->sf2->panic();else voice->sfz->panic();}
    next->applied[i]=0; // Recalled parameters must also replace the cached values.
   }
   if(next->hardSwitch){
    retire(active);for(auto& tail:tails){retire(tail);tail=nullptr;}
   }else{
    if(active&&active->renderMask){
     retire(tails[1]);tails[1]=tails[0];tailGain[1]=tailGain[0];tailStep[1]=tailStep[0];
     tails[0]=active;tailGain[0]=1;tailStep[0]=1/static_cast<float>(rate*2);
    }else retire(active);
   }
   // Legacy/test mode retains immediate handoff. Production waits for the
   // oldest fade to finish, so no rendered graph is cut to meet the budget.
   if(!smoothTransitions){unsigned count=graphVoices(next)+graphVoices(tails[0])+graphVoices(tails[1]);for(int t=1;t>=0&&count>8;--t)if(tails[t]){count-=graphVoices(tails[t]);retire(tails[t]);tails[t]=nullptr;}}
   activeMix=(tails[0]||tails[1])?0:1;
   if(smoothTransitions){transitionFrom=lastOutput;transitionTotal=transitionFrames=std::max(1,static_cast<int>(rate*.003));}
   active=next;controls(*active);
  }
  if(active&&active->layered){auto mask=auditionMask.load();for(std::size_t i=0;i<4;++i)if((active->audible&~mask)&(1u<<i)){auto& v=active->voices[i];if(v){if(v->sf2)v->sf2->panic();else v->sfz->panic();}}active->audible=mask;}
  if(active)for(std::size_t i=0;i<4;++i){if(!active->voices[i])continue;auto& v=*active->voices[i];
   auto state=active->controls[i].get();auto& rev=state?state->revision:(active->layered?partControls[i].revision:revision);unsigned before=rev.load();if((before&1)||before==active->applied[i])continue;
   std::array<float,16> params;for(std::size_t j=0;j<16;++j)params[j]=state?state->values[j].load():(active->layered?partControls[i].values[j].load():values[j].load());bool adsr=state?state->adsr.load():(active->layered?partControls[i].adsr.load():desiredAdsr.load());
   if(before==rev.load()){if(v.sf2)v.sf2->parameters(params,adsr);else v.sfz->parameters(params,adsr);active->applied[i]=before;}
  }
  const bool overflow=midi.takeOverflow()|hardwareMidi.takeOverflow();
  if(overflow||panicRequested.exchange(false)){
   MidiMessage ignored;for(int i=0;i<1024&&midi.pop(ignored);++i){}for(int i=0;i<1024&&hardwareMidi.pop(ignored);++i){}if(active)silence(*active);for(auto& t:tails){retire(t);t=nullptr;}resetPerformance();if(active)controls(*active);
  }else{if(midiBarrier.load()&&midiAcknowledged.load()==midiTicket.load()&&active&&active->generation==requestedGeneration.load())midiBarrier.store(false);MidiMessage m;for(auto queue:{&hardwareMidi,&midi})if(queue!=&hardwareMidi||!midiBarrier.load())for(int i=0;i<1024&&queue->pop(m);++i){if(active)event(*active,m);for(auto tail:tails)if(tail)event(*tail,m,true);}}
  float peak=0,gain=master.load();
  for(unsigned long offset=0;offset<frames;){int size=static_cast<int>(std::min<unsigned long>(256,frames-offset));
   if(active)renderGraph(*active,output[0]+offset,output[1]+offset,size);else{std::fill_n(output[0]+offset,size,0);std::fill_n(output[1]+offset,size,0);}
   std::fill_n(mixLeft.data(),size,0);std::fill_n(mixRight.data(),size,0);std::fill_n(mixWeights.data(),size,0);
   const bool overlapping=tails[0]||tails[1];bool activeAudible=false;
   for(int j=0;j<size;++j)if(std::abs(output[0][offset+j])+std::abs(output[1][offset+j])>1e-9f){activeAudible=true;break;}
   for(std::size_t t=0;t<tails.size();++t)if(tails[t]){
    renderGraph(*tails[t],tailLeft.data(),tailRight.data(),size);bool audible=false;
    for(int j=0;j<size;++j)if(std::abs(tailLeft[j])+std::abs(tailRight[j])>1e-9f){audible=true;break;}
    for(int j=0;j<size;++j){float fade=std::max(0.f,tailGain[t]-tailStep[t]*j);mixLeft[j]+=tailLeft[j]*fade;mixRight[j]+=tailRight[j]*fade;if(audible)mixWeights[j]+=fade;}
    tailGain[t]=std::max(0.f,tailGain[t]-tailStep[t]*size);if(tailGain[t]<=1e-6f){retire(tails[t]);tails[t]=nullptr;}
   }
   for(int j=0;j<size;++j){
    if(smoothTransitions&&overlapping){float step=1/static_cast<float>(rate*.020);activeMix=std::clamp(activeMix+(activeAudible?step:-step),0.f,1.f);
     float weight=mixWeights[j]>0?activeMix:1,normal=std::max(1.f,weight+mixWeights[j]);
     output[0][offset+j]=(output[0][offset+j]*weight+mixLeft[j])/normal;output[1][offset+j]=(output[1][offset+j]*weight+mixRight[j])/normal;
    }else{output[0][offset+j]+=mixLeft[j];output[1][offset+j]+=mixRight[j];if(!overlapping)activeMix=1;}
   }
   for(int j=0;j<size;++j){float smoothing=1-std::exp(-1.f/(static_cast<float>(rate)*.003f));if(smoothTransitions)masterSmoothed+=smoothing*(gain-masterSmoothed);else masterSmoothed=gain;for(int c=0;c<2;++c){auto& sample=output[c][offset+j];sample*=masterSmoothed;if(transitionFrames){float alpha=1-static_cast<float>(transitionFrames)/transitionTotal;sample=transitionFrom[c]*(1-alpha)+sample*alpha;}lastOutput[c]=sample;if(!std::isfinite(sample)){sample=0;meterClip.store(true);}peak=std::max(peak,std::abs(sample));}if(transitionFrames)--transitionFrames;}
   offset+=size;
  }
  auto prior=meterPeak.load();while(prior<peak&&!meterPeak.compare_exchange_weak(prior,peak)){}if(peak>=1)meterClip.store(true);
  liveSynths.store(graphVoices(active)+graphVoices(tails[0])+graphVoices(tails[1]));
  auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();if(frames)renderLoad.store(static_cast<float>(elapsed*rate/frames));
  return paContinue;
 }catch(...){std::fill_n(output[0],frames,0);std::fill_n(output[1],frames,0);fault.store(true);return paContinue;}
}
}

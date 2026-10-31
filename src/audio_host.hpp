#pragma once
#include "sfizz_engine.hpp"
#include "sf2_engine.hpp"
#include "midi.hpp"
#include "performance.hpp"
#include "sf2_reference.hpp"
#include "level_balance.hpp"
#include "portaudio.h"
#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>
#include <chrono>
namespace lp {
struct AudioDevice {int id;std::string name;};
// All public calls on UI thread; callback exclusively owns the active synth
// while stream runs. SFZ preparation and destruction never run in callback.
class AudioHost {
public:
 explicit AudioHost(const std::filesystem::path& sfizz);
 ~AudioHost();
 AudioHost(const AudioHost&)=delete;
 AudioHost& operator=(const AudioHost&)=delete;
 std::vector<AudioDevice> devices() const;
 void start(int device,unsigned long buffer=0);
 void stop();
 void controlPanel(int device,void* window);
 void load(const std::filesystem::path& sfz,const std::array<float,16>& parameters,bool adsrEnabled=false);
 void loadSf2(const std::filesystem::path& sf2,int bank,int program,const std::array<float,16>& parameters,bool adsrEnabled=false);
 std::vector<Sf2Preset> sf2Presets(const std::filesystem::path& sf2) const;
 // Owner thread, before loading/starting playback. Graphs snapshot fixed gains.
 void setLevelCompensations(const std::map<std::string,float>& gains);
 void unload();
 void releaseResources(); // Explicit destructive-file operation: join loader after stopping audio.
 void loadPerformance(const Performance& performance);
 void requestPerformance(const Performance& performance);
 void requestSound(const std::string& reference,const std::array<float,16>& parameters,bool adsr=false);
 void enableSmoothTransitions(bool enabled)noexcept{smoothTransitions=enabled;masterSmoothed=master.load();}
 void setLoadNotification(std::function<void()> notify){loadNotification=std::move(notify);}
 unsigned beginMidiSwitch()noexcept{midiBarrier.store(true);return midiTicket.fetch_add(1)+1;}
 void acknowledgeMidiSwitch(unsigned ticket)noexcept{midiAcknowledged.store(ticket);}
 float callbackLoad()const noexcept{return renderLoad.load();}
 unsigned long callbackFrames()const noexcept{return lastFrames.load();}
 unsigned synthCount()const noexcept{return liveSynths.load();}

 bool pollPerformance(); // UI thread: publish newest completed result; throw newest error.
 bool performanceLoading()const noexcept{return asyncLoading;}
 void setPerformanceAuditionMask(unsigned mask)noexcept{auditionMask.store(mask&15);}

 bool performanceActive() const noexcept {return activePerformance;}
 void setPerformancePartParameters(std::size_t index,const std::array<float,16>& params,bool adsrEnabled);
 void parameters(const std::array<float,16>& values,bool adsrEnabled=false);
 void send(MidiMessage message) noexcept;
 void sendRealtime(MidiMessage message) noexcept; // One dedicated hardware MIDI producer.
 void panic() noexcept;
 void collectRetired() noexcept; // UI thread: release completed transition engines.
 float outputPeak() noexcept {return meterPeak.exchange(0);}
 bool takeOutputClip() noexcept {return meterClip.exchange(false);}
 bool running() const noexcept {return stream!=nullptr;}
 bool loaded() const noexcept {return !instrument.empty();}
 int selectedDevice() const noexcept {return device;}
 unsigned long bufferSize() const noexcept {return buffer;}
 double latency() const noexcept;
 double sampleRate() const noexcept {return rate;}
 void masterVolume(float value){if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Invalid Master");master.store(value);}
 bool takeFault() noexcept {return fault.exchange(false);}
 unsigned long xruns() const noexcept {return underruns.load();}
private:
 friend struct AudioHostGraphProbe; // Deterministic cache-lifecycle regression fixture.
 static int callback(const void*,void*,unsigned long,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags,void*) noexcept;
 int render(float**,unsigned long,PaStreamCallbackFlags) noexcept;
 void open(int,unsigned long);
 void clearQueue() noexcept;
 void resetPerformance() noexcept;
 std::atomic<int> modulation{-1},sustain{-1},bend{0};
 std::atomic<bool> bendSeen{false};
 std::filesystem::path library,instrument,fluidLibrary;
 bool sf2Active=false;int sf2Bank=0,sf2Program=0;
 double rate=48000;std::atomic<float> master{1};
 std::map<std::string,float> levelGains;
 struct PartSynth {std::unique_ptr<SfizzEngine> sfz;std::unique_ptr<Sf2Engine> sf2;float levelGain=1;std::string reference;double sampleRate=0;};
 struct PartControl {std::array<std::atomic<float>,16> values;std::atomic<bool> adsr{false};std::atomic<unsigned> revision{2};};
 struct Graph {
  std::array<std::shared_ptr<PartSynth>,4> voices{};
  Performance performance{};
  bool layered=false,hardSwitch=false;
  std::array<unsigned,4> applied{};
  std::array<std::shared_ptr<PartControl>,4> controls{};
  unsigned long long generation=0;unsigned audible=15,renderMask=15;
  Graph* next=nullptr;
 };
 // UI prepares/publishes; callback exclusively uses active/tails. Completed
 // graphs go to an intrusive lock-free retirement list; no deletion in ASIO.
 std::array<std::shared_ptr<PartSynth>,4> preparedParts{};
 std::vector<std::pair<int,std::shared_ptr<PartSynth>>> warmParts;
 Graph* active=nullptr;Graph* waiting=nullptr;
 std::array<Graph*,2> tails{};
 std::array<float,2> tailGain{},tailStep{};
 float activeMix=1;
 std::atomic<Graph*> pending{nullptr},retired{nullptr};
 std::unique_ptr<Graph> prepare(double sampleRate);
 struct LoadJob {unsigned long long id;Performance selected;bool layered;std::array<std::shared_ptr<PartControl>,4> controls;double sampleRate;std::map<std::string,float> gains;std::array<std::shared_ptr<PartSynth>,4> cache;std::vector<std::pair<int,std::shared_ptr<PartSynth>>> warm;};
 std::mutex loadMutex;std::condition_variable loadWake;std::thread loadThread;
 std::vector<std::unique_ptr<Graph>> garbage;std::function<void()> loadNotification;
 std::array<std::shared_ptr<PartControl>,4> requestedControls{};
 struct Model {std::filesystem::path instrument;Performance performance;std::array<float,16> parameters;bool adsr,layered,sf2;int bank,program;std::array<std::shared_ptr<PartControl>,4> controls;unsigned long long generation;};
 std::optional<Model> rollback;unsigned long long publishedGeneration=0;
 void restoreModel();
 std::optional<LoadJob> loadJob;std::unique_ptr<Graph> loadResult;std::string loadError;
 bool loadStopping=false,asyncLoading=false;unsigned long long loadGeneration=0,resultGeneration=0;
 std::atomic<unsigned> auditionMask{15};
 void cancelPerformanceLoad();void loadWorker();void requestGraph(const Performance&,bool);
 void dispose(std::unique_ptr<Graph> graph)noexcept;
 std::atomic<unsigned long long> requestedGeneration{0};
 std::atomic<bool> midiBarrier{false};std::atomic<unsigned> midiTicket{0},midiAcknowledged{0};
 std::atomic<float> renderLoad{0};std::atomic<unsigned long> lastFrames{0};std::atomic<unsigned> liveSynths{0};
 bool smoothTransitions=false;float masterSmoothed=1;std::array<float,2> lastOutput{},transitionFrom{};int transitionFrames=0,transitionTotal=1;
 std::unique_ptr<Graph> preparePerformance(const LoadJob&);

 bool handoffReady(const Graph&)noexcept;
 unsigned graphVoices(const Graph*)const noexcept;
 void publish(std::unique_ptr<Graph>);
 void retire(Graph*) noexcept;
 void destroyGraphs() noexcept;
 void controls(Graph&);
 void event(Graph&,MidiMessage,bool tail=false);
 void renderGraph(Graph&,float*,float*,int);
 void silence(Graph&) noexcept;
 Performance performance{};
 bool activePerformance=false;
 std::array<float,256> scratchLeft{},scratchRight{},tailLeft{},tailRight{},mixLeft{},mixRight{},mixWeights{};

 std::array<PartControl,4> partControls{};
 std::atomic<float> meterPeak{0};std::atomic<bool> meterClip{false};
 std::atomic<bool> acceptingMidi{false};
 PaStream* stream{};int device=-1;unsigned long buffer=0;
 MidiQueue<> midi,hardwareMidi;
 std::atomic<bool> panicRequested{false},fault{false};
 std::atomic<unsigned long> underruns{0};
 std::array<std::atomic<float>,16> values;
 std::atomic<unsigned> revision{0};
 std::atomic<bool> desiredAdsr{false};bool currentAdsr=false;
 std::array<float,16> current=Patch{}.parameters;
};
}

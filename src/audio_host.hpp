#pragma once
#include "sfizz_engine.hpp"
#include "sf2_engine.hpp"
#include "midi.hpp"
#include "performance.hpp"
#include "sf2_reference.hpp"
#include "portaudio.h"
#include <atomic>
#include <memory>
#include <vector>
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
 void load(const std::filesystem::path& sfz,const std::array<float,15>& parameters,bool adsrEnabled=false);
 void loadSf2(const std::filesystem::path& sf2,int bank,int program,const std::array<float,15>& parameters,bool adsrEnabled=false);
 std::vector<Sf2Preset> sf2Presets(const std::filesystem::path& sf2) const;
 void unload();
 void loadPerformance(const Performance& performance);
 bool performanceActive() const noexcept {return activePerformance;}
 void setPerformancePartParameters(std::size_t index,const std::array<float,15>& params,bool adsrEnabled);
 void parameters(const std::array<float,15>& values,bool adsrEnabled=false);
 void send(MidiMessage message) noexcept;
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
 struct PartSynth {std::unique_ptr<SfizzEngine> sfz;std::unique_ptr<Sf2Engine> sf2;};
 struct Graph {
  std::array<PartSynth,4> voices{};
  Performance performance{};
  bool layered=false;
  std::array<unsigned,4> applied{};
  Graph* next=nullptr;
 };
 // UI prepares/publishes; callback exclusively uses active/tails. Completed
 // graphs go to an intrusive lock-free retirement list; no deletion in ASIO.
 Graph* active=nullptr;
 std::array<Graph*,2> tails{};
 std::array<unsigned long,2> tailFrames{};
 std::atomic<Graph*> pending{nullptr},retired{nullptr};
 std::unique_ptr<Graph> prepare(double sampleRate);
 void publish(std::unique_ptr<Graph>);
 void retire(Graph*) noexcept;
 void destroyGraphs() noexcept;
 void controls(Graph&);
 void event(Graph&,MidiMessage,bool tail=false);
 void renderGraph(Graph&,float*,float*,int);
 void silence(Graph&) noexcept;
 Performance performance{};
 bool activePerformance=false;
 std::array<float,256> scratchLeft{},scratchRight{},tailLeft{},tailRight{};
 struct PartControl {std::array<std::atomic<float>,15> values;std::atomic<bool> adsr{false};std::atomic<unsigned> revision{0};};
 std::array<PartControl,4> partControls{};
 std::atomic<float> meterPeak{0};std::atomic<bool> meterClip{false};
 PaStream* stream{};int device=-1;unsigned long buffer=0;
 MidiQueue<> midi;
 std::atomic<bool> panicRequested{false},fault{false};
 std::atomic<unsigned long> underruns{0};
 std::array<std::atomic<float>,15> values;
 std::atomic<unsigned> revision{0};
 std::atomic<bool> desiredAdsr{false};bool currentAdsr=false;
 std::array<float,15> current=Patch{}.parameters;
};
}

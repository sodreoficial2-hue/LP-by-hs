#pragma once
#include "session.hpp"
#include <array>
#include <vector>

namespace lp {
// Stereo insert processor. Configure/reset on the owner thread while stopped.
// process() allocates no memory; ADSR remains a per-voice sfizz responsibility.
class Dsp {
public:
 explicit Dsp(float sampleRate=48000);
 void parameters(const std::array<float,15>& values);
 void reset() noexcept;
 void smoothing(bool enabled)noexcept{smoothVolume=enabled;volumeSmoothed=p[0];}
 unsigned long long timeEffectFrames()const noexcept{return effectFrames;}
 void process(float* left,float* right,int frames) noexcept;
private:
 struct Biquad {
  float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
  float tick(float x) noexcept;
  void peak(float rate,float frequency,float db);
 };
 struct Comb {std::vector<float> samples;std::size_t cursor=0;float tick(float x) noexcept;};
 // Eight-line orthogonal feedback network; all memory is allocated at setup.
 struct Hall {
  struct Line {std::vector<float> samples;std::size_t at=0;float low=0,gain=0,target=0;};
  struct Diffuser {std::vector<float> samples;std::size_t at=0;float tick(float x) noexcept;};
  std::array<Line,8> lines;
  std::array<Diffuser,4> diffusion;
  std::vector<float> preDelay;std::size_t preAt=0;
  float inputLow=0,hp=0,damping=0,smooth=0;
  void prepare(float rate,int channel);
  void decay(float rate,float seconds);
  void reset() noexcept;
  float tick(float x) noexcept;
 };
 struct FilterState {float integrator1=0,integrator2=0;};
 struct FilterCoefficients {float a1=0,a2=0,a3=0;};
 std::array<FilterCoefficients,512> cutoffTable{};
 std::array<FilterState,2> filter{};
 float attackCoeff=0,releaseCoeff=0,volumeCoeff=0,hallCoeff=0,thresholdValue=.501187234f,ratioValue=1;
 float rate,envelope=0,phase=0,chorusPhase=0;
 float follower=0,followerAttack=0,followerRelease=0;
 std::array<float,15> p=Patch{}.parameters;
 std::array<std::array<Biquad,3>,2> eq;
 std::array<std::vector<float>,2> delay,chorus;
 std::array<std::array<Comb,4>,2> reverb;
 std::array<Hall,2> hall;
 bool smoothVolume=false;float volumeSmoothed=1;
 unsigned long long effectFrames=0;int delayTail=0,chorusTail=0,reverbTail=0;
 float hallMix=0,hallTarget=0;
 std::size_t delayAt=0,chorusAt=0;
};
}

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
 void process(float* left,float* right,int frames) noexcept;
private:
 struct Biquad {
  float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
  float tick(float x) noexcept;
  void peak(float rate,float frequency,float db);
 };
 struct Comb {std::vector<float> samples;std::size_t cursor=0;float tick(float x) noexcept;};
 struct FilterState {float integrator1=0,integrator2=0;};
 struct FilterCoefficients {float a1=0,a2=0,a3=0;};
 std::array<FilterCoefficients,512> cutoffTable{};
 std::array<FilterState,2> filter{};
 float rate,envelope=0,phase=0,chorusPhase=0;
 float follower=0,followerAttack=0,followerRelease=0;
 std::array<float,15> p=Patch{}.parameters;
 std::array<std::array<Biquad,3>,2> eq;
 std::array<std::vector<float>,2> delay,chorus;
 std::array<std::array<Comb,4>,2> reverb;
 std::size_t delayAt=0,chorusAt=0;
};
}

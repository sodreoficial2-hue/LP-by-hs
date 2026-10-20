#pragma once
#include <filesystem>
#include <memory>
#include <cstddef>
#include <array>
namespace lp {
// Owns one sfizz instance. All methods must run on one thread, or while the
// audio callback is stopped. Loading and destruction are not realtime-safe.
class SfizzEngine {
public:
 explicit SfizzEngine(const std::filesystem::path& library, float sampleRate=48000, int maxBlock=256,bool offline=false);
 ~SfizzEngine();
 SfizzEngine(const SfizzEngine&)=delete;
 SfizzEngine& operator=(const SfizzEngine&)=delete;
 void load(const std::filesystem::path& sfz); // Failed load preserves previous synth.
 void noteOn(int note,int velocity);
 void noteOff(int note,int velocity=0);
 void cc(int number,int value);
 void pitchBend(int value); // Signed 14-bit MIDI: -8192..8191, center 0.
 void panic();
 void parameters(const std::array<float,14>& values,bool adsrEnabled=false);
 std::array<float,4> envelope(int region); // Readback: only on owner thread / stopped stream.
 void unload();
 void render(float* left,float* right,int frames);
 bool loaded() const noexcept;
 int regions() const;
private:
 struct Impl;
 std::unique_ptr<Impl> impl;
};
}

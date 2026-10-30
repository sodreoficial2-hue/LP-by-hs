#pragma once
#include <filesystem>
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace lp {
struct Sf2Preset {int bank;int program;std::string name;};
class Sf2Engine {
public:
 explicit Sf2Engine(const std::filesystem::path& library,int sampleRate=48000);
 ~Sf2Engine();
 Sf2Engine(const Sf2Engine&)=delete;
 Sf2Engine& operator=(const Sf2Engine&)=delete;
 void load(const std::filesystem::path& path);
 std::vector<Sf2Preset> presets() const;
 void select(int bank,int program);
 void noteOn(int note,int velocity);
 void noteOff(int note);
 void cc(int controller,int value);
 void pitchBend(int unsigned14Bit);
 void panic();
 void smoothing(bool enabled);
 void parameters(const std::array<float,15>& values,bool adsrEnabled=false);
 void render(float* left,float* right,int frames);
 bool loaded() const noexcept;
private:
 struct Impl;std::unique_ptr<Impl> impl;
};
}

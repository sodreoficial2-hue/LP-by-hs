#pragma once
#include <cstdint>
#include <array>
namespace lp {
enum class Screen { Home, Instruments, PerformanceEditor, AudioMidi, Settings, About };
struct ParameterPanel {
 bool open=false;std::uint64_t lastUse=0;
 void touch(std::uint64_t now){lastUse=now;}
 void toggle(std::uint64_t now){open=!open;touch(now);}
 bool expired(std::uint64_t now) const{return open&&now-lastUse>=30000;}
};
// MIDI convention: C4 = 60. B2 (Si2) is MIDI 47, next zone begins at C3 = 48.
inline std::array<int,2> splitZone(int mode,int part){
 if(mode==1)return part<2?std::array<int,2>{0,47}:std::array<int,2>{48,127};
 if(mode==2)return part<3?std::array<int,2>{0,47}:std::array<int,2>{48,127};
 if(mode==3)return part<2?std::array<int,2>{0,47}:(part==2?std::array<int,2>{48,85}:std::array<int,2>{86,127});
 if(mode==4){constexpr int edges[]{0,48,64,96,128};return {edges[part],edges[part+1]-1};}
 return {0,127};
}
}

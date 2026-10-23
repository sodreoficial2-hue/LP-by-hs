#pragma once
#include <cstdint>
#include <array>
#include <algorithm>
namespace lp {
enum class Screen { Home, Instruments, PerformanceEditor, AudioMidi, Settings, About, Keyboard };
// All sixteen categories fit a single viewport at the minimum supported size.
struct CategoryGrid {
 static constexpr int count=16,columns=4,rows=4,gap=12,margin=24,top=12;
 int cardWidth,cardHeight;
 CategoryGrid(int width,int height):cardWidth((width-2*margin-3*gap)/4),cardHeight((height-2*top-3*gap)/4){}
 std::array<int,4> card(int index) const {return {margin+(index%4)*(cardWidth+gap),top+(index/4)*(cardHeight+gap),cardWidth,cardHeight};}
 static int neighbor(int index,int dx,int dy){int column=std::clamp(index%4+dx,0,3),row=std::clamp(index/4+dy,0,3);return row*4+column;}
};
// Shared bounds for page arrows, scrollbars, wheel and focus visibility.
struct PageScroll {
 int offset=0,viewport=0,content=0;
 int maximum() const {return std::max(0,content-viewport);}
 bool seek(int position){int old=offset;offset=std::clamp(position,0,maximum());return old!=offset;}
 bool move(int pixels){return seek(offset+pixels);}
 void resize(int available,int full){viewport=std::max(1,available);content=std::max(viewport,full);seek(offset);}
 bool reveal(int first,int last){
  if(last-first>viewport)return seek(first);
  if(first<offset)return seek(first);
  if(last>offset+viewport)return seek(last-viewport);
  return false;
 }
};
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

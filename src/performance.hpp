#pragma once
#include "session.hpp"
#include "midi.hpp"
#include <array>
#include <string>
#include <stdexcept>
namespace lp {
enum class PerformanceMode { Layer, Split };
struct PerformancePart {
 bool enabled=false;
 std::string instrument;
 int low=0,high=127;
 int transpose=0;
 Patch patch{};
};
struct Performance {
 std::string name;
 PerformanceMode mode=PerformanceMode::Layer;
 std::array<PerformancePart,4> parts{};
 // 1-4 contiguous keyboard zones, each with one or more assigned parts.
 // Layer mode: all enabled parts span the keyboard. Split mode: each part
 // may share its zone with other parts, allowing 2+2, 3+1 or 2+1+1.
 bool valid() const {
  if(name.empty()||name.size()>128)return false;
  unsigned count=0;
  for(const auto& p:parts)if(p.enabled){
   ++count;if(p.instrument.empty()||p.low<0||p.high>127||p.low>p.high||p.transpose< -48||p.transpose>48)return false;
  }
  return count>0;
 }
 bool receives(std::size_t part,int note) const {
  if(part>=parts.size()||note<0||note>127)return false;
  const auto& p=parts[part];
  return p.enabled && (mode==PerformanceMode::Layer || (note>=p.low&&note<=p.high));
 }
 int noteFor(std::size_t part,int note) const {
  if(!receives(part,note))return -1;
  int transposed=note+parts[part].transpose;
  return transposed>=0&&transposed<=127?transposed:-1;
 }
};
inline SavedPerformance savePerformance(const Performance& p){
 if(!p.valid())throw std::invalid_argument("Invalid Performance");
 SavedPerformance result;result.name=p.name;result.split=p.mode==PerformanceMode::Split;
 for(std::size_t i=0;i<4;++i){const auto& source=p.parts[i];auto& target=result.parts[i];target.enabled=source.enabled;target.instrument=source.instrument;target.low=source.low;target.high=source.high;target.transpose=source.transpose;target.patch=source.patch;}
 return result;
}
inline Performance restorePerformance(const SavedPerformance& p){
 Performance result;result.name=p.name;result.mode=p.split?PerformanceMode::Split:PerformanceMode::Layer;
 for(std::size_t i=0;i<4;++i){const auto& source=p.parts[i];auto& target=result.parts[i];target.enabled=source.enabled;target.instrument=source.instrument;target.low=source.low;target.high=source.high;target.transpose=source.transpose;target.patch=source.patch;}
 if(!result.valid())throw std::runtime_error("Corrupt saved Performance");
 return result;
}
}

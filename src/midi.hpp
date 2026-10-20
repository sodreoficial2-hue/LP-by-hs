#pragma once
#include "session.hpp"
#include <atomic>
#include <cstdint>
namespace lp {
struct MidiMessage {
 std::uint8_t status{},data1{},data2{};
 static MidiMessage packed(std::uint32_t value) {
  return {static_cast<std::uint8_t>(value),static_cast<std::uint8_t>(value>>8),static_cast<std::uint8_t>(value>>16)};
 }
 bool valid() const { return status>=0x80 && status<0xf0 && data1<128 && data2<128; }
};
// One producer, one consumer. Overflow requests panic so lost note-offs cannot
// leave voices stuck. No allocations, file access, or UI calls in this queue.
template<std::size_t Capacity=1024> class MidiQueue {
 static_assert(Capacity>=2);
 std::array<MidiMessage,Capacity> buffer{};
 std::atomic<std::size_t> write{0},read{0};
 std::atomic<bool> overflow{false};
public:
 bool push(MidiMessage event) noexcept {
  auto w=write.load(std::memory_order_relaxed),next=(w+1)%Capacity;
  if(next==read.load(std::memory_order_acquire)) {overflow.store(true,std::memory_order_release);return false;}
  buffer[w]=event;write.store(next,std::memory_order_release);return true;
 }
 bool pop(MidiMessage& event) noexcept {
  auto r=read.load(std::memory_order_relaxed);
  if(r==write.load(std::memory_order_acquire)) return false;
  event=buffer[r];read.store((r+1)%Capacity,std::memory_order_release);return true;
 }
 bool takeOverflow() noexcept { return overflow.exchange(false,std::memory_order_acq_rel); }
};
enum class MidiAction { Ignore,NoteOn,NoteOff,Control,Favorite,Parameter,Learned };
struct MidiResult { MidiAction action=MidiAction::Ignore;int index=0,value=0; };
class MidiRouter {
 Session& session;
 std::optional<std::size_t> learnParameter;
public:
 explicit MidiRouter(Session& s):session(s) {}
 void learn(std::size_t parameter) {
  if(parameter>=parameterNames.size() || !session.current || !session.favorites[*session.current])
   throw std::invalid_argument("Select favorite before MIDI Learn");
  learnParameter=parameter;
 }
 void cancelLearn() {learnParameter.reset();}
 MidiResult route(MidiMessage m) {
  if(!m.valid()) return {};
  auto kind=m.status&0xf0;
  if(kind==0xc0) {
   if(m.data1<20 && session.favorites[m.data1]) return {MidiAction::Favorite,m.data1,0};
   return {};
  }
  if(kind==0xb0 && learnParameter) {
   auto index=*learnParameter;learnParameter.reset();
   if(!session.current || !session.favorites[*session.current]) return {};
   // One CC maps to one knob per favorite, avoiding accidental double edits.
   auto& mapping=session.favorites[*session.current]->cc;
   for(auto& cc:mapping) if(cc==m.data1) cc=-1;
   mapping[index]=m.data1;
   return {MidiAction::Learned,static_cast<int>(index),m.data1};
  }
  if(kind==0xb0 && session.current && session.favorites[*session.current]) {
   auto& p=*session.favorites[*session.current];
   for(std::size_t i=0;i<p.cc.size();++i) if(p.cc[i]==m.data1) {
    session.parameter(i,m.data2/127.0f);return {MidiAction::Parameter,static_cast<int>(i),m.data2};
   }
  }
  if(kind==0x90 && m.data2) return {MidiAction::NoteOn,m.data1,m.data2};
  if(kind==0x80 || kind==0x90) return {MidiAction::NoteOff,m.data1,m.data2};
  if(kind==0xb0) return {MidiAction::Control,m.data1,m.data2};
  return {};
 }
};
}

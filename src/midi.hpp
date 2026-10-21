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
 int pitchBend() const noexcept {return (static_cast<int>(data2)*128+data1)-8192;}
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
// Knob MIDI Learn accepts CC only. Sustain and channel-mode controls remain native.
inline int knobBinding(MidiMessage m){
 return (m.status&0xf0)==0xb0&&m.data1!=64&&m.data1<120?m.data1:-1;
}
inline float knobValue(MidiMessage m){return m.data2/127.f;}
enum class MidiAction { Ignore,NoteOn,NoteOff,Control,PitchBend,Favorite,Parameter,Learned,FavoriteLearned,WrongProgram };
struct MidiResult { MidiAction action=MidiAction::Ignore;int index=0,value=0; };
inline bool forwardsToEngine(const MidiResult& result,MidiMessage message){
 return result.action==MidiAction::NoteOn||result.action==MidiAction::NoteOff||result.action==MidiAction::Control||result.action==MidiAction::PitchBend||(message.valid()&&(message.status&0xf0)==0xb0&&message.data1==1);
}
class MidiRouter {
 Session& session;
 struct ParameterLearn {std::size_t slot,parameter;};
 std::optional<ParameterLearn> learnParameter;
 std::optional<std::size_t> learnSlot;
public:
 explicit MidiRouter(Session& s):session(s) {}
 void learn(std::size_t parameter) {
  if(parameter>=parameterNames.size() || !session.current || !session.favorites[*session.current])
   throw std::invalid_argument("Select favorite before MIDI Learn");
  learnSlot.reset();learnParameter=ParameterLearn{*session.current,parameter};
 }
 void learnFavorite(std::size_t slot) {
  if(slot>=20||!session.favorites[slot])throw std::invalid_argument("Choose occupied favorite for MIDI Learn");
  learnParameter.reset();learnSlot=slot;
 }
 bool learning() const {return learnParameter.has_value()||learnSlot.has_value();}
 std::optional<std::size_t> pendingParameter() const {return learnParameter?std::optional<std::size_t>(learnParameter->parameter):std::nullopt;}
 void cancelLearn() {learnParameter.reset();learnSlot.reset();}
 MidiResult route(MidiMessage m) {
  if(!m.valid()) return {};
  if(learnParameter&&(!session.current||*session.current!=learnParameter->slot||!session.favorites[learnParameter->slot]))learnParameter.reset();
  auto kind=m.status&0xf0;
  if(kind==0xc0) {
   if(learnSlot) {
    auto slot=*learnSlot;
    // Fixed user numbering 1..20 is wire numbering 0..19.
    if(m.data1!=slot)return {MidiAction::WrongProgram,static_cast<int>(slot),m.data1};
    learnSlot.reset();
    if(!session.favorites[slot])return {};
    session.favorites[slot]->button={0xc0,m.status&15,m.data1};
    return {MidiAction::FavoriteLearned,static_cast<int>(slot),m.data1};
   }
   if(m.data1<20 && session.favorites[m.data1]) {
    const auto& binding=session.favorites[m.data1]->button;
    if(binding.kind==0 || binding.channel==(m.status&15)){learnParameter.reset();return {MidiAction::Favorite,m.data1,0};}
   }
   return {};
  }
  // Pedal and channel-mode messages must never be captured by knob Learn.
  if(kind==0xb0&&(m.data1==64||m.data1>=120))return {MidiAction::Control,m.data1,m.data2};

  if(knobBinding(m)>=0 && learnParameter) {
   auto index=learnParameter->parameter;learnParameter.reset();
   if(!session.current || !session.favorites[*session.current]) return {};
   // One CC maps to one knob per favorite, avoiding accidental double edits.
   auto& mapping=session.favorites[*session.current]->cc;
   for(auto& cc:mapping) if(cc==knobBinding(m)) cc=-1;
   mapping[index]=knobBinding(m);
   session.parameter(index,knobValue(m)); // The first movement also controls the sound.
   return {MidiAction::Learned,static_cast<int>(index),knobBinding(m)};
  }
  if(knobBinding(m)>=0 && session.current && session.favorites[*session.current]) {
   auto& p=*session.favorites[*session.current];
   for(std::size_t i=0;i<p.cc.size();++i) if(p.cc[i]==knobBinding(m)) {
    session.parameter(i,knobValue(m));return {MidiAction::Parameter,static_cast<int>(i),m.data2};
   }
  }
  if(kind==0x90 && m.data2) return {MidiAction::NoteOn,m.data1,m.data2};
  if(kind==0x80 || kind==0x90) return {MidiAction::NoteOff,m.data1,m.data2};
  if(kind==0xb0) return {MidiAction::Control,m.data1,m.data2};
  if(kind==0xe0)return {MidiAction::PitchBend,0,m.pitchBend()};
  return {};
 }
};
}

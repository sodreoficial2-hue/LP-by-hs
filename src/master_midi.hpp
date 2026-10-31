#pragma once
#include "midi.hpp"
#include "performance.hpp"
namespace lp {
// Absolute CC knobs/faders on any MIDI channel. Pedal and channel modes
// remain reserved by knobBinding; notes and Program Change never bind here.
inline bool learnMasterControl(Session& show,Patch& preview,Performance& editing,MidiMessage m){
 int binding=knobBinding(m);if(!m.valid()||binding<0)return false;
 auto clear=[&](Patch& patch){for(auto& cc:patch.cc)if(cc==binding)cc=-1;if(patch.performanceMix)for(auto& cc:patch.performanceMix->cc)if(cc==binding)cc=-1;};
 for(auto& patch:show.favorites)if(patch)clear(*patch);clear(preview);
 for(auto& performance:show.performanceLibrary)for(auto& part:performance.parts)clear(part.patch);
 for(auto& part:editing.parts)clear(part.patch);
 show.masterCC=binding;show.master=knobValue(m);return true;
}
inline bool applyMasterControl(Session& show,MidiMessage m){
 if(!m.valid()||knobBinding(m)<0||knobBinding(m)!=show.masterCC)return false;
 show.master=knobValue(m);return true;
}
}

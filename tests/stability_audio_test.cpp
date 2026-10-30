#include "audio_host.hpp"
#include "dsp.hpp"
#include <cassert>
#include <fstream>
#include <thread>
#include <chrono>
int fakePump(float*,float*,unsigned long,PaStreamCallbackFlags=0);int fakeStarts();
void ready(lp::AudioHost& host){auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(6);while(!host.pollPerformance()){assert(std::chrono::steady_clock::now()<limit);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){
 assert(argc==2);auto root=std::filesystem::temp_directory_path()/"lp-stability-audio";std::filesystem::create_directories(root);for(auto name:{"fast.sfz","slow.sfz","broken.sfz"})std::ofstream(root/name)<<"fixture";
 lp::AudioHost host(argv[1]);std::atomic<int> notifications{0};host.setLoadNotification([&]{++notifications;});host.start(1,128);int starts=fakeStarts();float l[256]{},r[256]{};
 lp::Performance p;p.name="Old";p.parts[0].enabled=true;p.parts[0].instrument=(root/"fast.sfz").u8string();host.loadPerformance(p);host.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[0]==.25f);
 auto next=p;next.parts[0].instrument=(root/"slow.sfz").u8string();next.parts[0].patch.parameters[0]=.1f;host.requestPerformance(next);fakePump(l,r,256);assert(l[0]==.25f); // Preparing a new layer must not touch the old live gain.
 auto params=next.parts[0].patch.parameters;params[0]=.3f;host.setPerformancePartParameters(0,params,false);fakePump(l,r,256);assert(l[0]==.25f);ready(host);host.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(std::abs(l[0]-.075f)<1e-6);
 host.panic();fakePump(l,r,256);
 auto ticket=host.beginMidiSwitch();host.requestSound((root/"slow.sfz").u8string(),lp::Patch{}.parameters);host.sendRealtime({0x90,64,100});host.acknowledgeMidiSwitch(ticket);fakePump(l,r,256);assert(l[0]==0);ready(host);fakePump(l,r,256);assert(l[0]==.25f); // First post-PC note waits for the actual new graph.
 host.panic();fakePump(l,r,256);auto first=host.beginMidiSwitch();host.requestSound((root/"fast.sfz").u8string(),lp::Patch{}.parameters);auto last=host.beginMidiSwitch();host.acknowledgeMidiSwitch(first);host.sendRealtime({0x90,67,100});ready(host);fakePump(l,r,256);assert(l[0]==0);host.acknowledgeMidiSwitch(last);fakePump(l,r,256);assert(l[0]==.25f);
 assert(host.callbackFrames()==256&&host.callbackLoad()>=0&&host.synthCount()==1&&fakeStarts()==starts);
 auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(notifications<3&&std::chrono::steady_clock::now()<limit)std::this_thread::yield();assert(notifications>=3);
 // A failed replacement keeps the last working model/controls; queued PC notes are cancelled safely.
 auto failureTicket=host.beginMidiSwitch();host.requestSound((root/"broken.sfz").u8string(),lp::Patch{}.parameters);host.sendRealtime({0x90,72,100});host.acknowledgeMidiSwitch(failureTicket);bool failed=false;try{ready(host);}catch(...){failed=true;}assert(failed&&host.loaded());fakePump(l,r,256);assert(l[0]==0);host.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[0]==.25f);
 // Production smoothing bounds a transition without loading extra synth graphs.
 host.enableSmoothTransitions(true);host.masterVolume(0);fakePump(l,r,256);assert(l[0]>.1f&&l[255]<l[0]);host.unload();fakePump(l,r,256);
 host.releaseResources();assert(!host.loaded()&&!host.running());host.start(1,128);host.requestSound((root/"fast.sfz").u8string(),lp::Patch{}.parameters);ready(host);host.sendRealtime({0x90,60,100});fakePump(l,r,256);assert(l[255]>0);
 lp::Dsp dsp;float a[256]{},b[256]{};for(int i=0;i<256;++i)a[i]=b[i]=.5f;dsp.process(a,b,256);assert(dsp.timeEffectFrames()==0);dsp.smoothing(true);auto v=lp::Patch{}.parameters;v[0]=.2f;dsp.parameters(v);for(int i=0;i<256;++i)a[i]=b[i]=.5f;dsp.process(a,b,256);assert(a[0]>.4f&&a[255]<a[0]);v[6]=1;dsp.parameters(v);dsp.process(a,b,256);assert(dsp.timeEffectFrames()>0);
 std::filesystem::remove_all(root);
}

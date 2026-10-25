// Audio probe: calls the production Sf2Engine and a real FluidSynth library.
#include "sf2_engine.hpp"
#include "session.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>

static void render(lp::Sf2Engine& synth,std::vector<float>& out,float seconds){
 int frames=static_cast<int>(std::lround(seconds*48000));std::vector<float> l(frames),r(frames);synth.render(l.data(),r.data(),frames);out.insert(out.end(),l.begin(),l.end());
}
static void save(const std::filesystem::path& path,const std::vector<float>& data){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size()*sizeof(float));}
int main(int argc,char** argv){try{
 if(argc!=4&&argc!=5)throw std::runtime_error("Expected library, fixture SF2 and output folder");
 const auto run=[&](const char* name){return argc==4||std::string(argv[4])==name;};
 std::filesystem::path output(argv[3]);std::filesystem::create_directories(output);
 struct Case{const char* name;float amount;int from,to,preset;};
 const Case cases[]={{"slow_up",.9f,69,81,0},{"medium_down",.5f,81,69,0},{"fast_up",.15f,69,81,0},{"bend_disabled_font",.9f,69,81,1},{"large_interval",.9f,45,93,0}};
 for(const auto& c:cases){if(!run(c.name))continue;lp::Sf2Engine s(argv[1]);s.load(argv[2]);s.select(0,c.preset);auto p=lp::Patch{}.parameters;p[14]=c.amount;s.parameters(p);std::vector<float> audio;
  s.noteOn(c.from,100);render(s,audio,.25f);s.noteOn(c.to,100);render(s,audio,2.4f);s.noteOff(c.to);render(s,audio,2.2f);s.noteOff(c.from);render(s,audio,.1f);s.panic();render(s,audio,.1f);save(output/(std::string(c.name)+".f32"),audio);
 }
 if(run("staccato_bend_panic")){lp::Sf2Engine s(argv[1]);s.load(argv[2]);s.select(0,0);auto p=lp::Patch{}.parameters;p[14]=.5f;s.parameters(p);std::vector<float>a;
  s.noteOn(69,100);render(s,a,.25f);s.noteOff(69);render(s,a,.03f);s.noteOn(81,100);render(s,a,.8f);s.pitchBend(16383);render(s,a,.2f);s.pitchBend(8192);render(s,a,.2f);s.panic();s.noteOn(69,100);render(s,a,.2f);save(output/"staccato_bend_panic.f32",a);
 }
 if(run("polyphonic_zero")){lp::Sf2Engine s(argv[1]);s.load(argv[2]);s.select(0,0);auto p=lp::Patch{}.parameters;p[14]=0;s.parameters(p);std::vector<float>a;
  s.noteOn(69,100);s.noteOn(76,100);render(s,a,.3f);s.panic();p[14]=.7f;s.parameters(p);s.noteOn(69,100);render(s,a,.2f);p[14]=0;s.parameters(p);s.noteOn(69,100);s.noteOn(76,100);render(s,a,.3f);save(output/"polyphonic_zero.f32",a);
 }
 if(run("sustain_release")){lp::Sf2Engine s(argv[1]);s.load(argv[2]);s.select(0,0);auto p=lp::Patch{}.parameters;p[14]=.5f;s.parameters(p);std::vector<float>a;
  s.noteOn(69,100);render(s,a,.2f);s.cc(64,127);s.noteOff(69);render(s,a,.2f);s.cc(64,0);render(s,a,.2f);s.panic();render(s,a,.1f);save(output/"sustain_release.f32",a);
 }
 if(run("rapid_legato")){lp::Sf2Engine s(argv[1]);s.load(argv[2]);s.select(0,0);auto p=lp::Patch{}.parameters;p[14]=.9f;s.parameters(p);std::vector<float>a;
  s.noteOn(69,100);render(s,a,.2f);s.noteOn(81,100);render(s,a,.4f);s.noteOn(76,100);render(s,a,1.8f);save(output/"rapid_legato.f32",a);
 }
 if(run("controller_reset")){lp::Sf2Engine s(argv[1]);s.load(argv[2]);s.select(0,0);auto p=lp::Patch{}.parameters;p[14]=.5f;s.parameters(p);std::vector<float>a;
  s.noteOn(69,100);render(s,a,.25f);s.cc(5,127);s.cc(37,127);s.cc(65,0);s.cc(127,0);s.noteOn(81,100);render(s,a,.8f);
  s.panic();s.noteOn(69,100);render(s,a,.25f);s.cc(121,0);s.noteOn(81,100);render(s,a,.8f);save(output/"controller_reset.f32",a);
 }
 std::cout<<"Rendered production SF2 glide scenarios\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

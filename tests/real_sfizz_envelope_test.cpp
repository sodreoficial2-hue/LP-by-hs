#include "sfizz_engine.hpp"
#include "envelope.hpp"
#include "session.hpp"
#include <fstream>
#include <iostream>
#include <vector>
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
void word(std::ofstream& out,unsigned value,int bytes){for(int i=0;i<bytes;++i)out.put(static_cast<char>((value>>(i*8))&255));}
std::array<std::vector<float>,2> render(lp::SfizzEngine& engine,int frames){
 std::array<std::vector<float>,2> out{std::vector<float>(frames),std::vector<float>(frames)};
 for(int at=0;at<frames;){int size=std::min(256,frames-at);engine.render(out[0].data()+at,out[1].data()+at,size);at+=size;}return out;
}
float rms(const std::vector<float>& x,int begin,int end){double energy=0;for(int i=begin;i<end;++i)energy+=x[i]*x[i];return std::sqrt(energy/(end-begin));}
int main(int argc,char** argv){
 check(argc>=2,"Expected real sfizz library");auto dir=std::filesystem::temp_directory_path()/"lp-real-envelope-test";std::filesystem::create_directories(dir);
 {std::ofstream out(dir/"tone.wav",std::ios::binary);out.write("RIFF",4);word(out,36+96000,4);out.write("WAVEfmt ",8);word(out,16,4);word(out,1,2);word(out,1,2);word(out,48000,4);word(out,96000,4);word(out,2,2);word(out,16,2);out.write("data",4);word(out,96000,4);
  for(int i=0;i<48000;++i){auto sample=static_cast<short>(12000*std::sin(6.28318530718*440*i/48000));word(out,static_cast<unsigned short>(sample),2);}}
 {std::ofstream out(dir/"envelope.sfz");for(int key:{60,61})out<<"<region> sample=tone.wav key="<<key<<" pitch_keycenter=60 pan="<<(key==60?-100:100)<<" loop_mode=loop_continuous loop_start=0 loop_end=47999 ampeg_attack=0.001 ampeg_decay=0.01 ampeg_sustain=100 ampeg_release="<<(key==60?.02f:.06f)<<" amp_veltrack=0\n";}
 lp::SfizzEngine engine(argv[1],48000,256,true); // Offline streaming must wait for disk reads.
 engine.load(dir/"envelope.sfz");check(engine.regions()==2,"Real SFZ regions");
 auto original0=engine.envelope(0),original1=engine.envelope(1);auto p=lp::Patch{}.parameters;
 p[1]=.25f;p[2]=std::sqrt(.25f/8);p[3]=.25f;p[4]=std::sqrt((.4f-.005f)/11.995f);
 engine.parameters(p,true);auto expected=lp::envelopeValues(p);
 for(int region=0;region<2;++region){auto actual=engine.envelope(region);for(int k=0;k<4;++k)check(std::abs(actual[k]-expected[k])<.0001f,"Real OSC ADSR readback");}
 engine.noteOn(60,100);auto first=render(engine,48000);
 check(rms(first[0],18000,21000)>rms(first[0],100,1200)*4,"Attack must rise over time");
 check(rms(first[0],23000,24000)>rms(first[0],45000,48000)*1.8f,"Decay must reach reduced sustain");
 float sustain=rms(first[0],45000,48000);check(sustain>1e-4f,"Sustain remains audible");
 engine.noteOn(61,100);auto chord=render(engine,48000);
 check(rms(chord[0],100,1200)>sustain*.8f,"New note must not restart older voice attack");
 check(rms(chord[1],18000,21000)>rms(chord[1],100,1200)*4,"New voice has independent attack");
 engine.noteOff(60);engine.noteOff(61);auto release=render(engine,48000);
 check(rms(release[0],0,2000)>1e-5f&&rms(release[0],35000,47000)<1e-7f,"Release tail then silence");
 engine.parameters(p,false);for(int region=0;region<2;++region){auto actual=engine.envelope(region);const auto& old=region?original1:original0;for(int k=0;k<4;++k)check(std::abs(actual[k]-old[k])<.0001f,"Restore native region envelopes");}
 if(argc>2){auto instruments=lp::catalog(argv[2]);check(instruments.size()==3,"Expected three recovered SFZ instruments");for(const auto& sfz:instruments){engine.load(sfz);engine.parameters(p,true);engine.noteOn(60,100);auto audio=render(engine,48000);if(rms(audio[0],0,48000)<=1e-6f)throw std::runtime_error("Recovered SFZ must render real audio: "+sfz.u8string());engine.panic();}}
 std::filesystem::remove_all(dir);std::cout<<"PASS: REAL sfizz OSC ADSR, measured attack/decay/sustain/release, independent voices, native restore and recovered sample rendering\n";
}

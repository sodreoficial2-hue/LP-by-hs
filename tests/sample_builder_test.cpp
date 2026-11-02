#include "sample_builder.hpp"
#include "sf2_headers.hpp"
#include <cassert>
#include <iostream>
#include <random>
using namespace lp::sampling;
Sample tone(double hz,double amplitude,int channels=1){Sample s;s.rate=24000;s.channels=channels;s.source="audio.wav";for(int i=0;i<24000;++i){float x=static_cast<float>(amplitude*(std::sin(2*pi*hz*i/s.rate)+.3*std::sin(4*pi*hz*i/s.rate)));for(int c=0;c<channels;++c)s.pcm.push_back(c?-x:x);}return s;}
void wav(const Sample& s,const std::filesystem::path& p){std::string fmt;u16(fmt,1);u16(fmt,s.channels);u32(fmt,s.rate);u32(fmt,s.rate*s.channels*2);u16(fmt,s.channels*2);u16(fmt,16);std::string pcm;for(auto x:s.pcm)u16(pcm,static_cast<std::int16_t>(x*32767));lp::atomicWrite(p,chunk("RIFF","WAVE"+chunk("fmt ",fmt)+chunk("data",pcm)));}
int main(int argc,char** argv){assert(argc==3);auto library=std::filesystem::path(argv[1]),folder=std::filesystem::path(argv[2]);std::filesystem::create_directories(folder);
 for(int n:{36,48,60,69,84,96}){auto s=tone(440*std::pow(2.,(n-69)/12.),.2);pitch(s);assert(s.root==n);assert(std::abs(s.cents)<12);assert(s.confidence>.9);}
 auto stereo=tone(440,.2,2);pitch(stereo);assert(stereo.root==69); // anti-phase stereo must not cancel pitch detection
 Sample noise;noise.rate=24000;noise.source="noise.wav";std::mt19937 rng(4);for(int i=0;i<24000;++i)noise.pcm.push_back(static_cast<float>(int(rng()%20000)-10000)/40000);pitch(noise);assert(noise.root==-1);
 Sample meta;meta.source="Piano_C#4_v90.wav";metadata(meta);assert(meta.root==61&&meta.velocity==90&&meta.explicitVelocity);
 meta=Sample{};meta.source="note60_vel32.wav";metadata(meta);assert(meta.root==60&&meta.velocity==32);
 meta=Sample{};meta.source="Bass_Bb1_v127.wav";metadata(meta);assert(meta.root==34&&meta.velocity==127);
 auto soft=folder/"soft.wav",hard=folder/"hard.wav",higher=folder/"C5_v100.wav";wav(tone(261.625565,.05),soft);wav(tone(261.625565,.4),hard);wav(tone(523.25113,.3,2),higher);
 auto p=analyze({hard,soft,higher},library);assert(p.samples.size()==3);auto& loud=p.samples[0];auto& quiet=p.samples[1];assert(loud.root==60&&quiet.root==60);assert(quiet.velocity<loud.velocity);assert(quiet.velLow==1&&quiet.velHigh+1==loud.velLow&&loud.velHigh==127);assert(quiet.low==0&&quiet.high==66&&p.samples[2].low==67&&p.samples[2].high==127);assert(loud.energy>quiet.energy*5);assert(quiet.loop);
 auto out=folder/"built.sf2";save(p,"Instrumento de teste",out);auto presets=lp::readSf2PresetHeaders(out);assert(presets.size()==1&&presets[0].bank==0&&presets[0].program==0);
 // Silence-separated notes: same pitch, independently measured dynamics.
 auto phrase=tone(220,.1);phrase.pcm.insert(phrase.pcm.end(),12000,0);auto next=tone(220,.5);phrase.pcm.insert(phrase.pcm.end(),next.pcm.begin(),next.pcm.end());auto segments=split(phrase);assert(segments.size()==2);assert(segments[1].energy>segments[0].energy*4);
 wav(phrase,folder/"phrase.wav");auto segmented=analyze({folder/"phrase.wav"},library);assert(segmented.samples.size()==2&&segmented.samples[0].root==57&&segmented.samples[1].root==57);
 auto decay=tone(220,.3);for(std::size_t i=0;i<decay.pcm.size();++i)decay.pcm[i]*=static_cast<float>(std::exp(-5.*i/decay.rate));pitch(decay);findLoop(decay);assert(!decay.loop);
 // Unknown notes block export, and cancellation never overwrites a destination.
 auto invalid=p;invalid.samples[0].root=-1;bool rejected=false;try{save(invalid,"bad",out);}catch(...){rejected=true;}assert(rejected&&lp::readSf2PresetHeaders(out).size()==1);
 rejected=false;try{analyze({soft},library,[]{return true;});}catch(...){rejected=true;}assert(rejected);
 rejected=false;try{analyze({folder/"absent.wav"},library);}catch(...){rejected=true;}assert(rejected);
 auto silent=tone(440,0);wav(silent,folder/"silent.wav");rejected=false;try{analyze({folder/"silent.wav"},library);}catch(...){rejected=true;}assert(rejected);
 // Explicit velocity names override amplitude ordering.
 wav(tone(440,.4),folder/"A4_v20.wav");wav(tone(440,.05),folder/"A4_v100.wav");auto explicitMap=analyze({folder/"A4_v20.wav",folder/"A4_v100.wav"},library);assert(explicitMap.samples[0].velHigh==60&&explicitMap.samples[1].velLow==61);
 std::cout<<"Sampler: decoding, pitch, stereo, segmentation, dynamics, loop, mapping, SF2 and safe failure passed\n";
}

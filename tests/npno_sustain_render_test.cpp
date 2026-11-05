// SPDX-License-Identifier: AGPL-3.0-or-later
#include "sfz_conversion.hpp"
#include "npno_reader.hpp"
#include "sfizz_probe.hpp"
#include <iostream>
#include <fstream>
using namespace lp;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
double db(const std::array<std::vector<float>,2>& a){double e=0;for(auto& c:a)for(float x:c)e+=x*x;return 10*std::log10(std::max(1e-24,e/(2*a[0].size())));}
int main(int argc,char** argv){try{
 require(argc==5||argc==6,"sfizz sndfile output fixtures [personal]");if(argc==6&&std::string(argv[5]).empty()){std::cout<<"N/A: fixture pessoal L/XL ausente\n";return 77;}
 auto output=std::filesystem::absolute(argv[3]);std::filesystem::remove_all(output);std::filesystem::create_directories(output/"Samples");ConversionOptions options;options.velocityCrossfadeExplicit=true;options.sustainSfizzLibrary=argv[1];MetaSidecar meta;
 std::filesystem::path instrument;std::vector<ConvertedZone> zones;
 if(argc==6){instrument=npnoParaSFZ(argv[5],output/"personal",meta,argv[2],{},options);zones=meta.zones;}
 else {
  WavWriter writer(argv[2]);for(int b=0;b<3;++b){std::vector<std::int16_t> pcm(48000*2);for(int n=0;n<48000;++n){auto v=std::int16_t((b==1?5600:11200)*std::sin(2*3.141592653589793*220*n/48000));pcm[2*n]=v;pcm[2*n+1]=v;}writer.writePcm16(output/"Samples"/(std::to_string(b)+".wav"),48000,pcm,2);}
  for(int root:{36,60,84})for(int band=0;band<8;++band)for(int bank=0;bank<2;++bank){ConvertedZone z;z.sample="Samples/"+std::to_string(bank)+".wav";z.frames=48000;z.rate=48000;z.channels=2;z.root=root;z.keyLow=root-12;z.keyHigh=root+11;z.velLow=band*16+1;z.velHigh=band==7?127:(band+1)*16;z.sourceBank=bank;z.canonicalBand=band;z.pedalClass=std::min(3,int(std::lround(band*.5)));z.sourceStroke=bank?100+root+z.pedalClass:root+band;z.bandInherited=band>0&&z.pedalClass==std::min(3,int(std::lround((band-1)*.5)));z.ampVeltrack=0;z.volumeDb=0;z.pcmPeak=(bank?5600.:11200.)/32768;z.loop=true;z.loopStart=4800;z.loopEnd=38400;z.sourceLoopDecay=8388524;z.releaseSeconds=root>72?2:.2;if(bank)z.cc64Low=64;else z.cc64High=63;zones.push_back(z);}
  for(int root:{36,60,84}){ConvertedZone z;z.sample="Samples/2.wav";z.frames=48000;z.rate=48000;z.channels=2;z.root=root;z.keyLow=root-12;z.keyHigh=root+11;z.sourceBank=2;z.releaseTrigger=true;z.releaseSeconds=.2;z.ampVeltrack=0;z.volumeDb=-18;zones.push_back(z);}
  meta.format="npno";prepareNpnoSustain(zones,meta,options,output,argv[2]);prepareSfzZones(zones,meta,options);meta.zones=zones;instrument=output/"matrix.sfz";atomicWrite(instrument,writeSfzInstrument(zones));
 }
 SfizzProbe synth(argv[1]);synth.load(instrument);auto report=sfzStream();report.str("");report<<"key,velocity,off_db,on_db,delta_db,off_voices,on_voices\n";double maxResidual=0,minDelta=100,maxDelta=-100;
 for(int key:{24,31,40,48,55,60,67,72,73,84,95})for(int vel:{10,40,80,127}){
  double levels[2]{};int voices[2]{};
  for(int state=0;state<2;++state){synth.silence(synth.synth);synth.cc(synth.synth,0,64,state?127:0);synth.on(synth.synth,0,key,vel);auto a=synth.render(4096);levels[state]=db(a);voices[state]=synth.voices(synth.synth);require(voices[state]==(state?2:1),"Contagem de vozes ataque/ressonância divergente");}
  double delta=levels[1]-levels[0];minDelta=std::min(minDelta,delta);maxDelta=std::max(maxDelta,delta);require(delta>=1&&delta<=3,"Pedal fora do alvo +1..+3 dB");report<<key<<','<<vel<<','<<levels[0]<<','<<levels[1]<<','<<delta<<','<<voices[0]<<','<<voices[1]<<'\n';
 }
 // Synthetic lowest band is 1..16; the physical sample and voice count
 // stay constant, while each doubling of touch adds 6.0206 dB.
 if(argc==5)for(int pedal:{0,127}){double previous=0;for(int v:{1,2,4,8,16}){synth.silence(synth.synth);synth.cc(synth.synth,0,64,pedal);synth.on(synth.synth,0,60,v);double level=db(synth.render(4096));if(v>1)require(std::abs(level-previous-20*std::log10(2.))<.02,"Toque suave permanece plano ou tracking duplicado");previous=level;}}
 for(const auto& z:zones)maxResidual=std::max(maxResidual,z.pedalResidualDb);require(maxResidual<=3,"Erro RMS pedal excede 3 dB");
 synth.silence(synth.synth);synth.cc(synth.synth,0,64,0);synth.on(synth.synth,0,60,127);synth.render(256);require(synth.voices(synth.synth)==1,"Ataque OFF inesperado");synth.cc(synth.synth,0,64,127);synth.render(256);require(synth.voices(synth.synth)==1,"Pedal ativou ressonância retroativa");
 synth.silence(synth.synth);synth.cc(synth.synth,0,64,127);synth.on(synth.synth,0,60,127);auto attack=synth.render(4096);auto tail=synth.render(48000*8);require(synth.voices(synth.synth)==1,"Ressonância persiste acima de 8 segundos");double last=0;for(int i=tail[0].size()-4096;i<int(tail[0].size());++i)last+=tail[0][i]*tail[0][i]+tail[1][i]*tail[1][i];double tailDb=10*std::log10(std::max(1e-24,last/8192));require(tailDb<db(attack)-3,"Envelope de loop não decaiu");double previous=0;for(int i=tail[0].size()-8192;i<int(tail[0].size())-4096;++i)previous+=tail[0][i]*tail[0][i]+tail[1][i]*tail[1][i];double stability=std::abs(tailDb-10*std::log10(std::max(1e-24,previous/8192)));require(stability<.5,"Patamar de sustain instável acima de 0,5 dB");
 synth.silence(synth.synth);synth.cc(synth.synth,0,64,127);synth.on(synth.synth,0,60,127);synth.render(256);synth.off(synth.synth,0,60,0);synth.render(256);int afterOff=synth.voices(synth.synth);require(afterOff==3,"Soltura deve acrescentar uma única voz");for(int cc:{0,127,0,127}){synth.cc(synth.synth,0,64,cc);synth.render(256);require(synth.voices(synth.synth)<=afterOff,"Pedal bouncing redisparou soltura");}
 synth.silence(synth.synth);synth.cc(synth.synth,0,64,127);for(int k:{30,36,42,48,54,60,66,72,78,84})synth.on(synth.synth,0,k,127);auto chord=synth.render(48000);double peak=0;for(auto& c:chord)for(float x:c)peak=std::max(peak,std::abs(double(x)));require(20*std::log10(peak)<=-3,"Acorde excede teto -3 dBFS");
 atomicWrite(output/"matrix.csv",report.str());atomicWrite(output/"meta.json",meta.json());std::cout<<"PASS: 44 pares ON/OFF; voices=1/2; delta="<<minDelta<<".."<<maxDelta<<" dB; residual="<<maxResidual<<" dB; sustain_tail="<<tailDb<<" dB; chord_peak="<<20*std::log10(peak)<<" dBFS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

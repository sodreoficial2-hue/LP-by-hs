#include "offline_sfizz.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=4)throw std::runtime_error("sfizz sndfile output");
 auto root=std::filesystem::absolute(argv[3]);std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 lp::WavWriter writer(argv[2]);std::vector<std::int16_t> pcm(48000);
 for(std::size_t i=0;i<pcm.size();++i)pcm[i]=std::int16_t(5000*std::sin(2*3.141592653589793*220*i/48000));
 writer.writePcm16(root/"tone.wav",48000,pcm);
 std::vector<lp::ConvertedZone> zones;
 for(int i=0;i<131;++i){lp::ConvertedZone z;z.sample="tone.wav";z.frames=pcm.size();z.rate=48000;z.root=60;z.keyLow=48;z.keyHigh=72;z.ampVeltrack=0;z.releaseSeconds=.2;z.releaseTrigger=i%3==0;z.volumeDb=-double(i%24);z.hasPan=true;z.pan=i%2?100:-100;zones.push_back(z);}
 lp::OfflineRegionProbe grouped(argv[1],root/"grouped.sfz",zones);
 lp::OfflineSfizz isolated(argv[1]);double maximum=0;
 for(std::size_t i:{0u,1u,2u,127u,128u,129u,130u})for(int key:{48,60,72})for(int velocity:{40,80,110}){
  lp::atomicWrite(root/"isolated.sfz",lp::writeSfzInstrument({zones[i]}));
  double reference=isolated.rmsDb(root/"isolated.sfz",key,velocity,0,zones[i].releaseTrigger);
  double measured=grouped.rmsDb(i,key,velocity,0,zones[i].releaseTrigger);
  maximum=std::max(maximum,std::abs(reference-measured));
  if(std::abs(reference-measured)>0.0001)throw std::runtime_error("Grouped probe diverges: "+std::to_string(i)+" delta="+std::to_string(measured-reference));
 }
 std::cout<<"PASS: isolated versus grouped RMS, selector byte boundary, release_key and stereo pan; maximum_db="<<maximum<<"\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

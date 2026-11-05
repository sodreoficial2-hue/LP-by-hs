#include "offline_sfizz.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=5)throw std::runtime_error("sfizz sndfile output");
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

 // Real binary fixtures include different soft-layer curves, loops and aliases.
 setenv("LP_REAL_SFIZZ_LIBRARY","",1);
 for(const char* fixture:{"layers-v5.npno","layers-v6.npno"}){
  lp::MetaSidecar metadata;lp::npnoParaSFZ(std::filesystem::path(argv[4])/fixture,root/fixture,metadata,argv[2]);
  auto probeZones=metadata.zones;std::vector<int> velocities;
  for(auto& z:probeZones){velocities.push_back((z.sourceVelLow+z.sourceVelHigh)/2);z.muted=false;z.velLow=1;z.velHigh=127;z.cc64Low=z.cc64High=-1;z.sequenceLength=z.sequencePosition=0;z.xfinLowVel=z.xfoutLowVel=-1;}
  lp::OfflineRegionProbe actual(argv[1],root/fixture/"actual.sfz",probeZones);
  for(std::size_t i=0;i<probeZones.size();++i)for(int key:{probeZones[i].keyLow,(probeZones[i].keyLow+probeZones[i].keyHigh)/2,probeZones[i].keyHigh}){
   lp::atomicWrite(root/fixture/"single.sfz",lp::writeSfzInstrument({probeZones[i]}));
   double reference=isolated.rmsDb(root/fixture/"single.sfz",key,velocities[i],0,probeZones[i].releaseTrigger);
   double measured=actual.rmsDb(i,key,velocities[i],0,probeZones[i].releaseTrigger);
   if(std::abs(reference-measured)>.0001)throw std::runtime_error(std::string(fixture)+" region "+std::to_string(i)+" grouped="+std::to_string(measured)+" isolated="+std::to_string(reference));
  }
 }
 std::cout<<"PASS: isolated versus grouped RMS, selector byte boundary, release_key and stereo pan; maximum_db="<<maximum<<"\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

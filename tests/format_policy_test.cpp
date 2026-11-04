#include "sfz_conversion.hpp"
#include "npno_reader.hpp"
#include "nsmp/nsmp.hpp"
#include <iostream>
#include <fstream>
using namespace lp;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class F>void fails(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"expected policy failure");}
void put(npno::Bytes& b,int at,unsigned x,int n){for(int k=0;k<n;++k)b.at(at+k)=static_cast<unsigned char>(x>>(8*k));}
void seal(npno::Bytes& b){if(b[4])put(b,24,nsmp::crc32(b,44,b.size()),4);else put(b,b.size()-2,nsmp::crc16(b,b.size()-2),2);}
int main(int argc,char** argv){try{
 check(argc==3,"fixtures output");auto fixtures=std::filesystem::path(argv[1]);auto output=std::filesystem::absolute(argv[2]);std::filesystem::remove_all(output);std::filesystem::create_directories(output);
 for(auto family:std::vector<std::pair<const char*,std::vector<int>>>{{"layers-v5.npno",{500,520,530,540}},{"layers-v6.npno",{600,610,620,630}}}){
  auto b=nsmp::readFile(fixtures/family.first);for(auto version:family.second){put(b,20,version,4);seal(b);auto p=output/(std::to_string(version)+".npno");atomicWrite(p,std::string(b.begin(),b.end()));auto module=npno::readModule(p);check(module.header.version==unsigned(version)&&module.checksumValid,"supported outer family dispatch");for(const auto& stroke:module.strokes)npno::decodeStroke(module,stroke);}
 }
 auto b=nsmp::readFile(fixtures/"layers-v6.npno");put(b,20,700,4);seal(b);auto bad=output/"bad.npno";atomicWrite(bad,std::string(b.begin(),b.end()));try{npno::readModule(bad);throw std::logic_error("missing failure");}catch(const std::runtime_error& e){check(std::string(e.what()).find("7.0")!=std::string::npos,"version in diagnostic");}
 b=nsmp::readFile(fixtures/"layers-v6.npno");b[24]^=1;atomicWrite(bad,std::string(b.begin(),b.end()));auto module=npno::readModule(bad);check(!module.checksumValid&&!module.warnings.empty(),"checksum warns and continues");
 ConvertedZone z;z.sample="Samples/test.wav";z.frames=44100;z.root=69;z.keyLow=21;z.keyHigh=108;z.rate=44100;std::vector<ConvertedZone> regions;
 for(int rr=1;rr<=2;++rr)for(int layer=0;layer<4;++layer){auto r=z;r.velLow=layer*32+1;r.velHigh=layer==3?127:(layer+1)*32;r.sequenceLength=2;r.sequencePosition=rr;regions.push_back(r);}
 MetaSidecar meta;auto off=regions;prepareSfzZones(off,meta);check(off.size()==8,"no expansion per MIDI note/stroke");
 for(int velocity=1;velocity<=127;++velocity)for(int seq=1;seq<=2;++seq){int active=0;for(auto& r:off)if(r.sequencePosition==seq&&velocity>=r.velLow&&velocity<=r.velHigh)++active;check(active==1,"exact one layer/alternative per note");}
 auto gap=regions;gap[1].velLow++;fails([&]{auditVelocityCoverage(gap);});auto overlap=regions;overlap[1].velLow--;fails([&]{auditVelocityCoverage(overlap);});
 ConversionOptions options;options.velocityCrossfade=true;auto on=regions;prepareSfzZones(on,meta,options);check(on.size()==off.size(),"crossfade keeps region count");int maxOff=0,maxOn=0;for(int v=1;v<=127;++v){int a=0,b=0;for(auto& r:off)if(r.sequencePosition==1&&v>=r.velLow&&v<=r.velHigh)++a;for(auto& r:on)if(r.sequencePosition==1&&v>=r.velLow&&v<=r.velHigh)++b;maxOff=std::max(maxOff,a);maxOn=std::max(maxOn,b);}check(maxOff==1&&maxOn==2,"voice cost explicit");
 auto missing=regions;missing.erase(std::remove_if(missing.begin(),missing.end(),[](const auto& z){return z.sequencePosition==2;}),missing.end());fails([&]{auditVelocityCoverage(missing);});
 auto panDuplicate=regions;auto extra=panDuplicate[0];extra.hasPan=true;extra.pan=-20;panDuplicate.push_back(extra);fails([&]{prepareSfzZones(panDuplicate,meta);});
 auto duplicate=regions;duplicate.push_back(duplicate[0]);fails([&]{prepareSfzZones(duplicate,meta);});
 std::cout<<"PASS: 8 outer revisions (synthetic mutations, 2 internal PCM16 families), unknown version diagnostic, checksum warning, shared RR exclusivity/stable count, velocity gaps/overlaps and opt-in crossfade voice cost 1->2\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

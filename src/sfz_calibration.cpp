// SPDX-License-Identifier: AGPL-3.0-or-later
#include "offline_sfizz.hpp"
#include <map>
#include <numeric>
#include <set>
#include <tuple>
namespace lp {
namespace {
// Project reference, not a decoded Nord DSP law: preserve PCM+trim layer RMS,
// remove additional velocity tracking, pool descending neighbors by least squares.
// T4's inverse Touch curve determines the disjoint velocity bands before this fit.
constexpr double pedalReferenceDb=0,releaseReferenceDb=-18;
struct Measurement {double mean=0;std::vector<double> db;std::vector<std::pair<int,int>> notes;};
using Stack=std::tuple<int,int,int,int,int,int>; // program/root/key range/lane/sequence
Stack stack(const ConvertedZone& z){return {z.program,z.root,z.keyLow,z.keyHigh,z.hasPan?int(z.pan):0,z.sequencePosition};}
}
void calibrateSfzZones(std::vector<ConvertedZone>& zones,MetaSidecar& meta,const ConversionOptions& options,const std::filesystem::path& output){
 if(options.calibrationSfizzLibrary.empty()){
  meta.limitations.push_back("Calibração isotônica de dinâmica por camada N/A: use --calibrate-sfizz=LIB para essa etapa opcional. A equalização RMS específica do pedal NPNO é registrada separadamente.");return;
 }
 for(auto& z:zones){z.velLow=std::max(1,z.velLow);if(z.velLow>z.velHigh||z.velHigh>127)throw std::runtime_error("Calibração: faixa de velocity inválida");}
 if(zones.size()>65536)throw std::runtime_error("Calibração excede limite de 65536 regiões");
 // One scalar cannot equalize different resampling durations across a wide
 // keyboard zone, especially key-release recordings. Subdivide only when the
 // operator requests NPNO measurement, preserving sample identity and PCM.
 if(meta.format=="npno"){
  std::vector<ConvertedZone> perKey;
  for(const auto& z:zones)for(int key=z.keyLow;key<=z.keyHigh;++key){if(perKey.size()==65536)throw std::runtime_error("Calibração por tecla excede limite de 65536 regiões SFZ");auto r=z;r.keyLow=r.keyHigh=key;perKey.push_back(r);}
  zones=std::move(perKey);meta.preservedFeatures.push_back("calibration_subdivision=per_key_NPNO; shared_PCM_unchanged=true; three_keyboard_registers_measured_across_module=true");
 }
 auto probe=output/".lp-offline-calibration.sfz";
 struct Remove {std::filesystem::path p;~Remove(){std::error_code ec;std::filesystem::remove(p,ec);}} cleanup{probe};
 std::vector<Measurement> measured(zones.size());std::map<Stack,std::vector<std::size_t>> attacks;
 for(auto& z:zones)if(!z.muted)z.ampVeltrack=0;
 auto measurementZones=[&](){auto copy=zones;for(auto& single:copy){single.sequenceLength=single.sequencePosition=0;single.velLow=1;single.velHigh=127;single.xfinLowVel=single.xfoutLowVel=-1;}return copy;};
 {OfflineRegionProbe initial(options.calibrationSfizzLibrary,probe,measurementZones());
 for(std::size_t i=0;i<zones.size();++i){conversionStatus(options,"calibrando zona "+std::to_string(i+1)+" de "+std::to_string(zones.size())+"...");auto& z=zones[i];if(z.muted)continue;
  z.ampVeltrack=0; // Explicit compensation: no hidden sfizz velocity->amplitude curve.
  auto& m=measured[i];
  std::set<int> keys{z.keyLow,(z.keyLow+z.keyHigh)/2,z.keyHigh};
  std::set<int> velocities{z.velLow,(z.velLow+z.velHigh)/2,z.velHigh};
  for(int key:keys)for(int velocity:velocities){m.notes.push_back({key,velocity});m.db.push_back(initial.rmsDb(i,key,velocity,z.cc64Low>=64?127:0,z.releaseTrigger));}
  m.mean=std::accumulate(m.db.begin(),m.db.end(),0.0)/m.db.size();
  if(!z.releaseTrigger&&z.sourceBank!=1)attacks[stack(z)].push_back(i);
 }
 }
 std::vector<double> targets(zones.size());for(std::size_t i=0;i<zones.size();++i)targets[i]=measured[i].mean;
 for(auto& entry:attacks){auto& ids=entry.second;std::stable_sort(ids.begin(),ids.end(),[&](auto a,auto b){return zones[a].velLow<zones[b].velLow;});
  struct Pool {std::vector<std::size_t> ids;double sum=0,weight=0;};std::vector<Pool> pools;
  for(auto i:ids){double weight=zones[i].velHigh-zones[i].velLow+1.;pools.push_back({{i},measured[i].mean*weight,weight});
   while(pools.size()>1){auto& a=pools[pools.size()-2];auto& b=pools.back();if(a.sum/a.weight<=b.sum/b.weight)break;a.sum+=b.sum;a.weight+=b.weight;a.ids.insert(a.ids.end(),b.ids.begin(),b.ids.end());pools.pop_back();}}
  for(const auto& pool:pools)for(auto i:pool.ids)targets[i]=pool.sum/pool.weight;
 }
 // Match each optional bank against its normal attack at the middle velocity.
 // This measures a bank's source level, instead of assuming that -12/-18 dB alone
 // gives -12/-18 dB RMS for differently recorded samples.
 for(std::size_t i=0;i<zones.size();++i){const auto& z=zones[i];if(z.muted||z.sourceBank<1)continue;
  auto found=attacks.find(stack(z));if(found==attacks.end())throw std::runtime_error("Calibração: banco sem ataque de referência");
  int v=(z.velLow+z.velHigh)/2;bool matched=false;
  for(auto a:found->second)if(v>=zones[a].velLow&&v<=zones[a].velHigh){targets[i]=targets[a]+(z.sourceBank==1?pedalReferenceDb:releaseReferenceDb);matched=true;break;}
  if(!matched)throw std::runtime_error("Calibração: velocity sem ataque de referência");
 }
 double maximum=0;int count=0;bool bounded=true;
 for(std::size_t i=0;i<zones.size();++i){conversionStatus(options,"calibrando zona "+std::to_string(i+1)+" de "+std::to_string(zones.size())+"...");auto& z=zones[i];if(z.muted)continue;double before=z.volumeDb;
  double requested=before+targets[i]-measured[i].mean;z.volumeDb=std::clamp(requested,-144.0,10.8);bounded=bounded&&std::abs(requested-z.volumeDb)<1e-8;z.calibrationOffsetDb=z.volumeDb-before;
 }
 {OfflineRegionProbe adjusted(options.calibrationSfizzLibrary,probe,measurementZones());
 for(std::size_t i=0;i<zones.size();++i){conversionStatus(options,"calibrando zona "+std::to_string(i+1)+" de "+std::to_string(zones.size())+"...");auto& z=zones[i];if(z.muted)continue;
  for(auto note:measured[i].notes){double db=adjusted.rmsDb(i,note.first,note.second,z.cc64Low>=64?127:0,z.releaseTrigger);z.calibrationResidualDb=std::max(z.calibrationResidualDb,std::abs(db-targets[i]));}
  z.calibrated=true;maximum=std::max(maximum,z.calibrationResidualDb);++count;
 }
 }
 meta.preservedFeatures.push_back("calibration_reference=source_PCM_plus_trim_isotonic_layer_RMS; continuous_Nord_DSP_curve=unknown; amp_veltrack=0; RMS_window_frames=4096; rate=48000; keys=low/mid/high_unique; velocities=low/mid/high_unique; regions="+std::to_string(count)+"; residual_max_db="+std::to_string(maximum));
 meta.preservedFeatures.push_back("calibrated_bank_reference_db: pedal=0; release=-18; tolerance_synthetic_db=0.5; volume_bounds=[-144,10.8]");
 if(!bounded||maximum>0.5)meta.limitations.push_back("Calibração fora da tolerância de projeto 0.5 dB (variação por tecla ou limite de ganho); consulte residual por região. Não declarar aceite timbrístico deste módulo.");
}
}

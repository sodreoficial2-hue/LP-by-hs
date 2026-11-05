// SPDX-License-Identifier: AGPL-3.0-or-later
#include "offline_sfizz.hpp"
#include <cstdlib>
#include <set>
namespace lp {
// Project approximation, not a decoded Nord time unit. A Q23 multiplier is
// interpreted at the source frame rate; time to -60 dB is bounded for safety.
double npnoLoopDecaySeconds(std::uint32_t value){
 if(value>0x800000)return -1;
 if(value==0x800000)return 4; // Continuous bounded limit as Q23 tends to unity.
 if(!value)return .15;
 return std::clamp(std::log(.001)/(35002.0*std::log(double(value)/8388608.0)),.15,4.0);
}
void prepareNpnoSustain(std::vector<ConvertedZone>& zones,MetaSidecar& meta,const ConversionOptions& options,const std::filesystem::path& output,const std::filesystem::path& sndfile){
 auditNpnoPedalBands(zones);
 // Correct the flat pianissimo range left by amp_veltrack=0. This local curve
 // is shared by attack, pedal, resonance and release, including crossfades.
 // Above the canonical soft-layer top the original PCM/trim balance is unchanged.
 for(auto& z:zones){int top=0;for(const auto& a:zones)if(a.sourceBank==0&&a.root==z.root&&a.velLow==1&&!a.releaseTrigger)top=a.velHigh;if(!top)throw std::runtime_error("Dinâmica NPNO sem camada suave canônica");z.softVelocityCeiling=top;z.ampVeltrack=100;}
 meta.preservedFeatures.push_back("soft_velocity_model=canonical_soft_layer_linear_amplitude_then_unity; gain=min(velocity/soft_velocity_ceiling,1); applies=attack/pedal/resonance/release; original_Nord_curve=unconfirmed; applied_after_optional_isotonic=true; calibration_residual_refers_to_pre_curve_reference=true; region_boundary_and_PCM_unchanged=true");
 auto lib=options.sustainSfizzLibrary.empty()?options.calibrationSfizzLibrary:options.sustainSfizzLibrary;
 if(lib.empty()){if(const char* env=std::getenv("LP_REAL_SFIZZ_LIBRARY"))lib=env;}
 if(lib.empty()){
#ifdef _WIN32
 lib=sndfile.parent_path()/"sfizz.dll";
#else
 lib=sndfile.parent_path()/"libsfizz.so";
#endif
 }
 for(auto& z:zones)if(z.loop&&!z.releaseTrigger){z.decaySeconds=npnoLoopDecaySeconds(z.sourceLoopDecay);if(z.decaySeconds>=0)z.sustainPercent=25;else meta.limitations.push_back("loopDecay sem aproximação segura: registro "+std::to_string(z.sourceStroke)+", Q23="+std::to_string(z.sourceLoopDecay)+"; envelope conservador sem decay.");}
 if(!std::filesystem::exists(lib)){
  if(!options.sustainSfizzLibrary.empty()||!options.calibrationSfizzLibrary.empty())throw std::runtime_error("Biblioteca sfizz offline informada não existe");
  // Never replace the audible normal attack with an unmeasured quieter bank.
  for(auto& z:zones){if(z.sourceBank==0)z.cc64High=-1;if(z.sourceBank==1)z.muted=true;}
  meta.limitations.push_back("FALHADO/N-A: sfizz offline ausente; calibração pedal, ressonância e headroom não medidos. Fallback seguro mantém banco 0 em ambos estados; banco 1 preservado e marcado muted.");return;
 }
 OfflineSfizz synth(lib);auto probe=output/".lp-sustain-probe.sfz";
 struct Remove{std::filesystem::path p;~Remove(){std::error_code ec;std::filesystem::remove(p,ec);}} cleanup{probe};
 auto measure=[&](ConvertedZone z,int key,int vel){z.velLow=1;z.velHigh=127;z.cc64Low=z.cc64High=-1;atomicWrite(probe,writeSfzInstrument({z}));return synth.rmsDb(probe,key,vel,0,false);};
 double residual=0;int measured=0;
 for(auto& z:zones)if(z.sourceBank==1&&!z.releaseTrigger&&!z.muted){
  const ConvertedZone* normal=nullptr;
  for(const auto& a:zones)if(a.sourceBank==0&&a.root==z.root&&a.canonicalBand==z.canonicalBand&&a.keyLow==z.keyLow&&a.keyHigh==z.keyHigh&&a.pan==z.pan){if(normal)throw std::runtime_error("Calibração pedal: ataque canônico duplicado");normal=&a;}
  if(!normal)throw std::runtime_error("Calibração pedal: ataque canônico ausente");
  std::vector<double> differences;int vel=(z.velLow+z.velHigh)/2;
  for(int key:{z.keyLow,(z.keyLow+z.keyHigh)/2,z.keyHigh})differences.push_back(measure(*normal,key,vel)-measure(z,key,vel));
  double delta=(differences[0]+differences[1]+differences[2])/3;double before=z.volumeDb;z.volumeDb=std::clamp(before+delta,-144.0,10.8);z.pedalOffsetDb=z.volumeDb-before;
  for(int key:{z.keyLow,(z.keyLow+z.keyHigh)/2,z.keyHigh})z.pedalResidualDb=std::max(z.pedalResidualDb,std::abs(measure(*normal,key,vel)-measure(z,key,vel)));residual=std::max(residual,z.pedalResidualDb);++measured;
  if(std::abs(delta-z.pedalOffsetDb)>.01)meta.limitations.push_back("Calibração pedal limitada: registro "+std::to_string(z.sourceStroke)+", banda "+std::to_string(z.canonicalBand)+", déficit dB="+std::to_string(delta-z.pedalOffsetDb));
 }
 // -12 dB coherent extra amplitude gives approximately +1.95 dB initially.
 // This project seed is measured below, not a claim of physical Nord resonance.
 constexpr double resonanceGainDb=-12,resonanceReleaseSeconds=4;
 if(options.npnoPedalResonance){auto original=zones.size();for(std::size_t i=0;i<original;++i)if(zones[i].sourceBank==1&&!zones[i].releaseTrigger&&!zones[i].muted){auto z=zones[i];z.npnoResonance=true;z.volumeDb=std::max(-144.0,z.volumeDb+resonanceGainDb);z.decaySeconds=std::min(2.0,z.decaySeconds<0?2.0:z.decaySeconds);z.sustainPercent=0;z.releaseSeconds=resonanceReleaseSeconds;zones.push_back(z);}}
 if(zones.size()>65536)throw std::runtime_error("Ressonância excede limite de 65536 regiões SFZ");
 // Conservative initial bound: ten coherent full-scale notes, each with its
 // active attack/resonance gains. PCM is never modified.
 double amplitude=0;for(const auto& z:zones)if(!z.muted&&!z.releaseTrigger)amplitude=std::max(amplitude,z.pcmPeak*std::pow(10.,z.volumeDb/20));
 double gain=std::min(0.,-3.2-20*std::log10(std::max(1e-12,10*amplitude*(options.velocityCrossfade?std::sqrt(2.):1.)*(1+(options.npnoPedalResonance?std::pow(10.,resonanceGainDb/20):0)))));
 for(auto& z:zones)z.moduleGainDb=gain;
 std::set<int> available;for(const auto& z:zones)if(z.sourceBank==0&&!z.muted)for(int k=z.keyLow;k<=z.keyHigh;++k)available.insert(k);
 std::vector<int> all(available.begin(),available.end());std::vector<std::vector<int>> chords;
 if(!all.empty()){
  for(std::size_t begin:{std::size_t(0),all.size()>10?(all.size()-10)/2:0,all.size()>10?all.size()-10:0})chords.emplace_back(all.begin()+begin,all.begin()+std::min(all.size(),begin+10));
  std::vector<int> spread;for(int n=0;n<10;++n)spread.push_back(all[std::size_t(n)*(all.size()-1)/9]);std::sort(spread.begin(),spread.end());spread.erase(std::unique(spread.begin(),spread.end()),spread.end());chords.push_back(spread);
  std::vector<int> stage;for(int k:{30,36,42,48,54,60,66,72,78,84})if(available.count(k))stage.push_back(k);if(stage.size()==10)chords.push_back(stage);
 }
 std::set<int> velocities{10,40,80,127};for(const auto& z:zones)if(z.sourceBank==0){velocities.insert(z.velLow);velocities.insert(z.velHigh);}
 auto rendered=zones;MetaSidecar probeMeta;prepareSfzZones(rendered,probeMeta,options);atomicWrite(probe,writeSfzInstrument(rendered));double peak=0;
 for(const auto& chord:chords)for(int velocity:velocities)peak=std::max(peak,synth.chordPeak(probe,chord,velocity,127));
 double peakDb=peak>0?20*std::log10(peak):-144;
 // Refine only when the measured chord grid is >2 dB below target or clips.
 // The finite grid, its gain and the remaining worst-case uncertainty are explicit.
 if(peak>0&&(peakDb<-5.2||peakDb>-3.0)){
  double requested=gain-3.2-peakDb,ceiling=10.8;
  for(const auto& z:zones)if(!z.muted)ceiling=std::min(ceiling,10.8-z.volumeDb);
  double adjusted=std::min(requested,ceiling),change=adjusted-gain;gain=adjusted;for(auto& z:zones)z.moduleGainDb=gain;
  // Scalar gain is linear in sfizz; re-render to verify the actual maximum.
  for(auto& z:rendered)z.moduleGainDb=gain;atomicWrite(probe,writeSfzInstrument(rendered));peak=0;
  for(const auto& chord:chords)for(int velocity:velocities)peak=std::max(peak,synth.chordPeak(probe,chord,velocity,127));peakDb=peak>0?20*std::log10(peak):-144;
  meta.preservedFeatures.push_back("headroom_refinement_db="+std::to_string(change)+"; trigger=measured_grid_outside_target_by_2dB_or_ceiling; chords="+std::to_string(chords.size())+"; velocities="+std::to_string(velocities.size()));
 }
 meta.preservedFeatures.push_back("pedal_calibration=sfizz_RMS_ON_equals_OFF_without_extra_resonance; positions=low/mid/high; regions="+std::to_string(measured)+"; residual_max_db="+std::to_string(residual));
 meta.preservedFeatures.push_back(std::string("resonance_enabled=")+(options.npnoPedalResonance?"true; ":"false; ")+"resonance_gain_db=-12; release_seconds=4; decay_max_seconds=2; sustain_percent=0; trigger=new_note_CC64_ge_64; target_delta_db=1..3; physical_resonance=false");
 meta.preservedFeatures.push_back("loopDecay_model=project_Q23_frame_multiplier_to_minus60dB; formula=clamp(log(0.001)/(35002*log(q/8388608)),0.15,4); sustain_percent=25; source_units_unconfirmed=true; q_zero_seconds=0.15; q_unity_limit_seconds=4; q_above_unity=legacy_without_decay");
 meta.preservedFeatures.push_back("headroom_module_gain_db="+std::to_string(gain)+"; measured_10_note_peak_dbfs="+std::to_string(peakDb)+"; target_measured_grid_dbfs=-3.2");
 if(residual>3)meta.limitations.push_back("FALHADO: calibração pedal excede 3 dB; residual="+std::to_string(residual));
 if(peakDb>-3)meta.limitations.push_back("FALHADO: pico medido excede teto de -3 dBFS; pico="+std::to_string(peakDb));
 if(peakDb<-5.2)meta.limitations.push_back("FALHADO: headroom conservador deixa acorde medido abaixo do alvo por mais de 2 dB; não aplicado boost sem prova da grade completa.");
 meta.limitations.push_back("loopDecay é aproximação de projeto com unidades temporais não confirmadas independentemente; medição de um acorde não prova todos os acordes possíveis nem sustentações ilimitadas.");
}
}

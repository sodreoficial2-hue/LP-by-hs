#include "sfz_conversion.hpp"
#include "sfizz_probe.hpp"
#include <iostream>
using Audio=std::array<std::vector<float>,2>;
Audio capture(const char* library,const std::filesystem::path& sfz,int velocity,int pedal){
 SfizzProbe synth(library);synth.load(sfz);synth.cc(synth.synth,0,64,pedal);synth.on(synth.synth,0,60,velocity);
 auto held=synth.render(96000);synth.off(synth.synth,0,60,0);auto tail=synth.render(288000);
 for(int channel=0;channel<2;++channel)held[channel].insert(held[channel].end(),tail[channel].begin(),tail[channel].end());return held;
}
double rms(const Audio& data,int begin,int count){double sum=0;for(int c=0;c<2;++c)for(int n=begin;n<begin+count;++n)sum+=double(data[c][n])*data[c][n];return 10*std::log10(std::max(1e-24,sum/(count*2)));}
double peak(const Audio& data,int begin,int count){double maximum=0;for(int c=0;c<2;++c)for(int n=begin;n<begin+count;++n)maximum=std::max(maximum,std::abs(double(data[c][n])));return maximum;}
void save(lp::WavWriter& writer,const std::filesystem::path& path,const Audio& data){std::vector<float> interleaved;interleaved.reserve(data[0].size()*2);for(std::size_t n=0;n<data[0].size();++n){interleaved.push_back(data[0][n]);interleaved.push_back(data[1][n]);}writer.write(path,48000,2,interleaved);}
int main(int argc,char** argv){try{
 if(argc!=5)throw std::runtime_error("sfizz sndfile fixture output");auto root=std::filesystem::absolute(argv[4]);std::filesystem::remove_all(root);
 lp::ConversionOptions options;options.sustainSfizzLibrary=argv[1];lp::MetaSidecar meta;
 lp::npnoParaSFZ(argv[3],root,meta,argv[2],{},options);auto before=meta.zones,after=meta.zones,muted=meta.zones;
 for(auto& z:before){if(z.releaseTrigger){z.volumeDb=z.releaseKeyBaseVolumeDb;z.releaseSeconds=.2;}else if(z.sourceBank==0&&z.releaseSeconds==.45)z.releaseSeconds=.2;}
 for(auto& z:muted)if(z.releaseTrigger&&!z.releaseKeyPedalPassthrough)z.muted=true;
 auto beforeFile=root/"before.sfz",afterFile=root/"after.sfz",mutedFile=root/"muted.sfz";
 lp::atomicWrite(beforeFile,lp::writeSfzInstrument(before));lp::atomicWrite(afterFile,lp::writeSfzInstrument(after));lp::atomicWrite(mutedFile,lp::writeSfzInstrument(muted));
 // A key-noise switch must not mute the damped string's remaining vibration.
 // Measure the 15 dB control contrast on release_key alone. The complete
 // note-off signal is still checked below for decay, continuity and silence.
 auto releaseOnly=after,releaseMuted=muted;
 for(auto& z:releaseOnly)if(!z.releaseTrigger)z.muted=true;
 for(auto& z:releaseMuted)if(!z.releaseTrigger)z.muted=true;
 auto releaseFile=root/"release-only.sfz",releaseMutedFile=root/"release-muted.sfz";
 // A silent attack keeps a valid instrument and note-on history in both files.
 const std::string silentAttack="\n<group>\n<region> sample=*silence\n";
 lp::atomicWrite(releaseFile,lp::writeSfzInstrument(releaseOnly)+silentAttack);
 lp::atomicWrite(releaseMutedFile,lp::writeSfzInstrument(releaseMuted)+silentAttack);
 lp::WavWriter writer(argv[2]);auto csv=lp::sfzStream();csv.str("");csv<<"velocity,window_ms,rms_before_db,rms_after_db,rms_muted_db\n";
 bool accepted=true;auto report=lp::sfzStream();report.str("");report<<"{\n\"fixture_kind\":\"synthetic_interoperability_vector\",\n\"cases\":[";
 bool first=true;
 for(int velocity:{40,80,110}){
  auto a=capture(argv[1],beforeFile,velocity,0),b=capture(argv[1],afterFile,velocity,0),off=capture(argv[1],mutedFile,velocity,0);
  auto release=capture(argv[1],releaseFile,velocity,0),releaseOff=capture(argv[1],releaseMutedFile,velocity,0);
  save(writer,root/("release-only-v"+std::to_string(velocity)+".wav"),release);
  save(writer,root/("release-muted-v"+std::to_string(velocity)+".wav"),releaseOff);
  save(writer,root/("before-v"+std::to_string(velocity)+".wav"),a);save(writer,root/("after-v"+std::to_string(velocity)+".wav"),b);save(writer,root/("muted-v"+std::to_string(velocity)+".wav"),off);
  bool monotone=true,bounded=true;std::vector<double> levels;for(int window=0;window<60;++window){
   int begin=96000+window*2400;double level=rms(b,begin,2400);levels.push_back(level);
   csv<<velocity<<','<<window*50<<','<<rms(a,begin,2400)<<','<<level<<','<<rms(off,begin,2400)<<'\n';
   if(window&&level>levels[window-1]+.0001&&std::max(level,levels[window-1])>=-70)monotone=false;
   if(window&&std::min(level,levels[window-1])>=-70&&std::abs(level-levels[window-1])>3)bounded=false;
  }
  int silentWindow=60;for(int window=59;window>=0&&levels[window]<-70;--window)silentWindow=window;
  double silenceSeconds=(silentWindow+1)*.05;bool silence=silenceSeconds<=.8;
  double onPeak=peak(b,96000,144000),offPeak=peak(off,96000,144000);
  double reduction=20*std::log10(std::max(1e-12,onPeak)/std::max(1e-12,offPeak));
  double releasePeak=peak(release,96000,144000),releaseOffPeak=peak(releaseOff,96000,144000);
  double releaseReduction=20*std::log10(std::max(1e-12,releasePeak)/std::max(1e-12,releaseOffPeak));
  bool contrast=releasePeak>1e-8&&releaseReduction>=15&&releaseOffPeak<1e-8;
  bool attackUnchanged=true;double decompositionError=0;
  for(int c=0;c<2;++c)for(int n=0;n<240000;++n){
   if(n<96000&&b[c][n]!=off[c][n])attackUnchanged=false;
   decompositionError=std::max(decompositionError,std::abs(double(b[c][n])-off[c][n]-release[c][n]));
  }
  bool naturalTail=offPeak>1e-9&&decompositionError<=std::max(1e-8,onPeak*1e-4);
  accepted=accepted&&monotone&&bounded&&silence&&contrast&&attackUnchanged&&naturalTail;
  if(!first)report<<',';first=false;report<<"{\"velocity\":"<<velocity<<",\"monotone\":"<<(monotone?"true":"false")<<",\"window_step_within_3db\":"<<(bounded?"true":"false")<<",\"silence_seconds\":"<<silenceSeconds<<",\"total_peak_reduction_db_diagnostic\":"<<reduction<<",\"release_only_peak_reduction_db\":"<<releaseReduction<<",\"release_only_reduction_at_least_15db\":"<<(contrast?"true":"false")<<",\"attack_bytes_unchanged\":"<<(attackUnchanged?"true":"false")<<",\"normal_tail_preserved\":"<<(naturalTail?"true":"false")<<",\"decomposition_max_error\":"<<decompositionError<<"}";
 }
 auto pedalBefore=capture(argv[1],beforeFile,80,127),pedalAfter=capture(argv[1],afterFile,80,127);
 bool pedalHeld=true,pedalTail=true;for(int c=0;c<2;++c)for(std::size_t n=0;n<pedalBefore[c].size();++n)if(pedalBefore[c][n]!=pedalAfter[c][n]){if(n<96000)pedalHeld=false;else pedalTail=false;}
 accepted=accepted&&pedalHeld&&pedalTail;
 report<<"],\n\"pedal_held_bytes_unchanged\":"<<(pedalHeld?"true":"false")<<",\n\"pedal_noteoff_bytes_unchanged\":"<<(pedalTail?"true":"false")<<",\n\"accepted\":"<<(accepted?"true":"false")<<"\n}\n";
 lp::atomicWrite(root/"release-rms.csv",csv.str());lp::atomicWrite(root/"release-acceptance.json",report.str());std::cout<<report.str();
 // Separate eight-second staccato/legato comparisons, with no pedal.
 for(const auto& entry:std::vector<std::pair<std::string,std::filesystem::path>>{{"before",beforeFile},{"after",afterFile}}){
  SfizzProbe synth(argv[1]);synth.load(entry.second);synth.cc(synth.synth,0,64,0);Audio sequence;
  auto append=[&](int count){auto data=synth.render(count);for(int c=0;c<2;++c)sequence[c].insert(sequence[c].end(),data[c].begin(),data[c].end());};
  synth.on(synth.synth,0,60,80);append(96000);synth.off(synth.synth,0,60,0);append(48000);
  for(int key:{60,62,64,65}){synth.on(synth.synth,0,key,80);append(6000);synth.off(synth.synth,0,key,0);append(6000);}
  synth.on(synth.synth,0,60,80);append(24000);synth.on(synth.synth,0,64,80);synth.off(synth.synth,0,60,0);append(24000);synth.off(synth.synth,0,64,0);append(144000);
  save(writer,root/(entry.first+"-staccato-legato-8s.wav"),sequence);
 }
 if(!accepted)throw std::runtime_error("Release acceptance failed; inspect the measured JSON/CSV rather than changing expected values");
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

// SPDX-License-Identifier: AGPL-3.0-or-later
#include "sound_parameters.hpp"
#include "background_store.hpp"
#include <cassert>
#include <iostream>
using namespace lp;
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-sound-parameters-test";std::filesystem::remove_all(root);std::filesystem::create_directories(root);auto file=root/"parameters.settings",base=root/"portable";
 SoundParameterStore memory;assert(!memory.load(file));Patch piano;piano.sfz="bank://piano";
 for(int i=0;i<16;++i){piano.parameters[i]=(i+1)/17.f;piano.cc[i]=i+20;}piano.adsrEnabled=true;piano.button={0xc0,2,4};
 auto key=soundParameterKey(piano.sfz,base);assert(memory.remember(key,piano));assert(!memory.remember(key,piano));
 auto sf2a=soundParameterKey("sf2://0:0:banks/piano.sf2",base),sf2b=soundParameterKey("sf2://0:1:banks/piano.sf2",base);
 assert(sf2a!=sf2b&&sf2a==soundParameterKey(makeSf2Reference(base/"banks/piano.sf2",0,0),base));
 Patch other;other.parameters[6]=.93f;memory.remember(sf2a,other);other.parameters[6]=.12f;memory.remember(sf2b,other);
 // Independent process lifetime contract: no profile is ever saved, and the writer
 // may coalesce rapid changes. Its destructor must drain the final snapshot.
 {BackgroundStore writer;for(int step=0;step<=100;++step){piano.parameters[0]=step/100.f;memory.remember(key,piano);writer.submit(file,memory.serialize());}}
 SoundParameterStore restarted;restarted.load(file);Patch category;category.sfz=piano.sfz;assert(restarted.restore(key,category));assert(category.parameters==piano.parameters&&category.cc==piano.cc&&category.adsrEnabled);
 // Restoring the same sound into a different favorite never copies its PC binding.
 Patch favorite;favorite.sfz=piano.sfz;favorite.button={0xc0,9,19};restarted.restore(key,favorite);assert(favorite.button.channel==9&&favorite.button.number==19&&favorite.parameters==piano.parameters);
 Patch first,second;restarted.restore(sf2a,first);restarted.restore(sf2b,second);assert(first.parameters[6]==.93f&&second.parameters[6]==.12f);
 // Later edits override, and an older show cannot resurrect obsolete knob values.
 piano.parameters[6]=.22f;restarted.remember(key,piano);assert(restarted.rememberMaster(.37f,7));Session imported;imported.master=.9f;imported.favorites[19]=favorite;imported.favorites[19]->parameters[6]=.8f;
 restarted.restoreSession(imported,base);assert(imported.favorites[19]->parameters[6]==.22f&&imported.master==.37f&&imported.masterCC==7);
 auto structure=showStructureSnapshot(imported);imported.favorites[19]->parameters[11]=.14f;imported.favorites[19]->adsrEnabled=false;imported.master=.25f;assert(showStructureSnapshot(imported)==structure);imported.favorites[19]->button.channel=0;assert(showStructureSnapshot(imported)!=structure);
 // Every Performance part is independent, including two parts using the same piano.
 Performance perf;perf.name="Piano e piano";for(auto& part:perf.parts){part.enabled=true;part.instrument=piano.sfz;part.patch=piano;}auto identity=savePerformance(perf);
 perf.parts[0].patch.parameters[6]=.11f;perf.parts[1].patch.parameters[6]=.88f;restarted.rememberPerformance(identity,perf,base);
 auto reloaded=restorePerformance(identity);assert(restarted.restorePerformance(identity,reloaded,base));assert(reloaded.parts[0].patch.parameters[6]==.11f&&reloaded.parts[1].patch.parameters[6]==.88f);
 Patch standalone;restarted.restore(key,standalone);assert(standalone.parameters[6]==.22f);auto unrelated=identity;unrelated.name="Outra Performance";auto untouched=restorePerformance(unrelated);assert(!restarted.restorePerformance(unrelated,untouched,base));
 assert(restarted.clearBinding(23));restarted.restore(key,standalone);assert(standalone.cc[3]==-1);backedUpWrite(file,restarted.serialize());
 // A replacement saves a recoverable previous version. Corruption cannot replace
 // it with default values, nor permit partially parsed settings to become live.
 auto previous=boundedFile(file);backedUpWrite(file,restarted.serialize());atomicWrite(file,"CORROMPIDO");SoundParameterStore recovered;assert(recovered.load(file));assert(boundedFile(file)==previous);assert(recovered.restore(key,standalone)&&standalone.parameters[6]==.22f);
 bool failed=false;try{auto invalid=piano;invalid.parameters[9]=NAN;recovered.remember(key,invalid);}catch(...){failed=true;}assert(failed);assert(recovered.restore(key,standalone)&&standalone.parameters[6]==.22f);
 {BackgroundStore writer;writer.submit(root/"missing/file",recovered.serialize());writer.flush();assert(!writer.takeErrors().empty());}assert(boundedFile(file)==previous);
 std::filesystem::remove_all(root);std::cout<<"PASSOU: parâmetros permanentes por timbre, sobrescrita, categorias/favoritos, SF2 independente, perfis antigos, partes de Performance, Master, gravação assíncrona e recuperação atômica\n";
}

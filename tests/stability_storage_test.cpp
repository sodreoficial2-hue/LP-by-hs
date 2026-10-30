#include "performance_workspace.hpp"
#include "performance_store.hpp"
#include "background_store.hpp"
#include "level_balance.hpp"
#include "instrument_catalog.hpp"
#include <cassert>
#include <fstream>
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-stability-storage";std::filesystem::remove_all(root);std::filesystem::create_directories(root);auto file=root/"performances.lphs",draft=root/"draft.settings";
 lp::Performance p;p.name="Piano";p.parts[0].enabled=true;p.parts[0].instrument="bank://piano";lp::Session session;session.performanceLibrary.push_back(lp::savePerformance(p));lp::PerformanceStore store;store.load(file);lp::PerformanceWorkspace workspace;workspace.epoch=store.nextEpoch();workspace.save(draft,p);store.commit(session,workspace.epoch);
 // Simulate an interrupted cleanup: the old draft remains, but its completion is durable.
 lp::PerformanceStore restarted;restarted.load(file);lp::Performance recovered;lp::PerformanceWorkspace old;old.load(draft,recovered);assert(restarted.completedDraft(old.epoch)&&restarted.nextEpoch()>old.epoch);
 session.performanceLibrary[0].name="New";store.commit(session);{std::ofstream out(file);out<<"damaged";}assert(restarted.load(file));lp::Session restored;restarted.mergeInto(restored);assert(restored.performanceLibrary[0].name=="Piano");
 {lp::BackgroundStore writer;for(int i=0;i<100;++i)writer.submit(root/"queued.txt",std::to_string(i));writer.flush();assert(lp::boundedFile(root/"queued.txt")=="99"&&writer.takeErrors().empty());writer.submit(root/"missing"/"draft","fail");writer.flush();assert(!writer.takeErrors().empty());}
 std::ofstream(root/"sample.wav")<<"old";std::ofstream(root/"definition.sfz")<<"#include \"envelope.sfz\"\n<region> sample=sample.wav\n";std::ofstream(root/"envelope.sfz")<<"ampeg_attack=0.1\n";
 auto ref=(root/"definition.sfz").u8string(),stamp=lp::levelStamp(ref);std::ofstream(root/"sample.wav",std::ios::trunc)<<"new larger audio";assert(stamp!=lp::levelStamp(ref));stamp=lp::levelStamp(ref);std::ofstream(root/"envelope.sfz",std::ios::trunc)<<"ampeg_attack=2.53\n";assert(stamp!=lp::levelStamp(ref));
 std::ofstream(root/"catalog.txt")<<"LPHS_BANK 1\n\"good\" 0 \"Piano\" \"definition.sfz\" 60 \"poly\" \"test\"\n\"missing\" 0 \"Missing\" \"missing.sfz\" 60 \"poly\" \"test\"\n";std::vector<std::string> warnings;auto bank=lp::loadInstrumentBank(root,{},&warnings);assert(bank.size()==1&&warnings.size()==1);
 std::filesystem::remove_all(root);
}

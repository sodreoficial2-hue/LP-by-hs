#include "performance_store.hpp"
#include "performance.hpp"
#include <cassert>
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-performance-store-test";std::filesystem::remove_all(root);std::filesystem::create_directory(root);auto file=root/"performances.lphs";
 lp::Performance p;p.name="Piano e cordas";for(auto& part:p.parts){part.enabled=true;part.instrument="bank://piano";part.patch.parameters[6]=.7f;part.patch.cc[0]=21;}
 lp::Session show;show.performanceLibrary.push_back(lp::savePerformance(p));lp::PerformanceStore store;store.load(file);store.commit(show);
 lp::PerformanceStore reopened;reopened.load(file);lp::Session fresh;reopened.mergeInto(fresh);assert(fresh.performanceLibrary.size()==1);assert(fresh.performanceLibrary[0].parts[3].patch.parameters[6]==.7f&&fresh.performanceLibrary[0].parts[3].patch.cc[0]==21);
 lp::Session imported;auto other=p;other.name="Baixo";other.parts[0].patch.parameters[0]=.3f;imported.performanceLibrary={lp::savePerformance(other),lp::savePerformance(p)};imported.favorites[0]=lp::Patch{};imported.favorites[0]->sfz="performance://0";imported.favorites[1]=lp::Patch{};imported.favorites[1]->sfz="performance://1";
 reopened.mergeInto(imported);assert(imported.performanceLibrary.size()==2);assert(imported.favorites[0]->sfz=="performance://1"&&imported.favorites[1]->sfz=="performance://0");reopened.commit(imported);
 lp::erasePerformance(imported,0);assert(!imported.favorites[1]&&imported.favorites[0]->sfz=="performance://0");reopened.commit(imported);store.load(file);fresh={};store.mergeInto(fresh);assert(fresh.performanceLibrary.size()==1&&fresh.performanceLibrary[0].name=="Baixo");
 // A failed atomic save keeps both the previous disk library and store state.
 lp::PerformanceStore failing;failing.load(root/"missing"/"library.lphs");bool failed=false;try{failing.commit(imported);}catch(...){failed=true;}assert(failed);lp::Session empty;failing.mergeInto(empty);assert(empty.performanceLibrary.empty());
 store.load(file);fresh={};store.mergeInto(fresh);assert(fresh.performanceLibrary.size()==1);
 std::filesystem::remove_all(root);
}

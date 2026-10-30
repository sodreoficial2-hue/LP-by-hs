#include "performance_workspace.hpp"
#include "performance_store.hpp"
#include <cassert>
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-workspace-test";std::filesystem::remove_all(root);std::filesystem::create_directories(root);auto file=root/"draft.settings";
 lp::Performance p;p.name="Soul com cordas";p.mode=lp::PerformanceMode::Split;for(int i=0;i<4;++i){auto& part=p.parts[i];part.enabled=true;part.instrument="bank://"+std::to_string(i);part.low=i*24;part.high=i*24+23;part.patch.parameters[0]=.2f+i*.1f;part.patch.cc[0]=20+i;}p.parts[3].high=127;
 lp::PerformanceWorkspace workspace;workspace.categories={0,1,3,6};workspace.searches={"Grande","Rhodes","Violinos","Warm"};workspace.solo=2;workspace.muted[1]=true;workspace.original=lp::performanceIdentity(lp::savePerformance(p));workspace.mode=4;workspace.customZones=true;workspace.selectedPart=3;
 workspace.save(file,p);lp::Performance restored;lp::PerformanceWorkspace loaded;assert(loaded.load(file,restored));assert(restored.parts[2].instrument==p.parts[2].instrument&&restored.parts[2].patch.cc[0]==22&&loaded.searches[3]=="Warm"&&loaded.categories[2]==3&&loaded.selectedPart==3&&loaded.customZones&&loaded.original==workspace.original);assert(loaded.auditionMask()==4);
 loaded.solo=-1;assert(loaded.auditionMask()==13);
 auto before=p;lp::copyPerformancePart(p,0,2);assert(p.parts[2].instrument==before.parts[0].instrument&&p.parts[2].patch.cc[0]==20&&p.parts[2].low==48&&p.parts[2].high==71);
 lp::swapPerformanceParts(p,0,3);assert(p.parts[0].instrument==before.parts[3].instrument&&p.parts[0].low==0&&p.parts[3].low==72);
 bool rejected=false;try{lp::copyPerformancePart(p,0,0);}catch(...){rejected=true;}assert(rejected);
 lp::PerformanceWorkspace empty;empty.searches[0]="Outra busca";empty.save(file,lp::Performance{});assert(loaded.load(file,restored)&&restored.name.empty()&&!restored.parts[0].enabled&&loaded.searches[0]=="Outra busca"&&loaded.original.empty());
 {std::ofstream out(file);out<<"truncated";}auto original=restored;rejected=false;try{loaded.load(file,restored);}catch(...){rejected=true;}assert(rejected&&restored.name==original.name);
 assert(lp::midiNoteLabel(60)=="Dó4 (MIDI 60)");std::filesystem::remove_all(root);
}

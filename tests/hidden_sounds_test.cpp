#include "hidden_sounds.hpp"
#include <cassert>
#include <fstream>
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-hidden-sounds-test";std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 auto base=root/"portable",file=root/"hidden.catalog";lp::HiddenSounds hidden;
 auto one=lp::soundExclusionKey(lp::makeSf2Reference(base/"soundbank/piano.sf2",0,0),base);
 auto two=lp::soundExclusionKey(lp::makeSf2Reference(base/"soundbank/piano.sf2",0,1),base);
 assert(one!=two&&!hidden.contains(one));hidden.exclude(one,file);assert(hidden.contains(one)&&!hidden.contains(two));
 lp::HiddenSounds reopened;reopened.load(file);assert(reopened.contains(one)&&!reopened.contains(two));
 auto moved=root/"moved";assert(one==lp::soundExclusionKey(lp::makeSf2Reference(moved/"soundbank/piano.sf2",0,0),moved));
 auto sfz=lp::soundExclusionKey("bank://electric-piano",base);reopened.exclude(sfz,file);assert(reopened.contains(sfz));
 auto external=lp::soundExclusionKey(lp::makeSf2Reference(root/"outside.sf2",4,2),base);assert(external.find("absolute:")!=std::string::npos);
 lp::SavedPerformance perf;perf.name="Soul \\\"Show\\\"";perf.parts[0].enabled=true;perf.parts[0].instrument=lp::makeSf2Reference(base/"soundbank/piano.sf2",0,0);
 auto pk=lp::performanceExclusionKey(perf,base);auto other=perf;other.name="Outro show";assert(pk!=lp::performanceExclusionKey(other,base));
 other=perf;other.parts[0].instrument=lp::makeSf2Reference(base/"soundbank/piano.sf2",0,1);assert(pk!=lp::performanceExclusionKey(other,base));
 other=perf;other.parts[0].patch.parameters[0]=.1f;assert(pk==lp::performanceExclusionKey(other,base));
 reopened.exclude(pk,file);lp::HiddenSounds restored;restored.load(file);assert(restored.contains(pk)&&restored.contains(sfz));
 auto before=std::filesystem::file_size(file);restored.exclude(pk,file);assert(std::filesystem::file_size(file)==before);
 bool failed=false;try{restored.exclude(two,root/"missing"/"fail.catalog");}catch(const std::exception&){failed=true;}assert(failed&&!restored.contains(two));
 {std::ofstream out(file);out<<"invalid registry";}failed=false;try{restored.load(file);}catch(const std::exception&){failed=true;}assert(failed&&restored.contains(pk));
 // Excluding a preset never deletes or modifies the bank itself.
 std::filesystem::create_directories(base/"soundbank");auto bank=base/"soundbank/piano.sf2";{std::ofstream out(bank);out<<"preserved";}
 restored.exclude(two,root/"new.catalog");std::ifstream in(bank);std::string content;in>>content;assert(content=="preserved");
 std::filesystem::remove_all(root);
}

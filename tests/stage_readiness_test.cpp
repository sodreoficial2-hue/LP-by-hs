#include "profile_paths.hpp"
#include "hidden_sounds.hpp"
#include "vision_protocol.hpp"
#include <cassert>
#include <fstream>
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-stage-readiness";std::filesystem::remove_all(root);std::filesystem::create_directories(root/"app/sounds");std::filesystem::create_directories(root/"show/sounds");
 lp::Session session;auto blank=session.serialize();session.current=0;assert(session.serialize()==blank);
 session.favorites[0]=lp::Patch{};session.favorites[0]->sfz=lp::makeSf2Reference(root/"app/sounds/piano.sf2",4,37);
 auto before=session.serialize();session.favorites[0]->cc[6]=74;assert(before!=session.serialize());before=session.serialize();session.favorites[0]->parameters[6]=.8f;assert(before!=session.serialize());
 lp::SavedPerformance performance;performance.name="Soul";performance.parts[0].enabled=true;performance.parts[0].instrument=(root/"show/sounds/strings.sfz").u8string();session.performanceLibrary.push_back(performance);
 auto encoded=session;lp::mapProfilePaths(encoded,[&](const std::string& r){return lp::portableReference(r,root/"app",root/"show");});
 assert(encoded.favorites[0]->sfz.find("app-path:")!=std::string::npos);assert(encoded.performanceLibrary[0].parts[0].instrument.find("profile-path:")!=std::string::npos);
 auto file=root/"show/test.lphs";encoded.save(file);lp::Session loaded;loaded.load(file);lp::mapProfilePaths(loaded,[&](const std::string& r){return lp::expandedReference(r,root/"moved-app",root/"moved-show");});
 auto sf2=lp::parseSf2Reference(loaded.favorites[0]->sfz);assert(sf2&&sf2->bank==4&&sf2->program==37&&sf2->path==root/"moved-app/sounds/piano.sf2");assert(loaded.performanceLibrary[0].parts[0].instrument==(root/"moved-show/sounds/strings.sfz").generic_u8string());
 auto original=session.serialize();auto candidate=session;lp::mapProfilePaths(candidate,[&](const std::string& r){auto path=lp::referencePath(r);return path.empty()?r:lp::withReferencePath(r,root/"repaired"/path.filename());});assert(session.serialize()==original);assert(candidate.serialize()!=original);assert(lp::parseSf2Reference(candidate.favorites[0]->sfz)->program==37);
 for(auto bad:{"app-path://../escape.sfz","profile-path://../../escape.sfz"}){bool rejected=false;try{lp::expandedReference(bad,root/"app",root/"show");}catch(...){rejected=true;}assert(rejected);}
 assert(lp::portableReference("bank://piano",root,root)=="bank://piano");assert(lp::portableReference("performance://0",root,root)=="performance://0");
 assert(lp::matchesSearch(L"Órgão Elétrico",L"orgao"));assert(lp::matchesSearch(L"Piano Acústico",L"ACUSTICO"));assert(!lp::matchesSearch(L"Rhodes",L"piano"));
 lp::HiddenSounds hidden;auto registry=root/"hidden.catalog";hidden.exclude("bank://one",registry);hidden.exclude("bank://two",registry);hidden.restore("bank://one",registry);assert(!hidden.contains("bank://one")&&hidden.contains("bank://two"));lp::HiddenSounds again;again.load(registry);assert(again.contains("bank://two"));
 bool failed=false;try{again.restore("",root/"absent/hidden.catalog");}catch(...){failed=true;}assert(failed&&again.contains("bank://two"));again.restore("",registry);hidden.load(registry);assert(hidden.entries().empty());
 for(auto action:{"restoreSounds","restoreAll","locateSounds"})assert(lp::vision::parse(std::string("LP1\t1\t")+action+"\t0\t0\t").action==action);
 assert(lp::vision::parse("LP1\t1\tsearch\t0\t0\t%C3%93rg%C3%A3o").text=="Órgão");
 std::filesystem::remove_all(root);
}

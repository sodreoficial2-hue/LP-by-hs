#include "npno_release.hpp"
#include "instrument_catalog.hpp"
#include <cassert>
#include <iostream>
using namespace lp;
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-release-integrity";std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 auto original=root/"test.npno",meta=std::filesystem::path(original.string()+".meta.json"),audit=std::filesystem::path(original.string()+".audit.json"),sfz=derivedSfz(original);
 atomicWrite(original,"original PCM unchanged");
 std::string source="// source_bank=2;\n<region> sample=release.wav trigger=release_key volume=0 ampeg_release=0.2\n// trigger=release example only\n<region> sample=attack.wav trigger=attack volume=-3\n<region> sample=other.wav trigger=release_special volume=-4\n";
 atomicWrite(sfz,source);
 std::string metadata=R"({"format":"npno","unknown":{"large_integer":9007199254740993,"release_key_gain_db":999,"text":"braces { [ and quotes \""},"zones":[{"sample":"attack.wav","volume_db":-3},{"sample":"release.wav","release_trigger":true,"volume_db":0,"source_loop_decay_q23":0,"custom":{"keep":[1,true,null]}}]})";
 atomicWrite(meta,metadata);atomicWrite(audit,R"({"unknown":[{"keep":true}]})");
 assert(updateNpnoRelease(original,-8,true));auto first=release_detail::read(sfz);
 assert(first.find("trigger=release_special")!=first.npos&&first.find("// trigger=release example only")!=first.npos);
 auto value=release_detail::object(release_detail::read(meta));assert(value.get("unknown").get("release_key_gain_db").get<double>()==999);
 assert(release_detail::read(meta).find("9007199254740993")!=std::string::npos);
 assert(value.get("zones").get<picojson::array>()[1].get("custom").get("keep").is<picojson::array>());
 // Pretty-printed, compact and CRLF JSON must behave identically.
 for(int i=0;i<12;++i){auto json=release_detail::object(release_detail::read(meta)).serialize(i%2==0);if(i%3==0){std::string crlf;for(char c:json){if(c=='\n')crlf+='\r';crlf+=c;}json=crlf;}atomicWrite(meta,json);assert(updateNpnoRelease(original,-12,false));assert(updateNpnoRelease(original,-8,true));assert(release_detail::read(sfz)==first);}
 auto valid=release_detail::read(meta);atomicWrite(meta,"{\"format\":\"npno\",}");rejects([&]{updateNpnoRelease(original,-12,true);});assert(release_detail::read(sfz)==first);atomicWrite(meta,valid);
 std::string empty="{}";release_detail::set(empty,"enabled","true");assert(release_detail::object(empty).get("enabled").get<bool>());
 rejects([&]{release_detail::object("{} junk");});rejects([&]{release_detail::object("{\"a\":NaN}");});
 assert(!release_detail::releaseRegion("// <region> trigger=release"));assert(!release_detail::releaseRegion("<region> sample=x trigger=release_special"));assert(release_detail::releaseRegion("<region> sample=x trigger = release_key"));
 // Simulate a storage error after template/SFZ replacement but before metadata.
 // The durable intent must survive, reject playback, then finish on retry.
 auto backup=meta;backup+=".saved";std::filesystem::rename(meta,backup);std::filesystem::create_directory(meta);
 auto newMeta=valid;release_detail::set(newMeta,"release_key_gain_db","-12");
 rejects([&]{release_detail::commit(original,source,"new coherent sfz",newMeta,release_detail::read(audit));});
 assert(std::filesystem::exists(release_detail::journalPath(original)));rejects([&]{playableInstrument(original);});
 std::filesystem::remove(meta);std::filesystem::rename(backup,meta);
 release_detail::recover(original);assert(release_detail::read(sfz)=="new coherent sfz"&&release_detail::read(meta)==newMeta);assert(!std::filesystem::exists(release_detail::journalPath(original)));
 // Replay an interruption at every replacement boundary, including a crash
 // after all files were replaced but before journal removal.
 for(std::size_t boundary=0;boundary<=4;++boundary){
  std::vector<std::string> wanted={source,"coherent sfz",newMeta,R"({"consistent":true})"};picojson::object entries;
  for(std::size_t i=0;i<4;++i){picojson::object e;e["bytes"]=picojson::value(wanted[i]);e["sha256"]=picojson::value(release_detail::digest(wanted[i]));entries[release_detail::suffixes()[i]]=picojson::value(e);auto target=original;target+=release_detail::suffixes()[i];atomicWrite(target,i<boundary?wanted[i]:"previous generation");}
  picojson::object intent;intent["version"]=picojson::value(1.0);intent["files"]=picojson::value(entries);atomicWrite(release_detail::journalPath(original),picojson::value(intent).serialize());
  assert(updateNpnoRelease(original,-12,true));for(std::size_t i=0;i<4;++i){auto target=original;target+=release_detail::suffixes()[i];assert(release_detail::read(target)==wanted[i]);}
 }
 atomicWrite(sfz,"new coherent sfz");
 // Corrupt recovery records must never touch any bank files.
 picojson::object files;for(const auto& suffix:release_detail::suffixes()){picojson::object entry;entry["bytes"]=picojson::value("corrupt");entry["sha256"]=picojson::value("bad hash");files[suffix]=picojson::value(entry);}
 picojson::object journal;journal["version"]=picojson::value(1.0);journal["files"]=picojson::value(files);atomicWrite(release_detail::journalPath(original),picojson::value(journal).serialize());
 rejects([&]{release_detail::recover(original);});assert(release_detail::read(sfz)=="new coherent sfz"&&release_detail::read(meta)==newMeta);
 assert(release_detail::read(original)=="original PCM unchanged");std::filesystem::remove_all(root);
 std::cout<<"PASS: structured JSON, unknown fields, repeated controls, exact triggers, interrupted commit recovery, integrity and playback gate\n";
}

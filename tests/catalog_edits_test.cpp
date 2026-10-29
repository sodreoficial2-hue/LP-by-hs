#include "catalog_edits.hpp"
#include <cassert>
#include <iostream>
int main(){
 namespace fs=std::filesystem;auto root=fs::temp_directory_path()/"lp-catalog-edits-test";fs::remove_all(root);fs::create_directories(root/"bank");
 auto file=root/"edits";lp::Instrument sound;sound.id="piano";sound.name="Original";sound.sfz=root/"bank/piano.sfz";sound.category=0;
 std::ofstream(sound.sfz)<<"<region> sample=shared.wav\n";std::ofstream(root/"bank/shared.wav")<<"sample";
 lp::CatalogEdits edits;auto key=lp::soundExclusionKey(lp::instrumentReference(sound),root);auto edit=edits.get(key);edit.name="Meu Piano elétrico";edit.category=6;edits.set(key,edit,file);
 lp::CatalogEdits reopened;reopened.load(file);reopened.apply(sound,root);assert(sound.category==6);assert(lp::instrumentDisplayName(sound)==edit.name);assert(lp::instrumentReference(sound)=="bank://piano");
 // Moving the portable retains identities and the settings.
 lp::Instrument relocated=sound;relocated.sfz=root/"moved/bank/piano.sfz";relocated.customName.clear();relocated.category=0;reopened.apply(relocated,root/"moved");assert(relocated.category==6&&relocated.customName==edit.name);
 lp::HiddenSounds hidden;hidden.exclude(key,root/"hidden");assert(fs::exists(sound.sfz));hidden.restore(key,root/"hidden");assert(!hidden.contains(key));
 // Shared SFZ definitions cannot be removed, including relative Windows paths.
 std::ofstream(root/"other.sfz")<<"#include \"bank\\piano.sfz\"\n";
 bool blocked=false;try{lp::ensureUnsharedDefinition(sound.sfz,root,{sound});}catch(...){blocked=true;}assert(blocked&&fs::exists(sound.sfz));fs::remove(root/"other.sfz");
 lp::ensureUnsharedDefinition(sound.sfz,root,{sound});edit.deleted=true;edits.set(key,edit,file);lp::deleteSoundFile(sound.sfz);assert(!fs::exists(sound.sfz)&&fs::exists(root/"bank/shared.wav"));
 std::ofstream(root/"bank/catalog.txt")<<"LPHS_BANK 1\n\"piano\" 0 \"Original\" \"piano.sfz\" 60 \"poly\" \"license\"\n";
 reopened.load(file);assert(reopened.deletedBankIds().count("piano"));assert(lp::loadInstrumentBank(root/"bank",reopened.deletedBankIds()).empty());
 // Failed persistent writes leave memory unchanged; corrupt input also preserves it.
 auto before=reopened.get(key);edit.name="Must not stick";blocked=false;try{reopened.set(key,edit,root/"missing/edits");}catch(...){blocked=true;}assert(blocked&&reopened.get(key).name==before.name);
 std::ofstream(file)<<"bad header";blocked=false;try{reopened.load(file);}catch(...){blocked=true;}assert(blocked&&reopened.get(key).name==before.name);
 std::ofstream(root/"keep.wav")<<"sample";blocked=false;try{lp::deleteSoundFile(root/"keep.wav");}catch(...){blocked=true;}assert(blocked&&fs::exists(root/"keep.wav"));
 fs::remove_all(root);std::cout<<"Catalog edits: persistence, portable identity, hide/restore, shared files, deletion, restart and failed writes passed\n";
}

#include "instrument_catalog.hpp"
#include <iostream>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
 try {
 check(lp::soundDisplayName("Red-PN-Royal Grand / Royal Grand",true)=="Royal Grand","SF2 filename/preset duplication");
 check(lp::soundDisplayName("Red-EP-001-DX7 EP 2.sf2")=="DX7 EP 2","Pack prefixes removed; musical model/variant retained");
 check(lp::soundDisplayName("My Bank / Red Grand Piano (0:5)",true)=="Grand Piano","Legacy imported preset label");
 check(lp::soundDisplayName("Grand Piano Grand Piano")=="Grand Piano","Repeated phrase");
 check(lp::soundDisplayName("Piano Piano Piano")=="Piano","Repeated word");
 check(lp::soundDisplayName("Piano - Piano")=="Piano","Repeated decorated name");
 check(lp::soundDisplayName("Red Órgão Órgão")=="Órgão","UTF-8 names retained");
 check(lp::soundDisplayName("Redwood Altered Strings")=="Redwood Altered Strings","No substring damage");
 check(lp::soundDisplayName("Piano 1 / Piano 2")=="Piano 1 / Piano 2","Distinct musical variants retained");
 check(lp::soundDisplayName("Red")=="Timbre sem nome","Nonempty display fallback");
 lp::Instrument imported;auto ref=lp::makeSf2Reference("Red-PN-Grand.sf2",0,5);imported.sfz=std::filesystem::u8path(ref);imported.name="Red-PN-Grand / Grand (0:5)";
 check(lp::instrumentName(ref,{imported})=="Grand","Favorites use SF2 preset name, not encoded path");
 check(lp::instrumentReference(imported)==ref,"Display cleanup preserves preset/path identity");

 auto root=std::filesystem::temp_directory_path()/"lp-catalog-regression";std::filesystem::remove_all(root);std::filesystem::create_directories(root);
 std::ofstream(root/"tone.sfz")<<"<region> sample=*saw";
 auto write=[&](const std::string& line){std::ofstream(root/"catalog.txt")<<"LPHS_BANK 1\n"<<line<<'\n';};
 write("\"stable-id\" 1 \"Piano elétrico\" \"tone.sfz\" 60 \"poly\" \"CC0-1.0\"");auto bank=lp::loadInstrumentBank(root);
 check(bank.size()==1&&bank[0].category==1,"UTF-8 catalogue and category");
 check(lp::resolveInstrument("bank://stable-id",bank)==std::filesystem::absolute(root/"tone.sfz"),"Stable ID resolution");
 lp::Session session;session.favorites[19]=lp::Patch{};session.favorites[19]->sfz=lp::instrumentReference(bank[0]);session.save(root/"show.lphs");lp::Session restored;restored.load(root/"show.lphs");
 auto moved=root/"moved";std::filesystem::create_directories(moved);std::filesystem::copy_file(root/"tone.sfz",moved/"tone.sfz",std::filesystem::copy_options::overwrite_existing);std::filesystem::copy_file(root/"catalog.txt",moved/"catalog.txt",std::filesystem::copy_options::overwrite_existing);
 auto relocated=lp::loadInstrumentBank(moved);
 auto unicode=root/std::filesystem::u8path(u8"Áudio - São Paulo");std::filesystem::create_directories(unicode);
 std::filesystem::copy_file(root/"tone.sfz",unicode/"tone.sfz",std::filesystem::copy_options::overwrite_existing);
 std::filesystem::copy_file(root/"catalog.txt",unicode/"catalog.txt",std::filesystem::copy_options::overwrite_existing);
 auto unicodeBank=lp::loadInstrumentBank(unicode);
 check(unicodeBank.size()==1 && std::filesystem::exists(unicodeBank[0].sfz),"Unicode directory support");check(lp::resolveInstrument(restored.favorites[19]->sfz,relocated)==std::filesystem::absolute(moved/"tone.sfz"),"Portable favorite after moving bank");check(restored.programChange(19),"Favorite 20 remains PC 20");
 check(lp::resolveInstrument("legacy.sfz",bank)=="legacy.sfz","Legacy reference compatibility");
 for(auto line:{"\"x\" 15 \"Name\" \"tone.sfz\" 60 \"poly\" \"CC0\"","\"x\" 0 \"Name\" \"../tone.sfz\" 60 \"poly\" \"CC0\"","\"x\" 0 \"Name\" \"missing.sfz\" 60 \"poly\" \"CC0\"","\"x\" 0 \"Name\" \"tone.sfz\" 128 \"poly\" \"CC0\""}){
  write(line);bool rejected=false;try{lp::loadInstrumentBank(root);}catch(const std::exception&){rejected=true;}check(rejected,"Malformed bank must fail visibly");
 }
 std::filesystem::remove_all(root);std::cout<<"PASS: catalog categories, UTF-8, portable favorites, legacy paths, invalid bank rejection\n";
 } catch(const std::exception& e){std::cerr<<"Catalog regression: "<<e.what()<<"\n";return 1;}
}

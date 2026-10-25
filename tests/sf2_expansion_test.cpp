#include "sf2_expansion.hpp"
#include "sf2_headers.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
int main(int argc,char** argv){
 auto base=std::filesystem::temp_directory_path()/"lp-expand";auto root=base/lp::sf2ExpansionFolder;
 assert(lp::expansionSf2Category(base,root/"01-Pianos-acusticos/Salamander.sf2",0,99)==0);
 assert(lp::expansionSf2Category(base,root/"12-Contrabaixos/LatelyBass.sf2",0,0)==11);
 auto gm=root/"14-Bancos-GM/GeneralUser.sf2";
 for(int program=0;program<128;++program){auto cat=lp::expansionSf2Category(base,gm,0,program);assert(cat&&*cat<13);}
 assert(lp::expansionSf2Category(base,gm,0,4)==1);
 assert(lp::expansionSf2Category(base,gm,8,16)==2);
 assert(lp::expansionSf2Category(base,gm,128,0)==8);
 assert(lp::expansionSf2Category(base,gm,0,21)==9);
 assert(lp::expansionSf2Category(base,gm,0,32)==11);
 assert(lp::expansionSf2Category(base,gm,0,89)==6);
 assert(!lp::expansionSf2Category(base,base/"soundbank/sf2/old.sf2",0,0));
 assert(!lp::expansionSf2Category(base,root/"../other/file.sf2",0,0));
 assert(!lp::expansionSf2Category(base,root/"unknown/file.sf2",0,0));
 if(argc>1){unsigned files=0,presets=0;std::array<unsigned,13> counts{};auto dir=std::filesystem::path(argv[1]);
  for(const auto& item:std::filesystem::recursive_directory_iterator(dir/lp::sf2ExpansionFolder))if(item.path().extension()==".sf2"){
   auto entries=lp::readSf2PresetHeaders(item.path());assert(!entries.empty());++files;
   for(const auto& p:entries){auto cat=lp::expansionSf2Category(dir,item.path(),p.bank,p.program);assert(cat&&*cat<13);++counts[*cat];++presets;}
  }
  std::cout<<"Files="<<files<<" Presets="<<presets<<" Categories:";for(auto n:counts)std::cout<<' '<<n;std::cout<<'\n';assert(files>0&&presets>=files);
 }
 auto broken=std::filesystem::temp_directory_path()/"lp-truncated.sf2";{std::ofstream out(broken);out<<"RIFF";}bool rejected=false;
 try{lp::readSf2PresetHeaders(broken);}catch(const std::runtime_error&){rejected=true;}std::filesystem::remove(broken);assert(rejected);
}

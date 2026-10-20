#include "instrument_catalog.hpp"
#include <iostream>
int main(int argc,char** argv) {
 try {
  const std::filesystem::path root=argc>1?argv[1]:"recovery/extracted/live/samples";
  if(std::filesystem::exists(root/"catalog.txt")){for(const auto& p:lp::loadInstrumentBank(root))std::cout<<p.id<<"\t"<<lp::instrumentCategories[p.category]<<"\t"<<p.name<<"\t"<<p.sfz.u8string()<<'\n';return 0;}
  for(auto& path:lp::catalog(root)) std::cout<<path.u8string()<<'\n';
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
